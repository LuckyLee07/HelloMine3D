#include "Ogre/OgreActorRenderer.h"
#include "Ogre/OgrePlayerRenderer.h"
#include "Ogre/StartupResourcePreflight.h"
#include "Actor/ItemEntity.h"
#include "Util/ResourcePackResolver.h"
#include "World/Block/BlockDatabase.h"
#include "World/Block/TerrainMaterialProfile.h"

#include <Ogre.h>
#include <OgreDefaultHardwareBufferManager.h>
#include <OgreVertexIndexData.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks = 0, failures = 0;
void check(const std::string& name, bool passed)
{
    ++checks;
    failures += !passed;
    std::cout << "[ITEM_BINDING] " << (passed ? "PASS " : "FAIL ") << name << '\n';
}
bool near(float a, float b) { return std::abs(a-b) < 1e-6f; }
struct Case {
    const char* name;
    Material::ID material;
    BlockId block;
    int x, sideY, topX, topY;
};
// Frozen semantic coordinates, independent of MaterialIcons, loaded blocks,
// itemVisualGeometry and either production renderer's UV calculations.
constexpr std::array<Case,5> Cases{{
    {"OakPlank",Material::OakPlank,BlockId::OakPlank,5,1,5,1},
    {"Cobblestone",Material::Cobblestone,BlockId::Cobblestone,7,1,7,1},
    {"Chest",Material::Chest,BlockId::Chest,0,1,0,1},
    {"Workbench",Material::Workbench,BlockId::Workbench,1,1,1,1},
    {"OakBark",Material::OakBark,BlockId::OakBark,4,0,5,0}
}};
struct Vertex { std::array<float,3> position{}; std::array<float,2> tile{},repeat{}; float root=0; };
struct Mesh { std::vector<Vertex> vertices; std::size_t indices=0; bool software=false; };
std::vector<float> readElement(const Ogre::VertexData& data, std::size_t vertex,
                              Ogre::VertexElementSemantic semantic, unsigned index,
                              Ogre::VertexElementType type, std::size_t count)
{
    const auto* element = data.vertexDeclaration->findElementBySemantic(semantic,index);
    if (!element || element->getType() != type)
        throw std::runtime_error("Missing actual production vertex attribute/type");
    const auto buffer = data.vertexBufferBinding->getBuffer(element->getSource());
    std::vector<float> result(count);
    buffer->readData((data.vertexStart+vertex)*buffer->getVertexSize()+element->getOffset(),
                     count*sizeof(float),result.data());
    return result;
}
Mesh readMesh(Ogre::ManualObject& object)
{
    if (object.getNumSections() != 1) throw std::runtime_error("Expected one actual item section");
    const auto& op = *object.getSection(0)->getRenderOperation();
    if (!op.vertexData || !op.indexData || op.operationType != Ogre::RenderOperation::OT_TRIANGLE_LIST)
        throw std::runtime_error("Expected production triangle list with indices");
    Mesh result;
    result.indices = op.indexData->indexCount;
    result.software = true;
    for (const auto& binding : op.vertexData->vertexBufferBinding->getBindings())
        result.software &= dynamic_cast<Ogre::DefaultHardwareVertexBuffer*>(binding.second.get()) != nullptr;
    for (std::size_t i=0;i<op.vertexData->vertexCount;++i) {
        const auto p=readElement(*op.vertexData,i,Ogre::VES_POSITION,0,Ogre::VET_FLOAT3,3);
        const auto tile=readElement(*op.vertexData,i,Ogre::VES_TEXTURE_COORDINATES,0,Ogre::VET_FLOAT2,2);
        const auto repeat=readElement(*op.vertexData,i,Ogre::VES_TEXTURE_COORDINATES,1,Ogre::VET_FLOAT2,2);
        const auto root=readElement(*op.vertexData,i,Ogre::VES_TEXTURE_COORDINATES,3,Ogre::VET_FLOAT1,1);
        result.vertices.push_back({{p[0],p[1],p[2]},{tile[0],tile[1]},{repeat[0],repeat[1]},root[0]});
    }
    return result;
}
void dumpMesh(Ogre::ManualObject& object,const std::string& output,
              const std::string& name,Material::ID material)
{
    const auto& op=*object.getSection(0)->getRenderOperation();
    const auto& profile=runtimeTerrainMaterialProfile().parameters();
    std::ofstream receipt(output+"/"+name+".json");
    receipt << "{\n  \"schema\":\"hellomine3d-item-binding-raw-operation-v1\",\n"
            << "  \"scope\":\"production software VBO/IBO; stub material, no GPU or actual texture binding\",\n"
            << "  \"name\":" << std::quoted(name) << ",\n  \"material_id\":" << int(material)
            << ",\n  \"object_name\":" << std::quoted(object.getName())
            << ",\n  \"material_name\":" << std::quoted(object.getSection(0)->getMaterialName())
            << ",\n  \"operation\":\"triangle_list\",\n  \"vertex_start\":" << op.vertexData->vertexStart
            << ",\n  \"vertex_count\":" << op.vertexData->vertexCount
            << ",\n  \"index_start\":" << op.indexData->indexStart
            << ",\n  \"index_count\":" << op.indexData->indexCount
            << ",\n  \"index_type\":\""
            << (op.indexData->indexBuffer->getType()==Ogre::HardwareIndexBuffer::IT_16BIT?"u16":"u32") << "\",\n"
            << "  \"atlas_pixels\":" << profile.atlasPixels << ",\n  \"tile_pixels\":" << profile.tilePixels
            << ",\n  \"tiles_per_row\":" << profile.tilesPerRow
            << ",\n  \"profile_version\":" << profile.formatVersion
            << ",\n  \"uses_array\":" << (runtimeTerrainMaterialProfile().usesTextureArray()?"true":"false")
            << ",\n  \"atlas_source\":" << std::quoted(runtimeResourcePackResolver().resolve(profile.atlasTexture))
            << ",\n  \"array_source\":" << std::quoted(profile.arrayTexture.empty()?std::string():
                runtimeResourcePackResolver().resolve(profile.arrayTexture))
            << ",\n  \"vertex_buffers\":[\n";
    bool first=true;
    for (const auto& binding:op.vertexData->vertexBufferBinding->getBindings()) {
        const std::string file=name+".vbo"+std::to_string(binding.first)+".bin";
        std::vector<unsigned char> bytes(binding.second->getSizeInBytes());
        binding.second->readData(0,bytes.size(),bytes.data());
        std::ofstream binary(output+"/"+file,std::ios::binary);
        binary.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        if (!first) receipt << ",\n";
        first=false;
        receipt << "    {\"source\":" << binding.first << ",\"stride\":" << binding.second->getVertexSize()
                << ",\"vertices\":" << binding.second->getNumVertices() << ",\"bytes\":" << bytes.size()
                << ",\"file\":" << std::quoted(file) << '}';
    }
    receipt << "\n  ],\n  \"elements\":[\n";
    first=true;
    for (const auto& element:op.vertexData->vertexDeclaration->getElements()) {
        if (!first) receipt << ",\n";
        first=false;
        receipt << "    {\"source\":" << element.getSource() << ",\"offset\":" << element.getOffset()
                << ",\"semantic\":" << int(element.getSemantic()) << ",\"semantic_index\":" << element.getIndex()
                << ",\"type\":" << int(element.getType()) << ",\"components\":" << Ogre::VertexElement::getTypeCount(element.getType())
                << ",\"bytes\":" << element.getSize() << '}';
    }
    const std::string indexFile=name+".ibo.bin";
    std::vector<unsigned char> bytes(op.indexData->indexBuffer->getSizeInBytes());
    op.indexData->indexBuffer->readData(0,bytes.size(),bytes.data());
    std::ofstream binary(output+"/"+indexFile,std::ios::binary);
    binary.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    receipt << "\n  ],\n  \"index_buffer\":{\"file\":" << std::quoted(indexFile) << ",\"bytes\":" << bytes.size() << "},\n"
            << "  \"world_transform_row_major\":[";
    object.getParentSceneNode()->_update(true,false);
    const auto& transform=object.getParentSceneNode()->_getFullTransform();
    for (int row=0;row<4;++row) for (int column=0;column<4;++column) {
        if (row || column) receipt << ',';
        receipt << std::setprecision(9) << transform[row][column];
    }
    receipt << "]\n}\n";
    if (!receipt || !binary) throw std::runtime_error("Failed to export actual operation bytes");
}
Ogre::ManualObject& heldObject(Ogre::SceneManager& scene)
{
    auto objects=scene.getMovableObjectIterator("ManualObject");
    Ogre::ManualObject* result=nullptr;
    while (objects.hasMoreElements()) {
        auto* object=dynamic_cast<Ogre::ManualObject*>(objects.getNext());
        if (object && object->getName().find("_HeldItemMesh") != std::string::npos) {
            if (result) throw std::runtime_error("Ambiguous real held object");
            result=object;
        }
    }
    if (!result) throw std::runtime_error("Missing actual held ManualObject");
    return *result;
}
bool safeTexelCentre(float coordinate,int tile,const TerrainMaterialParameters& profile)
{
    // The independent texel-centre requirement is expressed in physical
    // pixels, not by reproducing a producer's normalized UV formula.
    const double local=double(coordinate)*profile.atlasPixels-double(tile)*profile.tilePixels;
    return std::isfinite(local) && local>=.49 && local<=.51;
}
void cube(const std::string& prefix, const Case& c, const Mesh& mesh,
          const TerrainMaterialParameters& profile)
{
    const int tiles=profile.tilesPerRow;
    bool layout=mesh.software && mesh.vertices.size()==24 && mesh.indices==36;
    bool tile=layout, repeat=layout, root=layout, facing=layout, safeOrigin=layout;
    const std::array<std::array<float,2>,4> expectedRepeat{{{{0,0}},{{0,1}},{{1,1}},{{1,0}}}};
    const std::array<Ogre::Vector3,6> normals{{Ogre::Vector3::UNIT_Z,Ogre::Vector3::NEGATIVE_UNIT_Z,
        Ogre::Vector3::UNIT_X,Ogre::Vector3::NEGATIVE_UNIT_X,Ogre::Vector3::UNIT_Y,Ogre::Vector3::NEGATIVE_UNIT_Y}};
    if (layout) for (std::size_t face=0;face<6;++face) {
        const int x=face<4 ? c.x : c.topX, y=face<4 ? c.sideY : c.topY;
        for (std::size_t corner=0;corner<4;++corner) {
            const auto& v=mesh.vertices[face*4+corner];
            // This multiplication stays float, matching the production
            // shader's cell-selection arithmetic. Exact tile/N UV equality
            // is not a semantic requirement and can select N23 tile7 as6.
            tile &= int(std::floor(v.tile[0]*tiles))==x && int(std::floor(v.tile[1]*tiles))==y;
            safeOrigin &= safeTexelCentre(v.tile[0],x,profile) && safeTexelCentre(v.tile[1],y,profile);
            repeat &= near(v.repeat[0],expectedRepeat[corner][0]) && near(v.repeat[1],expectedRepeat[corner][1]);
            root &= v.root==0.f;
        }
        const auto& a=mesh.vertices[face*4].position;
        const auto& b=mesh.vertices[face*4+1].position;
        const auto& d=mesh.vertices[face*4+2].position;
        const Ogre::Vector3 p(a[0],a[1],a[2]),q(b[0],b[1],b[2]),r(d[0],d[1],d[2]);
        facing &= (q-p).crossProduct(r-p).normalisedCopy().dotProduct(normals[face])>.9999f;
    }
    check(prefix+"/actual-software-six-face-layout",layout && facing);
    check(prefix+"/semantic-tile-routing",tile);
    check(prefix+"/repeat-and-explicit-root-zero",repeat && root);
    check("NEW_HALF_TEXEL/"+prefix+"/safe-cell-origin",safeOrigin);
    if (!mesh.vertices.empty()) {
        const auto& v=mesh.vertices.front();
        std::cout << "[ITEM_BINDING_DATA] " << prefix << " face0_uv=" << v.tile[0] << ',' << v.tile[1]
                  << " target=" << std::floor(v.tile[0]*tiles) << ',' << std::floor(v.tile[1]*tiles)
                  << " expected=" << c.x << ',' << c.sideY << " uv1=" << v.repeat[0] << ',' << v.repeat[1]
                  << " root=" << v.root << '\n';
    }
}
std::array<bool,256> iconMask(const TerrainMaterialParameters& profile, bool custom)
{
    const std::string path=runtimeResourcePackResolver().resolve(profile.atlasTexture);
    std::ifstream input(path,std::ios::binary);
    std::vector<char> bytes{std::istreambuf_iterator<char>(input),{}};
    Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream(bytes.data(),bytes.size(),false,true));
    Ogre::Image atlas;
    atlas.load(stream,"png");
    check("RESOURCE/actual-PNG-decoded-dimensions",atlas.getWidth()==std::size_t(profile.atlasPixels) &&
          atlas.getHeight()==std::size_t(profile.atlasPixels));
    std::array<bool,256> mask{};
    bool independentPattern=true;
    for (int y=0;y<16;++y) for (int x=0;x<16;++x) {
        // Physical tile origin uses the frozen profile. Centre samples cover
        // the 16x16 contour grid without consulting the production cache.
        mask[y*16+x]=atlas.getColourAt(2*profile.tilePixels+(2*x+1)*profile.tilePixels/32,
            2*profile.tilePixels+(2*y+1)*profile.tilePixels/32,0).a>=.5f;
        if (custom) independentPattern &= mask[y*16+x] ==
            ((x==2 && y==3) || (x==9 && y==6) || (x==12 && y==12));
    }
    check("RESOURCE/icon-alpha-independent-effective-source",independentPattern &&
          std::count(mask.begin(),mask.end(),true)>0);
    return mask;
}
void icon(const std::string& prefix, const Mesh& mesh,const std::array<bool,256>& mask,
          const TerrainMaterialParameters& profile)
{
    const int tiles=profile.tilesPerRow;
    const auto opaque=[&](int x,int y) { return x>=0 && x<16 && y>=0 && y<16 && mask[y*16+x]; };
    std::size_t boundary=0;
    for (int y=0;y<16;++y) for (int x=0;x<16;++x) if (opaque(x,y))
        boundary += !opaque(x-1,y)+!opaque(x+1,y)+!opaque(x,y-1)+!opaque(x,y+1);
    const std::size_t faces=2+boundary;
    bool shape=mesh.software && mesh.vertices.size()==faces*4 && mesh.indices==faces*6;
    bool routes=!mesh.vertices.empty(), roots=routes, wallSamples=shape, safeOrigin=routes;
    for (std::size_t i=0;i<mesh.vertices.size();++i) {
        const auto& v=mesh.vertices[i];
        routes &= int(std::floor(v.tile[0]*tiles))==2 && int(std::floor(v.tile[1]*tiles))==2;
        safeOrigin &= safeTexelCentre(v.tile[0],2,profile) && safeTexelCentre(v.tile[1],2,profile);
        roots &= v.root==0.f;
        if (i>=8) {
            const int x=int(std::floor(v.repeat[0]*16)),y=int(std::floor(v.repeat[1]*16));
            wallSamples &= opaque(x,y) && near(v.repeat[0],(x+.5f)/16.f) && near(v.repeat[1],(y+.5f)/16.f);
        }
    }
    check(prefix+"/effective-alpha-extrusion-perimeter",shape && wallSamples);
    check(prefix+"/semantic-tile-and-root-zero",routes && roots);
    check("NEW_HALF_TEXEL/"+prefix+"/safe-cell-origin",safeOrigin);
    std::cout << "[ITEM_BINDING_DATA] " << prefix << " vertices=" << mesh.vertices.size()
              << " expected_vertices=" << faces*4 << " opaque_pixels=" << std::count(mask.begin(),mask.end(),true)
              << " independent_boundary_edges=" << boundary << '\n';
}
} // namespace

