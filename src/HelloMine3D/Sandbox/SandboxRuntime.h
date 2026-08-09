#ifndef SANDBOXRUNTIME_H_INCLUDED
#define SANDBOXRUNTIME_H_INCLUDED

#include <SFML/System/Clock.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Window.hpp>

#include "../Config.h"
#include "../Core/Camera.h"
#include "../Input/Keyboard.h"
#include "../Player/Player.h"
#include "../Util/NonCopyable.h"
#include "../World/World.h"
#include "FixedTickScheduler.h"
#include "WorldManager.h"

class RenderMaster;
class World;

class SandboxRuntime : public NonCopyable {
  public:
    SandboxRuntime(sf::Window &window, const Config &config, Camera &camera);

    void onEvent(const sf::Event &event);
    void update(const Keyboard &keyboard, sf::Time dt);
    void render(RenderMaster &renderer, bool showDebugInfo);
    WorldDebugStats collectDebugStats();

    Player &getPlayer();
    WorldManager &getWorldManager();

  private:
    void handlePlayerInteraction();
    void runFixedTicks(sf::Time dt);
    void drawSandboxDebug(World &world);

    sf::Window &m_window;
    Camera &m_camera;
    Player m_player;
    WorldManager m_worldManager;
    FixedTickScheduler m_tickScheduler;
    const sf::Time m_fixedTickStep = sf::seconds(1.f / 20.f);
    sf::Clock m_interactionTimer;
};

#endif // SANDBOXRUNTIME_H_INCLUDED
