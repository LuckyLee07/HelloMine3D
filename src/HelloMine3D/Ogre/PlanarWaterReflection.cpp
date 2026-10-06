#include "PlanarWaterReflection.h"
#include "RenderLifecycleDiagnostics.h"

#include <Ogre.h>
#include <OgreGL3PlusDepthBuffer.h>
#include <OgreGL3PlusHardwarePixelBuffer.h>
#include <GL/gl3w.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
constexpr const char* ReflectionUnit = "planarReflection";
constexpr const char* WaterProgram = "HelloMine3D/WaterFragment";

float decode(float c)
{
    c = std::max(c, 0.f);
    return c <= .04045f ? c / 12.92f : std::pow((c + .055f) / 1.055f, 2.4f);
}
bool finite(const Ogre::Matrix4& matrix)
{
    for (unsigned r = 0; r < 4; ++r)
        for (unsigned c = 0; c < 4; ++c)
            if (!std::isfinite(matrix[r][c]) || std::abs(matrix[r][c]) > 1.e8f) return false;
    return true;
}
void setIfPresent(Ogre::GpuProgramParameters& p, const char* name, float value)
{
    if (p._findNamedConstantDefinition(name, false)) p.setNamedConstant(name, value);
}
void setIfPresent(Ogre::GpuProgramParameters& p, const char* name, const Ogre::Vector3& value)
{
    if (p._findNamedConstantDefinition(name, false)) p.setNamedConstant(name, value);
}
// Allocation/attachment in this Ogre backend changes native FBO bindings. Keep
// them intact, including on a rejected target; no RenderSystem global toggle.
struct NativeState {
    GLint draw = 0, read = 0, renderbuffer = 0, pack = 0, row = 0, rows = 0, pixels = 0, readBuffer = 0, pixelPackBuffer = 0;
    GLboolean mask[4] = {}, srgb = GL_FALSE, scissor = GL_FALSE;
    NativeState()
    {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pixelPackBuffer);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer); glGetIntegerv(GL_READ_BUFFER, &readBuffer);
        glGetIntegerv(GL_PACK_ALIGNMENT, &pack); glGetIntegerv(GL_PACK_ROW_LENGTH, &row);
        glGetIntegerv(GL_PACK_SKIP_ROWS, &rows); glGetIntegerv(GL_PACK_SKIP_PIXELS, &pixels);
        glGetBooleanv(GL_COLOR_WRITEMASK, mask);
        srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB); scissor = glIsEnabled(GL_SCISSOR_TEST);
    }
    ~NativeState()
    {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(pixelPackBuffer));
        glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbuffer));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read)); glReadBuffer(static_cast<GLenum>(readBuffer));
        glPixelStorei(GL_PACK_ALIGNMENT, pack); glPixelStorei(GL_PACK_ROW_LENGTH, row);
        glPixelStorei(GL_PACK_SKIP_ROWS, rows); glPixelStorei(GL_PACK_SKIP_PIXELS, pixels);
        glColorMask(mask[0], mask[1], mask[2], mask[3]);
        if (srgb) glEnable(GL_FRAMEBUFFER_SRGB); else glDisable(GL_FRAMEBUFFER_SRGB);
        if (scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    }
};
std::size_t storageBytes(GLenum format)
{
    switch (format) {
    case GL_DEPTH_COMPONENT16: return 2;
    case GL_DEPTH_COMPONENT24: case GL_DEPTH24_STENCIL8:
    case GL_DEPTH_COMPONENT32: case GL_DEPTH_COMPONENT32F: return 4;
    case GL_DEPTH32F_STENCIL8: return 8;
    case GL_STENCIL_INDEX1: case GL_STENCIL_INDEX4: case GL_STENCIL_INDEX8: return 1;
    case GL_STENCIL_INDEX16: return 2;
    default: return 0;
    }
}
}

struct PlanarWaterReflection::Impl final : Ogre::RenderQueue::RenderableListener, Ogre::SceneManager::Listener, Ogre::RenderTargetListener {
    struct PrivateMaterial {
        Ogre::MaterialPtr material;
        Ogre::Technique* technique = nullptr;
        std::uint64_t preparedFrame = 0;
        bool prepared = false;
    };
    Ogre::SceneManager* scene = nullptr;
    Ogre::RenderSystem* renderer = nullptr;
    Ogre::Camera* camera = nullptr;
    Ogre::RenderTexture* target = nullptr;
    Ogre::Viewport* viewport = nullptr;
    Ogre::TexturePtr texture;
    // Deliberately not registered in Ogre's shared depth pool. Otherwise every
    // distinct resize can retain another depth buffer until engine shutdown.
    std::unique_ptr<Ogre::DepthBuffer> depth;
    Ogre::Pass* boundWaterPass = nullptr;
    Ogre::RenderQueue::RenderableListener* previousListener = nullptr;
    std::map<std::pair<Ogre::ResourceHandle, const Ogre::Technique*>, PrivateMaterial> materials;
    std::vector<Ogre::RenderTarget*> observedShadowTargets;
    Ogre::Matrix4 viewProjection = Ogre::Matrix4::IDENTITY;
    FrameInput input;
    Statistics stats;
    std::string prefix;
    float planeY = 0;
    bool selected = false, supported = false, rendering = false, submitted = false;
    std::uint64_t lastFrame = 0, materialSerial = 0;
    unsigned diagnosticCaptures = 0;
    std::uint64_t targetGeneration = 0;
    unsigned lifecycleObserverFailures = 0;
    std::function<void(const char*,const std::string&,bool)> lifecycleObserver;

