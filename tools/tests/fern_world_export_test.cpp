// Real resident World -> normal mesh update -> copied snapshot -> production
// ChunkSectionRenderable software buffers. No Camera/RenderSystem/window/GPU.
#include "Core/Camera.h"
#include "Player/Player.h"
#include "World/World.h"
#include "World/Block/BlockDatabase.h"
#include "World/Block/TerrainMaterialProfile.h"
#include "Feedback/BlockSurfaceGeometry.h"
#include "Util/ResourcePackResolver.h"
#include "Ogre/StartupResourcePreflight.h"
#include "Ogre/ChunkSectionRenderable.h"
#include <Ogre.h>
#include <OgreDefaultHardwareBufferManager.h>
#include <FreeImage.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {
namespace fs=std::filesystem;
int checks=0,failures=0;
void check(const std::string& name,bool value){++checks;failures+=!value;std::cout<<"[FERN_WORLD] "<<(value?"PASS ":"FAIL ")<<name<<'\n';}
void require(bool value,const std::string& message){check(message,value);if(!value)throw std::runtime_error(message);}
void writeBytes(const fs::path& path,const std::vector<unsigned char>& bytes){std::ofstream s(path,std::ios::binary);s.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!s)throw std::runtime_error("Cannot write "+path.string());}
using Chunks=std::map<std::pair<int,int>,std::tuple<std::uint64_t,int>>;
Chunks chunks(World& world){Chunks r;for(const auto& p:world.getChunkManager().getChunks())r[{p.first.x,p.first.z}]={p.second.getIncarnation(),int(p.second.getDataResidencyState())};return r;}
struct SceneOwner{Ogre::Root& root;Ogre::SceneManager* scene;~SceneOwner(){root.destroySceneManager(scene);}};
struct Source{
    glm::ivec3 position{},section{};ChunkBlock block;TerrainBiome biome{};float scale=0;
    int seed=0,version=0,sun=0,light=0;std::uint32_t revision=0;
    std::size_t vb=0,ve=0,ib=0,ie=0;std::vector<BlockSurfaceFace> feedback;
};
Source sourceFacts(World& world,glm::ivec3 p){
    Source s;s.position=p;s.section={int(std::floor(p.x/16.)),int(std::floor(p.y/16.)),int(std::floor(p.z/16.))};
    s.block=world.getBlock(p.x,p.y,p.z);require(s.block==ChunkBlock(BlockId::TallGrass,BlockMetadata::TallGrass::Fern),"source/actual-Fern-id10-metadata2");
    auto& manager=world.getChunkManager();s.seed=manager.getTerrainSeed();s.version=manager.getTerrainGenerationVersion();s.biome=manager.getTerrainGenerator().getBiomeAtWorld(p.x,p.z);
    const auto& d=BlockDatabase::get().getDefinition(static_cast<BlockId>(s.block.id));s.scale=d.behavior->verticalRenderScale(d,s.block);
    s.feedback=blockSurfaceGeometry(d,s.block,p,s.biome,s.seed);require(s.feedback.size()==6&&s.scale==1,"source/registered-six-face-scale1");
    s.sun=world.getSunlight(p.x,p.y,p.z);s.light=world.getBlockLight(p.x,p.y,p.z);
    auto* c=manager.findChunk(s.section.x,s.section.z);require(c&&c->hasLoaded(),"source/actual-resident-column");auto* section=c->findSection(s.section.y);require(section!=nullptr,"source/actual-section");s.revision=section->getBlockRevision();return s;
}
void identify(Source& s,const ChunkMesh& mesh){
    const auto& m=mesh.getClientMesh();std::vector<std::size_t> vertices,indices;
    for(std::size_t v=0;v<m.vertexPositions.size()/3;++v){const glm::vec3 p(m.vertexPositions[3*v],m.vertexPositions[3*v+1],m.vertexPositions[3*v+2]);const auto q=p-glm::vec3(s.position);if(q.x>0&&q.x<1&&q.y>0&&q.y<1&&q.z>0&&q.z<1)vertices.push_back(v);}
    require(vertices.size()==24,"mesh/actual-Fern-24-source-vertices");s.vb=vertices.front();s.ve=vertices.back()+1;
    require(s.ve-s.vb==24,"mesh/source-vertices-contiguous");
    for(std::size_t i=0;i<m.indices.size();i+=3){unsigned inside=0;for(unsigned j=0;j<3;++j)inside+=m.indices[i+j]>=s.vb&&m.indices[i+j]<s.ve;require(inside==0||inside==3,"mesh/no-cross-source-triangle");if(inside==3)for(unsigned j=0;j<3;++j)indices.push_back(i+j);}
    require(indices.size()==36,"mesh/actual-Fern-six-faces-36-indices");s.ib=indices.front();s.ie=indices.back()+1;require(s.ie-s.ib==36,"mesh/source-indices-contiguous");
    std::size_t roots=0;float minY=1,maxY=0;
    for(std::size_t n=0;n<24;++n){const auto& f=s.feedback[n/4];const auto v=s.vb+n;for(unsigned k=0;k<3;++k)require(std::abs(m.vertexPositions[v*3+k]-(f.positions[(n%4)*3+k]+s.position[k]))<2e-5f,"feedback/real-mesh-position-agreement");
        for(unsigned k=0;k<2;++k)require(m.textureRepeatCoords[v*2+k]==f.repeat[(n%4)*2+k],"feedback/real-mesh-repeat-agreement");
        const float y=m.vertexPositions[v*3+1]-s.position.y;minY=std::min(minY,y);maxY=std::max(maxY,y);roots+=n%4==0;
        require(std::abs(mesh.getLightSources()[v].x-s.sun/15.f)<1e-6f&&std::abs(mesh.getLightSources()[v].y-s.light/15.f)<1e-6f,"mesh/actual-World-light-source-agreement");
        require(std::isfinite(mesh.getLight()[v])&&mesh.getLight()[v]>=0&&mesh.getLight()[v]<=1&&mesh.getRootTags()[v]==0,"mesh/finite-light-no-tree-root-tag");
        require(f.tile==s.feedback.front().tile,"feedback/actual-Grass-top-one-cell");
    }
    require(roots==6&&std::abs(minY-.035f)<2e-5f&&std::abs(maxY-.495f)<2e-5f,"mesh/six-raised-roots-and-visible-fronds");
    // Independent magnitude bound from |sin|<=1, not a CPU copy of floraWind.
    const double rootHeight=1.0-m.textureRepeatCoords[s.vb*2+1];
    const double windBound=.085*std::hypot(.88,.48)*1.25;
    const double rootBound=windBound*rootHeight*rootHeight*rootHeight;
    const float ulp=std::nextafter(m.vertexPositions[s.vb*3],INFINITY)-m.vertexPositions[s.vb*3];
    check("math/raised-root-nonzero-weight-under-1e-5m",rootHeight>0&&rootBound>0&&rootBound<1e-5);
    std::cout<<std::setprecision(12)<<"[FERN_MATH] world="<<s.position.x<<','<<s.position.y<<','<<s.position.z<<" root_height="<<rootHeight<<" displacement_bound_m="<<rootBound<<" world_x_ulp="<<ulp<<'\n';
}
void sourceJson(std::ostream& o,const Source& s,std::size_t vb,std::size_t ib){
    o<<"{\"block_x\":"<<s.position.x<<",\"block_y\":"<<s.position.y<<",\"block_z\":"<<s.position.z<<",\"block_id\":"<<int(s.block.id)<<",\"metadata\":"<<int(s.block.metadata)
     <<",\"height_scale\":"<<s.scale<<",\"biome\":"<<int(s.biome)<<",\"seed\":"<<s.seed<<",\"terrain_generation_version\":"<<s.version
     <<",\"section_x\":"<<s.section.x<<",\"section_y\":"<<s.section.y<<",\"section_z\":"<<s.section.z<<",\"block_revision\":"<<s.revision
     <<",\"vertex_begin\":"<<s.vb+vb<<",\"vertex_end\":"<<s.ve+vb<<",\"index_begin\":"<<s.ib+ib<<",\"index_end\":"<<s.ie+ib
     <<",\"face_count\":6,\"tile_x\":"<<s.feedback.front().tile.x<<",\"tile_y\":"<<s.feedback.front().tile.y<<",\"sunlight\":"<<s.sun<<",\"block_light\":"<<s.light<<'}';
}
std::vector<std::array<float,11>> exportOperation(Ogre::SceneManager& scene,const fs::path& out,const std::string& name,const std::vector<TerrainRenderBatchPart>& parts,glm::ivec3 origin,const std::vector<Source>& sources){
    ChunkSectionRenderable object(name,parts,origin,"FernSoftwareExport",6);object.setCastShadows(false);
    auto* node=scene.getRootSceneNode()->createChildSceneNode(name+"Node",Ogre::Vector3(origin.x*16,origin.y*16,origin.z*16));node->attachObject(&object);node->_update(true,false);
    Ogre::RenderOperation op;object.getRenderOperation(op);require(op.useIndexes&&op.operationType==Ogre::RenderOperation::OT_TRIANGLE_LIST&&op.vertexData&&op.indexData,"operation/actual-indexed-triangle-list");
    require(op.vertexData->vertexStart==0&&op.indexData->indexStart==0&&op.indexData->indexBuffer->getType()==Ogre::HardwareIndexBuffer::IT_32BIT,"operation/original-u32-no-offset");
    const auto& bindings=op.vertexData->vertexBufferBinding->getBindings();require(bindings.size()==1&&bindings.begin()->first==0,"operation/one-real-interleaved-VBO");auto buffer=bindings.begin()->second;
    require(dynamic_cast<Ogre::DefaultHardwareVertexBuffer*>(buffer.get())&&buffer->getVertexSize()==44,"operation/actual-software-44B-buffer");
    require(dynamic_cast<Ogre::DefaultHardwareIndexBuffer*>(op.indexData->indexBuffer.get()),"operation/actual-software-index-buffer");
    require(buffer->getSizeInBytes()<=16*1024*1024&&op.indexData->indexBuffer->getSizeInBytes()<=16*1024*1024,"operation/bounded-original-buffers");
    std::vector<unsigned char> raw(buffer->getSizeInBytes());buffer->readData(0,raw.size(),raw.data());writeBytes(out/(name+".vbo0.bin"),raw);
    std::vector<unsigned char> idx(op.indexData->indexBuffer->getSizeInBytes());op.indexData->indexBuffer->readData(0,idx.size(),idx.data());writeBytes(out/(name+".ibo.bin"),idx);
    std::vector<std::array<float,11>> vertices(op.vertexData->vertexCount);buffer->readData(0,vertices.size()*44,vertices.data());const auto matrix=node->_getFullTransform();
    const auto& profile=runtimeTerrainMaterialProfile().parameters();auto& resolver=runtimeResourcePackResolver();
    std::ofstream o(out/(name+".json"));o<<std::setprecision(9)<<"{\"schema\":\"hellomine3d-item-binding-raw-operation-v1\",\"scope\":\"real World copied mesh and production software VBO/IBO; no GPU upload or shader binding\",\"name\":"<<std::quoted(name)<<",\"object_name\":"<<std::quoted(object.getName())<<",\"material_name\":\"FernSoftwareExport\",\"production_material_name\":\"HelloMine3D/Flora\",\"operation\":\"triangle_list\",\"vertex_start\":"<<op.vertexData->vertexStart<<",\"vertex_count\":"<<op.vertexData->vertexCount<<",\"index_start\":"<<op.indexData->indexStart<<",\"index_count\":"<<op.indexData->indexCount<<",\"index_type\":\"u32\",\"atlas_pixels\":"<<profile.atlasPixels<<",\"tile_pixels\":"<<profile.tilePixels<<",\"tiles_per_row\":"<<profile.tilesPerRow<<",\"profile_version\":"<<profile.formatVersion<<",\"uses_array\":"<<(runtimeTerrainMaterialProfile().usesTextureArray()?"true":"false")<<",\"atlas_source\":"<<std::quoted(resolver.resolve(profile.atlasTexture))<<",\"array_source\":"<<std::quoted(resolver.resolve(profile.arrayTexture))<<",\"profile_source\":"<<std::quoted(resolver.resolve(TerrainMaterialParameters::LogicalPath))<<",\"layout_source\":\"\",\"layout_scope\":\"fixed built-in semantic cells; actual block resource sources retained\",\"block_source\":"<<std::quoted(resolver.resolve("media/blocks/TallGrass.block"))<<",\"grass_source\":"<<std::quoted(resolver.resolve("media/blocks/Grass.block"))<<",\"queue_group\":"<<int(object.getRenderQueueGroup())<<",\"casts_shadows\":"<<(object.getCastShadows()?"true":"false")<<",\"growth_scaling\":\"NOT_APPLICABLE: actual registered Fern scale is 1\",\"vertex_buffers\":[{\"source\":0,\"stride\":44,\"vertices\":"<<buffer->getNumVertices()<<",\"bytes\":"<<raw.size()<<",\"file\":"<<std::quoted(name+".vbo0.bin")<<"}],\"elements\":[";
    bool first=true;for(const auto& e:op.vertexData->vertexDeclaration->getElements()){if(!first)o<<',';first=false;o<<"{\"source\":"<<e.getSource()<<",\"offset\":"<<e.getOffset()<<",\"semantic\":"<<int(e.getSemantic())<<",\"semantic_index\":"<<e.getIndex()<<",\"type\":"<<int(e.getType())<<",\"components\":"<<Ogre::VertexElement::getTypeCount(e.getType())<<",\"bytes\":"<<e.getSize()<<'}';}
    o<<"],\"index_buffer\":{\"file\":"<<std::quoted(name+".ibo.bin")<<",\"bytes\":"<<idx.size()<<"},\"world_transform_row_major\":[";for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){if(r||c)o<<',';o<<matrix[r][c];}o<<"],\"source_blocks\":[";
    std::size_t vb=0,ib=0;first=true;for(const auto& p:parts){for(const auto& s:sources)if(s.section==p.location){if(!first)o<<',';first=false;sourceJson(o,s,vb,ib);}vb+=p.mesh->getClientMesh().vertexPositions.size()/3;ib+=p.mesh->getClientMesh().indices.size();}
    o<<"]}\n";if(!o)throw std::runtime_error("Cannot export JSON");
    for(auto& v:vertices){const Ogre::Vector3 p=matrix*Ogre::Vector3(v[0],v[1],v[2]);v[0]=p.x;v[1]=p.y;v[2]=p.z;}
    node->detachObject(&object);scene.destroySceneNode(node);return vertices;
}
void locateNatural(World& world,const fs::path& out,int x,int z){
    const auto before=chunks(world);const auto& manager=world.getChunkManager();std::vector<glm::ivec3> found;std::size_t queries=0;bool known=true;
    for(int dz=-16;dz<16&&found.size()<2;++dz)for(int dx=-16;dx<16&&found.size()<2;++dx){auto* c=world.getChunkManager().findChunk(int(std::floor((x+dx)/16.)),int(std::floor((z+dz)/16.)));known &= c&&c->hasLoaded();if(!c||!c->hasLoaded())continue;for(int y=0;y<192&&found.size()<2;++y){++queries;if(world.getBlock(x+dx,y,z+dz)==ChunkBlock(BlockId::TallGrass,BlockMetadata::TallGrass::Fern))found.push_back({x+dx,y,z+dz});}}
    check("natural/bounded-only-resident-query-window",known&&queries<=32*32*192&&chunks(world)==before);check("natural/two-real-generated-Fern-found",found.size()==2);
    std::ofstream o(out/"natural-locate.json");o<<"{\"schema\":\"hellomine3d-fern-natural-locator-v1\",\"seed\":"<<manager.getTerrainSeed()<<",\"terrain_generation_version\":"<<manager.getTerrainGenerationVersion()<<",\"centre_x\":"<<x<<",\"centre_z\":"<<z<<",\"queries\":"<<queries<<",\"known_window\":"<<(known?"true":"false")<<",\"chunks_unchanged\":"<<(chunks(world)==before?"true":"false")<<",\"sources\":[";for(std::size_t i=0;i<found.size();++i){if(i)o<<',';const auto s=sourceFacts(world,found[i]);sourceJson(o,s,0,0);}o<<"]}\n";if(!o)throw std::runtime_error("Cannot write natural locator");
}
}
int main(int argc,char** argv){
    if(argc!=4&&argc!=7){std::cerr<<"Usage: fern-world-test repository-root fresh-output-directory standard|compatibility [--locate-natural centre-x centre-z]\n";return 2;}
    try{
        const fs::path repo=fs::absolute(argv[1]),out=fs::absolute(argv[2]);const std::string mode=argv[3];if(mode!="standard"&&mode!="compatibility")throw std::runtime_error("Invalid mode");if(fs::exists(out))throw std::runtime_error("Output must be fresh");fs::create_directories(out);
        const bool natural=argc==7;if(natural&&std::string(argv[4])!="--locate-natural")throw std::runtime_error("Invalid locator option");const int centreX=natural?std::stoi(argv[5]):0,centreZ=natural?std::stoi(argv[6]):0;
        const std::string position=std::to_string(centreX+.5)+" 216 "+std::to_string(centreZ+.5);setenv("HELLOMINE3D_ROOT",repo.c_str(),1);setenv("HELLOMINE3D_SEED","20260807",1);setenv("HELLOMINE3D_PLAYER_POSITION",position.c_str(),1);setenv("HELLOMINE3D_PLAYER_ROTATION","0 0 0",1);
        FreeImage_Initialise(FALSE);const auto resources=loadStartupResourceManifest(repo.string());std::vector<ResourcePackRequirement> requirements;for(const auto& r:resources)requirements.push_back({r.category,r.relativePath});runtimeResourcePackResolver().freeze(repo.string(),requirements,{});validateStartupResources(repo.string(),resources);runtimeTerrainMaterialProfile().freezeFromResourceView(runtimeResourcePackResolver());runtimeTerrainMaterialProfile().freezeRenderingMode(mode=="standard",true);BlockDatabase::get();
        Ogre::Root root("","",(out/"ogre.log").string());Ogre::DefaultHardwareBufferManager buffers;
        // Empty diagnostic techniques: SimpleRenderable::setMaterial loads this
        // material without Technique::_compile querying a null RenderSystem.
        auto material=Ogre::MaterialManager::getSingleton().create("FernSoftwareExport",Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);material->removeAllTechniques();
        auto* scene=root.createSceneManager(Ogre::ST_GENERIC,"FernWorldSoftware");SceneOwner sceneOwner{root,scene};
        check("setup/no-RenderSystem-no-context-no-window",root.getRenderSystem()==nullptr&&!root.isInitialised()&&root.getAutoCreatedWindow()==nullptr);check("setup/diagnostic-material-has-no-GPU-technique",material->getNumTechniques()==0);
        Config config;config.worldSeed=20260807;config.renderDistance=1;::Camera logic(config);Player player;World world(logic,config,player,(out/"isolated-save").string(),false,1);logic.hookEntity(player);logic.update();const auto playerBefore=player.getSaveState();const auto initial=chunks(world);
        require(initial.size()==9,"World/actual-nine-preloaded-chunks");
        if(natural)locateNatural(world,out,centreX,centreZ);else{
            const std::array<glm::ivec3,2> positions{{{4,200,4},{12,232,12}}};for(auto p:positions){require(world.getBlock(p.x,p.y,p.z)==BlockId::Air,"fixture/source-was-resident-Air");world.setBlock(p.x,p.y,p.z,{BlockId::TallGrass,BlockMetadata::TallGrass::Fern});}
            std::vector<Source> sources;for(auto p:positions)sources.push_back(sourceFacts(world,p));const auto edited=chunks(world);std::map<int,WorldSectionMeshSnapshot> selected;unsigned updates=0;
            for(;updates<256&&selected.size()!=2;++updates){world.update(logic);auto snapshot=world.collectSectionMeshSnapshot(false);require(snapshot.cpuReadySections.size()<=8,"snapshot/normal-bounded-offer");for(auto& section:snapshot.cpuReadySections)for(const auto& s:sources)if(section.location==s.section){require(section.blockRevision==s.revision,"snapshot/real-current-block-revision");selected[section.location.y]=std::move(section);break;}}
            require(selected.size()==2,"World/normal-update-offered-both-real-Fern-sections");require(chunks(world)==edited&&edited==initial,"World/no-new-chunks-incarnations-or-residency-change");
            for(auto& s:sources){auto& mesh=selected.at(s.section.y).meshes.floraMesh;require(mesh.faces==6&&mesh.getClientMesh().vertexPositions.size()==72&&mesh.getClientMesh().indices.size()==36,"section/isolated-six-face-24-36-production-flora");identify(s,mesh);require(ChunkSectionRenderable::validateCpuMesh(mesh,s.section).valid,"section/production-CPU-validation");}
            std::vector<TerrainRenderBatchPart> parts;std::vector<std::array<float,11>> single;for(std::size_t i=0;i<sources.size();++i){auto& s=sources[i];parts.push_back({s.section,&selected.at(s.section.y).meshes.floraMesh});auto v=exportOperation(*scene,out,i==0?"section-a":"section-b",{parts.back()},s.section,{s});single.insert(single.end(),v.begin(),v.end());}
            const auto origin=terrainRenderBatchOrigin(sources.front().section);const auto batch=exportOperation(*scene,out,"batch",parts,origin,sources);require(single.size()==batch.size(),"batch/same-real-source-vertex-count");for(std::size_t i=0;i<batch.size();++i)for(unsigned a=0;a<11;++a)require(std::abs(single[i][a]-batch[i][a])<2e-5f,"batch/actual-world-transform-and-all-original-attributes-agree");
            std::ofstream manifest(out/"manifest.json");manifest<<"{\"schema\":\"hellomine3d-fern-world-export-v1\",\"mode\":"<<std::quoted(mode)<<",\"scope\":\"isolated actual World CPU mesh; software original buffers; GPU upload/visible client attribution NOT_RUN\",\"seed\":"<<world.getChunkManager().getTerrainSeed()<<",\"terrain_generation_version\":"<<world.getChunkManager().getTerrainGenerationVersion()<<",\"updates\":"<<updates<<",\"operations\":[\"section-a.json\",\"section-b.json\",\"batch.json\"],\"unique_source_vertices\":48,\"unique_source_indices\":72,\"batch_origin\":["<<origin.x<<','<<origin.y<<','<<origin.z<<"],\"batch_height_sections\":3,\"natural_generated_source\":false,\"growth_scaling\":\"NOT_APPLICABLE\"}\n";if(!manifest)throw std::runtime_error("Cannot write manifest");
        }
        const auto after=player.getSaveState();check("World/logic-eye-player-inventory-settings-preserved",logic.position==player.position+glm::vec3(0,.6f,0)&&after.position==playerBefore.position&&after.rotation==playerBefore.rotation&&after.heldItem==playerBefore.heldItem&&after.inventory==playerBefore.inventory&&config.worldSeed==20260807&&config.renderDistance==1);
        check("World/final-loaded-chunk-identities-preserved",chunks(world)==initial);
    }catch(const std::exception& e){++failures;std::cerr<<"[FERN_WORLD] EXCEPTION "<<e.what()<<'\n';}
    std::cout<<"[FERN_WORLD] checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
