#ifndef BLOCKINTERACTIONSYSTEM_H_INCLUDED
#define BLOCKINTERACTIONSYSTEM_H_INCLUDED

#include "../../Maths/glm.h"

class Player;
class World;

class BlockInteractionSystem {
  public:
    static bool breakBlock(World &world, Player &player,
                           const glm::vec3 &location);
    static bool placeBlock(World &world, Player &player,
                           const glm::vec3 &location);
};

#endif // BLOCKINTERACTIONSYSTEM_H_INCLUDED
