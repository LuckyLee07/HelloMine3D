#include "CaveGenerator.h"

#include "../../Block/BlockId.h"
#include "../../Chunk/Chunk.h"
#include "../../WorldCoordinates.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr int MinimumCaveY = 8;
constexpr int SurfaceBuffer = 5;
constexpr int WaterBuffer = 8;
constexpr int EntranceEdgeInset = 12;
constexpr int EntranceMinimumRelief = 16;
constexpr int EntranceHalfWidth = 1;
constexpr int EntranceHeight = 3;
constexpr int ChamberRadius = 3;

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

std::uint64_t entranceHash(std::uint64_t seed, int cellX, int cellZ,
                           int candidate) noexcept
{
    std::uint64_t value = seed ^ 0xa0761d6478bd642full;
    value ^= mix(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(cellX)) + 0xe7037ed1a0b428dbull);
    value ^= mix(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(cellZ)) + 0x8ebc6af09c88c6e3ull);
    value ^= mix(static_cast<std::uint64_t>(candidate) +
                 0x589965cc75374cc3ull);
    return mix(value);
}
} // namespace

CaveGenerator::CaveGenerator(int seed, int generationVersion)
    : m_seed(mix(static_cast<std::uint64_t>(
          static_cast<std::int64_t>(seed))))
    , m_generationVersion(
          generationVersion >= LegacyTerrainGenerationVersion &&
                  generationVersion <= CurrentTerrainGenerationVersion
              ? generationVersion
              : CurrentTerrainGenerationVersion)
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
            const int maximumY =
                m_generationVersion >= MountainTerrainGenerationVersion
                ? std::min(surfaceHeights.get(x, z) - SurfaceBuffer,
                           WATER_LEVEL + 24)
                : std::min(surfaceHeights.get(x, z) - SurfaceBuffer,
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

CaveGenerator::NaturalEntrance CaveGenerator::getNaturalEntranceForCell(
    int cellX, int cellZ,
    const SurfaceHeightSampler &surfaceHeight,
    const BiomeSampler &biome) const
{
    NaturalEntrance entrance;
    entrance.cellX = cellX;
    entrance.cellZ = cellZ;
    if (m_generationVersion < MountainTerrainGenerationVersion ||
        !surfaceHeight || !biome) {
        return entrance;
    }

    constexpr int CandidateSpan =
        EntranceCellBlocks - EntranceEdgeInset * 2;
    const std::array<std::array<int, 2>, 4> directions{{
        {{1, 0}}, {{-1, 0}}, {{0, 1}}, {{0, -1}},
    }};
    for (int candidate = 0; candidate < EntranceCandidateCount;
         ++candidate) {
        const std::uint64_t hash = entranceHash(
            m_seed, cellX, cellZ, candidate);
        const int worldX = cellX * EntranceCellBlocks +
            EntranceEdgeInset + static_cast<int>(hash % CandidateSpan);
        const int worldZ = cellZ * EntranceCellBlocks +
            EntranceEdgeInset + static_cast<int>(
                (hash >> 16) % CandidateSpan);
        const int surface = surfaceHeight(worldX, worldZ);
        if (biome(worldX, worldZ) != TerrainBiome::Mountain ||
            surface < WATER_LEVEL + EntranceMinimumRelief) {
            continue;
        }

        int directionIndex = static_cast<int>((hash >> 32) % 4);
        int greatestCover = surfaceHeight(
            worldX + directions[directionIndex][0] * EntranceTunnelLength,
            worldZ + directions[directionIndex][1] * EntranceTunnelLength);
        for (int index = 0; index < 4; ++index) {
            const int cover = surfaceHeight(
                worldX + directions[index][0] * EntranceTunnelLength,
                worldZ + directions[index][1] * EntranceTunnelLength);
            if (cover > greatestCover) {
                directionIndex = index;
                greatestCover = cover;
            }
        }
        if (greatestCover < surface + 2) {
            continue;
        }
        const int targetX = worldX +
            directions[directionIndex][0] * EntranceTunnelLength;
        const int targetZ = worldZ +
            directions[directionIndex][1] * EntranceTunnelLength;
        if (biome(targetX, targetZ) != TerrainBiome::Mountain) {
            continue;
        }

        entrance.valid = true;
        entrance.anchorX = worldX;
        entrance.anchorY = surface;
        entrance.anchorZ = worldZ;
        entrance.directionX = directions[directionIndex][0];
        entrance.directionZ = directions[directionIndex][1];
        entrance.endY = surface - EntranceTunnelLength * 3 / 4;
        return entrance;
    }
    return entrance;
}

std::size_t CaveGenerator::carveNaturalEntrances(
    Chunk &chunk, const SurfaceHeightSampler &surfaceHeight,
    const BiomeSampler &biome) const
{
    if (m_generationVersion < MountainTerrainGenerationVersion) {
        return 0;
    }

    std::size_t carved = 0;
    const glm::ivec2 chunkLocation = chunk.getLocation();
    const int chunkMinimumX = chunkLocation.x * CHUNK_SIZE;
    const int chunkMinimumZ = chunkLocation.y * CHUNK_SIZE;
    const int chunkMaximumX = chunkMinimumX + CHUNK_SIZE - 1;
    const int chunkMaximumZ = chunkMinimumZ + CHUNK_SIZE - 1;
    const int reach = EntranceTunnelLength + ChamberRadius;
    const int minimumCellX = WorldCoordinates::floorDiv(
        chunkMinimumX - reach, EntranceCellBlocks);
    const int maximumCellX = WorldCoordinates::floorDiv(
        chunkMaximumX + reach, EntranceCellBlocks);
    const int minimumCellZ = WorldCoordinates::floorDiv(
        chunkMinimumZ - reach, EntranceCellBlocks);
    const int maximumCellZ = WorldCoordinates::floorDiv(
        chunkMaximumZ + reach, EntranceCellBlocks);

    const auto carveBlock = [&](int worldX, int y, int worldZ) {
        if (worldX < chunkMinimumX || worldX > chunkMaximumX ||
            worldZ < chunkMinimumZ || worldZ > chunkMaximumZ || y < 1) {
            return;
        }
        const int localX = worldX - chunkMinimumX;
        const int localZ = worldZ - chunkMinimumZ;
        const BlockId block = static_cast<BlockId>(
            chunk.getBlock(localX, y, localZ).id);
        if (block == BlockId::Air || block == BlockId::Water) {
            return;
        }
        chunk.setBlock(localX, y, localZ, BlockId::Air);
        ++carved;
    };

    for (int cellX = minimumCellX; cellX <= maximumCellX; ++cellX) {
        for (int cellZ = minimumCellZ; cellZ <= maximumCellZ; ++cellZ) {
            const NaturalEntrance entrance = getNaturalEntranceForCell(
                cellX, cellZ, surfaceHeight, biome);
            if (!entrance.valid) {
                continue;
            }

            const int perpendicularX = -entrance.directionZ;
            const int perpendicularZ = entrance.directionX;
            for (int step = 0; step <= EntranceTunnelLength; ++step) {
                const int centerX = entrance.anchorX +
                    entrance.directionX * step;
                const int centerZ = entrance.anchorZ +
                    entrance.directionZ * step;
                const int floorY = entrance.anchorY - step * 3 / 4;
                for (int lateral = -EntranceHalfWidth;
                     lateral <= EntranceHalfWidth; ++lateral) {
                    for (int vertical = 0; vertical < EntranceHeight;
                         ++vertical) {
                        carveBlock(centerX + perpendicularX * lateral,
                                   floorY + vertical,
                                   centerZ + perpendicularZ * lateral);
                    }
                }
            }

            const int endX = entrance.anchorX +
                entrance.directionX * EntranceTunnelLength;
            const int endZ = entrance.anchorZ +
                entrance.directionZ * EntranceTunnelLength;
            for (int offsetX = -ChamberRadius;
                 offsetX <= ChamberRadius; ++offsetX) {
                for (int offsetZ = -ChamberRadius;
                     offsetZ <= ChamberRadius; ++offsetZ) {
                    if (offsetX * offsetX + offsetZ * offsetZ >
                        ChamberRadius * ChamberRadius) {
                        continue;
                    }
                    for (int vertical = -1; vertical <= 3; ++vertical) {
                        carveBlock(endX + offsetX,
                                   entrance.endY + vertical,
                                   endZ + offsetZ);
                    }
                }
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
