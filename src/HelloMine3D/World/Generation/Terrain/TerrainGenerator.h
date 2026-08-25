#ifndef TERRAINGENERATOR_H_INCLUDED
#define TERRAINGENERATOR_H_INCLUDED

class Chunk;

enum class TerrainBiome {
    Desert,
    Grassland,
    LightForest,
    TemperateForest,
    Ocean
};

inline constexpr int LegacyTerrainGenerationVersion = 1;
inline constexpr int WaystoneTerrainGenerationVersion = 2;
inline constexpr int CurrentTerrainGenerationVersion = 3;

class TerrainGenerator {
  public:
    virtual void generateTerrainFor(Chunk &chunk) = 0;
    virtual int getMinimumSpawnHeight() const noexcept = 0;
    virtual int getGenerationVersion() const noexcept = 0;
    virtual TerrainBiome getBiomeAtWorld(int worldX,
                                         int worldZ) const noexcept = 0;
    virtual int getSurfaceHeightAtWorld(int worldX,
                                        int worldZ) const noexcept = 0;

    virtual ~TerrainGenerator() = default;
};

#endif // TERRAINGENERATOR_H_INCLUDED
