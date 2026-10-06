#ifndef TERRAINGENERATOR_H_INCLUDED
#define TERRAINGENERATOR_H_INCLUDED

#include <array>
#include <cstddef>
#include <functional>

#include "../../Block/ChunkBlock.h"

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
inline constexpr int UndergroundPolishTerrainGenerationVersion = 28;
inline constexpr int WorkshopCourtyardTerrainGenerationVersion = 29;
inline constexpr int RockLandmarkTerrainGenerationVersion = 30;
inline constexpr int HighlandWorkshopSightlineTerrainGenerationVersion = 31;
inline constexpr int CurrentTerrainGenerationVersion =
    HighlandWorkshopSightlineTerrainGenerationVersion;

// Derived presentation ownership, never save data or a prediction of an
// edited block. The consumer must compare the expected id AND metadata with
// its copied resident block before assigning this root to a mesh face.
struct NaturalTreeOwnershipBlock {
    std::array<int, 3> position{};
    ChunkBlock block;
    std::array<int, 2> root{};
};
using NaturalTreeOwnershipVisitor =
    std::function<void(const NaturalTreeOwnershipBlock &)>;
inline constexpr int NaturalTreeOwnershipRadius = 6;
inline constexpr std::size_t NaturalTreeMaximumSourceRoots = 25;
inline constexpr std::size_t NaturalTreeMaximumSectionOwnershipBlocks = 4096;

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

    // v24+ uses the exact production tree plan and projection ordering.
    // Unsupported historical/custom generators return false and emit no
    // ownership. A query never loads a Chunk or changes generation/storage.
    virtual bool visitNaturalTreeOwnership(
        int, int, int, const NaturalTreeOwnershipVisitor &) const
    {
        return false;
    }

    virtual ~TerrainGenerator() = default;
};

#endif // TERRAINGENERATOR_H_INCLUDED
