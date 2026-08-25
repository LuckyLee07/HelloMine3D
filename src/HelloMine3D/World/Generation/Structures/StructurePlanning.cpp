#include "StructurePlanning.h"

#include <algorithm>
#include <cstdint>
#include <limits>
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

// Terrain-v3 structure selection and candidate streams. These values are part
// of the generation identity and must not be changed without another terrain
// version.
constexpr int CellTypeSalt = 12648430;
constexpr int RuinSaltLow = 1103515245;
constexpr int RuinSaltHigh = -1640531527;
constexpr int CampSaltLow = 214013;
constexpr int CampSaltHigh = -1103515245;
constexpr std::uint64_t LootSalt = 0xa0761d6478bd642full;

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

bool eligibleSiteBiome(StructureType type, TerrainBiome biome) noexcept
{
    if (type == StructureType::Ruin) {
        return biome == TerrainBiome::LightForest ||
               biome == TerrainBiome::TemperateForest;
    }
    if (type == StructureType::RaiderCamp) {
        return biome == TerrainBiome::Desert ||
               biome == TerrainBiome::Grassland;
    }
    return false;
}

int siteRadiusX(StructureType type) noexcept
{
    return type == StructureType::Ruin
               ? DeterministicStructurePlanner::RuinRadius
               : DeterministicStructurePlanner::CampRadiusX;
}

int siteRadiusZ(StructureType type) noexcept
{
    return type == StructureType::Ruin
               ? DeterministicStructurePlanner::RuinRadius
               : DeterministicStructurePlanner::CampRadiusZ;
}

int siteSaltLow(StructureType type) noexcept
{
    return type == StructureType::Ruin ? RuinSaltLow : CampSaltLow;
}

int siteSaltHigh(StructureType type) noexcept
{
    return type == StructureType::Ruin ? RuinSaltHigh : CampSaltHigh;
}

int boundedLootAmount(std::uint64_t hash, unsigned shift,
                      int minimum, int maximum) noexcept
{
    const std::uint64_t range = static_cast<std::uint64_t>(
        maximum - minimum + 1);
    return minimum + static_cast<int>((hash >> shift) % range);
}
} // namespace

const char *structureTypeName(StructureType type) noexcept
{
    switch (type) {
        case StructureType::Waystone: return "waystone";
        case StructureType::Ruin: return "ruin";
        case StructureType::RaiderCamp: return "raider_camp";
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

bool StructureLootEntry::operator==(
    const StructureLootEntry &other) const noexcept
{
    return materialId == other.materialId && amount == other.amount;
}

StructureLootSnapshot
structureLootForPlan(const StructurePlanSnapshot &plan)
{
    StructureLootSnapshot loot;
    loot.key = plan.key;
    loot.chestPosition = plan.chestPosition;
    if (!plan.valid || !plan.hasChest ||
        (plan.key.type != StructureType::Ruin &&
         plan.key.type != StructureType::RaiderCamp)) {
        return loot;
    }

    std::uint64_t hash = plan.selectionHash ^ LootSalt;
    hash ^= mixStructureValue(static_cast<std::uint64_t>(
        static_cast<unsigned>(plan.key.type) + 1u));
    hash ^= mixStructureValue(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(plan.key.terrainGenerationVersion)));
    hash ^= mixStructureValue(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(plan.key.cellX)));
    hash ^= mixStructureValue(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(plan.key.cellZ)));
    loot.selectionHash = mixStructureValue(hash);

    if (plan.key.type == StructureType::Ruin) {
        loot.entries = {
            {Material::ID::IronIngot,
             boundedLootAmount(loot.selectionHash, 0, 1, 2)},
            {Material::ID::Glass,
             boundedLootAmount(loot.selectionHash, 13, 2, 5)},
            {Material::ID::WheatSeeds,
             boundedLootAmount(loot.selectionHash, 29, 2, 6)},
        };
    }
    else {
        loot.entries = {
            {Material::ID::Bread,
             boundedLootAmount(loot.selectionHash, 0, 1, 3)},
            {Material::ID::CoalOre,
             boundedLootAmount(loot.selectionHash, 17, 4, 8)},
            {Material::ID::IronOre,
             boundedLootAmount(loot.selectionHash, 37, 1, 4)},
        };
    }
    loot.valid = true;
    return loot;
}

