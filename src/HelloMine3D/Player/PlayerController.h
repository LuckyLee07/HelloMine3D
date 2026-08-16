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
    int hotbarDelta = 0;
    int hotbarSlot = -1;
    // Device boundaries convert relative mouse pixels into degrees before
    // submitting the frame input.
    glm::vec2 lookDelta{0.f};
};

class PlayerController {
  public:
    // Samples held controls and applies frame-local actions such as look,
    // hotbar selection and flight toggles. Repeating the same sample must not
    // add movement impulse.
    void applyInput(Player &player, const PlayerInputState &input);
    // Consumes the most recent held state exactly once per fixed simulation
    // tick.
    void applyMovement(Player &player, float dt);
};

#endif // PLAYERCONTROLLER_H_INCLUDED
