#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include "HdrPipeline.h"
#include "RenderLifecycleDiagnostics.h"
#include <Ogre.h>
#include <OgreDepthBuffer.h>
#include <OgreCompositorChain.h>
#include <GLSL/OgreGLSLShader.h>
#if defined(__APPLE__)
#include <OpenGL/OpenGL.h>
#endif
#include <GL/gl3w.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
namespace {
constexpr const char* Compositor = "HelloMine3D/LinearHdr";
constexpr std::size_t MaximumPixels = 3840u * 2160u;
constexpr const char* ScenePrograms[] = {
    "HelloMine3D/TerrainFragment", "HelloMine3D/TerrainArrayFragment",
    "HelloMine3D/TerrainShadowFragment", "HelloMine3D/TerrainShadowArrayFragment",
    "HelloMine3D/ActorFragment", "HelloMine3D/ActorShadowFragment",
    "HelloMine3D/WaterFragment", "HelloMine3D/SkyboxFragment",
    "HelloMine3D/BlockFeedbackFragment", "HelloMine3D/BlockFeedbackArrayFragment",
    "HelloMine3D/BlockParticleFragment", "HelloMine3D/BlockParticleArrayFragment",
    "HelloMine3D/CaveBoundaryFragment"};
bool forcedFallback()
{
    const char* value = std::getenv("HELLOMINE3D_HDR_FALLBACK");
    return value && std::string(value) == "1";
}
// Only a native storage probe; material/program errors are checked separately
// and never silently reclassified as an unsupported framebuffer.
bool floatProbe()
{
    Ogre::TexturePtr texture;
    const std::string name = "HelloMine3D/HdrFloatProbe";
    GLint draw = 0, read = 0, pack = 0, row = 0, rows = 0, pixels = 0, depthBinding = 0;
    GLboolean mask[4];
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
    glGetIntegerv(GL_PACK_ALIGNMENT, &pack); glGetIntegerv(GL_PACK_ROW_LENGTH, &row);
    glGetIntegerv(GL_PACK_SKIP_ROWS, &rows); glGetIntegerv(GL_PACK_SKIP_PIXELS, &pixels);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &depthBinding); glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    const GLboolean srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB), scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLuint depth = 0, fbo = 0;
    bool success = false;
    try {
        texture = Ogre::TextureManager::getSingleton().createManual(name,
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D,
            4, 4, 0, Ogre::PF_FLOAT16_RGBA, Ogre::TU_RENDERTARGET, nullptr, false);
        if (texture->getFormat() == Ogre::PF_FLOAT16_RGBA) {
            texture->getBuffer()->getRenderTarget()->getCustomAttribute("GL_FBOID", &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glGenRenderbuffers(1, &depth); glBindRenderbuffer(GL_RENDERBUFFER, depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 4, 4);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
                glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_SCISSOR_TEST); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                const GLfloat signal[] = {0.18f, 2.f, 8.f, 0.5f};
                glClearBufferfv(GL_COLOR, 0, signal);
                glReadBuffer(GL_COLOR_ATTACHMENT0);
                glPixelStorei(GL_PACK_ALIGNMENT, 1); glPixelStorei(GL_PACK_ROW_LENGTH, 0);
                glPixelStorei(GL_PACK_SKIP_ROWS, 0); glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
                std::array<GLfloat,4> actual{{-1,-1,-1,-1}};
                glReadPixels(1, 1, 1, 1, GL_RGBA, GL_FLOAT, actual.data());
                success = true;
                for (std::size_t i = 0; i < actual.size(); ++i)
                    success = success && std::isfinite(actual[i]) && std::abs(actual[i] - signal[i]) < .003f;
                std::cout << "[HDR_FLOAT_PROBE] format=RGBA16F depth=24 signal=" << actual[0] << ',' << actual[1] << ',' << actual[2] << ',' << actual[3] << " pass=" << success << '\n';
            }
        }
    } catch (const Ogre::Exception& error) {
        std::cout << "[HDR_FLOAT_PROBE] allocation_failed=" << error.getDescription() << '\n';
    }
    if (fbo) { glBindFramebuffer(GL_FRAMEBUFFER, fbo); glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0); }
    if (depth) glDeleteRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(depthBinding));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw)); glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read));
    glPixelStorei(GL_PACK_ALIGNMENT, pack); glPixelStorei(GL_PACK_ROW_LENGTH, row);
    glPixelStorei(GL_PACK_SKIP_ROWS, rows); glPixelStorei(GL_PACK_SKIP_PIXELS, pixels);
    glColorMask(mask[0],mask[1],mask[2],mask[3]);
    if (srgb) glEnable(GL_FRAMEBUFFER_SRGB); else glDisable(GL_FRAMEBUFFER_SRGB);
    if (scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    texture.setNull(); Ogre::TextureManager::getSingleton().remove(name);
    return success;
}

