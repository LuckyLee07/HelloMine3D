#include "ClassicOverWorldGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>

#include "../../../Maths/GeneralMaths.h"
#include "../../../Util/Random.h"
#include "../../../Item/ContainerInventory.h"
#include "../../Block/ChestContainer.h"
#include "../../Chunk/Chunk.h"
#include "../../WorldCoordinates.h"

#include "../Structures/TreeGenerator.h"
#include "../Structures/StructureBuilder.h"

namespace {
constexpr int MaximumStructureRadius = 6;
constexpr int MountainBiomeValue = -1000000;
constexpr int MountainRockHeight = WATER_LEVEL + 36;

int normalizeTerrainGenerationVersion(int generationVersion) noexcept
{
    return generationVersion >= LegacyTerrainGenerationVersion &&
            generationVersion <= CurrentTerrainGenerationVersion
        ? generationVersion
        : CurrentTerrainGenerationVersion;
}

std::uint64_t mixStructureValue(std::uint64_t value)
{
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ull;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebull;
    return value ^ (value >> 31);
}

std::uint64_t structureHash(int seed, int worldX, int worldZ)
{
    std::uint64_t value = mixStructureValue(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(seed)));
    value ^= mixStructureValue(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(worldX)) + 0x632be59bd9b4e019ull);
    value ^= mixStructureValue(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(worldZ)) + 0x8cb92baa3f3d8dd7ull);
    return mixStructureValue(value ^ 0xd1b54a32d192ed03ull);
}

double smoothStep(double minimum, double maximum, double value) noexcept
{
    const double amount = std::max(
        0.0, std::min(1.0, (value - minimum) / (maximum - minimum)));
    return amount * amount * (3.0 - 2.0 * amount);
}

double valueNoiseLattice(int seed, int x, int z,
                         std::uint64_t salt) noexcept
{
    std::uint64_t value = mixStructureValue(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(seed)) + salt);
    value ^= mixStructureValue(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(x)) + 0x632be59bd9b4e019ull);
    value ^= mixStructureValue(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(z)) + 0x8cb92baa3f3d8dd7ull);
    constexpr double Unit = 1.0 / 9007199254740991.0;
    return static_cast<double>(mixStructureValue(value) >> 11) *
        Unit * 2.0 - 1.0;
}

double valueNoise2D(int seed, int worldX, int worldZ, double scale,
                    std::uint64_t salt) noexcept
{
    const double x = static_cast<double>(worldX) / scale;
    const double z = static_cast<double>(worldZ) / scale;
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const int x1 = x0 + 1;
    const int z1 = z0 + 1;
    const double tx = smoothStep(0.0, 1.0, x - x0);
    const double tz = smoothStep(0.0, 1.0, z - z0);
    const double lower = valueNoiseLattice(seed, x0, z0, salt) +
        (valueNoiseLattice(seed, x1, z0, salt) -
         valueNoiseLattice(seed, x0, z0, salt)) * tx;
    const double upper = valueNoiseLattice(seed, x0, z1, salt) +
        (valueNoiseLattice(seed, x1, z1, salt) -
         valueNoiseLattice(seed, x0, z1, salt)) * tx;
    return lower + (upper - lower) * tz;
}
} // namespace

ClassicOverWorldGenerator::ClassicOverWorldGenerator(
    int seed, int generationVersion, int explorationRewardVersion)
    : m_seed(seed)
    , m_generationVersion(
          normalizeTerrainGenerationVersion(generationVersion))
    , m_explorationRewardVersion(explorationRewardVersion)
    , m_random(seed)
    , m_biomeNoiseGen(seed * 2)
    , m_caveGenerator(seed,
                      normalizeTerrainGenerationVersion(generationVersion))
    , m_grassBiome(seed)
    , m_temperateForest(seed)
    , m_desertBiome(seed)
    , m_oceanBiome(seed)
    , m_lightForest(seed)
{
    if (!ExplorationRewards::validVersion(m_explorationRewardVersion)) {
        m_explorationRewardVersion = ExplorationRewards::CurrentVersion;
    }
    setUpNoise();
}

