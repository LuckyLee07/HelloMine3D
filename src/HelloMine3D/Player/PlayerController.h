#ifndef PLAYERCONTROLLER_H_INCLUDED
#define PLAYERCONTROLLER_H_INCLUDED

#include <SFML/Graphics.hpp>

#include "../Input/ToggleKey.h"
#include "../Maths/glm.h"

class Keyboard;
class Player;

struct PlayerInputState {
    bool moveForward = false;
    bool moveBackward = false;
    bool moveLeft = false;
    bool moveRight = false;
    bool sprint = false;
    bool jump = false;
    bool descend = false;
    bool toggleFlying = false;
    bool toggleSneaking = false;
    int hotbarDelta = 0;
    int hotbarSlot = -1;
    glm::vec2 lookDelta{0.f};
};

class PlayerController {
  public:
    PlayerController();

    void handleInput(Player &player, const sf::Window &window,
                     const Keyboard &keyboard);
    void applyInput(Player &player, const PlayerInputState &input);

  private:
    PlayerInputState collectInput(const sf::Window &window,
                                  const Keyboard &keyboard);
    glm::vec2 collectMouseDelta(const sf::Window &window);

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