// Native attachment probes restore bindings and readback state. They allocate
// no extra full-size target; the only signal is cleared into the real scene.
struct NativeState {
    GLint draw=0, read=0, renderbuffer=0, readBuffer=0, packBuffer=0;
    GLint pack=0, row=0, rows=0, pixels=0;
    GLboolean mask[4]{}, srgb=GL_FALSE, scissor=GL_FALSE;
    NativeState() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
        glGetIntegerv(GL_RENDERBUFFER_BINDING,&renderbuffer);
        glGetIntegerv(GL_READ_BUFFER,&readBuffer); glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&packBuffer);
        glGetIntegerv(GL_PACK_ALIGNMENT,&pack); glGetIntegerv(GL_PACK_ROW_LENGTH,&row);
        glGetIntegerv(GL_PACK_SKIP_ROWS,&rows); glGetIntegerv(GL_PACK_SKIP_PIXELS,&pixels);
        glGetBooleanv(GL_COLOR_WRITEMASK,mask); srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB); scissor=glIsEnabled(GL_SCISSOR_TEST);
    }
    ~NativeState() {
        glBindRenderbuffer(GL_RENDERBUFFER,static_cast<GLuint>(renderbuffer));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,static_cast<GLuint>(draw));
        glBindFramebuffer(GL_READ_FRAMEBUFFER,static_cast<GLuint>(read)); glReadBuffer(static_cast<GLenum>(readBuffer));
        glBindBuffer(GL_PIXEL_PACK_BUFFER,static_cast<GLuint>(packBuffer));
        glPixelStorei(GL_PACK_ALIGNMENT,pack); glPixelStorei(GL_PACK_ROW_LENGTH,row);
        glPixelStorei(GL_PACK_SKIP_ROWS,rows); glPixelStorei(GL_PACK_SKIP_PIXELS,pixels);
        glColorMask(mask[0],mask[1],mask[2],mask[3]);
        if(srgb) glEnable(GL_FRAMEBUFFER_SRGB); else glDisable(GL_FRAMEBUFFER_SRGB);
        if(scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    }
};
std::size_t formatBytes(GLint format) {
    switch(format) {
    case GL_RGBA16F: return 8;
    case GL_DEPTH_COMPONENT16: return 2;
    case GL_DEPTH_COMPONENT24: case GL_DEPTH_COMPONENT32: case GL_DEPTH_COMPONENT32F:
    case GL_DEPTH24_STENCIL8: return 4;
    case GL_DEPTH32F_STENCIL8: return 8;
    case GL_STENCIL_INDEX1: case GL_STENCIL_INDEX4: case GL_STENCIL_INDEX8: return 1;
    case GL_STENCIL_INDEX16: return 2;
    default: return 0;
    }
}
struct Attachment {
    GLint kind=GL_NONE, object=0, format=0, width=0, height=0, samples=0;
};
Attachment attachment(GLenum slot) {
    Attachment result;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,slot,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,&result.kind);
    if(result.kind==GL_NONE) return result;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,slot,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&result.object);
    if(result.kind==GL_RENDERBUFFER) {
        glBindRenderbuffer(GL_RENDERBUFFER,static_cast<GLuint>(result.object));
        glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_INTERNAL_FORMAT,&result.format);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_WIDTH,&result.width);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_HEIGHT,&result.height);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_SAMPLES,&result.samples);
    } else if(result.kind==GL_TEXTURE) {
        GLint level=0; glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,slot,GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL,&level);
        const GLint previous = [] { GLint value=0; glGetIntegerv(GL_TEXTURE_BINDING_2D,&value); return value; }();
        glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(result.object));
        glGetTexLevelParameteriv(GL_TEXTURE_2D,level,GL_TEXTURE_INTERNAL_FORMAT,&result.format);
        glGetTexLevelParameteriv(GL_TEXTURE_2D,level,GL_TEXTURE_WIDTH,&result.width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D,level,GL_TEXTURE_HEIGHT,&result.height);
        glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(previous));
    }
    return result;
}
struct WindowStorage {
    GLint samples=0, sampleBuffers=0, red=0, green=0, blue=0, alpha=0, depth=0, stencil=0;
    bool multisampleEnabled=false;
    std::size_t colourBytes() const { return static_cast<std::size_t>(red+green+blue+alpha+7)/8; }
    std::size_t depthBytes() const {
        if(depth==24) return 4; // The sized depth24/packed24-8 storage is four bytes.
        return static_cast<std::size_t>(depth+stencil+7)/8;
    }
};
WindowStorage windowStorage() {
    NativeState saved; WindowStorage result;
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glGetIntegerv(GL_SAMPLE_BUFFERS,&result.sampleBuffers); glGetIntegerv(GL_SAMPLES,&result.samples);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_BACK_LEFT,GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,&result.red);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_BACK_LEFT,GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,&result.green);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_BACK_LEFT,GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE,&result.blue);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_BACK_LEFT,GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,&result.alpha);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_DEPTH,GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,&result.depth);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_STENCIL,GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,&result.stencil);
    result.multisampleEnabled=glIsEnabled(GL_MULTISAMPLE)==GL_TRUE;
    return result;
}
bool validateSceneStorage(Ogre::RenderTexture& target,unsigned width,unsigned height,bool msaa,std::size_t& depthBytes) {
    NativeState saved;
    GLuint resolve=0,multisample=0;
    target.getCustomAttribute("GL_FBOID",&resolve); target.getCustomAttribute("GL_MULTISAMPLEFBOID",&multisample);
    const GLuint draw=msaa?multisample:resolve;
    const GLint expected=msaa?4:0;
    bool valid=resolve && draw && (msaa || !multisample);
    glBindFramebuffer(GL_FRAMEBUFFER,resolve);
    valid=valid && glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    const auto resolved=attachment(GL_COLOR_ATTACHMENT0);
    valid=valid && resolved.kind==GL_TEXTURE && resolved.format==GL_RGBA16F &&
        resolved.width==static_cast<GLint>(width) && resolved.height==static_cast<GLint>(height) && resolved.samples==0;
    glBindFramebuffer(GL_FRAMEBUFFER,draw);
    valid=valid && glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    const auto colour=attachment(GL_COLOR_ATTACHMENT0),depth=attachment(GL_DEPTH_ATTACHMENT),stencil=attachment(GL_STENCIL_ATTACHMENT);
    const auto matches=[&](const Attachment& value) {
        return value.kind==GL_RENDERBUFFER && value.samples==expected && value.width==static_cast<GLint>(width) &&
            value.height==static_cast<GLint>(height) && formatBytes(value.format)!=0;
    };
    valid=valid && colour.format==GL_RGBA16F && colour.width==static_cast<GLint>(width) &&
        colour.height==static_cast<GLint>(height) && colour.samples==expected && matches(depth);
    if(msaa) valid=valid && colour.kind==GL_RENDERBUFFER;
    depthBytes=formatBytes(depth.format);
    if(stencil.kind!=GL_NONE) {
        valid=valid && matches(stencil);
        if(stencil.object!=depth.object) depthBytes+=formatBytes(stencil.format);
    }
    valid=valid && depthBytes>0 && depthBytes<=8;
    std::array<GLfloat,4> actual{{-1,-1,-1,-1}};
    if(valid) {
        glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_SCISSOR_TEST); glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        const GLfloat signal[]{.18f,2.f,8.f,.5f}; glClearBufferfv(GL_COLOR,0,signal);
        // Exercise the backend's actual production MSAA -> single-HDR blit.
        if(msaa) target.swapBuffers();
        glBindFramebuffer(GL_READ_FRAMEBUFFER,resolve); glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,0); glPixelStorei(GL_PACK_ALIGNMENT,1); glPixelStorei(GL_PACK_ROW_LENGTH,0);
        glPixelStorei(GL_PACK_SKIP_ROWS,0); glPixelStorei(GL_PACK_SKIP_PIXELS,0);
        glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,actual.data());
        for(std::size_t i=0;i<actual.size();++i) valid=valid && std::isfinite(actual[i]) && std::abs(actual[i]-signal[i])<.003f;
    }
    const GLenum error=glGetError(); valid=valid && error==GL_NO_ERROR;
    std::cout<<"[HDR_SCENE_STORAGE] msaa4="<<msaa<<" resolve_fbo="<<resolve<<" draw_fbo="<<draw
             <<" colour_samples="<<colour.samples<<" depth_samples="<<depth.samples<<" stencil_samples="<<stencil.samples
             <<" colour_format="<<colour.format<<" depth_format="<<depth.format<<" stencil_format="<<stencil.format
             <<" resolved_format="<<resolved.format<<" resolved_samples="<<resolved.samples
             <<" signal="<<actual[0]<<','<<actual[1]<<','<<actual[2]<<','<<actual[3]<<" gl_error="<<error<<" pass="<<valid<<'\n';
    return valid;
}

