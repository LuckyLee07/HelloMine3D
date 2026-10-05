#include "OgreCameraDiagnostics.h"
#include "../Core/Camera.h"
#include "../Player/Player.h"
#include "../World/World.h"
#include <Ogre.h>
#include <OgreAutoParamDataSource.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
namespace fs = std::filesystem;
constexpr const char* Schema = "hellomine3d-actual-client-camera-capture-v1";
constexpr std::size_t TotalLimit = 64u * 1024u * 1024u;
constexpr std::size_t JsonLimit = 2u * 1024u * 1024u;
constexpr std::size_t PngLimit = 16u * 1024u * 1024u;
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("Camera diagnostics: " + message);
}
std::string q(const std::string& value) {
    std::ostringstream out; out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4)
            << std::setfill('0') << int(c) << std::dec;
        else out << c;
    }
    return out.str() + '"';
}
template<class T> std::string n(T value) {
    require(std::isfinite(static_cast<double>(value)), "non-finite source value");
    std::ostringstream out; out << std::setprecision(10) << +value; return out.str();
}
std::string b(bool value) { return value ? "true" : "false"; }
using Fields = std::vector<std::pair<std::string,std::string>>;
std::string object(const Fields& fields) {
    std::string out = "{";
    for (const auto& p : fields) { if (out.size() > 1) out += ','; out += q(p.first)+':'+p.second; }
    return out + '}';
}
std::string array(const std::vector<std::string>& items) {
    std::string out = "[";
    for (const auto& item : items) { if (out.size() > 1) out += ','; out += item; }
    return out + ']';
}
template<class V> std::string v3(const V& value) { return array({n(value.x),n(value.y),n(value.z)}); }
std::string matrix(const Ogre::Matrix4& value) {
    std::vector<std::string> items;
    for (unsigned r=0;r<4;++r) for (unsigned c=0;c<4;++c) items.push_back(n(value[r][c]));
    return array(items);
}
std::string pointer(const void* p) { std::ostringstream out; out << p; return q(out.str()); }
int floorSection(int p) { int s=p/CHUNK_SIZE; if(p<0 && p%CHUNK_SIZE) --s; return s; }
using Key = std::array<int,3>;
Key key(const glm::ivec3& p) { return {{p.x,p.y,p.z}}; }
std::string coord(const Key& p) { return array({n(p[0]),n(p[1]),n(p[2])}); }
std::map<Key,std::uint32_t> versions(const WorldMeshSnapshot& snapshot) {
    std::map<Key,std::uint32_t> out;
    for (const auto& s : snapshot.liveSectionVersions) out.emplace(key(s.location),s.blockRevision);
    return out;
}
std::string operation(Ogre::Renderable& renderable) {
    Ogre::RenderOperation op; renderable.getRenderOperation(op);
    std::vector<std::string> elements, buffers;
    if (op.vertexData) {
        for (const auto& e : op.vertexData->vertexDeclaration->getElements())
            elements.push_back(object({{"source",n(e.getSource())},{"offset",n(e.getOffset())},
                {"semantic",n(e.getSemantic())},{"semantic_index",n(e.getIndex())},
                {"type",n(e.getType())},{"components",n(Ogre::VertexElement::getTypeCount(e.getType()))}}));
        for (const auto& binding : op.vertexData->vertexBufferBinding->getBindings()) {
            const auto& buffer=binding.second;
            buffers.push_back(object({{"source",n(binding.first)},{"stride",n(buffer->getVertexSize())},
                {"vertices",n(buffer->getNumVertices())},{"bytes",n(buffer->getSizeInBytes())},
                {"system_memory",b(buffer->isSystemMemory())},{"has_shadow",b(buffer->hasShadowBuffer())}}));
        }
    }
    Fields index{{"present",b(op.useIndexes && op.indexData && !op.indexData->indexBuffer.isNull())}};
    if (op.useIndexes && op.indexData && !op.indexData->indexBuffer.isNull()) {
        const auto& buffer=op.indexData->indexBuffer;
        index.emplace_back("type",q(buffer->getType()==Ogre::HardwareIndexBuffer::IT_16BIT?"uint16":"uint32"));
        index.emplace_back("start",n(op.indexData->indexStart));
        index.emplace_back("count",n(op.indexData->indexCount));
        index.emplace_back("bytes",n(buffer->getSizeInBytes()));
        index.emplace_back("system_memory",b(buffer->isSystemMemory()));
        index.emplace_back("has_shadow",b(buffer->hasShadowBuffer()));
    }
    // Deliberately no parent hardware/shadow lock: unlock can perform a pending
    // hardware upload. Original GPU geometry needs a separate strict GL guard.
    // Never fill these fields from Profile/ItemVisual or a reconstructed cube.
    return object({{"operation_type",n(op.operationType)},{"use_indices",b(op.useIndexes)},
        {"vertex_start",n(op.vertexData?op.vertexData->vertexStart:0)},
        {"vertex_count",n(op.vertexData?op.vertexData->vertexCount:0)},
        {"elements",array(elements)},{"vertex_buffers",array(buffers)},{"index_buffer",object(index)},
        {"positions",q("OPEN_ORIGINAL_GPU_BYTES_NOT_READ")},
        {"indices",q("OPEN_ORIGINAL_GPU_BYTES_NOT_READ")}});
}
struct PngInfo { std::uintmax_t bytes=0; unsigned width=0,height=0; };
PngInfo pngInfo(const fs::path& path) {
    require(fs::is_regular_file(path) && !fs::is_symlink(path), "actual frame PNG missing or symlink");
    PngInfo result; result.bytes=fs::file_size(path);
    require(result.bytes>=33 && result.bytes<=PngLimit,"PNG byte bound");
    std::ifstream in(path,std::ios::binary); std::array<unsigned char,24> header{};
    in.read(reinterpret_cast<char*>(header.data()),header.size());
    const std::array<unsigned char,8> signature{{137,80,78,71,13,10,26,10}};
    require(in.good() && std::equal(signature.begin(),signature.end(),header.begin()) &&
        std::memcmp(header.data()+12,"IHDR",4)==0,"actual file is not PNG/IHDR");
    auto be=[&](unsigned off) { unsigned value=0; for(unsigned i=0;i<4;++i)value=(value<<8)|header[off+i];return value; };
    result.width=be(16);result.height=be(20);
    require(result.width && result.height && result.width<=4096 && result.height<=4096 &&
        std::uint64_t(result.width)*result.height<=16777216,"PNG dimension bound");
    return result;
}
}

