#pragma once
// Default-off owned engineering observation. Production Player/logic-camera,
// World medium, reflection component, pass binding and native draw remain used.
#include "ReferenceWorldEditDiagnostics.h"
#include "PlanarWaterReflection.h"
#include "../Player/Player.h"
#include <set>
#include <Ogre.h>
#include <OgreGL3PlusTextureManager.h>
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
    static constexpr const char* Scenario="HELLOMINE3D_REFERENCE_WATER_SCENARIO";
    static constexpr unsigned MaximumFrames=2048, MaximumSeconds=30, PhaseFrames=512, PhaseSeconds=8;
    inline static constexpr std::array<const char*,4> Phases{{"above-a","collar","below","above-return"}};
    inline static constexpr std::array<const char*,4> PlanePhases{{"high-a","low-b","high-return","low-return"}};
    static void validateEntrypoint(bool validateOnly) {
        using namespace ReferenceEdit;
        if(!std::getenv(Entry) && !std::getenv(Fault) && !std::getenv("HELLOMINE3D_REFERENCE_WATER_DIR") && value(Scenario).empty())return;
        require(value(Entry)=="1","Water transition exact opt-in1 required");
        require(!validateOnly,"Water transition cannot combine validate-only");
        const auto scenario=value(Scenario),fault=value(Fault);
        require(scenario.empty() || scenario=="plane-switch-v1","Water transition unknown scenario");
        require(fault.empty() || fault=="retain-water-binding" || fault=="retain-selected-plane","Water transition unknown fault");
        require(fault.empty() || (scenario.empty()?fault=="retain-water-binding":fault=="retain-selected-plane"),"Water transition fault/scenario mismatch");
        const std::set<std::string> allowed{Entry,Fault,Scenario,"HELLOMINE3D_REFERENCE_WATER_DIR","HELLOMINE3D_ROOT","HELLOMINE3D_SAVE_DIR","HELLOMINE3D_CATALOGUE_DIR","HELLOMINE3D_WINDOW_HIDDEN","HELLO_RENDER_CAPTURE","HELLO_RENDER_CAPTURE_DIR","HELLO_RENDER_CAPTURE_MS","HELLO_RENDER_CAPTURE_MAX_DELTA_MS","HELLO_RENDER_CAPTURE_EXIT","HELLOMINE3D_MSAA4","HELLOMINE3D_SEED","HELLOMINE3D_WORLD_TIME","HELLOMINE3D_PLAYER_POSITION","HELLOMINE3D_PLAYER_ROTATION"};
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
    ReferenceWaterTransitionProbe(const std::string& output,World& world,Ogre::SceneManager& scene,Ogre::Camera& camera):m_output(output),m_scene(scene),m_camera(camera),m_started(Clock::now()),m_phaseStarted(m_started),m_fault(ReferenceEdit::value(Fault)=="retain-water-binding"),m_planeSwitch(ReferenceEdit::value(Scenario)=="plane-switch-v1"),m_selectedPlaneFault(ReferenceEdit::value(Fault)=="retain-selected-plane") {
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
    bool planeSwitch()const noexcept{return m_planeSwitch;}
    bool faultSelectedPlane()const noexcept{return m_selectedPlaneFault && m_phase==1;}
    float expectedPlane()const noexcept{return m_planeSwitch && (m_phase==1 || m_phase==3)?64.9f:66.9f;}
    unsigned phase()const noexcept{return m_phase;}
    unsigned frame()const noexcept{return m_frame;}
    unsigned warmFrames()const noexcept{return m_warm;}
    unsigned teleportCount()const noexcept{return m_teleports;}
    float delta()const noexcept{return m_delta;}
    const glm::vec3& requested()const noexcept{return m_requested;}
    std::string label()const{return m_planeSwitch?PlanePhases.at(m_phase):Phases.at(m_phase);}
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
        m_requested=m_planeSwitch?((m_phase==1 || m_phase==3)?glm::vec3(176.5f,65.5f,-175.5f):glm::vec3(194.5f,67.5f,-182.5f)):glm::vec3(194.5f,66.9f+offsets[m_phase]-.6f,-182.5f);
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
    float linkedPlane()const noexcept{return m_linkedPlane;}
    bool linkedMatrixMatchesPass(const Ogre::Pass& pass)const {
        using namespace ReferenceEdit;require(m_planeSwitch,"Water plane matrix check requires scenario");
        const auto parameters=pass.getFragmentProgramParameters();
        const auto* definition=parameters->_findNamedConstantDefinition("planarReflectionViewProj",false);
        const auto* vs=dynamic_cast<const Ogre::GLSLShader*>(pass.getVertexProgram().get());
        require(definition && definition->constType==Ogre::GCT_MATRIX_4X4 && definition->elementSize==16 && definition->arraySize==1 && vs,"Water plane actual pass matrix missing");
        float raw[16]{};parameters->_readRawConstants(definition->physicalIndex,16,raw);
        // Vendored monolithic fragment uploads choose GL transpose from the
        // vertex stage flag; driver uniform reads always return column-major.
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){
            const float expected=vs->getColumnMajorMatrices()?raw[r*4+c]:raw[c*4+r];
            if(!std::isfinite(expected) || !std::isfinite(m_linkedMatrix[c*4+r]) || std::abs(expected-m_linkedMatrix[c*4+r])>5.e-5f)return false;
        }return true;
    }
    bool linkedStorage(unsigned width,unsigned height)const noexcept{return m_linkedWidth==int(width) && m_linkedHeight==int(height) && m_linkedFormat==GL_RGBA16F;}
    std::string drawFacts()const{return m_draw.empty()?"null":m_draw;}
    void observed(const std::string& facts){emit("observation",facts);}
    void checkpoint(const std::string& facts,std::uint64_t updateCount) {using namespace ReferenceEdit;require(ready(),"Water transition no actual native Water draw");
        // Caller emitted observation first. Preserve malformed native binding
        // facts before rejecting the newly required complete inactive sampler.
        if(!m_planeSwitch && (m_phase==1 || m_phase==2))require(m_linkedEnabled==0.f && m_completeFallback,"Water transition inactive complete sampler mismatch");
        emit("checkpoint",facts);emit("end","{}");m_updateFloor=updateCount;++m_phase;m_phaseBegin=m_frame+1;m_phaseStarted=Clock::now();m_warm=0;m_begun=false;m_armed=false;m_draw.clear();if(complete())detach();}
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
    bool m_completeFallback=false,m_listener=false,m_fault=false,m_planeSwitch=false,m_selectedPlaneFault=false,m_anchor=false,m_begun=false,m_armed=false;std::uint64_t m_updateFloor=0;
    glm::vec3 m_requested{0},m_origin{0},m_rotation{0};std::string m_draw,m_restoration;
    float m_linkedEnabled=0,m_linkedPlane=0;std::array<float,16> m_linkedMatrix{};unsigned m_linkedTexture=0;int m_linkedWidth=0,m_linkedHeight=0,m_linkedFormat=0;
    static double elapsed(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
    void emit(const char* event,const std::string& facts){using namespace ReferenceEdit;require(m_sequence<32,"Water transition journal bound");
        Fields fields{{"schema",quote(m_planeSwitch?"hellomine3d-reference-water-plane-selection-journal-v1":"hellomine3d-reference-water-transition-journal-v1")},{"sequence",number(++m_sequence)},{"event",quote(event)},{"phase",quote(label())},{"frame",number(m_frame)},{"elapsed_ms",number(elapsed(m_started))},{"normal_input","false"},{"input_event_count",number(m_inputs)},{"facts",facts}};
        if(m_planeSwitch)fields.push_back({"scenario",quote("plane-switch-v1")});
        m_journal<<object(fields)<<'\n';m_journal.flush();require(bool(m_journal),"Water transition journal write failed");}
    void writeSummary(const char* status,const std::string& reason,bool saved){using namespace ReferenceEdit;std::ofstream out(m_output/"summary.json");
        Fields fields{{"schema",quote(m_planeSwitch?"hellomine3d-reference-water-plane-selection-summary-v1":"hellomine3d-reference-water-transition-summary-v1")},{"status",quote(status)},{"reason",quote(reason)},{"completed_phases",number(m_phase)},{"frame",number(m_frame)},{"elapsed_ms",number(elapsed(m_started))},{"records",number(m_sequence)},{"input_event_count",number(m_inputs)},{"normal_input","false"},{"normal_save",boolean(saved)},{"restoration",m_restoration.empty()?"null":m_restoration},{"native_fault",quote(m_selectedPlaneFault?"retain-selected-plane":m_fault?"retain-water-binding":"")},{"normal_simulation","true"}};
        if(m_planeSwitch)fields.push_back({"scenario",quote("plane-switch-v1")});
        else fields.push_back({"other_resident_plane_transition",quote("NOT_RUN_NO_DECLARED_SECOND_RESIDENT_LEVEL")});
        out<<object(fields)<<'\n';require(bool(out),"Water transition summary write failed");}
    void cancel()noexcept{if(m_pending.query){GLint active=0;glGetQueryiv(GL_PRIMITIVES_GENERATED,GL_CURRENT_QUERY,&active);if(GLuint(active)==m_pending.query)glEndQuery(GL_PRIMITIVES_GENERATED);glDeleteQueries(1,&m_pending.query);}if(m_pending.object)m_pending.object->setNativeDrawObserver(nullptr);m_pending={};}
    std::string draw(ChunkSectionRenderable& water,const Ogre::Pass& pass,GLuint count){using namespace ReferenceEdit;
        const auto errorBefore=glGetError();require(errorBefore==GL_NO_ERROR,"Water transition pre-observation GL error");
        GLint program=0,pipeline=0,active=0;glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_PROGRAM_PIPELINE_BINDING,&pipeline);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
        require(program>0 && !pipeline && glIsProgram(GLuint(program)) && count>0,"Water transition actual native program/primitive missing");GLint linked=0;glGetProgramiv(GLuint(program),GL_LINK_STATUS,&linked);require(linked==GL_TRUE,"Water transition actual native link failed");
        const auto* vs=dynamic_cast<const Ogre::GLSLShader*>(pass.getVertexProgram().get());const auto* fs=dynamic_cast<const Ogre::GLSLShader*>(pass.getFragmentProgram().get());GLuint shaders[2]{};GLsizei n=0;glGetAttachedShaders(GLuint(program),2,&n,shaders);require(vs && fs && n==2 && ((shaders[0]==vs->getGLShaderHandle() && shaders[1]==fs->getGLShaderHandle()) || (shaders[1]==vs->getGLShaderHandle() && shaders[0]==fs->getGLShaderHandle())),"Water transition actual attached stage mismatch");
        Fields uniforms;for(const char* name:{"linearHdrMode","planarReflectionEnabled","planarReflectionPlaneY","globalTime","fogDensity","waterDetailStrength"}){const auto loc=glGetUniformLocation(GLuint(program),name);require(loc>=0,"Water transition actual uniform missing "+std::string(name));float v=0;glGetUniformfv(GLuint(program),loc,&v);uniforms.push_back({name,number(v)});}
        float enabled=0;glGetUniformfv(GLuint(program),glGetUniformLocation(GLuint(program),"planarReflectionEnabled"),&enabled);
        if(m_planeSwitch){
            glGetUniformfv(GLuint(program),glGetUniformLocation(GLuint(program),"planarReflectionPlaneY"),&m_linkedPlane);
            const GLchar* matrixName="planarReflectionViewProj";GLuint matrixIndex=GL_INVALID_INDEX;
            glGetUniformIndices(GLuint(program),1,&matrixName,&matrixIndex);
            require(matrixIndex!=GL_INVALID_INDEX,"Water plane actual driver matrix missing");
            GLint type=0,size=0;glGetActiveUniformsiv(GLuint(program),1,&matrixIndex,GL_UNIFORM_TYPE,&type);glGetActiveUniformsiv(GLuint(program),1,&matrixIndex,GL_UNIFORM_SIZE,&size);
            const auto matrixLocation=glGetUniformLocation(GLuint(program),matrixName);
            require(matrixLocation>=0 && type==GL_FLOAT_MAT4 && size==1,"Water plane actual driver matrix type mismatch");
            glGetUniformfv(GLuint(program),matrixLocation,m_linkedMatrix.data());
            for(const auto v:m_linkedMatrix)require(std::isfinite(v),"Water plane nonfinite actual driver matrix");
        }
        const GLchar* samplerName="planarReflectionTexture";
        const auto loc=glGetUniformLocation(GLuint(program),samplerName);
        require(loc>=0,"Water transition actual sampler missing");
        GLuint samplerIndex=GL_INVALID_INDEX;glGetUniformIndices(GLuint(program),1,&samplerName,&samplerIndex);
        const bool samplerTypeObserved=samplerIndex!=GL_INVALID_INDEX;
        GLint samplerType=0,samplerSize=0;
        if(samplerTypeObserved){
            glGetActiveUniformsiv(GLuint(program),1,&samplerIndex,GL_UNIFORM_TYPE,&samplerType);
            glGetActiveUniformsiv(GLuint(program),1,&samplerIndex,GL_UNIFORM_SIZE,&samplerSize);
        }
        GLint unit=0;glGetUniformiv(GLuint(program),loc,&unit);require(unit>=0 && unit<32,"Water transition sampler unit bound");
        auto* textures=dynamic_cast<Ogre::GL3PlusTextureManager*>(Ogre::TextureManager::getSingletonPtr());
        require(textures,"Water transition actual GL3Plus texture manager missing");
        const GLuint fallbackTexture=textures->getWarningTextureID();
        const bool fallbackTextureValid=fallbackTexture>0 && glIsTexture(fallbackTexture);
        GLint originalBinding=0,samplerObject=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&originalBinding);
        struct ActiveGuard{GLint value;~ActiveGuard(){glActiveTexture(GLenum(value));}}guard{active};
        glActiveTexture(GL_TEXTURE0+GLenum(unit));
        glGetIntegerv(GL_SAMPLER_BINDING,&samplerObject);
        const bool samplerObjectValid=samplerObject==0 || glIsSampler(GLuint(samplerObject));
        GLint texture=0,w=0,h=0,format=0,baseLevel=0,maxLevel=0,minFilter=0,magFilter=0,textureMinFilter=0,textureMagFilter=0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
        const bool storageObserved=texture>0 && glIsTexture(GLuint(texture));
        // Older versions queried storage only when enabled and otherwise emitted
        // initialized zeros. Query every actual nonzero 2D binding, including the
        // inactive complete placeholder; zero values with !storageObserved are
        // explicitly unavailable, not a claim about actual image storage.
        if(storageObserved){
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&w);
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&h);
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
            glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,&baseLevel);
            glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,&maxLevel);
            glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&textureMinFilter);
            glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&textureMagFilter);
            minFilter=textureMinFilter;magFilter=textureMagFilter;
        }
        const bool samplerParametersObserved=storageObserved && samplerObjectValid;
        if(samplerParametersObserved && samplerObject!=0){
            glGetSamplerParameteriv(GLuint(samplerObject),GL_TEXTURE_MIN_FILTER,&minFilter);
            glGetSamplerParameteriv(GLuint(samplerObject),GL_TEXTURE_MAG_FILTER,&magFilter);
        }
        GLint sampledBindingAfter=0,samplerObjectAfter=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&sampledBindingAfter);
        glGetIntegerv(GL_SAMPLER_BINDING,&samplerObjectAfter);
        glActiveTexture(GLenum(active));
        GLint restored=0,restoredBinding=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&restored);glGetIntegerv(GL_TEXTURE_BINDING_2D,&restoredBinding);
        GLboolean depthWrite=GL_TRUE;glGetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);
        GLint src=0,dst=0;glGetIntegerv(GL_BLEND_SRC_RGB,&src);glGetIntegerv(GL_BLEND_DST_RGB,&dst);
        const bool blend=glIsEnabled(GL_BLEND)==GL_TRUE;const auto error=glGetError();
        require(error==GL_NO_ERROR && restored==active && restoredBinding==originalBinding && sampledBindingAfter==texture && samplerObjectAfter==samplerObject,"Water transition GL query state/error");
        m_linkedEnabled=enabled;m_linkedTexture=unsigned(texture);m_linkedWidth=w;m_linkedHeight=h;m_linkedFormat=format;
        m_completeFallback=samplerTypeObserved && samplerType==GL_SAMPLER_2D && samplerSize==1 &&
            storageObserved && samplerParametersObserved && fallbackTextureValid && GLuint(texture)==fallbackTexture &&
            w==8 && h==8 && format==GL_RGB8 && baseLevel==0 && maxLevel==0 &&
            minFilter==GL_LINEAR && magFilter==GL_LINEAR;
        require(blend && src==GL_SRC_ALPHA && dst==GL_ONE_MINUS_SRC_ALPHA && depthWrite==GL_FALSE,"Water transition production transparent draw state mismatch");
        Ogre::RenderOperation op;water.getRenderOperation(op);require(op.srcRenderable==&water && op.useIndexes && op.numberOfInstances==1 && op.operationType==Ogre::RenderOperation::OT_TRIANGLE_LIST && op.indexData && count==op.indexData->indexCount/3,"Water transition actual primitive/source mismatch");
        Fields facts{{"observation_domain",quote("actual-driver-after-native-Water-draw")},{"frame",number(m_frame)},{"program",number(program)},{"program_linked","true"},{"attached_production_stages","true"},{"primitive_count",number(count)},{"object",quote(water.getName())},{"vertex_program",quote(pass.getVertexProgramName())},{"fragment_program",quote(pass.getFragmentProgramName())},{"uniforms",object(uniforms)},{"sampler",object({{"observation_version",quote("complete-2d-sampler-v1")},{"location",number(loc)},{"unit",number(unit)},{"sampling_enabled",boolean(enabled>.5f)},{"binding_observed","true"},{"storage_observed",boolean(storageObserved)},{"sampler_type_observed",boolean(samplerTypeObserved)},{"actual_sampler_type",number(samplerType)},{"actual_sampler_array_size",number(samplerSize)},{"expected_fallback_texture_id",number(fallbackTexture)},{"fallback_identity_domain",quote("actual-GL3PlusTextureManager.getWarningTextureID")},{"fallback_texture_valid",boolean(fallbackTextureValid)},{"sampler_object",number(samplerObject)},{"sampler_object_valid",boolean(samplerObjectValid)},{"sampler_parameters_observed",boolean(samplerParametersObserved)},{"texture",number(texture)},{"width",number(w)},{"height",number(h)},{"format",number(format)},{"base_level",number(baseLevel)},{"max_level",number(maxLevel)},{"texture_min_filter",number(textureMinFilter)},{"texture_mag_filter",number(textureMagFilter)},{"min_filter",number(minFilter)},{"mag_filter",number(magFilter)}})},{"blend",object({{"enabled",boolean(blend)},{"src_rgb",number(src)},{"dst_rgb",number(dst)},{"depth_write",boolean(depthWrite==GL_TRUE)}})},{"gl_error_before",number(errorBefore)},{"gl_error",number(error)},{"query_state",object({{"active_texture_before",number(active)},{"active_texture_after",number(restored)},{"original_2d_binding_before",number(originalBinding)},{"original_2d_binding_after",number(restoredBinding)},{"sampled_2d_binding_before",number(texture)},{"sampled_2d_binding_after",number(sampledBindingAfter)},{"sampler_object_binding_before",number(samplerObject)},{"sampler_object_binding_after",number(samplerObjectAfter)}})},{"state_restored","true"}};
        if(m_planeSwitch){std::vector<std::string> values;for(const auto v:m_linkedMatrix)values.push_back(number(v));facts.push_back({"planar_matrix_column_major16",array(values)});facts.push_back({"matrix_observation_domain",quote("actual-driver-linked-Water-uniform-column-major")});}
        return object(facts);
    }
};
