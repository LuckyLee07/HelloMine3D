#ifndef PLAYERDIGEVENT_H_INCLUDED
#define PLAYERDIGEVENT_H_INCLUDED

#include "../../Maths/glm.h"
#include "IWorldEvent.h"

class Player;

enum class PlayerDigAction {
    Break,
    Place,
    Use,
};

/// @brief Event class that handles what happens to a block in a world as a player interacts.
class PlayerDigEvent : public IWorldEvent {
  public:
    PlayerDigEvent(PlayerDigAction action, const glm::vec3 &location,
                   Player &player);

    void handle(World &world);

  private:
    void dig(World &world);

    PlayerDigAction m_action;
    glm::vec3 m_digSpot;
    Player *m_pPlayer;
};

#endif // PLAYERDIGEVENT_H_INCLUDED
