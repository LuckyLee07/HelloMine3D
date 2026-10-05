#pragma once
#include "ChunkSectionRenderable.h"
#include <OgreRenderObjectListener.h>
#include <memory>
#include <vector>

class World;
namespace Ogre { class SceneManager; class Camera; class RenderWindow; }

// Render-thread-only, explicitly enabled client diagnostic. It observes existing
// objects/context; it never creates meshes, programs, textures or World state.
class FloraWindCapture final : public ChunkSectionRenderable::NativeDrawObserver,
                               public Ogre::RenderObjectListener {
public:
    struct PartState {
        glm::ivec3 section{0};
        std::uint64_t incarnation = 0;
        std::uint32_t liveRevision = 0, uploadRevision = 0;
        bool gpuResident = false, incarnationKnown = false;
    };
    struct SourceBlock { glm::ivec3 position{0}; };
    struct Binding {
        ChunkSectionRenderable* renderable = nullptr;
        glm::ivec3 origin{0};
        // Repack only the retained actual production CPU parts, in the same
        // order as their existing GPU object's constructor; no synthetic mesh.
        PackedTerrainRenderBatch cpu;
        std::vector<PartState> parts;
        std::vector<SourceBlock> sources;
    };
    FloraWindCapture(const std::string& newDirectory, Ogre::SceneManager&,
                     Ogre::Camera&, Ogre::RenderWindow&);
    ~FloraWindCapture() override;
    FloraWindCapture(const FloraWindCapture&) = delete;
    FloraWindCapture& operator=(const FloraWindCapture&) = delete;
    void beginFrame(const std::string& phase, const std::string& mode,
                    std::uint64_t frameId, World&, std::vector<Binding> bindings);
    // After actual UI backend submission, before swap. Root supplies independently
    // refreshed retained parts and upload/residency facts for endpoint comparison.
    void finishFrame(std::vector<Binding> endBindings);
    bool isFrameOpen() const noexcept;
    std::size_t frameCount() const noexcept;
    bool isComplete() const noexcept; // four completed packets, not acceptance
    std::string framePngPath() const;
    // Call before destroying/replacing an existing instance, while context lives.
    void detachRenderable(ChunkSectionRenderable&) noexcept;
    void beforeNativeDraw(ChunkSectionRenderable&, Ogre::SceneManager*, Ogre::RenderSystem*) override;
    void afterNativeDraw(ChunkSectionRenderable&, Ogre::SceneManager*, Ogre::RenderSystem*) override;
    void notifyRenderSingleObject(Ogre::Renderable*, const Ogre::Pass*,
        const Ogre::AutoParamDataSource*, const Ogre::LightList*, bool) override;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
