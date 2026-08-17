#ifndef BLOCKINTERACTIONSYSTEM_H_INCLUDED
#define BLOCKINTERACTIONSYSTEM_H_INCLUDED

#include "../../Maths/glm.h"
#include "../Block/BlockId.h"

class ItemStack;

class Player;
class World;

struct BlockMiningEvaluation {
    float requiredSeconds = 0.25f;
    float speedMultiplier = 1.0f;
    bool matchingClass = false;
    bool meetsTier = false;
    bool dropAllowed = true;
};

class BlockInteractionSystem {
  public:
    static BlockMiningEvaluation evaluateMining(
        BlockId blockId, const ItemStack &heldItem);
    static bool breakBlock(World &world, Player &player,
                           const glm::vec3 &location);
    static bool placeBlock(World &world, Player &player,
                           const glm::vec3 &location);
    static bool useBlock(World &world, Player &player,
                         const glm::vec3 &location);
};

#endif // BLOCKINTERACTIONSYSTEM_H_INCLUDED
