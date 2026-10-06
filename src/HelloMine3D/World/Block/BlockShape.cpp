#include "BlockShape.h"

#include "../../Util/ResourcePaths.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
std::string trim(const std::string &value)
{
    std::size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

[[noreturn]] void fail(const std::string &path, const std::string &key,
                       const std::string &detail)
{
    throw std::runtime_error("Invalid block shape file '" + path +
                             "': key '" + key + "' " + detail + ".");
}

bool isValidName(const std::string &name)
{
    if (name.empty()) {
        return false;
    }
    for (char value : name) {
        if (!std::isalnum(static_cast<unsigned char>(value)) &&
            value != '_' && value != '-') {
            return false;
        }
    }
    return true;
}

BlockShapeFace parseFace(const std::string &path,
                         const std::string &value)
{
    std::istringstream input(value);
    BlockShapeFace face{};
    for (float &coordinate : face) {
        if (!(input >> coordinate)) {
            fail(path, "Face", "must contain 12 coordinates");
        }
        if (!std::isfinite(coordinate) || coordinate < 0.f ||
            coordinate > 1.f) {
            fail(path, "Face", "has a coordinate outside [0, 1]");
        }
    }
    input >> std::ws;
    if (!input.eof()) {
        fail(path, "Face", "contains trailing data");
    }
    return face;
}
// Freeze a bounded union surface. Micro-grid faces are greedily merged only
// when their source material agrees, so overlaps never emit interior triangles.
BlockShapeVariant buildVariant(const std::vector<BlockShapeBox> &boxes,
                               const std::string &path)
{
    BlockShapeVariant result; result.boxes = boxes;
    std::array<int, 512> owner; owner.fill(-1);
    const auto index = [](int x,int y,int z) { return x+8*(z+8*y); };
    for (std::size_t b=0;b<boxes.size();++b) {
        const auto &box=boxes[b];
        for(int y=int(box.minimum[1]*8);y<int(box.maximum[1]*8);++y)
        for(int z=int(box.minimum[2]*8);z<int(box.maximum[2]*8);++z)
        for(int x=int(box.minimum[0]*8);x<int(box.maximum[0]*8);++x)
            if(owner[index(x,y,z)]<0) owner[index(x,y,z)]=int(b);
    }
    const auto at = [&](const std::array<int,3> &q) {
        for(int a=0;a<3;++a) if(q[a]<0 || q[a]>=8) return -1;
        return owner[index(q[0],q[1],q[2])];
    };
    // Matches the legacy front/back/left/right/top/bottom surface winding.
    const int axis[6]={2,2,0,0,1,1};
    const int sign[6]={1,-1,-1,1,1,-1};
    for(int f=0;f<6;++f) for(int slice=0;slice<8;++slice) {
        const int a=axis[f], u=(a+1)%3, v=(a+2)%3;
        std::array<int,64> mask; mask.fill(-1);
        for(int j=0;j<8;++j) for(int i=0;i<8;++i) {
            std::array<int,3> q{}; q[a]=slice;q[u]=i;q[v]=j;
            const int b=at(q); q[a]+=sign[f];
            if(b>=0 && at(q)<0) mask[i+8*j]=boxes[b].materials[f];
        }
        for(int j=0;j<8;++j) for(int i=0;i<8;++i) {
            const int material=mask[i+8*j]; if(material<0) continue;
            int width=1,height=1;
            while(i+width<8 && mask[i+width+8*j]==material) ++width;
            bool grow=true;
            while(j+height<8 && grow) {
                for(int k=0;k<width;++k) if(mask[i+k+8*(j+height)]!=material) grow=false;
                if(grow) ++height;
            }
            for(int y=0;y<height;++y) for(int x=0;x<width;++x) mask[i+x+8*(j+y)]=-1;
            BlockShapeSurface face;face.material=static_cast<unsigned char>(material);
            // e_u cross e_v is positive axis; reverse winding for negative side.
            const int orderPositive[4][2]={{0,0},{1,0},{1,1},{0,1}};
            const int orderNegative[4][2]={{0,1},{1,1},{1,0},{0,0}};
            for(int k=0;k<4;++k) {
                const auto &corner=sign[f]>0?orderPositive[k]:orderNegative[k];
                face.positions[k*3+a]=(slice+(sign[f]>0?1:0))/8.f;
                face.positions[k*3+u]=(i+corner[0]*width)/8.f;
                face.positions[k*3+v]=(j+corner[1]*height)/8.f;
                face.repeat[k*2]=face.positions[k*3+u];
                face.repeat[k*2+1]=face.positions[k*3+v];
            }
            result.surfaces.push_back(face);
            if(result.surfaces.size()>BlockShape::MaxSurfaces)
                fail(path,"Box","exceeds the surface budget");
        }
    }
    return result;
}

void freezeCompound(BlockShape &shape,std::vector<BlockShapeBox> boxes,
                    const std::string &path)
{
    if(boxes.empty()) fail(path,"Box","is missing");
    shape.variants[0]=buildVariant(boxes,path);
    std::array<bool,512> filled{},collisionFilled{};
    for(const auto &box:boxes)
        for(int y=int(box.minimum[1]*8);y<int(box.maximum[1]*8);++y)
        for(int z=int(box.minimum[2]*8);z<int(box.maximum[2]*8);++z)
        for(int x=int(box.minimum[0]*8);x<int(box.maximum[0]*8);++x)
            {
                filled[x+8*(z+8*y)]=true;
                if(box.collidable) collisionFilled[x+8*(z+8*y)]=true;
            }
    shape.fillsCell=std::all_of(filled.begin(),filled.end(),[](bool v){return v;});
    shape.fillsCollisionCell=std::all_of(collisionFilled.begin(),collisionFilled.end(),[](bool v){return v;});
    for(int yaw=1;yaw<4;++yaw) {
        auto variant=shape.variants[yaw-1];
        for(auto &box:variant.boxes) {
            const float minX=box.minimum[0],maxX=box.maximum[0];
            box.minimum[0]=1.f-box.maximum[2];box.maximum[0]=1.f-box.minimum[2];
            box.minimum[2]=minX;box.maximum[2]=maxX;
        }
        for(auto &face:variant.surfaces) for(int k=0;k<4;++k) {
            const float x=face.positions[k*3];
            face.positions[k*3]=1.f-face.positions[k*3+2];face.positions[k*3+2]=x;
        }
        shape.variants[yaw]=std::move(variant);
    }
}

BlockShapeBox parseBox(const std::string &path,const std::string &value)
{
    std::istringstream input(value);BlockShapeBox box;
    int collision=0,selection=0;
    for(float &v:box.minimum) if(!(input>>v)) fail(path,"Box","requires six bounds");
    for(float &v:box.maximum) if(!(input>>v)) fail(path,"Box","requires six bounds");
    if(!(input>>collision>>selection) || collision<0 || collision>1 || selection<0 || selection>1)
        fail(path,"Box","requires collision and selection flags 0 or 1");
    box.collidable=collision!=0;box.selectable=selection!=0;
    for(auto &material:box.materials) {
        int role=-1;if(!(input>>role) || role<0 || role>2)
            fail(path,"Box","requires six material roles in [0,2]");
        material=static_cast<unsigned char>(role);
    }
    input>>std::ws;if(!input.eof()) fail(path,"Box","contains trailing data");
    for(int a=0;a<3;++a) {
        const float lo=box.minimum[a],hi=box.maximum[a];
        if(!std::isfinite(lo)||!std::isfinite(hi)||lo<0||hi>1||lo>=hi ||
           std::abs(lo*8-std::round(lo*8))>0.00001f ||
           std::abs(hi*8-std::round(hi*8))>0.00001f)
            fail(path,"Box","bounds must be increasing eighth-metre coordinates in [0,1]");
    }
    return box;
}
} // namespace

