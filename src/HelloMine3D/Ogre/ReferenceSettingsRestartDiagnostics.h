#pragma once
// One owned hidden observation of normal settings and same-save process restart.
// No alternate World, rendering, phase loop, delta freeze or retained resource.
#include "ReferenceResidencyDiagnostics.h"
#include <set>
#if defined(_WIN32)
#include <process.h>
extern char** _environ;
#else
#include <unistd.h>
extern char** environ;
#endif

class ReferenceSettingsRestartObservation final : public ChunkSectionRenderable::NativeDrawObserver, public Ogre::RenderObjectListener {
public:
    using Clock=std::chrono::steady_clock;
    static constexpr const char* Entry="HELLOMINE3D_REFERENCE_SETTINGS_RESTART_DIR";
    static void validateEntrypoint(bool validateOnly) {
        using namespace ReferenceEdit;if(!std::getenv(Entry))return;
        require(!validateOnly,"Settings restart cannot combine validation-only");
        const std::set<std::string> allowed{Entry,"HELLOMINE3D_ROOT","HELLOMINE3D_SAVE_DIR","HELLOMINE3D_CATALOGUE_DIR","HELLOMINE3D_WINDOW_HIDDEN","HELLO_RENDER_CAPTURE","HELLO_RENDER_CAPTURE_DIR","HELLO_RENDER_CAPTURE_MS","HELLO_RENDER_CAPTURE_MAX_DELTA_MS","HELLO_RENDER_CAPTURE_EXIT"};
#if defined(_WIN32)
        auto vars=_environ;
#else
        auto vars=environ;
#endif
        for(auto cursor=vars;cursor && *cursor;++cursor){const std::string line(*cursor),key=line.substr(0,line.find('='));bool family=false;for(const char* prefix:{"HELLOMINE3D_","HELLO_RENDER_","HELLO_PERF_","HELLO_VISUAL_"})family=family || key.compare(0,std::strlen(prefix),prefix)==0;require(!family || allowed.count(key),"Settings restart extra diagnostic environment "+key);}
        require(value("HELLOMINE3D_WINDOW_HIDDEN")=="1" && value("HELLO_RENDER_CAPTURE")=="1" && value("HELLO_RENDER_CAPTURE_MS")=="3500,7000" && value("HELLO_RENDER_CAPTURE_EXIT")=="1" && value("HELLO_RENDER_CAPTURE_MAX_DELTA_MS")=="5000","Settings restart hidden normal capture required");
        const auto output=path(Entry),stage=output.parent_path(),session=stage.parent_path(),root=path("HELLOMINE3D_ROOT"),save=path("HELLOMINE3D_SAVE_DIR"),catalogue=path("HELLOMINE3D_CATALOGUE_DIR"),frames=path("HELLO_RENDER_CAPTURE_DIR");
        const auto name=stage.filename().string();require(name=="stage-1-standard-hdr" || name=="stage-2-compatibility-legacy" || name=="stage-3-standard-hdr","Settings restart stage invalid");
        require(output==stage/"restart-facts" && frames==stage/"frames" && root==session/"Runtime.app/Contents/Resources" && save==session/"save" && catalogue==session/"catalogue","Settings restart owned layout required");
        require(std::filesystem::is_directory(stage) && !std::filesystem::exists(output) && !std::filesystem::exists(frames),"Settings restart new facts/frames required");
        const auto marker=session/".hellomine3d-reference-settings-restart-owned";for(const auto& child:{marker,save/"world.meta",save/"chunks"})require(!std::filesystem::is_symlink(std::filesystem::symlink_status(child)),"Settings restart child symlink prohibited");
        require(std::filesystem::is_regular_file(marker) && std::filesystem::is_regular_file(save/"world.meta") && std::filesystem::is_directory(save/"chunks") && std::filesystem::is_directory(root),"Settings restart real marker/save/runtime required");
        std::ifstream input(marker,std::ios::binary);require(std::string((std::istreambuf_iterator<char>(input)),{})=="HelloMine3D owned reference settings restart session v1\n","Settings restart marker invalid");
        require(name!="stage-1-standard-hdr" || !std::filesystem::exists(catalogue),"Settings restart first catalogue must be new");
    }
    static std::string validateConfig(const Config& c){using namespace ReferenceEdit;if(!std::getenv(Entry))return {};const auto out=path(Entry);const bool hdr=out.parent_path().filename()!="stage-2-compatibility-legacy";require(!c.isFullscreen && c.windowX==1280 && c.windowY==720 && c.renderDistance==3 && c.directionalShadowQuality==DirectionalShadowQuality::Medium && c.postProcessingQuality==PostProcessingQuality::Off && (hdr?(c.visualDetail==VisualDetail::Standard && c.renderPipeline==RenderPipeline::LinearHdr):(c.visualDetail==VisualDetail::Compatibility && c.renderPipeline==RenderPipeline::Legacy)),"Settings restart actual config profile differs");return out.string();}
    static std::string world(World& w){using namespace ReferenceEdit;const auto i=w.observeWorldIdentity();return object({{"world_id",quote(i.worldId)},{"seed",number(i.seed)},{"terrain_generation_version",number(i.terrainGenerationVersion)}});}
    static std::string player(World& w,const Player& p){using namespace ReferenceEdit;const auto state=p.getSaveState();std::vector<std::string> inventory;for(const auto& slot:state.inventory)inventory.push_back(object({{"material",number(int(slot.materialId))},{"amount",number(slot.amount)},{"durability",number(slot.durability)}}));return object({{"position",xyz(state.position)},{"rotation",xyz(state.rotation)},{"held",number(state.heldItem)},{"health",number(w.getPlayerHealth())},{"food_cooldown",number(w.getFoodCooldownTicksRemaining())},{"attack_cooldown",number(w.getAttackCooldownTicksRemaining())},{"inventory",array(inventory)}});}
    ReferenceSettingsRestartObservation(const std::string& output,World& w,const Player& p,Ogre::SceneManager& scene,Ogre::Camera& camera,const Config& c,const std::string& configPath):m_output(output),m_scene(scene),m_camera(camera),m_config(c),m_configPath(configPath),m_loadedPlayer(player(w,p)),m_loadedWorld(world(w)),m_loadedTime(w.getWorldTime()),m_started(Clock::now()) {
        using namespace ReferenceEdit;require(!std::filesystem::exists(m_output),"Settings restart output exists");std::filesystem::create_directory(m_output);
        const auto save=path("HELLOMINE3D_SAVE_DIR");std::ifstream in(save/"world.meta");for(std::string line;std::getline(in,line);)if(line.compare(0,9,"world_id ")==0)m_diskWorld=line.substr(9);require(!m_diskWorld.empty(),"Settings restart disk world identity missing");m_scene.addRenderObjectListener(this);m_listener=true;
    }
    ~ReferenceSettingsRestartObservation() override {detach();}
    void detach()noexcept{cancel();if(m_listener){m_scene.removeRenderObjectListener(this);m_listener=false;}}
    void normalUpdate(float delta){if(std::isfinite(delta) && delta>0){m_delta=delta;++m_updates;}}
    void beginFrame(unsigned frame,World& w,bool simulationAdvanced){cancel();m_frame=frame;m_terrain.clear();m_water.clear();m_eligible=!m_done && simulationAdvanced && frame>0 && m_updates>0 && m_delta>0 && w.getWorldTime()>m_loadedTime;}
    void notifyRenderSingleObject(Ogre::Renderable* renderable,const Ogre::Pass* pass,const Ogre::AutoParamDataSource* source,const Ogre::LightList*,bool suppressed) override {
        if(!m_eligible || suppressed || !pass || !source || source->getCurrentCamera()!=&m_camera)return;auto* object=dynamic_cast<ChunkSectionRenderable*>(renderable);if(!object)return;
        const auto name=pass->getParent()->getParent()->getName();const bool terrain=name=="HelloMine3D/Terrain",water=name=="HelloMine3D/Water";if((!terrain && !water) || (terrain && !m_terrain.empty()) || (water && !m_water.empty()))return;
        ReferenceEdit::require(!m_pending.object,"Settings restart nested observation");m_pending={object,pass,0,terrain};object->setNativeDrawObserver(this);
    }
    void beforeNativeDraw(ChunkSectionRenderable& object,Ogre::SceneManager* scene,Ogre::RenderSystem*) override {if(m_pending.object!=&object)return;using namespace ReferenceEdit;require(scene==&m_scene && m_pending.pass->getPassIterationCount()==1 && !m_pending.pass->hasGeometryProgram(),"Settings restart production draw scope invalid");GLint query=0;glGetQueryiv(GL_PRIMITIVES_GENERATED,GL_CURRENT_QUERY,&query);require(!query,"Settings restart existing query untouched");glGenQueries(1,&m_pending.query);require(m_pending.query!=0,"Settings restart query unavailable");glBeginQuery(GL_PRIMITIVES_GENERATED,m_pending.query);}
    void afterNativeDraw(ChunkSectionRenderable& object,Ogre::SceneManager*,Ogre::RenderSystem*) override {if(m_pending.object!=&object || !m_pending.query)return;glEndQuery(GL_PRIMITIVES_GENERATED);GLuint primitives=0;glGetQueryObjectuiv(m_pending.query,GL_QUERY_RESULT,&primitives);glDeleteQueries(1,&m_pending.query);m_pending.query=0;const auto facts=draw(object,*m_pending.pass,primitives,m_frame);if(m_pending.terrain)m_terrain=facts;else m_water=facts;object.setNativeDrawObserver(nullptr);m_pending={};}
    bool ready()const{return m_eligible && !m_terrain.empty() && !m_water.empty();}
    void finish(World& w,const Player& p,Ogre::RenderWindow& window,const std::string& hdr,const std::string& planar,const std::optional<float>& spatialAaStrength){if(!ready())return;using namespace ReferenceEdit;
        std::vector<std::string> cells;
        for(const auto& sample:ReferenceResidencyProbe::Samples) {
            // Blocking World lock and find-only lookup: missing/unloaded data
            // returns Air. Every fixed sample is non-Air, so exact ID/meta
            // proves the loaded cell without a best-effort map lock result.
            const auto block=w.getBlock(sample.p.x,sample.p.y,sample.p.z);
            const bool known=block.id!=static_cast<Block_t>(BlockId::Air);
            require(known && block==sample.block,"Settings restart actual loaded non-Air cell/meta differs");
            const auto cp=World::getChunkXZ(sample.p.x,sample.p.z);
            cells.push_back(object({{"position",xyz(sample.p)},{"id",number(block.id)},
                {"metadata",number(block.metadata)},{"known",boolean(known)},
                {"chunk",array({number(cp.x),number(cp.z)})},
                {"observation",quote("blocking-World.getBlock-find-only-nonAir")},
                {"observed_frame",number(m_frame)}}));
        }
        require(window.getWidth()>0 && window.getHeight()>0 && std::uint64_t(window.getWidth())*window.getHeight()<=8294400,"Settings restart actual physical pixel budget");
        GLint read=0,drawFbo=0,samples=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFbo);glBindFramebuffer(GL_FRAMEBUFFER,0);glGetIntegerv(GL_SAMPLES,&samples);const auto error=glGetError();glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(drawFbo));GLint restoredRead=0,restoredDraw=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&restoredRead);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&restoredDraw);require(error==GL_NO_ERROR && restoredRead==read && restoredDraw==drawFbo && glGetError()==GL_NO_ERROR,"Settings restart window native query/restore failed");require(m_inputs==0,"Settings restart ordinary input observed");