void checkProgram(const char* name, Ogre::GpuProgramType expectedStage)
{
    auto program = Ogre::HighLevelGpuProgramManager::getSingleton().getByName(name);
    if (program.isNull()) throw std::runtime_error(std::string("Missing HDR program: ") + name);
    if (program->getType() != expectedStage)
        throw std::runtime_error(std::string("Unexpected HDR shader stage: ") + name);
    program->load();
    if (program->hasCompileError() || !program->isSupported())
        throw std::runtime_error(std::string("Invalid HDR shader program: ") + name);
    // GLSL load() only preprocesses this backend's source. Compile the actual
    // shader object now, preserving Ogre's defines and vertex boilerplate, so
    // a bad asset cannot remain hidden behind forced/capability fallback.
    auto* shader = dynamic_cast<Ogre::GLSLShader*>(program.get());
    if (!shader)
        throw std::runtime_error(std::string("Unexpected HDR shader backend: ") + name);
    if (!shader->compile(true))
        throw std::runtime_error(std::string("Invalid HDR shader program: ") + name);
}
void checkResolveLink(Ogre::Pass& pass)
{
    if (!pass.hasVertexProgram() || !pass.hasFragmentProgram())
        throw std::runtime_error("Missing HDR resolve shader program stages.");
    auto vertex = pass.getVertexProgram();
    auto fragment = pass.getFragmentProgram();
    if (vertex.isNull() || fragment.isNull() ||
        vertex->getType() != Ogre::GPT_VERTEX_PROGRAM ||
        fragment->getType() != Ogre::GPT_FRAGMENT_PROGRAM)
        throw std::runtime_error("Invalid HDR resolve shader program stages.");
    auto* vs = dynamic_cast<Ogre::GLSLShader*>(vertex.get());
    auto* fs = dynamic_cast<Ogre::GLSLShader*>(fragment.get());
    if (!vs || !fs)
        throw std::runtime_error("HDR resolve requires the active GLSL backend.");
    GLint currentBefore = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentBefore);
    const GLenum errorBefore = glGetError();
    if (errorBefore != GL_NO_ERROR)
        throw std::runtime_error("HDR resolve link validation entered with OpenGL error: " +
                                 std::to_string(errorBefore));
    vertex->load(); fragment->load();
    if (!vs->compile(true) || !fs->compile(true) ||
        vertex->hasCompileError() || fragment->hasCompileError() ||
        !vertex->isSupported() || !fragment->isSupported())
        throw std::runtime_error("Invalid compiled HDR resolve shader resources.");
    struct TemporaryProgram
    {
        GLuint handle = glCreateProgram();
        ~TemporaryProgram() { if (handle) glDeleteProgram(handle); }
        void destroy() { glDeleteProgram(handle); handle = 0; }
    } certificate;
    if (!certificate.handle)
        throw std::runtime_error("Cannot create HDR resolve shader link certificate.");
    const GLuint handle = certificate.handle;
    GLint linked = 0;
    std::exception_ptr failure;
    try
    {
        // Link Ogre's actual compiled stages, including child libraries, without
        // binding a draw program or entering Ogre's persistent program cache.
        vs->attachToProgramObject(handle);
        fs->attachToProgramObject(handle);
        glLinkProgram(handle);
        glGetProgramiv(handle, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            std::array<char, 16384> log{};
            GLsizei written = 0;
            glGetProgramInfoLog(handle, static_cast<GLsizei>(log.size()), &written, log.data());
            throw std::runtime_error("Invalid linked HDR resolve shader resources (" +
                vertex->getName() + " + " + fragment->getName() + "): " +
                std::string(log.data(), static_cast<std::size_t>(std::max<GLsizei>(0, written))));
        }
    }
    catch (...) { failure = std::current_exception(); }
    // Deleting this never-bound program also detaches all attached child stages.
    // Ogre keeps ownership of the shaders; even a failed attachment/link drains.
    certificate.destroy();
    const bool deleted = glIsProgram(handle) == GL_FALSE;
    GLint currentAfter = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentAfter);
    const GLenum errorAfter = glGetError();
    std::cout << "[HDR_RESOLVE_LINK] vertex=" << vertex->getName()
              << " fragment=" << fragment->getName() << " temporary_program=" << handle
              << " linked=" << linked << " deleted=" << deleted
              << " current_program_before=" << currentBefore
              << " current_program_after=" << currentAfter
              << " gl_error_before=" << errorBefore << " gl_error_after=" << errorAfter << '\n';
    if (!deleted || currentBefore != currentAfter || errorAfter != GL_NO_ERROR)
        throw std::runtime_error("HDR resolve link validation did not preserve native program state.");
    if (failure) std::rethrow_exception(failure);
}
}
HdrPipeline::HdrPipeline() = default;
HdrPipeline::~HdrPipeline() { remove(); }
bool HdrPipeline::preferMsaa4(RenderPipeline requested) noexcept
{
    const char* value=std::getenv("HELLOMINE3D_MSAA4");
    // Prefer the bounded four-sample HDR route during ordinary rendering.
    // Exact zero explicitly selects the single-HDR comparison/fallback path.
    return requested==RenderPipeline::LinearHdr &&
        !(value && value[0]=='0' && value[1]=='\0');
}
bool HdrPipeline::msaa4WindowSupported() noexcept
{
#if defined(__APPLE__)
    // Config options advertise sample counts before a GL context exists. Check
    // the native pixel format so an unavailable window format is not selected.
    const CGLPixelFormatAttribute attributes[]{kCGLPFAOpenGLProfile,
        static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),kCGLPFANoRecovery,kCGLPFAAccelerated,
        kCGLPFADoubleBuffer,kCGLPFAColorSize,static_cast<CGLPixelFormatAttribute>(32),
        kCGLPFAAlphaSize,static_cast<CGLPixelFormatAttribute>(8),
        kCGLPFAStencilSize,static_cast<CGLPixelFormatAttribute>(8),kCGLPFADepthSize,static_cast<CGLPixelFormatAttribute>(16),kCGLPFAMultisample,
        kCGLPFASampleBuffers,static_cast<CGLPixelFormatAttribute>(1),kCGLPFASamples,
        static_cast<CGLPixelFormatAttribute>(4),static_cast<CGLPixelFormatAttribute>(0)};
    CGLPixelFormatObj format=nullptr; GLint count=0,samples=0,buffers=0;
    const auto status=CGLChoosePixelFormat(attributes,&format,&count);
    const bool valid=status==kCGLNoError && format && count>0 &&
        CGLDescribePixelFormat(format,0,kCGLPFASamples,&samples)==kCGLNoError && samples==4 &&
        CGLDescribePixelFormat(format,0,kCGLPFASampleBuffers,&buffers)==kCGLNoError && buffers==1;
    if(format) CGLDestroyPixelFormat(format);
    return valid;
