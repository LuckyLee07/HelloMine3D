#ifndef PLAYERCONTROLLER_H_INCLUDED
#define PLAYERCONTROLLER_H_INCLUDED

#include "../Maths/glm.h"

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
    void applyInput(Player &player, const PlayerInputState &input);
};

#endif // PLAYERCONTROLLER_H_INCLUDED
