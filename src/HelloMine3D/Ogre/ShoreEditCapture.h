#pragma once

#include "ChunkSectionRenderable.h"
#include <OgreRenderObjectListener.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Explicit render-thread diagnostic of original production upload storage.
// Bootstrap supplies copies from its actual uploader and locked live revisions.
// Optional native mode observes existing main-camera draw callbacks only. It
// never reads World, replays rendering, or binds a VAO/ARRAY/ELEMENT_ARRAY buffer.
namespace Ogre { class SceneManager; class Camera; class RenderWindow; }
class ShoreEditCapture final : public ChunkSectionRenderable::NativeDrawObserver,
                               public Ogre::RenderObjectListener {
public:
    struct Part {
        glm::ivec3 section{0};
        std::uint32_t uploadRevision = 0, liveRevision = 0;
        bool liveKnown = false, gpuResident = false, stillCpuReady = true;
    };
    struct Binding {
        ChunkSectionRenderable* renderable = nullptr;
        glm::ivec3 origin{0};
        std::string layer; // "water" or "solid"
        std::string ownerKey;
        PackedTerrainRenderBatch cpu;
        std::vector<Part> parts; // Original constructor order, at most four.
        std::uint64_t uploadSerial = 0; // Bootstrap's diagnostic object lifetime.
    };

    // Requires a fresh directory. At most six phases, eight original objects per
    // phase, 16 MiB VBO+IBO per object, and 256 MiB total observer writes.
    ShoreEditCapture(const std::string& newOutputDirectory, Ogre::SceneManager&,
                     Ogre::Camera&, bool nativeDraw = false);
    // Separate, read-only natural water seam observation. Coordinates are exact
    // framebuffer pixels, bottom-left origin, frozen by Bootstrap only after
    // inspecting the actual shared indexed surface and main-camera projection.
    // This constructor does not enable or relax the six shore edit phases.
    struct WaterSeamOptions {
        std::string renderingMode;
        int roiX = 0, roiY = 0, roiWidth = 0, roiHeight = 0;
    };
    ShoreEditCapture(const std::string& newOutputDirectory, Ogre::SceneManager&,
                     Ogre::Camera&, Ogre::RenderWindow&, const WaterSeamOptions&);
    ~ShoreEditCapture() override;
    ShoreEditCapture(const ShoreEditCapture&) = delete;
    ShoreEditCapture& operator=(const ShoreEditCapture&) = delete;

    // Synchronous, current context. Root independently retains World/map facts
    // and saves the actual window to phasePngPath() in this same original frame.
    // Raw storage agreement does not prove inner VAO fetch or visible pixels.
    void capturePhase(const std::string& phase, const std::string& mode,
                      std::uint64_t frameId, const std::vector<Binding>& bindings);
    std::size_t phaseCount() const noexcept;
    std::string phaseJsonPath() const;
    std::string phasePngPath() const;

    bool nativeDrawEnabled() const noexcept;
    // Arm after the actual uploader has finished, before this frame's draws.
    // Water seam mode accepts zero to two valid current original bindings so
    // missing owners still retain their actual frame as OPEN; shore mode is
    // unchanged. A binding supplied here must still be a current valid upload.
    void beginNativeFrame(std::uint64_t renderFrameId, std::vector<Binding>);
    // Missing/unbound/unsupported draws remain OPEN; all original objects and
    // their copied endpoints must agree in this one frame to return true.
    bool finishNativeFrame(std::vector<Binding> endBindings);
    // Call after finishNativeFrame, before swap, including unsupported/missing
    // draw frames. Each actual frame retains metadata and an exact lossless ROI;
    // only first/middle/last checkpoints retain full original storage and PNG.
    void captureWaterFrame(double elapsedSeconds, double deltaSeconds,
                           const std::string& checkpoint);
    std::size_t waterFrameCount() const noexcept;
    // Caller supplies bounded actual World endpoint facts, not an atomic copy.
    // A short/gapped/unsupported observation remains OPEN in the session index.
    void finishWaterSession(const std::string& sourceBeforeJson,
                            const std::string& sourceAfterJson,
                            const std::string& reason);
    void cancelNativeFrame() noexcept;
    void detachRenderable(ChunkSectionRenderable&) noexcept;
    void retainNativeFailure(const std::string& reason) noexcept;
    void beforeNativeDraw(ChunkSectionRenderable&, Ogre::SceneManager*, Ogre::RenderSystem*) override;
    void afterNativeDraw(ChunkSectionRenderable&, Ogre::SceneManager*, Ogre::RenderSystem*) override;
    void notifyRenderSingleObject(Ogre::Renderable*, const Ogre::Pass*,
        const Ogre::AutoParamDataSource*, const Ogre::LightList*, bool) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