void ClassicOverWorldGenerator::setUpNoise()
{
    std::cout << "Seed: " << m_seed << '\n';

    NoiseParameters biomeParmams;
    biomeParmams.octaves = 5;
    biomeParmams.amplitude = 120;
    biomeParmams.smoothness = 1035;
    biomeParmams.heightOffset = 0;
    biomeParmams.roughness = 0.75;

    m_biomeNoiseGen.setParameters(biomeParmams);
}

void ClassicOverWorldGenerator::generateTerrainFor(Chunk &chunk)
{
    m_pChunk = &chunk;

    auto location = chunk.getLocation();
    m_random.setSeed(m_seed ^ (location.x * 73428767) ^
                     (location.y * 91227153));

    getBiomeMap();
    getHeightMap();

    auto maxHeight = m_heightMap.getMaxValue();
    maxHeight = std::max(maxHeight, WATER_LEVEL);

    std::vector<BlockPosition> plantPositions;
    generateBaseTerrain(maxHeight, plantPositions);
    applyCavePass();
    applyOreDecorators();
    applyPlantDecorators(plantPositions);
    applyTreeDecorators();
    applyLandmarkDecorators();
}

void ClassicOverWorldGenerator::applyCavePass()
{
    m_caveGenerator.carve(*m_pChunk, m_heightMap);
    if (m_generationVersion >= MountainTerrainGenerationVersion) {
        m_caveGenerator.carveNaturalEntrances(
            *m_pChunk,
            [this](int worldX, int worldZ) {
                return getSurfaceHeightAtWorld(worldX, worldZ);
            },
            [this](int worldX, int worldZ) {
                return getBiomeAtWorld(worldX, worldZ);
            });
    }
}

int ClassicOverWorldGenerator::getMinimumSpawnHeight() const noexcept
{
    return WATER_LEVEL;
}

int ClassicOverWorldGenerator::getSeed() const noexcept
{
    return m_seed;
}

int ClassicOverWorldGenerator::getExplorationRewardVersion() const noexcept
{
    return m_explorationRewardVersion;
}

int ClassicOverWorldGenerator::getGenerationVersion() const noexcept
{
    return m_generationVersion;
}

TerrainBiome ClassicOverWorldGenerator::getBiomeAtWorld(
    int worldX, int worldZ) const noexcept
{
    if (m_generationVersion >= MountainTerrainGenerationVersion &&
        getMountainStrengthAtWorld(worldX, worldZ) >= 0.48 &&
        getTerrainV4HeightAtWorld(worldX, worldZ) >=
            WATER_LEVEL + 16) {
        return TerrainBiome::Mountain;
    }
    const int chunkX = WorldCoordinates::floorDiv(worldX, CHUNK_SIZE);
    const int chunkZ = WorldCoordinates::floorDiv(worldZ, CHUNK_SIZE);
    const int localX = WorldCoordinates::floorMod(worldX, CHUNK_SIZE);
    const int localZ = WorldCoordinates::floorMod(worldZ, CHUNK_SIZE);
    const int biomeValue = static_cast<int>(m_biomeNoiseGen.getHeight(
        localX, localZ, chunkX + 10, chunkZ + 10));
    return getBiomeKindForValue(biomeValue);
}

int ClassicOverWorldGenerator::getSurfaceHeightAtWorld(
    int worldX, int worldZ) const noexcept
{
    if (m_generationVersion >= MountainTerrainGenerationVersion) {
        return getTerrainV4HeightAtWorld(worldX, worldZ);
    }
    const int chunkX = WorldCoordinates::floorDiv(worldX, CHUNK_SIZE);
    const int chunkZ = WorldCoordinates::floorDiv(worldZ, CHUNK_SIZE);
    const int localX = WorldCoordinates::floorMod(worldX, CHUNK_SIZE);
    const int localZ = WorldCoordinates::floorMod(worldZ, CHUNK_SIZE);
    return getHeightAt(localX, localZ, chunkX, chunkZ);
}

ClassicOverWorldGenerator::LandmarkPlacement
ClassicOverWorldGenerator::getLandmarkForCell(int cellX, int cellZ) const
{
    const StructurePlanSnapshot plan = getStructurePlanForCell(
        StructureType::Waystone, cellX, cellZ);
    return {plan.valid, plan.anchor.x, plan.anchor.y, plan.anchor.z};
}

