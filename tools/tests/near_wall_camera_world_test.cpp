// Resident World + Ogre Camera + production GL depth math.
// The selected GL3PlusRenderSystem is never initialised: no context/window/draw.
// SAT below is independent of camera clipping.
#include "OgreThirdPersonCameraRig.h"
#include "Core/Camera.h"
#include "Player/Player.h"
#include "World/World.h"
#include "World/Block/BlockDatabase.h"
#include "World/Block/TerrainMaterialProfile.h"
#include "Util/ResourcePackResolver.h"
#include "Ogre/StartupResourcePreflight.h"
#include "Ogre/OgrePlayerRenderer.h"
#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreCamera.h>
#include <OgreDefaultHardwareBufferManager.h>
#include <OgreGL3PlusRenderSystem.h>
#include <OgreManualObject.h>
#include <OgreSceneNode.h>
#include <OgreMaterialManager.h>
#include <OgreResourceGroupManager.h>
#include <OgreVertexIndexData.h>
#include <FreeImage.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {
namespace T = ThirdPersonCameraPresentation;
namespace R = OgreThirdPersonCameraRig;
struct RenderSystemDetach {
    Ogre::Root& root;
    ~RenderSystemDetach(){root.setRenderSystem(nullptr);}
};
struct SceneOwner {
    Ogre::Root& root;
    Ogre::SceneManager* scene;
    ~SceneOwner(){root.destroySceneManager(scene);}
};
int checks=0, failures=0, fallbackRisks=0;
void check(const std::string& name,bool ok,const std::string& detail={}) {
    ++checks; if(!ok)++failures;
    std::cout<<"[CAMERA_WORLD] "<<(ok?"PASS ":"FAIL ")<<name<<" "<<detail<<'\n';
}
struct V { double x,y,z; };
V operator+(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V operator-(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
V operator*(V a,double n){return {a.x*n,a.y*n,a.z*n};}
double dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double length(V a){return std::sqrt(dot(a,a));}
V v(const Ogre::Vector3& a){return {a.x,a.y,a.z};}
V v(const glm::vec3& a){return {a.x,a.y,a.z};}
bool close(V a,V b,double e=1e-4){return length(a-b)<=e;}
bool finite(V a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
struct Box { V lo,hi; };
bool inside(V p,Box b,double e=1e-6){return p.x>b.lo.x+e&&p.x<b.hi.x-e&&p.y>b.lo.y+e&&p.y<b.hi.y-e&&p.z>b.lo.z+e&&p.z<b.hi.z-e;}
bool boxesOverlap(Box a,Box b){return a.hi.x>b.lo.x+1e-6&&a.lo.x<b.hi.x-1e-6&&a.hi.y>b.lo.y+1e-6&&a.lo.y<b.hi.y-1e-6&&a.hi.z>b.lo.z+1e-6&&a.lo.z<b.hi.z-1e-6;}

// Complete triangle/AABB SAT:3 box axes,triangle normal,9 edge-cross axes.
// Normalize each nondegenerate axis, tolerance1e-6 world units. Touching alone
// is excluded; no camera-radius/path/corner algorithm is used as expected.
bool triangleBox(const std::array<V,3>& p,Box box) {
    const V centre=(box.lo+box.hi)*.5, half=(box.hi-box.lo)*.5;
    const std::array<V,3> a{{p[0]-centre,p[1]-centre,p[2]-centre}};
    const std::array<V,3> e{{a[1]-a[0],a[2]-a[1],a[0]-a[2]}};
    const std::array<V,3> basis{{{1,0,0},{0,1,0},{0,0,1}}};
    std::vector<V> axes(basis.begin(),basis.end());
    axes.push_back(cross(e[0],e[1]));
    for(V edge:e)for(V unit:basis)axes.push_back(cross(edge,unit));
    for(V axis:axes) {
        const double n=length(axis);if(n<1e-12)continue;axis=axis*(1/n);
        const double radius=half.x*std::abs(axis.x)+half.y*std::abs(axis.y)+half.z*std::abs(axis.z);
        const std::array<double,3> d{{dot(a[0],axis),dot(a[1],axis),dot(a[2],axis)}};
        if(*std::max_element(d.begin(),d.end())<=-radius+1e-6||*std::min_element(d.begin(),d.end())>=radius-1e-6)return false;
    }
    return true;
}
bool quadBox(const std::array<V,4>& q,Box b){return triangleBox({q[0],q[1],q[2]},b)||triangleBox({q[0],q[2],q[3]},b);}
void calibrate() {
    const std::array<V,4> q{{{0,0,0},{2,0,0},{2,2,0},{0,2,0}}};
    const Box crossed{{.5,.5,-.2},{1.5,1.5,.2}};
    check("SAT/all-four-corners-outside",std::none_of(q.begin(),q.end(),[&](V p){return inside(p,crossed);}));
    check("SAT/interior-two-triangle-overlap",quadBox(q,crossed));
    check("SAT/edge-cross-axis-rejects-AABB-only-false-positive",!triangleBox({V{0,0,0},V{2,0,0},V{0,2,0}},Box{{1.4,1.4,-.2},{1.9,1.9,.2}}));
    check("SAT/separated-control",!quadBox(q,Box{{3,.5,-.2},{4,1.5,.2}}));
    check("SAT/contact-only-control",!quadBox(q,Box{{.5,.5,0},{1.5,1.5,1}}));
}
using ChunkState=std::tuple<std::uintptr_t,std::uint64_t,int,bool,std::vector<std::uint32_t>>;
using Residency=std::map<std::pair<int,int>,ChunkState>;
Residency resident(World& w) {
    Residency result;
    for(auto& entry:w.getChunkManager().getChunks()) {
        Chunk& c=entry.second;const auto& p=c.getLocation();std::vector<std::uint32_t> revisions;
        for(std::size_t i=0;i<c.getSectionCount();++i) {
            const auto* section=c.findSection(static_cast<int>(i));
            if(!section)throw std::runtime_error("Existing section missing");
            revisions.push_back(section->getBlockRevision());
        }
        result[{p.x,p.y}]={reinterpret_cast<std::uintptr_t>(&c),c.getIncarnation(),static_cast<int>(c.getDataResidencyState()),c.hasLoaded(),revisions};
    }
    return result;
}
using Cell=std::tuple<int,int,int>;
Box voxel(int x,int y,int z){return {{double(x),double(y),double(z)},{double(x+1),double(y+1),double(z+1)}};}
std::vector<Cell> solids(World& w,int minX,int maxX,int minY,int maxY,int minZ,int maxZ,bool& known) {
    std::vector<Cell> result;known=true;
    for(int x=minX;x<=maxX;++x)for(int z=minZ;z<=maxZ;++z) {
        const auto key=World::getChunkXZ(x,z);
        known &= w.getChunkManager().chunkLoadedAt(key.x,key.z);
        for(int y=minY;y<=maxY;++y)if(w.getBlock(x,y,z).getData().isCollidable)result.emplace_back(x,y,z);
    }
    return result;
}
struct Fixtures {
    World& world;std::vector<Cell> changed;
    void clear() {for(const auto& c:changed)world.setBlock(std::get<0>(c),std::get<1>(c),std::get<2>(c),BlockId::Air);changed.clear();}
    void block(int x,int y,int z){world.setBlock(x,y,z,BlockId::Stone);changed.emplace_back(x,y,z);}
    void rear(int z){for(int x=-3;x<=4;++x)for(int y=199;y<=204;++y)block(x,y,z);}
    void side(int x){for(int z=-1;z<=4;++z)for(int y=199;y<=204;++y)block(x,y,z);}
    void ceiling(){for(int x=-3;x<=4;++x)for(int z=1;z<=6;++z)block(x,202,z);}
};
void writeV(std::ostream& out,V p){out<<'['<<p.x<<','<<p.y<<','<<p.z<<']';}
struct RendererProbe {
    Ogre::SceneManager& scene;
    OgrePlayerRenderer& renderer;
    const PlayerAvatarPresentation::Profile profile=PlayerAvatarPresentation::defaultProfile();
    std::map<std::string,std::pair<std::uintptr_t,std::uintptr_t>> identities;
    void syncAndRead(const Player& player,Material::ID held,T::Mode effective,const std::string& prefix,std::ostream& out) {
        PlayerAvatarPresentation::Snapshot source;
        source.position={player.position.x,player.position.y-player.box.dimensions.y,player.position.z};
        source.rotationDegrees={player.rotation.x,player.rotation.y,player.rotation.z};
        source.velocity={player.velocity.x,player.velocity.y,player.velocity.z};
        source.grounded=player.isOnGround();
        const auto pose=PlayerAvatarPresentation::derivePose(source,profile,PlayerAvatarPresentation::MotionStrength::Off);
        R::syncAvatarForCamera(renderer,profile,pose,effective,false,held);
        const bool expected=effective==T::Mode::ThirdPersonRear;
        auto iterator=scene.getMovableObjectIterator("ManualObject");
        std::size_t parts=0,heldObjects=0;bool visibility=true,software=true,geometry=true,allFinite=true;bool first=true;
        std::map<std::string,std::pair<std::uintptr_t,std::uintptr_t>> current;
        out<<",\"actual_player_objects\":[";
        while(iterator.hasMoreElements()) {
            auto* object=dynamic_cast<Ogre::ManualObject*>(iterator.getNext());
            if(!object)throw std::runtime_error("Nonmanual object in manual collection");
            const std::string name=object->getName();const bool isHeld=name.find("_HeldItemMesh")!=std::string::npos;
            if(isHeld)++heldObjects;else if(name.find("_PartMesh_")!=std::string::npos)++parts;else throw std::runtime_error("Unexpected renderer object");
            visibility &= object->isVisible()==expected;
            if(object->getNumSections()!=1)throw std::runtime_error("Real object missing one production section");
            const auto* operation=object->getSection(0)->getRenderOperation();const auto& data=*operation->vertexData;
            const auto* position=data.vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);
            if(!position||position->getType()!=Ogre::VET_FLOAT3)throw std::runtime_error("Actual position declaration missing");
            const auto buffer=data.vertexBufferBinding->getBuffer(position->getSource());
            software &= dynamic_cast<Ogre::DefaultHardwareVertexBuffer*>(buffer.get())!=nullptr;
            geometry &= operation->indexData&&operation->indexData->indexCount>0&&data.vertexCount==(isHeld?24u:8u);
            current[name]={reinterpret_cast<std::uintptr_t>(object),reinterpret_cast<std::uintptr_t>(buffer.get())};
            const auto& transform=object->getParentSceneNode()->_getFullTransform();
            if(!first)out<<',';first=false;
            out<<"{\"name\":"<<std::quoted(name)<<",\"visible\":"<<(object->isVisible()?"true":"false")<<",\"vertex_count\":"<<data.vertexCount<<",\"world_vertices\":[";
            for(std::size_t vertex=0;vertex<data.vertexCount;++vertex) {
                std::array<float,3> p{};buffer->readData((data.vertexStart+vertex)*buffer->getVertexSize()+position->getOffset(),sizeof(p),p.data());
                const V world=v(transform.transformAffine(Ogre::Vector3(p[0],p[1],p[2])));allFinite &= finite(world);
                if(vertex)out<<',';writeV(out,world);
            }
            out<<"]}";
        }
        out<<']';
        check(prefix+"/actual-eight-body-and-one-held-object",parts==8&&heldObjects==1);
        check(prefix+"/actual-body-held-effective-visibility",visibility);
        check(prefix+"/actual-production-software-buffer-geometry",software&&geometry&&allFinite);
        check(prefix+"/actual-object-buffer-reused-through-camera-mode",identities.empty()||identities==current);identities=current;
    }
};
RendererProbe* actualRenderer=nullptr;
std::string settingsSnapshot(const Config& c) {
    // Adapter has no mutable Config reference. Retain actual relevant setting
    // values independently from returned effective mode, plus ordinary RD.
    std::ostringstream s;s<<int(c.cameraPerspective)<<' '<<c.fov<<' '<<c.renderDistance<<' '<<c.windowX<<' '<<c.windowY<<' '<<int(c.visualDetail);return s.str();
}
bool playerUnchanged(const Player& p,const PlayerSaveState& s,std::uint64_t revision,V velocity,V boxPosition) {
    const auto now=p.getSaveState();return close(v(now.position),v(s.position),0)&&close(v(now.rotation),v(s.rotation),0)&&now.heldItem==s.heldItem&&now.health==s.health&&now.foodCooldownTicks==s.foodCooldownTicks&&now.attackCooldownTicks==s.attackCooldownTicks&&now.inventory==s.inventory&&p.getInventoryRevision()==revision&&close(v(p.velocity),velocity,0)&&close(v(p.box.position),boxPosition,0);
}
T::Pose sample(World& world,Player& player,::Camera& logic,Ogre::Camera& camera,float nominalNear,Config& config,T::State& state,float dt,const std::string& name,int frame,std::ostream& out,const glm::vec3* hit=nullptr,World* adapterWorld=nullptr,bool nullWorld=false) {
    const auto before=resident(world);const auto playerBefore=player.getSaveState();const auto invRev=player.getInventoryRevision();
    const V eye=v(logic.position),rotation=v(logic.rotation),velocity=v(player.velocity),boxPosition=v(player.box.position);const auto settings=settingsSnapshot(config);
    const float fovBefore=camera.getFOVy().valueDegrees(),aspectBefore=camera.getAspectRatio(),farBefore=camera.getFarClipDistance();
    const auto projectionBefore=camera.getProjectionType();
    const auto pose=R::updateCameraPose(logic,camera,nominalNear,nullWorld?nullptr:(adapterWorld?adapterWorld:&world),&player,config.cameraPerspective,state,dt,hit);
    R::applyCameraPose(camera,pose.position,pose.rotation);
    const Ogre::Vector3* actual=camera.getWorldSpaceCorners();std::array<V,4> near{};for(int i=0;i<4;++i)near[i]=v(actual[i]);
    const V position=v(camera.getDerivedPosition()),forward=v(camera.getDerivedDirection()),right=v(camera.getDerivedRight()),up=v(camera.getDerivedUp());
    const auto& view=camera.getViewMatrix();V centre{0,0,0};for(V c:near)centre=centre+c*.25;
    const std::string prefix=name+"/"+std::to_string(frame);
    const float actualNear=camera.getNearClipDistance();
    check(prefix+"/actual-near-positive-bounded-by-independent-nominal",std::isfinite(actualNear)&&actualNear>0&&actualNear<=nominalNear+1e-6f);
    check(prefix+"/actual-near-and-nominal-facts-match-source",std::abs(pose.nominalNearClipDistance-nominalNear)<1e-6f&&std::abs(pose.renderNearClipDistance-actualNear)<1e-6f);
    check(prefix+"/actual-fov-aspect-far-and-projection-kind-unchanged",fovBefore==camera.getFOVy().valueDegrees()&&aspectBefore==camera.getAspectRatio()&&farBefore==camera.getFarClipDistance()&&projectionBefore==camera.getProjectionType());
    if(!nullWorld)check(prefix+"/actual-resident-render-near-safety-resolved",!pose.nearClipSafetyUnresolved);
    else check(prefix+"/absent-adapter-world-remains-explicitly-unresolved",pose.nearClipSafetyUnresolved&&pose.nearClipStatus==R::NearClipStatus::NoWorld);
    check(prefix+"/actual-authoritative-player-eye",close(eye,v(player.position+glm::vec3(0,.6f,0)),1e-5));
    check(prefix+"/actual-view-finite",finite(position)&&finite(forward)&&finite(right)&&finite(up)&&std::all_of(near.begin(),near.end(),finite));
    check(prefix+"/actual-axes-orthonormal",std::abs(length(forward)-1)<1e-5&&std::abs(length(right)-1)<1e-5&&std::abs(length(up)-1)<1e-5&&std::abs(dot(forward,right))<1e-5&&std::abs(dot(forward,up))<1e-5&&std::abs(dot(right,up))<1e-5);
    check(prefix+"/actual-near-centre",close(centre,position+forward*camera.getNearClipDistance()));
    check(prefix+"/actual-view-eye-origin",length(v(view.transformAffine(camera.getDerivedPosition())))<1e-4);
    const double pi=std::acos(-1.0),yaw=pose.rotation.y*pi/180,pitch=pose.rotation.x*pi/180;
    check(prefix+"/ordinary-yaw-pitch-handedness",close(forward,{std::sin(yaw)*std::cos(pitch),-std::sin(pitch),-std::cos(yaw)*std::cos(pitch)},1e-5));
    bool known=false;const auto cells=solids(world,-7,7,198,206,-7,7,known);std::vector<Cell> nearHits;
    bool cameraClear=true,playerClear=true;const Box body{v(player.box.position)-v(player.box.dimensions),v(player.box.position)+v(player.box.dimensions)};
    const Box truthEnvelope{{-7,198,-7},{8,207,8}};
    check(prefix+"/actual-all-geometry-inside-resident-truth-envelope",inside(position,truthEnvelope)&&inside(body.lo,truthEnvelope)&&inside(body.hi,truthEnvelope)&&std::all_of(near.begin(),near.end(),[&](V p){return inside(p,truthEnvelope);}));
    for(const auto& c:cells){const Box b=voxel(std::get<0>(c),std::get<1>(c),std::get<2>(c));if(quadBox(near,b))nearHits.push_back(c);cameraClear &= !inside(position,b);playerClear &= !boxesOverlap(body,b);}
    check(prefix+"/actual-fixture-and-oracle-grid-resident",known);
    check(prefix+"/actual-player-AABB-not-inside-fixture",playerClear);
    check(prefix+"/actual-camera-point-outside-voxel",cameraClear);
    if(pose.effectiveMode==T::Mode::ThirdPersonRear) {
        check(prefix+"/actual-near-two-triangles-clear",nearHits.empty());
        check(prefix+"/actual-near-two-triangles-outside-player",!quadBox(near,body));
        check(prefix+"/third-restores-independent-nominal-near",std::abs(actualNear-nominalNear)<1e-6f);
    } else {
        // Root's r5 actual World/Ogre baseline proved one nominal wide-plane
        // intersection. The repair gate rejects every actual first-person hit.
        check(prefix+"/actual-first-near-two-triangles-clear",nearHits.empty());
        if(!nearHits.empty()) {
            ++fallbackRisks;
            std::cout<<"[CAMERA_RISK] "<<prefix<<" effective_first near_plane_voxels="<<nearHits.size()<<'\n';
        }
    }
    check(prefix+"/actual-production-query-budget",pose.collisionQueries<=2048&&!pose.collisionBudgetExhausted);
    check(prefix+"/actual-near-query-subset-bounded",pose.nearClipQueries<=128&&pose.nearClipQueries<=pose.collisionQueries);
    check(prefix+"/no-new-chunks-incarnations-or-section-revisions",before==resident(world));
    check(prefix+"/logic-eye-rotation-unchanged",close(eye,v(logic.position),0)&&close(rotation,v(logic.rotation),0));
    check(prefix+"/player-source-inventory-unchanged",playerUnchanged(player,playerBefore,invRev,velocity,boxPosition));
    check(prefix+"/requested-settings-unchanged",settings==settingsSnapshot(config));
    const auto visibility=R::visibilityForCamera(pose.effectiveMode,false);
    check(prefix+"/effective-mode-handoff",visibility.avatarVisible==(pose.effectiveMode==T::Mode::ThirdPersonRear)&&visibility.firstPersonHandVisible!=visibility.avatarVisible);
    out<<std::setprecision(10)<<"{\"case\":"<<std::quoted(name)<<",\"frame\":"<<frame<<",\"dt\":"<<dt<<",\"mode\":"<<std::quoted(pose.effectiveMode==T::Mode::FirstPerson?"first":"third")<<",\"distance\":"<<pose.distance<<",\"queries\":"<<pose.collisionQueries<<",\"near_queries\":"<<pose.nearClipQueries<<",\"nominal_near_source\":"<<nominalNear<<",\"actual_render_near\":"<<actualNear<<",\"near_status\":"<<static_cast<int>(pose.nearClipStatus)<<",\"near_safety_unresolved\":"<<(pose.nearClipSafetyUnresolved?"true":"false")<<",\"actual_fov_y\":"<<camera.getFOVy().valueDegrees()<<",\"actual_aspect\":"<<camera.getAspectRatio()<<",\"obstruction\":"<<(pose.obstructionHit?"true":"false")<<",\"resident_chunks\":"<<before.size()<<",\"position\":";writeV(out,position);
    out<<",\"adapter_world_present\":"<<(nullWorld?"false":"true")<<",\"oracle_reference_world_present\":true";
    out<<",\"logic_eye\":";writeV(out,eye);out<<",\"player_box_min\":";writeV(out,body.lo);out<<",\"player_box_max\":";writeV(out,body.hi);out<<",\"near\":[";for(int i=0;i<4;++i){if(i)out<<',';writeV(out,near[i]);}out<<"],\"view\":[";for(int i=0;i<4;++i)for(int j=0;j<4;++j){if(i||j)out<<',';out<<view[i][j];}out<<"],\"forward\":";writeV(out,forward);out<<",\"right\":";writeV(out,right);out<<",\"up\":";writeV(out,up);out<<",\"terrain_near_hits\":[";
    for(std::size_t i=0;i<nearHits.size();++i){if(i)out<<',';out<<'['<<std::get<0>(nearHits[i])<<','<<std::get<1>(nearHits[i])<<','<<std::get<2>(nearHits[i])<<']';}out<<']';
    if(!actualRenderer)throw std::runtime_error("Actual renderer probe missing");
    actualRenderer->syncAndRead(player,player.getInventorySlot(playerBefore.heldItem).getMaterial().id,pose.effectiveMode,prefix,out);
    check(prefix+"/actual-renderer-preserves-player-inventory",playerUnchanged(player,playerBefore,invRev,velocity,boxPosition));
    out<<"}\n";
    return pose;
}
void setAuthority(Player& player,::Camera& logic,glm::vec3 eye,glm::vec3 rot) {
    player.position=eye-glm::vec3(0,.6f,0);player.rotation=rot;player.velocity=glm::vec3(0);player.box.update(player.position);logic.hookEntity(player);logic.update();
}
}

int main(int argc,char** argv) {
    if(argc!=3){std::cerr<<"Usage: camera-world-test repository-root new-output-directory\n";return 2;}
    try {
        const std::filesystem::path output=argv[2];if(std::filesystem::exists(output))throw std::runtime_error("Output must be fresh");std::filesystem::create_directories(output);
        setenv("HELLOMINE3D_ROOT",argv[1],1);setenv("HELLOMINE3D_SEED","20260807",1);setenv("HELLOMINE3D_PLAYER_POSITION","0.5 200 0.5",1);setenv("HELLOMINE3D_PLAYER_ROTATION","0 0 0",1);
        FreeImage_Initialise(FALSE);
        const auto resources=loadStartupResourceManifest(argv[1]);std::vector<ResourcePackRequirement> requirements;for(const auto& entry:resources)requirements.push_back({entry.category,entry.relativePath});
        runtimeResourcePackResolver().freeze(argv[1],requirements,{});validateStartupResources(argv[1],resources);runtimeTerrainMaterialProfile().freezeFromResourceView(runtimeResourcePackResolver());runtimeTerrainMaterialProfile().freezeRenderingMode(true,true);BlockDatabase::get();
        calibrate();
        Ogre::Root root("","",(output/"ogre.log").string());
        // Frustum's constructor converts its projection through Root's RS.
        // Select the real GL implementation, but never initialise it. Native
        // construction enumerates display/config data only; buffers stay CPU.
        Ogre::GL3PlusRenderSystem projectionSystem;
        RenderSystemDetach detach{root};
        root.setRenderSystem(&projectionSystem);
        Ogre::DefaultHardwareBufferManager buffers;
        check("SETUP/actual-production-projection-RenderSystem",root.getRenderSystem()==&projectionSystem);
        check("SETUP/uninitialised-no-main-context-window-or-target",!root.isInitialised()&&projectionSystem._getMainContext()==nullptr&&root.getAutoCreatedWindow()==nullptr&&!projectionSystem.getRenderTargetIterator().hasMoreElements());
        for(const char* name:{OgrePlayerRenderer::MaterialName,OgrePlayerRenderer::HeldMaterialName,OgrePlayerRenderer::HeldTransparentMaterialName})Ogre::MaterialManager::getSingleton().create(name,Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        auto* scene=root.createSceneManager(Ogre::ST_GENERIC,"ActualCameraWorldOracle");SceneOwner sceneOwner{root,scene};auto* camera=scene->createCamera("OrdinaryCamera");float nominalNear=.1f;camera->setNearClipDistance(nominalNear);camera->setFarClipDistance(128);camera->setFOVy(Ogre::Degree(90));camera->setAspectRatio(16.f/9);
        Config config;config.renderDistance=1;config.worldSeed=20260807;config.cameraPerspective=CameraPerspective::ThirdPerson;::Camera logic(config);Player player;
        {
            World world(logic,config,player,(output/"isolated-save").string(),false,1);Fixtures fixture{world,{}};
            auto inventory=player.getSaveState();inventory.heldItem=0;inventory.inventory.assign(5,{});inventory.inventory[0]={Material::Stone,7,0};player.applySaveState(inventory);
            OgrePlayerRenderer renderer(*scene);RendererProbe rendererProbe{*scene,renderer,PlayerAvatarPresentation::defaultProfile(),{}};actualRenderer=&rendererProbe;
            setAuthority(player,logic,{.5f,200.6f,.5f},{0,0,0});
            // Establish an Air diagnostic envelope only through actual setBlock.
            // Existing surrounding terrain is retained beyond the bounded grid.
            bool known=false;const auto original=solids(world,-7,7,198,206,-7,7,known);check("PREP/actual-resident-truth-grid",known);
            bool nine=true;for(int x=-1;x<=1;++x)for(int z=-1;z<=1;++z)nine &= world.getChunkManager().chunkLoadedAt(x,z);check("PREP/actual-resident-nine-chunk-neighborhood",nine);
            for(const auto& c:original)world.setBlock(std::get<0>(c),std::get<1>(c),std::get<2>(c),BlockId::Air);
            std::ofstream out(output/"actual-camera-world.jsonl");if(!out)throw std::runtime_error("Cannot create actual output");
            for(int rate:{30,120})for(int scenario=0;scenario<5;++scenario) {
                fixture.clear();setAuthority(player,logic,{.5f,200.6f,.5f},{scenario==2?45.f:scenario==3?0.f:0.f,scenario==3?45.f:0.f,0});
                if(scenario==1)fixture.rear(3);if(scenario==2)fixture.ceiling();if(scenario==3){for(int y=199;y<=204;++y){fixture.block(-2,y,2);fixture.block(-1,y,2);}fixture.side(1);}if(scenario==4)fixture.rear(1);
                T::State state;const std::string name=std::string("resident/")+std::array<const char*,5>{{"clear","rear_wall","low_ceiling","shoulder_L_corner","near_wall_fallback"}}[scenario]+"/"+std::to_string(rate);
                T::Pose p;for(int frame=0;frame<=rate/5;++frame)p=sample(world,player,logic,*camera,nominalNear,config,state,1.f/rate,name,frame,out);
                if(scenario==0)check(name+"/clear-third-control",p.effectiveMode==T::Mode::ThirdPersonRear&&!p.obstructionHit);
                else check(name+"/actual-resident-obstruction-detected",p.obstructionHit);
                if(scenario==4)check(name+"/near-wall-effective-first",p.effectiveMode==T::Mode::FirstPerson);
            }
            fixture.clear();fixture.side(2);setAuthority(player,logic,{1.05f,200.6f,.5f},{0,0,0});camera->setFOVy(Ogre::Degree(120));camera->setAspectRatio(3);T::State wideThird;
            const auto wideControl=sample(world,player,logic,*camera,nominalNear,config,wideThird,1.f/60,"supported_wide_third_side_wall",0,out);
            check("supported_wide_third/retains-third-and-real-obstruction",wideControl.effectiveMode==T::Mode::ThirdPersonRear&&wideControl.obstructionHit);
            fixture.clear();fixture.rear(1);fixture.side(1);setAuthority(player,logic,{.5f,200.6f,.5f},{0,0,0});T::State wide;
            const auto wideFallback=sample(world,player,logic,*camera,nominalNear,config,wide,1.f/60,"fallback_supported_wide_plane_risk",0,out);check("fallback_supported_wide/effective-first-control",wideFallback.effectiveMode==T::Mode::FirstPerson);
            // Reset only actual rendering near to the independent nominal so
            // a delayed/no-op dt0 correction cannot inherit the prior shrink.
            camera->setNearClipDistance(nominalNear);T::State pausedWide;
            check("fallback_supported_wide/dt0-first-frame-effective-first",sample(world,player,logic,*camera,nominalNear,config,pausedWide,0,"fallback_supported_wide_paused_immediate",0,out).effectiveMode==T::Mode::FirstPerson);
            check("fallback_supported_wide/dt0-first-frame-near-shrinks",camera->getNearClipDistance()<nominalNear-1e-6f);
            fixture.clear();config.cameraPerspective=CameraPerspective::FirstPerson;
            check("open_first/effective-first",sample(world,player,logic,*camera,nominalNear,config,pausedWide,0,"open_first_restores_nominal",0,out).effectiveMode==T::Mode::FirstPerson);
            check("open_first/actual-near-restores-nominal",std::abs(camera->getNearClipDistance()-nominalNear)<1e-6f);config.cameraPerspective=CameraPerspective::ThirdPerson;
            // Unsupported nominal must stay authoritative even if this real
            // side wall first reduces the actual render near below0.1. Clearing
            // the wall then exposes a feedback bug that would enable ThirdPerson.
            fixture.side(1);nominalNear=.2f;camera->setNearClipDistance(nominalNear);T::State unsupported;
            check("projection_unsupported/effective-first",sample(world,player,logic,*camera,nominalNear,config,unsupported,1.f/60,"unsupported_near_existing_first_policy",0,out).effectiveMode==T::Mode::FirstPerson);
            check("projection_unsupported/actual-near-reduced-below-supported-ceiling",camera->getNearClipDistance()<.1f);
            fixture.clear();check("projection_unsupported/reduced-render-near-never-enables-third",sample(world,player,logic,*camera,nominalNear,config,unsupported,0,"unsupported_nominal_clear_after_reduced_near",1,out).effectiveMode==T::Mode::FirstPerson);
            check("projection_unsupported/open-first-restores-original-nominal",std::abs(camera->getNearClipDistance()-nominalNear)<1e-6f);
            nominalNear=.1f;camera->setNearClipDistance(nominalNear);camera->setFOVy(Ogre::Degree(90));camera->setAspectRatio(16.f/9);T::State absent;
            check("null_world/effective-first",sample(world,player,logic,*camera,nominalNear,config,absent,1.f/60,"null_world",0,out,nullptr,nullptr,true).effectiveMode==T::Mode::FirstPerson);
            T::State aiming;glm::vec3 selected(.5f,200.6f,-5.5f);sample(world,player,logic,*camera,nominalNear,config,aiming,0,"selected_real_source_point",0,out,&selected);
            // Existing fallback release must survive one clear frame. No tick,
            // gameplay movement, synthetic query callback or GPU is involved.
            fixture.rear(1);fixture.side(1);camera->setFOVy(Ogre::Degree(120));camera->setAspectRatio(3);T::State release;sample(world,player,logic,*camera,nominalNear,config,release,1.f/60,"release_blocked",0,out);check("release/blocked-source-actually-reduces-near",camera->getNearClipDistance()<nominalNear-1e-6f);fixture.clear();
            check("release/one-clear-frame-does-not-release",sample(world,player,logic,*camera,nominalNear,config,release,1.f/60,"release_clear",1,out).effectiveMode==T::Mode::FirstPerson);
            const auto beforePause=release;sample(world,player,logic,*camera,nominalNear,config,release,0,"release_paused",2,out);check("release/zero-delta-retains-timer",release.fallbackClearSeconds==beforePause.fallbackClearSeconds);
            T::Pose recovered;for(int frame=3;frame<40;++frame)recovered=sample(world,player,logic,*camera,nominalNear,config,release,1.f/60,"release_clear",frame,out);
            check("release/actual-clear-eventual-third",recovered.effectiveMode==T::Mode::ThirdPersonRear);
            actualRenderer=nullptr;
        }
        check("SCOPE/only-production-projection-no-initialise-context-window-target",root.getRenderSystem()==&projectionSystem&&!root.isInitialised()&&projectionSystem._getMainContext()==nullptr&&root.getAutoCreatedWindow()==nullptr&&!projectionSystem.getRenderTargetIterator().hasMoreElements());
        // World/resource singletons may outlive main; keep FreeImage alive for
        // their ordinary static destruction, as the existing standalone tools.
    } catch(const std::exception& e){check("unexpected-exception",false,e.what());}
    std::cout<<"[CAMERA_WORLD] checks="<<checks<<" failures="<<failures<<" first_policy_near_plane_risk_frames="<<fallbackRisks<<"\n";
    return failures?1:0;
}
