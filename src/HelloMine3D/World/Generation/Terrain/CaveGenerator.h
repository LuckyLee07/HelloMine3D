#ifndef CAVEGENERATOR_H_INCLUDED
#define CAVEGENERATOR_H_INCLUDED

#include "../../../Util/Array2D.h"
#include "../../WorldConstants.h"
#include "TerrainGenerator.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

class Chunk;
class ClassicOverWorldGenerator;

/// Deterministic world-space cave pass shared by every generated chunk.
class CaveGenerator {
  public:
    using SurfaceHeightSampler = std::function<int(int, int)>;
    using BiomeSampler = std::function<TerrainBiome(int, int)>;
    // A production-only fast reject. Returning false must prove that the
    // exact biome sampler cannot return Mountain at the same coordinate.
    using EntranceCandidatePrefilter = std::function<bool(int, int)>;

    struct NaturalEntrance {
        bool valid = false;
        int cellX = 0;
        int cellZ = 0;
        int anchorX = 0;
        int anchorY = 0;
        int anchorZ = 0;
        int directionX = 0;
        int directionZ = 0;
        int endY = 0;
    };

    enum class AdventureUndergroundLayout {
        MinerCache,
        MossCellar
    };

    struct AdventureUndergroundPlan {
        bool valid = false;
        NaturalEntrance entrance;
        AdventureUndergroundLayout layout =
            AdventureUndergroundLayout::MinerCache;
        int chamberX = 0;
        int chamberAirY = 0;
        int chamberZ = 0;
        int riftDirectionX = 0;
        int riftDirectionZ = 0;
        int riftEndAirY = 0;
        int destinationX = 0;
        int destinationZ = 0;
        int poolDirectionX = 0;
        int poolDirectionZ = 0;
        std::uint64_t stableKey = 0;
    };

    static constexpr int EntranceCellBlocks = CHUNK_SIZE * 6;
    static constexpr int EntranceCandidateCount = 12;
    static constexpr int EntranceTunnelLength = 24;
    static constexpr int VegetationPlanPadding = 6;
    static constexpr int AdventureUndergroundReach = 64;
    static constexpr int AdventureChamberAlongRadius = 9;
    static constexpr int AdventureChamberPerpendicularRadius = 7;
    static constexpr int AdventureChamberVerticalRadius = 5;
    static constexpr int AdventureRiftLength = 24;
    static constexpr int AdventureRiftHalfWidth = 1;
    static constexpr int AdventureRiftHeight = 9;
    static constexpr int AdventureDestinationRadius = 5;
    static constexpr int AdventurePoolRadius = 3;
    static constexpr int AdventureMaximumWritesPerPlan = 12000;
    static constexpr int AdventureMaximumAirWritesPerPlan = 9000;
    static constexpr int AdventureMaximumWaterPerPlan = 384;
    static constexpr int AdventureMaximumStructureBlocksPerPlan = 1500;
    static constexpr std::size_t AdventurePlanCacheCapacity = 256;

    explicit CaveGenerator(
        int seed,
        int generationVersion = LegacyTerrainGenerationVersion);

    std::size_t carve(
        Chunk &chunk,
        const Array2D<int, CHUNK_SIZE> &surfaceHeights) const;
    std::size_t carveNaturalEntrances(
        Chunk &chunk, const SurfaceHeightSampler &surfaceHeight,
        const BiomeSampler &biome,
        std::vector<NaturalEntrance> *vegetationPlans = nullptr) const;
    NaturalEntrance getNaturalEntranceForCell(
        int cellX, int cellZ,
        const SurfaceHeightSampler &surfaceHeight,
        const BiomeSampler &biome) const;
    AdventureUndergroundPlan getAdventureUndergroundPlanForCell(
        int cellX, int cellZ,
        const SurfaceHeightSampler &surfaceHeight,
        const BiomeSampler &biome) const;
    std::size_t projectAdventureUnderground(
        Chunk &chunk, const SurfaceHeightSampler &surfaceHeight,
        const BiomeSampler &biome) const;