    RenderLifecycleTargetFacts facts() const
    {
        RenderLifecycleTargetFacts f; f.active=stats.active; f.generation=targetGeneration; f.updateCount=stats.updateCount;
        f.width=stats.width; f.height=stats.height; f.targetCount=target?1u:0u; f.depthCount=depth?1u:0u;
        f.cameraCount=camera?1u:0u; f.cameraName=camera?camera->getName():std::string();
        f.selected=selected; f.binderBound=bool(input.bindViewParameters); f.waterSamplerBound=boundWaterPass!=nullptr;
        f.lodCameraBound=camera && camera->getLodCamera()!=camera; f.listenersActive=rendering || !observedShadowTargets.empty();
        f.privateMaterials=materials.size(); f.privatePasses=stats.privatePasses; f.observerFailures=lifecycleObserverFailures;
        if(!texture.isNull())f.textureName=texture->getName();
        for(const auto& entry:materials)f.materialNames.push_back(entry.second.material->getName());
        f.ownedDepthAttached=target && depth && target->getDepthBuffer()==depth.get();
        f.depthPool=target?target->getDepthBufferPool():0;
        f.native=RenderLifecycle::native(target,false);return f;
    }

    void status(const char* reason)
    {
        stats.active = false;
        if (stats.reason != reason) {
            stats.reason = reason;
            std::cout << "[PLANAR_REFLECTION] active=0 reason=" << reason << '\n';
        }
    }
    void unbindWater() noexcept
    {
        if (!boundWaterPass) return;
        try {
            if (boundWaterPass->hasFragmentProgram())
                setIfPresent(*boundWaterPass->getFragmentProgramParameters(), "planarReflectionEnabled", 0.f);
            auto* unit = boundWaterPass->getTextureUnitState(ReflectionUnit);
            if (unit) boundWaterPass->removeTextureUnitState(boundWaterPass->getTextureUnitStateIndex(unit));
        } catch (...) {}
        boundWaterPass = nullptr;
    }
    void releaseTarget() noexcept
    {
        RenderLifecycleTargetFacts retired; const bool observe=bool(lifecycleObserver) && target;
        if(observe){try{retired=facts();}catch(...){++lifecycleObserverFailures;}}
        unbindWater();
        try { if (target) { target->removeAllViewports(); target->detachDepthBuffer(); } } catch (...) {}
        if (camera) camera->_notifyViewport(nullptr);
        viewport = nullptr; target = nullptr; depth.reset();
        try {
            if (!texture.isNull() && Ogre::TextureManager::getSingletonPtr())
                Ogre::TextureManager::getSingleton().remove(texture->getName());
        } catch (...) {}
        texture.setNull();
        stats.width = stats.height = 0; stats.colourBytes = stats.depthStencilBytes = 0;
        if(observe){try{const auto after=RenderLifecycle::retire(retired.native,{retired.textureName},{});lifecycleObserver("planar-target",std::string("{\"before\":")+retired.json()+",\"after\":"+after.json+"}",after.pass && !lifecycleObserverFailures);}catch(...){++lifecycleObserverFailures;}}
    }
    void clearMaterials() noexcept
    {
        RenderLifecycleTargetFacts retired; const bool observe=bool(lifecycleObserver) && !materials.empty();
        if(observe){try{retired=facts();}catch(...){++lifecycleObserverFailures;}}
        for (auto& entry : materials) {
            try { if (Ogre::MaterialManager::getSingletonPtr())
                Ogre::MaterialManager::getSingleton().remove(entry.second.material->getName()); } catch (...) {}
        }
        materials.clear(); stats.privateMaterials = stats.privatePasses = 0;
        if(observe){try{const auto after=RenderLifecycle::retire({}, {},retired.materialNames);lifecycleObserver("planar-materials",std::string("{\"before\":")+retired.json()+",\"after\":"+after.json+"}",after.pass && !lifecycleObserverFailures);}catch(...){++lifecycleObserverFailures;}}
    }
    bool makeTarget(TargetSize size)
    {
        NativeState saved;
        releaseTarget();
        try {
            texture = Ogre::TextureManager::getSingleton().createManual(prefix + "/Colour",
                Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D,
                size.width, size.height, 0, Ogre::PF_FLOAT16_RGBA, Ogre::TU_RENDERTARGET, nullptr, false, 0);
            target = texture->getBuffer()->getRenderTarget();
            target->setAutoUpdated(false);
            target->setDepthBufferPool(Ogre::DepthBuffer::POOL_NO_DEPTH);
            depth.reset(renderer->_createDepthBufferFor(target));
            if (texture->getFormat() != Ogre::PF_FLOAT16_RGBA || texture->isHardwareGammaEnabled() ||
                !depth || !target->attachDepthBuffer(depth.get())) {
                releaseTarget(); return false;
            }
            auto* nativeDepth = dynamic_cast<Ogre::GL3PlusDepthBuffer*>(depth.get());
            if (!nativeDepth || !nativeDepth->getDepthBuffer()) { releaseTarget(); return false; }
            std::size_t depthBytes = storageBytes(nativeDepth->getDepthBuffer()->getGLFormat());
            auto* stencil = nativeDepth->getStencilBuffer();
            if (stencil && stencil != nativeDepth->getDepthBuffer()) depthBytes += storageBytes(stencil->getGLFormat());
            if (!depthBytes) { releaseTarget(); return false; }

            // Probe this actual production target, with its actual owned depth
            // attachment, before its first scene update. No extra probe RTT.
            GLuint fbo = 0; target->getCustomAttribute("GL_FBOID", &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            if (!fbo || glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                releaseTarget(); return false;
            }
            glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_SCISSOR_TEST); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            const GLfloat signal[] = { .18f, 2.f, 8.f, .5f };
            glClearBufferfv(GL_COLOR, 0, signal); glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            glPixelStorei(GL_PACK_ALIGNMENT, 1); glPixelStorei(GL_PACK_ROW_LENGTH, 0);
            glPixelStorei(GL_PACK_SKIP_ROWS, 0); glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
            std::array<GLfloat, 4> actual{{ -1, -1, -1, -1 }};
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, actual.data());
            bool valid = true;
            for (std::size_t i = 0; i < actual.size(); ++i)
                valid = valid && std::isfinite(actual[i]) && std::abs(actual[i] - signal[i]) < .003f;
            std::cout << "[PLANAR_REFLECTION_STORAGE] size=" << size.width << 'x' << size.height
                      << " signal=" << actual[0] << ',' << actual[1] << ',' << actual[2] << ',' << actual[3]
                      << " colour_bytes=" << size.pixels() * 8 << " depth_stencil_bytes=" << size.pixels() * depthBytes
                      << " pass=" << valid << '\n';
            if (!valid) { releaseTarget(); return false; }
            viewport = target->addViewport(camera);
            viewport->setOverlaysEnabled(false); viewport->setShadowsEnabled(true);
            viewport->setClearEveryFrame(true, Ogre::FBT_COLOUR | Ogre::FBT_DEPTH);
            stats.width = size.width; stats.height = size.height;
            stats.colourBytes = static_cast<std::size_t>(size.pixels() * 8);
            stats.depthStencilBytes = static_cast<std::size_t>(size.pixels() * depthBytes);
            ++targetGeneration;
            return true;
        } catch (const Ogre::Exception& error) {
            std::cout << "[PLANAR_REFLECTION_STORAGE] allocation_failed=" << error.getDescription() << '\n';
            releaseTarget(); return false;
        }
    }
    bool renderableQueued(Ogre::Renderable* rend, Ogre::uint8 group, Ogre::ushort priority,
                          Ogre::Technique** technique, Ogre::RenderQueue* queue) override
    {
        const auto& source = rend->getMaterial();
        const Ogre::String name = source.isNull() ? "" : source->getName();
        bool water = name == "HelloMine3D/Water";
        if (*technique) for (unsigned short p = 0; p < (*technique)->getNumPasses(); ++p) {
            auto* pass = (*technique)->getPass(p);
            water = water || (pass->hasFragmentProgram() && pass->getFragmentProgramName() == WaterProgram);
        }
        if (water || excludes(name, group)) {
            if (water) ++stats.rejectedWater; else ++stats.rejectedFeedback;
            return false;
        }
        if (previousListener && !previousListener->renderableQueued(rend, group, priority, technique, queue)) return false;
        // Nested shadow cameras use the engine's normal depth-caster technique.
        // Never replace their passes with reflection colour/environment params.
        if (scene->getCurrentViewport() != viewport || scene->_getCurrentRenderStage() == Ogre::SceneManager::IRS_RENDER_TO_TEXTURE)
            return true;
        if (!*technique || source.isNull()) throw std::runtime_error("Reflection resident has no material technique.");
        const auto key = std::make_pair(source->getHandle(), static_cast<const Ogre::Technique*>(*technique));
        auto found = materials.find(key);
        const auto count = (*technique)->getNumPasses();
        if (found == materials.end()) {
            if (materials.size() >= MaximumPrivateMaterials || stats.privatePasses + count > MaximumPrivatePasses)
                throw std::runtime_error("Planar reflection private material/pass budget exceeded.");
            PrivateMaterial owned;
            owned.material = Ogre::MaterialManager::getSingleton().create(prefix + "/Material/" + std::to_string(++materialSerial),
                Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
            owned.technique = owned.material->createTechnique();
            *owned.technique = **technique;
            owned.material->load();
            found = materials.emplace(key, std::move(owned)).first;
            stats.privateMaterials = materials.size(); stats.privatePasses += count;
        }
        auto& owned = found->second;
        if (!owned.prepared || owned.preparedFrame != input.frameSerial) {
            if (owned.technique->getNumPasses() != count)
                throw std::runtime_error("Reflection source technique changed pass count; resetWorld required.");
            for (unsigned short p = 0; p < count; ++p) {
                auto* pass = owned.technique->getPass(p);
                // Pass assignment deep-copies GpuProgramUsage/parameters and
                // texture states. Matrices remain automatic for this camera.
                *pass = *(*technique)->getPass(p);
                if (input.bindViewParameters) input.bindViewParameters(name, *pass, *camera);
                if (pass->hasFragmentProgram()) {
                    auto params = pass->getFragmentProgramParameters();
                    setIfPresent(*params, "linearHdrMode", 1.f);
                    setIfPresent(*params, "cameraPosition", camera->getDerivedPosition());
                    setIfPresent(*params, "planarReflectionEnabled", 0.f);
                }
            }
            owned.preparedFrame = input.frameSerial; owned.prepared = true;
        }
        *technique = owned.technique;
        return true;
    }
    void shadowTextureCasterPreViewProj(Ogre::Light*, Ogre::Camera* shadowCamera, std::size_t) override
    {
        // This callback precedes an actual target update. The engine's
        // shadowTexturesUpdated argument counts lights rather than cascades
        // and can include lights that did not cast, so do not use it as a
        // draw/update measurement.
        auto* view = shadowCamera->getViewport();
        if (!view) return;
        auto* shadowTarget = view->getTarget();
        if (std::find(observedShadowTargets.begin(), observedShadowTargets.end(), shadowTarget) == observedShadowTargets.end()) {
            observedShadowTargets.push_back(shadowTarget);
            shadowTarget->addListener(this);
        }
    }
    void postRenderTargetUpdate(const Ogre::RenderTargetEvent& event) override
    {
        ++stats.shadowUpdates;
        stats.shadowBatches += event.source->getBatchCount();
    }
};

