#include "HdrPipeline.h"
#include <Ogre.h>
#include <GLSL/OgreGLSLShader.h>
#include <GL/gl3w.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
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
}
HdrPipeline::~HdrPipeline() { remove(); }
void HdrPipeline::remove() noexcept
{
    if (m_viewport && m_instance) {
        try { Ogre::CompositorManager::getSingleton().removeCompositor(m_viewport, Compositor); } catch (...) {}
    }
    m_instance = nullptr; m_active = false;
}
bool HdrPipeline::install()
{
    m_width = static_cast<unsigned>(std::max(0, m_viewport->getActualWidth()));
    m_height = static_cast<unsigned>(std::max(0, m_viewport->getActualHeight()));
    if (!m_width || !m_height || std::size_t(m_width) * m_height > MaximumPixels) {
        std::cout << "[HDR_TARGET_BUDGET] requested=" << m_width << 'x' << m_height
                  << " maximum_pixels=" << MaximumPixels << " allocated=0 allowed=0\n";
        return false;
    }
    // Viewport::_updateDimensions can notify compositor resize immediately,
    // before beforeFrame runs. Fixed definitions prevent that eager allocation.
    auto definition = Ogre::CompositorManager::getSingleton().getByName(Compositor);
    if (definition.isNull() || definition->getNumTechniques() != 1)
        throw std::runtime_error("Invalid HDR compositor definition.");
    auto* scene = definition->getTechnique(0)->getTextureDefinition("scene");
    if (!scene) throw std::runtime_error("Missing HDR scene texture definition.");
    scene->width = m_width; scene->height = m_height;
    scene->fsaa = false; scene->hwGammaWrite = false;
    try {
        m_instance = Ogre::CompositorManager::getSingleton().addCompositor(m_viewport, Compositor, 0);
        if (!m_instance) throw std::runtime_error("Missing HDR compositor instance.");
        m_instance->setEnabled(true);
        const auto texture = m_instance->getTextureInstance("scene", 0);
        if (texture.isNull() || texture->getFormat() != Ogre::PF_FLOAT16_RGBA || texture->isHardwareGammaEnabled()) {
            remove(); return false;
        }
        m_active = true;
        std::cout << "[HDR_PIPELINE] requested=linear-hdr active=linear-hdr fallback=0 size="
                  << m_width << 'x' << m_height << " colour_bytes=" << std::size_t(m_width) * m_height * 8
                  << " exposure=1 fixed=1 optional_post=LDR\n";
        return true;
    } catch (const Ogre::Exception& error) {
        remove(); std::cout << "[HDR_PIPELINE] target_failed=" << error.getDescription() << '\n'; return false;
    }
}
void HdrPipeline::initialize(Ogre::Viewport& viewport, Ogre::RenderSystem& renderer, RenderPipeline requested)
{
    m_viewport = &viewport;
    m_requested = requested;
    if (requested != RenderPipeline::LinearHdr) {
        std::cout << "[HDR_PIPELINE] requested=legacy active=legacy fallback=0\n"; return;
    }
    // Broken assets/shaders fail explicitly before framebuffer capability tests.
    checkProgram("HelloMine3D/HdrResolveFragment", Ogre::GPT_FRAGMENT_PROGRAM);
    checkProgram("HelloMine3D/PostProcessVertex", Ogre::GPT_VERTEX_PROGRAM);
    for (const auto* name : ScenePrograms) checkProgram(name, Ogre::GPT_FRAGMENT_PROGRAM);
    auto material = Ogre::MaterialManager::getSingleton().getByName("HelloMine3D/HdrResolve");
    if (material.isNull()) throw std::runtime_error("Missing HDR resolve material.");
    material->load();
    if (Ogre::CompositorManager::getSingleton().getByName(Compositor).isNull())
        throw std::runtime_error("Missing HDR compositor definition.");
    const auto* caps = renderer.getCapabilities();
    const bool supported = caps && caps->hasCapability(Ogre::RSC_HWRENDER_TO_TEXTURE) &&
        caps->hasCapability(Ogre::RSC_TEXTURE_FLOAT) && caps->hasCapability(Ogre::RSC_FRAGMENT_PROGRAM) &&
        Ogre::GpuProgramManager::getSingleton().isSyntaxSupported("glsl150");
    if (forcedFallback() || !supported || !floatProbe() || !install()) {
        remove();
        std::cout << "[HDR_PIPELINE] requested=linear-hdr active=legacy fallback=1 reason="
                  << (forcedFallback() ? "forced" : !supported ? "unsupported-capability" : "float-target-unavailable") << '\n';
    }
}
void HdrPipeline::beforeFrame()
{
    if (!m_active) return;
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
                if (!sceneProgram) continue;
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
