#ifndef BLOCKSELECTION_H_INCLUDED
#define BLOCKSELECTION_H_INCLUDED

#include <optional>

#include "../../Maths/glm.h"
#include "../Block/BlockId.h"

class World;

struct BlockSelection {
    glm::ivec3 blockPosition{0};
    glm::ivec3 placementPosition{0};
    glm::vec3 hitPoint{0.f};
    BlockId blockId = BlockId::Air;
};

class BlockSelectionSystem {
  public:
    static std::optional<BlockSelection>
    pick(World &world, const glm::vec3 &origin, const glm::vec3 &rotation,
         float maxDistance = 6.f, float stepSize = 0.05f);
};

#endif // BLOCKSELECTION_H_INCLUDED
