#pragma once
#include "../Config.h"
#include <cstddef>
namespace Ogre { class Viewport; class RenderSystem; class CompositorInstance; }
class HdrPipeline {
public:
    HdrPipeline() = default;
    ~HdrPipeline();
    HdrPipeline(const HdrPipeline&) = delete;
    HdrPipeline& operator=(const HdrPipeline&) = delete;
    void initialize(Ogre::Viewport& viewport, Ogre::RenderSystem& renderer, RenderPipeline requested);
    void beforeFrame();
    void applySceneParameters() const;
    bool active() const noexcept { return m_active; }
    bool fallback() const noexcept { return m_requested == RenderPipeline::LinearHdr && !m_active; }
    RenderPipeline actualMode() const noexcept { return m_active ? RenderPipeline::LinearHdr : RenderPipeline::Legacy; }
private:
    bool install();
    void remove() noexcept;
    Ogre::Viewport* m_viewport = nullptr;
    Ogre::CompositorInstance* m_instance = nullptr;
    RenderPipeline m_requested = RenderPipeline::Legacy;
    bool m_active = false;
    unsigned m_width = 0, m_height = 0;
};
