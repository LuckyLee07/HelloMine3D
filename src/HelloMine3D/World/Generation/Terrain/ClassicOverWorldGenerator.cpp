#include "ClassicOverWorldGenerator.h"

#include <cstdint>
#include <functional>
#include <iostream>

#include "../../../Maths/GeneralMaths.h"
#include "../../../Util/Random.h"
#include "../../Chunk/Chunk.h"
#include "../../WorldCoordinates.h"

#include "../Structures/TreeGenerator.h"

namespace {
constexpr int MaximumStructureRadius = 6;

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
} // namespace

ClassicOverWorldGenerator::ClassicOverWorldGenerator(int seed)
    : m_seed(seed)
    , m_random(seed)
    , m_biomeNoiseGen(seed * 2)
    , m_caveGenerator(seed)
    , m_grassBiome(seed)
    , m_temperateForest(seed)
    , m_desertBiome(seed)
    , m_oceanBiome(seed)
    , m_lightForest(seed)
{
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
}

void ClassicOverWorldGenerator::applyCavePass()
{
    m_caveGenerator.carve(*m_pChunk, m_heightMap);
}

int ClassicOverWorldGenerator::getMinimumSpawnHeight() const noexcept
{
    return WATER_LEVEL;
}

int ClassicOverWorldGenerator::getSeed() const noexcept
{
    return m_seed;
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
                        if (m_random.intInRange(0, biome.getPlantFrequency()) ==
                            5) {
                            plantPositions.push_back({x, y + 1, z});
                        }
                        m_pChunk->setBlock(
                            x, y, z, getBiome(x, z).getTopBlock(m_random));
                    }
                    else {
                        m_pChunk->setBlock(x, y, z,
                                           biome.getUnderWaterBlock(m_random));
                    }
                }
                else if (y > height - 3) {
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

int ClassicOverWorldGenerator::getHeightAt(int x, int z, int chunkX,
                                           int chunkZ) const
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
    if (biomeValue > 160) {
        return m_oceanBiome;
    }
    else if (biomeValue > 150) {
        return m_grassBiome;
    }
    else if (biomeValue > 130) {
        return m_lightForest;
    }
    else if (biomeValue > 120) {
        return m_temperateForest;
    }
    else if (biomeValue > 110) {
        return m_lightForest;
    }
    else if (biomeValue > 100) {
        return m_grassBiome;
    }
    else {
        return m_desertBiome;
    }
}
