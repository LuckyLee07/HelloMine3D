#ifndef BLOCKSELECTION_H_INCLUDED
#define BLOCKSELECTION_H_INCLUDED

#include <optional>

#include "../../Actor/ActorTypes.h"
#include "../../Maths/glm.h"
#include "../Block/BlockId.h"

class World;

struct BlockSelection {
    glm::ivec3 blockPosition{0};
    glm::ivec3 placementPosition{0};
    glm::vec3 hitPoint{0.f};
    BlockId blockId = BlockId::Air;
};

struct ActorSelection {
    ActorId actorId = InvalidActorId;
    glm::vec3 hitPoint{0.f};
    float distance = 0.f;
};

struct PlayerTargetSelection {
    std::optional<BlockSelection> block;
    std::optional<ActorSelection> actor;
};

class BlockSelectionSystem {
  public:
    static std::optional<BlockSelection>
    pick(World &world, const glm::vec3 &origin, const glm::vec3 &rotation,
         float maxDistance = 6.f, float stepSize = 0.05f);
};

class ActorSelectionSystem {
  public:
    static std::optional<ActorSelection>
    pick(World &world, const glm::vec3 &origin, const glm::vec3 &rotation,
         float maxDistance = 6.f, float stepSize = 0.05f);
};

class PlayerTargetSelectionSystem {
  public:
    static PlayerTargetSelection
    pick(World &world, const glm::vec3 &origin, const glm::vec3 &rotation,
         float maxDistance = 6.f, float stepSize = 0.05f);
};

#endif // BLOCKSELECTION_H_INCLUDED
