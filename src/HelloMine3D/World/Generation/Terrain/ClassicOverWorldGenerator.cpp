#include "ClassicOverWorldGenerator.h"

#include <functional>
#include <iostream>

#include "../../../Maths/GeneralMaths.h"
#include "../../../Util/Random.h"
#include "../../Chunk/Chunk.h"

#include "../Structures/TreeGenerator.h"

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

    std::vector<BlockPosition> treePositions;
    std::vector<BlockPosition> plantPositions;
    generateBaseTerrain(maxHeight, treePositions, plantPositions);
    applyCavePass();
    applyOreDecorators();
    applyPlantDecorators(plantPositions);
    applyTreeDecorators(treePositions);
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
    int maxHeight, std::vector<BlockPosition> &treePositions,
    std::vector<BlockPosition> &plantPositions)
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

                        if (canPlaceStructureAt(x, z, 6) &&
                            m_random.intInRange(0,
                                                biome.getTreeFrequency()) ==
                                5) {
                            treePositions.push_back({x, y + 1, z});
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

void ClassicOverWorldGenerator::applyTreeDecorators(
    const std::vector<BlockPosition> &positions)
{
    for (auto &tree : positions) {
        const int x = tree.x;
        const int z = tree.z;

        getBiome(x, z).makeTree(m_random, *m_pChunk, x, tree.y, z);
    }
}

bool ClassicOverWorldGenerator::canPlaceStructureAt(int x, int z,
                                                    int radius) const
{
    return x >= radius && z >= radius && x < CHUNK_SIZE - radius &&
           z < CHUNK_SIZE - radius;
}

const Biome &ClassicOverWorldGenerator::getBiome(int x, int z) const
{
    int biomeValue = m_biomeMap.get(x, z);

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