#if defined(_WIN32)
        const auto pid=_getpid();
#else
        const auto pid=getpid();
#endif
        const auto loadedWorld=m_loadedWorld.substr(0,m_loadedWorld.size()-1)+",\"disk_world_id\":"+quote(m_diskWorld)+",\"save_directory\":"+quote(value("HELLOMINE3D_SAVE_DIR"))+"}";
        const auto facts=object({{"schema",quote("hellomine3d-reference-settings-restart-observation-v1")},{"status",quote("OBSERVED")},{"pid",number(pid)},{"stage",quote(m_output.parent_path().filename().string())},{"frame",number(m_frame)},{"elapsed_ms",number(std::chrono::duration<double,std::milli>(Clock::now()-m_started).count())},{"input_event_count",number(m_inputs)},
            {"config",object({{"path",quote(m_configPath)},{"visualdetail",quote(m_config.visualDetail==VisualDetail::Standard?"standard":"compatibility")},{"renderpipeline",quote(m_config.renderPipeline==RenderPipeline::LinearHdr?"linear-hdr":"legacy")},{"window_points",array({number(m_config.windowX),number(m_config.windowY)})},{"fullscreen",boolean(m_config.isFullscreen)},{"renderdistance",number(m_config.renderDistance)},{"shadow",number(int(m_config.directionalShadowQuality))},{"post",number(int(m_config.postProcessingQuality))}})},
            {"loaded_world",loadedWorld},{"loaded_player",m_loadedPlayer},{"loaded_world_time",number(m_loadedTime)},{"world",world(w)},{"player",player(w,p)},{"world_time",number(w.getWorldTime())},{"simulation_delta",number(m_delta)},{"simulation_updates",number(m_updates)},{"cells",array(cells)},
            {"window",object({{"framebuffer","0"},{"width",number(window.getWidth())},{"height",number(window.getHeight())},{"samples",number(samples)},{"gl_error",number(error)},{"bindings_restored","true"}})},{"terrain_draw",m_terrain},{"water_draw",m_water},{"hdr",hdr},{"planar",planar},{"spatial_aa_enabled",boolean(spatialAaStrength && *spatialAaStrength>.5f)},{"spatial_aa_strength",spatialAaStrength?number(*spatialAaStrength):"null"},{"spatial_aa_observation_domain",quote(spatialAaStrength?"actual-HdrResolve-Ogre-pass-parameter":"actual-HdrPipeline-inactive")}});
        require(facts.size()<=1024u*1024u,"Settings restart snapshot byte budget");const auto target=m_output/"snapshot.json";require(!std::filesystem::exists(target),"Settings restart snapshot overwrite");std::ofstream out(target);out<<facts<<'\n';require(bool(out),"Settings restart snapshot write failed");m_done=true;m_eligible=false;detach();
    }
    void input(){++m_inputs;}
