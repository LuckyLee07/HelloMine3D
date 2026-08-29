#ifndef CLASSICOVERWORLDGENERATOR_H_INCLUDED
#define CLASSICOVERWORLDGENERATOR_H_INCLUDED

#include "TerrainGenerator.h"

#include "../../../Util/Array2D.h"
#include "../../../Util/Random.h"

#include "../../../Maths/NoiseGenerator.h"
#include "../../WorldConstants.h"
#include "CaveGenerator.h"
#include "../Structures/StructurePlanning.h"
#include "../../../Gameplay/ExplorationRewards.h"

#include "../Biome/DesertBiome.h"
#include "../Biome/GrasslandBiome.h"
#include "../Biome/LightForest.h"
#include "../Biome/OceanBiome.h"
#include "../Biome/TemperateForestBiome.h"

#include <vector>

class Chunk;

/// @brief Generates chunks based on perlin noise and recognizable MC parameters.
class ClassicOverWorldGenerator : public TerrainGenerator {
  public:
    struct LandmarkPlacement {
        bool valid = false;
        int x = 0;
        int y = 0;
        int z = 0;
    };

    static constexpr int LandmarkCellChunks =
        DeterministicStructurePlanner::WaystoneCellChunks;
    static constexpr int LandmarkRadius =
        DeterministicStructurePlanner::WaystoneRadius;

    explicit ClassicOverWorldGenerator(
        int seed = 0,
        int generationVersion = CurrentTerrainGenerationVersion,
        int explorationRewardVersion =
            ExplorationRewards::CurrentVersion);

    void generateTerrainFor(Chunk &chunk) override;
    int getMinimumSpawnHeight() const noexcept override;
    int getGenerationVersion() const noexcept override;
    TerrainBiome getBiomeAtWorld(int worldX,
                                 int worldZ) const noexcept override;
    int getSurfaceHeightAtWorld(int worldX,
                                int worldZ) const noexcept override;
    int getSeed() const noexcept;
    int getExplorationRewardVersion() const noexcept;
    LandmarkPlacement getLandmarkForCell(int cellX, int cellZ) const;
    StructurePlanSnapshot getStructurePlanForCell(
        StructureType type, int cellX, int cellZ) const;
    std::vector<StructurePlanSnapshot> getStructurePlansForChunk(
        int chunkX, int chunkZ) const;

  private:
    struct BlockPosition {
        int x = 0;
        int y = 0;
        int z = 0;
    };

    void setUpNoise();

    void generateBaseTerrain(int maxHeight,
                             std::vector<BlockPosition> &plantPositions);
    void applyCavePass();
    void applyOreDecorators();
    void placeOreVein(Random<std::minstd_rand> &random, BlockId oreBlock,
                      int startX, int startY, int startZ, int size);
    void applyPlantDecorators(const std::vector<BlockPosition> &positions);
    void applyTreeDecorators();
    void applyLandmarkDecorators();
    void projectStructurePlan(const StructurePlanSnapshot &plan);

    void getHeightIn(int xMin, int zMin, int xMax, int zMax);
    int getHeightAt(int x, int z, int chunkX, int chunkZ) const;
    int getLegacyHeightAt(int x, int z, int chunkX, int chunkZ) const;
    int getTerrainV4HeightAtWorld(int worldX, int worldZ) const noexcept;
    double getMountainStrengthAtWorld(int worldX,
                                      int worldZ) const noexcept;
    void getHeightMap();
    void getBiomeMap();

    const Biome &getBiome(int x, int z) const;
    const Biome &getBiomeAt(int x, int z, int chunkX,
                            int chunkZ) const;
    const Biome &getBiomeForValue(int biomeValue) const;
    static TerrainBiome getBiomeKindForValue(int biomeValue) noexcept;

    Array2D<int, CHUNK_SIZE> m_heightMap;
    Array2D<int, CHUNK_SIZE + 1> m_biomeMap;

    int m_seed = 0;
    int m_generationVersion = CurrentTerrainGenerationVersion;
    int m_explorationRewardVersion =
        ExplorationRewards::CurrentVersion;
    Random<std::minstd_rand> m_random;

    NoiseGenerator m_biomeNoiseGen;
    CaveGenerator m_caveGenerator;

    GrasslandBiome m_grassBiome;
    TemperateForestBiome m_temperateForest;
    DesertBiome m_desertBiome;
    OceanBiome m_oceanBiome;
    LightForest m_lightForest;

    Chunk *m_pChunk = nullptr;
};

#endif // CLASSICOVERWORLDGENERATOR_H_INCLUDED
