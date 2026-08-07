#include "Application.h"

#include <SFML/Window/Event.hpp>

float g_timeElapsed = 0;

Application::Application(sf::Window& window, const Config& config)
    : m_window(window)
    , m_camera(config)
    , m_sandboxRuntime(window, config, m_camera)
{
}

void Application::on_event(const sf::Event& event)
{
    m_sandboxRuntime.onEvent(event);
}

void Application::on_update(const Keyboard& keyboard, sf::Time dt)
{
    m_sandboxRuntime.update(keyboard, dt);
}

void Application::on_render(bool show_debug_info)
{
    m_sandboxRuntime.render(m_masterRenderer, show_debug_info);
}

WorldDebugStats Application::collectDebugStats()
{
    return m_sandboxRuntime.collectDebugStats();
}