PlanarWaterReflection::TargetSize PlanarWaterReflection::targetSize(int width, int height) noexcept
{
    if (width <= 0 || height <= 0) return {};
    TargetSize size{ (static_cast<unsigned>(width) + 1u) / 2u, (static_cast<unsigned>(height) + 1u) / 2u };
    return size.pixels() <= MaximumPixels ? size : TargetSize{};
}
bool PlanarWaterReflection::excludes(const std::string& name, unsigned group) noexcept
{
    return group >= Ogre::RENDER_QUEUE_OVERLAY || name == "HelloMine3D/Water" ||
        name == "HelloMine3D/BlockSurfaceFeedback" || name == "HelloMine3D/FloraSurfaceFeedback" ||
        name == "HelloMine3D/BlockFragments";
}
PlanarWaterReflection::PlanarWaterReflection() : m_impl(new Impl) {}
PlanarWaterReflection::~PlanarWaterReflection()
{
    resetWorld();
    if (m_impl->scene && m_impl->camera) {
        const auto name=m_impl->camera->getName();
        m_impl->scene->destroyCamera(m_impl->camera); m_impl->camera=nullptr;
        if(m_impl->lifecycleObserver){try{const bool alive=m_impl->scene->hasCamera(name);m_impl->lifecycleObserver("planar-camera",std::string("{\"before\":{\"camera_name\":")+RenderLifecycle::quote(name)+"},\"after\":{\"camera_name_alive\":"+(alive?"true":"false")+"}}",!alive && !m_impl->lifecycleObserverFailures);}catch(...){++m_impl->lifecycleObserverFailures;}}
    }
}
void PlanarWaterReflection::setLifecycleReleaseObserver(std::function<void(const char*,const std::string&,bool)> observer)
{
    m_impl->lifecycleObserver=std::move(observer);
}
RenderLifecycleTargetFacts PlanarWaterReflection::lifecycleFacts() const {return m_impl->facts();}
void PlanarWaterReflection::initialize(Ogre::SceneManager& scene, Ogre::RenderSystem& renderer)
{
    if (m_impl->scene) throw std::runtime_error("PlanarWaterReflection initialized twice.");
    auto& s = *m_impl; s.scene = &scene; s.renderer = &renderer;
    s.prefix = scene.getName() + "/PlanarWaterReflection";
    s.camera = scene.createCamera(s.prefix + "/Camera");
    s.camera->setAutoAspectRatio(false); s.camera->setUseRenderingDistance(true);
    const auto* caps = renderer.getCapabilities();
    s.supported = caps && caps->hasCapability(Ogre::RSC_HWRENDER_TO_TEXTURE) && caps->hasCapability(Ogre::RSC_TEXTURE_FLOAT);
    const char* forced = std::getenv("HELLOMINE3D_REFLECTION_FALLBACK");
    if (forced && std::string(forced) == "1") s.supported = false;
    s.status("no-selected-resident-water");
}
void PlanarWaterReflection::selectPlaneY(float y) noexcept
{
    auto& s = *m_impl;
    if (!std::isfinite(y)) { clearSelection(); return; }
    if (!s.selected || s.planeY != y) s.stats.active = false;
    s.planeY = y; s.selected = true;
}
void PlanarWaterReflection::clearSelection() noexcept
{
    m_impl->selected = false; m_impl->stats.active = false;
}
void PlanarWaterReflection::resetWorld() noexcept
{
    auto& s = *m_impl;
    s.selected = false; s.stats.active = false; s.submitted = false;
    s.releaseTarget(); s.clearMaterials(); s.stats.reason = "world-reset";
    if (s.camera) s.camera->setLodCamera(nullptr);
    s.input.bindViewParameters = {};
}
const PlanarWaterReflection::Statistics& PlanarWaterReflection::statistics() const noexcept { return m_impl->stats; }
void PlanarWaterReflection::render(Ogre::Camera& mainCamera, Ogre::Viewport& mainViewport, const FrameInput& input)
{
    auto& s = *m_impl;
    if (!s.scene || !s.camera) throw std::runtime_error("PlanarWaterReflection was not initialized.");
    if (s.rendering) throw std::runtime_error("Recursive planar reflection update is prohibited.");
    if (s.submitted && s.lastFrame == input.frameSerial) return;
    s.submitted = true; s.lastFrame = input.frameSerial; s.input = input;
    s.stats.frameSerial = input.frameSerial; s.stats.sceneRevision = input.sceneRevision;
    s.stats.colourBatches = s.stats.colourTriangles = s.stats.shadowUpdates = s.stats.shadowBatches = 0;
    s.stats.rejectedWater = s.stats.rejectedFeedback = 0; s.stats.cpuMilliseconds = 0;
    if (!input.enabled || !input.linearHdr || !s.supported) {
        s.releaseTarget(); s.clearMaterials();
        s.status(!input.enabled ? "off" : !input.linearHdr ? "legacy-colour" : "unsupported-capability"); return;
    }
    if (!s.selected) { s.releaseTarget(); s.status("no-selected-resident-water"); return; }
    const auto eye = mainCamera.getDerivedPosition();
    if (input.cameraUnderwater || !std::isfinite(eye.y) || eye.y <= s.planeY + .15f) {
        s.status("underwater-or-surface-crossing"); return;
    }
    const auto size = targetSize(mainViewport.getActualWidth(), mainViewport.getActualHeight());
    if (!size) { s.releaseTarget(); s.status("physical-pixel-budget"); return; }
    if (mainCamera.getProjectionType() != Ogre::PT_PERSPECTIVE || mainCamera.isCustomProjectionMatrixEnabled() || mainCamera.isCustomViewMatrixEnabled()) {
        s.status("unsupported-projection"); return;
    }
    s.camera->disableReflection(); s.camera->disableCustomNearClipPlane();
    s.camera->setPosition(eye); s.camera->setOrientation(mainCamera.getDerivedOrientation());
    s.camera->setNearClipDistance(mainCamera.getNearClipDistance()); s.camera->setFarClipDistance(mainCamera.getFarClipDistance());
    s.camera->setFOVy(mainCamera.getFOVy()); s.camera->setAspectRatio(mainCamera.getAspectRatio());
    s.camera->setFrustumOffset(mainCamera.getFrustumOffset()); s.camera->setFocalLength(mainCamera.getFocalLength());
    s.camera->setLodCamera(&mainCamera);
    s.camera->enableReflection(Ogre::Plane(Ogre::Vector3::UNIT_Y, Ogre::Vector3(0, s.planeY, 0)));
    s.camera->enableCustomNearClipPlane(Ogre::Plane(Ogre::Vector3::UNIT_Y, Ogre::Vector3(0, s.planeY + .03f, 0)));
    if ((!s.target || s.stats.width != size.width || s.stats.height != size.height) && !s.makeTarget(size)) {
        s.status("float-target-unavailable"); return;
    }
    // Match AutoParamDataSource's production RTT projection, including the
    // backend's texture orientation, rather than guessing a GL UV flip.
    auto projection = s.camera->getProjectionMatrixWithRSDepth();
    if (s.target->requiresTextureFlipping()) for (unsigned c = 0; c < 4; ++c) projection[1][c] = -projection[1][c];
    s.viewProjection = projection * s.camera->getViewMatrix();
    if (!finite(s.viewProjection)) { s.status("invalid-oblique-projection"); return; }
    s.viewport->setBackgroundColour(Ogre::ColourValue(decode(input.authoredBackground.r),
        decode(input.authoredBackground.g), decode(input.authoredBackground.b), input.authoredBackground.a));
    const auto started = std::chrono::steady_clock::now();
    auto* queue = s.scene->getRenderQueue();
    s.previousListener = queue->getRenderableListener();
    struct ScopedListeners {
        Impl& s; Ogre::RenderQueue& queue;
        ScopedListeners(Impl& state, Ogre::RenderQueue& q) : s(state), queue(q)
        { s.scene->addListener(&s); queue.setRenderableListener(&s); s.rendering = true; }
        ~ScopedListeners()
        {
            for (auto* shadowTarget : s.observedShadowTargets) shadowTarget->removeListener(&s);
            s.observedShadowTargets.clear();
            s.scene->removeListener(&s); queue.setRenderableListener(s.previousListener);
            s.previousListener = nullptr; s.rendering = false;
        }
    } scoped(s, *queue);
    // Asset/shader/binder errors propagate. Only target/capability failures
    // above may fall back; a bad resident shader is not hidden by this view.
    s.target->update(false);
    s.stats.cpuMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    s.stats.colourBatches = s.target->getBatchCount(); s.stats.colourTriangles = s.target->getTriangleCount();
    ++s.stats.updateCount; s.stats.active = true;
    if (s.stats.reason != "rendered") {
        s.stats.reason = "rendered";
        std::cout << "[PLANAR_REFLECTION] active=1 plane_y=" << s.planeY << " size=" << s.stats.width << 'x' << s.stats.height
                  << " colour=linear-HDR auto_update=0 overlays=0 recursion=0 resident_only=1\n";
    }
}
void PlanarWaterReflection::bindWaterPass(Ogre::Pass& pass)
{
    auto& s = *m_impl;
    if (!pass.hasFragmentProgram()) throw std::runtime_error("Water pass has no fragment program.");
    auto parameters = pass.getFragmentProgramParameters();
    if (!s.stats.active) {
        setIfPresent(*parameters, "planarReflectionEnabled", 0.f);
        s.unbindWater(); return;
    }
    const char* required[] = { "planarReflectionTexture", "planarReflectionViewProj", "planarReflectionEnabled",
                              "planarReflectionPlaneY", "planarReflectionTexelSize" };
    for (const char* name : required) if (!parameters->_findNamedConstantDefinition(name, false))
        throw std::runtime_error(std::string("Water shader missing planar reflection interface: ") + name);
    if (s.boundWaterPass && s.boundWaterPass != &pass) s.unbindWater();
    s.boundWaterPass = &pass;
    auto* unit = pass.getTextureUnitState(ReflectionUnit);
    if (!unit) { unit = pass.createTextureUnitState(); unit->setName(ReflectionUnit); }
    unit->setTexture(s.texture); unit->setHardwareGammaEnabled(false);
    unit->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP); unit->setTextureFiltering(Ogre::TFO_BILINEAR);
    parameters->setNamedConstant("planarReflectionTexture", static_cast<int>(pass.getTextureUnitStateIndex(unit)));
    parameters->setNamedConstant("planarReflectionViewProj", s.viewProjection);
    parameters->setNamedConstant("planarReflectionEnabled", 1.f);
    parameters->setNamedConstant("planarReflectionPlaneY", s.planeY);
    parameters->setNamedConstant("planarReflectionTexelSize", Ogre::Vector2(1.f / s.stats.width, 1.f / s.stats.height));
}