StructurePlanSnapshot ClassicOverWorldGenerator::getStructurePlanForCell(
    StructureType type, int cellX, int cellZ) const
{
    const DeterministicStructurePlanner planner(
        m_seed, m_generationVersion,
        [this](int worldX, int worldZ) {
            return getSurfaceHeightAtWorld(worldX, worldZ);
        },
        [this](int worldX, int worldZ) {
            return getBiomeAtWorld(worldX, worldZ);
        });
    return planner.planForCell(type, cellX, cellZ);
}

std::vector<StructurePlanSnapshot>
ClassicOverWorldGenerator::getStructurePlansForChunk(
    int chunkX, int chunkZ) const
{
    const DeterministicStructurePlanner planner(
        m_seed, m_generationVersion,
        [this](int worldX, int worldZ) {
            return getSurfaceHeightAtWorld(worldX, worldZ);
        },
        [this](int worldX, int worldZ) {
            return getBiomeAtWorld(worldX, worldZ);
        });
    return planner.plansForChunk(chunkX, chunkZ);
}

void ClassicOverWorldGenerator::getHeightIn(int xMin, int zMin, int xMax,
                                            int zMax)
{

    auto getHeightAt = [&](int x, int z) {
        const Biome &biome = getBiome(x, z);

        return biome.getHeight(x, z, m_pChunk->getLocation().x,
                               m_pChunk->getLocation().y);
    };

    float bottomLeft = static_cast<float>(getHeightAt(xMin, zMin));
    float bottomRight = static_cast<float>(getHeightAt(xMax, zMin));
    float topLeft = static_cast<float>(getHeightAt(xMin, zMax));
    float topRight = static_cast<float>(getHeightAt(xMax, zMax));

    for (int x = xMin; x < xMax; ++x)
        for (int z = zMin; z < zMax; ++z) {
            if (x == CHUNK_SIZE)
                continue;
            if (z == CHUNK_SIZE)
                continue;

            float h = smoothInterpolation(
                bottomLeft, topLeft, bottomRight, topRight,
                static_cast<float>(xMin), static_cast<float>(xMax),
                static_cast<float>(zMin), static_cast<float>(zMax),
                static_cast<float>(x), static_cast<float>(z));

            m_heightMap.get(x, z) = static_cast<int>(h);
        }
}

void ClassicOverWorldGenerator::getHeightMap()
{
    if (m_generationVersion >= MountainTerrainGenerationVersion) {
        const glm::ivec2 location = m_pChunk->getLocation();
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                m_heightMap.get(x, z) = getTerrainV4HeightAtWorld(
                    location.x * CHUNK_SIZE + x,
                    location.y * CHUNK_SIZE + z);
            }
        }
        return;
    }
    constexpr static auto HALF_CHUNK = CHUNK_SIZE / 2;
    constexpr static auto CHUNK = CHUNK_SIZE;

    getHeightIn(0, 0, HALF_CHUNK, HALF_CHUNK);
    getHeightIn(HALF_CHUNK, 0, CHUNK, HALF_CHUNK);
    getHeightIn(0, HALF_CHUNK, HALF_CHUNK, CHUNK);
    getHeightIn(HALF_CHUNK, HALF_CHUNK, CHUNK, CHUNK);
}

void ClassicOverWorldGenerator::getBiomeMap()
{
    auto location = m_pChunk->getLocation();

    for (int x = 0; x < CHUNK_SIZE + 1; x++)
        for (int z = 0; z < CHUNK_SIZE + 1; z++) {
            const int worldX = location.x * CHUNK_SIZE + x;
            const int worldZ = location.y * CHUNK_SIZE + z;
            if (m_generationVersion >= MountainTerrainGenerationVersion &&
                getBiomeAtWorld(worldX, worldZ) ==
                    TerrainBiome::Mountain) {
                m_biomeMap.get(x, z) = MountainBiomeValue;
                continue;
            }
            double h = m_biomeNoiseGen.getHeight(x, z, location.x + 10,
                                                 location.y + 10);
            m_biomeMap.get(x, z) = static_cast<int>(h);
        }
}

