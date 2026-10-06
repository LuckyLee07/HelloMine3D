#pragma once

#include <OgreRenderTargetListener.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../Config.h"
#include "../Diagnostics/CrashReportInbox.h"
#include "../Gameplay/DifficultyProfile.h"

namespace Ogre
{
    class Camera;
    class RenderWindow;
    class SceneManager;
}

namespace OIS
{
    class Keyboard;
    class KeyEvent;
    class MouseEvent;
}

class Player;
class World;
class GameApplicationFlow;
class WorldManagementService;
class PauseNotificationCapture;
struct WorldDebugStats;
struct MiningProgressSnapshot;
struct ActionFeedbackSnapshot;

enum class OgreUserInterfaceActionType
{
    None,
    OpenWorld,
    ApplySettings,
    ApplyDifficulty,
    ClaimVictoryReward,
    SelectHotbar,
    ReturnToMainMenu,
    Quit,
    OpenWorldBackups
};

struct OgreUserInterfaceAction
{
    OgreUserInterfaceActionType type = OgreUserInterfaceActionType::None;
    std::string worldId;
    UserSettings settings;
    WorldDifficulty difficulty = WorldDifficulty::Normal;
    int hotbarSlot = -1;
};

struct OgreUserInterfaceValidation
{
    bool valid = false;
    bool debugPanelVisible = false;
    std::size_t hotbarSlots = 0;
    int selectedSlot = -1;
    bool containerOpen = false;
    std::string message;
};

enum class SurfaceMapDiagnosticView { Hud, Flat };
enum class SurfaceMapDiagnosticLayer { None, LiveCoarse, FineHistory };

// Bounded copied facts from the actual UI. These do not own World or ImGui
// objects, request terrain, or claim a diagnostic world-coordinate resolution
// was a normal mouse click. Framebuffer evidence is captured by Bootstrap.
struct OgreSurfaceMapDiagnosticSample
{
    bool known = false;
    int height = 0;
    int blockId = 0;
};

struct OgreSurfaceMapDiagnosticCell
{
    bool available = false;
    int worldX = 0, worldZ = 0, step = 0;
    OgreSurfaceMapDiagnosticSample surface;
};

struct OgreSurfaceMapDiagnosticDraw
{
    bool submitted = false;
    SurfaceMapDiagnosticLayer layer = SurfaceMapDiagnosticLayer::None;
    int worldX = 0, worldZ = 0, step = 0;
    OgreSurfaceMapDiagnosticSample surface;
    std::uint32_t colour = 0;
    float left = 0.f, top = 0.f, right = 0.f, bottom = 0.f;
    std::size_t vertexBegin = 0, vertexEnd = 0;
    std::size_t indexBegin = 0, indexEnd = 0;
};

struct OgreSurfaceMapDiagnosticFacts
{
    bool enabled = false;
    std::uint64_t frameId = 0;
    int targetX = 0, targetZ = 0;
    bool mapPageActive = false;
    SurfaceMapDiagnosticView activeView = SurfaceMapDiagnosticView::Hud;
    int minimapStep = 0;
    bool sampledThisFrame = false;
    SurfaceMapDiagnosticView queryView = SurfaceMapDiagnosticView::Hud;
    int queryStep = 0, queryCenterX = 0, queryCenterZ = 0;
    std::size_t queryCount = 0, sampleCount = 0;
    bool sizeMatched = false, targetQueried = false;
    bool targetSampleAvailable = false;
    OgreSurfaceMapDiagnosticSample targetSample;
    std::uint64_t targetObservedFrame = 0;
    SurfaceMapDiagnosticView targetObservedView = SurfaceMapDiagnosticView::Hud;
    int targetObservedStep = 0;
    OgreSurfaceMapDiagnosticSample lastObservedTarget;
    OgreSurfaceMapDiagnosticCell fineTarget, fineWest, fineNorth;
    OgreSurfaceMapDiagnosticCell flatLiveTarget, diagnosticResolution;
    std::size_t fineTileCount = 0, flatPendingCount = 0;
    int flatCursor = 0;
    bool flatNextBatchContainsTarget = false;
    OgreSurfaceMapDiagnosticDraw drawTarget, drawWest, drawNorth;
    // The actual hovered/selected branch, independent of diagnosticResolution.
    OgreSurfaceMapDiagnosticCell actualInspection;
    bool backendSubmitted = false;
    std::size_t backendVertexCount = 0, backendIndexCount = 0;
    float framebufferScaleX = 1.f, framebufferScaleY = 1.f;
};

class OgreUserInterface final : public Ogre::RenderTargetListener
{
  public:
    OgreUserInterface(Ogre::RenderWindow &window,
                      Ogre::SceneManager &sceneManager,
                      Ogre::Camera &camera, Player *player, World *world,
                      GameApplicationFlow &applicationFlow,
                      WorldManagementService &worldManagement,
                      const UserSettings &settings,
                      std::string presentationFontPath,
                      std::function<void()> uiFeedback = {},
                      std::vector<PendingCrashReport> crashReports = {});
    ~OgreUserInterface() override;

    OgreUserInterface(const OgreUserInterface &) = delete;
    OgreUserInterface &operator=(const OgreUserInterface &) = delete;

    void beginFrame(float deltaSeconds, const WorldDebugStats &worldStats,
                    const MiningProgressSnapshot &miningProgress,
                    const ActionFeedbackSnapshot &actionFeedback);
    void focusChanged(bool focused);
    void keyEvent(const OIS::KeyEvent &event, bool pressed,
                  const OIS::Keyboard &keyboard);
    void mouseMoved(const OIS::MouseEvent &event);
    void mouseButton(const OIS::MouseEvent &event,
                     int button, bool pressed);
    bool wantsKeyboardInput() const;
    bool wantsMouseInput() const;
    bool wantsHudPointer() const noexcept;
    bool toggleHudPointer() noexcept;
    bool dismissHudInteraction() noexcept;
    bool hasBlockingModal() const noexcept;
    bool isDebugPanelVisible() const noexcept;
    void setWorldContext(Player *player, World *world) noexcept;
    void setFirstPersonPresentationVisible(bool visible) noexcept;
    bool isFirstPersonPresentationVisible() const noexcept;
    // Explicit diagnostic only; the normal client has no active observer.
    bool setMaterialIdentityMap3dVisible(bool visible) noexcept;
    // Explicit isolated diagnostic only. Configuring the target does not open
    // a page or change any map cache, sampling queue, deadline, or selection.
    void configureSurfaceMapDiagnostic(int targetX, int targetZ) noexcept;
    bool setSurfaceMapDiagnosticView(SurfaceMapDiagnosticView view) noexcept;
    OgreSurfaceMapDiagnosticFacts surfaceMapDiagnosticFacts() const noexcept;
    void clearSurfaceMapDiagnostic() noexcept;
    void setThirdPersonAimIndicator(bool visible, float normalizedX,
                                    float normalizedY) noexcept;
    // Optional diagnostic observer; non-owning, normal client remains null.
    void setPauseNotificationCapture(PauseNotificationCapture* observer) noexcept;
    void setStatusMessage(std::string message);
    void setRenderPipelineFallback(bool fallback) noexcept;
    void showWorldBackups(const std::string& worldId);
    void setAudioCaption(std::string cueId, std::string caption);
    bool dismissSettings() noexcept;
    void reportSettingsApplied(bool succeeded,
                               const UserSettings &settings,
                               std::string message);
    OgreUserInterfaceAction consumeAction();

    void postViewportUpdate(
        const Ogre::RenderTargetViewportEvent &event) override;

    static OgreUserInterfaceValidation validateConfiguration(
        const Player &player);

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
