#ifndef TERRAINGENERATOR_H_INCLUDED
#define TERRAINGENERATOR_H_INCLUDED

#include <array>

class Chunk;

enum class TerrainBiome {
    Desert,
    Grassland,
    LightForest,
    TemperateForest,
    Ocean,
    Mountain,
    Wetland,
    RockPlateau,
    River,
    Lake
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
inline constexpr int LandformDiversityTerrainGenerationVersion = 13;
inline constexpr int LandmarkArchitectureTerrainGenerationVersion = 14;
inline constexpr int SurfaceTransitionTerrainGenerationVersion = 15;
inline constexpr int AdventureRegionTerrainGenerationVersion = 16;
inline constexpr int AdventureWaterTerrainGenerationVersion = 17;
inline constexpr int AdventureEcologyTerrainGenerationVersion = 18;
inline constexpr int AdventureExplorationTerrainGenerationVersion = 19;
inline constexpr int LandmarkExpeditionTerrainGenerationVersion = 20;
inline constexpr int LandmarkApproachTerrainGenerationVersion = 21;
inline constexpr int LandmarkWorkshopTerrainGenerationVersion = 22;
inline constexpr int AdventureUndergroundTerrainGenerationVersion = 23;
inline constexpr int VegetationPolishTerrainGenerationVersion = 24;
inline constexpr int LocalReliefTerrainGenerationVersion = 25;
inline constexpr int WaterbankPolishTerrainGenerationVersion = 26;
inline constexpr int LandmarkPolishTerrainGenerationVersion = 27;
inline constexpr int CurrentTerrainGenerationVersion =
    LandmarkPolishTerrainGenerationVersion;

class TerrainGenerator {
  public:
    virtual void generateTerrainFor(Chunk &chunk) = 0;
    virtual int getMinimumSpawnHeight() const noexcept = 0;
    virtual int getGenerationVersion() const noexcept = 0;
    virtual TerrainBiome getBiomeAtWorld(int worldX,
                                         int worldZ) const noexcept = 0;
    virtual int getSurfaceHeightAtWorld(int worldX,
                                        int worldZ) const noexcept = 0;

    // Pure presentation hint, never water physics or a prediction of saved
    // blocks. The mesh only consumes it where resident surface water exists.
    virtual std::array<float,2> getWaterSurfaceVelocityAtWorld(
        int worldX, int worldZ) const noexcept
    {
        switch (getBiomeAtWorld(worldX, worldZ)) {
            case TerrainBiome::Ocean: return {.8f,.6f};
            case TerrainBiome::Lake: return {.16f,.12f};
            case TerrainBiome::Wetland: return {.04f,.03f};
            default: return {.12f,.09f};
        }
    }

    virtual ~TerrainGenerator() = default;
};

#endif // TERRAINGENERATOR_H_INCLUDED