void ClassicOverWorldGenerator::generateBaseTerrain(
    int maxHeight, std::vector<BlockPosition> &plantPositions)
{
    for (int y = 0; y < maxHeight + 1; y++)
        for (int x = 0; x < CHUNK_SIZE; x++)
            for (int z = 0; z < CHUNK_SIZE; z++) {
                int height = m_heightMap.get(x, z);
                auto &biome = getBiome(x, z);
                const bool mountainRock =
                    getBiomeKindForValue(m_biomeMap.get(x, z)) ==
                        TerrainBiome::Mountain &&
                    height >= MountainRockHeight;

                if (y > height) {
                    if (y <= WATER_LEVEL) {
                        m_pChunk->setBlock(x, y, z, BlockId::Water);
                    }
                    continue;
                }
                else if (y == height) {
                    if (y >= WATER_LEVEL) {
                        if (y < WATER_LEVEL + 4) {
                            m_pChunk->setBlock(x, y, z,
                                               biome.getBeachBlock(m_random));
                            continue;
                        }

                        // Tree ownership moved to the world-space structure
                        // pass. Keep the old interior roll in the base stream
                        // so unrelated plant and surface choices remain stable.
                        if (x >= MaximumStructureRadius &&
                            z >= MaximumStructureRadius &&
                            x < CHUNK_SIZE - MaximumStructureRadius &&
                            z < CHUNK_SIZE - MaximumStructureRadius) {
                            (void)m_random.intInRange(
                                0, biome.getTreeFrequency());
                        }
                        if (!mountainRock &&
                            m_random.intInRange(
                                0, biome.getPlantFrequency()) == 5) {
                            plantPositions.push_back({x, y + 1, z});
                        }
                        m_pChunk->setBlock(
                            x, y, z,
                            mountainRock
                                ? ChunkBlock(BlockId::Stone)
                                : getBiome(x, z).getTopBlock(m_random));
                    }
                    else {
                        m_pChunk->setBlock(x, y, z,
                                           biome.getUnderWaterBlock(m_random));
                    }
                }
                else if (y > height - 3 && !mountainRock) {
                    m_pChunk->setBlock(x, y, z, BlockId::Dirt);
                }
                else {
                    m_pChunk->setBlock(x, y, z, BlockId::Stone);
                }
            }
}

void ClassicOverWorldGenerator::applyOreDecorators()
{
    auto location = m_pChunk->getLocation();
    Random<std::minstd_rand> oreRandom(
        m_seed ^ (location.x * 13371337) ^ (location.y * 265443576) ^
        0x5a5a);

    auto runOrePass = [&](BlockId oreBlock, int attempts, int minY, int maxY,
                          int minSize, int maxSize) {
        for (int i = 0; i < attempts; ++i) {
            const int x = oreRandom.intInRange(0, CHUNK_SIZE - 1);
            const int y = oreRandom.intInRange(minY, maxY);
            const int z = oreRandom.intInRange(0, CHUNK_SIZE - 1);
            const int size = oreRandom.intInRange(minSize, maxSize);
            placeOreVein(oreRandom, oreBlock, x, y, z, size);
        }
    };

    runOrePass(BlockId::CoalOre, 14, 8, 96, 3, 8);
    runOrePass(BlockId::IronOre, 8, 6, 64, 2, 6);
}

void ClassicOverWorldGenerator::placeOreVein(Random<std::minstd_rand> &random,
                                             BlockId oreBlock, int startX,
                                             int startY, int startZ, int size)
{
    int x = startX;
    int y = startY;
    int z = startZ;
    for (int i = 0; i < size; ++i) {
        if (x >= 0 && x < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE && y > 0) {
            if (static_cast<BlockId>(m_pChunk->getBlock(x, y, z).id) ==
                BlockId::Stone) {
                m_pChunk->setBlock(x, y, z, oreBlock);
            }
        }

        x += random.intInRange(-1, 1);
        y += random.intInRange(-1, 1);
        z += random.intInRange(-1, 1);
    }
}

void ClassicOverWorldGenerator::applyPlantDecorators(
    const std::vector<BlockPosition> &positions)
{
    for (auto &plant : positions) {
        const int x = plant.x;
        const int z = plant.z;

        auto block = getBiome(x, z).getPlant(m_random);
        m_pChunk->setBlock(x, plant.y, z, block);
    }
}

