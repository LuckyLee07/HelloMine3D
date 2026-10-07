#pragma once
// Default-off owned-clone engineering evidence. Never an input driver or a
// replacement for ordinary editing acceptance. Native facts own no GPU object.
#include "ChunkSectionRenderable.h"
#include "RenderLifecycleDiagnostics.h"
#include "../World/World.h"
#include <OgreAutoParamDataSource.h>
#include <OgreRenderObjectListener.h>
#include <OgreGL3PlusHardwareVertexBuffer.h>
#include <OgreGL3PlusHardwareIndexBuffer.h>
#include <GLSL/OgreGLSLShader.h>
#include <cstring>
#include <iomanip>
#include <map>
#include <optional>

namespace ReferenceEdit {
using Fields = std::vector<std::pair<std::string,std::string>>;
inline void require(bool value,const std::string& reason) { if(!value)throw std::runtime_error("Reference world edit: "+reason); }
inline std::string quote(const std::string& s) { return RenderLifecycle::quote(s); }
template<class T> inline std::string number(T value) { require(std::isfinite(double(value)),"nonfinite fact");std::ostringstream o;o<<std::setprecision(17)<<+value;return o.str(); }
inline std::string boolean(bool value) { return value?"true":"false"; }
inline std::string object(const Fields& f) { std::string s="{";for(const auto& v:f){if(s.size()>1)s+=',';s+=quote(v.first)+':'+v.second;}return s+'}'; }
inline std::string array(const std::vector<std::string>& f) { std::string s="[";for(const auto& v:f){if(s.size()>1)s+=',';s+=v;}return s+']'; }
template<class V> inline std::string xyz(const V& v) { return array({number(v.x),number(v.y),number(v.z)}); }
inline std::string matrix(const Ogre::Matrix4& m) { std::vector<std::string> a;for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)a.push_back(number(m[r][c]));return array(a); }
inline std::string camera(const Ogre::Camera& c) {
    const auto q=c.getDerivedOrientation();const auto off=c.getFrustumOffset();
    return object({{"eye",xyz(c.getDerivedPosition())},{"quaternion_wxyz",array({number(q.w),number(q.x),number(q.y),number(q.z)})},
        {"fov_y_radians",number(c.getFOVy().valueRadians())},{"aspect",number(c.getAspectRatio())},
        {"near",number(c.getNearClipDistance())},{"far",number(c.getFarClipDistance())},
        {"frustum_offset",array({number(off.x),number(off.y)})},{"view_row_major",matrix(c.getViewMatrix())}});
}
inline std::string lights(const LocalLightSnapshot& l) {
    std::vector<std::string> sources;for(std::size_t i=0;i<l.count;++i){const auto& s=l.sources[i];sources.push_back(object({{"position",xyz(s.position)},{"radius",number(s.radius)},{"colour",xyz(s.colour)},{"energy",number(s.energy)}}));}
    return object({{"revision",number(l.revision)},{"count",number(l.count)},{"inspected_sections",number(l.inspectedSections)},
        {"inspected_cells",number(l.inspectedCells)},{"candidates",number(l.candidates)},{"sources",array(sources)}});
}
inline std::string value(const char* key) { const char* v=std::getenv(key);return v?v:""; }
inline std::filesystem::path path(const char* key) {
    namespace fs=std::filesystem;const fs::path p(value(key));
    require(!p.empty() && p.is_absolute() && p.lexically_normal()==p,"explicit canonical path required: "+std::string(key));
    for(auto a=p;!a.empty();a=a.parent_path()){require(!fs::is_symlink(fs::symlink_status(a)),"symlink path prohibited");if(a==a.parent_path())break;}
    require(fs::weakly_canonical(p)==p,"noncanonical path prohibited");return p;
}
}

