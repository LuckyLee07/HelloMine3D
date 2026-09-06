#include "TerrainFoundation.h"

#include <algorithm>
#include <cmath>

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
    Column column;
    column.height = std::max(1, std::min(176, static_cast<int>(std::lround(height))));
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
