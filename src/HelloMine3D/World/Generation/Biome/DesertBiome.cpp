#include "DesertBiome.h"

#include "../../WorldConstants.h"
#include "../Structures/TreeGenerator.h"

namespace
{
    const BiomeDefinition &desertDefinition()
    {
        static const BiomeDefinition definition{
            NoiseParameters{9, 80, 335, -7, 0.56}, 1350, 500,
            ChunkBlock(BlockId::Sand), ChunkBlock(BlockId::Sand),
            ChunkBlock(BlockId::Sand), ChunkBlock(BlockId::DeadShrub)};
        return definition;
    }
}

DesertBiome::DesertBiome(int seed)
    : Biome(desertDefinition(), seed)
{
}

ChunkBlock DesertBiome::getTopBlock(Rand &rand) const
{
    (void)rand;
    return getDefinition().topBlock;
}

ChunkBlock DesertBiome::getUnderWaterBlock(Rand &rand) const
{
    (void)rand;
    return getDefinition().underWaterBlock;
}

void DesertBiome::makeTree(Rand &rand, Chunk &chunk, int x, int y, int z) const
{
    if (y < WATER_LEVEL + 15) {
        if (rand.intInRange(0, 100) > 75) {
            makePalmTree(chunk, rand, x, y, z);
        }
        else {
            makeCactus(chunk, rand, x, y, z);
        }
    }
    else {
        makeCactus(chunk, rand, x, y, z);
    }
}

ChunkBlock DesertBiome::getPlant(Rand &rand) const
{
    (void)rand;
    return getDefinition().plantBlock;
}