void PlanarWaterReflection::captureDiagnostic(const std::string& absoluteOutputPrefix) const
{
    auto& s = *m_impl;
    const auto enabled = [](const char* value) {
        if (!value || !*value) return false;
        const std::string text(value);
        return text != "0" && text != "false" && text != "FALSE" && text != "False" &&
               text != "off" && text != "OFF";
    };
    const char* capture = std::getenv("HELLO_RENDER_CAPTURE");
    if (!capture || !*capture) capture = std::getenv("HELLO_VISUAL_CAPTURE");
    if (!enabled(std::getenv("HELLOMINE3D_WINDOW_HIDDEN")) || !enabled(capture) ||
        enabled(std::getenv("HELLO_PERF_CAPTURE")) || std::getenv("HELLO_PERF_CAPTURE_DIR") ||
        std::getenv("HELLOMINE3D_RC_PERF_PROFILE") || std::getenv("HELLOMINE3D_E2_BATCH_MANIFEST"))
        throw std::runtime_error("Planar readback requires hidden render capture and is prohibited in performance runs.");
    if (!s.stats.active || !s.target || s.rendering || s.diagnosticCaptures >= 4)
        throw std::runtime_error("Planar readback requires a completed active view and has a four-capture lifetime budget.");
    namespace fs = std::filesystem;
    const fs::path prefix(absoluteOutputPrefix);
    if (!prefix.is_absolute() || absoluteOutputPrefix.size() > 4096)
        throw std::runtime_error("Planar readback prefix must be an absolute bounded path.");
    const fs::path rawPath(absoluteOutputPrefix + ".native-linear.rgba32f");
    const fs::path previewPath(absoluteOutputPrefix + ".camera-preview.png");
    const fs::path factsPath(absoluteOutputPrefix + ".facts.txt");
    if (fs::exists(rawPath) || fs::exists(previewPath) || fs::exists(factsPath))
        throw std::runtime_error("Planar readback refuses to overwrite existing evidence.");
    fs::create_directories(prefix.parent_path());
    ++s.diagnosticCaptures;
    const std::size_t pixels = std::size_t(s.stats.width) * s.stats.height;
    if (!pixels || pixels > MaximumPixels) throw std::runtime_error("Planar readback pixel budget exceeded.");
    std::vector<float> actual(pixels * 4, std::numeric_limits<float>::quiet_NaN());
    GLenum before = GL_NO_ERROR, after = GL_NO_ERROR;
    GLuint fbo = 0; GLint colourObject = 0;
    {
        NativeState saved;
        before = glGetError();
        s.target->getCustomAttribute("GL_FBOID", &fbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo); glReadBuffer(GL_COLOR_ATTACHMENT0);
        glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &colourObject);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 1); glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0); glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        glReadPixels(0, 0, static_cast<GLsizei>(s.stats.width), static_cast<GLsizei>(s.stats.height),
                     GL_RGBA, GL_FLOAT, actual.data());
        after = glGetError();
    }
    std::ofstream raw(rawPath, std::ios::binary);
    raw.write(reinterpret_cast<const char*>(actual.data()), static_cast<std::streamsize>(actual.size() * sizeof(float)));
    raw.close();
    if (!raw) throw std::runtime_error("Cannot write planar native pixel evidence.");
    std::array<float, 4> minimum{{ INFINITY, INFINITY, INFINITY, INFINITY }}, maximum{{ -INFINITY, -INFINITY, -INFINITY, -INFINITY }};
    std::size_t nonfinite = 0, overOne = 0;
    std::vector<unsigned char> preview(pixels * 4);
    const bool flipped = s.target->requiresTextureFlipping();
    const auto displayByte = [](float value) {
        if (!std::isfinite(value)) return static_cast<unsigned char>(255);
        value = std::clamp(value, 0.f, 1.f);
        const float encoded = value <= .0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.f / 2.4f) - .055f;
        return static_cast<unsigned char>(std::clamp(std::lround(encoded * 255), 0l, 255l));
    };
    for (unsigned y = 0; y < s.stats.height; ++y) {
        // RTT projection flips Y on this backend. Show the reflection camera
        // upright; raw file remains exact glReadPixels bottom-left row order.
        const unsigned previewY = flipped ? y : s.stats.height - 1u - y;
        for (unsigned x = 0; x < s.stats.width; ++x) {
            const std::size_t at = (std::size_t(y) * s.stats.width + x) * 4;
            const std::size_t out = (std::size_t(previewY) * s.stats.width + x) * 4;
            for (unsigned c = 0; c < 4; ++c) {
                if (!std::isfinite(actual[at+c])) ++nonfinite;
                else { minimum[c] = std::min(minimum[c], actual[at+c]); maximum[c] = std::max(maximum[c], actual[at+c]); }
                if (c < 3) { if (actual[at+c] > 1.f) ++overOne; preview[out+c] = displayByte(actual[at+c]); }
            }
            preview[out+3] = 255;
        }
    }
    Ogre::Image image;
    image.loadDynamicImage(preview.data(), s.stats.width, s.stats.height, 1, Ogre::PF_BYTE_RGBA, false);
    image.save(previewPath.string());
    std::ofstream facts(factsPath);
    facts << std::setprecision(10) << "evidence=DEVELOPER_DIAGNOSTIC normal_input=0\n"
          << "raw_format=RGBA_FLOAT32_NATIVE_ENDIAN raw_origin=GL_BOTTOM_LEFT source=actual_native_RGBA16F_RTT\n"
          << "preview=sRGB_encode(clamp(linear_rgb,0,1)) tone_map=none alpha=opaque orientation=reflection_camera\n"
          << "requires_texture_flipping=" << flipped << " fbo=" << fbo << " colour_object=" << colourObject << '\n'
          << "width=" << s.stats.width << " height=" << s.stats.height << " raw_bytes=" << actual.size() * sizeof(float) << '\n'
          << "frame=" << s.stats.frameSerial << " scene_revision=" << s.stats.sceneRevision << " plane_y=" << s.planeY << '\n'
          << "gl_error_before=" << before << " gl_error_after=" << after << " nonfinite_components=" << nonfinite << " rgb_components_over_one=" << overOne << '\n'
          << "colour_batches=" << s.stats.colourBatches << " shadow_updates=" << s.stats.shadowUpdates << " shadow_batches=" << s.stats.shadowBatches << '\n'
          << "rejected_water=" << s.stats.rejectedWater << " rejected_feedback=" << s.stats.rejectedFeedback << '\n';
    const auto position = s.camera->getDerivedPosition();
    const auto direction = s.camera->getDerivedDirection();
    facts << "reflected_eye=" << position.x << ',' << position.y << ',' << position.z << '\n'
          << "reflected_direction=" << direction.x << ',' << direction.y << ',' << direction.z << '\n'
          << "view_projection_row_major=";
    for (unsigned r = 0; r < 4; ++r) for (unsigned c = 0; c < 4; ++c) facts << (r || c ? "," : "") << s.viewProjection[r][c];
    facts << "\nminimum_rgba=";
    for (unsigned c = 0; c < 4; ++c) facts << (c ? "," : "") << minimum[c];
    facts << "\nmaximum_rgba=";
    for (unsigned c = 0; c < 4; ++c) facts << (c ? "," : "") << maximum[c];
    auto* unit = s.boundWaterPass ? s.boundWaterPass->getTextureUnitState(ReflectionUnit) : nullptr;
    facts << "\nmain_water_texture_bound=" << (unit && unit->_getTexturePtr().get() == s.texture.get()) << '\n';
    if (unit) facts << "main_water_texture_unit=" << s.boundWaterPass->getTextureUnitStateIndex(unit) << '\n';
    facts.close();
    if (!facts) throw std::runtime_error("Cannot write planar diagnostic facts.");
    std::cout << "[PLANAR_REFLECTION_CAPTURE] prefix=" << absoluteOutputPrefix << " size=" << s.stats.width << 'x' << s.stats.height
              << " frame=" << s.stats.frameSerial << " raw_bytes=" << actual.size() * sizeof(float)
              << " gl_errors=" << before << ',' << after << " nonfinite=" << nonfinite << '\n';
    if (before != GL_NO_ERROR || after != GL_NO_ERROR || nonfinite)
        throw std::runtime_error("Planar native readback had GL errors or nonfinite pixels; failed evidence preserved.");
}