void ClassicOverWorldGenerator::applyTreeDecorators()
{
    const glm::ivec2 target = m_pChunk->getLocation();
    const int minimumX = target.x * CHUNK_SIZE - MaximumStructureRadius;
    const int maximumX = (target.x + 1) * CHUNK_SIZE - 1 +
                         MaximumStructureRadius;
    const int minimumZ = target.y * CHUNK_SIZE - MaximumStructureRadius;
    const int maximumZ = (target.y + 1) * CHUNK_SIZE - 1 +
                         MaximumStructureRadius;

    for (int worldX = minimumX; worldX <= maximumX; ++worldX) {
        const int sourceChunkX =
            WorldCoordinates::floorDiv(worldX, CHUNK_SIZE);
        const int localX =
            WorldCoordinates::floorMod(worldX, CHUNK_SIZE);
        for (int worldZ = minimumZ; worldZ <= maximumZ; ++worldZ) {
            const int sourceChunkZ =
                WorldCoordinates::floorDiv(worldZ, CHUNK_SIZE);
            const int localZ =
                WorldCoordinates::floorMod(worldZ, CHUNK_SIZE);
            const Biome &biome = getBiomeAt(
                localX, localZ, sourceChunkX, sourceChunkZ);
            const int height = getHeightAt(
                localX, localZ, sourceChunkX, sourceChunkZ);
            if (height < WATER_LEVEL + 4) {
                continue;
            }
            if (m_generationVersion >= MountainTerrainGenerationVersion &&
                getBiomeAtWorld(worldX, worldZ) ==
                    TerrainBiome::Mountain) {
                continue;
            }

            const int frequency = biome.getTreeFrequency();
            const std::uint64_t hash =
                structureHash(m_seed, worldX, worldZ);
            if (frequency < 5 ||
                hash % static_cast<std::uint64_t>(frequency + 1) != 5) {
                continue;
            }

            Random<std::minstd_rand> structureRandom(
                static_cast<int>((hash ^ (hash >> 32)) & 0x7fffffffull));
            biome.makeTree(structureRandom, *m_pChunk, worldX,
                           height + 1, worldZ);
        }
    }
}

void ClassicOverWorldGenerator::applyLandmarkDecorators()
{
    const glm::ivec2 target = m_pChunk->getLocation();
    for (const StructurePlanSnapshot &plan :
         getStructurePlansForChunk(target.x, target.y)) {
        projectStructurePlan(plan);
    }
}

