#pragma once
// Explicit developer diagnostics only. No object in these facts owns an Ogre
// resource: names and native IDs must not extend the measured lifetime.
#include <Ogre.h>
#include <GL/gl3w.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace RenderLifecycle {
inline std::string quote(const std::string& value) {
    std::ostringstream out; out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c == '\n') out << "\\n";
        else if (c == '\r') out << "\\r";
        else if (c == '\t') out << "\\t";
        else if (c < 32) throw std::runtime_error("Invalid control byte in lifecycle evidence.");
        else out << c;
    }
    return out.str() + '"';
}
struct Attachment {
    unsigned kind=GL_NONE, object=0, format=0, width=0, height=0, samples=0;
};
struct Native {
    unsigned resolveFbo=0, drawFbo=0, errorBefore=0, errorAfter=0;
    bool complete=false;
    Attachment colour, resolved, depth, stencil;
};
inline Attachment attachment(GLenum slot) {
    Attachment a; GLint value=0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,slot,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,&value); a.kind=unsigned(value);
    if (a.kind==GL_NONE) return a;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,slot,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&value); a.object=unsigned(value);
    if (a.kind==GL_RENDERBUFFER) {
        glBindRenderbuffer(GL_RENDERBUFFER,a.object);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_INTERNAL_FORMAT,&value); a.format=unsigned(value);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_WIDTH,&value); a.width=unsigned(value);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_HEIGHT,&value); a.height=unsigned(value);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_SAMPLES,&value); a.samples=unsigned(value);
    } else if (a.kind==GL_TEXTURE) {
        GLint level=0; glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,slot,GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL,&level);
        glBindTexture(GL_TEXTURE_2D,a.object);
        glGetTexLevelParameteriv(GL_TEXTURE_2D,level,GL_TEXTURE_INTERNAL_FORMAT,&value); a.format=unsigned(value);
        glGetTexLevelParameteriv(GL_TEXTURE_2D,level,GL_TEXTURE_WIDTH,&value); a.width=unsigned(value);
        glGetTexLevelParameteriv(GL_TEXTURE_2D,level,GL_TEXTURE_HEIGHT,&value); a.height=unsigned(value);
    }
    return a;
}
inline Native native(Ogre::RenderTexture* target, bool multisample) {
    Native n; if (!target) return n;
    n.errorBefore=glGetError();
    GLint draw=0,read=0,rb=0,tex=0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
    glGetIntegerv(GL_RENDERBUFFER_BINDING,&rb); glGetIntegerv(GL_TEXTURE_BINDING_2D,&tex);
    target->getCustomAttribute("GL_FBOID",&n.resolveFbo);
    if (multisample) target->getCustomAttribute("GL_MULTISAMPLEFBOID",&n.drawFbo); else n.drawFbo=n.resolveFbo;
    if (n.resolveFbo && n.drawFbo) {
        glBindFramebuffer(GL_FRAMEBUFFER,n.resolveFbo);
        n.complete=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
        n.resolved=attachment(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_FRAMEBUFFER,n.drawFbo);
        n.complete=n.complete && glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
        n.colour=attachment(GL_COLOR_ATTACHMENT0); n.depth=attachment(GL_DEPTH_ATTACHMENT); n.stencil=attachment(GL_STENCIL_ATTACHMENT);
    }
    glBindTexture(GL_TEXTURE_2D,unsigned(tex)); glBindRenderbuffer(GL_RENDERBUFFER,unsigned(rb));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,unsigned(draw)); glBindFramebuffer(GL_READ_FRAMEBUFFER,unsigned(read));
    n.errorAfter=glGetError(); return n;
}
inline std::string json(const Attachment& a) {
    std::ostringstream o; o << std::boolalpha << "{\"kind\":"<<a.kind<<",\"object\":"<<a.object<<",\"format\":"<<a.format
      <<",\"width\":"<<a.width<<",\"height\":"<<a.height<<",\"samples\":"<<a.samples<<'}'; return o.str();
}
inline std::string json(const Native& n) {
    std::ostringstream o; o<<std::boolalpha<<"{\"resolve_fbo\":"<<n.resolveFbo<<",\"draw_fbo\":"<<n.drawFbo<<",\"complete\":"<<n.complete
      <<",\"gl_error_before\":"<<n.errorBefore<<",\"gl_error_after\":"<<n.errorAfter
      <<",\"colour\":"<<json(n.colour)<<",\"resolved\":"<<json(n.resolved)<<",\"depth\":"<<json(n.depth)<<",\"stencil\":"<<json(n.stencil)<<'}';return o.str();
}
inline bool valid(const Native& n,unsigned w,unsigned h,unsigned samples) {
    const auto dims=[&](const Attachment& a){return a.object && a.width==w && a.height==h && a.samples==samples;};
    return n.complete && !n.errorBefore && !n.errorAfter && n.resolved.kind==GL_TEXTURE && n.resolved.format==GL_RGBA16F &&
      n.resolved.width==w && n.resolved.height==h && n.resolved.samples==0 && n.colour.format==GL_RGBA16F &&
      dims(n.colour) && n.depth.kind==GL_RENDERBUFFER && dims(n.depth) &&
      (n.stencil.kind==GL_NONE || dims(n.stencil));
}
struct Retirement {
    std::string json;
    bool pass=true;
};
inline Retirement retire(const Native& before,const std::vector<std::string>& textureNames,const std::vector<std::string>& materialNames) {
    Retirement r;std::ostringstream o;o<<std::boolalpha<<"{\"gl_error_before\":";unsigned eb=glGetError();o<<eb<<",\"objects\":[";r.pass=eb==0;
    std::vector<std::pair<unsigned,unsigned>> ids;
    const auto add=[&](unsigned kind,unsigned id){if(id && std::find(ids.begin(),ids.end(),std::make_pair(kind,id))==ids.end())ids.emplace_back(kind,id);};
    add(GL_FRAMEBUFFER,before.resolveFbo);add(GL_FRAMEBUFFER,before.drawFbo);
    for(const auto& a : {before.colour,before.resolved,before.depth,before.stencil}) if(a.kind==GL_TEXTURE || a.kind==GL_RENDERBUFFER)add(a.kind,a.object);
    bool comma=false;
    for(const auto& id:ids){bool alive=id.first==GL_FRAMEBUFFER ? glIsFramebuffer(id.second)==GL_TRUE : id.first==GL_TEXTURE ? glIsTexture(id.second)==GL_TRUE : glIsRenderbuffer(id.second)==GL_TRUE;
      if(comma)o<<',';comma=true;o<<"{\"kind\":"<<id.first<<",\"id\":"<<id.second<<",\"alive\":"<<alive<<'}';r.pass=r.pass&&!alive;}
    o<<"],\"manager_names\":[";comma=false;
    const auto names=[&](const std::vector<std::string>& values,bool texture){for(const auto& name:values){
      bool alive=texture ? Ogre::TextureManager::getSingleton().resourceExists(name) : Ogre::MaterialManager::getSingleton().resourceExists(name);
      if(comma)o<<',';comma=true;o<<"{\"kind\":"<<quote(texture?"texture":"material")<<",\"name\":"<<quote(name)<<",\"alive\":"<<alive<<'}';r.pass=r.pass&&!alive;}};
    names(textureNames,true);names(materialNames,false);unsigned ea=glGetError();r.pass=r.pass&&ea==0;o<<"],\"gl_error_after\":"<<ea<<'}';r.json=o.str();return r;
}
inline std::string namesJson(const std::vector<std::string>& names) { std::ostringstream o;o<<'[';for(std::size_t i=0;i<names.size();++i){if(i)o<<',';o<<quote(names[i]);}return o.str()+']'; }
}
struct RenderLifecycleTargetFacts {
    std::uint64_t generation=0, updateCount=0;
    unsigned width=0,height=0,targetCount=0,depthCount=0,cameraCount=0,observerFailures=0,depthPool=0;
    std::size_t privateMaterials=0,privatePasses=0;
    bool active=false,selected=false,binderBound=false,waterSamplerBound=false,lodCameraBound=false,listenersActive=false,ownedDepthAttached=false;
    std::string textureName,cameraName;
    std::vector<std::string> materialNames;
    RenderLifecycle::Native native;
    std::string json() const {
        std::ostringstream o;o<<std::boolalpha<<"{\"active\":"<<active<<",\"generation\":"<<generation<<",\"update_count\":"<<updateCount
          <<",\"width\":"<<width<<",\"height\":"<<height<<",\"target_count\":"<<targetCount<<",\"depth_count\":"<<depthCount
          <<",\"owned_depth_attached\":"<<ownedDepthAttached<<",\"depth_pool\":"<<depthPool<<",\"camera_count\":"<<cameraCount<<",\"private_materials\":"<<privateMaterials<<",\"private_passes\":"<<privatePasses
          <<",\"selected\":"<<selected<<",\"binder_bound\":"<<binderBound<<",\"water_sampler_bound\":"<<waterSamplerBound
          <<",\"lod_camera_bound\":"<<lodCameraBound<<",\"listeners_active\":"<<listenersActive<<",\"observer_failures\":"<<observerFailures
          <<",\"texture_name\":"<<RenderLifecycle::quote(textureName)<<",\"camera_name\":"<<RenderLifecycle::quote(cameraName)
          <<",\"material_names\":"<<RenderLifecycle::namesJson(materialNames)<<",\"native\":"<<RenderLifecycle::json(native)<<'}';return o.str();
    }
};
class RenderLifecycleProbe {
public:
    static constexpr unsigned MaximumFrames=4096;
    static constexpr unsigned MaximumSeconds=45;
    static constexpr unsigned MaximumJournalRecords=128;
    static constexpr unsigned StageMaximumFrames=1024;
    static constexpr unsigned StageMaximumSeconds=8;
    inline static constexpr std::array<const char*,13> Phases{{"warm-a","resize-small","resize-large","resize-restore","clear-a1","load-b","warm-b","clear-b","load-a2","warm-a2","clear-a2","components-destroyed","root-shutdown"}};
    // Admission precedes ResourcePaths/resource preflight and its optional
    // manifest writer. Ordinary validation/manifest output keeps its old path.
    static void validateEntrypoint(bool validateOnly) {
        if(!std::getenv("HELLOMINE3D_RENDER_LIFECYCLE_PROBE") && !std::getenv("HELLOMINE3D_LIFECYCLE_FAULT"))return;
        if(validateOnly)throw std::runtime_error("Lifecycle probe cannot combine HELLOMINE3D_VALIDATE_ONLY.");
        const char* manifest=std::getenv("HELLOMINE3D_EFFECTIVE_MANIFEST_OUT");
        if(manifest && manifest[0]!='\0')throw std::runtime_error("Lifecycle probe cannot combine HELLOMINE3D_EFFECTIVE_MANIFEST_OUT.");
    }
    static std::string validateEnvironment(bool hdr,bool standard,bool fullscreen,unsigned width,unsigned height) {
        const char* enabled=std::getenv("HELLOMINE3D_RENDER_LIFECYCLE_PROBE");
        const char* fault=std::getenv("HELLOMINE3D_LIFECYCLE_FAULT");
        if(!enabled){if(fault)throw std::runtime_error("Lifecycle native fault requires explicit probe.");return {};}
        if(fault && std::string(fault)!="delayed-hdr-drain")throw std::runtime_error("Unknown lifecycle native fault.");
        if(std::string(enabled)!="1")throw std::runtime_error("Lifecycle probe requires exact opt-in value1.");
        const auto value=[](const char* key){const char* p=std::getenv(key);return p?std::string(p):std::string();};
        if(value("HELLOMINE3D_WINDOW_HIDDEN")!="1" || !hdr || !standard || fullscreen || width!=1280 || height!=720)
            throw std::runtime_error("Lifecycle probe requires hidden windowed HDR/standard1280x720, not ordinary input.");
        // Check actual activation keys, not all environment variables. Zero
        // remains a disabled flag; ROOT/SAVE/CATALOGUE/TIME/pose/MSAA identity
        // inputs are required or permitted by this diagnostic's own contract.
        for(const char* key:{"HELLO_RENDER_CAPTURE","HELLO_PERF_CAPTURE",
                "HELLOMINE3D_PLANAR_DIAGNOSTIC","HELLOMINE3D_PLANAR_REFLECTION_OFF",
                "HELLOMINE3D_HDR_FALLBACK","HELLOMINE3D_REFLECTION_FALLBACK",
                "HELLOMINE3D_FORCE_LEGACY_TERRAIN","HELLOMINE3D_V10C_FALLBACK",
                "HELLOMINE3D_V10D_SHADOW_FALLBACK","HELLOMINE3D_V10E_POST_FALLBACK",
                "HELLOMINE3D_V10D_SHADOW_DIAGNOSTICS","HELLOMINE3D_DISABLE_VERTEX_AO",
                "HELLOMINE3D_V10E_SETTINGS_FIXTURE","HELLOMINE3D_V10D_SHADOW_FIXTURE",
                "HELLOMINE3D_V10E_POST_FIXTURE","HELLOMINE3D_TRANSPARENT_FIXTURE",
                "HELLOMINE3D_SPAWN_VALIDATION_ACTORS","HELLOMINE3D_ORE_FIXTURE",
                "HELLOMINE3D_CONTAINER_FIXTURE","HELLOMINE3D_CRAFTING_FIXTURE",
                "HELLOMINE3D_COMBAT_FIXTURE","HELLOMINE3D_HUD_FIXTURE",
                "HELLOMINE3D_CROP_FIXTURE","HELLOMINE3D_VERTICAL_SLICE_FIXTURE"})
            if(!value(key).empty() && value(key)!="0")throw std::runtime_error(std::string("Lifecycle probe cannot combine ")+key);
        // OgreRenderCapture falls back to this historical activation alias only
        // when the primary value is absent/empty. A primary zero disables it.
        if(value("HELLO_RENDER_CAPTURE").empty() && !value("HELLO_VISUAL_CAPTURE").empty() && value("HELLO_VISUAL_CAPTURE")!="0")
            throw std::runtime_error("Lifecycle probe cannot combine HELLO_VISUAL_CAPTURE.");
        for(const char* key:{"HELLOMINE3D_REFERENCE_VISUAL_SCENE",
                "HELLOMINE3D_VISUAL_CAMERA_SWEEP","HELLOMINE3D_VISUAL_CAMERA_PATH",
                "HELLOMINE3D_RC_PERF_PROFILE","HELLOMINE3D_E2_BATCH_MANIFEST",
                "HELLOMINE3D_PLAYER_MOTION_CAPTURE","HELLOMINE3D_ACTOR_VISUAL_CAPTURE",
                "HELLOMINE3D_ACTOR_VISUAL_DISTANCE","HELLOMINE3D_BLOCK_FEEDBACK_CAPTURE",
                "HELLOMINE3D_MATERIAL_IDENTITY_CAPTURE_DIR","HELLOMINE3D_CAMERA_DIAGNOSTICS_DIR",
                "HELLOMINE3D_FERN_WIND_CAPTURE_DIR","HELLOMINE3D_SHORE_EDIT_CAPTURE_DIR",
                "HELLOMINE3D_PAUSE_NOTIFICATIONS_DIR","HELLOMINE3D_HUD_PAGE_FIXTURE",
                "HELLOMINE3D_HUD_INSPECT_SLOT","HELLOMINE3D_MACHINE_FIXTURE",
                "HELLOMINE3D_P11_LIGHT_FIXTURE","HELLOMINE3D_VERTEX_LIGHTING_FIXTURE",
                "HELLOMINE3D_CONTROLLED_CRASH","HELLOMINE3D_RESOURCE_PACKS",
                "HELLOMINE3D_EXIT_AFTER_FRAMES"})
            if(!value(key).empty())throw std::runtime_error(std::string("Lifecycle probe cannot combine fixture ")+key);
        const auto path=[&](const char* key){std::filesystem::path p(value(key));if(p.empty() || !p.is_absolute() || p.lexically_normal()!=p)throw std::runtime_error(std::string("Lifecycle requires explicit canonical path ")+key);
          for(auto at=p;!at.empty();at=at.parent_path()){if(std::filesystem::is_symlink(std::filesystem::symlink_status(at)))throw std::runtime_error("Lifecycle paths cannot traverse symlinks.");if(at==at.parent_path())break;}
          if(std::filesystem::weakly_canonical(p)!=p)throw std::runtime_error("Lifecycle path is not canonical.");return p;};
        const auto output=path("HELLOMINE3D_RENDER_LIFECYCLE_DIR"),a=path("HELLOMINE3D_SAVE_DIR"),b=path("HELLOMINE3D_LIFECYCLE_SAVE_B"),cat=path("HELLOMINE3D_CATALOGUE_DIR"),resources=path("HELLOMINE3D_ROOT");
        const auto session=output.parent_path();
        if(output.filename()!="lifecycle" || a!=session/"save-a" || b!=session/"save-b" || cat!=session/"catalogue" || resources!=session/"Runtime.app/Contents/Resources" || std::filesystem::exists(output) || std::filesystem::exists(cat))
            throw std::runtime_error("Lifecycle probe requires independent save-a/save-b, new catalogue/lifecycle and owned Runtime.app siblings.");
        const auto marker=session/".hellomine3d-render-lifecycle-owned";
        if(!std::filesystem::is_regular_file(marker) || std::filesystem::is_symlink(marker))throw std::runtime_error("Lifecycle session ownership marker missing.");
        std::ifstream m(marker);std::stringstream text;text<<m.rdbuf();if(text.str()!="HelloMine3D owned render lifecycle session v1\n")throw std::runtime_error("Lifecycle session ownership marker invalid.");
        for(const auto& save:{a,b})if(!std::filesystem::is_regular_file(save/"world.meta") || std::filesystem::is_symlink(save/"world.meta") || !std::filesystem::is_directory(save/"chunks"))throw std::runtime_error("Lifecycle requires two existing real cloned saves.");
        return output.string();
    }
    explicit RenderLifecycleProbe(std::string directory):m_directory(std::move(directory)),m_started(std::chrono::steady_clock::now()) {
        std::filesystem::create_directory(m_directory);m_journal.open(std::filesystem::path(m_directory)/"journal.jsonl",std::ios::out);
        if(!m_journal)throw std::runtime_error("Cannot open lifecycle journal.");
    }
    ~RenderLifecycleProbe(){if(!m_finished){try{summary("FAILED",m_failure.empty()?"Probe did not finish component/root shutdown.":m_failure);}catch(...){}}}
    void rootIdentity(const void* root,const void* scene) {std::ostringstream r,s;r<<root;s<<scene;m_rootIdentity=r.str();m_sceneIdentity=s.str();}
    unsigned stage() const {return m_stage;}
    bool failed() const {return !m_failure.empty();}
    bool begun() const {return m_begun;}
    unsigned stageFrame() const {return m_stageFrame;}
    void context(std::uint64_t epoch,const void* world,const std::string& directory,const std::string& worldId) {m_epoch=epoch;m_world=world;m_worldDirectory=directory;m_worldId=worldId;}
    void bounds(unsigned frame) const {
        const auto now=std::chrono::steady_clock::now();
        if(frame>=MaximumFrames || now-m_started>std::chrono::seconds(MaximumSeconds) || (m_begun && (frame-m_stageFrame>=StageMaximumFrames || now-m_stageStarted>std::chrono::seconds(StageMaximumSeconds))))throw std::runtime_error("Render lifecycle diagnostic deadline exceeded.");
        if(!m_failure.empty())throw std::runtime_error(m_failure);
    }
    void begin(unsigned frame,const std::string& snapshot) {bounds(frame);if(m_begun || m_stage>=Phases.size())throw std::runtime_error("Invalid lifecycle stage begin.");m_begun=true;m_stageFrame=frame;m_stageStarted=std::chrono::steady_clock::now();emit("begin",frame,snapshot);}
    void commit(unsigned frame,const std::string& snapshot) {bounds(frame);if(!m_begun)throw std::runtime_error("Lifecycle stage not begun.");emit("checkpoint",frame,snapshot);emit("end",frame,snapshot);m_begun=false;++m_stage;}
    void release(const char* owner,const std::string& payload,bool pass) {emit("release",m_lastFrame,"null",std::string("\"owner\":")+RenderLifecycle::quote(owner)+",\"release\":"+payload+",\"runtime_pass\":"+(pass?"true":"false"));if(!pass)m_failure="Lifecycle native/resource release failed.";}
    void observeFrame(unsigned frame){m_lastFrame=frame;bounds(frame);}
    void finish(){if(m_stage!=Phases.size() || m_begun || !m_failure.empty())throw std::runtime_error("Lifecycle incomplete at root shutdown.");summary("COMPLETE","");m_finished=true;}
    void fail(const std::string& reason,const std::string& snapshot="null") noexcept {m_failure=reason;try{emit("failure",m_lastFrame,snapshot,std::string("\"reason\":")+RenderLifecycle::quote(reason));summary("FAILED",reason);}catch(...){}}
private:
    void emit(const char* event,unsigned frame,const std::string& snapshot,const std::string& extra={}) {
        if(m_sequence>=MaximumJournalRecords)throw std::runtime_error("Lifecycle journal record budget exceeded.");
        const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-m_started).count();std::ostringstream address;address<<m_world;
        m_journal<<"{\"schema\":\"hellomine3d-render-lifecycle-journal-v1\",\"sequence\":"<<++m_sequence<<",\"event\":"<<RenderLifecycle::quote(event)<<",\"phase\":"<<RenderLifecycle::quote(m_stage<Phases.size()?Phases[m_stage]:"finished")<<",\"frame\":"<<frame<<",\"elapsed_ms\":"<<elapsed<<",\"normal_input\":false,\"world_epoch\":"<<m_epoch<<",\"world_instance\":"<<RenderLifecycle::quote(address.str())<<",\"world_directory\":"<<RenderLifecycle::quote(m_worldDirectory)<<",\"world_id\":"<<RenderLifecycle::quote(m_worldId)<<",\"snapshot\":"<<snapshot;if(!extra.empty())m_journal<<','<<extra;m_journal<<"}\n";m_journal.flush();if(!m_journal)throw std::runtime_error("Lifecycle journal write failed.");
        std::cout<<"[RENDER_LIFECYCLE] event="<<event<<" phase="<<(m_stage<Phases.size()?Phases[m_stage]:"finished")<<" frame="<<frame<<" normal_input=0\n";
    }
    void summary(const char* status,const std::string& reason) {
        std::ofstream out(std::filesystem::path(m_directory)/"summary.json");
        out<<"{\"schema\":\"hellomine3d-render-lifecycle-summary-v1\",\"status\":"<<RenderLifecycle::quote(status)
           <<",\"normal_input\":false,\"root_instance\":"<<RenderLifecycle::quote(m_rootIdentity)
           <<",\"scene_instance\":"<<RenderLifecycle::quote(m_sceneIdentity)<<",\"world_epoch\":"<<m_epoch
           <<",\"completed_phases\":"<<m_stage<<",\"maximum_frames\":"<<MaximumFrames
           <<",\"maximum_seconds\":"<<MaximumSeconds<<",\"maximum_journal_records\":"<<MaximumJournalRecords
           <<",\"stage_maximum_frames\":"<<StageMaximumFrames<<",\"stage_maximum_seconds\":"<<StageMaximumSeconds
           <<",\"reason\":"<<RenderLifecycle::quote(reason)<<"}\n";
        if(!out)throw std::runtime_error("Lifecycle summary write failed.");
    }
    std::string m_directory,m_failure,m_worldDirectory,m_worldId,m_rootIdentity,m_sceneIdentity;std::ofstream m_journal;
    std::chrono::steady_clock::time_point m_started,m_stageStarted;
    const void* m_world=nullptr;std::uint64_t m_epoch=0,m_sequence=0;
    unsigned m_stage=0,m_stageFrame=0,m_lastFrame=0;bool m_begun=false,m_finished=false;
};
