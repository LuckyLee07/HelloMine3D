#ifndef CAVEGENERATOR_H_INCLUDED
#define CAVEGENERATOR_H_INCLUDED

#include "../../../Util/Array2D.h"
#include "../../WorldConstants.h"
#include "TerrainGenerator.h"

#include <cstddef>
#include <cstdint>
#include <functional>

class Chunk;

/// Deterministic world-space cave pass shared by every generated chunk.
class CaveGenerator {
  public:
    using SurfaceHeightSampler = std::function<int(int, int)>;
    using BiomeSampler = std::function<TerrainBiome(int, int)>;

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

    static constexpr int EntranceCellBlocks = CHUNK_SIZE * 6;
    static constexpr int EntranceCandidateCount = 12;
    static constexpr int EntranceTunnelLength = 24;

    explicit CaveGenerator(
        int seed,
        int generationVersion = LegacyTerrainGenerationVersion);

    std::size_t carve(
        Chunk &chunk,
        const Array2D<int, CHUNK_SIZE> &surfaceHeights) const;
    std::size_t carveNaturalEntrances(
        Chunk &chunk, const SurfaceHeightSampler &surfaceHeight,
        const BiomeSampler &biome) const;
    NaturalEntrance getNaturalEntranceForCell(
        int cellX, int cellZ,
        const SurfaceHeightSampler &surfaceHeight,
        const BiomeSampler &biome) const;

  private:
    bool shouldCarve(int worldX, int y, int worldZ) const noexcept;
    double sample(double x, double y, double z,
                  std::uint64_t salt) const noexcept;
    double lattice(int x, int y, int z,
                   std::uint64_t salt) const noexcept;

    std::uint64_t m_seed = 0;
    int m_generationVersion = LegacyTerrainGenerationVersion;
};

#endif // CAVEGENERATOR_H_INCLUDED