void ClassicOverWorldGenerator::projectStructurePlan(
    const StructurePlanSnapshot &plan)
{
    if (!plan.valid) {
        return;
    }
    const int x = plan.anchor.x;
    const int y = plan.anchor.y;
    const int z = plan.anchor.z;
    StructureBuilder builder;

    if (plan.key.type == StructureType::Waystone) {
        for (int clearY = y + 1;
             clearY <= y + DeterministicStructurePlanner::WaystoneHeight;
             ++clearY) {
            builder.fill(clearY,
                         x - LandmarkRadius, x + LandmarkRadius + 1,
                         z - LandmarkRadius, z + LandmarkRadius + 1,
                         BlockId::Air);
        }
        builder.fill(y + 1,
                     x - LandmarkRadius, x + LandmarkRadius + 1,
                     z - LandmarkRadius, z + LandmarkRadius + 1,
                     BlockId::Stone);
        builder.makeColumn(x - LandmarkRadius, z - LandmarkRadius,
                           y + 2, 3, BlockId::Stone);
        builder.makeColumn(x + LandmarkRadius, z - LandmarkRadius,
                           y + 2, 3, BlockId::Stone);
        builder.makeColumn(x - LandmarkRadius, z + LandmarkRadius,
                           y + 2, 3, BlockId::Stone);
        builder.makeColumn(x + LandmarkRadius, z + LandmarkRadius,
                           y + 2, 3, BlockId::Stone);
        builder.addBlock(x, y + 2, z, BlockId::IronOre);
        builder.addBlock(x, y + 3, z, BlockId::WaystoneCore);
        builder.addBlock(x, y + 4, z, BlockId::Glass);
        builder.addBlock(x, y + 5, z, BlockId::Stone);
        builder.addBlock(x - 1, y + 2, z, BlockId::CoalOre);
        builder.addBlock(x + 1, y + 2, z, BlockId::CoalOre);
        builder.addBlock(x, y + 2, z - 1, BlockId::CoalOre);
        builder.addBlock(x, y + 2, z + 1, BlockId::CoalOre);
    }
    else if (plan.key.type == StructureType::Ruin) {
        const int radius = DeterministicStructurePlanner::RuinRadius;
        for (int clearY = y + 2;
             clearY <= y + DeterministicStructurePlanner::RuinHeight;
             ++clearY) {
            builder.fill(clearY, x - radius, x + radius + 1,
                         z - radius, z + radius + 1, BlockId::Air);
        }
        for (int supportY = y - 2; supportY <= y; ++supportY) {
            builder.fill(supportY, x - radius, x + radius + 1,
                         z - radius, z + radius + 1, BlockId::Stone);
        }
        builder.fill(y + 1, x - radius, x + radius + 1,
                     z - radius, z + radius + 1, BlockId::Stone);
        builder.makeRowX(x - radius, x + radius, y + 2,
                         z - radius, BlockId::Stone);
        builder.makeRowX(x - radius, x + radius, y + 2,
                         z + radius, BlockId::Stone);
        builder.makeRowZ(z - radius + 1, z + radius - 1,
                         x - radius, y + 2, BlockId::Stone);
        builder.makeRowZ(z - radius + 1, z + radius - 1,
                         x + radius, y + 2, BlockId::Stone);
        builder.makeColumn(x - radius, z - radius,
                           y + 3, 3, BlockId::Stone);
        builder.makeColumn(x + radius, z - radius,
                           y + 3, 3, BlockId::Stone);
        builder.makeColumn(x - radius, z + radius,
                           y + 3, 3, BlockId::Stone);
        builder.makeColumn(x + radius, z + radius,
                           y + 3, 3, BlockId::Stone);
        builder.makeColumn(x - radius, z, y + 3, 2, BlockId::Stone);
        builder.makeColumn(x + radius, z, y + 3, 2, BlockId::Stone);
        builder.makeColumn(x, z - radius, y + 3, 2, BlockId::Stone);
        builder.makeColumn(x, z + radius, y + 3, 2, BlockId::Stone);
        builder.addBlock(x, y + 2, z, BlockId::Chest);
        builder.addBlock(x - 2, y + 2, z - 2, BlockId::IronOre);
        builder.addBlock(x + 2, y + 2, z - 2, BlockId::IronOre);
        builder.addBlock(x - 2, y + 2, z + 2, BlockId::IronOre);
        builder.addBlock(x + 2, y + 2, z + 2, BlockId::IronOre);
        builder.addBlock(x - 1, y + 5, z, BlockId::Glass);
        builder.addBlock(x + 1, y + 5, z, BlockId::Glass);
        builder.addBlock(x, y + 5, z - 1, BlockId::Glass);
        builder.addBlock(x, y + 5, z + 1, BlockId::Glass);
    }
    else if (plan.key.type == StructureType::RaiderCamp) {
        const int radiusX = DeterministicStructurePlanner::CampRadiusX;
        const int radiusZ = DeterministicStructurePlanner::CampRadiusZ;
        for (int clearY = y + 2;
             clearY <= y + DeterministicStructurePlanner::CampHeight;
             ++clearY) {
            builder.fill(clearY, x - radiusX, x + radiusX + 1,
                         z - radiusZ, z + radiusZ + 1, BlockId::Air);
        }
        for (int supportY = y - 2; supportY <= y; ++supportY) {
            builder.fill(supportY, x - radiusX, x + radiusX + 1,
                         z - radiusZ, z + radiusZ + 1, BlockId::Dirt);
        }
        builder.fill(y + 1, x - radiusX, x + radiusX + 1,
                     z - radiusZ, z + radiusZ + 1, BlockId::Dirt);
        builder.makeRowX(x - radiusX, x + radiusX, y + 2,
                         z - radiusZ, BlockId::OakBark);
        builder.makeRowX(x - radiusX, x + radiusX, y + 2,
                         z + radiusZ, BlockId::OakBark);
        builder.makeRowZ(z - radiusZ + 1, z + radiusZ - 1,
                         x - radiusX, y + 2, BlockId::OakBark);
        builder.makeRowZ(z - radiusZ + 1, z + radiusZ - 1,
                         x + radiusX, y + 2, BlockId::OakBark);
        builder.addBlock(x, y + 2, z - radiusZ, BlockId::Air);
        builder.addBlock(x, y + 3, z - radiusZ, BlockId::Air);
        builder.makeColumn(x - radiusX, z - radiusZ,
                           y + 2, 3, BlockId::OakBark);
        builder.makeColumn(x + radiusX, z - radiusZ,
                           y + 2, 3, BlockId::OakBark);
        builder.makeColumn(x - radiusX, z + radiusZ,
                           y + 2, 3, BlockId::OakBark);
        builder.makeColumn(x + radiusX, z + radiusZ,
                           y + 2, 3, BlockId::OakBark);
        builder.makeColumn(x - 2, z - 2, y + 2, 2, BlockId::OakBark);
        builder.makeColumn(x + 2, z - 2, y + 2, 2, BlockId::OakBark);
        builder.makeColumn(x - 2, z + 2, y + 2, 2, BlockId::OakBark);
        builder.makeColumn(x + 2, z + 2, y + 2, 2, BlockId::OakBark);
        builder.fill(y + 4, x - 3, x + 4, z - 3, z + 4,
                     BlockId::OakLeaf);
        builder.addBlock(x, y + 2, z + 1, BlockId::Chest);
        builder.addBlock(x, y + 2, z - 2, BlockId::CoalOre);
        builder.addBlock(x - 1, y + 2, z - 2, BlockId::Stone);
        builder.addBlock(x + 1, y + 2, z - 2, BlockId::Stone);
        builder.addBlock(x, y + 2, z - 3, BlockId::Stone);
        builder.addBlock(x, y + 2, z - 1, BlockId::Stone);
    }
    else {
        return;
    }
    builder.build(*m_pChunk);

    const StructureLootSnapshot loot = structureLootForPlan(
        plan, m_explorationRewardVersion);
    if (!loot.valid) {
        return;
    }
    const glm::ivec2 target = m_pChunk->getLocation();
    const int localX = loot.chestPosition.x - target.x * CHUNK_SIZE;
    const int localZ = loot.chestPosition.z - target.y * CHUNK_SIZE;
    if (localX < 0 || localX >= CHUNK_SIZE ||
        localZ < 0 || localZ >= CHUNK_SIZE) {
        return;
    }

    ContainerInventory inventory(ChestContainer::SlotCount);
    for (const StructureLootEntry &entry : loot.entries) {
        const Material &material = Material::toMaterial(entry.materialId);
        if (inventory.addItem(material, entry.amount) != entry.amount) {
            std::cerr << "Unable to initialize structure loot for "
                      << structureTypeName(plan.key.type) << " at "
                      << loot.chestPosition.x << ','
                      << loot.chestPosition.y << ','
                      << loot.chestPosition.z << '\n';
            return;
        }
    }
    const BlockEntityRecord record{
        {localX, loot.chestPosition.y, localZ},
        ChestContainer::BlockEntityType, inventory.serialize()};
    if (!m_pChunk->createBlockEntity(record)) {
        std::cerr << "Unable to initialize structure chest for "
                  << structureTypeName(plan.key.type) << " at "
                  << loot.chestPosition.x << ','
                  << loot.chestPosition.y << ','
                  << loot.chestPosition.z << '\n';
    }
}

