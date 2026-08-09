#pragma once

#include <OgreRenderQueueListener.h>

#include <cstddef>
#include <memory>
#include <string>

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
struct WorldDebugStats;

struct OgreUserInterfaceValidation
{
    bool valid = false;
    bool debugPanelVisible = false;
    std::size_t hotbarSlots = 0;
    int selectedSlot = -1;
    std::string message;
};

class OgreUserInterface final : public Ogre::RenderQueueListener
{
  public:
    OgreUserInterface(Ogre::RenderWindow &window,
                      Ogre::SceneManager &sceneManager,
                      Ogre::Camera &camera, Player &player);
    ~OgreUserInterface() override;

    OgreUserInterface(const OgreUserInterface &) = delete;
    OgreUserInterface &operator=(const OgreUserInterface &) = delete;

    void beginFrame(float deltaSeconds, const WorldDebugStats &worldStats);
    void keyEvent(const OIS::KeyEvent &event, bool pressed,
                  const OIS::Keyboard &keyboard);
    void mouseMoved(const OIS::MouseEvent &event);
    void mouseButton(const OIS::MouseEvent &event,
                     int button, bool pressed);
    bool wantsKeyboardInput() const;
    bool wantsMouseInput() const;
    bool isDebugPanelVisible() const noexcept;

    void postRenderQueues() override;

    static OgreUserInterfaceValidation validateConfiguration(
        const Player &player);

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
