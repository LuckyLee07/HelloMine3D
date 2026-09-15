#include "TerrainFoundation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
std::uint64_t mix(std::uint64_t value) noexcept
{
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ull;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebull;
    return value ^ (value >> 31);
}

double smooth(double lower, double upper, double value) noexcept
{
    const double t = std::max(0.0, std::min(1.0,
        (value - lower) / (upper - lower)));
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

double lattice(std::uint64_t seed, std::int64_t x, std::int64_t z,
               std::uint64_t salt) noexcept
{
    const std::uint64_t hash = mix(seed ^ salt ^
        mix(static_cast<std::uint64_t>(x) + 0x632be59bd9b4e019ull) ^
        mix(static_cast<std::uint64_t>(z) + 0x8cb92baa3f3d8dd7ull));
    return static_cast<double>(hash >> 11) *
        (2.0 / 9007199254740991.0) - 1.0;
}
} // namespace

TerrainFoundation::TerrainFoundation(int seed) noexcept
    : m_seed(mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(seed))))
{
}

double TerrainFoundation::noise(double x, double z, double scale,
                                std::uint64_t salt) const noexcept
{
    // Even INT_MIN/INT_MAX inputs stay well inside int64 lattice range. Offset
    // arithmetic happens in double, never as overflowing signed world ints.
    x /= scale;
    z /= scale;
    const auto ix = static_cast<std::int64_t>(std::floor(x));
    const auto iz = static_cast<std::int64_t>(std::floor(z));
    const double tx = smooth(0.0, 1.0, x - static_cast<double>(ix));
    const double tz = smooth(0.0, 1.0, z - static_cast<double>(iz));
    const double a = lattice(m_seed, ix, iz, salt);
    const double b = lattice(m_seed, ix + 1, iz, salt);
    const double c = lattice(m_seed, ix, iz + 1, salt);
    const double d = lattice(m_seed, ix + 1, iz + 1, salt);
    const double front = a + (b - a) * tx;
    const double back = c + (d - c) * tx;
    return front + (back - front) * tz;
}

TerrainFoundation::Column TerrainFoundation::sample(int worldX, int worldZ) const noexcept
{
    return sampleBase(worldX, worldZ, nullptr);
}

TerrainFoundation::Column TerrainFoundation::sampleBase(
    int worldX, int worldZ, double *rawHeight, bool heightOnly) const noexcept
{
    const double x = static_cast<double>(worldX);
    const double z = static_cast<double>(worldZ);
    // Wide landmass, intermediate rolling hills, and small bounded detail.
    // Biome labels are assigned afterwards, so biome thresholds cannot create
    // a step in the planning surface or an axis-specific ocean shelf.
    const double landmass = noise(x, z, 700.0, 0x243f6a8885a308d3ull);
    const double hills = smooth(-0.4, 0.5,
        noise(x, z, 420.0, 0x13198a2e03707344ull));
    const double base = 70.0 + landmass * 28.0 +
        hills * noise(x, z, 145.0, 0xa4093822299f31d0ull) * 8.0 +
        noise(x, z, 48.0, 0x082efa98ec4e6c89ull) * 1.5;

    const double mountainDomain = smooth(0.05, 0.65,
        noise(x, z, 620.0, 0x452821e638d01377ull));
    const double ridge = 1.0 - std::abs(
        noise(x, z, 190.0, 0xbe5466cf34e90c6cull));
    // A smooth ribbon lowers mountain relief into connected local valleys.
    // It does not carve rivers or place water above the existing sea level.
    const double valley = 1.0 - smooth(0.035, 0.45, std::abs(
        noise(x, z, 400.0, 0xc0ac29b7c97c50ddull)));
    const double mountain = mountainDomain * (1.0 - valley * 0.82);
    double height = base + mountain * (18.0 + ridge * ridge * 58.0);
    // Soft upper shoulder prevents a hard-clipped plateau while retaining the
    // final safety clamp. C1 at 148; no discontinuity in height or first slope.
    if (height > 148.0) {
        height = 148.0 + 28.0 * (1.0 - std::exp(-(height - 148.0) / 28.0));
    }
    if (rawHeight != nullptr) { *rawHeight = height; }
    Column column;
    column.height = std::max(1, std::min(176, static_cast<int>(std::lround(height))));
    // Shore probes only need the raw/rounded height. Avoid the ecology noise
    // for those pure coordinate lookups without changing any column result.
    if (heightOnly) { return column; }
    if (column.height < 64) {
        column.biome = TerrainBiome::Ocean;
    }
    else if (mountain >= 0.48 && column.height >= 80) {
        column.biome = TerrainBiome::Mountain;
    }
    else {
        const double ecology = noise(x, z, 300.0, 0x3f84d5b5b5470917ull);
        column.biome = ecology < -0.35 ? TerrainBiome::Desert :
                       ecology < -0.05 ? TerrainBiome::Grassland :
                       ecology < 0.30 ? TerrainBiome::LightForest :
                                        TerrainBiome::TemperateForest;
    }
    return column;
}