std::string PlanarWaterReflection::worldEditDiagnosticFacts() const
{
    const auto& s=*m_impl;
    if (!std::getenv("HELLOMINE3D_REFERENCE_EDIT_PROBE") || !s.camera || !s.target || !s.stats.active || s.rendering)
        throw std::runtime_error("World-edit view facts require its admitted completed active view.");
    auto projection=s.camera->getProjectionMatrixWithRSDepth();
    const bool flip=s.target->requiresTextureFlipping();
    if(flip)for(unsigned c=0;c<4;++c)projection[1][c]=-projection[1][c];
    const auto matrix=[](const Ogre::Matrix4& m){std::ostringstream o;o<<std::setprecision(17)<<'[';
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)o<<(r || c?",":"")<<m[r][c];return o.str()+']';};
    std::ostringstream o;o<<std::boolalpha<<std::setprecision(17)
        <<"{\"frame\":"<<s.stats.frameSerial<<",\"scene_revision\":"<<s.stats.sceneRevision<<",\"update_count\":"<<s.stats.updateCount
        <<",\"plane_y\":"<<s.planeY<<",\"clip_y\":"<<s.planeY+.03f<<",\"texture_flip\":"<<flip
        <<",\"camera_name\":"<<RenderLifecycle::quote(s.camera->getName())
        <<",\"view_row_major\":"<<matrix(s.camera->getViewMatrix())
        <<",\"projection_row_major\":"<<matrix(projection)<<",\"view_projection_row_major\":"<<matrix(s.viewProjection)
        <<",\"target\":"<<s.facts().json()<<'}';
    return o.str();
}