  private:
    friend class ClassicOverWorldGenerator;

    // Only the production generator may opt into the cross-chunk plan cache:
    // its samplers and exact-equivalent prefilter are immutable for this
    // CaveGenerator instance. The public callback API remains uncached so
    // callers cannot accidentally reuse a plan from different callbacks.
    std::size_t projectAdventureUnderground(
        Chunk &chunk, const SurfaceHeightSampler &surfaceHeight,
        const BiomeSampler &biome,
        const EntranceCandidatePrefilter &candidatePrefilter) const;
    std::size_t projectAdventureUndergroundImpl(
        Chunk &chunk, const SurfaceHeightSampler &surfaceHeight,
        const BiomeSampler &biome,
        const EntranceCandidatePrefilter &candidatePrefilter,
        bool usePlanCache) const;
    NaturalEntrance getNaturalEntranceForCellWithPrefilter(
        int cellX, int cellZ,
        const SurfaceHeightSampler &surfaceHeight,
        const BiomeSampler &biome,
        const EntranceCandidatePrefilter &candidatePrefilter) const;
    AdventureUndergroundPlan
    getAdventureUndergroundPlanForCellWithPrefilter(
        int cellX, int cellZ,
        const SurfaceHeightSampler &surfaceHeight,
        const BiomeSampler &biome,
        const EntranceCandidatePrefilter &candidatePrefilter,
        bool usePlanningCache) const;
    AdventureUndergroundPlan getRawAdventureUndergroundPlanForCell(
        int cellX, int cellZ,
        const NaturalEntrance &entrance,
        const SurfaceHeightSampler &surfaceHeight) const;
    AdventureUndergroundPlan getCachedAdventureUndergroundPlanForCell(
        int cellX, int cellZ,
        const SurfaceHeightSampler &surfaceHeight,
        const BiomeSampler &biome,
        const EntranceCandidatePrefilter &candidatePrefilter) const;
    NaturalEntrance getCachedNaturalEntranceForCell(
        int cellX, int cellZ,
        const SurfaceHeightSampler &surfaceHeight,
        const BiomeSampler &biome,
        const EntranceCandidatePrefilter &candidatePrefilter) const;
    AdventureUndergroundPlan getCachedRawAdventureUndergroundPlanForCell(
        int cellX, int cellZ,
        const SurfaceHeightSampler &surfaceHeight,
        const BiomeSampler &biome,
        const EntranceCandidatePrefilter &candidatePrefilter) const;
    void rememberCachedAdventureUndergroundPlanForCell(
        int cellX, int cellZ,
        const AdventureUndergroundPlan &plan) const;
    bool shouldCarve(int worldX, int y, int worldZ) const noexcept;
    double sample(double x, double y, double z,
                  std::uint64_t salt) const noexcept;
    double lattice(int x, int y, int z,
                   std::uint64_t salt) const noexcept;

    std::uint64_t m_seed = 0;
    int m_generationVersion = LegacyTerrainGenerationVersion;
    struct AdventurePlanCacheEntry {
        bool occupied = false;
        bool hasEntrance = false;
        bool hasRawPlan = false;
        bool hasFinalPlan = false;
        int cellX = 0;
        int cellZ = 0;
        NaturalEntrance entrance;
        AdventureUndergroundPlan rawPlan;
        AdventureUndergroundPlan plan;
    };
    static_assert(AdventurePlanCacheCapacity *
                      sizeof(AdventurePlanCacheEntry) < 64 * 1024,
                  "adventure plan cache must stay within 64 KiB");
    mutable std::array<AdventurePlanCacheEntry,
                       AdventurePlanCacheCapacity>
        m_adventurePlanCache{};
};

#endif // CAVEGENERATOR_H_INCLUDED
