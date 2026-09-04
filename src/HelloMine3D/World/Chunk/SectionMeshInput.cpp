#include "SectionMeshInput.h"

#include "ChunkSection.h"
#include "../Generation/Terrain/TerrainGenerator.h"

#include <algorithm>

namespace {
// Order matches m_neighbourLayerAllSolid.
constexpr int kNeighbourOffsetX[4] = {1, 0, -1, 0};
constexpr int kNeighbourOffsetZ[4] = {0, 1, 0, -1};
} // namespace

int SectionMeshInput::index(int x, int y, int z)
{
    return (x + 1) + (z + 1) * Size + (y + 1) * Size * Size;
}

void SectionMeshInput::capture(
    ChunkSection &section, const TerrainGenerator &terrainGenerator,
    int terrainSeed)
{
    m_location = section.getLocation();
    m_terrainSeed = terrainSeed;

    for (int z = -1; z <= CHUNK_SIZE; ++z) {
        for (int x = -1; x <= CHUNK_SIZE; ++x) {
            const int worldX = m_location.x * CHUNK_SIZE + x;
            const int worldZ = m_location.z * CHUNK_SIZE + z;
            m_biomes[(x + 1) + (z + 1) * Size] =
                terrainGenerator.getBiomeAtWorld(worldX, worldZ);
        }
    }

    // ChunkSection::getBlock() resolves out-of-range coordinates through the
    // world, which is why this has to run under the world lock.
    for (int y = -1; y <= CHUNK_SIZE; ++y) {
        for (int z = -1; z <= CHUNK_SIZE; ++z) {
            for (int x = -1; x <= CHUNK_SIZE; ++x) {
                m_blocks[index(x, y, z)] = section.getBlock(x, y, z);
                m_sunlight[index(x, y, z)] =
                    section.getSunlight(x, y, z);
                m_blockLight[index(x, y, z)] =
                    section.getBlockLight(x, y, z);
            }
        }
    }

    for (int y = -1; y <= CHUNK_SIZE; ++y) {
        m_ownLayerAllSolid[y + 1] = section.getLayer(y).isAllSolid();
    }

    for (int neighbour = 0; neighbour < 4; ++neighbour) {
        const ChunkSection *adjacent = section.findAdjacent(
            kNeighbourOffsetX[neighbour], kNeighbourOffsetZ[neighbour]);
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            m_neighbourLayerAllSolid[neighbour][y] =
                adjacent != nullptr && adjacent->getLayer(y).isAllSolid();
        }
    }
}

LightLevel SectionMeshInput::getSunlight(int x, int y, int z) const
{
    if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 ||
        z > CHUNK_SIZE) {
        return MIN_LIGHT_LEVEL;
    }

    return m_sunlight[index(x, y, z)];
}

LightLevel SectionMeshInput::getBlockLight(int x, int y, int z) const
{
    if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 ||
        z > CHUNK_SIZE) {
        return MIN_LIGHT_LEVEL;
    }

    return m_blockLight[index(x, y, z)];
}

LightLevel SectionMeshInput::getCombinedLight(int x, int y, int z) const
{
    return std::max(getSunlight(x, y, z), getBlockLight(x, y, z));
}

TerrainBiome SectionMeshInput::getBiome(int x, int z) const
{
    if (x < -1 || x > CHUNK_SIZE || z < -1 || z > CHUNK_SIZE) {
        return TerrainBiome::Grassland;
    }
    return m_biomes[(x + 1) + (z + 1) * Size];
}

int SectionMeshInput::getTerrainSeed() const noexcept
{
    return m_terrainSeed;
}

ChunkBlock SectionMeshInput::getBlock(int x, int y, int z) const
{
    if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE || z < -1 ||
        z > CHUNK_SIZE) {
        return ChunkBlock(BlockId::Air);
    }

    return m_blocks[index(x, y, z)];
}

bool SectionMeshInput::shouldMakeLayer(int y) const
{
    if (y < 0 || y >= CHUNK_SIZE) {
        return false;
    }

    if (!m_ownLayerAllSolid[y + 1] || !m_ownLayerAllSolid[y + 2] ||
        !m_ownLayerAllSolid[y]) {
        return true;
    }

    for (int neighbour = 0; neighbour < 4; ++neighbour) {
        if (!m_neighbourLayerAllSolid[neighbour][y]) {
            return true;
        }
    }

    return false;
}

bool SectionMeshInput::needsMeshBuild() const
{
    for (int y = 0; y < CHUNK_SIZE; ++y) {
        if (shouldMakeLayer(y)) {
            return true;
        }
    }

    return false;
}

const glm::ivec3 &SectionMeshInput::getLocation() const
{
    return m_location;
}
