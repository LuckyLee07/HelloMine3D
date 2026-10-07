#pragma once
// Explicit, bounded prototype observation. Ordinary rendering never installs it.
#include "ChunkSectionRenderable.h"
#include "HdrPipeline.h"
#include <Ogre.h>
#include <OgreAutoParamDataSource.h>
#include <OgreRenderObjectListener.h>
#include <OgreRenderQueueListener.h>
#include <OgreRenderQueueSortingGrouping.h>
#include <GL/gl3w.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>

class ReferenceWaterDepthPrepassDiagnostic final
    : public Ogre::RenderQueue::RenderableListener,
      public Ogre::RenderObjectListener,
      public Ogre::RenderQueueListener,
      public ChunkSectionRenderable::NativeDrawObserver {
public:
    static bool validateConfig(bool hdr) {
        const char* value = std::getenv("HELLOMINE3D_REFERENCE_WATER_DEPTH_PREPASS");
        if (!value || !*value) return false;
        require(std::string(value) == "1", "exact opt-in1 required");
        auto exact = [](const char* key, const char* expected) {
            const char* v = std::getenv(key); return v && std::string(v) == expected;
        };
        require(hdr && exact("HELLOMINE3D_WINDOW_HIDDEN", "1") &&
            exact("HELLO_RENDER_CAPTURE", "1") && exact("HELLO_RENDER_CAPTURE_EXIT", "1") &&
            exact("HELLO_RENDER_CAPTURE_MS", "3500,7000") &&
            exact("HELLO_RENDER_CAPTURE_MAX_DELTA_MS", "5000"), "bounded hidden HDR capture required");
        for (const char* key : {"HELLOMINE3D_ROOT", "HELLOMINE3D_SAVE_DIR", "HELLOMINE3D_CATALOGUE_DIR", "HELLO_RENDER_CAPTURE_DIR"}) {
            const char* v = std::getenv(key);
            require(v && std::filesystem::path(v).is_absolute(), "absolute owned capture paths required");
        }
        for (const char* key : {"HELLOMINE3D_REFERENCE_WATER_DIR", "HELLOMINE3D_REFERENCE_WATER_PROBE", "HELLOMINE3D_REFERENCE_WATER_FAULT", "HELLOMINE3D_REFERENCE_EDIT_DIR", "HELLOMINE3D_REFERENCE_EDIT_PROBE", "HELLOMINE3D_REFERENCE_EDIT_FAULT", "HELLOMINE3D_REFERENCE_RESIDENCY_DIR", "HELLOMINE3D_REFERENCE_RESIDENCY_PROBE", "HELLOMINE3D_REFERENCE_RESIDENCY_FAULT", "HELLOMINE3D_REFERENCE_SETTINGS_RESTART_DIR", "HELLOMINE3D_RENDER_LIFECYCLE_DIR", "HELLOMINE3D_RENDER_LIFECYCLE_PROBE", "HELLOMINE3D_LIFECYCLE_FAULT", "HELLOMINE3D_SHORE_EDIT_CAPTURE_DIR", "HELLOMINE3D_FERN_WIND_CAPTURE_DIR", "HELLOMINE3D_MATERIAL_IDENTITY_CAPTURE_DIR", "HELLOMINE3D_CAMERA_DIAGNOSTICS_DIR"})
            require(std::getenv(key) == nullptr, "other draw observers forbidden");
        return true;
    }
    ReferenceWaterDepthPrepassDiagnostic(Ogre::Root& root, Ogre::SceneManager& scene,
                                        Ogre::Camera& camera, HdrPipeline& hdr)
        : m_root(root), m_scene(scene), m_camera(camera), m_hdr(hdr), m_started(Clock::now()) {
        require(hdr.active(), "actual HDR target required");
        auto material = Ogre::MaterialManager::getSingleton().getByName("HelloMine3D/Water");
        require(!material.isNull(), "Water material missing"); material->load();
        auto* tech = material->getBestTechnique();
        require(tech && tech->getNumPasses() == 1, "single original colour pass required");
        auto* colour = tech->getPass(0);
        require(colour->hasVertexProgram() && colour->hasFragmentProgram() &&
            colour->getVertexProgramParameters()->_findNamedConstantDefinition("waterBoundaryPinsV1", false) &&
            colour->getFragmentProgramParameters()->_findNamedConstantDefinition("waterDepthOnly", false), "prototype shader interface required");
        require(colour->isTransparent() && !colour->getDepthWriteEnabled() && colour->getColourWriteEnabled(), "original Water colour state required");
        colour->getFragmentProgramParameters()->setNamedConstant("waterDepthOnly", 0.f);
        m_material = Ogre::MaterialManager::getSingleton().create("HelloMine3D/ReferenceWaterDepthPrototype", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        try {
            m_material->setReceiveShadows(false);
            m_material->removeAllTechniques();
            m_technique = m_material->createTechnique(); m_depth = m_technique->createPass();
            prepare(*colour); m_material->load();
        } catch (...) {
            Ogre::MaterialManager::getSingleton().remove(m_material->getHandle());
            m_material.setNull(); throw;
        }
        m_previous = scene.getRenderQueue()->getRenderableListener();
        scene.addRenderObjectListener(this); scene.addRenderQueueListener(this);
        scene.getRenderQueue()->setRenderableListener(this);
        std::cout << "[WATER_DEPTH_PROTOTYPE] installed=1 colour_pass=0 depth_priority=99 colour_priority=100 material=1 pass=1 extra_geometry=0 extra_RTT=0\n";
    }
    ~ReferenceWaterDepthPrepassDiagnostic() override {
        if (m_object) m_object->setNativeDrawObserver(nullptr);
        if (m_query) { glEndQuery(GL_PRIMITIVES_GENERATED); glDeleteQueries(1, &m_query); }
        auto* queue = m_scene.getRenderQueue();
        if (queue->getRenderableListener() == this) queue->setRenderableListener(m_previous);
        m_scene.removeRenderObjectListener(this); m_scene.removeRenderQueueListener(this);
        if (!m_material.isNull()) { Ogre::MaterialManager::getSingleton().remove(m_material->getHandle()); m_material.setNull(); }
        std::cout << "[WATER_DEPTH_PROTOTYPE] released=1 observed_frames=" << m_observedFrames
                  << " queried_draws=" << m_queriedDraws << " material=0 pass=0 extra_geometry=0 extra_RTT=0\n";
    }
    bool renderableQueued(Ogre::Renderable* rend, Ogre::uint8 group, Ogre::ushort priority,
                          Ogre::Technique** technique, Ogre::RenderQueue* queue) override {
        if (m_previous && !m_previous->renderableQueued(rend, group, priority, technique, queue)) return false;
        if (!mainScope() || !*technique || (*technique)->getParent()->getName() != "HelloMine3D/Water") return true;
        beginFrame();
        require(group == Ogre::RENDER_QUEUE_8 && priority == OGRE_RENDERABLE_DEFAULT_PRIORITY && (*technique)->getNumPasses() == 1, "Water queue contract changed");
        require(m_objects.size() < MaximumWaterDraws && m_objects.insert(rend).second, "Water queue budget or duplicate exceeded");
        if (m_objects.size() == 1) prepare(*(*technique)->getPass(0));
        // Public group insertion bypasses RenderQueue's listener. A complete
        // priority99 group is drawn before priority100, across every section.
        queue->getQueueGroup(group)->addRenderable(rend, m_technique, priority - 1);
        return true;
    }
    void notifyRenderSingleObject(Ogre::Renderable* rend, const Ogre::Pass* pass,
                                  const Ogre::AutoParamDataSource* source, const Ogre::LightList*, bool suppressed) override {
        if (!mainScope() || !source || source->getCurrentCamera() != &m_camera || !pass ||
            (pass != m_depth && pass->getParent()->getParent()->getName() != "HelloMine3D/Water")) return;
        require(!suppressed && !m_object, "suppressed or nested native draw");
        auto* object = dynamic_cast<ChunkSectionRenderable*>(rend);
        require(object && m_objects.count(rend), "nonresident Water native draw");
        m_object = object; m_pass = pass; object->setNativeDrawObserver(this);
    }
    void beforeNativeDraw(ChunkSectionRenderable& object, Ogre::SceneManager* scene, Ogre::RenderSystem*) override {
        require(&object == m_object && scene == &m_scene && m_pass, "native draw identity mismatch");
        if (m_pass == m_depth) require(m_colourDraws == 0, "depth draw after colour");
        else require(m_depthDraws == m_objects.size(), "colour before all resident depth draws");
        if (sampleFrame()) {
            GLint active = 0; glGetQueryiv(GL_PRIMITIVES_GENERATED, GL_CURRENT_QUERY, &active);
            require(active == 0 && glGetError() == GL_NO_ERROR, "existing primitive query or GL error");
            glGenQueries(1, &m_query); require(m_query != 0, "primitive query allocation failed");
            glBeginQuery(GL_PRIMITIVES_GENERATED, m_query);
        }
    }
    void afterNativeDraw(ChunkSectionRenderable& object, Ogre::SceneManager*, Ogre::RenderSystem*) override {
        require(&object == m_object && m_pass, "native post identity mismatch");
        const bool depth = m_pass == m_depth;
        if (m_query) {
            GLuint generated = 0; glEndQuery(GL_PRIMITIVES_GENERATED);
            glGetQueryObjectuiv(m_query, GL_QUERY_RESULT, &generated); glDeleteQueries(1, &m_query); m_query = 0;
            require(generated == object.indexCount() / 3 && generated > 0, "actual Water primitive count mismatch");
            GLboolean mask = GL_FALSE, colours[4] = {}; GLint program = 0, linked = 0, function = 0;
            glGetBooleanv(GL_DEPTH_WRITEMASK, &mask); glGetBooleanv(GL_COLOR_WRITEMASK, colours);
            glGetIntegerv(GL_CURRENT_PROGRAM, &program); glGetIntegerv(GL_DEPTH_FUNC, &function);
            require(program > 0, "actual draw has no program"); glGetProgramiv(GLuint(program), GL_LINK_STATUS, &linked);
            const GLint depthLocation = glGetUniformLocation(GLuint(program), "waterDepthOnly");
            const GLint hdrLocation = glGetUniformLocation(GLuint(program), "linearHdrMode");
            GLfloat depthValue = -1.f, hdrValue = 0.f;
            require(depthLocation >= 0 && hdrLocation >= 0, "actual draw interface absent");
            glGetUniformfv(GLuint(program), depthLocation, &depthValue); glGetUniformfv(GLuint(program), hdrLocation, &hdrValue);
            require(linked && depthValue == (depth ? 1.f : 0.f) && hdrValue > .5f &&
                mask == (depth ? GL_TRUE : GL_FALSE) && glIsEnabled(GL_DEPTH_TEST) && function == GL_LEQUAL,
                "actual depth/uniform state mismatch");
            for (auto c : colours) require(c == (depth ? GL_FALSE : GL_TRUE), "actual colour write mask mismatch");
            require(glGetError() == GL_NO_ERROR, "native Water observation GL error");
            ++m_queriedDraws;
            if ((depth && m_depthDraws == 0) || (!depth && m_colourDraws == 0))
                std::cout << "[WATER_DEPTH_DRAW] frame=" << m_frame << " kind=" << (depth ? "depth" : "colour")
                          << " order=" << m_depthDraws + m_colourDraws << " queued=" << m_objects.size()
                          << " program=" << program << " depth_write=" << unsigned(mask)
                          << " colour_write=" << unsigned(colours[0]) << " depth_only=" << depthValue
                          << " hdr=" << hdrValue << " primitives=" << generated << " gl_error=0\n";
        }
        if (depth) ++m_depthDraws; else ++m_colourDraws;
        object.setNativeDrawObserver(nullptr); m_object = nullptr; m_pass = nullptr;
    }
    void postRenderQueues() override {
        if (!mainScope() || m_root.getNextFrameNumber() != m_frame || m_objects.empty()) return;
        require(!m_object && !m_query && m_depthDraws == m_objects.size() && m_colourDraws == m_objects.size(), "resident depth/colour draw completeness mismatch");
        ++m_observedFrames;
        if (sampleFrame()) std::cout << "[WATER_DEPTH_FRAME] frame=" << m_frame << " resident_water=" << m_objects.size()
            << " depth_draws=" << m_depthDraws << " colour_draws=" << m_colourDraws
            << " all_depth_before_colour=1 extra_geometry_bytes=0 extra_RTT_bytes=0 gl_error=0\n";
    }
private:
    using Clock = std::chrono::steady_clock;
    static constexpr std::size_t MaximumWaterDraws = 1024;
    Ogre::Root& m_root; Ogre::SceneManager& m_scene; Ogre::Camera& m_camera; HdrPipeline& m_hdr;
    Ogre::RenderQueue::RenderableListener* m_previous = nullptr;
    Ogre::MaterialPtr m_material; Ogre::Technique* m_technique = nullptr; Ogre::Pass* m_depth = nullptr;
    ChunkSectionRenderable* m_object = nullptr; const Ogre::Pass* m_pass = nullptr; GLuint m_query = 0;
    unsigned long m_frame = std::numeric_limits<unsigned long>::max();
    unsigned m_depthDraws = 0, m_colourDraws = 0, m_observedFrames = 0, m_queriedDraws = 0;
    std::unordered_set<Ogre::Renderable*> m_objects;
    Clock::time_point m_started;
    static void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(std::string("Water depth prototype: ") + reason); }
    bool mainScope() const {
        auto* viewport = m_scene.getCurrentViewport();
        return m_hdr.active() && viewport && viewport->getCamera() == &m_camera &&
            viewport->getTarget() == m_hdr.sceneTarget() && m_scene._getCurrentRenderStage() == Ogre::SceneManager::IRS_NONE;
    }
    void beginFrame() {
        const auto frame = m_root.getNextFrameNumber();
        if (frame == m_frame) return;
        require(frame < 2048 && Clock::now() - m_started < std::chrono::seconds(30), "frame or wall-time bound exceeded");
        m_frame = frame; m_objects.clear(); m_depthDraws = m_colourDraws = 0;
    }
    bool sampleFrame() const { return m_frame < 4 || m_frame % 120 == 0; }
    void prepare(Ogre::Pass& colour) {
        *m_depth = colour;
        m_depth->setColourWriteEnabled(false); m_depth->setDepthWriteEnabled(true);
        m_depth->setSceneBlending(Ogre::SBF_ONE, Ogre::SBF_ZERO);
        m_depth->getFragmentProgramParameters()->setNamedConstant("waterDepthOnly", 1.f);
        require(!m_depth->isTransparent() && m_depth->getDepthCheckEnabled(), "opaque depth classification required");
    }
};
