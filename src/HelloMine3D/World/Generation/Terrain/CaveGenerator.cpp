#include "CaveGenerator.h"

#include "../../Block/BlockId.h"
#include "../../Chunk/Chunk.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr int MinimumCaveY = 8;
constexpr int SurfaceBuffer = 5;
constexpr int WaterBuffer = 8;

double fade(double value) noexcept
{
    return value * value * (3.0 - 2.0 * value);
}

double interpolate(double left, double right, double amount) noexcept
{
    return left + (right - left) * amount;
}

std::uint64_t mix(std::uint64_t value) noexcept
{
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ull;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebull;
    return value ^ (value >> 31);
}
} // namespace

CaveGenerator::CaveGenerator(int seed)
    : m_seed(mix(static_cast<std::uint64_t>(
          static_cast<std::int64_t>(seed))))
{
}

std::size_t CaveGenerator::carve(
    Chunk &chunk,
    const Array2D<int, CHUNK_SIZE> &surfaceHeights) const
{
    std::size_t carved = 0;
    const glm::ivec2 chunkLocation = chunk.getLocation();
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        const int worldX = chunkLocation.x * CHUNK_SIZE + x;
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            const int worldZ = chunkLocation.y * CHUNK_SIZE + z;
            const int maximumY = std::min(
                surfaceHeights.get(x, z) - SurfaceBuffer,
                WATER_LEVEL - WaterBuffer);
            for (int y = MinimumCaveY; y <= maximumY; ++y) {
                if (static_cast<BlockId>(chunk.getBlock(x, y, z).id) !=
                        BlockId::Stone ||
                    !shouldCarve(worldX, y, worldZ)) {
                    continue;
                }
                chunk.setBlock(x, y, z, BlockId::Air);
                ++carved;
            }
        }
    }
    return carved;
}

bool CaveGenerator::shouldCarve(int worldX, int y, int worldZ) const noexcept
{
    const double tunnels = sample(
        static_cast<double>(worldX) / 18.0,
        static_cast<double>(y) / 12.0,
        static_cast<double>(worldZ) / 18.0,
        0x9e3779b97f4a7c15ull);
    const double detail = sample(
        static_cast<double>(worldX) / 8.0,
        static_cast<double>(y) / 7.0,
        static_cast<double>(worldZ) / 8.0,
        0xd1b54a32d192ed03ull);
    return tunnels + detail * 0.35 > 0.52;
}

double CaveGenerator::sample(double x, double y, double z,
                             std::uint64_t salt) const noexcept
{
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int z0 = static_cast<int>(std::floor(z));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const int z1 = z0 + 1;
    const double tx = fade(x - static_cast<double>(x0));
    const double ty = fade(y - static_cast<double>(y0));
    const double tz = fade(z - static_cast<double>(z0));

    const double lowerFront = interpolate(
        lattice(x0, y0, z0, salt), lattice(x1, y0, z0, salt), tx);
    const double lowerBack = interpolate(
        lattice(x0, y0, z1, salt), lattice(x1, y0, z1, salt), tx);
    const double upperFront = interpolate(
        lattice(x0, y1, z0, salt), lattice(x1, y1, z0, salt), tx);
    const double upperBack = interpolate(
        lattice(x0, y1, z1, salt), lattice(x1, y1, z1, salt), tx);
    return interpolate(
        interpolate(lowerFront, lowerBack, tz),
        interpolate(upperFront, upperBack, tz), ty);
}

double CaveGenerator::lattice(int x, int y, int z,
                              std::uint64_t salt) const noexcept
{
    std::uint64_t value = m_seed ^ salt;
    value ^= mix(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(x)) + 0x632be59bd9b4e019ull);
    value ^= mix(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(y)) + 0x8cb92baa3f3d8dd7ull);
    value ^= mix(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(z)) + 0x58f38ded3d7d53d9ull);
    const std::uint64_t hashed = mix(value);
    constexpr double Unit = 1.0 / 9007199254740991.0;
    return static_cast<double>(hashed >> 11) * Unit * 2.0 - 1.0;
}
