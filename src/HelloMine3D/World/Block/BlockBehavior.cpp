#include "BlockBehavior.h"

#include "BlockDefinition.h"
#include "ChunkBlock.h"

Material::ID
BlockBehavior::getDrop(const BlockDefinition &definition,
                       const ChunkBlock &) const
{
    return definition.defaultDrop;
}

int BlockBehavior::emission(const BlockDefinition &definition,
                            const ChunkBlock &) const noexcept
{
    return definition.light;
}

bool BlockBehavior::receivesRandomTicks(const BlockDefinition &,
                                        const ChunkBlock &) const noexcept
{
    return false;
}

void BlockBehavior::onRandomTick(World &, const glm::ivec3 &,
                                 const ChunkBlock &) const
{
}

bool BlockBehavior::canPlace(World &, const Player &, const glm::ivec3 &,
                             const ChunkBlock &) const
{
    return true;
}

float BlockBehavior::verticalRenderScale(
    const BlockDefinition &, const ChunkBlock &) const noexcept
{
    return 1.f;
}

void BlockBehavior::onPlaced(World &, Player &, const glm::ivec3 &,
                             const ChunkBlock &, const ChunkBlock &) const
{
}

void BlockBehavior::onBroken(World &, Player &, const glm::ivec3 &,
                             const ChunkBlock &) const
{
}

bool BlockBehavior::supportsUse() const noexcept
{
    return false;
}

void BlockBehavior::onUse(World &, Player &, const glm::ivec3 &,
                          const ChunkBlock &) const
{
}