#else
    return true; // The renderer's advertised format and subsequent native probe remain required.
#endif
}
void HdrPipeline::setLifecycleReleaseObserver(std::function<void(const char*,const std::string&,bool)> observer, bool delayedDrainFault)
{
    m_lifecycleObserver=std::move(observer);
    m_lifecycleDelayedDrainFault=bool(m_lifecycleObserver) && delayedDrainFault;
}
RenderLifecycleTargetFacts HdrPipeline::lifecycleFacts() const
{
    RenderLifecycleTargetFacts f;
    f.active=m_active; f.generation=m_targetGeneration; f.width=m_width; f.height=m_height;
    f.targetCount=m_sceneTarget?1u:0u; f.depthCount=m_sceneDepth?1u:0u; f.observerFailures=m_lifecycleObserverFailures;
    if(m_instance) {
        const auto texture=m_instance->getTextureInstance("scene",0);
        if(!texture.isNull()) f.textureName=texture->getName();
    }
    f.ownedDepthAttached=m_sceneTarget && m_sceneDepth && m_sceneTarget->getDepthBuffer()==m_sceneDepth.get();
    f.depthPool=m_sceneTarget?m_sceneTarget->getDepthBufferPool():0;
    f.native=RenderLifecycle::native(m_sceneTarget,m_msaa4Active);
    return f;
}
void HdrPipeline::remove() noexcept
{
    // Compiled compositor quad operations own material/TUS references to the
    // previous texture. Drain them at this outer frame/shutdown boundary before
    // allocating the replacement, not one draw later inside the engine.
    RenderLifecycleTargetFacts retired;
    const bool observe=bool(m_lifecycleObserver) && m_sceneTarget;
    if(observe) {try {retired=lifecycleFacts();} catch(...) {++m_lifecycleObserverFailures;}}
    const bool delayedDrain=observe && m_lifecycleDelayedDrainFault;
    // Consume the fault only at a real allocated target retirement. Empty
    // initialization/removal paths cannot consume it, and cleanup drains normally.
    if(delayedDrain) m_lifecycleDelayedDrainFault=false;
    bool drained=true;
    if (m_sceneTarget) { try { m_sceneTarget->detachDepthBuffer(); } catch (...) {drained=false;} }
    m_sceneDepth.reset(); m_sceneTarget=nullptr;
    if (m_viewport && m_instance) {
        try {
            auto& manager=Ogre::CompositorManager::getSingleton();
            manager.removeCompositor(m_viewport, Compositor);
            if(!delayedDrain) manager.getCompositorChain(m_viewport)->_compile();
        } catch (const Ogre::Exception& error) {
            drained=false;std::cerr<<"[HDR_RELEASE] compositor_drain_failed="<<error.getDescription()<<'\n';
        } catch (...) {drained=false;std::cerr<<"[HDR_RELEASE] compositor_drain_failed=unknown\n";}
    }
    // Viewport destruction does not clear Camera::mLastViewport. A complete
    // main draw normally restores it, but interrupted RTT rendering/cleanup
    // must also leave the camera bound to the still-live owning main viewport.
    auto* mainCamera=m_viewport?m_viewport->getCamera():nullptr;
    if(mainCamera) mainCamera->_notifyViewport(m_viewport);
    const bool cameraRestored=mainCamera && mainCamera->getViewport()==m_viewport;
    m_instance = nullptr; m_active = false; m_msaa4Active = false;
    if(observe) {
        try {
            const auto after=RenderLifecycle::retire(retired.native,{retired.textureName},{});
            m_lifecycleObserver("hdr-target",std::string("{\"before\":")+retired.json()+",\"after\":"+after.json+",\"compiled_operations_drained\":"+(drained && !delayedDrain?"true":"false")+",\"camera_viewport_restored\":"+(cameraRestored?"true":"false")+"}",after.pass && drained && cameraRestored && !m_lifecycleObserverFailures && retired.targetCount==1 && !retired.native.errorBefore && !retired.native.errorAfter);
        } catch(...) {++m_lifecycleObserverFailures;}
    }
}
void HdrPipeline::setSpatialAa(bool multisample)
{
    if(m_hasSpatialAa) {
        auto material=Ogre::MaterialManager::getSingleton().getByName("HelloMine3D/HdrResolve");
        material->getTechnique(0)->getPass(0)->getFragmentProgramParameters()->setNamedConstant(
            "spatialAaStrength",m_spatialAaRequested && !multisample?1.f:0.f);
    }
}
bool HdrPipeline::install()
{
    m_width=static_cast<unsigned>(std::max(0,m_viewport->getActualWidth()));
    m_height=static_cast<unsigned>(std::max(0,m_viewport->getActualHeight()));
    if(!m_width || !m_height || std::size_t(m_width)*m_height>MaximumPixels) {
        std::cout<<"[HDR_TARGET_BUDGET] requested="<<m_width<<'x'<<m_height
                 <<" maximum_pixels="<<MaximumPixels<<" allocated=0 allowed=0\n";
        return false;
    }
    GLint maximum=0; glGetIntegerv(GL_MAX_SAMPLES,&maximum);
    const auto window=windowStorage();
    const bool candidate=m_msaa4Preferred && maximum>=4 && m_viewport->getTarget()->getFSAA()==4 &&
        window.sampleBuffers>0 && window.samples==4 && window.multisampleEnabled;
    if(candidate && installTarget(true)) return true;
    if(m_msaa4Preferred) std::cout<<"[HDR_MSAA4] requested=1 active=0 fallback=single-HDR"
        <<" reason="<<(candidate?"native-attachment-or-resolve-unavailable":"window-or-sample-cap-unavailable")
        <<" window_metadata_samples="<<m_viewport->getTarget()->getFSAA()<<" window_native_samples="<<window.samples
        <<" max_samples="<<maximum<<'\n';
    return installTarget(false);
}
bool HdrPipeline::installTarget(bool multisample)
{
    const std::size_t pixels=std::size_t(m_width)*m_height;
    std::cout<<"[HDR_TARGET_BUDGET] requested="<<m_width<<'x'<<m_height<<" maximum_pixels="<<MaximumPixels
             <<" sample_limit="<<(multisample?4:1)<<" maximum_scene_format_bytes="<<pixels*(multisample?72u:16u)
             <<" allowed=1 history_bytes=0\n";
    auto definition=Ogre::CompositorManager::getSingleton().getByName(Compositor);
    if(definition.isNull() || definition->getNumTechniques()!=1) throw std::runtime_error("Invalid HDR compositor definition.");
    auto* scene=definition->getTechnique(0)->getTextureDefinition("scene");
    if(!scene || scene->formatList.size()!=1 || scene->formatList[0]!=Ogre::PF_FLOAT16_RGBA || scene->pooled)
        throw std::runtime_error("Invalid HDR scene texture definition.");
    // input previous at chain position zero inherits the main target's FSAA.
    // Fixed dimensions continue to prevent eager viewport resize allocation.
    scene->width=m_width; scene->height=m_height; scene->fsaa=multisample; scene->hwGammaWrite=false;
    scene->depthBufferId=Ogre::DepthBuffer::POOL_NO_DEPTH;
    setSpatialAa(multisample); // Set before Ogre clones the resolve material.
    NativeState saved;
    try {
        m_instance=Ogre::CompositorManager::getSingleton().addCompositor(m_viewport,Compositor,0);
        if(!m_instance) throw std::runtime_error("Missing HDR compositor instance.");
        m_instance->setEnabled(true);
        const auto texture=m_instance->getTextureInstance("scene",0);
        if(texture.isNull() || texture->getFormat()!=Ogre::PF_FLOAT16_RGBA || texture->isHardwareGammaEnabled() ||
            texture->getFSAA()!=(multisample?4u:0u)) { remove(); return false; }
        m_sceneTarget=texture->getBuffer()->getRenderTarget();
        m_sceneDepth.reset(m_renderer->_createDepthBufferFor(m_sceneTarget));
        if(!m_sceneDepth || !m_sceneTarget->attachDepthBuffer(m_sceneDepth.get())) { remove(); return false; }
        std::size_t depthBytes=0;
        if(!validateSceneStorage(*m_sceneTarget,m_width,m_height,multisample,depthBytes)) { remove(); return false; }
        const std::size_t samples=multisample?4u:1u;
        const auto window=windowStorage();
        const std::size_t windowPixels=std::size_t(m_viewport->getTarget()->getWidth())*m_viewport->getTarget()->getHeight();
        const auto windowSamples=static_cast<std::size_t>(std::max(1,window.samples));
        const auto windowColour=windowPixels*window.colourBytes()*windowSamples;
        const auto windowDepth=windowPixels*window.depthBytes()*windowSamples;
        const auto windowExtra=windowPixels*(window.colourBytes()+window.depthBytes())*(windowSamples-1);
        const auto sceneMultisampleColour=multisample?pixels*8u*4u:0u;
        const auto sceneResolved=pixels*8u,sceneDepth=pixels*depthBytes*samples;
        m_active=true; m_msaa4Active=multisample; ++m_targetGeneration;
        std::cout<<"[HDR_STORAGE_BUDGET] domain=format-logical-bytes scene_multisample_colour_bytes="<<sceneMultisampleColour
                 <<" scene_depth_stencil_bytes="<<sceneDepth<<" scene_single_or_resolved_colour_bytes="<<sceneResolved
                 <<" scene_total_bytes="<<sceneMultisampleColour+sceneDepth+sceneResolved
                 <<" scene_extra_vs_single_hdr_bytes="<<(multisample?pixels*(32u+depthBytes*3u):0u)
                 <<" window_back_colour_bytes="<<windowColour<<" window_depth_stencil_bytes="<<windowDepth
                 <<" window_extra_samples_bytes="<<windowExtra<<" window_samples="<<window.samples
                 <<" window_colour_bits="<<window.red<<','<<window.green<<','<<window.blue<<','<<window.alpha
                 <<" window_depth_stencil_bits="<<window.depth<<','<<window.stencil
                 <<" accounted_scene_and_window_back_bytes="<<sceneMultisampleColour+sceneDepth+sceneResolved+windowColour+windowDepth
                 <<" window_front_and_present_driver_owned=1 physical_vram_usage=not_measured history_bytes=0\n";
        std::cout<<"[HDR_PIPELINE] requested=linear-hdr active=linear-hdr fallback=0 size="<<m_width<<'x'<<m_height
                 <<" colour_bytes="<<sceneResolved<<" msaa4="<<multisample<<" texture_fsaa="<<texture->getFSAA()
                 <<" target_fsaa="<<m_sceneTarget->getFSAA()<<" depth_pool=none exposure=1 fixed=1 optional_post=LDR\n";
        std::cout<<"[HDR_SPATIAL_AA] requested="<<m_spatialAaRequested<<" available="<<m_hasSpatialAa
                 <<" enabled="<<(m_hasSpatialAa && m_spatialAaRequested && !multisample)
                 <<" maximum_samples="<<(m_hasSpatialAa && m_spatialAaRequested && !multisample?9:1)
                 <<" extra_targets=0 history_bytes=0\n";
        return true;
    } catch(const Ogre::Exception& error) {
        remove(); std::cout<<"[HDR_PIPELINE] target_failed="<<error.getDescription()<<'\n'; return false;
    }
}
void HdrPipeline::initialize(Ogre::Viewport& viewport, Ogre::RenderSystem& renderer, RenderPipeline requested)
{
    m_viewport = &viewport; m_renderer = &renderer;
    m_msaa4Preferred = preferMsaa4(requested);
    m_requested = requested;
    if (requested != RenderPipeline::LinearHdr) {
        std::cout << "[HDR_PIPELINE] requested=legacy active=legacy fallback=0\n"; return;
    }
    // Broken assets/shaders fail explicitly before framebuffer capability tests.
    checkProgram("HelloMine3D/HdrResolveFragment", Ogre::GPT_FRAGMENT_PROGRAM);
    checkProgram("HelloMine3D/PostProcessVertex", Ogre::GPT_VERTEX_PROGRAM);
    for (const auto* name : ScenePrograms) checkProgram(name, Ogre::GPT_FRAGMENT_PROGRAM);
    // Declared surface programs are assets too. Validate before the storage
    // fallback so malformed PBR code cannot hide behind an unsupported HDR FBO.
    // A complete older program resource may omit both new variants.
    const bool hasSurface = !Ogre::HighLevelGpuProgramManager::getSingleton()
        .getByName("HelloMine3D/TerrainSurfaceFragment").isNull();
    const bool hasSurfaceShadow = !Ogre::HighLevelGpuProgramManager::getSingleton()
        .getByName("HelloMine3D/TerrainShadowSurfaceFragment").isNull();
    if (hasSurface != hasSurfaceShadow)
        throw std::runtime_error("Surface shader resource must declare both receiver variants.");
    if (hasSurface)
        for (const char* name : {"HelloMine3D/TerrainSurfaceFragment",
                                 "HelloMine3D/TerrainShadowSurfaceFragment"})
            checkProgram(name, Ogre::GPT_FRAGMENT_PROGRAM);
    auto material = Ogre::MaterialManager::getSingleton().getByName("HelloMine3D/HdrResolve");
    if (material.isNull()) throw std::runtime_error("Missing HDR resolve material.");
    material->load();
    if (!material->getNumTechniques() || !material->getTechnique(0)->getNumPasses())
        throw std::runtime_error("Missing HDR resolve material pass.");
    auto* resolvePass = material->getTechnique(0)->getPass(0);
    checkResolveLink(*resolvePass);
    auto resolveParameters = resolvePass->getFragmentProgramParameters();
    const char* aaOff = std::getenv("HELLOMINE3D_SPATIAL_AA_OFF");
    m_spatialAaRequested = !(aaOff && std::string(aaOff) == "1");
    m_hasSpatialAa = resolveParameters->_findNamedConstantDefinition("spatialAaStrength", false) != nullptr;
    if (m_hasSpatialAa)
    {
        if (!resolveParameters->_findNamedConstantDefinition("inverseTextureSize", false))
            throw std::runtime_error("Spatial HDR resolve is missing its pixel-size interface.");
        const auto* pixelSize = resolveParameters->findAutoConstantEntry("inverseTextureSize");
        if (!pixelSize || pixelSize->paramType != Ogre::GpuProgramParameters::ACT_INVERSE_TEXTURE_SIZE ||
            pixelSize->data != 0 || pixelSize->elementCount < 2)
            throw std::runtime_error("Spatial HDR resolve must bind the actual scene texture pixel size.");
        resolveParameters->setNamedConstant("spatialAaStrength", m_spatialAaRequested ? 1.f : 0.f);
    }
    if (Ogre::CompositorManager::getSingleton().getByName(Compositor).isNull())
        throw std::runtime_error("Missing HDR compositor definition.");
    const auto* caps = renderer.getCapabilities();
    const bool supported = caps && caps->hasCapability(Ogre::RSC_HWRENDER_TO_TEXTURE) &&
        caps->hasCapability(Ogre::RSC_TEXTURE_FLOAT) && caps->hasCapability(Ogre::RSC_FRAGMENT_PROGRAM) &&
        Ogre::GpuProgramManager::getSingleton().isSyntaxSupported("glsl150");
    m_storageSupported = !forcedFallback() && supported && floatProbe();
    if (!m_storageSupported || !install()) {
        remove();
        std::cout << "[HDR_PIPELINE] requested=linear-hdr active=legacy fallback=1 reason="
                  << (forcedFallback() ? "forced" : !supported ? "unsupported-capability" : "float-target-unavailable") << '\n';
    }
}
void HdrPipeline::beforeFrame()
{
    if (m_requested != RenderPipeline::LinearHdr || !m_storageSupported) return;
    const unsigned width = static_cast<unsigned>(std::max(0, m_viewport->getActualWidth()));
    const unsigned height = static_cast<unsigned>(std::max(0, m_viewport->getActualHeight()));
    if (m_width == width && m_height == height) return;
    remove();
    if (!install()) std::cout << "[HDR_PIPELINE] requested=linear-hdr active=legacy fallback=1 reason=resize-target-unavailable\n";
    applySceneParameters();
}
void HdrPipeline::applySceneParameters() const
{
    // SceneManager clones six sky materials, and CaveBoundary owns a clone.
    // Bind by shader interface so a resize fallback also updates those copies.
    auto materials = Ogre::MaterialManager::getSingleton().getResourceIterator();
    while (materials.hasMoreElements()) {
        auto* material = static_cast<Ogre::Material*>(materials.getNext().get());
        for (unsigned short t = 0; t < material->getNumTechniques(); ++t) {
            auto* technique = material->getTechnique(t);
            for (unsigned short p = 0; p < technique->getNumPasses(); ++p) {
                auto* pass = technique->getPass(p);
                if (!pass->hasFragmentProgram()) continue;
                const auto& program = pass->getFragmentProgramName();
                const bool sceneProgram = std::any_of(std::begin(ScenePrograms), std::end(ScenePrograms),
                    [&](const char* name) { return program == name; });
                const bool surfaceProgram = program == "HelloMine3D/TerrainSurfaceFragment" ||
                    program == "HelloMine3D/TerrainShadowSurfaceFragment";
                if (!sceneProgram && !surfaceProgram) continue;
                auto parameters = pass->getFragmentProgramParameters();
                if (!parameters->_findNamedConstantDefinition("linearHdrMode", false)) {
                    if (m_active) throw std::runtime_error(
                        "Scene shader missing HDR colour interface: " + material->getName());
                    continue;
                }
                parameters->setNamedConstant("linearHdrMode", m_active ? 1.f : 0.f);
            }
        }
    }
}