struct OgreCameraDiagnostics::Impl {
    struct Voxel {
        Key position{}, section{}; std::uint32_t before=0,after=0;
        bool beforeKnown=false,afterKnown=false,collidable=false;
        unsigned id=0,metadata=0;
    };
    struct Section { Ogre::Renderable* source=nullptr; unsigned index=0; std::string descriptor; std::vector<std::string> draws; };
    struct PlayerObject { std::string name,role,parent; int part=-1; bool visible=false,effectiveVisible=false;
        unsigned visibilityFlags=0,queue=0; std::string transform; std::vector<Section> sections; };
    fs::path output; Ogre::SceneManager& scene; std::thread::id thread=std::this_thread::get_id();
    std::set<std::string> phases; std::vector<std::string> frameFiles;
    std::uintmax_t storedBytes=0; bool open=false,truncated=false,queueTruncated=false;
    std::uint64_t frameId=0; std::string phase,stem,cameraJson,playerJson;
    std::string failure; const Ogre::Camera* camera=nullptr; World* world=nullptr;
    std::vector<Voxel> voxels; std::vector<PlayerObject> objects;
    std::map<Ogre::Renderable*,std::pair<std::size_t,std::size_t>> selected;
    Key scanMinimum{},scanMaximum{}; std::uint64_t requestedVoxels=0;
    std::size_t queries=0,mainNotifications=0,selectedNotifications=0,otherCameraNotifications=0;
    explicit Impl(const std::string& directory,Ogre::SceneManager& manager):output(fs::absolute(directory)),scene(manager) {
        require(!fs::exists(output),"output directory must be fresh");
        require(fs::create_directories(output),"cannot create output directory");
    }
    void checkThread() const { require(thread==std::this_thread::get_id(),"render thread ownership"); }
    void write(const fs::path& file,const std::string& text) {
        require(text.size()<=JsonLimit,"JSON byte bound");
        const fs::path temporary=file.string()+".tmp";
        require(!fs::exists(temporary),"temporary evidence file already exists");
        std::ofstream out(temporary,std::ios::binary); require(out.good(),"cannot create evidence JSON");
        out.write(text.data(),static_cast<std::streamsize>(text.size()));out.close();
        require(out.good(),"cannot finish evidence JSON");fs::rename(temporary,file);
    }
    void writeIndex(const std::string& status) {
        std::vector<std::string> files;for(const auto& f:frameFiles)files.push_back(q(f));
        const std::string text=object({{"schema",q(Schema)},{"status",q(status)},
            {"frames",array(files)},{"stored_frame_bytes",n(storedBytes)},
            {"failure",q(failure)},{"bounds",object({{"phases",n(12)},{"frames",n(24)},
                {"voxels_per_frame",n(512)},{"player_objects",n(9)},
                {"selected_queue_notifications",n(64)},{"stored_bytes",n(TotalLimit)}})},
            {"original_geometry",q("OPEN_ORIGINAL_GPU_BYTES_NOT_READ")},
            {"hand_raw_and_pixel_attribution",q("OPEN_NOT_CAPTURED")},
            {"GL_errors_and_state",q("OPEN_MODULE_MAKES_NO_GL_CALLS")}})+"\n";
        require(storedBytes+text.size()<=TotalLimit,"session stored byte bound");write(output/"index.json",text);
    }
};

