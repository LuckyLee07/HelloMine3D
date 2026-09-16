#ifndef HELLOMINE3D_TERRAIN_FOUNDATION_H
#define HELLOMINE3D_TERRAIN_FOUNDATION_H

#include "TerrainGenerator.h"
#include <cstdint>

// Versioned terrain planning surface. Pure, bounded work over signed world
// coordinates; no chunk ownership, random stream, world mutation or resident
// data dependency.
class TerrainFoundation {
  public:
    enum class Surface {
        Original,
        Grass,
        Dirt,
        Sand,
        Stone
    };

    struct Column {
        int height = 0;
        TerrainBiome biome = TerrainBiome::Grassland;
        Surface surface = Surface::Original;
    };

    explicit TerrainFoundation(int seed) noexcept;
    Column sample(int worldX, int worldZ) const noexcept;
    Column sampleV8(int worldX, int worldZ) const noexcept;
    Column sampleV9(int worldX, int worldZ) const noexcept;
    Column sampleV10(int worldX, int worldZ) const noexcept;

    static int chunkSeed(int seed, int chunkX, int chunkZ,
                         std::uint64_t salt) noexcept;

  private:
    Column sampleBase(int worldX, int worldZ,
                      double *rawHeight,
                      bool heightOnly = false) const noexcept;
    double noise(double x, double z, double scale,
                 std::uint64_t salt) const noexcept;
    std::uint64_t m_seed = 0;
};

#endif
