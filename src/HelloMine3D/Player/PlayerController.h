#ifndef PLAYERCONTROLLER_H_INCLUDED
#define PLAYERCONTROLLER_H_INCLUDED

#include <SFML/Graphics.hpp>

#include "../Input/ToggleKey.h"

class Keyboard;
class Player;

class PlayerController {
  public:
    PlayerController();

    void handleInput(Player &player, const sf::Window &window,
                     const Keyboard &keyboard);

  private:
    void keyboardInput(Player &player, const Keyboard &keyboard);
    void mouseInput(Player &player, const sf::Window &window);

    ToggleKey m_itemDown;
    ToggleKey m_itemUp;
    ToggleKey m_flyKey;

    ToggleKey m_num1;
    ToggleKey m_num2;
    ToggleKey m_num3;
    ToggleKey m_num4;
    ToggleKey m_num5;

    ToggleKey m_slow;
    ToggleKey m_useMouseKey;

    bool m_useMouse = true;
    bool m_hasLastMousePosition = false;
    sf::Vector2i m_lastMousePosition;
};

#endif // PLAYERCONTROLLER_H_INCLUDED
