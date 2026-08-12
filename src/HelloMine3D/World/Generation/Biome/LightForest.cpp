#include "LightForest.h"

#include "../Structures/TreeGenerator.h"

namespace
{
    const BiomeDefinition &lightForestDefinition()
    {
        static const BiomeDefinition definition{
            NoiseParameters{5, 100, 195, -32, 0.52}, 60, 80,
            ChunkBlock(BlockId::Grass), ChunkBlock(BlockId::Dirt),
            ChunkBlock(BlockId::Sand),
            ChunkBlock(BlockId::TallGrass,
                       BlockMetadata::TallGrass::Mature)};
        return definition;
    }
}

LightForest::LightForest(int seed)
    : Biome(lightForestDefinition(), seed)
{
}

ChunkBlock LightForest::getTopBlock(Rand &rand) const
{
    (void)rand;
    return getDefinition().topBlock;
}

ChunkBlock LightForest::getUnderWaterBlock(Rand &rand) const
{
    return rand.intInRange(0, 10) > 9 ? BlockId::Sand : BlockId::Dirt;
}

void LightForest::makeTree(Rand &rand, Chunk &chunk, int x, int y, int z) const
{
    makeOakTree(chunk, rand, x, y, z);
}

ChunkBlock LightForest::getPlant(Rand &rand) const
{
    return rand.intInRange(0, 10) > 8 ? BlockId::Rose
                                      : getDefinition().plantBlock;
}
