#include "BlockBehavior.h"

#include "BlockDefinition.h"
#include "ChunkBlock.h"

Material::ID
BlockBehavior::getDrop(const BlockDefinition &definition,
                       const ChunkBlock &) const
{
    return definition.defaultDrop;
}

void BlockBehavior::onPlaced(World &, Player &, const glm::ivec3 &,
                             const ChunkBlock &, const ChunkBlock &) const
{
}

void BlockBehavior::onBroken(World &, Player &, const glm::ivec3 &,
                             const ChunkBlock &) const
{
}

void BlockBehavior::onUse(World &, Player &, const glm::ivec3 &,
                          const ChunkBlock &) const
{
}
