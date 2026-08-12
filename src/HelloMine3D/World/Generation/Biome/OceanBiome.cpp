#include "OceanBiome.h"

#include "../Structures/TreeGenerator.h"

namespace
{
    const BiomeDefinition &oceanDefinition()
    {
        static const BiomeDefinition definition{
            NoiseParameters{7, 43, 55, 0, 0.50}, 50, 100,
            ChunkBlock(BlockId::Grass), ChunkBlock(BlockId::Sand),
            ChunkBlock(BlockId::Sand),
            ChunkBlock(BlockId::TallGrass,
                       BlockMetadata::TallGrass::Mature)};
        return definition;
    }
}

OceanBiome::OceanBiome(int seed)
    : Biome(oceanDefinition(), seed)
{
}

ChunkBlock OceanBiome::getTopBlock(Rand &rand) const
{
    (void)rand;
    return getDefinition().topBlock;
}

ChunkBlock OceanBiome::getUnderWaterBlock(Rand &rand) const
{
    (void)rand;
    return getDefinition().underWaterBlock;
}

void OceanBiome::makeTree(Rand &rand, Chunk &chunk, int x, int y, int z) const
{
    rand.intInRange(0, 5) < 3 ? makePalmTree(chunk, rand, x, y, z)
                              : makeOakTree(chunk, rand, x, y, z);
}

ChunkBlock OceanBiome::getPlant(Rand &rand) const
{
    return rand.intInRange(0, 10) > 6 ? BlockId::Rose
                                      : getDefinition().plantBlock;
}
