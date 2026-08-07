#include "TemperateForestBiome.h"

#include "../Structures/TreeGenerator.h"

namespace
{
    const BiomeDefinition &temperateForestDefinition()
    {
        static const BiomeDefinition definition{
            NoiseParameters{5, 100, 195, -30, 0.52}, 55, 75,
            ChunkBlock(BlockId::Grass), ChunkBlock(BlockId::Sand),
            ChunkBlock(BlockId::Sand), ChunkBlock(BlockId::TallGrass)};
        return definition;
    }
}

TemperateForestBiome::TemperateForestBiome(int seed)
    : Biome(temperateForestDefinition(), seed)
{
}

ChunkBlock TemperateForestBiome::getTopBlock(Rand &rand) const
{
    return rand.intInRange(0, 10) < 8 ? BlockId::Grass : BlockId::Dirt;
}

ChunkBlock TemperateForestBiome::getUnderWaterBlock(Rand &rand) const
{
    return rand.intInRange(0, 10) > 8 ? BlockId::Dirt : BlockId::Sand;
}

void TemperateForestBiome::makeTree(Rand &rand, Chunk &chunk, int x, int y,
                                    int z) const
{
    makeOakTree(chunk, rand, x, y, z);
}

ChunkBlock TemperateForestBiome::getPlant(Rand &rand) const
{
    (void)rand;
    return getDefinition().plantBlock;
}