int main(int argc,char** argv)
{
    if (argc!=5) { std::cerr << "Usage: item-binding-test root default|legacy32|legacy23 pack-or-empty output-dir\n"; return 2; }
    try {
        const std::string rootPath=argv[1],mode=argv[2],pack=argv[3],output=argv[4];
        if (mode!="default" && mode!="legacy32" && mode!="legacy23") throw std::runtime_error("Invalid mode");
        const bool custom=mode!="default";
        const int expectedTiles=mode=="legacy23" ? 23 : custom ? 32 : 16;
        setenv("HELLOMINE3D_ROOT",rootPath.c_str(),1);
        const auto resources=loadStartupResourceManifest(rootPath);
        std::vector<ResourcePackRequirement> requirements;
        for (const auto& resource:resources) requirements.push_back({resource.category,resource.relativePath});
        runtimeResourcePackResolver().freeze(rootPath,requirements,custom ? std::vector<std::string>{pack} : std::vector<std::string>{});
        validateStartupResources(rootPath,resources);
        runtimeTerrainMaterialProfile().freezeFromResourceView(runtimeResourcePackResolver());
        runtimeTerrainMaterialProfile().freezeRenderingMode(true,true);
        const auto& profile=runtimeTerrainMaterialProfile().parameters();
        check("RESOURCE/legal-profile-parser-and-effective-view",profile.atlasPixels==expectedTiles*16 &&
              profile.tilePixels==16 && profile.tilesPerRow==expectedTiles && profile.formatVersion==(custom?1:2));
        check("RESOURCE/complete-frozen-mode",runtimeTerrainMaterialProfile().usesTextureArray()==!custom &&
              runtimeTerrainMaterialProfile().renderingModeReason()==(custom?"legacy-resource-profile":"standard-64"));
        check("RESOURCE/pack-override-count",runtimeResourcePackResolver().overrideCount()==(custom?2u:0u));
        std::ofstream(output+"/effective-manifest.txt") << runtimeResourcePackResolver().effectiveManifest();
        std::cout << "[ITEM_BINDING_PROFILE] mode=" << mode << " version=" << profile.formatVersion
                  << " atlas=" << profile.atlasPixels << " tile=" << profile.tilePixels << " tiles=" << profile.tilesPerRow
                  << " reason=" << runtimeTerrainMaterialProfile().renderingModeReason() << '\n';
        // First registry and cache access occurs only after real resource/profile freezing.
        const auto& blocks=BlockDatabase::get();
        Ogre::Root root("","",output+"/ogre.log");
        Ogre::DefaultHardwareBufferManager buffers;
        for (const char* name:{"HelloMine3D/ActorPlayer","HelloMine3D/PlayerHeld",
             "HelloMine3D/PlayerHeldTransparent","HelloMine3D/Terrain","HelloMine3D/Transparent"})
            Ogre::MaterialManager::getSingleton().create(name,Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        auto* scene=root.createSceneManager(Ogre::ST_GENERIC,"ItemBindingOracle");
        {
            OgrePlayerRenderer held(*scene);
            OgreActorRenderer dropped(*scene);
            const auto avatar=PlayerAvatarPresentation::defaultProfile();
            const auto pose=PlayerAvatarPresentation::derivePose({},avatar,PlayerAvatarPresentation::MotionStrength::Off);
            ActorId id=100;
            for (const auto& c:Cases) {
                const auto iconCoord=Material::iconCoordinate(c.material);
                const auto& block=blocks.getDefinition(c.block);
                check(std::string("AUTHORITY/")+c.name,Material::toMaterial(c.material).toBlockID()==c.block &&
                    iconCoord.x==c.x && iconCoord.y==c.sideY &&
                    block.render.texSideCoord==glm::ivec2(c.x,c.sideY) &&
                    block.render.texTopCoord==glm::ivec2(c.topX,c.topY) &&
                    block.render.texBottomCoord==glm::ivec2(c.topX,c.topY));
                held.sync(avatar,pose,true,c.material);
                cube(std::string("HELD/")+c.name,c,readMesh(heldObject(*scene)),profile);
                dumpMesh(heldObject(*scene),output,std::string("held-")+c.name,c.material);
                ItemEntity entity(++id,c.material,7,glm::vec3(4,2,0));
                const ActorSnapshot snapshot=entity.getSnapshot();
                dropped.sync({snapshot},glm::vec3(0,2,0),0.f,0.f);
                auto* object=dynamic_cast<Ogre::ManualObject*>(scene->getMovableObject("Actor_"+std::to_string(id)+"_Mesh","ManualObject"));
                if (!object) throw std::runtime_error("Missing real ItemEntity drop object");
                cube(std::string("DROP/")+c.name,c,readMesh(*object),profile);
                dumpMesh(*object,output,std::string("drop-")+c.name,c.material);
                check(std::string("SNAPSHOT/")+c.name,snapshot.type=="item" && snapshot.itemMaterialId==c.material &&
                    snapshot.itemAmount==7 && entity.getSnapshot().itemMaterialId==c.material && entity.getAmount()==7);
            }
            const auto mask=iconMask(profile,custom);
            held.sync(avatar,pose,true,Material::WoodenPickaxe);
            icon("HELD/WoodenPickaxe",readMesh(heldObject(*scene)),mask,profile);
            dumpMesh(heldObject(*scene),output,"held-WoodenPickaxe",Material::WoodenPickaxe);
            ItemEntity entity(++id,Material::WoodenPickaxe,1,glm::vec3(4,2,0));
            dropped.sync({entity.getSnapshot()},glm::vec3(0,2,0),0.f,0.f);
            auto* object=dynamic_cast<Ogre::ManualObject*>(scene->getMovableObject("Actor_"+std::to_string(id)+"_Mesh","ManualObject"));
            if (!object) throw std::runtime_error("Missing real icon drop object");
            icon("DROP/WoodenPickaxe",readMesh(*object),mask,profile);
            dumpMesh(*object,output,"drop-WoodenPickaxe",Material::WoodenPickaxe);
        }
        check("LIFECYCLE/actual-renderers-release-nodes",scene->getRootSceneNode()->numChildren()==0);
        check("SCOPE/no-render-system-window-context-or-GPU",root.getRenderSystem()==nullptr);
        root.destroySceneManager(scene);
    } catch (const std::exception& error) {
        check("unexpected-exception",false);
        std::cerr << error.what() << '\n';
    }
    std::cout << "[ITEM_BINDING] checks=" << checks << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