BlockShape loadBlockShape(const std::string &name,
                          const std::string &shapeDirectory)
{
    return loadBlockShapeFile(
        name, ResourcePaths::join(shapeDirectory, name + ".shape"));
}

BlockShape loadBlockShapeFile(const std::string &name,
                              const std::string &path)
{
    if (!isValidName(name)) {
        throw std::runtime_error("Invalid block shape name '" + name +
                                 "'.");
    }
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Unable to open block shape file '" + path +
                                 "'.");
    }

    BlockShape shape;
    shape.name = name;
    std::string keyLine;
    std::size_t lineNumber = 0;
    std::vector<BlockShapeBox> boxes;
    bool hasVersion = false;
    while (std::getline(input, keyLine)) {
        ++lineNumber;
        const std::string key = trim(keyLine);
        if (key.empty()) {
            continue;
        }
        if (key != "Face" && key != "Version" && key != "Box") {
            fail(path, key, "is unknown at line " +
                                std::to_string(lineNumber));
        }

        std::string valueLine;
        if (!std::getline(input, valueLine)) {
            fail(path, key, "is missing a value");
        }
        ++lineNumber;
        const std::string value = trim(valueLine);
        if (value.empty()) {
            fail(path, key, "is missing a value at line " +
                                std::to_string(lineNumber));
        }
        if (key == "Version") {
            if(hasVersion || !shape.faces.empty() || !boxes.empty() || value != "2")
                fail(path,key,"must declare version 2 once before geometry");
            hasVersion=true;shape.version=2;
        } else if (key == "Box") {
            if(shape.version!=2) fail(path,key,"requires Version 2");
            if(boxes.size()>=BlockShape::MaxBoxes) fail(path,key,"exceeds eight boxes");
            boxes.push_back(parseBox(path,value));
        } else {
            if(shape.version!=1) fail(path,key,"is not valid in version 2");
            shape.faces.push_back(parseFace(path,value));
        }
    }

    if (shape.version == 2) { freezeCompound(shape,std::move(boxes),path); return shape; }
    if (shape.faces.empty()) {
        fail(path, "Face", "is missing");
    }
    return shape;
}
