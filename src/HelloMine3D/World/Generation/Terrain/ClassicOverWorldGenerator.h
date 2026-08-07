#ifndef CLASSICOVERWORLDGENERATOR_H_INCLUDED
#define CLASSICOVERWORLDGENERATOR_H_INCLUDED

#include "TerrainGenerator.h"

#include "../../../Util/Array2D.h"
#include "../../../Util/Random.h"

#include "../../../Maths/NoiseGenerator.h"
#include "../../WorldConstants.h"

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
    explicit ClassicOverWorldGenerator(int seed = 0);

    void generateTerrainFor(Chunk &chunk) override;
    int getMinimumSpawnHeight() const noexcept override;
    int getSeed() const noexcept;

  private:
    struct BlockPosition {
        int x = 0;
        int y = 0;
        int z = 0;
    };

    void setUpNoise();

    void generateBaseTerrain(int maxHeight,
                             std::vector<BlockPosition> &treePositions,
                             std::vector<BlockPosition> &plantPositions);
    void applyOreDecorators();
    void placeOreVein(Random<std::minstd_rand> &random, BlockId oreBlock,
                      int startX, int startY, int startZ, int size);
    void applyPlantDecorators(const std::vector<BlockPosition> &positions);
    void applyTreeDecorators(const std::vector<BlockPosition> &positions);
    bool canPlaceStructureAt(int x, int z, int radius) const;

    void getHeightIn(int xMin, int zMin, int xMax, int zMax);
    void getHeightMap();
    void getBiomeMap();

    const Biome &getBiome(int x, int z) const;

    Array2D<int, CHUNK_SIZE> m_heightMap;
    Array2D<int, CHUNK_SIZE + 1> m_biomeMap;

    int m_seed = 0;
    Random<std::minstd_rand> m_random;

    NoiseGenerator m_biomeNoiseGen;

    GrasslandBiome m_grassBiome;
    TemperateForestBiome m_temperateForest;
    DesertBiome m_desertBiome;
    OceanBiome m_oceanBiome;
    LightForest m_lightForest;

    Chunk *m_pChunk = nullptr;
};

#endif // CLASSICOVERWORLDGENERATOR_H_INCLUDED