DeterministicStructurePlanner::DeterministicStructurePlanner(
    int seed, int terrainGenerationVersion,
    SurfaceHeightSampler surfaceHeight, BiomeSampler biome)
    : m_seed(seed)
    , m_terrainGenerationVersion(terrainGenerationVersion)
    , m_surfaceHeight(std::move(surfaceHeight))
    , m_biome(std::move(biome))
{
}

StructureType DeterministicStructurePlanner::selectedStructureTypeForCell(
    int cellX, int cellZ) const
{
    if (m_terrainGenerationVersion <
        ExplorationSiteMinimumTerrainVersion) {
        return StructureType::Waystone;
    }
    const std::uint64_t selection = compatibilityStructureHash(
        m_seed ^ CellTypeSalt,
        cellX * 47 + m_terrainGenerationVersion * 7,
        cellZ * 53 - m_terrainGenerationVersion * 11);
    switch (selection % 3ull) {
        case 0: return StructureType::Waystone;
        case 1: return StructureType::Ruin;
        default: return StructureType::RaiderCamp;
    }
}

StructurePlanSnapshot DeterministicStructurePlanner::planForCell(
    StructureType type, int cellX, int cellZ) const
{
    switch (type) {
        case StructureType::Waystone:
            return planWaystoneForCell(cellX, cellZ);
        case StructureType::Ruin:
        case StructureType::RaiderCamp:
            return planExplorationSiteForCell(type, cellX, cellZ);
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
        !m_surfaceHeight ||
        (m_terrainGenerationVersion >=
             ExplorationSiteMinimumTerrainVersion &&
         selectedStructureTypeForCell(cellX, cellZ) !=
             StructureType::Waystone)) {
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
    if (m_biome && bestCandidate >= 0) {
        plan.biome = m_biome(bestX, bestZ);
    }
    plan.valid = bestHeight >= WATER_LEVEL + 4;
    if (plan.valid) {
        plan.footprint = {
            bestX - WaystoneRadius, bestX + WaystoneRadius,
            bestHeight + 1, bestHeight + WaystoneHeight,
            bestZ - WaystoneRadius, bestZ + WaystoneRadius};
    }
    return plan;
}

StructurePlanSnapshot
DeterministicStructurePlanner::planExplorationSiteForCell(
    StructureType type, int cellX, int cellZ) const
{
    StructurePlanSnapshot plan;
    plan.key = {type, m_terrainGenerationVersion, cellX, cellZ};
    plan.projectionPriority = type == StructureType::Ruin
        ? RuinProjectionPriority : CampProjectionPriority;
    plan.plannedBlockCount = type == StructureType::Ruin
        ? RuinPlannedBlockCount : CampPlannedBlockCount;
    if ((type != StructureType::Ruin &&
         type != StructureType::RaiderCamp) ||
        m_terrainGenerationVersion <
            ExplorationSiteMinimumTerrainVersion ||
        selectedStructureTypeForCell(cellX, cellZ) != type ||
        !m_surfaceHeight || !m_biome) {
        return plan;
    }

    const int cellSize = SiteCellChunks * CHUNK_SIZE;
    const int coordinateRange = cellSize - SiteEdgeInset * 2;
    const int radiusX = siteRadiusX(type);
    const int radiusZ = siteRadiusZ(type);
    struct SiteCandidate {
        int x = 0;
        int z = 0;
        int anchorHeight = -1;
        int index = -1;
        TerrainBiome biome = TerrainBiome::Ocean;
        std::uint64_t hash = 0;
    };
    std::vector<SiteCandidate> candidates;
    candidates.reserve(SiteCandidateCount);

    for (int attempt = 0; attempt < SiteCandidateCount; ++attempt) {
        const std::uint64_t xHash = compatibilityStructureHash(
            m_seed ^ siteSaltLow(type),
            cellX * 43 + attempt, cellZ * 29 - attempt);
        const std::uint64_t zHash = compatibilityStructureHash(
            m_seed ^ siteSaltHigh(type),
            cellX * 19 - attempt, cellZ * 61 + attempt);
        const int worldX = cellX * cellSize + SiteEdgeInset +
            static_cast<int>(xHash %
                             static_cast<std::uint64_t>(coordinateRange));
        const int worldZ = cellZ * cellSize + SiteEdgeInset +
            static_cast<int>(zHash %
                             static_cast<std::uint64_t>(coordinateRange));
        const TerrainBiome biome = m_biome(worldX, worldZ);
        if (!eligibleSiteBiome(type, biome)) {
            continue;
        }
        const int anchorHeight = m_surfaceHeight(worldX, worldZ);
        if (anchorHeight < WATER_LEVEL + 4) {
            continue;
        }

        const std::uint64_t candidateHash = mixStructureValue(
            xHash ^ (zHash << 1) ^
            static_cast<std::uint64_t>(attempt) ^
            static_cast<std::uint64_t>(static_cast<unsigned>(type)));
        candidates.push_back({worldX, worldZ, anchorHeight, attempt,
                              biome, candidateHash});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const SiteCandidate &left, const SiteCandidate &right) {
                  if (left.anchorHeight != right.anchorHeight) {
                      return left.anchorHeight > right.anchorHeight;
                  }
                  return left.hash < right.hash;
              });

    SiteCandidate selected;
    int selectedSurfaceHeight = -1;
    for (const SiteCandidate &candidate : candidates) {
        int minimumHeight = std::numeric_limits<int>::max();
        int maximumHeight = std::numeric_limits<int>::min();
        for (int x = candidate.x - radiusX;
             x <= candidate.x + radiusX; ++x) {
            for (int z = candidate.z - radiusZ;
                 z <= candidate.z + radiusZ; ++z) {
                const int height = m_surfaceHeight(x, z);
                minimumHeight = std::min(minimumHeight, height);
                maximumHeight = std::max(maximumHeight, height);
            }
        }
        if (minimumHeight >= WATER_LEVEL + 4 &&
            maximumHeight - minimumHeight <= MaximumSiteRelief) {
            selected = candidate;
            selectedSurfaceHeight = maximumHeight;
            break;
        }
    }

    if (selected.index < 0) {
        return plan;
    }
    const int maximumY = selectedSurfaceHeight +
        (type == StructureType::Ruin ? RuinHeight : CampHeight);
    plan.valid = true;
    plan.anchor = {selected.x, selectedSurfaceHeight, selected.z};
    plan.footprint = {selected.x - radiusX, selected.x + radiusX,
                      selectedSurfaceHeight - 2, maximumY,
                      selected.z - radiusZ, selected.z + radiusZ};
    plan.biome = selected.biome;
    plan.selectedCandidate = selected.index;
    plan.selectionHash = selected.hash;
    plan.hasChest = true;
    plan.chestPosition = {
        selected.x, selectedSurfaceHeight + 2,
        selected.z + (type == StructureType::RaiderCamp ? 1 : 0)};
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
    const int cellSize = SiteCellChunks * CHUNK_SIZE;
    const int minimumCellX = WorldCoordinates::floorDiv(
        targetMinimumX - MaximumHorizontalRadius, cellSize);
    const int maximumCellX = WorldCoordinates::floorDiv(
        targetMaximumX + MaximumHorizontalRadius, cellSize);
    const int minimumCellZ = WorldCoordinates::floorDiv(
        targetMinimumZ - MaximumHorizontalRadius, cellSize);
    const int maximumCellZ = WorldCoordinates::floorDiv(
        targetMaximumZ + MaximumHorizontalRadius, cellSize);

    for (int cellX = minimumCellX; cellX <= maximumCellX; ++cellX) {
        for (int cellZ = minimumCellZ; cellZ <= maximumCellZ; ++cellZ) {
            const StructureType selected =
                selectedStructureTypeForCell(cellX, cellZ);
            StructurePlanSnapshot plan = planForCell(
                selected, cellX, cellZ);
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