TerrainFoundation::Column TerrainFoundation::sampleV8(
    int worldX, int worldZ) const noexcept
{
    // v7 remains a separate, byte-for-byte stable route. Shore shaping only
    // reads the seed and signed world coordinates; it never asks for a Chunk.
    double rawHeight = 0.0;
    Column column = sampleBase(worldX, worldZ, &rawHeight);
    if (column.height < 56 || column.height > 80 ||
        column.biome == TerrainBiome::Mountain) {
        return column;
    }

    const double x = static_cast<double>(worldX);
    const double z = static_cast<double>(worldZ);
    const double bend = noise(x, z, 116.0, 0xc474dce0f0b68b6full);
    const double detail = noise(x, z, 46.0, 0x9031df796ea784a5ull);
    const double shallowShelf = 1.0 * smooth(56.0, 60.0, rawHeight) *
        (1.0 - smooth(60.0, 64.0, rawHeight));
    const auto clampedWorld = [](std::int64_t value) {
        return static_cast<int>(std::max(
            static_cast<std::int64_t>(std::numeric_limits<int>::min()),
            std::min(static_cast<std::int64_t>(
                         std::numeric_limits<int>::max()), value)));
    };
    double shoulder = 0.0;
    if (rawHeight >= 63.5 && rawHeight < 72.0) {
        // A bounded, world-coordinate shore probe gives flat v7 lowlands a
        // gentle dry ramp without creating an equal-width height stripe.
        // The probe only samples the pure foundation, never neighbour Chunks.
        double nearestWater = std::numeric_limits<double>::infinity();
        for (const auto offset : {std::pair<int, int>{96, 0},
                                  {-96, 0}, {0, 96}, {0, -96}}) {
            const int otherX = clampedWorld(static_cast<std::int64_t>(worldX) +
                                       offset.first);
            const int otherZ = clampedWorld(static_cast<std::int64_t>(worldZ) +
                                       offset.second);
            double otherRaw = 0.0;
            (void)sampleBase(otherX, otherZ, &otherRaw, true);
            if (otherRaw >= 63.5) { continue; }
            const double distance = std::hypot(
                static_cast<double>(otherX) - x,
                static_cast<double>(otherZ) - z);
            const double estimate = distance *
                (rawHeight - 63.5) / (rawHeight - otherRaw);
            nearestWater = std::min(nearestWater, estimate);
        }
        if (nearestWater < 64.0) {
            const double rise = 18.0 + detail * 4.0;
            const double amplitude = 3.5 + bend * 0.7;
            shoulder = amplitude * smooth(0.0, rise, nearestWater) *
                (1.0 - smooth(32.0, 64.0, nearestWater)) *
                (1.0 - smooth(66.0, 72.0, rawHeight));
        }
        if (shoulder > 0.0) {
            // A nearby water column wins over the coarse 96-block estimate.
            // This prevents a single raised shore column from becoming a
            // four-block lip at an oblique or narrow inlet.
            for (const auto offset : {std::pair<int, int>{1, 0},
                                      {-1, 0}, {0, 1}, {0, -1}}) {
                const int otherX = clampedWorld(
                    static_cast<std::int64_t>(worldX) + offset.first);
                const int otherZ = clampedWorld(
                    static_cast<std::int64_t>(worldZ) + offset.second);
                if (otherX == worldX && otherZ == worldZ) { continue; }
                const Column neighbour = sampleBase(otherX, otherZ, nullptr, true);
                const int allowance = neighbour.height < 64 ? 2 : 3;
                shoulder = std::max(0.0, std::min(shoulder,
                    static_cast<double>(neighbour.height + allowance) -
                    rawHeight - shallowShelf));
            }
        }
    }
    const double height = rawHeight + shoulder + shallowShelf;
    column.height = std::max(1, std::min(176,
        static_cast<int>(std::lround(height))));

    // The v7 ocean threshold is height-based. Keep labels and surface height
    // in agreement when a shaped column crosses that threshold.
    if (column.height < 64) {
        column.biome = TerrainBiome::Ocean;
    }
    else if (column.biome == TerrainBiome::Ocean) {
        const double ecology = noise(x, z, 300.0, 0x3f84d5b5b5470917ull);
        column.biome = ecology < -0.35 ? TerrainBiome::Desert :
                       ecology < -0.05 ? TerrainBiome::Grassland :
                       ecology < 0.30 ? TerrainBiome::LightForest :
                                            TerrainBiome::TemperateForest;
    }

    // Low-frequency boundaries make a variable-width, coherent dry shore;
    // material decisions use the same column as generation and placement.
    const double dryLimit = 64.5 + bend * 1.8 + detail * 0.15;
    if (column.height < 64) {
        if (column.height >= 59) {
            const double rock = noise(x, z, 82.0, 0x91e10da5c79e7b1dull);
            column.surface = column.height <= 60 && rock > 0.45
                ? Surface::Stone : Surface::Sand;
        }
        return column;
    }
    if (static_cast<double>(column.height) <= dryLimit) {
        column.surface = Surface::Sand;
        return column;
    }
    if (static_cast<double>(column.height) <= dryLimit + 1.35 &&
        column.biome != TerrainBiome::Desert) {
        column.surface = Surface::Dirt;
        return column;
    }

    // Widen the sand-to-grass ecotone on both sides of the biome boundary.
    // Smooth coordinate noise bends the contour without per-block random dots.
    if (column.biome == TerrainBiome::Desert ||
        column.biome == TerrainBiome::Grassland) {
        const double ecology = noise(x, z, 300.0, 0x3f84d5b5b5470917ull) +
            noise(x, z, 75.0, 0x3ab095d86cdb72e4ull) * 0.12;
        if (ecology < -0.375) {
            column.surface = Surface::Sand;
        }
        else if (ecology < -0.305) {
            column.surface = Surface::Dirt;
        }
        else if (column.height <= 76) {
            column.surface = Surface::Grass;
        }
    }
    return column;
}

int TerrainFoundation::chunkSeed(int seed, int chunkX, int chunkZ,
                                 std::uint64_t salt) noexcept
{
    const std::uint64_t hash = mix(
        static_cast<std::uint64_t>(static_cast<std::int64_t>(seed)) ^ salt ^
        mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(chunkX))) ^
        mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(chunkZ)) +
            0x9e3779b97f4a7c15ull));
    return static_cast<int>(hash & 0x7fffffffull);
}