int ClassicOverWorldGenerator::getHeightAt(int x, int z, int chunkX,
                                           int chunkZ) const
{
    if (m_generationVersion >= MountainTerrainGenerationVersion) {
        return getTerrainV4HeightAtWorld(
            chunkX * CHUNK_SIZE + x, chunkZ * CHUNK_SIZE + z);
    }
    return getLegacyHeightAt(x, z, chunkX, chunkZ);
}

int ClassicOverWorldGenerator::getLegacyHeightAt(
    int x, int z, int chunkX, int chunkZ) const
{
    const int xMin = x < CHUNK_SIZE / 2 ? 0 : CHUNK_SIZE / 2;
    const int zMin = z < CHUNK_SIZE / 2 ? 0 : CHUNK_SIZE / 2;
    const int xMax = xMin + CHUNK_SIZE / 2;
    const int zMax = zMin + CHUNK_SIZE / 2;
    const auto cornerHeight = [&](int sampleX, int sampleZ) {
        return getBiomeAt(sampleX, sampleZ, chunkX, chunkZ)
            .getHeight(sampleX, sampleZ, chunkX, chunkZ);
    };
    return static_cast<int>(smoothInterpolation(
        static_cast<float>(cornerHeight(xMin, zMin)),
        static_cast<float>(cornerHeight(xMin, zMax)),
        static_cast<float>(cornerHeight(xMax, zMin)),
        static_cast<float>(cornerHeight(xMax, zMax)),
        static_cast<float>(xMin), static_cast<float>(xMax),
        static_cast<float>(zMin), static_cast<float>(zMax),
        static_cast<float>(x), static_cast<float>(z)));
}

