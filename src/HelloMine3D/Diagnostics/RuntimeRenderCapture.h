#pragma once

#include <SFML/System/Time.hpp>
#include <SFML/Window/Window.hpp>

namespace RuntimeRenderCapture
{
    void update(const sf::Window &window, sf::Time deltaTime);
    bool isEnabled();
    bool isComplete();
    bool shouldCloseWindow();
} // namespace RuntimeRenderCapture
