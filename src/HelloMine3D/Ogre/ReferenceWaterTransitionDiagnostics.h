#pragma once
// Default-off owned engineering observation. Production Player/logic-camera,
// World medium, reflection component, pass binding and native draw remain used.
#include "ReferenceWorldEditDiagnostics.h"
#include "PlanarWaterReflection.h"
#include "../Player/Player.h"
#include <set>
#include <Ogre.h>
#if defined(_WIN32)
extern char** _environ;
#else
extern char** environ;
#endif

class ReferenceWaterTransitionProbe final : public ChunkSectionRenderable::NativeDrawObserver,
                                             public Ogre::RenderObjectListener {
public:
    static constexpr const char* Entry="HELLOMINE3D_REFERENCE_WATER_PROBE";
    static constexpr const char* Fault="HELLOMINE3D_REFERENCE_WATER_FAULT";
    static constexpr unsigned MaximumFrames=2048, MaximumSeconds=30, PhaseFrames=512, PhaseSeconds=8;
    inline static constexpr std::array<const char*,4> Phases{{"above-a","collar","below","above-return"}};
    static void validateEntrypoint(bool validateOnly) {
        using namespace ReferenceEdit;
        if(!std::getenv(Entry) && !std::getenv(Fault) && !std::getenv("HELLOMINE3D_REFERENCE_WATER_DIR"))return;
        require(value(Entry)=="1","Water transition exact opt-in1 required");
        require(!validateOnly,"Water transition cannot combine validate-only");
        require(value(Fault).empty() || value(Fault)=="retain-water-binding","Water transition unknown fault");
        const std::set<std::string> allowed{Entry,Fault,"HELLOMINE3D_REFERENCE_WATER_DIR","HELLOMINE3D_ROOT","HELLOMINE3D_SAVE_DIR","HELLOMINE3D_CATALOGUE_DIR","HELLOMINE3D_WINDOW_HIDDEN","HELLO_RENDER_CAPTURE","HELLO_RENDER_CAPTURE_DIR","HELLO_RENDER_CAPTURE_MS","HELLO_RENDER_CAPTURE_MAX_DELTA_MS","HELLO_RENDER_CAPTURE_EXIT","HELLOMINE3D_MSAA4","HELLOMINE3D_SEED","HELLOMINE3D_WORLD_TIME","HELLOMINE3D_PLAYER_POSITION","HELLOMINE3D_PLAYER_ROTATION"};
#if defined(_WIN32)
        auto vars=_environ;
#else
        auto vars=environ;
#endif
        for(auto it=vars;it && *it;++it){const std::string line(*it),key=line.substr(0,line.find('='));bool family=false;
            for(const char* prefix:{"HELLOMINE3D_","HELLO_RENDER_","HELLO_PERF_","HELLO_VISUAL_"})family=family || key.compare(0,std::strlen(prefix),prefix)==0;
            require(!family || allowed.count(key),"Water transition extra diagnostic environment "+key);}
        require(value("HELLOMINE3D_WINDOW_HIDDEN")=="1" && value("HELLO_RENDER_CAPTURE")=="1" && value("HELLO_RENDER_CAPTURE_MS")=="60000" && value("HELLO_RENDER_CAPTURE_EXIT")=="0" && value("HELLO_RENDER_CAPTURE_MAX_DELTA_MS")=="5000","Water transition hidden primary capture outside deadline required");
        require(value("HELLOMINE3D_MSAA4")=="1" && value("HELLOMINE3D_SEED")=="42" && value("HELLOMINE3D_WORLD_TIME")=="6000" && value("HELLOMINE3D_PLAYER_POSITION")=="195.5 68.02 -176.2" && value("HELLOMINE3D_PLAYER_ROTATION")=="5 45 0","Water transition fixed initial fixture required");
        const auto out=path("HELLOMINE3D_REFERENCE_WATER_DIR"),session=out.parent_path(),save=path("HELLOMINE3D_SAVE_DIR"),cat=path("HELLOMINE3D_CATALOGUE_DIR"),root=path("HELLOMINE3D_ROOT"),capture=path("HELLO_RENDER_CAPTURE_DIR");
        require(out==session/"water" && save==session/"save" && cat==session/"catalogue" && root==session/"Runtime.app/Contents/Resources" && capture==out,"Water transition owned layout required");
        require(!std::filesystem::exists(out) && !std::filesystem::exists(cat),"Water transition output/catalogue must be new");
        const auto marker=session/".hellomine3d-reference-water-transition-owned";
        for(const auto& p:{marker,save/"world.meta",save/"chunks"})require(!std::filesystem::is_symlink(std::filesystem::symlink_status(p)),"Water transition child symlink prohibited");
        require(std::filesystem::is_regular_file(marker) && std::filesystem::is_regular_file(save/"world.meta") && std::filesystem::is_directory(save/"chunks") && std::filesystem::is_directory(root),"Water transition actual owned input required");
        std::ifstream in(marker,std::ios::binary);require(std::string((std::istreambuf_iterator<char>(in)),{})=="HelloMine3D owned reference water transition session v1\n","Water transition marker invalid");
    }
    static std::string validateConfig(const Config& c) {using namespace ReferenceEdit;if(!std::getenv(Entry))return {};
        require(!c.isFullscreen && c.windowX==1280 && c.windowY==720 && c.renderDistance==3 && c.visualDetail==VisualDetail::Standard && c.renderPipeline==RenderPipeline::LinearHdr && c.directionalShadowQuality==DirectionalShadowQuality::Medium && c.postProcessingQuality==PostProcessingQuality::Off && c.cameraPerspective==CameraPerspective::FirstPerson,"Water transition actual config1280x720/RD3/standard/HDR4/medium/off/first required");return path("HELLOMINE3D_REFERENCE_WATER_DIR").string();}
    ReferenceWaterTransitionProbe(const std::string& output,World& world,Ogre::SceneManager& scene,Ogre::Camera& camera):m_output(output),m_scene(scene),m_camera(camera),m_started(Clock::now()),m_phaseStarted(m_started),m_fault(ReferenceEdit::value(Fault)=="retain-water-binding") {
        using namespace ReferenceEdit;require(!std::filesystem::exists(m_output),"Water transition output exists");
        // The template column is finite/resident/non-Air. No edits or load query.
        for(int y=64;y<=66;++y)require(world.getBlock(194,y,-183)==ChunkBlock(Block_t(7),0),"Water transition actual water column/meta differs");
        require(world.getBlock(194,63,-183).id!=0,"Water transition resident bed missing");
        std::filesystem::create_directory(m_output);m_journal.open(m_output/"journal.jsonl");require(bool(m_journal),"Water transition journal open failed");
        m_scene.addRenderObjectListener(this);m_listener=true;
    }
    ~ReferenceWaterTransitionProbe() override {detach();}
    bool complete()const noexcept{return m_phase==4;}
    bool faultBinding()const noexcept{return m_fault && m_phase==1;}
    unsigned phase()const noexcept{return m_phase;}
    unsigned frame()const noexcept{return m_frame;}
    unsigned warmFrames()const noexcept{return m_warm;}
    unsigned teleportCount()const noexcept{return m_teleports;}
    float delta()const noexcept{return m_delta;}
    const glm::vec3& requested()const noexcept{return m_requested;}
    std::string label()const{return Phases.at(m_phase);}
    void input()noexcept{++m_inputs;}
    void detach()noexcept{cancel();if(m_listener){m_scene.removeRenderObjectListener(this);m_listener=false;}}
    void drive(unsigned frame,float delta,const Player& player,const std::function<bool(const glm::vec3&)>& teleport) {
        using namespace ReferenceEdit;cancel();m_draw.clear();m_armed=false;m_frame=frame;m_delta=delta;
        require(!complete(),"Water transition extra frame after completion");require(m_inputs==0,"Water transition ordinary input observed");
        require(frame<MaximumFrames && elapsed(m_started)<MaximumSeconds*1000.0 && frame-m_phaseBegin<PhaseFrames && elapsed(m_phaseStarted)<PhaseSeconds*1000.0,"Water transition frame/wall bound");
        if(!m_anchor){m_origin=player.position;m_rotation=player.rotation;m_anchor=true;}
        const float offsets[4]{1.20f,.14f,-.35f,1.20f};
        // Core Camera follows Player at +0.6m. Actual eye/logic pose are later
        // observed, never inferred as successful from this request.
        m_requested={194.5f,66.9f+offsets[m_phase]-.6f,-182.5f};
        require(teleport(m_requested),"Water transition production teleport rejected");++m_teleports;
        if(!m_begun){emit("begin",object({{"requested_player",xyz(m_requested)},{"production_teleport",boolean(true)}}));m_begun=true;}
    }
    void arm(bool actualPhase) {if(actualPhase && std::isfinite(m_delta) && m_delta>0)++m_warm;else m_warm=0;m_armed=m_warm>=12;}
    void notifyRenderSingleObject(Ogre::Renderable* renderable,const Ogre::Pass* pass,const Ogre::AutoParamDataSource* source,const Ogre::LightList*,bool suppressed) override {
        if(!m_armed || !m_draw.empty() || suppressed || !pass || !source || source->getCurrentCamera()!=&m_camera || pass->getParent()->getParent()->getName()!="HelloMine3D/Water")return;
        auto* water=dynamic_cast<ChunkSectionRenderable*>(renderable);if(!water)return;
        ReferenceEdit::require(!m_pending.object,"Water transition nested native observation");m_pending={water,pass,0};water->setNativeDrawObserver(this);
    }
    void beforeNativeDraw(ChunkSectionRenderable& water,Ogre::SceneManager* scene,Ogre::RenderSystem*) override {
        if(m_pending.object!=&water)return;using namespace ReferenceEdit;require(scene==&m_scene && m_pending.pass->getPassIterationCount()==1 && !m_pending.pass->hasGeometryProgram(),"Water transition actual draw scope");GLint current=0;glGetQueryiv(GL_PRIMITIVES_GENERATED,GL_CURRENT_QUERY,&current);require(!current,"Water transition existing native query");glGenQueries(1,&m_pending.query);require(m_pending.query!=0,"Water transition native query unavailable");glBeginQuery(GL_PRIMITIVES_GENERATED,m_pending.query);
    }
    void afterNativeDraw(ChunkSectionRenderable& water,Ogre::SceneManager*,Ogre::RenderSystem*) override {
        if(m_pending.object!=&water || !m_pending.query)return;
        glEndQuery(GL_PRIMITIVES_GENERATED);GLuint count=0;glGetQueryObjectuiv(m_pending.query,GL_QUERY_RESULT,&count);glDeleteQueries(1,&m_pending.query);m_pending.query=0;
        m_draw=draw(water,*m_pending.pass,count);water.setNativeDrawObserver(nullptr);m_pending={};
    }
    bool ready()const noexcept{return m_armed && !m_draw.empty();}
    std::uint64_t updateFloor()const noexcept{return m_updateFloor;}
    float linkedEnabled()const noexcept{return m_linkedEnabled;}
    unsigned linkedTexture()const noexcept{return m_linkedTexture;}
    bool linkedStorage(unsigned width,unsigned height)const noexcept{return m_linkedWidth==int(width) && m_linkedHeight==int(height) && m_linkedFormat==GL_RGBA16F;}
    std::string drawFacts()const{return m_draw.empty()?"null":m_draw;}
    void observed(const std::string& facts){emit("observation",facts);}
    void checkpoint(const std::string& facts,std::uint64_t updateCount) {using namespace ReferenceEdit;require(ready(),"Water transition no actual native Water draw");emit("checkpoint",facts);emit("end","{}");m_updateFloor=updateCount;++m_phase;m_phaseBegin=m_frame+1;m_phaseStarted=Clock::now();m_warm=0;m_begun=false;m_armed=false;m_draw.clear();if(complete())detach();}
    const glm::vec3& origin()const noexcept{return m_origin;}
    const glm::vec3& rotation()const noexcept{return m_rotation;}
    std::string prefix()const{return (m_output/label()).string();}
    std::string mainPng()const{return prefix()+".main.png";}
    void restored(const Player& player){using namespace ReferenceEdit;require(glm::length(player.position-m_origin)==0.f && glm::length(player.rotation-m_rotation)==0.f,"Water transition actual original pose restore mismatch");m_restoration=object({{"original_player",xyz(m_origin)},{"original_rotation",xyz(m_rotation)},{"actual_player",xyz(player.position)},{"actual_rotation",xyz(player.rotation)},{"production_teleport","true"}});}
    void finish(bool saved){using namespace ReferenceEdit;require(complete() && saved && !m_restoration.empty(),"Water transition incomplete phases/save");inventory();writeSummary("COMPLETE","",saved);}
    void fail(const std::string& reason)noexcept{try{detach();writeSummary("FAILED",reason,false);}catch(...){}}
    void inventory()const {using namespace ReferenceEdit;std::uintmax_t bytes=0;unsigned files=0;for(const auto& e:std::filesystem::directory_iterator(m_output)){require(!e.is_symlink() && e.is_regular_file(),"Water transition unexpected output kind");bytes+=e.file_size();++files;}require(files<=32 && bytes<=256u*1024u*1024u,"Water transition file/byte bound");}
private:
    using Clock=std::chrono::steady_clock;
    struct Pending {ChunkSectionRenderable* object=nullptr;const Ogre::Pass* pass=nullptr;GLuint query=0;};
    std::filesystem::path m_output;Ogre::SceneManager& m_scene;Ogre::Camera& m_camera;
    Clock::time_point m_started,m_phaseStarted;std::ofstream m_journal;Pending m_pending;
    unsigned m_phase=0,m_frame=0,m_phaseBegin=0,m_warm=0,m_inputs=0,m_sequence=0,m_teleports=0;float m_delta=0;
    bool m_listener=false,m_fault=false,m_anchor=false,m_begun=false,m_armed=false;std::uint64_t m_updateFloor=0;
    glm::vec3 m_requested{0},m_origin{0},m_rotation{0};std::string m_draw,m_restoration;
    float m_linkedEnabled=0;unsigned m_linkedTexture=0;int m_linkedWidth=0,m_linkedHeight=0,m_linkedFormat=0;
    static double elapsed(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
    void emit(const char* event,const std::string& facts){using namespace ReferenceEdit;require(m_sequence<32,"Water transition journal bound");m_journal<<object({{"schema",quote("hellomine3d-reference-water-transition-journal-v1")},{"sequence",number(++m_sequence)},{"event",quote(event)},{"phase",quote(Phases.at(m_phase))},{"frame",number(m_frame)},{"elapsed_ms",number(elapsed(m_started))},{"normal_input","false"},{"input_event_count",number(m_inputs)},{"facts",facts}})<<'\n';m_journal.flush();require(bool(m_journal),"Water transition journal write failed");}
    void writeSummary(const char* status,const std::string& reason,bool saved){using namespace ReferenceEdit;std::ofstream out(m_output/"summary.json");out<<object({{"schema",quote("hellomine3d-reference-water-transition-summary-v1")},{"status",quote(status)},{"reason",quote(reason)},{"completed_phases",number(m_phase)},{"frame",number(m_frame)},{"elapsed_ms",number(elapsed(m_started))},{"records",number(m_sequence)},{"input_event_count",number(m_inputs)},{"normal_input","false"},{"normal_save",boolean(saved)},{"restoration",m_restoration.empty()?"null":m_restoration},{"native_fault",quote(m_fault?"retain-water-binding":"")},{"normal_simulation","true"},{"other_resident_plane_transition",quote("NOT_RUN_NO_DECLARED_SECOND_RESIDENT_LEVEL")}})<<'\n';require(bool(out),"Water transition summary write failed");}
    void cancel()noexcept{if(m_pending.query){GLint active=0;glGetQueryiv(GL_PRIMITIVES_GENERATED,GL_CURRENT_QUERY,&active);if(GLuint(active)==m_pending.query)glEndQuery(GL_PRIMITIVES_GENERATED);glDeleteQueries(1,&m_pending.query);}if(m_pending.object)m_pending.object->setNativeDrawObserver(nullptr);m_pending={};}
    std::string draw(ChunkSectionRenderable& water,const Ogre::Pass& pass,GLuint count){using namespace ReferenceEdit;
        GLint program=0,pipeline=0,active=0;glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_PROGRAM_PIPELINE_BINDING,&pipeline);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
        require(program>0 && !pipeline && glIsProgram(GLuint(program)) && count>0,"Water transition actual native program/primitive missing");GLint linked=0;glGetProgramiv(GLuint(program),GL_LINK_STATUS,&linked);require(linked==GL_TRUE,"Water transition actual native link failed");
        const auto* vs=dynamic_cast<const Ogre::GLSLShader*>(pass.getVertexProgram().get());const auto* fs=dynamic_cast<const Ogre::GLSLShader*>(pass.getFragmentProgram().get());GLuint shaders[2]{};GLsizei n=0;glGetAttachedShaders(GLuint(program),2,&n,shaders);require(vs && fs && n==2 && ((shaders[0]==vs->getGLShaderHandle() && shaders[1]==fs->getGLShaderHandle()) || (shaders[1]==vs->getGLShaderHandle() && shaders[0]==fs->getGLShaderHandle())),"Water transition actual attached stage mismatch");
        Fields uniforms;for(const char* name:{"linearHdrMode","planarReflectionEnabled","planarReflectionPlaneY","globalTime","fogDensity","waterDetailStrength"}){const auto loc=glGetUniformLocation(GLuint(program),name);require(loc>=0,"Water transition actual uniform missing "+std::string(name));float v=0;glGetUniformfv(GLuint(program),loc,&v);uniforms.push_back({name,number(v)});}
        float enabled=0;glGetUniformfv(GLuint(program),glGetUniformLocation(GLuint(program),"planarReflectionEnabled"),&enabled);
        const auto loc=glGetUniformLocation(GLuint(program),"planarReflectionTexture");require(loc>=0,"Water transition actual sampler missing");GLint unit=0;glGetUniformiv(GLuint(program),loc,&unit);require(unit>=0 && unit<32,"Water transition sampler unit bound");
        struct ActiveGuard{GLint value;~ActiveGuard(){glActiveTexture(GLenum(value));}}guard{active};glActiveTexture(GL_TEXTURE0+GLenum(unit));GLint texture=0,w=0,h=0,format=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);if(enabled>.5f){require(texture>0 && glIsTexture(GLuint(texture)),"Water transition enabled sampler invalid");glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&w);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&h);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);}
        glActiveTexture(GLenum(active));GLint restored=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&restored);GLboolean depthWrite=GL_TRUE;glGetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);GLint src=0,dst=0;glGetIntegerv(GL_BLEND_SRC_RGB,&src);glGetIntegerv(GL_BLEND_DST_RGB,&dst);const bool blend=glIsEnabled(GL_BLEND)==GL_TRUE;const auto error=glGetError();require(error==GL_NO_ERROR && restored==active,"Water transition GL query state/error");
        m_linkedEnabled=enabled;m_linkedTexture=unsigned(texture);m_linkedWidth=w;m_linkedHeight=h;m_linkedFormat=format;
        require(blend && src==GL_SRC_ALPHA && dst==GL_ONE_MINUS_SRC_ALPHA && depthWrite==GL_FALSE,"Water transition production transparent draw state mismatch");
        Ogre::RenderOperation op;water.getRenderOperation(op);require(op.srcRenderable==&water && op.useIndexes && op.numberOfInstances==1 && op.operationType==Ogre::RenderOperation::OT_TRIANGLE_LIST && op.indexData && count==op.indexData->indexCount/3,"Water transition actual primitive/source mismatch");
        return object({{"observation_domain",quote("actual-driver-after-native-Water-draw")},{"frame",number(m_frame)},{"program",number(program)},{"program_linked","true"},{"attached_production_stages","true"},{"primitive_count",number(count)},{"object",quote(water.getName())},{"vertex_program",quote(pass.getVertexProgramName())},{"fragment_program",quote(pass.getFragmentProgramName())},{"uniforms",object(uniforms)},{"sampler",object({{"location",number(loc)},{"unit",number(unit)},{"sampling_enabled",boolean(enabled>.5f)},{"texture",number(texture)},{"width",number(w)},{"height",number(h)},{"format",number(format)}})},{"blend",object({{"enabled",boolean(blend)},{"src_rgb",number(src)},{"dst_rgb",number(dst)},{"depth_write",boolean(depthWrite==GL_TRUE)}})},{"gl_error",number(error)},{"state_restored","true"}});
    }
};
