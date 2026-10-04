#include "SectionMeshInput.h"

#include "ChunkSection.h"
#include "NaturalTreeRootTag.h"
#include "../Generation/Terrain/TerrainGenerator.h"

#include <algorithm>

namespace {
// Order matches m_neighbourLayerAllSolid.
constexpr int kNeighbourOffsetX[4] = {1, 0, -1, 0};
constexpr int kNeighbourOffsetZ[4] = {0, 1, 0, -1};

bool isTreeBlock(ChunkBlock block)
{
    const auto id = static_cast<BlockId>(block.id);
    return id == BlockId::OakBark || id == BlockId::OakLeaf ||
           id == BlockId::Cactus;
}
} // namespace

int SectionMeshInput::index(int x, int y, int z)
{
    return (x + 1) + (z + 1) * Size + (y + 1) * Size * Size;
}

void SectionMeshInput::capture(
    ChunkSection &section, const TerrainGenerator &terrainGenerator,
    int terrainSeed, bool omitEnclosedPayload)
{
    m_location = section.getLocation();
    m_terrainSeed = terrainSeed;
    m_containsWater = false;
    m_naturalTreeRootTags.fill(0);
    static_assert(sizeof(m_naturalTreeRootTags) == 8192,
                  "Natural tree ownership uses at most 8 KiB per snapshot");
    // Decide enclosure before copying 18^3 cells or querying climate. The
    // same flags still govern the builder; only unused snapshot work is cut.
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
    if (omitEnclosedPayload && !needsMeshBuild()) {
        return;
    }
    static_assert(CHUNK_SIZE == 16, "Ecology colour grid follows section size");
    m_ecologyColour.capture(m_location.x * CHUNK_SIZE, m_location.z * CHUNK_SIZE,
        [&terrainGenerator](int x, int z) {
            return terrainGenerator.getBiomeAtWorld(x, z);
        });

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
    bool containsTreeCandidate = false;
    for (int y = -1; y <= CHUNK_SIZE; ++y) {
        for (int z = -1; z <= CHUNK_SIZE; ++z) {
            for (int x = -1; x <= CHUNK_SIZE; ++x) {
                m_blocks[index(x, y, z)] = section.getBlock(x, y, z);
                if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE &&
                    z >= 0 && z < CHUNK_SIZE)
                    containsTreeCandidate = containsTreeCandidate ||
                        isTreeBlock(m_blocks[index(x, y, z)]);
                m_containsWater = m_containsWater ||
                    m_blocks[index(x, y, z)] == BlockId::Water;
                m_sunlight[index(x, y, z)] =
                    section.getSunlight(x, y, z);
                m_blockLight[index(x, y, z)] =
                    section.getBlockLight(x, y, z);
            }
        }
    }

    if (containsTreeCandidate) {
        const std::int64_t minimumX =
            static_cast<std::int64_t>(m_location.x) * CHUNK_SIZE;
        const std::int64_t minimumY =
            static_cast<std::int64_t>(m_location.y) * CHUNK_SIZE;
        const std::int64_t minimumZ =
            static_cast<std::int64_t>(m_location.z) * CHUNK_SIZE;
        std::size_t ownershipBlocks = 0;
        bool exceededOwnershipBudget = false;
        const bool supported = terrainGenerator.visitNaturalTreeOwnership(
            m_location.x, m_location.y, m_location.z,
            [&](const NaturalTreeOwnershipBlock &owned) {
                if (++ownershipBlocks > NaturalTreeMaximumSectionOwnershipBlocks) {
                    exceededOwnershipBudget = true;
                    return;
                }
                const auto x = static_cast<std::int64_t>(owned.position[0]) - minimumX;
                const auto y = static_cast<std::int64_t>(owned.position[1]) - minimumY;
                const auto z = static_cast<std::int64_t>(owned.position[2]) - minimumZ;
                const auto rootX = static_cast<std::int64_t>(owned.root[0]) - minimumX;
                const auto rootZ = static_cast<std::int64_t>(owned.root[1]) - minimumZ;
                if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE ||
                    z < 0 || z >= CHUNK_SIZE ||
                    rootX < NaturalTreeRootTag::MinimumCoordinate ||
                    rootX > NaturalTreeRootTag::MaximumCoordinate ||
                    rootZ < NaturalTreeRootTag::MinimumCoordinate ||
                    rootZ > NaturalTreeRootTag::MaximumCoordinate ||
                    !isTreeBlock(owned.block) ||
                    getBlock(static_cast<int>(x), static_cast<int>(y),
                             static_cast<int>(z)) != owned.block)
                    return;
                m_naturalTreeRootTags[static_cast<std::size_t>(
                    x + CHUNK_SIZE * (z + CHUNK_SIZE * y))] =
                    NaturalTreeRootTag::encode(static_cast<int>(rootX),
                                               static_cast<int>(rootZ));
            });
        if (!supported || exceededOwnershipBudget)
            m_naturalTreeRootTags.fill(0);
    }

    for (int z = -1; z <= CHUNK_SIZE; ++z) {
        for (int x = -1; x <= CHUNK_SIZE; ++x) {
            int depth = 0;
            auto &velocity=m_waterVelocity[(x+1)+(z+1)*Size];
            velocity=glm::vec2(0.f);
            // Only natural sea-level water needs a graph/climate hint. Saved
            // depth, banks and all other heights still use resident blocks.
            const int surfaceY=64-m_location.y*CHUNK_SIZE;
            if(getBlock(x,surfaceY,z)==BlockId::Water) {
                const auto motion=terrainGenerator.getWaterSurfaceVelocityAtWorld(
                    m_location.x*CHUNK_SIZE+x,m_location.z*CHUNK_SIZE+z);
                velocity={motion[0],motion[1]};
            }
            static_assert(sizeof(m_waterVelocity)<=2592,"Water motion snapshot stays bounded");
            if (getBlock(x, -1, z) == BlockId::Water) {
                // getBlock only reads existing chunks while this capture owns
                // the world lock; it never creates or loads a neighbour.
                for (int y = -2; y >= -MaxWaterDepth; --y) {
                    if (section.getBlock(x, y, z) != BlockId::Water) break;
                    ++depth;
                }
            }
            for (int y = -1; y <= CHUNK_SIZE; ++y) {
                depth = getBlock(x, y, z) == BlockId::Water
                    ? std::min(depth + 1, MaxWaterDepth) : 0;
                m_waterDepth[index(x, y, z)] = static_cast<std::uint8_t>(depth);
            }
        }
    }

}

std::uint16_t SectionMeshInput::getNaturalTreeRootTag(int x, int y, int z) const noexcept
{
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE ||
        z < 0 || z >= CHUNK_SIZE)
        return 0;
    return m_naturalTreeRootTags[x + CHUNK_SIZE * (z + CHUNK_SIZE * y)];
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

glm::vec2 SectionMeshInput::getWaterSurfaceVelocity(int x,int y,int z) const noexcept
{
    if(x<-1 || x>CHUNK_SIZE || z<-1 || z>CHUNK_SIZE || getBlock(x,y,z)!=BlockId::Water)
        return glm::vec2(0.f);
    if(m_location.y*CHUNK_SIZE+y!=64)return {.12f,.09f};
    return m_waterVelocity[(x+1)+(z+1)*Size];
}

float SectionMeshInput::getWaterDepth(int x, int y, int z) const
{
    if (x < -1 || x > CHUNK_SIZE || y < -1 || y > CHUNK_SIZE ||
        z < -1 || z > CHUNK_SIZE) return 0.f;
    return static_cast<float>(m_waterDepth[index(x, y, z)]);
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
