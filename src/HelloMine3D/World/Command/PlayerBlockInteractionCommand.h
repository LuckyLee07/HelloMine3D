#ifndef PLAYERBLOCKINTERACTIONCOMMAND_H_INCLUDED
#define PLAYERBLOCKINTERACTIONCOMMAND_H_INCLUDED

#include "../../Maths/glm.h"
#include "IWorldCommand.h"

class Player;

enum class PlayerBlockInteractionAction {
    Break,
    Place,
    Use,
};

/// Requests one player block interaction through the World command FIFO.
class PlayerBlockInteractionCommand : public IWorldCommand {
  public:
    PlayerBlockInteractionCommand(PlayerBlockInteractionAction action,
                                  const glm::vec3 &location,
                                  Player &player);

    void execute(World &world) override;

  private:
    void interact(World &world);

    PlayerBlockInteractionAction m_action;
    glm::vec3 m_blockPosition;
    Player *m_player;
};

#endif // PLAYERBLOCKINTERACTIONCOMMAND_H_INCLUDED
