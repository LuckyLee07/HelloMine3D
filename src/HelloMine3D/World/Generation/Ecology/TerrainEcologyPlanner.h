#ifndef HELLOMINE3D_TERRAIN_ECOLOGY_PLANNER_H
#define HELLOMINE3D_TERRAIN_ECOLOGY_PLANNER_H

#include "../Terrain/TerrainFoundation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

enum class EcologyTreeShape : std::uint8_t {
    Standard,
    Tall,
    Broad
};

enum class EcologyGroundCover : std::uint8_t {
    None,
    TallGrass,
    Rose
};

struct EcologyTreePlan {
    bool place = false;
    EcologyTreeShape shape = EcologyTreeShape::Standard;
    int randomSeed = 0;
    double density = 0.0;
};

struct EcologyGroundCoverPlan {
    EcologyGroundCover cover = EcologyGroundCover::None;
    double density = 0.0;
};

// Pure terrain-v12 placement policy. It consumes only the world identity,
// signed world coordinates and the already-planned v11 column; it never owns
// or reads a Chunk and does not consume the generator's mutable random stream.
class TerrainEcologyPlanner {
  public:
    explicit TerrainEcologyPlanner(int seed) noexcept
        : m_seed(mix(static_cast<std::uint64_t>(
              static_cast<std::int64_t>(seed))))
    {
    }

    EcologyTreePlan planTree(
        int worldX, int worldZ,
        const TerrainFoundation::Column &column) const noexcept
    {
        EcologyTreePlan plan;
        if (!isForest(column.biome) || column.height < 68 ||
            (column.surface != TerrainFoundation::Surface::Original &&
             column.surface != TerrainFoundation::Surface::Grass)) {
            return plan;
        }

        constexpr std::int64_t CellSize = 5;
        const std::int64_t cellX = floorDiv(worldX, CellSize);
        const std::int64_t cellZ = floorDiv(worldZ, CellSize);
        const std::uint64_t cellHash = hashAt(
            cellX, cellZ, 0x82efa98ec4e6c897ull);
        const std::int64_t anchorX = cellX * CellSize + 1 +
            static_cast<std::int64_t>((cellHash >> 9) % 3ull);
        const std::int64_t anchorZ = cellZ * CellSize + 1 +
            static_cast<std::int64_t>((cellHash >> 21) % 3ull);
        if (anchorX != static_cast<std::int64_t>(worldX) ||
            anchorZ != static_cast<std::int64_t>(worldZ)) {
            return plan;
        }

        const double x = static_cast<double>(worldX);
        const double z = static_cast<double>(worldZ);
        const double field = noise(
            x, z, 112.0, 0x243f6a8885a308d3ull) +
            noise(x, z, 36.0, 0x13198a2e03707344ull) * 0.55;
        plan.density = smooth(-0.55, 0.55, field);
        const double chance = column.biome == TerrainBiome::LightForest
            ? 0.20 + 0.60 * plan.density * plan.density
            : 0.24 + 0.73 * plan.density * plan.density;
        const double placementRoll = unit(
            mix(cellHash ^ 0xa4093822299f31d0ull));
        if (placementRoll >= chance) {
            return plan;
        }

        const double form = noise(
            x, z, 112.0, 0x082efa98ec4e6c89ull);
        const double formRoll = unit(
            mix(cellHash ^ 0x452821e638d01377ull));
        const double tallThreshold = 0.14 + 0.22 * plan.density;
        const double broadThreshold = 0.74 + 0.12 * plan.density;
        if (formRoll < tallThreshold ||
            (plan.density > 0.72 && form > 0.35 && formRoll < 0.55)) {
            plan.shape = EcologyTreeShape::Tall;
        }
        else if (formRoll > broadThreshold ||
                 (plan.density < 0.28 && form < -0.25 && formRoll > 0.38)) {
            plan.shape = EcologyTreeShape::Broad;
        }
        else {
            plan.shape = EcologyTreeShape::Standard;
        }
        plan.randomSeed = static_cast<int>(
            (mix(cellHash ^ 0xbe5466cf34e90c6cull) ^
             (cellHash >> 32)) & 0x7fffffffull);
        plan.place = true;
        return plan;
    }

