#include "StructurePlanning.h"

#include <algorithm>
#include <cstdint>
#include <tuple>
#include <utility>

#include "../../WorldConstants.h"
#include "../../WorldCoordinates.h"

namespace {
// Signed halves of the frozen terrain-v2 salt. Naming the exact 32-bit values
// avoids compiler-dependent narrowing diagnostics while preserving the old
// XOR stream bit for bit.
constexpr int WaystoneSaltLow = -205731575;
constexpr int WaystoneSaltHigh = 1779033703;

std::uint64_t mixStructureValue(std::uint64_t value)
{
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ull;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebull;
    return value ^ (value >> 31);
}

std::uint64_t compatibilityStructureHash(int seed, int worldX, int worldZ)
{
    std::uint64_t value = mixStructureValue(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(seed)));
    value ^= mixStructureValue(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(worldX)) + 0x632be59bd9b4e019ull);
    value ^= mixStructureValue(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(worldZ)) + 0x8cb92baa3f3d8dd7ull);
    return mixStructureValue(value ^ 0xd1b54a32d192ed03ull);
}

bool preferredPlan(const StructurePlanSnapshot &left,
                   const StructurePlanSnapshot &right)
{
    if (left.projectionPriority != right.projectionPriority) {
        return left.projectionPriority > right.projectionPriority;
    }
    return std::tie(left.key.type, left.key.terrainGenerationVersion,
                    left.key.cellX, left.key.cellZ,
                    left.anchor.x, left.anchor.y, left.anchor.z) <
           std::tie(right.key.type, right.key.terrainGenerationVersion,
                    right.key.cellX, right.key.cellZ,
                    right.anchor.x, right.anchor.y, right.anchor.z);
}
} // namespace

const char *structureTypeName(StructureType type) noexcept
{
    switch (type) {
        case StructureType::Waystone: return "waystone";
    }
    return "unknown";
}

bool StructureInstanceKey::operator==(
    const StructureInstanceKey &other) const noexcept
{
    return type == other.type &&
           terrainGenerationVersion == other.terrainGenerationVersion &&
           cellX == other.cellX && cellZ == other.cellZ;
}

bool StructureFootprint::valid() const noexcept
{
    return minimumX <= maximumX && minimumY <= maximumY &&
           minimumZ <= maximumZ;
}

int StructureFootprint::width() const noexcept
{
    return valid() ? maximumX - minimumX + 1 : 0;
}

int StructureFootprint::height() const noexcept
{
    return valid() ? maximumY - minimumY + 1 : 0;
}

int StructureFootprint::depth() const noexcept
{
    return valid() ? maximumZ - minimumZ + 1 : 0;
}

bool StructureFootprint::overlapsHorizontal(
    const StructureFootprint &other) const noexcept
{
    return valid() && other.valid() &&
           minimumX <= other.maximumX && maximumX >= other.minimumX &&
           minimumZ <= other.maximumZ && maximumZ >= other.minimumZ;
}

bool StructureFootprint::overlaps(
    const StructureFootprint &other) const noexcept
{
    return valid() && other.valid() &&
           minimumX <= other.maximumX && maximumX >= other.minimumX &&
           minimumY <= other.maximumY && maximumY >= other.minimumY &&
           minimumZ <= other.maximumZ && maximumZ >= other.minimumZ;
}

bool StructureFootprint::overlapsChunk(int chunkX, int chunkZ) const noexcept
{
    if (!valid()) {
        return false;
    }
    const int chunkMinimumX = chunkX * CHUNK_SIZE;
    const int chunkMaximumX = chunkMinimumX + CHUNK_SIZE - 1;
    const int chunkMinimumZ = chunkZ * CHUNK_SIZE;
    const int chunkMaximumZ = chunkMinimumZ + CHUNK_SIZE - 1;
    return minimumX <= chunkMaximumX && maximumX >= chunkMinimumX &&
           minimumZ <= chunkMaximumZ && maximumZ >= chunkMinimumZ;
}

DeterministicStructurePlanner::DeterministicStructurePlanner(
    int seed, int terrainGenerationVersion,
    SurfaceHeightSampler surfaceHeight)
    : m_seed(seed)
    , m_terrainGenerationVersion(terrainGenerationVersion)
    , m_surfaceHeight(std::move(surfaceHeight))
{
}

StructurePlanSnapshot DeterministicStructurePlanner::planForCell(
    StructureType type, int cellX, int cellZ) const
{
    switch (type) {
        case StructureType::Waystone:
            return planWaystoneForCell(cellX, cellZ);
    }
    StructurePlanSnapshot invalid;
    invalid.key = {type, m_terrainGenerationVersion, cellX, cellZ};
    return invalid;
}