private:
    struct Pending {ChunkSectionRenderable* object=nullptr;const Ogre::Pass* pass=nullptr;GLuint query=0;bool terrain=false;};
    std::filesystem::path m_output;Ogre::SceneManager& m_scene;Ogre::Camera& m_camera;Config m_config;std::string m_configPath,m_loadedPlayer,m_loadedWorld,m_diskWorld,m_terrain,m_water;float m_loadedTime=0,m_delta=0;Clock::time_point m_started;unsigned m_frame=0,m_updates=0,m_inputs=0;bool m_done=false,m_eligible=false,m_listener=false;Pending m_pending;
    void cancel()noexcept{if(m_pending.query){GLint active=0;glGetQueryiv(GL_PRIMITIVES_GENERATED,GL_CURRENT_QUERY,&active);if(GLuint(active)==m_pending.query)glEndQuery(GL_PRIMITIVES_GENERATED);glDeleteQueries(1,&m_pending.query);}if(m_pending.object)m_pending.object->setNativeDrawObserver(nullptr);m_pending={};}
    static std::string draw(ChunkSectionRenderable& source,const Ogre::Pass& pass,GLuint primitiveCount,unsigned frame){using namespace ReferenceEdit;GLint program=0,pipeline=0,active=0;glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_PROGRAM_PIPELINE_BINDING,&pipeline);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);require(program>0 && !pipeline && glIsProgram(GLuint(program)),"Settings restart actual linked program missing");GLint linked=0;glGetProgramiv(GLuint(program),GL_LINK_STATUS,&linked);require(linked==GL_TRUE && primitiveCount>0,"Settings restart real draw/link missing");
        const auto* vs=dynamic_cast<const Ogre::GLSLShader*>(pass.getVertexProgram().get());const auto* fs=dynamic_cast<const Ogre::GLSLShader*>(pass.getFragmentProgram().get());GLuint attached[2]{};GLsizei count=0;glGetAttachedShaders(GLuint(program),2,&count,attached);require(vs && fs && count==2 && ((attached[0]==vs->getGLShaderHandle() && attached[1]==fs->getGLShaderHandle()) || (attached[1]==vs->getGLShaderHandle() && attached[0]==fs->getGLShaderHandle())),"Settings restart actual stage/pass mismatch");
        Fields uniforms;for(const char* name:{"linearHdrMode","planarReflectionEnabled","waterDetailStrength","waterBoundaryPinsV1"}){const auto location=glGetUniformLocation(GLuint(program),name);if(location>=0){float value=0;glGetUniformfv(GLuint(program),location,&value);uniforms.push_back({name,number(value)});}}
        std::vector<std::string> samplers;unsigned arrays=0;GLint uniformCount=0;glGetProgramiv(GLuint(program),GL_ACTIVE_UNIFORMS,&uniformCount);struct ActiveGuard{GLint active;~ActiveGuard(){glActiveTexture(GLenum(active));}}guard{active};
        for(GLint index=0;index<uniformCount;++index){char name[256]{};GLsizei length=0;GLint size=0;GLenum type=0;glGetActiveUniform(GLuint(program),GLuint(index),sizeof name,&length,&size,&type,name);GLenum target=0,binding=0;if(type==GL_SAMPLER_2D || type==GL_SAMPLER_2D_SHADOW){target=GL_TEXTURE_2D;binding=GL_TEXTURE_BINDING_2D;}else if(type==GL_SAMPLER_2D_ARRAY){target=GL_TEXTURE_2D_ARRAY;binding=GL_TEXTURE_BINDING_2D_ARRAY;++arrays;}else continue;
            require(size==1 && samplers.size()<8,"Settings restart active sampler bound");const auto location=glGetUniformLocation(GLuint(program),name);GLint unit=0,texture=0,width=0,height=0,depth=0,format=0;glGetUniformiv(GLuint(program),location,&unit);require(unit>=0 && unit<32,"Settings restart sampler unit bound");glActiveTexture(GL_TEXTURE0+GLenum(unit));glGetIntegerv(binding,&texture);float reflectionEnabled=1;const auto reflectionLocation=glGetUniformLocation(GLuint(program),"planarReflectionEnabled");if(reflectionLocation>=0)glGetUniformfv(GLuint(program),reflectionLocation,&reflectionEnabled);
            const bool samplingEnabled=std::string(name)!="planarReflectionTexture" || reflectionEnabled>.5f;
            require(!samplingEnabled || (texture>0 && glIsTexture(GLuint(texture))),"Settings restart consumed bound texture absent");unsigned mips=0;
            if(texture>0){require(glIsTexture(GLuint(texture)),"Settings restart invalid bound texture");glGetTexLevelParameteriv(target,0,GL_TEXTURE_WIDTH,&width);glGetTexLevelParameteriv(target,0,GL_TEXTURE_HEIGHT,&height);glGetTexLevelParameteriv(target,0,GL_TEXTURE_DEPTH,&depth);glGetTexLevelParameteriv(target,0,GL_TEXTURE_INTERNAL_FORMAT,&format);for(unsigned level=0;level<16;++level){GLint edge=0;glGetTexLevelParameteriv(target,GLint(level),GL_TEXTURE_WIDTH,&edge);if(edge<=0)break;++mips;}}
            samplers.push_back(object({{"name",quote(name)},{"sampling_enabled",boolean(samplingEnabled)},{"location",number(location)},{"unit",number(unit)},{"target",number(target)},{"texture",number(texture)},{"width",number(width)},{"height",number(height)},{"depth",number(depth)},{"internal_format",number(format)},{"mip_levels",number(mips)}}));}
        glActiveTexture(GLenum(active));GLint restored=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&restored);const auto error=glGetError();require(error==GL_NO_ERROR && restored==active,"Settings restart actual sampler query/restore failed");Ogre::RenderOperation operation;source.getRenderOperation(operation);require(operation.srcRenderable==&source && operation.useIndexes && operation.numberOfInstances==1 && operation.operationType==Ogre::RenderOperation::OT_TRIANGLE_LIST && operation.indexData && primitiveCount==operation.indexData->indexCount/3,"Settings restart actual indexed primitive count mismatch");Ogre::Matrix4 transform;source.getWorldTransforms(&transform);const auto position=transform.getTrans();
        return object({{"frame",number(frame)},{"view",quote("main")},{"object",quote(source.getName())},{"material",quote(pass.getParent()->getParent()->getName())},{"vertex_program",quote(pass.getVertexProgramName())},{"fragment_program",quote(pass.getFragmentProgramName())},{"program",number(program)},{"program_linked","true"},{"production_attached_shaders","true"},{"primitive_count",number(primitiveCount)},{"source_origin",xyz(position)},{"source_section",array({number(int(std::floor(position.x/16.f))),number(int(std::floor(position.y/16.f))),number(int(std::floor(position.z/16.f)))})},{"use_indexes",boolean(operation.useIndexes)},{"instance_count",number(operation.numberOfInstances)},{"expected_triangles",number(operation.indexData->indexCount/3)},{"uniforms",object(uniforms)},{"samplers",array(samplers)},{"array_sampler_count",number(arrays)},{"gl_error",number(error)},{"state_restored","true"}});
    }
};
