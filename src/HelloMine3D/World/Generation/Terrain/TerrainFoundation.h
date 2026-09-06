#ifndef HELLOMINE3D_TERRAIN_FOUNDATION_H
#define HELLOMINE3D_TERRAIN_FOUNDATION_H

#include "TerrainGenerator.h"
#include <cstdint>

// Terrain-v5 planning surface. Pure, bounded work over signed world coordinates;
// no chunk ownership, random stream, world mutation or resident-data dependency.
class TerrainFoundation {
  public:
    struct Column {
        int height = 0;
        TerrainBiome biome = TerrainBiome::Grassland;
    };

    explicit TerrainFoundation(int seed) noexcept;
    Column sample(int worldX, int worldZ) const noexcept;

    static int chunkSeed(int seed, int chunkX, int chunkZ,
                         std::uint64_t salt) noexcept;

  private:
    double noise(double x, double z, double scale,
                 std::uint64_t salt) const noexcept;
    std::uint64_t m_seed = 0;
};

#endif
