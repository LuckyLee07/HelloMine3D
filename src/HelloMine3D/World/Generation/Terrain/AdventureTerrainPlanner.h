#ifndef HELLOMINE3D_ADVENTURE_TERRAIN_PLANNER_H
#define HELLOMINE3D_ADVENTURE_TERRAIN_PLANNER_H

#include "TerrainFoundation.h"
#include <array>
#include <cstdint>

// Planning identities are independent of saved block and biome IDs.
enum class AdventureRegion : std::uint8_t {
    Meadow, Woodland, ConiferHighland, Dunes, Canyon, Wetland, Alpine,
    Coast, Ocean
};

class AdventureTerrainPlanner {
  public:
    static constexpr std::size_t InlandRegionCount = 7;
    struct Sample {
        TerrainFoundation::Column column;
        AdventureRegion region = AdventureRegion::Meadow;
        std::array<double, InlandRegionCount> weights{};
        double temperature = 0.0;
        double moisture = 0.0;
        double land = 0.0;
    };

    explicit AdventureTerrainPlanner(int seed) noexcept;
    Sample sample(int worldX, int worldZ) const noexcept;

  private:
    double noise(double x, double z, double scale,
                 std::uint64_t salt) const noexcept;
    double random(std::int64_t x, std::int64_t z,
                  std::uint64_t salt) const noexcept;
    std::uint64_t m_seed = 0;
};

#endif
