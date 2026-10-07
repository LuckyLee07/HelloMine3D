#pragma once

#include <OgreColourValue.h>
#include <OgrePrerequisites.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

struct RenderLifecycleTargetFacts;
namespace Ogre { class Camera; class Pass; class RenderSystem; class SceneManager; class Viewport; }

// Renders existing Ogre residents only. The owner selects one actual, animated
// water surface's mean world height; this class never queries World/streaming.
class PlanarWaterReflection {
public:
    struct TargetSize {
        unsigned width = 0, height = 0;
        std::uint64_t pixels() const noexcept { return std::uint64_t(width) * height; }
        explicit operator bool() const noexcept { return width && height; }
    };
    static constexpr std::uint64_t MaximumPixels = 1920u * 1080u;
    static constexpr std::size_t MaximumPrivateMaterials = 96;
    static constexpr std::size_t MaximumPrivatePasses = 384;
    static TargetSize targetSize(int physicalWidth, int physicalHeight) noexcept;
    static bool excludes(const std::string& material, unsigned queueGroup) noexcept;

    using ViewParameterBinder = std::function<void(const Ogre::String& sourceMaterial,
                                                  Ogre::Pass& privatePass,
                                                  const Ogre::Camera& reflectedCamera)>;
    struct FrameInput {
        bool enabled = true;
        bool linearHdr = false;
        bool cameraUnderwater = false;
        std::uint64_t frameSerial = 0;
        std::uint64_t sceneRevision = 0;
        // Authored display colour, decoded here before the linear target clear.
        Ogre::ColourValue authoredBackground = Ogre::ColourValue::Black;
        ViewParameterBinder bindViewParameters;
    };
    struct Statistics {
        std::uint64_t updateCount = 0, frameSerial = 0, sceneRevision = 0;
        std::uint64_t lastRenderedFrame = 0; // Completed actual RTT update, not latest input.
        unsigned width = 0, height = 0;
        std::size_t colourBytes = 0, depthStencilBytes = 0;
        std::size_t privateMaterials = 0, privatePasses = 0;
        std::size_t colourBatches = 0, colourTriangles = 0;
        std::size_t shadowUpdates = 0, shadowBatches = 0;
        std::size_t rejectedWater = 0, rejectedFeedback = 0;
        double cpuMilliseconds = 0; // Submission/wait wall time, not GPU time.
        bool active = false;
        std::string reason = "uninitialized";
    };

    PlanarWaterReflection();
    ~PlanarWaterReflection();
    PlanarWaterReflection(const PlanarWaterReflection&) = delete;
    PlanarWaterReflection& operator=(const PlanarWaterReflection&) = delete;
    void initialize(Ogre::SceneManager&, Ogre::RenderSystem&);
    // Before first render, certify the actual Water pass against its already
    // compiled/linked, never-bound startup GL program. Zero reuses that numeric
    // certificate for the same immutable program pair; it never links per frame.
    // Complete absence is a legal old shader, partial/invalid interfaces fail.
    // Call again before render when replacing a pass; old owned bindings release.
    void prepareWaterPass(Ogre::Pass&, unsigned linkedProgram = 0);
    void selectPlaneY(float actualMeanY) noexcept;
    void clearSelection() noexcept;
    // After resident uploads, camera and environment sync; before main draw.
    // At most one update per frameSerial. Never an automatically updated RTT.
    void render(Ogre::Camera& mainCamera, Ogre::Viewport& mainViewport, const FrameInput&);
    // Bind after render; the disabled path preserves the approximate shader.
    // When its optional scalar 2D sampler/flag exists, an owned blank TUS keeps
    // that inactive sampler complete via Ogre's existing warning texture.
    // It owns no texture/RTT and is removed before active binding or reset.
    void bindWaterPass(Ogre::Pass&);
    // Explicit developer readback only: hidden + render capture, never perf.
    // At most four per component lifetime. Writes native linear float pixels,
    // an sRGB camera-oriented preview and facts; never edits the scene/view.
    void captureDiagnostic(const std::string& absoluteOutputPrefix) const;
    // Numeric view/storage facts for the strictly admitted owned World-edit
    // probe. No readback, retained resource pointer or render occurs here.
    std::string worldEditDiagnosticFacts() const;
    // Default-off water-transition probe: numeric input/update/pass/storage
    // observations, including inactive frames. Retains no resource reference.
    std::string transitionDiagnosticFacts(const Ogre::Pass&) const;
    // Call before destroying residents/SceneManager, including world switches.
    void resetWorld() noexcept;
    RenderLifecycleTargetFacts lifecycleFacts() const;
    void setLifecycleReleaseObserver(std::function<void(const char*,const std::string&,bool)> observer);
    const Statistics& statistics() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
