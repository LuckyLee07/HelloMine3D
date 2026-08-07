#include "GrasslandBiome.h"

#include "../Structures/TreeGenerator.h"

namespace
{
    const BiomeDefinition &grasslandDefinition()
    {
        static const BiomeDefinition definition{
            NoiseParameters{9, 85, 235, -20, 0.51}, 1000, 20,
            ChunkBlock(BlockId::Grass), ChunkBlock(BlockId::Sand),
            ChunkBlock(BlockId::Dirt), ChunkBlock(BlockId::TallGrass)};
        return definition;
    }
}

GrasslandBiome::GrasslandBiome(int seed)
    : Biome(grasslandDefinition(), seed)
{
}

ChunkBlock GrasslandBiome::getTopBlock(Rand &rand) const
{
    (void)rand;
    return getDefinition().topBlock;
}

ChunkBlock GrasslandBiome::getUnderWaterBlock(Rand &rand) const
{
    return rand.intInRange(0, 10) > 8 ? BlockId::Dirt : BlockId::Sand;
}

ChunkBlock GrasslandBiome::getBeachBlock(Rand &rand) const
{
    return rand.intInRange(0, 10) > 2 ? BlockId::Grass : BlockId::Dirt;
}

void GrasslandBiome::makeTree(Rand &rand, Chunk &chunk, int x, int y,
                              int z) const
{
    makeOakTree(chunk, rand, x, y, z);
}

ChunkBlock GrasslandBiome::getPlant(Rand &rand) const
{
    return rand.intInRange(0, 10) > 6 ? BlockId::Rose
                                      : getDefinition().plantBlock;
}
