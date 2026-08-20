#pragma once

#include <OgreRenderQueueListener.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "../Config.h"

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
struct WorldDebugStats;
struct MiningProgressSnapshot;

enum class OgreUserInterfaceActionType
{
    None,
    OpenWorld,
    ApplySettings,
    ReturnToMainMenu,
    Quit
};

struct OgreUserInterfaceAction
{
    OgreUserInterfaceActionType type = OgreUserInterfaceActionType::None;
    std::string worldId;
    UserSettings settings;
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

class OgreUserInterface final : public Ogre::RenderQueueListener
{
  public:
    OgreUserInterface(Ogre::RenderWindow &window,
                      Ogre::SceneManager &sceneManager,
                      Ogre::Camera &camera, Player *player, World *world,
                      GameApplicationFlow &applicationFlow,
                      WorldManagementService &worldManagement,
                      const UserSettings &settings,
                      std::function<void()> uiFeedback = {});
    ~OgreUserInterface() override;

    OgreUserInterface(const OgreUserInterface &) = delete;
    OgreUserInterface &operator=(const OgreUserInterface &) = delete;

    void beginFrame(float deltaSeconds, const WorldDebugStats &worldStats,
                    const MiningProgressSnapshot &miningProgress);
    void keyEvent(const OIS::KeyEvent &event, bool pressed,
                  const OIS::Keyboard &keyboard);
    void mouseMoved(const OIS::MouseEvent &event);
    void mouseButton(const OIS::MouseEvent &event,
                     int button, bool pressed);
    bool wantsKeyboardInput() const;
    bool wantsMouseInput() const;
    bool isDebugPanelVisible() const noexcept;
    void setWorldContext(Player *player, World *world) noexcept;
    void setStatusMessage(std::string message);
    void setAudioCaption(std::string caption);
    bool dismissSettings() noexcept;
    void reportSettingsApplied(bool succeeded,
                               const UserSettings &settings,
                               std::string message);
    OgreUserInterfaceAction consumeAction();

    void postRenderQueues() override;

    static OgreUserInterfaceValidation validateConfiguration(
        const Player &player);

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