OgreCameraDiagnostics::OgreCameraDiagnostics(const std::string& directory,Ogre::SceneManager& scene)
    :m_impl(std::make_unique<Impl>(directory,scene)) {
    m_impl->writeIndex("INITIALIZED_NOT_CAPTURED");scene.addRenderObjectListener(this);
}
OgreCameraDiagnostics::~OgreCameraDiagnostics() {
    m_impl->scene.removeRenderObjectListener(this);
    if(m_impl->open)try {m_impl->failure="frame was not finished";m_impl->writeIndex("INCOMPLETE");}catch(...){}
}
bool OgreCameraDiagnostics::isFrameOpen() const noexcept {return m_impl->open;}
std::size_t OgreCameraDiagnostics::frameCount() const noexcept {return m_impl->frameFiles.size();}
std::string OgreCameraDiagnostics::framePngPath() const {
    m_impl->checkThread();require(m_impl->open,"no current frame PNG");
    return (m_impl->output/(m_impl->stem+".png")).string();
}
void OgreCameraDiagnostics::beginFrame(const std::string& phase,World& world,
    const Player& player,const ::Camera& logic,const Ogre::Camera& camera,
    ThirdPersonCameraPresentation::Mode effective,CameraPerspective requested,std::uint64_t frameId,
    float nominalNearClipDistance,std::size_t nearClipQueries,
    bool nearClipSafetyUnresolved,int nearClipStatus) {
    auto& p=*m_impl;p.checkThread();require(!p.open,"previous frame is still open");
    require(p.frameFiles.size()<24,"frame limit");require(!phase.empty() && phase.size()<=64,"phase name bound");
    require(p.storedBytes+PngLimit+2*JsonLimit<=TotalLimit,"cannot reserve bounded next PNG/JSON/index");
    require(p.phases.count(phase) || p.phases.size()<12,"phase limit");
    p.phase=phase;p.frameId=frameId;p.camera=&camera;p.world=&world;p.truncated=false;p.queueTruncated=false;
    p.queries=p.mainNotifications=p.selectedNotifications=p.otherCameraNotifications=0;
    p.objects.clear();p.selected.clear();p.voxels.clear();p.phases.insert(phase);
    std::ostringstream stem;stem<<"frame-"<<std::setw(3)<<std::setfill('0')<<p.frameFiles.size();p.stem=stem.str();
    const Ogre::Vector3* corners=camera.getWorldSpaceCorners();std::vector<std::string> near;
    Ogre::Vector3 minimum=Ogre::Vector3::ZERO,maximum=Ogre::Vector3::ZERO;
    for(unsigned i=0;i<4;++i) {near.push_back(v3(corners[i]));if(!i)minimum=maximum=corners[i];else {minimum.makeFloor(corners[i]);maximum.makeCeil(corners[i]);}}
    const auto orientation=camera.getDerivedOrientation();
    p.cameraJson=object({{"name",q(camera.getName())},{"identity",pointer(&camera)},
        {"requested",q(cameraPerspectiveToken(requested))},
        {"effective",q(effective==ThirdPersonCameraPresentation::Mode::ThirdPersonRear?"third":"first")},
        {"position",v3(camera.getDerivedPosition())},{"orientation_wxyz",array({n(orientation.w),n(orientation.x),n(orientation.y),n(orientation.z)})},
        {"logic_position",v3(logic.position)},{"logic_rotation",v3(logic.rotation)},
        {"fov_y_degrees",n(camera.getFOVy().valueDegrees())},{"aspect",n(camera.getAspectRatio())},
        {"near",n(camera.getNearClipDistance())},{"far",n(camera.getFarClipDistance())},
        {"nominal_near",n(nominalNearClipDistance)},
        {"near_clip_queries",n(nearClipQueries)},
        {"near_clip_safety_unresolved",b(nearClipSafetyUnresolved)},
        {"near_clip_status",n(nearClipStatus)},
        {"projection_type",n(camera.getProjectionType())},{"near_world_corners",array(near)},
        {"corner_order",q("top_right,top_left,bottom_left,bottom_right")},
        {"view_row_major",matrix(camera.getViewMatrix())},
        {"projection_row_major",matrix(camera.getProjectionMatrix())},
        {"projection_RS_depth_row_major",matrix(camera.getProjectionMatrixWithRSDepth())}});
    const auto save=player.getSaveState();
    p.playerJson=object({{"position",v3(player.position)},{"rotation",v3(player.rotation)},
        {"box_position",v3(player.box.position)},{"box_half_extents",v3(player.box.dimensions)},
        {"selected_slot",n(save.heldItem)},{"container_open",b(player.hasOpenContainer())},
        {"crafting_open",b(player.hasOpenCrafting())},{"inventory_revision",n(player.getInventoryRevision())}});
    // Physical collider bounds are retained as observations, never substituted
    // for actual avatar/held geometry. The stored box.position may be stale;
    // the authoritative Player.position is always recorded separately.
    const Ogre::Vector3 playerCentre(player.position.x,player.position.y,player.position.z);
    const Ogre::Vector3 half(player.box.dimensions.x,player.box.dimensions.y,player.box.dimensions.z);
    minimum.makeFloor(playerCentre-half);maximum.makeCeil(playerCentre+half);
    auto it=p.scene.getMovableObjectIterator("ManualObject");std::size_t inspected=0;
    while(it.hasMoreElements()) {
        require(++inspected<=4096,"ManualObject iteration bound");
        auto* manual=dynamic_cast<Ogre::ManualObject*>(it.getNext());if(!manual)continue;
        const std::string name=manual->getName();if(name.rfind("PlayerAvatar_",0)!=0)continue;
        const std::string partSuffix="_PartMesh_";const auto partAt=name.rfind(partSuffix);
        const bool held=name.size()>=13 && name.compare(name.size()-13,13,"_HeldItemMesh")==0;
        int part=-1;
        if(partAt!=std::string::npos && partAt+partSuffix.size()+1==name.size() &&
           name.back()>='0' && name.back()<='7')part=name.back()-'0';
        if(!held && part<0)continue;
        require(p.objects.size()<9,"multiple avatar/player object bound");
        Impl::PlayerObject record;record.name=name;record.part=part;record.role=held?"held":"part";
        record.visible=manual->getVisible();record.effectiveVisible=manual->isVisible();
        record.visibilityFlags=manual->getVisibilityFlags();record.queue=manual->getRenderQueueGroup();
        record.parent=manual->getParentNode()?manual->getParentNode()->getName():"";
        record.transform=matrix(manual->_getParentNodeFullTransform());
        require(manual->getNumSections()<=8,"player sections/object bound");
        for(unsigned s=0;s<manual->getNumSections();++s) {
            auto* section=manual->getSection(s);Impl::Section sec;sec.source=section;sec.index=s;
            sec.descriptor=operation(*section);p.selected.emplace(section,std::make_pair(p.objects.size(),record.sections.size()));
            record.sections.push_back(std::move(sec));
        }
        const auto& bounds=manual->getWorldBoundingBox(true);
        if(!bounds.isNull() && !bounds.isInfinite()) {minimum.makeFloor(bounds.getMinimum());maximum.makeCeil(bounds.getMaximum());}
        p.objects.push_back(std::move(record));
    }
    minimum-=Ogre::Vector3(1,1,1);maximum+=Ogre::Vector3(1,1,1);
    for(unsigned a=0;a<3;++a) {
        require(std::isfinite(minimum[a]) && std::isfinite(maximum[a]) &&
            minimum[a]>std::numeric_limits<int>::min()+1.0 && maximum[a]<std::numeric_limits<int>::max()-1.0,"voxel coordinate range");
        p.scanMinimum[a]=static_cast<int>(std::floor(minimum[a]));p.scanMaximum[a]=static_cast<int>(std::floor(maximum[a]));
    }
    p.requestedVoxels=1;
    for(unsigned a=0;a<3;++a) {const auto count=std::uint64_t(std::int64_t(p.scanMaximum[a])-p.scanMinimum[a]+1);
        require(count && count<=100000,"voxel span bound");require(p.requestedVoxels<=1000000/count,"voxel product bound");p.requestedVoxels*=count;}
    const auto before=versions(world.collectSectionMeshSnapshot(false));
    for(std::int64_t x=p.scanMinimum[0];x<=p.scanMaximum[0] && p.voxels.size()<512;++x)
      for(std::int64_t y=p.scanMinimum[1];y<=p.scanMaximum[1] && p.voxels.size()<512;++y)
        for(std::int64_t z=p.scanMinimum[2];z<=p.scanMaximum[2] && p.voxels.size()<512;++z) {
            Impl::Voxel voxel;voxel.position={{int(x),int(y),int(z)}};
            voxel.section={{floorSection(int(x)),floorSection(int(y)),floorSection(int(z))}};
            const auto found=before.find(voxel.section);voxel.beforeKnown=found!=before.end();
            if(voxel.beforeKnown) {voxel.before=found->second;++p.queries;
                const auto block=world.getBlock(int(x),int(y),int(z));voxel.id=block.id;voxel.metadata=block.metadata;
                voxel.collidable=block.getData().isCollidable;}
            p.voxels.push_back(voxel);
        }
    p.truncated=p.requestedVoxels>p.voxels.size();p.open=true;
    p.writeIndex("FRAME_OPEN");
}
void OgreCameraDiagnostics::notifyRenderSingleObject(Ogre::Renderable* renderable,
    const Ogre::Pass* pass,const Ogre::AutoParamDataSource* source,const Ogre::LightList*,bool suppressed) {
    auto& p=*m_impl;if(!p.open)return;p.checkThread();
    if(!source || source->getCurrentCamera()!=p.camera) {++p.otherCameraNotifications;return;}
    ++p.mainNotifications;const auto found=p.selected.find(renderable);if(found==p.selected.end())return;
    if(p.selectedNotifications>=64) {p.queueTruncated=true;return;}
    require(pass,"selected main-camera notification has no pass");++p.selectedNotifications;
    auto& sec=p.objects[found->second.first].sections[found->second.second];
    sec.draws.push_back(object({{"renderable_identity",pointer(renderable)},
        {"camera_identity",pointer(source->getCurrentCamera())},{"pass_index",n(pass->getIndex())},
        {"material",q(pass->getParent()->getParent()->getName())},
        {"vertex_program",q(pass->hasVertexProgram()?pass->getVertexProgramName():"")},
        {"fragment_program",q(pass->hasFragmentProgram()?pass->getFragmentProgramName():"")},
        {"suppress_render_state_changes",b(suppressed)},
        {"world_row_major",matrix(source->getWorldMatrix())},{"view_row_major",matrix(source->getViewMatrix())},
        {"projection_row_major",matrix(source->getProjectionMatrix())},{"WVP_row_major",matrix(source->getWorldViewProjMatrix())},
        {"provenance",q("ACTUAL_MAIN_CAMERA_AUTOPARAM_SOURCE_PRE_GPU_BIND")},
        {"actual_GL_uniforms",q("OPEN_LISTENER_PRECEDES_GPU_AUTO_PARAMETER_BIND")}}));
}
void OgreCameraDiagnostics::finishFrame(bool actualFirstPersonHandVisible) {
    auto& p=*m_impl;p.checkThread();require(p.open,"no current frame to finish");
    try {
        const auto after=versions(p.world->collectSectionMeshSnapshot(false));
        std::vector<std::string> copied;std::size_t unknown=0,changed=0,collidable=0;
        for(auto& voxel:p.voxels) {
            const auto found=after.find(voxel.section);voxel.afterKnown=found!=after.end();if(voxel.afterKnown)voxel.after=found->second;
            const bool known=voxel.beforeKnown && voxel.afterKnown && voxel.before==voxel.after;
            if(!known)++unknown;if(voxel.beforeKnown && (!voxel.afterKnown || voxel.before!=voxel.after))++changed;
            if(known && voxel.collidable)++collidable;
            copied.push_back(object({{"position",coord(voxel.position)},{"section",coord(voxel.section)},
                {"known",b(known)},{"before_known",b(voxel.beforeKnown)},{"after_known",b(voxel.afterKnown)},
                {"before_block_revision",voxel.beforeKnown?n(voxel.before):"null"},
                {"after_block_revision",voxel.afterKnown?n(voxel.after):"null"},
                {"block_id",voxel.beforeKnown?n(voxel.id):"null"},{"metadata",voxel.beforeKnown?n(voxel.metadata):"null"},
                {"collidable",known?b(voxel.collidable):"null"},
                {"GPU_residency",q("OPEN_NOT_OBSERVED")}}));
        }
        std::vector<std::string> objects;std::set<int> parts;std::size_t held=0;
        for(const auto& observed:p.objects) {
            if(observed.part>=0)parts.insert(observed.part);else ++held;
            std::vector<std::string> sections;
            for(const auto& sec:observed.sections)sections.push_back(object({{"index",n(sec.index)},
                {"renderable_identity",pointer(sec.source)},{"render_operation",sec.descriptor},
                {"main_camera_queue_membership",b(!sec.draws.empty())},{"main_camera_draws",array(sec.draws)}}));
            objects.push_back(object({{"name",q(observed.name)},{"role",q(observed.role)},
                {"part_index",n(observed.part)},{"parent_node",q(observed.parent)},
                {"visible",b(observed.visible)},{"is_visible",b(observed.effectiveVisible)},
                {"visibility_flags",n(observed.visibilityFlags)},{"queue_group",n(observed.queue)},
                {"parent_world_row_major",observed.transform},{"sections",array(sections)}}));
        }
        const auto png=pngInfo(p.output/(p.stem+".png"));
        const std::string text=object({{"schema",q(Schema)},{"phase",q(p.phase)},{"frame_id",n(p.frameId)},
            {"status",q("CAPTURED_WITH_OPEN_GEOMETRY_AND_HAND_ATTRIBUTION")},
            {"camera",p.cameraJson},{"player",p.playerJson},{"player_objects",array(objects)},
            {"player_part_indices_observed",n(parts.size())},{"held_objects_observed",n(held)},
            {"queue",object({{"main_camera_notifications",n(p.mainNotifications)},
                {"selected_notifications",n(p.selectedNotifications)},{"other_camera_notifications",n(p.otherCameraNotifications)},
                {"selected_notifications_truncated",b(p.queueTruncated)},
                {"phase",q("pre_actual_GPU_parameter_bind")}})},
            {"world",object({{"scan_minimum",coord(p.scanMinimum)},{"scan_maximum",coord(p.scanMaximum)},
                {"requested_voxels",n(p.requestedVoxels)},{"copied_voxels",n(p.voxels.size())},
                {"getBlock_calls",n(p.queries)},{"scan_truncated",b(p.truncated)},
                {"unknown_voxels",n(unknown)},{"changed_sections_at_voxels",n(changed)},
                {"known_collidable_voxels",n(collidable)},{"voxels",array(copied)},
                {"knownness",q("LOCKED_LIVE_LOADED_NEAR_SECTION_VERSIONS_BEFORE_AFTER_NONATOMIC_BLOCK_READS")},
                {"atomic_copy",b(false)},{"incarnation_ABA",q("OPEN_NO_INCARNATION_IN_PUBLIC_LIVE_VERSION_SNAPSHOT")},
                {"loads_requested",n(0)},{"saves_requested",n(0)}})},
            {"first_person_hand",object({{"actual_presentation_flag",b(actualFirstPersonHandVisible)},
                {"original_UI_buffers",q("OPEN_NOT_CAPTURED")},{"backend_pixel_attribution",q("OPEN_NOT_CAPTURED")}})},
            {"actual_backend_frame",object({{"file",q(p.stem+".png")},{"format",q("PNG")},
                {"width",n(png.width)},{"height",n(png.height)},{"bytes",n(png.bytes)},
                {"producer",q("ROOT_NORMAL_BACKEND_WRITE_CONTENTS_TO_FILE_AFTER_DRAW_BEFORE_SWAP")}})},
            {"GL_observation",q("OPEN_MODULE_MAKES_NO_GL_CALLS")}})+"\n";
        require(text.size()<=JsonLimit && p.storedBytes+png.bytes+text.size()+JsonLimit<=TotalLimit,"session/frame byte bound");
        p.write(p.output/(p.stem+".json"),text);p.storedBytes+=png.bytes+text.size();
        p.frameFiles.push_back(p.stem+".json");p.open=false;p.camera=nullptr;p.world=nullptr;p.selected.clear();
        p.writeIndex("CAPTURED_WITH_OPEN_FIELDS");
    } catch(const std::exception& error) {
        p.failure=error.what();p.open=false;p.camera=nullptr;p.world=nullptr;p.selected.clear();
        try{p.writeIndex("FAILED");}catch(...){}throw;
    }
}