    EcologyGroundCoverPlan planGroundCover(
        int worldX, int worldZ,
        const TerrainFoundation::Column &column) const noexcept
    {
        EcologyGroundCoverPlan plan;
        if (!isForest(column.biome) || column.height < 68 ||
            (column.surface != TerrainFoundation::Surface::Original &&
             column.surface != TerrainFoundation::Surface::Grass)) {
            return plan;
        }
        const double x = static_cast<double>(worldX);
        const double z = static_cast<double>(worldZ);
        const double field = noise(
            x, z, 62.0, 0xc0ac29b7c97c50ddull) +
            noise(x, z, 23.0, 0xe43d17bc92f6a805ull) * 0.30;
        plan.density = smooth(-0.36, 0.58, field);
        const double chance = 0.003 +
            0.038 * plan.density * plan.density;
        const std::uint64_t coordinateHash = hashAt(
            worldX, worldZ, 0x4c91eac7096b32dfull);
        if (unit(coordinateHash) >= chance) {
            return plan;
        }
        plan.cover = unit(mix(coordinateHash ^ 0x3ab095d86cdb72e4ull)) < 0.22
            ? EcologyGroundCover::Rose
            : EcologyGroundCover::TallGrass;
        return plan;
    }

  private:
    static bool isForest(TerrainBiome biome) noexcept
    {
        return biome == TerrainBiome::LightForest ||
               biome == TerrainBiome::TemperateForest;
    }

    static std::int64_t floorDiv(std::int64_t value,
                                 std::int64_t divisor) noexcept
    {
        std::int64_t quotient = value / divisor;
        if (value % divisor < 0) { --quotient; }
        return quotient;
    }

    static std::uint64_t mix(std::uint64_t value) noexcept
    {
        value ^= value >> 30;
        value *= 0xbf58476d1ce4e5b9ull;
        value ^= value >> 27;
        value *= 0x94d049bb133111ebull;
        return value ^ (value >> 31);
    }

    static double smooth(double lower, double upper, double value) noexcept
    {
        const double t = std::max(0.0, std::min(
            1.0, (value - lower) / (upper - lower)));
        return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
    }

    static double unit(std::uint64_t hash) noexcept
    {
        return static_cast<double>(hash >> 11) *
            (1.0 / 9007199254740991.0);
    }

    std::uint64_t hashAt(std::int64_t x, std::int64_t z,
                         std::uint64_t salt) const noexcept
    {
        return mix(m_seed ^ salt ^
            mix(static_cast<std::uint64_t>(x) +
                0x632be59bd9b4e019ull) ^
            mix(static_cast<std::uint64_t>(z) +
                0x8cb92baa3f3d8dd7ull));
    }

    double lattice(std::int64_t x, std::int64_t z,
                   std::uint64_t salt) const noexcept
    {
        return unit(hashAt(x, z, salt)) * 2.0 - 1.0;
    }

    double noise(double x, double z, double scale,
                 std::uint64_t salt) const noexcept
    {
        x /= scale;
        z /= scale;
        const auto ix = static_cast<std::int64_t>(std::floor(x));
        const auto iz = static_cast<std::int64_t>(std::floor(z));
        const double tx = smooth(0.0, 1.0, x - static_cast<double>(ix));
        const double tz = smooth(0.0, 1.0, z - static_cast<double>(iz));
        const double a = lattice(ix, iz, salt);
        const double b = lattice(ix + 1, iz, salt);
        const double c = lattice(ix, iz + 1, salt);
        const double d = lattice(ix + 1, iz + 1, salt);
        const double front = a + (b - a) * tx;
        const double back = c + (d - c) * tx;
        return front + (back - front) * tz;
    }

    std::uint64_t m_seed = 0;
};

#endif
