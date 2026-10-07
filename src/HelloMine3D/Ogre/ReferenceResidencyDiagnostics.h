#pragma once
// Owned hidden engineering observation of real streaming. No normal input,
// alternate World, resource ownership or replacement streaming algorithm.
#include "ReferenceWorldEditDiagnostics.h"
#include "../Sandbox/Events/ChunkEvents.h"
#include "../Player/Player.h"
#include <atomic>
#include <deque>
#include <mutex>
#include <tuple>

namespace ReferenceResidency {
using ReferenceEdit::Fields;using ReferenceEdit::quote;using ReferenceEdit::number;
using ReferenceEdit::boolean;using ReferenceEdit::object;using ReferenceEdit::array;
using ReferenceEdit::xyz;using ReferenceEdit::matrix;using ReferenceEdit::camera;
using ReferenceEdit::lights;using ReferenceEdit::value;using ReferenceEdit::path;
inline void require(bool ok,const std::string& reason){if(!ok)throw std::runtime_error("Reference residency: "+reason);}
inline bool column(glm::ivec3 p){return p.x==12 && (p.z==-12 || p.z==-13);}
inline bool selected(glm::ivec3 p){return column(p) && p.y==4;}
}

class ReferenceResidencyProbe final : public ChunkSectionRenderable::NativeDrawObserver,public Ogre::RenderObjectListener {
public:
    static constexpr unsigned MaximumFrames=4096,MaximumSeconds=45,PhaseMaximumFrames=1024,PhaseMaximumSeconds=12,MaximumJournalRecords=128;
    inline static constexpr std::array<const char*,3> Phases{{"resident-a","departed-b","returned-a"}};
    struct Sample {glm::ivec3 p;ChunkBlock block;};
    inline static const std::array<Sample,4> Samples{{{{201,69,-186},ChunkBlock(Block_t(44),0)},{{194,66,-183},ChunkBlock(Block_t(7),0)},{{199,68,-196},ChunkBlock(Block_t(35),0)},{{196,66,-184},ChunkBlock(Block_t(33),2)}}};
    struct Binding {ChunkSectionRenderable* object=nullptr;glm::ivec3 section{0};std::uint32_t revision=0;std::uint64_t serial=0;PackedTerrainRenderBatch cpu;std::vector<std::string> sourceSections;};
    struct Retired {std::string name;glm::ivec3 origin{0};GLuint vertex=0,index=0;std::size_t vertexBytes=0,indexBytes=0;std::uint64_t serial=0;};
    static void validateEntrypoint(bool validateOnly) {
        if(!std::getenv("HELLOMINE3D_REFERENCE_RESIDENCY_PROBE") && !std::getenv("HELLOMINE3D_REFERENCE_RESIDENCY_FAULT"))return;
        using namespace ReferenceResidency;
        require(value("HELLOMINE3D_REFERENCE_RESIDENCY_PROBE")=="1","exact opt-in 1 required");
        const auto fault=value("HELLOMINE3D_REFERENCE_RESIDENCY_FAULT");require(fault.empty() || fault=="skip-target-retirement","unknown fault");
        require(!validateOnly,"cannot combine HELLOMINE3D_VALIDATE_ONLY");require(value("HELLOMINE3D_EFFECTIVE_MANIFEST_OUT").empty(),"cannot combine HELLOMINE3D_EFFECTIVE_MANIFEST_OUT");
        require(!std::getenv("HELLOMINE3D_RENDER_LIFECYCLE_PROBE") && !std::getenv("HELLOMINE3D_LIFECYCLE_FAULT") && !std::getenv("HELLOMINE3D_REFERENCE_EDIT_PROBE") && !std::getenv("HELLOMINE3D_REFERENCE_EDIT_FAULT"),"cannot combine lifecycle/edit probe or fault");
        require(value("HELLOMINE3D_WINDOW_HIDDEN")=="1" && value("HELLO_RENDER_CAPTURE")=="1" && value("HELLO_RENDER_CAPTURE_MS")=="60000" && value("HELLO_RENDER_CAPTURE_EXIT")=="0" && value("HELLO_RENDER_CAPTURE_MAX_DELTA_MS")=="5000","hidden primary capture outside probe deadline required");
        require(value("HELLOMINE3D_MSAA4")=="1" && value("HELLOMINE3D_WORLD_TIME")=="6000" && value("HELLOMINE3D_SEED")=="42","fixed HDR4 initial time/seed required");
        require(value("HELLOMINE3D_PLAYER_POSITION")=="195.5 68.02 -176.2" && value("HELLOMINE3D_PLAYER_ROTATION")=="5 45 0","fixed initial residency pose required");
        for(const char* k:{"HELLO_PERF_CAPTURE","HELLOMINE3D_PLANAR_DIAGNOSTIC","HELLOMINE3D_PLANAR_REFLECTION_OFF","HELLOMINE3D_HDR_FALLBACK","HELLOMINE3D_REFLECTION_FALLBACK","HELLOMINE3D_FORCE_LEGACY_TERRAIN","HELLOMINE3D_V10C_FALLBACK","HELLOMINE3D_V10D_SHADOW_FALLBACK","HELLOMINE3D_V10E_POST_FALLBACK","HELLOMINE3D_V10D_SHADOW_DIAGNOSTICS","HELLOMINE3D_DISABLE_VERTEX_AO","HELLOMINE3D_V10E_SETTINGS_FIXTURE","HELLOMINE3D_V10D_SHADOW_FIXTURE","HELLOMINE3D_V10E_POST_FIXTURE","HELLOMINE3D_TRANSPARENT_FIXTURE","HELLOMINE3D_SPAWN_VALIDATION_ACTORS","HELLOMINE3D_ORE_FIXTURE","HELLOMINE3D_CONTAINER_FIXTURE","HELLOMINE3D_CRAFTING_FIXTURE","HELLOMINE3D_COMBAT_FIXTURE","HELLOMINE3D_HUD_FIXTURE","HELLOMINE3D_CROP_FIXTURE","HELLOMINE3D_VERTICAL_SLICE_FIXTURE"})require(value(k).empty() || value(k)=="0","cannot combine "+std::string(k));
        for(const char* k:{"HELLO_PERF_CAPTURE_DIR","HELLOMINE3D_RC_PERF_PROFILE","HELLOMINE3D_E2_BATCH_MANIFEST","HELLOMINE3D_REFERENCE_VISUAL_SCENE","HELLOMINE3D_VISUAL_CAMERA_SWEEP","HELLOMINE3D_VISUAL_CAMERA_PATH","HELLOMINE3D_PLAYER_MOTION_CAPTURE","HELLOMINE3D_ACTOR_VISUAL_CAPTURE","HELLOMINE3D_ACTOR_VISUAL_DISTANCE","HELLOMINE3D_BLOCK_FEEDBACK_CAPTURE","HELLOMINE3D_MATERIAL_IDENTITY_CAPTURE_DIR","HELLOMINE3D_CAMERA_DIAGNOSTICS_DIR","HELLOMINE3D_FERN_WIND_CAPTURE_DIR","HELLOMINE3D_SHORE_EDIT_CAPTURE_DIR","HELLOMINE3D_PAUSE_NOTIFICATIONS_DIR","HELLOMINE3D_HUD_PAGE_FIXTURE","HELLOMINE3D_HUD_INSPECT_SLOT","HELLOMINE3D_MACHINE_FIXTURE","HELLOMINE3D_P11_LIGHT_FIXTURE","HELLOMINE3D_VERTEX_LIGHTING_FIXTURE","HELLOMINE3D_CONTROLLED_CRASH","HELLOMINE3D_RESOURCE_PACKS","HELLOMINE3D_REFERENCE_SETTINGS_RESTART_DIR","HELLOMINE3D_EXIT_AFTER_FRAMES","HELLO_VISUAL_CAPTURE"})require(value(k).empty(),"cannot combine "+std::string(k));
        const auto out=path("HELLOMINE3D_REFERENCE_RESIDENCY_DIR"),save=path("HELLOMINE3D_SAVE_DIR"),cat=path("HELLOMINE3D_CATALOGUE_DIR"),root=path("HELLOMINE3D_ROOT"),capture=path("HELLO_RENDER_CAPTURE_DIR"),session=out.parent_path();
        require(out==session/"residency" && save==session/"save" && cat==session/"catalogue" && root==session/"Runtime.app/Contents/Resources" && capture==out,"owned sibling layout required");
        require(!std::filesystem::exists(out) && !std::filesystem::exists(cat),"existing output prohibited");const auto marker=session/".hellomine3d-reference-residency-owned";
        for(const auto& p:{marker,save/"world.meta",save/"chunks"})require(!std::filesystem::is_symlink(std::filesystem::symlink_status(p)),"owned child symlink prohibited");
        require(std::filesystem::is_regular_file(marker) && std::filesystem::is_regular_file(save/"world.meta") && std::filesystem::is_directory(save/"chunks") && std::filesystem::is_directory(root),"real owned marker/save/runtime required");
        std::ifstream in(marker,std::ios::binary);require(std::string((std::istreambuf_iterator<char>(in)),{})=="HelloMine3D owned reference residency session v1\n","owned marker missing");
    }
    static std::string validateEnvironment(bool hdr,bool standard,bool full,unsigned w,unsigned h,unsigned rd,bool medium){if(!std::getenv("HELLOMINE3D_REFERENCE_RESIDENCY_PROBE"))return {};ReferenceResidency::require(hdr && standard && !full && w==1280 && h==720 && rd==3 && medium,"HDR/standard window1280x720 RD3 medium required");return ReferenceResidency::value("HELLOMINE3D_REFERENCE_RESIDENCY_DIR");}
    // Invoked before the loader starts. Never subscribe/unsubscribe concurrently
    // with World publication; the bus dies after its loader has joined.
    ReferenceResidencyProbe(const std::string& out,World& world,Ogre::SceneManager& scene,Ogre::Camera& camera,Ogre::RenderWindow& window):m_output(out),m_scene(scene),m_camera(camera),m_world(&world),m_fault(!ReferenceResidency::value("HELLOMINE3D_REFERENCE_RESIDENCY_FAULT").empty()) {
        struct ConstructionGuard {ReferenceResidencyProbe& owner;SandboxEventBus& bus;std::vector<SandboxEventBus::SubscriptionId> ids;bool complete=false;~ConstructionGuard(){if(!complete){owner.detachDraws();for(const auto id:ids)bus.unsubscribe(id);}}} construction{*this,world.getEventBus(),{},false};
        using namespace ReferenceResidency;require(window.getWidth()==2560 && window.getHeight()==1440,"actual2560x1440 required");require(!std::filesystem::exists(m_output),"output exists");std::filesystem::create_directory(m_output);m_journal.open(m_output/"journal.jsonl");require(bool(m_journal),"journal creation failed");m_started=m_phaseStarted=Clock::now();
        for(int z:{-12,-13}){const auto* c=world.getChunkManager().findChunk(12,z);require(c && c->hasLoaded(),"pre-loader target absent");m_current[z]=chunkCopy(world,12,z,"pre-loader-baseline",false);m_initialIncarnation[z]=c->getIncarnation();}
        for(const auto& s:Samples){const auto b=world.getBlock(s.p.x,s.p.y,s.p.z);require(b==s.block,"r9 original cell/meta differs");}
        for(auto type:{SandboxEventType::ChunkLoaded,SandboxEventType::ChunkUnloaded,SandboxEventType::ChunkSaved})construction.ids.push_back(world.getEventBus().subscribe(type,[this](const SandboxEvent& e){copyEvent(e);},{"ReferenceResidency",SandboxEventHandlerEffect::ObserveOnly,SandboxEventRepublishPolicy::Forbidden}));
        m_scene.addRenderObjectListener(this);m_listener=true;
        emit("header",object({{"normal_input","false"},{"normal_simulation_advances","true"},{"readback_budget","2"},{"maximum_frames",number(MaximumFrames)},{"maximum_seconds",number(MaximumSeconds)},{"phase_maximum_frames",number(PhaseMaximumFrames)},{"phase_maximum_seconds",number(PhaseMaximumSeconds)},{"maximum_journal_records",number(MaximumJournalRecords)},{"tracked_event_ring",number(32)},{"bindings_bound",number(16)},{"native_object_bound",number(64)},{"draw_facts_bound",number(32)},{"total_file_bytes_bound",number(256u*1024u*1024u)},{"fault",quote(m_fault?"skip-target-retirement":"")},{"targets",targetFacts()}}));
        construction.complete=true;
    }
    ~ReferenceResidencyProbe() override {detachDraws();} // World/bus already gone; no unsubscribe through a stale pointer.
    unsigned phase()const noexcept{return m_phase;}bool complete()const noexcept{return m_phase==3;}bool armed()const noexcept{return m_armed;}
    static bool witness(glm::ivec3 p){return p.x==14 && p.y==4 && (p.z==-11 || p.z==-12);}
    bool selected(glm::ivec3 p)const{return ReferenceResidency::selected(p);}bool tracked(glm::ivec3 p)const{return ReferenceResidency::column(p);}
    const glm::vec3& originPosition()const{return m_originPosition;}const glm::vec3& originRotation()const{return m_originRotation;}
    void anchor(const Player& player){m_originPosition=player.position;m_originRotation=player.rotation;}
    template<class Facts>void beginFrame(unsigned frame,unsigned inputs,Facts&& beginFacts){using namespace ReferenceResidency;cancel();m_draws.clear();m_mainAccess=m_reflectionAccess=m_mainOriginAccess=m_reflectionOriginAccess=0;m_uploadFrameFacts="null";m_frame=frame;m_eventFrame.store(frame);m_inputs=inputs;require(inputs==0,"ordinary input observed");bounds();drainEvents();if(!m_begun){m_begun=true;m_phaseFrame=frame;m_phaseStarted=Clock::now();emit("begin",beginFacts());}}
    void arm(const LocalLightSnapshot& lights,bool ready){if(m_phase==1 || !ready || m_frame-m_phaseFrame<12)return;ReferenceResidency::require(!m_bindings.empty(),"selected upload absent");m_armed=true;m_lights=lights;m_draws.clear();}
    bool beginning()const{return !m_begun;}
    bool warm()const{return m_frame-m_phaseFrame>=12;}
    struct WitnessUpload {std::uint32_t revision;std::uint64_t serial;unsigned frame;std::uint64_t incarnation;ChunkMeshState sourceState;std::string source,domain,frameFacts;};
    void witnessUploaded(glm::ivec3 section,std::uint32_t revision,std::uint64_t incarnation,ChunkMeshState state,const char* source,const char* domain){if(witness(section))m_witnessUploads[std::make_tuple(section.x,section.y,section.z)]={revision,++m_witnessSerial,m_frame,incarnation,state,source,domain,std::string(domain)=="frame"?m_uploadFrameFacts:"null"};}
    void recordUploadFrameFacts(std::string facts){m_uploadFrameFacts=std::move(facts);}
    void completeUploadFrameFacts(std::string facts){m_uploadFrameFacts=std::move(facts);for(auto& entry:m_witnessUploads){auto& u=entry.second;if(u.frame==m_frame && u.domain=="frame")u.frameFacts=m_uploadFrameFacts;}}
    void forgetWitness(glm::ivec3 section){if(witness(section))m_witnessUploads.erase(std::make_tuple(section.x,section.y,section.z));}
    std::optional<WitnessUpload> witnessUpload(glm::ivec3 section)const{const auto i=m_witnessUploads.find(std::make_tuple(section.x,section.y,section.z));if(i==m_witnessUploads.end())return {};return i->second;}
    void markReturnMovement(){m_returnWitnessFloor=m_witnessSerial;m_returnMovementFrame=m_frame;}
    unsigned returnMovementFrame()const{return m_returnMovementFrame;}
    std::uint64_t returnWitnessFloor()const{return m_returnWitnessFloor;}
    void uploaded(glm::ivec3 section,std::uint32_t revision){if(selected(section))m_uploaded[section.z]=revision;}
    std::optional<std::uint32_t> uploadedRevision(glm::ivec3 section)const{const auto i=m_uploaded.find(section.z);if(i==m_uploaded.end())return {};return i->second;}
    void retain(ChunkSectionRenderable& object,const std::vector<TerrainRenderBatchPart>& parts,glm::ivec3 origin){
        if(!tracked(origin))return;using namespace ReferenceResidency;require(m_native.size()<64,"tracked native object bound");
        Ogre::RenderOperation op;object.getRenderOperation(op);auto vb=op.vertexData->vertexBufferBinding->getBuffer(0);auto* v=dynamic_cast<Ogre::GL3PlusHardwareVertexBuffer*>(vb.get());auto* i=dynamic_cast<Ogre::GL3PlusHardwareIndexBuffer*>(op.indexData->indexBuffer.get());require(v && i,"native tracked buffers unavailable");
        Retired native{object.getName(),origin,v->getGLBufferId(),i->getGLBufferId(),vb->getSizeInBytes(),op.indexData->indexBuffer->getSizeInBytes(),++m_uploadSerial};m_native.emplace(&object,native);
        bool include=false;for(const auto& p:parts)include=include || selected(p.location);if(!include)return;
        const auto name=object.getMaterial()->getName();if(name!="HelloMine3D/Terrain" && name!="HelloMine3D/Water")return;
        require(m_bindings.size()<16,"selected binding bound");Binding b;b.object=&object;b.section=origin;b.serial=native.serial;b.revision=uploadedRevision(origin).value_or(0);b.cpu=packTerrainRenderBatch(parts,origin);for(const auto& p:parts)b.sourceSections.push_back(xyz(p.location));require(b.cpu.vertices.size()*44+b.cpu.indices.size()*4<=16u*1024u*1024u,"CPU copy bound");m_bindings.emplace(&object,std::move(b));object.setNativeDrawObserver(this);
    }
    std::optional<Retired> detach(ChunkSectionRenderable& o){o.setNativeDrawObserver(nullptr);m_pending.erase(&o);m_bindings.erase(&o);const auto i=m_native.find(&o);if(i==m_native.end())return {};const auto copy=i->second;m_native.erase(i);return copy;}
    void retired(const std::vector<Retired>& objects){using namespace ReferenceResidency;for(const auto& o:objects){const bool v=glIsBuffer(o.vertex)==GL_TRUE,i=glIsBuffer(o.index)==GL_TRUE;const auto error=glGetError();emit("gpu-retired",object({{"name",quote(o.name)},{"origin",xyz(o.origin)},{"upload_serial",number(o.serial)},{"vbo",number(o.vertex)},{"ibo",number(o.index)},{"vertex_bytes",number(o.vertexBytes)},{"index_bytes",number(o.indexBytes)},{"vbo_alive",boolean(v)},{"ibo_alive",boolean(i)},{"gl_error",number(error)},{"observation",quote("immediate-before-next-allocate")}}));require(!v && !i && error==GL_NO_ERROR,"retired native storage still alive");}}
    bool skipRetirement(glm::ivec3 section){if(!m_fault || m_failed || m_phase!=1)return false;if(m_faultConsumed)return section==m_faultSection;if(selected(section)){m_faultConsumed=true;m_faultSection=section;event("fault-retained",nativeFacts());return true;}return false;}
    std::string nativeFacts()const{using namespace ReferenceResidency;std::vector<std::string> values;for(const auto& entry:m_native){const auto& o=entry.second;values.push_back(object({{"name",quote(o.name)},{"origin",xyz(o.origin)},{"upload_serial",number(o.serial)},{"vbo",number(o.vertex)},{"ibo",number(o.index)},{"vertex_bytes",number(o.vertexBytes)},{"index_bytes",number(o.indexBytes)},{"vbo_alive",boolean(glIsBuffer(o.vertex)==GL_TRUE)},{"ibo_alive",boolean(glIsBuffer(o.index)==GL_TRUE)}}));}const auto error=glGetError();require(error==GL_NO_ERROR,"native inventory GL error");return array(values);}
    std::string targetFacts()const{std::lock_guard<std::mutex> lock(m_eventsMutex);std::vector<std::string>a;for(const auto& e:m_current)a.push_back(e.second);return ReferenceResidency::array(a);}
    bool targetsAbsent()const{std::lock_guard<std::mutex> lock(m_eventsMutex);return m_unloaded[0] && m_unloaded[1] && !m_present[0] && !m_present[1];}
    bool returned()const{std::lock_guard<std::mutex> lock(m_eventsMutex);return m_reloaded[0] && m_reloaded[1] && m_present[0] && m_present[1];}
    std::string drawFacts()const{return ReferenceResidency::array(m_draws);}
    std::string aggregateEventFacts()const{using namespace ReferenceResidency;return object({{"loaded",number(m_aggregateEvents[0].load())},{"unloaded",number(m_aggregateEvents[1].load())},{"saved",number(m_aggregateEvents[2].load())}});}
    std::string accessFacts()const{using namespace ReferenceResidency;return object({{"domain",quote("production-RenderObjectListener-before-native-draw")},{"frame",number(m_frame)},{"main_callbacks",number(m_mainAccess)},{"reflection_callbacks",number(m_reflectionAccess)},{"origin_main_accesses",number(m_mainOriginAccess)},{"origin_reflection_accesses",number(m_reflectionOriginAccess)}});}
    void checkInventory()const{std::uintmax_t bytes=0;unsigned files=0;for(const auto& e:std::filesystem::directory_iterator(m_output)){ReferenceResidency::require(e.is_regular_file() && !e.is_symlink(),"nonregular diagnostic artifact");bytes+=e.file_size();ReferenceResidency::require(++files<=300 && bytes<=256u*1024u*1024u,"whole diagnostic file/byte budget");}}
    std::string prefix()const{return (m_output/Phases[m_phase]).string();}std::string mainPng()const{return prefix()+".main.png";}
    void checkpoint(const std::string& facts){using namespace ReferenceResidency;emit("checkpoint",facts);std::ofstream out(m_output/(std::string(Phases[m_phase])+".json"));out<<facts<<'\n';require(bool(out),"checkpoint write failed");if(m_phase!=1){require(m_armed && m_mainPbr && m_reflectionPbr,"actual main/private PBR draw missing");require(++m_readbacks<=2,"readback bound");}emit("end","{}");cancel();++m_phase;m_begun=false;m_phaseStarted=Clock::now();}
    void event(const char* name,const std::string& facts){drainEvents();if(!m_failed){const std::string n(name);if(n=="normal-save"){ReferenceResidency::require(m_phase==3 && m_tail==0,"save tail order");m_tail=1;}else if(n=="world-cleared"){ReferenceResidency::require(m_tail==1 && !m_world,"clear tail order");m_tail=2;}else if(n=="components-destroyed"){ReferenceResidency::require(m_tail==2 && !m_listener,"component tail order");m_tail=3;}else if(n=="root-shutdown"){ReferenceResidency::require(m_tail==3,"root tail order");m_tail=4;}}emit(name,facts);}
    void worldDestroyed(){m_world=nullptr;drainEvents();}
    void detachDraws()noexcept{cancel();for(auto& e:m_bindings)if(e.first)e.first->setNativeDrawObserver(nullptr);m_bindings.clear();m_native.clear();m_witnessUploads.clear();if(m_listener){m_scene.removeRenderObjectListener(this);m_listener=false;}}
    void finish(){ReferenceResidency::require(complete() && m_world==nullptr && m_tail==4 && !m_listener && m_native.empty(),"three phases/ordered teardown incomplete");bounds();writeSummary("COMPLETE","");}
    void fail(const std::string& reason,const std::string& snapshot)noexcept{try{m_failed=true;cancel();emit("failure",ReferenceResidency::object({{"reason",ReferenceResidency::quote(reason)},{"snapshot",snapshot}}));writeSummary("FAIL",reason);}catch(...){}}
    void notifyRenderSingleObject(Ogre::Renderable* renderable,const Ogre::Pass* pass,const Ogre::AutoParamDataSource* source,const Ogre::LightList*,bool suppressed) override {
        const auto* current=source?source->getCurrentCamera():nullptr;
        const bool main=current==&m_camera,reflection=current && current->getName().find("/PlanarWaterReflection/Camera")!=std::string::npos;
        if(m_phase==1 && !suppressed && pass && (main || reflection)){if(main)++m_mainAccess;else ++m_reflectionAccess;auto* target=dynamic_cast<ChunkSectionRenderable*>(renderable);if(target && m_native.count(target)){if(main)++m_mainOriginAccess;else ++m_reflectionOriginAccess;}}
        if(!m_armed)return;auto* object=dynamic_cast<ChunkSectionRenderable*>(renderable);if(!object || !m_bindings.count(object))return;
        auto& p=m_pending[object];ReferenceResidency::require(!p.query,"nested listener query");p.pass=nullptr;p.camera=source?source->getCurrentCamera():nullptr;
        if(suppressed || !pass || !p.camera)return;
        if(p.camera!=&m_camera && p.camera->getName().find("/PlanarWaterReflection/Camera")==std::string::npos)return;
        p.pass=pass;
    }
    void beforeNativeDraw(ChunkSectionRenderable& object,Ogre::SceneManager* scene,Ogre::RenderSystem* renderer) override {
        if(!m_armed || !m_bindings.count(&object))return;auto& p=m_pending[&object];if(!p.pass)return;
        ReferenceResidency::require(scene==&m_scene && renderer && p.pass->getPassIterationCount()==1 && !p.pass->hasGeometryProgram(),"unsupported native draw scope");
        GLint previous=0;glGetQueryiv(GL_PRIMITIVES_GENERATED,GL_CURRENT_QUERY,&previous);ReferenceResidency::require(previous==0,"existing primitive query left untouched");
        glGenQueries(1,&p.query);ReferenceResidency::require(p.query!=0,"query allocation failed");glBeginQuery(GL_PRIMITIVES_GENERATED,p.query);
    }
    void afterNativeDraw(ChunkSectionRenderable& object,Ogre::SceneManager*,Ogre::RenderSystem*) override {
        if(!m_armed || !m_bindings.count(&object))return;auto& p=m_pending[&object];if(!p.query)return;
        const GLuint query=p.query;glEndQuery(GL_PRIMITIVES_GENERATED);GLuint generated=0;glGetQueryObjectuiv(query,GL_QUERY_RESULT,&generated);glDeleteQueries(1,&query);p.query=0;
        observe(object,p,generated);p.pass=nullptr;
    }
private:
    using Clock=std::chrono::steady_clock;
    struct Pending{const Ogre::Pass* pass=nullptr;const Ogre::Camera* camera=nullptr;GLuint query=0;};
    struct Event{std::string name,facts;unsigned frame;int z;};
    std::filesystem::path m_output;Ogre::SceneManager& m_scene;Ogre::Camera& m_camera;World* m_world;
    std::ofstream m_journal;Clock::time_point m_started,m_phaseStarted;unsigned m_frame=0,m_phaseFrame=0,m_phase=0,m_sequence=0,m_inputs=0,m_readbacks=0;
    unsigned m_tail=0,m_mainAccess=0,m_reflectionAccess=0,m_mainOriginAccess=0,m_reflectionOriginAccess=0;bool m_failed=false;
    bool m_begun=false,m_armed=false,m_mainPbr=false,m_reflectionPbr=false,m_fault=false,m_faultConsumed=false,m_listener=false;
    std::map<std::tuple<int,int,int>,WitnessUpload>m_witnessUploads;std::uint64_t m_witnessSerial=0,m_returnWitnessFloor=0;unsigned m_returnMovementFrame=0;glm::ivec3 m_faultSection{0};glm::vec3 m_originPosition{0},m_originRotation{0};std::map<int,std::uint32_t>m_uploaded;std::map<ChunkSectionRenderable*,Binding>m_bindings;std::map<ChunkSectionRenderable*,Pending>m_pending;std::map<ChunkSectionRenderable*,Retired>m_native;
    std::string m_uploadFrameFacts="null";std::vector<std::string>m_draws;LocalLightSnapshot m_lights;std::uint64_t m_uploadSerial=0,m_writtenBytes=0;
    std::array<std::atomic<unsigned>,3>m_aggregateEvents{};mutable std::mutex m_eventsMutex;std::deque<Event>m_events;std::map<int,std::string>m_current;std::map<int,std::uint64_t>m_initialIncarnation;std::string m_eventFailure;std::atomic<unsigned>m_eventFrame{0};std::array<bool,2>m_present{{true,true}},m_unloaded{{false,false}},m_reloaded{{false,false}};
    static double milliseconds(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
    void bounds()const{using namespace ReferenceResidency;require(m_frame<MaximumFrames && milliseconds(m_started)<MaximumSeconds*1000,"session frame/wall bound");if(m_begun)require(m_frame-m_phaseFrame<PhaseMaximumFrames && milliseconds(m_phaseStarted)<PhaseMaximumSeconds*1000,"phase frame/wall bound");}
    // Only the pre-loader constructor and production ChunkManager event sites
    // call this; latter are inside the existing World mutex/commit boundary.
    static std::string chunkCopy(World& w,int x,int z,const char* observation,bool storage){using namespace ReferenceResidency;const auto* c=w.getChunkManager().findChunk(x,z);std::vector<std::string>cells;if(c && c->hasLoaded())for(const auto& s:Samples)if(World::getChunkXZ(s.p.x,s.p.z)==VectorXZ{x,z}){const auto local=World::getBlockXZ(s.p.x,s.p.z);const auto b=c->getBlock(local.x,s.p.y,local.z);cells.push_back(object({{"position",xyz(s.p)},{"id",number(b.id)},{"metadata",number(b.metadata)}}));}
        return object({{"coord",array({number(x),number(z)})},{"present",boolean(c!=nullptr)},{"data_residency",number(c?unsigned(c->getDataResidencyState()):0u)},{"incarnation",c?number(c->getIncarnation()):"null"},{"needs_save",boolean(c && c->needsSave())},{"cells",array(cells)},{"observation",quote(observation)},{"from_storage",boolean(storage)}});}
    void copyEvent(const SandboxEvent& event)noexcept{try{using namespace ReferenceResidency;VectorXZ p;bool storage=false;const char* name=nullptr;if(event.type==SandboxEventType::ChunkLoaded){const auto& e=static_cast<const ChunkLoadedEvent&>(event);p=e.position;storage=e.fromStorage;name="chunk-loaded";}else if(event.type==SandboxEventType::ChunkSaved){p=static_cast<const ChunkSavedEvent&>(event).position;name="chunk-saved";}else{p=static_cast<const ChunkUnloadedEvent&>(event).position;name="chunk-unloaded";}++m_aggregateEvents[event.type==SandboxEventType::ChunkLoaded?0:event.type==SandboxEventType::ChunkUnloaded?1:2];if(p.x!=12 || (p.z!=-12 && p.z!=-13))return;
        const auto facts=chunkCopy(*m_world,p.x,p.z,name,storage);const auto* c=m_world->getChunkManager().findChunk(p.x,p.z);const unsigned slot=p.z==-12?0:1;std::lock_guard<std::mutex>lock(m_eventsMutex);require(m_events.size()<32,"tracked event ring overflow");m_current[p.z]=facts;m_present[slot]=c!=nullptr;if(event.type==SandboxEventType::ChunkUnloaded){require(!c,"unloaded event retained target");m_unloaded[slot]=true;}if(event.type==SandboxEventType::ChunkLoaded && m_unloaded[slot]){require(c && c->hasLoaded() && storage && c->getIncarnation()!=m_initialIncarnation.at(p.z),"return lacks storage/new incarnation");for(const auto& s:Samples)if(World::getChunkXZ(s.p.x,s.p.z)==p){const auto l=World::getBlockXZ(s.p.x,s.p.z);require(c->getBlock(l.x,s.p.y,l.z)==s.block,"reloaded cell/meta changed");}m_reloaded[slot]=true;}m_events.push_back({name,facts,m_eventFrame.load(),p.z});
        }catch(const std::exception& e){std::lock_guard<std::mutex>lock(m_eventsMutex);m_eventFailure=e.what();}}
    void drainEvents(){std::deque<Event>events;std::string error;{std::lock_guard<std::mutex>lock(m_eventsMutex);events.swap(m_events);error=m_eventFailure;}for(const auto& e:events){if(e.name=="chunk-unloaded")m_uploaded.erase(e.z);emit(e.name.c_str(),ReferenceResidency::object({{"observed_main_frame",ReferenceResidency::number(e.frame)},{"target",e.facts}}));}ReferenceResidency::require(error.empty(),error);}
    void emit(const char* event,const std::string& snapshot){using namespace ReferenceResidency;require(m_sequence<MaximumJournalRecords,"journal bound");checkInventory();m_journal<<object({{"schema",quote("hellomine3d-reference-residency-journal-v1")},{"sequence",number(++m_sequence)},{"event",quote(event)},{"phase",quote(m_phase<3?Phases[m_phase]:"complete")},{"frame",number(m_frame)},{"elapsed_ms",number(milliseconds(m_started))},{"normal_input","false"},{"input_event_count",number(m_inputs)},{"snapshot",snapshot}})<<'\n'<<std::flush;require(bool(m_journal),"journal write failed");}
    void writeSummary(const char* status,const std::string& reason){using namespace ReferenceResidency;std::ofstream o(m_output/"summary.json");o<<object({{"schema",quote("hellomine3d-reference-residency-summary-v1")},{"status",quote(status)},{"completed_phases",number(m_phase)},{"tail_complete",boolean(std::string(status)=="COMPLETE")},{"normal_input","false"},{"input_event_count",number(m_inputs)},{"normal_simulation_advances","true"},{"readbacks",number(m_readbacks)},{"frames",number(m_frame)},{"elapsed_ms",number(milliseconds(m_started))},{"fault",quote(m_fault?"skip-target-retirement":"")},{"reason",quote(reason)}})<<'\n';require(bool(o),"summary write failed");}
    void cancel() noexcept {
        for(auto& entry:m_pending)if(entry.second.query){GLint active=0;glGetQueryiv(GL_PRIMITIVES_GENERATED,GL_CURRENT_QUERY,&active);if(GLuint(active)==entry.second.query)glEndQuery(GL_PRIMITIVES_GENERATED);glDeleteQueries(1,&entry.second.query);entry.second.query=0;}
        m_pending.clear();m_armed=false;m_mainPbr=m_reflectionPbr=false;
    }
    void bytes(const std::string& file,const void* data,std::size_t size) {
        ReferenceResidency::require(size<=16u*1024u*1024u && m_writtenBytes+size<=256u*1024u*1024u,"byte evidence budget");m_writtenBytes+=size;
        const auto path=m_output/file;ReferenceResidency::require(!std::filesystem::exists(path),"buffer evidence overwrite");std::ofstream out(path,std::ios::binary);out.write(static_cast<const char*>(data),std::streamsize(size));ReferenceResidency::require(bool(out),"byte evidence write failed");
    }
    void observe(ChunkSectionRenderable& object,const Pending& p,GLuint generated) {
        using namespace ReferenceResidency;require(m_draws.size()<32,"native operation count bound");
        GLint program=0,pipeline=0,vao=0,ebo=0,copy=0;glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_PROGRAM_PIPELINE_BINDING,&pipeline);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&ebo);glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&copy);
        require(program && !pipeline && vao && glIsProgram(GLuint(program)) && glIsVertexArray(GLuint(vao)),"actual linked GL program/VAO missing");
        GLint linked=0;glGetProgramiv(GLuint(program),GL_LINK_STATUS,&linked);require(linked==GL_TRUE,"actual program unlinked");
        const auto* vs=dynamic_cast<const Ogre::GLSLShader*>(p.pass->getVertexProgram().get());const auto* fs=dynamic_cast<const Ogre::GLSLShader*>(p.pass->getFragmentProgram().get());require(vs && fs,"production GLSL stages unavailable");
        GLuint attached[2]{};GLsizei count=0;glGetAttachedShaders(GLuint(program),2,&count,attached);require(count==2 && ((attached[0]==vs->getGLShaderHandle() && attached[1]==fs->getGLShaderHandle()) || (attached[1]==vs->getGLShaderHandle() && attached[0]==fs->getGLShaderHandle())),"actual attached shader/pass mismatch");
        Ogre::RenderOperation operation;object.getRenderOperation(operation);require(operation.srcRenderable==&object && operation.useIndexes && operation.numberOfInstances==1 && operation.operationType==Ogre::RenderOperation::OT_TRIANGLE_LIST && operation.vertexData && operation.indexData && operation.vertexData->vertexStart==0 && operation.indexData->indexStart==0,"non-production indexed triangle operation");
        require(generated==operation.indexData->indexCount/3,"actual primitive coverage mismatch");
        auto hardware=operation.vertexData->vertexBufferBinding->getBuffer(0);auto* vertex=dynamic_cast<Ogre::GL3PlusHardwareVertexBuffer*>(hardware.get());auto* index=dynamic_cast<Ogre::GL3PlusHardwareIndexBuffer*>(operation.indexData->indexBuffer.get());
        require(vertex && index && hardware->getVertexSize()==44 && operation.indexData->indexBuffer->getType()==Ogre::HardwareIndexBuffer::IT_32BIT,"production packed buffer format mismatch");
        const auto& b=m_bindings.at(&object);const std::size_t vb=b.cpu.vertices.size()*44,ib=b.cpu.indices.size()*4;
        require(vb && ib && operation.vertexData->vertexCount==b.cpu.vertices.size() && operation.indexData->indexCount==b.cpu.indices.size(),"original CPU upload operation differs");
        require(glIsBuffer(vertex->getGLBufferId()) && glIsBuffer(index->getGLBufferId()) && GLuint(ebo)==index->getGLBufferId(),"actual bound EBO/storage missing");
        std::vector<std::string> attributes;
        const std::array<const char*,5> names{{"vertex","uv0","uv1","uv2","uv3"}};
        const std::array<GLint,5> components{{3,2,2,3,1}};
        const std::array<std::uintptr_t,5> offsets{{0,12,20,28,40}};
        for(unsigned slot=0;slot<names.size();++slot) {
            const GLint location=glGetAttribLocation(GLuint(program),names[slot]);if(location<0)continue;
            GLint enabled=0,buffer=0,type=0,size=0,stride=0,normal=0,integer=0,divisor=0;
            glGetVertexAttribiv(GLuint(location),GL_VERTEX_ATTRIB_ARRAY_ENABLED,&enabled);
            glGetVertexAttribiv(GLuint(location),GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&buffer);
            glGetVertexAttribiv(GLuint(location),GL_VERTEX_ATTRIB_ARRAY_TYPE,&type);
            glGetVertexAttribiv(GLuint(location),GL_VERTEX_ATTRIB_ARRAY_SIZE,&size);
            glGetVertexAttribiv(GLuint(location),GL_VERTEX_ATTRIB_ARRAY_STRIDE,&stride);
            glGetVertexAttribiv(GLuint(location),GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&normal);
            glGetVertexAttribiv(GLuint(location),GL_VERTEX_ATTRIB_ARRAY_INTEGER,&integer);
            glGetVertexAttribiv(GLuint(location),GL_VERTEX_ATTRIB_ARRAY_DIVISOR,&divisor);
            void* pointer=nullptr;glGetVertexAttribPointerv(GLuint(location),GL_VERTEX_ATTRIB_ARRAY_POINTER,&pointer);
            const auto offset=reinterpret_cast<std::uintptr_t>(pointer);
            require(enabled==GL_TRUE && GLuint(buffer)==vertex->getGLBufferId() && type==GL_FLOAT &&
                size==components[slot] && stride==44 && !normal && !integer && !divisor && offset==offsets[slot],
                "actual VAO packed attribute fetch differs from original VBO");
            attributes.push_back(ReferenceResidency::object({{"name",quote(names[slot])},{"location",number(location)},
                {"enabled",boolean(enabled==GL_TRUE)},{"buffer",number(buffer)},{"type",number(type)},
                {"size",number(size)},{"stride",number(stride)},{"normalized",boolean(normal!=0)},
                {"integer",boolean(integer!=0)},{"divisor",number(divisor)},{"pointer_offset",number(offset)}}));
        }
        require(glGetAttribLocation(GLuint(program),"vertex")>=0 && glGetAttribLocation(GLuint(program),"uv2")>=0,
            "actual production position/light inputs absent");

        std::vector<unsigned char> v(vb),i(ib);GLint64 allocated=0;
        struct CopyGuard { GLint saved;~CopyGuard(){glBindBuffer(GL_COPY_READ_BUFFER,GLuint(saved));} } guard{copy};
        glBindBuffer(GL_COPY_READ_BUFFER,vertex->getGLBufferId());glGetBufferParameteri64v(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&allocated);require(allocated==GLint64(vb),"actual VBO size mismatch");glGetBufferSubData(GL_COPY_READ_BUFFER,0,GLsizeiptr(vb),v.data());
        glBindBuffer(GL_COPY_READ_BUFFER,index->getGLBufferId());glGetBufferParameteri64v(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&allocated);require(allocated==GLint64(ib),"actual IBO size mismatch");glGetBufferSubData(GL_COPY_READ_BUFFER,0,GLsizeiptr(ib),i.data());
        glBindBuffer(GL_COPY_READ_BUFFER,GLuint(copy));GLint restored=0;glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&restored);require(restored==copy,"GL copy binding not restored");
        const bool equal=std::memcmp(v.data(),b.cpu.vertices.data(),vb)==0 && std::memcmp(i.data(),b.cpu.indices.data(),ib)==0;require(equal,"actual GPU/production CPU upload bytes differ");
        const auto stem=std::string(Phases[m_phase])+"-draw"+std::to_string(m_draws.size());bytes(stem+".vbo.bin",v.data(),vb);bytes(stem+".ibo.bin",i.data(),ib);bytes(stem+".cpu-vbo.bin",b.cpu.vertices.data(),vb);bytes(stem+".cpu-ibo.bin",b.cpu.indices.data(),ib);
        const bool main=p.camera==&m_camera;const GLint lightLocation=glGetUniformLocation(GLuint(program),"localLightCount");std::string actualLights="null";
        if(lightLocation>=0){GLint actual=0;glGetUniformiv(GLuint(program),lightLocation,&actual);require(actual==GLint(m_lights.count),"actual linked light count differs from World snapshot");std::vector<std::string> positions,colours;
            for(unsigned slot=0;slot<8;++slot){float a[4]{},c[4]{};const auto n=std::to_string(slot);const GLint al=glGetUniformLocation(GLuint(program),("localLightPositionRadius["+n+"]").c_str()),cl=glGetUniformLocation(GLuint(program),("localLightColourEnergy["+n+"]").c_str());require(al>=0 && cl>=0,"actual light array interface absent");glGetUniformfv(GLuint(program),al,a);glGetUniformfv(GLuint(program),cl,c);
                const LocalLightSource empty;const auto& s=slot<m_lights.count?m_lights.sources[slot]:empty;float expectedA[4]{},expectedC[4]{};if(slot<m_lights.count){expectedA[0]=s.position.x;expectedA[1]=s.position.y;expectedA[2]=s.position.z;expectedA[3]=s.radius;expectedC[0]=s.colour.x;expectedC[1]=s.colour.y;expectedC[2]=s.colour.z;expectedC[3]=s.energy;}require(std::memcmp(a,expectedA,sizeof a)==0 && std::memcmp(c,expectedC,sizeof c)==0,"actual linked light arrays differ from same-frame World snapshot");
                positions.push_back(array({number(a[0]),number(a[1]),number(a[2]),number(a[3])}));colours.push_back(array({number(c[0]),number(c[1]),number(c[2]),number(c[3])}));}
            actualLights=objectJsonLights(actual,positions,colours);if(main)m_mainPbr=true;else m_reflectionPbr=true;
        }
        std::vector<std::string> times;for(const char* name:{"globalTime","legacyTime"}){const GLint location=glGetUniformLocation(GLuint(program),name);if(location>=0){float time=0;glGetUniformfv(GLuint(program),location,&time);times.push_back(ReferenceResidency::object({{"name",quote(name)},{"value",number(time)}}));}}
        const auto error=glGetError();require(error==GL_NO_ERROR,"native observation GL error");
        m_draws.push_back(ReferenceResidency::object({{"view",quote(main?"main":"reflection")},{"object_name",quote(object.getName())},{"section",xyz(b.section)},{"source_sections",array(b.sourceSections)},{"uploaded_revision",number(b.revision)},{"upload_serial",number(b.serial)},{"frame",number(m_frame)},{"camera",camera(*p.camera)},
            {"material",quote(p.pass->getParent()->getParent()->getName())},{"vertex_program",quote(p.pass->getVertexProgramName())},{"fragment_program",quote(p.pass->getFragmentProgramName())},{"program",number(program)},{"program_linked","true"},{"production_attached_shaders","true"},{"vao",number(vao)},{"vbo",number(vertex->getGLBufferId())},{"ibo",number(index->getGLBufferId())},{"gl_is_buffer","true"},{"attributes",array(attributes)},{"vertex_bytes",number(vb)},{"index_bytes",number(ib)},{"primitive_count",number(generated)},{"expected_triangles",number(b.cpu.indices.size()/3)},{"cpu_gpu_bytes_equal",boolean(equal)},
            {"vbo_file",quote(stem+".vbo.bin")},{"ibo_file",quote(stem+".ibo.bin")},{"cpu_vbo_file",quote(stem+".cpu-vbo.bin")},{"cpu_ibo_file",quote(stem+".cpu-ibo.bin")},{"local_lights",actualLights},{"time_uniforms",array(times)},{"gl_error",number(error)},{"state_restored","true"}}));
    }
    static std::string objectJsonLights(int count,const std::vector<std::string>& p,const std::vector<std::string>& c) { return ReferenceResidency::object({{"domain",ReferenceResidency::quote("actual-linked-GL-program-after-production-draw")},{"count",ReferenceResidency::number(count)},{"position_radius",ReferenceResidency::array(p)},{"colour_energy",ReferenceResidency::array(c)}}); }
};
