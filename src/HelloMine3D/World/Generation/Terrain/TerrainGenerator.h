#ifndef TERRAINGENERATOR_H_INCLUDED
#define TERRAINGENERATOR_H_INCLUDED

class Chunk;

enum class TerrainBiome {
    Desert,
    Grassland,
    LightForest,
    TemperateForest,
    Ocean,
    Mountain
};

inline constexpr int LegacyTerrainGenerationVersion = 1;
inline constexpr int WaystoneTerrainGenerationVersion = 2;
inline constexpr int ExplorationSiteTerrainGenerationVersion = 3;
inline constexpr int MountainTerrainGenerationVersion = 4;
inline constexpr int FoundationTerrainGenerationVersion = 5;
inline constexpr int VoxelOakTerrainGenerationVersion = 6;
inline constexpr int ForestEcologyTerrainGenerationVersion = 7;
inline constexpr int SurfaceCoastTerrainGenerationVersion = 8;
inline constexpr int InlandMeadowTerrainGenerationVersion = 9;
inline constexpr int InlandReliefTerrainGenerationVersion = 10;
inline constexpr int InlandWaterTerrainGenerationVersion = 11;
inline constexpr int VegetationMosaicTerrainGenerationVersion = 12;
inline constexpr int CurrentTerrainGenerationVersion =
    VegetationMosaicTerrainGenerationVersion;

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