StructurePlanSnapshot
DeterministicStructurePlanner::planWaystoneForCell(int cellX,
                                                   int cellZ) const
{
    StructurePlanSnapshot plan;
    plan.key = {StructureType::Waystone, m_terrainGenerationVersion,
                cellX, cellZ};
    plan.projectionPriority = WaystoneProjectionPriority;
    plan.plannedBlockCount = WaystonePlannedBlockCount;
    if (m_terrainGenerationVersion < WaystoneMinimumTerrainVersion ||
        !m_surfaceHeight) {
        return plan;
    }

    const int cellSize = WaystoneCellChunks * CHUNK_SIZE;
    const int inset = WaystoneRadius + 1;
    const int coordinateRange = cellSize - inset * 2;
    int bestHeight = -1;
    int bestX = 0;
    int bestZ = 0;
    int bestCandidate = -1;
    std::uint64_t bestHash = 0;

    // This is the exact terrain-v2 Waystone selection stream. Keeping the
    // compatibility branch here makes the structure type and terrain version
    // explicit without moving existing unexplored v2 landmarks.
    for (int attempt = 0; attempt < WaystoneCandidateCount; ++attempt) {
        const std::uint64_t xHash = compatibilityStructureHash(
            m_seed ^ WaystoneSaltLow,
            cellX * 31 + attempt, cellZ * 17 - attempt);
        const std::uint64_t zHash = compatibilityStructureHash(
            m_seed ^ WaystoneSaltHigh,
            cellX * 13 - attempt, cellZ * 37 + attempt);
        const int worldX = cellX * cellSize + inset +
            static_cast<int>(xHash %
                             static_cast<std::uint64_t>(coordinateRange));
        const int worldZ = cellZ * cellSize + inset +
            static_cast<int>(zHash %
                             static_cast<std::uint64_t>(coordinateRange));
        const int height = m_surfaceHeight(worldX, worldZ);
        if (height > bestHeight) {
            bestHeight = height;
            bestX = worldX;
            bestZ = worldZ;
            bestCandidate = attempt;
            bestHash = mixStructureValue(
                xHash ^ (zHash << 1) ^
                static_cast<std::uint64_t>(attempt));
        }
    }

    plan.anchor = {bestX, bestHeight, bestZ};
    plan.selectedCandidate = bestCandidate;
    plan.selectionHash = bestHash;
    plan.valid = bestHeight >= WATER_LEVEL + 4;
    if (plan.valid) {
        plan.footprint = {
            bestX - WaystoneRadius, bestX + WaystoneRadius,
            bestHeight + 1, bestHeight + WaystoneHeight,
            bestZ - WaystoneRadius, bestZ + WaystoneRadius};
    }
    return plan;
}

std::vector<StructurePlanSnapshot>
DeterministicStructurePlanner::plansForChunk(int chunkX, int chunkZ) const
{
    std::vector<StructurePlanSnapshot> candidates;
    const int targetMinimumX = chunkX * CHUNK_SIZE;
    const int targetMaximumX = targetMinimumX + CHUNK_SIZE - 1;
    const int targetMinimumZ = chunkZ * CHUNK_SIZE;
    const int targetMaximumZ = targetMinimumZ + CHUNK_SIZE - 1;
    const int cellSize = WaystoneCellChunks * CHUNK_SIZE;
    const int minimumCellX = WorldCoordinates::floorDiv(
        targetMinimumX - WaystoneRadius, cellSize);
    const int maximumCellX = WorldCoordinates::floorDiv(
        targetMaximumX + WaystoneRadius, cellSize);
    const int minimumCellZ = WorldCoordinates::floorDiv(
        targetMinimumZ - WaystoneRadius, cellSize);
    const int maximumCellZ = WorldCoordinates::floorDiv(
        targetMaximumZ + WaystoneRadius, cellSize);

    for (int cellX = minimumCellX; cellX <= maximumCellX; ++cellX) {
        for (int cellZ = minimumCellZ; cellZ <= maximumCellZ; ++cellZ) {
            StructurePlanSnapshot plan = planForCell(
                StructureType::Waystone, cellX, cellZ);
            if (plan.valid && plan.footprint.overlapsChunk(chunkX, chunkZ)) {
                candidates.push_back(std::move(plan));
            }
        }
    }
    std::vector<StructurePlanSnapshot> resolved =
        resolveOverlaps(std::move(candidates));
    if (resolved.size() > MaximumPlansPerChunk) {
        resolved.resize(MaximumPlansPerChunk);
    }
    return resolved;
}

std::vector<StructurePlanSnapshot>
DeterministicStructurePlanner::resolveOverlaps(
    std::vector<StructurePlanSnapshot> candidates)
{
    candidates.erase(std::remove_if(
        candidates.begin(), candidates.end(),
        [](const StructurePlanSnapshot &candidate) {
            return !candidate.valid || !candidate.footprint.valid();
        }), candidates.end());
    std::sort(candidates.begin(), candidates.end(), preferredPlan);

    std::vector<StructurePlanSnapshot> resolved;
    resolved.reserve(candidates.size());
    for (const StructurePlanSnapshot &candidate : candidates) {
        const bool blocked = std::any_of(
            resolved.begin(), resolved.end(),
            [&candidate](const StructurePlanSnapshot &accepted) {
                return candidate.footprint.overlapsHorizontal(
                    accepted.footprint);
            });
        if (!blocked) {
            resolved.push_back(candidate);
        }
    }
    std::sort(resolved.begin(), resolved.end(),
              [](const StructurePlanSnapshot &left,
                 const StructurePlanSnapshot &right) {
                  return std::tie(left.key.type,
                                  left.key.terrainGenerationVersion,
                                  left.key.cellX, left.key.cellZ) <
                         std::tie(right.key.type,
                                  right.key.terrainGenerationVersion,
                                  right.key.cellX, right.key.cellZ);
              });
    return resolved;
}
