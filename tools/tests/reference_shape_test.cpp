#include "World/Block/BlockShape.h"
#include "World/Block/BlockGeometry.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

ChunkBlock blockValue(BlockId id,BlockMetadata_t metadata) { ChunkBlock block;block.id=static_cast<Block_t>(id);block.metadata=metadata;return block; }
int main(int argc,char **argv) {
    if(argc!=3) return 2;
    try {
        const std::filesystem::path root(argv[1]), output(argv[2]);
        int checks=0;
        const auto require=[&](bool value,const std::string &label) {
            if(!value) throw std::runtime_error(label);++checks;
        };
        const auto source=[&](const std::string &name) {
            return loadBlockShapeFile(name,(root/"media/shapes"/(name+".shape")).string());
        };
        const auto cross=source("Cross"),door=source("OakDoorClosed");
        require(cross.version==1 && cross.faces.size()==2 && door.faces.size()==2,"legacy-shape-data-preserved");
        const auto step=source("StoneStep"),frame=source("StoneWindowFrame");
        require(step.version==2 && step.variants[0].boxes.size()==2 && !step.fillsCell,"step-two-boxes-partial-cell");
        require(frame.version==2 && frame.variants[0].boxes.size()==4 && !frame.fillsCell,"frame-four-boxes-partial-cell");
        for(const auto *shape:{&step,&frame}) for(int yaw=0;yaw<4;++yaw) {
            const auto &variant=shape->variants[yaw];
            require(variant.surfaces.size()<=BlockShape::MaxSurfaces,"bounded-surfaces");
            float area=0;
            for(const auto &face:variant.surfaces) {
                std::array<glm::vec3,4> q;
                for(int k=0;k<4;++k) {
                    q[k]={face.positions[k*3],face.positions[k*3+1],face.positions[k*3+2]};
                    require(q[k].x>=0 && q[k].x<=1 && q[k].y>=0 && q[k].y<=1 && q[k].z>=0 && q[k].z<=1,"single-cell-bounds");
                }
                const auto normal=glm::cross(q[1]-q[0],q[2]-q[0]);
                const float size=glm::length(normal);require(size>0,"nondegenerate-face");area+=size;
                const glm::vec3 centre=(q[0]+q[1]+q[2]+q[3])*.25f;
                const auto inside=[&](glm::vec3 point) {
                    for(const auto &box:variant.boxes)
                        if(point.x>=box.minimum[0] && point.x<=box.maximum[0] &&
                           point.y>=box.minimum[1] && point.y<=box.maximum[1] &&
                           point.z>=box.minimum[2] && point.z<=box.maximum[2]) return true;
                    return false;
                };
                require(inside(centre-glm::normalize(normal)*.001f) && !inside(centre+glm::normalize(normal)*.001f),"only-union-exterior-outward");
                float minU=1,maxU=0,minV=1,maxV=0;
                for(int k=0;k<4;++k) {minU=std::min(minU,face.repeat[k*2]);maxU=std::max(maxU,face.repeat[k*2]);minV=std::min(minV,face.repeat[k*2+1]);maxV=std::max(maxV,face.repeat[k*2+1]);}
                require(std::abs((maxU-minU)*(maxV-minV)-size)<.00001f,"metre-scaled-uv");
            }
            require(std::abs(area-(shape==&step?5.5f:2.625f))<.00001f,"union-area-four-yaws");
        }
        BlockDefinition definition;definition.id=BlockId::StoneWindowFrame;definition.collidable=true;definition.render.shape=frame;
        const ChunkBlock frameBlock=blockValue(BlockId::StoneWindowFrame,0);
        require(!BlockGeometry::collides(definition,frameBlock,{0,0,0},{{.3f,.3f,.4f},{.7f,.7f,.6f}}),"frame-hole-no-collision");
        require(BlockGeometry::collides(definition,frameBlock,{0,0,0},{{.05f,.3f,.4f},{.2f,.7f,.6f}}),"frame-post-collision");
        for(int yaw=0;yaw<4;++yaw) {
            float distance=0;glm::ivec3 normal(0);
            const glm::vec3 direction=yaw%2?glm::vec3(1,0,0):glm::vec3(0,0,1);
            const glm::vec3 origin=glm::vec3(.5f)-direction*2.f;
            require(!BlockGeometry::pick(definition,blockValue(BlockId::StoneWindowFrame,yaw),{0,0,0},origin,direction,4,distance,normal),"frame-ray-holes-all-yaws");
            const glm::vec3 postOrigin=origin+glm::vec3(0,.45f,0);
            require(BlockGeometry::pick(definition,blockValue(BlockId::StoneWindowFrame,yaw),{0,0,0},postOrigin,direction,4,distance,normal) && std::abs(distance-1.875f)<.0001f,"frame-ray-hit-all-yaws");
            require(BlockGeometry::orientationFromYaw(yaw*90.f)==yaw,"placement-yaw");
        }
        require(BlockGeometry::orientationFromYaw(-90)==3 && BlockGeometry::orientationFromYaw(360)==0,"negative-wrapped-yaw");
        require(!BlockGeometry::validMetadata(blockValue(BlockId::StoneStep,4)) && BlockGeometry::validMetadata(blockValue(BlockId::TallGrass,255)),"new-id-only-metadata-validation");
        const std::string box="0 0 0 1 1 1 1 1 1 1 1 1 0 2\n";
        const auto reject=[&](const std::string &value) {
            const auto path=output/"invalid.shape";{std::ofstream file(path);file<<value;}
            bool rejected=false;try {loadBlockShapeFile("Invalid",path.string());}catch(const std::runtime_error &) {rejected=true;}
            require(rejected,"parser-negative");
        };
        reject("Version\n2\n");reject("Box\n"+box);reject("Version\n3\n");reject("Version\n2\nVersion\n2\n");
        reject("Version\n2\nFace\n0 0 0 1 0 0 1 1 0 0 1 0\n");
        reject("Version\n2\nBox\n0 0 0 1.125 1 1 1 1 1 1 1 1 0 2\n");
        reject("Version\n2\nBox\n0 0 0 0.1 1 1 1 1 1 1 1 1 0 2\n");
        reject("Version\n2\nBox\n0 0 0 0 1 1 1 1 1 1 1 1 0 2\n");
        reject("Version\n2\nBox\n0 0 0 1 1 1 2 1 1 1 1 1 0 2\n");
        reject("Version\n2\nBox\n0 0 0 1 1 1 1 1 1 1 1 1 0 3\n");
        reject("Version\n2\nBox\n"+box+"Unknown\n0\n");
        std::string over="Version\n2\n";for(int i=0;i<9;++i)over+="Box\n"+box;reject(over);
        const auto path=output/"overlap.shape";{std::ofstream file(path);file<<"Version\n2\nBox\n"<<box<<"Box\n"<<box;}
        const auto overlap=loadBlockShapeFile("Overlap",path.string());
        require(overlap.fillsCell && overlap.variants[0].surfaces.size()==6,"overlap-union-eliminates-internal-duplicates");
        const auto decorationPath=output/"decoration.shape";
        {std::ofstream file(decorationPath);file<<"Version\n2\nBox\n0 0 0 1 1 1 0 1 1 1 1 1 0 2\n";}
        const auto decoration=loadBlockShapeFile("Decoration",decorationPath.string());
        require(decoration.fillsCell && !decoration.fillsCollisionCell,"full-render-decoration-is-not-full-solid");
        std::cout<<"[REFERENCE-SHAPE] "<<checks<<" checks passed\n";return 0;
    } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
