#pragma once
#include "../Config.h"
#include <cstddef>
#include <memory>
#include <cstdint>
#include <functional>
#include <string>
struct RenderLifecycleTargetFacts;
namespace Ogre { class Viewport; class RenderSystem; class CompositorInstance; class RenderTexture; class DepthBuffer; }
class HdrPipeline {
public:
    HdrPipeline();
    ~HdrPipeline();
    HdrPipeline(const HdrPipeline&) = delete;
    HdrPipeline& operator=(const HdrPipeline&) = delete;
    static bool preferMsaa4(RenderPipeline requested) noexcept;
    static bool msaa4WindowSupported() noexcept;
    void initialize(Ogre::Viewport& viewport, Ogre::RenderSystem& renderer, RenderPipeline requested);
    void beforeFrame();
    RenderLifecycleTargetFacts lifecycleFacts() const;
    void setLifecycleReleaseObserver(std::function<void(const char*,const std::string&,bool)> observer, bool delayedDrainFault = false);
    void applySceneParameters() const;
    bool active() const noexcept { return m_active; }
    Ogre::RenderTexture* sceneTarget() const noexcept { return m_sceneTarget; }
    bool fallback() const noexcept { return m_requested == RenderPipeline::LinearHdr && !m_active; }
    RenderPipeline actualMode() const noexcept { return m_active ? RenderPipeline::LinearHdr : RenderPipeline::Legacy; }
private:
    bool install();
    bool installTarget(bool multisample);
    void setSpatialAa(bool multisample);
    void remove() noexcept;
    Ogre::Viewport* m_viewport = nullptr;
    Ogre::CompositorInstance* m_instance = nullptr;
    Ogre::RenderSystem* m_renderer = nullptr;
    Ogre::RenderTexture* m_sceneTarget = nullptr;
    std::unique_ptr<Ogre::DepthBuffer> m_sceneDepth;
    RenderPipeline m_requested = RenderPipeline::Legacy;
    bool m_active = false;
    bool m_storageSupported = false;
    bool m_msaa4Preferred = false, m_msaa4Active = false;
    bool m_spatialAaRequested = true, m_hasSpatialAa = false;
    unsigned m_width = 0, m_height = 0;
    std::uint64_t m_targetGeneration = 0;
    unsigned m_lifecycleObserverFailures = 0;
    bool m_lifecycleDelayedDrainFault = false;
    std::function<void(const char*,const std::string&,bool)> m_lifecycleObserver;
};
