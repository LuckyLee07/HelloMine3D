#ifndef BLOCKBEHAVIOR_H_INCLUDED
#define BLOCKBEHAVIOR_H_INCLUDED

#include "../../Item/Material.h"
#include "../../Maths/glm.h"

class Player;
class World;
struct BlockDefinition;
struct ChunkBlock;

/// Lightweight extension points for behavior that differs between blocks.
/// Static render and collision data stays in BlockDefinition.
class BlockBehavior {
  public:
    virtual ~BlockBehavior() = default;

    virtual Material::ID getDrop(const BlockDefinition &definition,
                                 const ChunkBlock &block) const;
    virtual int emission(const BlockDefinition &definition,
                         const ChunkBlock &block) const noexcept;
    virtual bool receivesRandomTicks(const BlockDefinition &definition,
                                     const ChunkBlock &block) const noexcept;
    virtual void onRandomTick(World &world, const glm::ivec3 &position,
                              const ChunkBlock &block) const;
    virtual bool canPlace(World &world, const Player &player,
                          const glm::ivec3 &position,
                          const ChunkBlock &previousBlock) const;
    virtual float verticalRenderScale(
        const BlockDefinition &definition,
        const ChunkBlock &block) const noexcept;
    virtual void onPlaced(World &world, Player &player,
                          const glm::ivec3 &position,
                          const ChunkBlock &previousBlock,
                          const ChunkBlock &placedBlock) const;
    virtual void onBroken(World &world, Player &player,
                          const glm::ivec3 &position,
                          const ChunkBlock &brokenBlock) const;
    virtual void onUse(World &world, Player &player,
                       const glm::ivec3 &position,
                       const ChunkBlock &block) const;
};

#endif // BLOCKBEHAVIOR_H_INCLUDED