double ClassicOverWorldGenerator::getMountainStrengthAtWorld(
    int worldX, int worldZ) const noexcept
{
    const double domain = valueNoise2D(
        m_seed, worldX, worldZ, 480.0, 0x243f6a8885a308d3ull);
    const double ridge = 1.0 - std::abs(valueNoise2D(
        m_seed, worldX, worldZ, 150.0, 0x13198a2e03707344ull));
    return smoothStep(0.12, 0.62, domain * 0.76 + ridge * 0.24);
}

int ClassicOverWorldGenerator::getTerrainV4HeightAtWorld(
    int worldX, int worldZ) const noexcept
{
    const int chunkX = WorldCoordinates::floorDiv(worldX, CHUNK_SIZE);
    const int chunkZ = WorldCoordinates::floorDiv(worldZ, CHUNK_SIZE);
    const int localX = WorldCoordinates::floorMod(worldX, CHUNK_SIZE);
    const int localZ = WorldCoordinates::floorMod(worldZ, CHUNK_SIZE);
    const int legacyHeight = getLegacyHeightAt(
        localX, localZ, chunkX, chunkZ);
    const double strength = getMountainStrengthAtWorld(worldX, worldZ);
    const double ridge = 1.0 - std::abs(valueNoise2D(
        m_seed, worldX, worldZ, 92.0, 0xa4093822299f31d0ull));
    const double detail = valueNoise2D(
        m_seed, worldX, worldZ, 38.0, 0x082efa98ec4e6c89ull);
    const double foundation = std::max(
        0, WATER_LEVEL + 5 - legacyHeight) * strength;
    const double relief = strength *
        (18.0 + ridge * ridge * 58.0 + detail * 5.0);
    return std::max(1, std::min(176, static_cast<int>(std::lround(
        static_cast<double>(legacyHeight) + foundation + relief))));
}

const Biome &ClassicOverWorldGenerator::getBiome(int x, int z) const
{
    return getBiomeForValue(m_biomeMap.get(x, z));
}

const Biome &ClassicOverWorldGenerator::getBiomeAt(
    int x, int z, int chunkX, int chunkZ) const
{
    const int biomeValue = static_cast<int>(m_biomeNoiseGen.getHeight(
        x, z, chunkX + 10, chunkZ + 10));
    return getBiomeForValue(biomeValue);
}

const Biome &
ClassicOverWorldGenerator::getBiomeForValue(int biomeValue) const
{
    switch (getBiomeKindForValue(biomeValue)) {
        case TerrainBiome::Ocean:
            return m_oceanBiome;
        case TerrainBiome::Grassland:
            return m_grassBiome;
        case TerrainBiome::LightForest:
            return m_lightForest;
        case TerrainBiome::TemperateForest:
            return m_temperateForest;
        case TerrainBiome::Desert:
            return m_desertBiome;
        case TerrainBiome::Mountain:
            return m_lightForest;
    }
    return m_grassBiome;
}

TerrainBiome ClassicOverWorldGenerator::getBiomeKindForValue(
    int biomeValue) noexcept
{
    if (biomeValue == MountainBiomeValue) {
        return TerrainBiome::Mountain;
    }
    if (biomeValue > 160) {
        return TerrainBiome::Ocean;
    }
    if (biomeValue > 150) {
        return TerrainBiome::Grassland;
    }
    if (biomeValue > 130) {
        return TerrainBiome::LightForest;
    }
    if (biomeValue > 120) {
        return TerrainBiome::TemperateForest;
    }
    if (biomeValue > 110) {
        return TerrainBiome::LightForest;
    }
    if (biomeValue > 100) {
        return TerrainBiome::Grassland;
    }
    return TerrainBiome::Desert;
}