class ReferenceWorldEditProbe final : public ChunkSectionRenderable::NativeDrawObserver, public Ogre::RenderObjectListener {
public:
    static constexpr unsigned MaximumFrames=2048, MaximumSeconds=30, PhaseMaximumSeconds=8, MaximumJournalRecords=64;
    inline static constexpr std::array<const char*,3> Phases{{"baseline-a","edited-b","restored-a"}};
    struct Edit { glm::ivec3 position; ChunkBlock original, changed; };
    struct Binding { ChunkSectionRenderable* object=nullptr;glm::ivec3 section{0};std::uint32_t revision=0;std::uint64_t serial=0;PackedTerrainRenderBatch cpu; };
    static void validateEntrypoint(bool validateOnly) {
        if(!std::getenv("HELLOMINE3D_REFERENCE_EDIT_PROBE") && !std::getenv("HELLOMINE3D_REFERENCE_EDIT_FAULT"))return;
        using namespace ReferenceEdit;
        const auto mode=value("HELLOMINE3D_REFERENCE_EDIT_PROBE"),fault=value("HELLOMINE3D_REFERENCE_EDIT_FAULT");
        require(mode=="lamp" || mode=="wall" || mode=="shore","exact lamp|wall|shore opt-in required");
        require(fault.empty() || fault=="skip-reflection-update","unknown native fault");
        require(!validateOnly,"cannot combine HELLOMINE3D_VALIDATE_ONLY");
        require(value("HELLOMINE3D_EFFECTIVE_MANIFEST_OUT").empty(),"cannot combine HELLOMINE3D_EFFECTIVE_MANIFEST_OUT");
        require(!std::getenv("HELLOMINE3D_RENDER_LIFECYCLE_PROBE") && !std::getenv("HELLOMINE3D_LIFECYCLE_FAULT"),"cannot combine render lifecycle probe/fault");
        require(value("HELLOMINE3D_WINDOW_HIDDEN")=="1" && value("HELLO_RENDER_CAPTURE")=="1","hidden primary render capture required");
        require(value("HELLO_RENDER_CAPTURE_MS")=="60000" && value("HELLO_RENDER_CAPTURE_EXIT")=="0" && value("HELLO_RENDER_CAPTURE_MAX_DELTA_MS")=="5000","capture timer must remain outside bounded edit session");
        require(value("HELLOMINE3D_MSAA4")=="1" && value("HELLOMINE3D_WORLD_TIME")=="6000" && value("HELLOMINE3D_SEED")=="42","fixed HDR4/world time/seed required");
        require(value("HELLOMINE3D_PLAYER_POSITION")=="195.5 68.02 -176.2" && value("HELLOMINE3D_PLAYER_ROTATION")=="5 45 0","fixed edit camera input required");
        for(const char* k:{"HELLO_PERF_CAPTURE","HELLOMINE3D_PLANAR_DIAGNOSTIC","HELLOMINE3D_PLANAR_REFLECTION_OFF","HELLOMINE3D_HDR_FALLBACK","HELLOMINE3D_REFLECTION_FALLBACK","HELLOMINE3D_FORCE_LEGACY_TERRAIN","HELLOMINE3D_V10C_FALLBACK","HELLOMINE3D_V10D_SHADOW_FALLBACK","HELLOMINE3D_V10E_POST_FALLBACK","HELLOMINE3D_V10D_SHADOW_DIAGNOSTICS","HELLOMINE3D_DISABLE_VERTEX_AO","HELLOMINE3D_V10E_SETTINGS_FIXTURE","HELLOMINE3D_V10D_SHADOW_FIXTURE","HELLOMINE3D_V10E_POST_FIXTURE","HELLOMINE3D_TRANSPARENT_FIXTURE","HELLOMINE3D_SPAWN_VALIDATION_ACTORS","HELLOMINE3D_ORE_FIXTURE","HELLOMINE3D_CONTAINER_FIXTURE","HELLOMINE3D_CRAFTING_FIXTURE","HELLOMINE3D_COMBAT_FIXTURE","HELLOMINE3D_HUD_FIXTURE","HELLOMINE3D_CROP_FIXTURE","HELLOMINE3D_VERTICAL_SLICE_FIXTURE"})
            require(value(k).empty() || value(k)=="0","cannot combine "+std::string(k));
        for(const char* k:{"HELLO_PERF_CAPTURE_DIR","HELLOMINE3D_RC_PERF_PROFILE","HELLOMINE3D_E2_BATCH_MANIFEST","HELLOMINE3D_REFERENCE_VISUAL_SCENE","HELLOMINE3D_VISUAL_CAMERA_SWEEP","HELLOMINE3D_VISUAL_CAMERA_PATH","HELLOMINE3D_PLAYER_MOTION_CAPTURE","HELLOMINE3D_ACTOR_VISUAL_CAPTURE","HELLOMINE3D_ACTOR_VISUAL_DISTANCE","HELLOMINE3D_BLOCK_FEEDBACK_CAPTURE","HELLOMINE3D_MATERIAL_IDENTITY_CAPTURE_DIR","HELLOMINE3D_CAMERA_DIAGNOSTICS_DIR","HELLOMINE3D_FERN_WIND_CAPTURE_DIR","HELLOMINE3D_SHORE_EDIT_CAPTURE_DIR","HELLOMINE3D_PAUSE_NOTIFICATIONS_DIR","HELLOMINE3D_HUD_PAGE_FIXTURE","HELLOMINE3D_HUD_INSPECT_SLOT","HELLOMINE3D_MACHINE_FIXTURE","HELLOMINE3D_P11_LIGHT_FIXTURE","HELLOMINE3D_VERTEX_LIGHTING_FIXTURE","HELLOMINE3D_CONTROLLED_CRASH","HELLOMINE3D_RESOURCE_PACKS","HELLOMINE3D_EXIT_AFTER_FRAMES"})
            require(value(k).empty(),"cannot combine "+std::string(k));
        const auto out=path("HELLOMINE3D_REFERENCE_EDIT_DIR"),save=path("HELLOMINE3D_SAVE_DIR"),cat=path("HELLOMINE3D_CATALOGUE_DIR"),root=path("HELLOMINE3D_ROOT"),capture=path("HELLO_RENDER_CAPTURE_DIR"),session=out.parent_path();
        require(out==session/"edit" && save==session/"save" && cat==session/"catalogue" && root==session/"Runtime.app/Contents/Resources" && capture==out,"owned sibling layout required");
        require(!std::filesystem::exists(out) && !std::filesystem::exists(cat),"existing edit/catalogue output prohibited");
        const auto markerPath=session/".hellomine3d-reference-world-edit-owned";
        for(const auto& child:{markerPath,save/"world.meta",save/"chunks"})
            require(!std::filesystem::is_symlink(std::filesystem::symlink_status(child)),"owned marker/meta/chunks child symlink prohibited");
        require(std::filesystem::is_regular_file(markerPath) && std::filesystem::is_regular_file(save/"world.meta") && std::filesystem::is_directory(save/"chunks") && std::filesystem::is_directory(root),"real regular owned marker/meta and clone save/runtime required");
        std::ifstream marker(markerPath,std::ios::binary);std::string text((std::istreambuf_iterator<char>(marker)),{});
        require(text=="HelloMine3D owned reference world edit session v1\n","owned edit marker missing");
    }
    static std::string validateEnvironment(bool hdr,bool standard,bool fullscreen,unsigned w,unsigned h,unsigned rd,bool medium) {
        if(!std::getenv("HELLOMINE3D_REFERENCE_EDIT_PROBE"))return {};
        ReferenceEdit::require(hdr && standard && !fullscreen && w==1280 && h==720 && rd==3 && medium,"hidden windowed HDR/standard1280x720 RD3 medium required");
        return ReferenceEdit::value("HELLOMINE3D_REFERENCE_EDIT_DIR");
    }
    ReferenceWorldEditProbe(const std::string& out,Ogre::SceneManager& scene,Ogre::Camera& camera,Ogre::RenderWindow& window)
      :m_output(out),m_scene(scene),m_camera(camera),m_mode(ReferenceEdit::value("HELLOMINE3D_REFERENCE_EDIT_PROBE")),m_fault(!ReferenceEdit::value("HELLOMINE3D_REFERENCE_EDIT_FAULT").empty()) {
        using namespace ReferenceEdit;require(window.getWidth()==2560 && window.getHeight()==1440,"actual hidden native window must be2560x1440");require(!std::filesystem::exists(m_output),"output already exists");std::filesystem::create_directory(m_output);
        m_journal.open(m_output/"journal.jsonl");require(bool(m_journal),"cannot create journal");
        if(m_mode=="lamp")m_edits.push_back({{201,69,-186},ChunkBlock(Block_t(44),0),ChunkBlock(Block_t(0),0)});
        else if(m_mode=="wall")for(int x=199;x<=200;++x)for(int y=68;y<=70;++y)m_edits.push_back({{x,y,-196},ChunkBlock(Block_t(35),0),ChunkBlock(Block_t(0),0)});
        else m_edits.push_back({{194,66,-183},ChunkBlock(Block_t(7),0),ChunkBlock(Block_t(3),0)});
        m_started=m_phaseStarted=Clock::now();m_scene.addRenderObjectListener(this);
        emit("header",0,object({{"schema",quote("reference-world-edit-v1")},{"normal_input","false"},{"input_event_count","0"},{"simulation_delta","0"},{"performance","false"},{"window_mode",quote("hidden")},{"launch_method",quote("direct")},{"save_directory",quote(value("HELLOMINE3D_SAVE_DIR"))},{"fault",quote(m_fault?"skip-reflection-update":"")},{"maximum_frames",number(MaximumFrames)},{"maximum_seconds",number(MaximumSeconds)},{"phase_maximum_seconds",number(PhaseMaximumSeconds)},{"maximum_journal_records",number(MaximumJournalRecords)},{"readback_budget","4"}}));
    }
    ~ReferenceWorldEditProbe() override { cancel();for(auto& b:m_bindings)if(b.first)b.first->setNativeDrawObserver(nullptr);m_scene.removeRenderObjectListener(this);restoreTime(); }
    unsigned phase() const noexcept { return m_phase; }
    bool complete() const noexcept { return m_phase==3; }
    bool matchedCaptured() const noexcept { return m_matched; }
    bool mutationPending() const noexcept { return m_mutationPending; }
    void mutationFacts(const std::string& facts) { emit("mutation",m_frame,facts);m_mutationPending=false; }
    bool frameArmed() const noexcept { return m_armed; }
    void recordSectionReadiness(const std::string& facts) { m_sectionReadiness=facts; }
    const std::string& sectionReadinessFacts() const noexcept { return m_sectionReadiness; }
    bool skipReflection() const noexcept { return m_fault && m_phase==1; }
    bool sectionSelected(glm::ivec3 s) const { for(const auto& e:m_edits)if(s==glm::ivec3(int(std::floor(e.position.x/16.0)),e.position.y/16,int(std::floor(e.position.z/16.0))))return true;return false; }
    void retainUpload(ChunkSectionRenderable& object,const ChunkMesh& mesh,glm::ivec3 section,std::uint32_t revision) {
        if(!sectionSelected(section) || (object.getMaterial()->getName()!="HelloMine3D/Terrain" && object.getMaterial()->getName()!="HelloMine3D/Water"))return;
        ReferenceEdit::require(m_bindings.size()<8,"original resident object limit");
        Binding b; b.object=&object;b.section=section;b.revision=revision;b.serial=++m_uploadSerial;
        b.cpu=packTerrainRenderBatch({{section,&mesh}},section);
        ReferenceEdit::require(b.cpu.vertices.size()*44+b.cpu.indices.size()*4<=16u*1024u*1024u,"CPU upload copy bound");
        m_bindings.emplace(&object,std::move(b));object.setNativeDrawObserver(this);
    }
    void detach(ChunkSectionRenderable& object) { object.setNativeDrawObserver(nullptr);m_pending.erase(&object);m_bindings.erase(&object); }
    void beginFrame(World& world,unsigned frame,unsigned inputs) {
        using namespace ReferenceEdit;require(frame<MaximumFrames && milliseconds(m_started)<MaximumSeconds*1000,"session frame/wall bound");
        require(milliseconds(m_phaseStarted)<PhaseMaximumSeconds*1000,"phase wall bound");require(inputs==0,"ordinary input callback observed");
        cancel();m_frame=frame;m_inputs=inputs;
        if(!m_begun){
            if(m_phase==0){for(const auto& e:m_edits)require(world.getBlock(e.position.x,e.position.y,e.position.z)==e.original,"fixture original block/meta differs");m_originalVerified=true;}
            else {apply(world,m_phase==1);m_mutationPending=true;}
            m_phaseBeginFrame=frame;m_phaseStarted=Clock::now();m_begun=true;
            emit("begin",frame,object({{"blocks",blocks(world)},{"world_visual_revision",number(world.visualRevision())}}));
        }
    }
    // All timers are replaced in production material parameters; private passes
    // deep-copy these frozen inputs. Restore only original auto entries, before
    // Scene/Root teardown; no changes outside this opt-in lifetime.
    void freezeTime() {
        auto iterator=Ogre::MaterialManager::getSingleton().getResourceIterator();
        while(iterator.hasMoreElements()){
            auto* material=static_cast<Ogre::Material*>(iterator.getNext().get());
            for(unsigned short ti=0;ti<material->getNumTechniques();++ti)for(unsigned short pi=0;pi<material->getTechnique(ti)->getNumPasses();++pi){auto* pass=material->getTechnique(ti)->getPass(pi);
                if(pass->hasVertexProgram())freezeParameters(pass->getVertexProgramParameters());
                if(pass->hasFragmentProgram())freezeParameters(pass->getFragmentProgramParameters());
            }
        }
    }
    bool restoredTime() const noexcept { return m_timeRestored; }
    void restoreTime() noexcept {
        if(m_timeRestored)return;m_timeRestored=true;
        try{for(auto& saved:m_times){saved.parameters->setNamedConstant(saved.name,saved.value);saved.parameters->setNamedAutoConstantReal(saved.name,saved.type,saved.factor);}}catch(...){m_timeRestored=false;}
        m_times.clear();
    }
    void arm(const LocalLightSnapshot& lights,bool ready) {
        if(!ready || m_frame-m_phaseBeginFrame<12)return;
        ReferenceEdit::require(!m_bindings.empty(),"no actual resident upload objects");m_armed=true;m_lights=lights;m_draws.clear();m_pending.clear();
        m_label=m_phase==0 && !m_matched?"matched-a0":Phases[m_phase];
    }
    std::string blocks(World& world) const { std::vector<std::string> a;for(const auto& e:m_edits){const auto b=world.getBlock(e.position.x,e.position.y,e.position.z);a.push_back(ReferenceEdit::object({{"position",ReferenceEdit::xyz(e.position)},{"id",ReferenceEdit::number(b.id)},{"metadata",ReferenceEdit::number(b.metadata)}}));}return ReferenceEdit::array(a); }
    void requireExpected(World& world) const { for(const auto& e:m_edits)ReferenceEdit::require(world.getBlock(e.position.x,e.position.y,e.position.z)==(m_phase==1?e.changed:e.original),"phase block/meta differs"); }
    std::string drawFacts() const { return ReferenceEdit::array(m_draws); }
    std::string prefix() const { return (m_output/m_label).string(); }
    std::string mainPng() const { return prefix()+".main.png"; }
    void checkpoint(const std::string& facts) {
        ReferenceEdit::require(m_armed,"checkpoint frame was not armed");
        // Retain partial old RTT facts before rejecting the native fault.
        ReferenceEdit::require(m_readbacks<4,"native readback lifetime bound");++m_readbacks;
        emit(m_label=="matched-a0"?"matched-control":"checkpoint",m_frame,facts);
        std::ofstream file(m_output/(m_label+".json"));file<<facts<<'\n';ReferenceEdit::require(bool(file),"cannot write checkpoint");
        ReferenceEdit::require(m_mainPbr && m_reflectionPbr,"missing actual same-frame main/private PBR native draw");
        if(m_label=="matched-a0")m_matched=true;
        else {emit("end",m_frame,ReferenceEdit::object({{"checkpoint",ReferenceEdit::quote(m_label+".json")}}));++m_phase;m_begun=false;m_phaseStarted=Clock::now();}
        cancel();
    }
    void finish(World& world) {
        using namespace ReferenceEdit;require(complete() && m_originalVerified && m_matched,"three edit phases incomplete");for(const auto& e:m_edits)require(world.getBlock(e.position.x,e.position.y,e.position.z)==e.original,"restored block/meta differs");
        require(world.save(),"normal restored World save failed");m_saved=true;
        emit("save",m_frame,object({{"normal_world_save_returned","true"},{"blocks",blocks(world)}}));
        require(milliseconds(m_started)<MaximumSeconds*1000,"normal save exceeded session wall bound");restoreTime();require(m_timeRestored,"shader time restoration failed");
        writeSummary("COMPLETE","");
    }
    void fail(const std::string& reason,World* world) noexcept {
        try {cancel();if(world && m_originalVerified){apply(*world,false);for(const auto& e:m_edits)ReferenceEdit::require(world->getBlock(e.position.x,e.position.y,e.position.z)==e.original,"failure restoration failed");m_failureRestored=true;}restoreTime();emit("failure",m_frame,ReferenceEdit::object({{"reason",ReferenceEdit::quote(reason)},{"blocks_restored",ReferenceEdit::boolean(m_failureRestored)},{"section_readiness",m_sectionReadiness}}));writeSummary("FAIL",reason);}catch(...){}
    }
    void notifyRenderSingleObject(Ogre::Renderable* renderable,const Ogre::Pass* pass,const Ogre::AutoParamDataSource* source,const Ogre::LightList*,bool suppressed) override {
        if(!m_armed)return;auto* object=dynamic_cast<ChunkSectionRenderable*>(renderable);if(!object || !m_bindings.count(object))return;
        auto& p=m_pending[object];ReferenceEdit::require(!p.query,"nested listener query");p.pass=nullptr;p.camera=source?source->getCurrentCamera():nullptr;
        if(suppressed || !pass || !p.camera)return;
        if(p.camera!=&m_camera && p.camera->getName().find("/PlanarWaterReflection/Camera")==std::string::npos)return;
        p.pass=pass;
    }
    void beforeNativeDraw(ChunkSectionRenderable& object,Ogre::SceneManager* scene,Ogre::RenderSystem* renderer) override {
        if(!m_armed || !m_bindings.count(&object))return;auto& p=m_pending[&object];if(!p.pass)return;
        ReferenceEdit::require(scene==&m_scene && renderer && p.pass->getPassIterationCount()==1 && !p.pass->hasGeometryProgram(),"unsupported native draw scope");
        GLint previous=0;glGetQueryiv(GL_PRIMITIVES_GENERATED,GL_CURRENT_QUERY,&previous);ReferenceEdit::require(previous==0,"existing primitive query left untouched");
        glGenQueries(1,&p.query);ReferenceEdit::require(p.query!=0,"query allocation failed");glBeginQuery(GL_PRIMITIVES_GENERATED,p.query);
    }
    void afterNativeDraw(ChunkSectionRenderable& object,Ogre::SceneManager*,Ogre::RenderSystem*) override {
        if(!m_armed || !m_bindings.count(&object))return;auto& p=m_pending[&object];if(!p.query)return;
        const GLuint query=p.query;glEndQuery(GL_PRIMITIVES_GENERATED);GLuint generated=0;glGetQueryObjectuiv(query,GL_QUERY_RESULT,&generated);glDeleteQueries(1,&query);p.query=0;
        observe(object,p,generated);p.pass=nullptr;
    }
private:
    using Clock=std::chrono::steady_clock;
    struct Pending { const Ogre::Pass* pass=nullptr;const Ogre::Camera* camera=nullptr;GLuint query=0; };
    struct Time { Ogre::GpuProgramParametersSharedPtr parameters;std::string name;Ogre::GpuProgramParameters::AutoConstantType type;float factor,value; };
    std::filesystem::path m_output;Ogre::SceneManager& m_scene;Ogre::Camera& m_camera;
    std::string m_mode,m_label,m_sectionReadiness="null";bool m_saved=false,m_mutationPending=false,m_fault=false,m_begun=false,m_matched=false,m_armed=false,m_originalVerified=false,m_failureRestored=false,m_timeRestored=false;
    bool m_mainPbr=false,m_reflectionPbr=false;
    unsigned m_phase=0,m_frame=0,m_phaseBeginFrame=0,m_sequence=0,m_inputs=0,m_readbacks=0;std::uint64_t m_uploadSerial=0,m_writtenBytes=0;
    Clock::time_point m_started,m_phaseStarted;std::ofstream m_journal;std::vector<Edit> m_edits;
    std::map<ChunkSectionRenderable*,Binding> m_bindings;std::map<ChunkSectionRenderable*,Pending> m_pending;
    std::vector<Time> m_times;std::vector<std::string> m_draws;LocalLightSnapshot m_lights;
    static double milliseconds(Clock::time_point start) { return std::chrono::duration<double,std::milli>(Clock::now()-start).count(); }
    void apply(World& world,bool edited) { for(const auto& e:m_edits)world.setBlock(e.position.x,e.position.y,e.position.z,edited?e.changed:e.original); }
    void emit(const char* event,unsigned frame,const std::string& facts) {
        using namespace ReferenceEdit;require(m_sequence<MaximumJournalRecords,"journal bound");
        m_journal<<object({{"seq",number(++m_sequence)},{"event",quote(event)},{"mode",quote(m_mode)},{"phase",quote(m_phase<3?Phases[m_phase]:"complete")},{"frame",number(frame)},{"elapsed_ms",number(milliseconds(m_started))},{"facts",facts}})<<'\n'<<std::flush;require(bool(m_journal),"journal write failed");
    }
    void writeSummary(const char* status,const std::string& reason) {
        using namespace ReferenceEdit;std::ofstream o(m_output/"summary.json");o<<object({{"schema",quote("reference-world-edit-v1")},{"status",quote(status)},{"mode",quote(m_mode)},{"completed_phases",number(m_phase)},{"matched_control_captured",boolean(m_matched)},{"readbacks",number(m_readbacks)},{"normal_world_save_returned",boolean(m_saved)},{"normal_input","false"},{"input_event_count",number(m_inputs)},{"simulation_delta","0"},{"animation_time","4"},{"time_parameters_restored",boolean(m_timeRestored)},{"failure_blocks_restored",boolean(m_failureRestored)},{"fault",quote(m_fault?"skip-reflection-update":"")},{"frames",number(m_frame)},{"elapsed_ms",number(milliseconds(m_started))},{"reason",quote(reason)}})<<'\n';require(bool(o),"summary write failed");
    }
    void freezeParameters(const Ogre::GpuProgramParametersSharedPtr& parameters) {
        for(const char* name:{"globalTime","legacyTime"}){const auto* definition=parameters->_findNamedConstantDefinition(name,false);if(!definition)continue;
            const auto* automatic=parameters->findAutoConstantEntry(name);if(!automatic)continue;
            ReferenceEdit::require(automatic->paramType==Ogre::GpuProgramParameters::ACT_TIME || automatic->paramType==Ogre::GpuProgramParameters::ACT_TIME_0_X,"unknown time auto parameter");
            ReferenceEdit::require(m_times.size()<128,"time parameter ownership bound");
            float old=0;parameters->_readRawConstants(definition->physicalIndex,1,&old);
            m_times.push_back({parameters,name,automatic->paramType,automatic->fData,old});
            parameters->clearNamedAutoConstant(name);parameters->setNamedConstant(name,4.f);
        }
    }
    void cancel() noexcept {
        for(auto& entry:m_pending)if(entry.second.query){GLint active=0;glGetQueryiv(GL_PRIMITIVES_GENERATED,GL_CURRENT_QUERY,&active);if(GLuint(active)==entry.second.query)glEndQuery(GL_PRIMITIVES_GENERATED);glDeleteQueries(1,&entry.second.query);entry.second.query=0;}
        m_pending.clear();m_armed=false;m_mainPbr=m_reflectionPbr=false;
    }
    void bytes(const std::string& file,const void* data,std::size_t size) {
        ReferenceEdit::require(size<=16u*1024u*1024u && m_writtenBytes+size<=256u*1024u*1024u,"byte evidence budget");m_writtenBytes+=size;
        const auto path=m_output/file;ReferenceEdit::require(!std::filesystem::exists(path),"buffer evidence overwrite");std::ofstream out(path,std::ios::binary);out.write(static_cast<const char*>(data),std::streamsize(size));ReferenceEdit::require(bool(out),"byte evidence write failed");
    }
    void observe(ChunkSectionRenderable& object,const Pending& p,GLuint generated) {
        using namespace ReferenceEdit;require(m_draws.size()<32,"native operation count bound");
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
            attributes.push_back(ReferenceEdit::object({{"name",quote(names[slot])},{"location",number(location)},
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
        const auto stem=m_label+"-draw"+std::to_string(m_draws.size());bytes(stem+".vbo.bin",v.data(),vb);bytes(stem+".ibo.bin",i.data(),ib);bytes(stem+".cpu-vbo.bin",b.cpu.vertices.data(),vb);bytes(stem+".cpu-ibo.bin",b.cpu.indices.data(),ib);
        const bool main=p.camera==&m_camera;const GLint lightLocation=glGetUniformLocation(GLuint(program),"localLightCount");std::string actualLights="null";
        if(lightLocation>=0){GLint actual=0;glGetUniformiv(GLuint(program),lightLocation,&actual);require(actual==GLint(m_lights.count),"actual linked light count differs from World snapshot");std::vector<std::string> positions,colours;
            for(unsigned slot=0;slot<8;++slot){float a[4]{},c[4]{};const auto n=std::to_string(slot);const GLint al=glGetUniformLocation(GLuint(program),("localLightPositionRadius["+n+"]").c_str()),cl=glGetUniformLocation(GLuint(program),("localLightColourEnergy["+n+"]").c_str());require(al>=0 && cl>=0,"actual light array interface absent");glGetUniformfv(GLuint(program),al,a);glGetUniformfv(GLuint(program),cl,c);
                const LocalLightSource empty;const auto& s=slot<m_lights.count?m_lights.sources[slot]:empty;float expectedA[4]{},expectedC[4]{};if(slot<m_lights.count){expectedA[0]=s.position.x;expectedA[1]=s.position.y;expectedA[2]=s.position.z;expectedA[3]=s.radius;expectedC[0]=s.colour.x;expectedC[1]=s.colour.y;expectedC[2]=s.colour.z;expectedC[3]=s.energy;}require(std::memcmp(a,expectedA,sizeof a)==0 && std::memcmp(c,expectedC,sizeof c)==0,"actual linked light arrays differ from same-frame World snapshot");
                positions.push_back(array({number(a[0]),number(a[1]),number(a[2]),number(a[3])}));colours.push_back(array({number(c[0]),number(c[1]),number(c[2]),number(c[3])}));}
            actualLights=objectJsonLights(actual,positions,colours);if(main)m_mainPbr=true;else m_reflectionPbr=true;
        }
        std::vector<std::string> times;for(const char* name:{"globalTime","legacyTime"}){const GLint location=glGetUniformLocation(GLuint(program),name);if(location>=0){float time=0;glGetUniformfv(GLuint(program),location,&time);require(time==4.f,"actual shader time not frozen");times.push_back(ReferenceEdit::object({{"name",quote(name)},{"value",number(time)}}));}}
        const auto error=glGetError();require(error==GL_NO_ERROR,"native observation GL error");
        m_draws.push_back(ReferenceEdit::object({{"view",quote(main?"main":"reflection")},{"object_name",quote(object.getName())},{"section",xyz(b.section)},{"uploaded_revision",number(b.revision)},{"upload_serial",number(b.serial)},{"frame",number(m_frame)},{"camera",camera(*p.camera)},
            {"material",quote(p.pass->getParent()->getParent()->getName())},{"vertex_program",quote(p.pass->getVertexProgramName())},{"fragment_program",quote(p.pass->getFragmentProgramName())},{"program",number(program)},{"program_linked","true"},{"production_attached_shaders","true"},{"vao",number(vao)},{"vbo",number(vertex->getGLBufferId())},{"ibo",number(index->getGLBufferId())},{"gl_is_buffer","true"},{"attributes",array(attributes)},{"vertex_bytes",number(vb)},{"index_bytes",number(ib)},{"primitive_count",number(generated)},{"expected_triangles",number(b.cpu.indices.size()/3)},{"cpu_gpu_bytes_equal",boolean(equal)},
            {"vbo_file",quote(stem+".vbo.bin")},{"ibo_file",quote(stem+".ibo.bin")},{"cpu_vbo_file",quote(stem+".cpu-vbo.bin")},{"cpu_ibo_file",quote(stem+".cpu-ibo.bin")},{"local_lights",actualLights},{"time_uniforms",array(times)},{"gl_error",number(error)},{"state_restored","true"}}));
    }
    static std::string objectJsonLights(int count,const std::vector<std::string>& p,const std::vector<std::string>& c) { return ReferenceEdit::object({{"domain",ReferenceEdit::quote("actual-linked-GL-program-after-production-draw")},{"count",ReferenceEdit::number(count)},{"position_radius",ReferenceEdit::array(p)},{"colour_energy",ReferenceEdit::array(c)}}); }
};
