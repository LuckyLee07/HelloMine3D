#pragma once

#include <SFML/System/Clock.hpp>
#include <SFML/Window/Window.hpp>

#include "Core/Camera.h"
#include "Input/Keyboard.h"
#include "Renderer/RenderMaster.h"
#include "Sandbox/SandboxRuntime.h"
float extern g_timeElapsed;


class Keyboard;

class Application
{
  public:
    Application(sf::Window& window, const Config& config);

    void on_event(const sf::Event& event);
    void on_update(const Keyboard& keyboard, sf::Time dt);
    void on_render(bool show_debug_info);
    WorldDebugStats collectDebugStats();

  private:
    sf::Window& m_window;
    RenderMaster m_masterRenderer;
    Camera m_camera;
    SandboxRuntime m_sandboxRuntime;
};
