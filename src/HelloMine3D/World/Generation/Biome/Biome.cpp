#include "Biome.h"

Biome::Biome(const BiomeDefinition &definition, int seed)
    : m_definition(definition)
    , m_heightGenerator(seed)
{
    m_heightGenerator.setParameters(m_definition.heightNoise);
}

ChunkBlock Biome::getBeachBlock(Rand &rand) const
{
    (void)rand;
    return m_definition.beachBlock;
}

int Biome::getHeight(int x, int z, int chunkX, int chunkZ) const
{
    return static_cast<int>(m_heightGenerator.getHeight(x, z, chunkX, chunkZ));
}

int Biome::getTreeFrequency() const noexcept
{
    return m_definition.treeFrequency;
}

int Biome::getPlantFrequency() const noexcept
{
    return m_definition.plantFrequency;
}

const BiomeDefinition &Biome::getDefinition() const noexcept
{
    return m_definition;
}
