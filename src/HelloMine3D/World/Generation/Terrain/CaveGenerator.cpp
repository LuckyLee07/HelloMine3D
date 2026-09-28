#include "CaveGenerator.h"
#include "UndergroundPolish.h"

#include "../../../Item/ContainerInventory.h"
#include "../../../Item/Material.h"
#include "../../Block/BlockId.h"
#include "../../Block/ChestContainer.h"
#include "../../Chunk/Chunk.h"
#include "../../WorldCoordinates.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <tuple>

namespace {
constexpr int MinimumCaveY = 8;
constexpr int SurfaceBuffer = 5;
constexpr int WaterBuffer = 8;
constexpr int EntranceEdgeInset = 12;
constexpr int EntranceMinimumRelief = 16;
constexpr int EntranceHalfWidth = 1;
constexpr int EntranceHeight = 3;
constexpr int ChamberRadius = 3;
constexpr int AdventureMinimumY = 8;
constexpr int AdventureSurfaceBuffer = 5;
constexpr int AdventureChamberDistance = 29;
constexpr int AdventureRiftStartDistance = 6;
constexpr int AdventureDestinationDistance =
    AdventureRiftStartDistance + CaveGenerator::AdventureRiftLength - 1;
constexpr int AdventureRoomHeight = 7;
constexpr int AdventureInitialDepthMargin = 6;
constexpr std::size_t AdventureMaximumSafetyColumns = 408;

struct AdventureColumnRange {
    int minimumY = std::numeric_limits<int>::max();
    int maximumY = std::numeric_limits<int>::min();
};

template <typename Value>
class AdventureCoordinateTable {
  public:
    struct Entry {
        int x = 0;
        int z = 0;
        Value value{};
    };

    AdventureCoordinateTable() { clear(); }

    void clear() noexcept
    {
        m_size = 0;
        m_buckets.fill(-1);
    }

    std::size_t size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }

    Entry *begin() noexcept { return m_entries.data(); }
    Entry *end() noexcept { return m_entries.data() + m_size; }
    const Entry *begin() const noexcept { return m_entries.data(); }
    const Entry *end() const noexcept
    {
        return m_entries.data() + m_size;
    }

    Entry *find(int x, int z) noexcept
    {
        return const_cast<Entry *>(
            static_cast<const AdventureCoordinateTable &>(*this)
                .find(x, z));
    }

    const Entry *find(int x, int z) const noexcept
    {
        std::size_t bucket = bucketFor(x, z);
        for (std::size_t probe = 0; probe < BucketCount; ++probe) {
            const int index = m_buckets[bucket];
            if (index < 0) {
                return nullptr;
            }
            const Entry &entry =
                m_entries[static_cast<std::size_t>(index)];
            if (entry.x == x && entry.z == z) {
                return &entry;
            }
            bucket = (bucket + 1) & (BucketCount - 1);
        }
        return nullptr;
    }

    Entry *emplace(int x, int z) noexcept
    {
        std::size_t bucket = bucketFor(x, z);
        for (std::size_t probe = 0; probe < BucketCount; ++probe) {
            const int index = m_buckets[bucket];
            if (index < 0) {
                if (m_size >= AdventureMaximumSafetyColumns) {
                    return nullptr;
                }
                Entry &entry = m_entries[m_size];
                entry = Entry{};
                entry.x = x;
                entry.z = z;
                m_buckets[bucket] = static_cast<int>(m_size);
                ++m_size;
                return &entry;
            }
            Entry &entry =
                m_entries[static_cast<std::size_t>(index)];
            if (entry.x == x && entry.z == z) {
                return &entry;
            }
            bucket = (bucket + 1) & (BucketCount - 1);
        }
        return nullptr;
    }

  private:
    static constexpr std::size_t BucketCount = 1024;
    static_assert((BucketCount & (BucketCount - 1)) == 0,
                  "coordinate table buckets must be a power of two");

    static std::size_t bucketFor(int x, int z) noexcept
    {
        std::uint64_t value =
            (static_cast<std::uint64_t>(
                 static_cast<std::uint32_t>(x)) << 32) |
            static_cast<std::uint32_t>(z);
        value ^= value >> 30;
        value *= 0xbf58476d1ce4e5b9ull;
        value ^= value >> 27;
        value *= 0x94d049bb133111ebull;
        value ^= value >> 31;
        return static_cast<std::size_t>(value) & (BucketCount - 1);
    }

    std::array<Entry, AdventureMaximumSafetyColumns> m_entries{};
    std::array<int, BucketCount> m_buckets{};
    std::size_t m_size = 0;
};

using AdventureWriteColumns =
    AdventureCoordinateTable<AdventureColumnRange>;
using AdventureSurfaceSamples = AdventureCoordinateTable<int>;

struct AdventureHorizontalBounds {
    bool valid = false;
    int minimumX = 0;
    int maximumX = 0;
    int minimumZ = 0;
    int maximumZ = 0;
};

bool checkedInt(std::int64_t value, int &result) noexcept
{
    if (value < std::numeric_limits<int>::min() ||
        value > std::numeric_limits<int>::max()) {
        return false;
    }
    result = static_cast<int>(value);
    return true;
}

AdventureHorizontalBounds adventureCoreProjectionHorizontalBounds(
    const CaveGenerator::AdventureUndergroundPlan &plan) noexcept
{
    if (!plan.valid || !plan.entrance.valid) {
        return AdventureHorizontalBounds{};
    }

    // Excluding the legacy entrance's step 0..23 floor repairs, every write is
    // inside the chamber/destination endpoint box expanded by nine blocks:
    // the chamber's longest radius is nine, its pool reaches 5 + 4, and the
    // destination room and structures stay within five. The step 24..29
    // entrance connector lies within five blocks of the chamber; the rift and
    // its lead-in lie between chamber and destination. A disjoint chunk can
    // therefore only be changed by one of the explicitly checked early floor
    // repairs in the production fast path below.
    constexpr std::int64_t Padding =
        CaveGenerator::AdventureChamberAlongRadius;
    static_assert(
        CaveGenerator::AdventureChamberAlongRadius >=
            CaveGenerator::AdventureChamberPerpendicularRadius &&
        CaveGenerator::AdventureChamberAlongRadius >=
            CaveGenerator::AdventurePoolRadius + 5 &&
        CaveGenerator::AdventureChamberAlongRadius >=
            CaveGenerator::AdventureDestinationRadius,
        "projection bound padding must cover every horizontal write");
    const std::int64_t minimumX = std::min<std::int64_t>(
        plan.chamberX, plan.destinationX);
    const std::int64_t maximumX = std::max<std::int64_t>(
        plan.chamberX, plan.destinationX);
    const std::int64_t minimumZ = std::min<std::int64_t>(
        plan.chamberZ, plan.destinationZ);
    const std::int64_t maximumZ = std::max<std::int64_t>(
        plan.chamberZ, plan.destinationZ);

    AdventureHorizontalBounds bounds;
    bounds.valid =
        checkedInt(minimumX - Padding, bounds.minimumX) &&
        checkedInt(maximumX + Padding, bounds.maximumX) &&
        checkedInt(minimumZ - Padding, bounds.minimumZ) &&
        checkedInt(maximumZ + Padding, bounds.maximumZ);
    return bounds;
}

std::int64_t floorDiv64(std::int64_t value,
                        std::int64_t divisor) noexcept
{
    const std::int64_t quotient = value / divisor;
    const std::int64_t remainder = value % divisor;
    return remainder < 0 ? quotient - 1 : quotient;
}

bool addAdventureColumn(AdventureWriteColumns &columns,
                        std::int64_t worldX, std::int64_t worldZ,
                        std::int64_t minimumY,
                        std::int64_t maximumY)
{
    int x = 0;
    int z = 0;
    int minimum = 0;
    int maximum = 0;
    if (!checkedInt(worldX, x) || !checkedInt(worldZ, z) ||
        !checkedInt(minimumY, minimum) ||
        !checkedInt(maximumY, maximum) || minimum > maximum) {
        return false;
    }
    auto *entry = columns.emplace(x, z);
    if (entry == nullptr) {
        return false;
    }
    entry->value.minimumY = std::min(
        entry->value.minimumY, minimum);
    entry->value.maximumY = std::max(
        entry->value.maximumY, maximum);
    return true;
}

bool collectAdventureWriteColumns(
    const CaveGenerator::AdventureUndergroundPlan &plan,
    AdventureWriteColumns &columns)
{
    columns.clear();
    if (!plan.valid || !plan.entrance.valid) {
        return false;
    }
    const int entrancePerpendicularX = -plan.entrance.directionZ;
    const int entrancePerpendicularZ = plan.entrance.directionX;
    const int riftPerpendicularX = -plan.riftDirectionZ;
    const int riftPerpendicularZ = plan.riftDirectionX;
    const int riftDrop = plan.chamberAirY - plan.riftEndAirY;
    const auto add = [&columns](std::int64_t x, std::int64_t z,
                                std::int64_t minimumY,
                                std::int64_t maximumY) {
        return addAdventureColumn(
            columns, x, z, minimumY, maximumY);
    };

    for (int along = -CaveGenerator::AdventureChamberAlongRadius;
         along <= CaveGenerator::AdventureChamberAlongRadius; ++along) {
        for (int lateral =
                 -CaveGenerator::AdventureChamberPerpendicularRadius;
             lateral <=
                 CaveGenerator::AdventureChamberPerpendicularRadius;
             ++lateral) {
            const double horizontalNormalized =
                static_cast<double>(along * along) /
                    (CaveGenerator::AdventureChamberAlongRadius *
                     CaveGenerator::AdventureChamberAlongRadius) +
                static_cast<double>(lateral * lateral) /
                    (CaveGenerator::AdventureChamberPerpendicularRadius *
                     CaveGenerator::AdventureChamberPerpendicularRadius);
            int minimumVertical = std::numeric_limits<int>::max();
            int maximumVertical = std::numeric_limits<int>::min();
            for (int vertical =
                     -CaveGenerator::AdventureChamberVerticalRadius;
                 vertical <=
                     CaveGenerator::AdventureChamberVerticalRadius;
                 ++vertical) {
                const double normalized =
                    horizontalNormalized +
                    static_cast<double>(vertical * vertical) /
                        (CaveGenerator::AdventureChamberVerticalRadius *
                         CaveGenerator::AdventureChamberVerticalRadius);
                if (normalized > 1.0) {
                    continue;
                }
                minimumVertical = std::min(minimumVertical, vertical);
                maximumVertical = std::max(maximumVertical, vertical);
            }
            if (minimumVertical <= maximumVertical &&
                !add(static_cast<std::int64_t>(plan.chamberX) +
                         plan.entrance.directionX * along +
                         entrancePerpendicularX * lateral,
                     static_cast<std::int64_t>(plan.chamberZ) +
                         plan.entrance.directionZ * along +
                         entrancePerpendicularZ * lateral,
                     static_cast<std::int64_t>(plan.chamberAirY) + 3 +
                         minimumVertical,
                     static_cast<std::int64_t>(plan.chamberAirY) + 3 +
                         maximumVertical)) {
                return false;
            }
        }
    }

    for (int step = 0; step < CaveGenerator::AdventureRiftLength;
         ++step) {
        const int airY = plan.chamberAirY -
            riftDrop * step /
                (CaveGenerator::AdventureRiftLength - 1);
        const int distance = AdventureRiftStartDistance + step;
        for (int lateral = -CaveGenerator::AdventureRiftHalfWidth;
             lateral <= CaveGenerator::AdventureRiftHalfWidth;
             ++lateral) {
            const std::int64_t worldX =
                static_cast<std::int64_t>(plan.chamberX) +
                plan.riftDirectionX * distance +
                riftPerpendicularX * lateral;
            const std::int64_t worldZ =
                static_cast<std::int64_t>(plan.chamberZ) +
                plan.riftDirectionZ * distance +
                riftPerpendicularZ * lateral;
            if (!add(worldX, worldZ, airY - 1,
                     static_cast<std::int64_t>(airY) +
                         CaveGenerator::AdventureRiftHeight - 1)) {
                return false;
            }
        }
    }

    for (int along = -CaveGenerator::AdventureDestinationRadius;
         along <= CaveGenerator::AdventureDestinationRadius; ++along) {
        for (int lateral = -CaveGenerator::AdventureDestinationRadius;
             lateral <= CaveGenerator::AdventureDestinationRadius;
             ++lateral) {
            if (along * along + lateral * lateral >
                    CaveGenerator::AdventureDestinationRadius *
                        CaveGenerator::AdventureDestinationRadius) {
                continue;
            }
            const std::int64_t worldX =
                static_cast<std::int64_t>(plan.destinationX) +
                plan.riftDirectionX * along +
                riftPerpendicularX * lateral;
            const std::int64_t worldZ =
                static_cast<std::int64_t>(plan.destinationZ) +
                plan.riftDirectionZ * along +
                riftPerpendicularZ * lateral;
            if (!add(worldX, worldZ, plan.riftEndAirY - 1,
                     static_cast<std::int64_t>(plan.riftEndAirY) +
                         AdventureRoomHeight - 1)) {
                return false;
            }
        }
    }

    const std::int64_t poolX =
        static_cast<std::int64_t>(plan.chamberX) +
        plan.poolDirectionX * 5;
    const std::int64_t poolZ =
        static_cast<std::int64_t>(plan.chamberZ) +
        plan.poolDirectionZ * 5;
    for (int x = -CaveGenerator::AdventurePoolRadius;
         x <= CaveGenerator::AdventurePoolRadius; ++x) {
        for (int z = -CaveGenerator::AdventurePoolRadius;
             z <= CaveGenerator::AdventurePoolRadius; ++z) {
            if (x * x + z * z >
                    CaveGenerator::AdventurePoolRadius *
                        CaveGenerator::AdventurePoolRadius) {
                continue;
            }
            if (!add(poolX + x, poolZ + z,
                     plan.chamberAirY - 3,
                     plan.chamberAirY + 2)) {
                return false;
            }
        }
    }
    for (int x = -CaveGenerator::AdventurePoolRadius - 1;
         x <= CaveGenerator::AdventurePoolRadius + 1; ++x) {
        for (int z = -CaveGenerator::AdventurePoolRadius - 1;
             z <= CaveGenerator::AdventurePoolRadius + 1; ++z) {
            const int radiusSquared = x * x + z * z;
            if (radiusSquared <=
                    CaveGenerator::AdventurePoolRadius *
                        CaveGenerator::AdventurePoolRadius ||
                radiusSquared >
                    (CaveGenerator::AdventurePoolRadius + 1) *
                        (CaveGenerator::AdventurePoolRadius + 1)) {
                continue;
            }
            if (!add(poolX + x, poolZ + z,
                     plan.chamberAirY - 2,
                     plan.chamberAirY - 2)) {
                return false;
            }
        }
    }

    int outcropDirectionX = -plan.poolDirectionX;
    int outcropDirectionZ = -plan.poolDirectionZ;
    if (outcropDirectionX == plan.riftDirectionX &&
        outcropDirectionZ == plan.riftDirectionZ) {
        outcropDirectionX = plan.entrance.directionX;
        outcropDirectionZ = plan.entrance.directionZ;
    }
    const bool alongChamber =
        std::abs(outcropDirectionX) ==
            std::abs(plan.entrance.directionX) &&
        std::abs(outcropDirectionZ) ==
            std::abs(plan.entrance.directionZ);
    const int outcropDistance = alongChamber
        ? CaveGenerator::AdventureChamberAlongRadius
        : CaveGenerator::AdventureChamberPerpendicularRadius;
    if (!add(static_cast<std::int64_t>(plan.chamberX) +
                 outcropDirectionX * outcropDistance,
             static_cast<std::int64_t>(plan.chamberZ) +
                 outcropDirectionZ * outcropDistance,
             plan.chamberAirY + 2, plan.chamberAirY + 3)) {
        return false;
    }

    if (plan.layout ==
            CaveGenerator::AdventureUndergroundLayout::MossCellar) {
        int resourceIndex = 0;
        for (int level = 1; level <= 2; ++level) {
            for (int along = -2; along <= 2; ++along) {
                if (resourceIndex++ >= 9) {
                    continue;
                }
                if (!add(
                        static_cast<std::int64_t>(plan.destinationX) +
                            plan.riftDirectionX * along +
                            riftPerpendicularX * 5,
                        static_cast<std::int64_t>(plan.destinationZ) +
                            plan.riftDirectionZ * along +
                            riftPerpendicularZ * 5,
                        static_cast<std::int64_t>(plan.riftEndAirY) +
                            level,
                        static_cast<std::int64_t>(plan.riftEndAirY) +
                            level)) {
                    return false;
                }
            }
        }
    }

    // The upper natural entrance is generated by the unchanged legacy pass.
    // Version 23 starts writing at its old terminal chamber, then connects the
    // new chamber, rift and destination without touching exposed terrain.
    for (int step = CaveGenerator::EntranceTunnelLength;
         step <= AdventureChamberDistance; ++step) {
        for (int lateral = -1; lateral <= 1; ++lateral) {
            if (!add(
                    static_cast<std::int64_t>(plan.entrance.anchorX) +
                        plan.entrance.directionX * step +
                        entrancePerpendicularX * lateral,
                    static_cast<std::int64_t>(plan.entrance.anchorZ) +
                        plan.entrance.directionZ * step +
                        entrancePerpendicularZ * lateral,
                    plan.chamberAirY - 1, plan.chamberAirY + 2)) {
                return false;
            }
        }
    }
    for (int step = 0; step < AdventureRiftStartDistance; ++step) {
        for (int lateral = -1; lateral <= 1; ++lateral) {
            if (!add(
                    static_cast<std::int64_t>(plan.chamberX) +
                        plan.riftDirectionX * step +
                        riftPerpendicularX * lateral,
                    static_cast<std::int64_t>(plan.chamberZ) +
                        plan.riftDirectionZ * step +
                        riftPerpendicularZ * lateral,
                    plan.chamberAirY - 1, plan.chamberAirY + 2)) {
                return false;
            }
        }
    }
    return true;
}

bool adventureColumnsIntersectHorizontalBounds(
    const AdventureWriteColumns &columns,
    const AdventureHorizontalBounds &bounds) noexcept
{
    if (!bounds.valid) {
        return true;
    }
    for (const auto &entry : columns) {
        if (entry.x >= bounds.minimumX && entry.x <= bounds.maximumX &&
            entry.z >= bounds.minimumZ && entry.z <= bounds.maximumZ) {
            return true;
        }
    }
    return false;
}

bool collectLegacyEntranceCarveColumns(
    const CaveGenerator::NaturalEntrance &entrance,
    AdventureWriteColumns &columns)
{
    columns.clear();
    if (!entrance.valid) {
        return false;
    }
    const int perpendicularX = -entrance.directionZ;
    const int perpendicularZ = entrance.directionX;
    for (int step = 0;
         step <= CaveGenerator::EntranceTunnelLength; ++step) {
        const int airY = entrance.anchorY - step * 3 / 4;
        for (int lateral = -1; lateral <= 1; ++lateral) {
            if (!addAdventureColumn(
                    columns,
                    static_cast<std::int64_t>(entrance.anchorX) +
                        entrance.directionX * step +
                        perpendicularX * lateral,
                    static_cast<std::int64_t>(entrance.anchorZ) +
                        entrance.directionZ * step +
                        perpendicularZ * lateral,
                    static_cast<std::int64_t>(airY) - 1,
                    static_cast<std::int64_t>(airY) + 2)) {
                return false;
            }
        }
    }
    const std::int64_t endX =
        static_cast<std::int64_t>(entrance.anchorX) +
        entrance.directionX * CaveGenerator::EntranceTunnelLength;
    const std::int64_t endZ =
        static_cast<std::int64_t>(entrance.anchorZ) +
        entrance.directionZ * CaveGenerator::EntranceTunnelLength;
    for (int offsetX = -ChamberRadius;
         offsetX <= ChamberRadius; ++offsetX) {
        for (int offsetZ = -ChamberRadius;
             offsetZ <= ChamberRadius; ++offsetZ) {
            if (offsetX * offsetX + offsetZ * offsetZ >
                    ChamberRadius * ChamberRadius) {
                continue;
            }
            if (!addAdventureColumn(
                    columns, endX + offsetX, endZ + offsetZ,
                    entrance.endY - 1,
                    static_cast<std::int64_t>(entrance.endY) + 3)) {
                return false;
            }
        }
    }
    return true;
}

AdventureHorizontalBounds adventureHorizontalBounds(
    const AdventureWriteColumns &columns) noexcept
{
    AdventureHorizontalBounds bounds;
    for (const auto &entry : columns) {
        const int x = entry.x;
        const int z = entry.z;
        if (!bounds.valid) {
            bounds = {true, x, x, z, z};
            continue;
        }
        bounds.minimumX = std::min(bounds.minimumX, x);
        bounds.maximumX = std::max(bounds.maximumX, x);
        bounds.minimumZ = std::min(bounds.minimumZ, z);
        bounds.maximumZ = std::max(bounds.maximumZ, z);
    }
    return bounds;
}

AdventureHorizontalBounds adventurePotentialHorizontalBounds(
    const CaveGenerator::AdventureUndergroundPlan &plan,
    const AdventureWriteColumns &coreColumns)
{
    AdventureHorizontalBounds bounds =
        adventureHorizontalBounds(coreColumns);
    const int perpendicularX = -plan.entrance.directionZ;
    const int perpendicularZ = plan.entrance.directionX;
    for (int step = 0;
         step < CaveGenerator::EntranceTunnelLength; ++step) {
        for (int lateral = -1; lateral <= 1; ++lateral) {
            int worldX = 0;
            int worldZ = 0;
            if (!checkedInt(
                    static_cast<std::int64_t>(plan.entrance.anchorX) +
                        plan.entrance.directionX * step +
                        perpendicularX * lateral,
                    worldX) ||
                !checkedInt(
                    static_cast<std::int64_t>(plan.entrance.anchorZ) +
                        plan.entrance.directionZ * step +
                        perpendicularZ * lateral,
                    worldZ)) {
                return AdventureHorizontalBounds{};
            }
            if (!bounds.valid) {
                bounds = {true, worldX, worldX, worldZ, worldZ};
            }
            else {
                bounds.minimumX = std::min(bounds.minimumX, worldX);
                bounds.maximumX = std::max(bounds.maximumX, worldX);
                bounds.minimumZ = std::min(bounds.minimumZ, worldZ);
                bounds.maximumZ = std::max(bounds.maximumZ, worldZ);
            }
        }
    }
    return bounds;
}

bool overlaps(const AdventureHorizontalBounds &left,
              const AdventureHorizontalBounds &right) noexcept
{
    return left.valid && right.valid &&
        left.minimumX <= right.maximumX &&
        right.minimumX <= left.maximumX &&
        left.minimumZ <= right.maximumZ &&
        right.minimumZ <= left.maximumZ;
}

bool columnRangesOverlap(const AdventureWriteColumns &left,
                         const AdventureWriteColumns &right) noexcept
{
    const AdventureWriteColumns *smaller = &left;
    const AdventureWriteColumns *larger = &right;
    if (smaller->size() > larger->size()) {
        std::swap(smaller, larger);
    }
    for (const auto &entry : *smaller) {
        const auto *other = larger->find(entry.x, entry.z);
        if (other == nullptr) {
            continue;
        }
        if (entry.value.minimumY <= other->value.maximumY &&
            other->value.minimumY <= entry.value.maximumY) {
            return true;
        }
    }
    return false;
}

bool hasAdventureSurfaceClearance(
    const AdventureWriteColumns &columns,
    const CaveGenerator::SurfaceHeightSampler &surfaceHeight,
    AdventureSurfaceSamples &surfaceSamples)
{
    if (columns.empty() ||
        columns.size() > AdventureMaximumSafetyColumns) {
        return false;
    }
    for (const auto &entry : columns) {
        const AdventureColumnRange &range = entry.value;
        auto *sampled = surfaceSamples.find(entry.x, entry.z);
        if (sampled == nullptr) {
            sampled = surfaceSamples.emplace(entry.x, entry.z);
            if (sampled == nullptr) {
                return false;
            }
            sampled->value = surfaceHeight(entry.x, entry.z);
        }
        if (range.minimumY < AdventureMinimumY ||
            range.maximumY >= 256 ||
            static_cast<std::int64_t>(range.maximumY) >
                static_cast<std::int64_t>(sampled->value) -
                    AdventureSurfaceBuffer) {
            return false;
        }
    }
    return true;
}

bool adventurePlanPrecedes(
    const CaveGenerator::AdventureUndergroundPlan &left,
    const CaveGenerator::AdventureUndergroundPlan &right) noexcept
{
    return std::tie(left.stableKey, left.entrance.cellX,
                    left.entrance.cellZ) >
        std::tie(right.stableKey, right.entrance.cellX,
                 right.entrance.cellZ);
}

bool addAdventureEntranceFloorRepairs(
    const CaveGenerator::AdventureUndergroundPlan &plan,
    const CaveGenerator::SurfaceHeightSampler &surfaceHeight,
    AdventureWriteColumns &columns,
    AdventureSurfaceSamples &surfaceSamples)
{
    const int perpendicularX = -plan.entrance.directionZ;
    const int perpendicularZ = plan.entrance.directionX;
    for (int step = 0;
         step < CaveGenerator::EntranceTunnelLength; ++step) {
        const int floorY = plan.entrance.anchorY - step * 3 / 4 - 1;
        for (int lateral = -1; lateral <= 1; ++lateral) {
            int worldX = 0;
            int worldZ = 0;
            if (!checkedInt(
                    static_cast<std::int64_t>(plan.entrance.anchorX) +
                        plan.entrance.directionX * step +
                        perpendicularX * lateral,
                    worldX) ||
                !checkedInt(
                    static_cast<std::int64_t>(plan.entrance.anchorZ) +
                        plan.entrance.directionZ * step +
                        perpendicularZ * lateral,
                    worldZ)) {
                return false;
            }
            auto *sampled = surfaceSamples.find(worldX, worldZ);
            if (sampled == nullptr) {
                sampled = surfaceSamples.emplace(worldX, worldZ);
                if (sampled == nullptr) {
                    return false;
                }
                sampled->value = surfaceHeight(worldX, worldZ);
            }
            const std::int64_t surface = sampled->value;
            if (floorY > surface) {
                return false;
            }
            // Noise caves only reach surface-5, but the legacy terminal
            // chamber can remove the last approach floors independently.
            const int chamberAlong =
                CaveGenerator::EntranceTunnelLength - step;
            const bool terminalChamberCarvesFloor =
                chamberAlong * chamberAlong + lateral * lateral <=
                    ChamberRadius * ChamberRadius &&
                floorY >= plan.entrance.endY - 1 &&
                floorY <= plan.entrance.endY + 3;
            if ((terminalChamberCarvesFloor ||
                 static_cast<std::int64_t>(floorY) <=
                     surface - AdventureSurfaceBuffer) &&
                !addAdventureColumn(
                    columns, worldX, worldZ, floorY, floorY)) {
                return false;
            }
        }
    }
    return true;
}

bool certifyAdventureSurfaceSafety(
    const CaveGenerator::AdventureUndergroundPlan &plan,
    const CaveGenerator::SurfaceHeightSampler &surfaceHeight,
    AdventureWriteColumns &columns)
{
    if (!plan.valid || !surfaceHeight) {
        return false;
    }
    AdventureSurfaceSamples surfaceSamples;
    return addAdventureEntranceFloorRepairs(
            plan, surfaceHeight, columns, surfaceSamples) &&
        hasAdventureSurfaceClearance(
            columns, surfaceHeight, surfaceSamples);
}

AdventureHorizontalBounds legacyEntrancePotentialBoundsForCell(
    int cellX, int cellZ) noexcept
{
    constexpr std::int64_t MinimumOffset =
        EntranceEdgeInset -
        CaveGenerator::EntranceTunnelLength - ChamberRadius;
    constexpr std::int64_t MaximumOffset =
        CaveGenerator::EntranceCellBlocks - EntranceEdgeInset - 1 +
        CaveGenerator::EntranceTunnelLength + ChamberRadius;
    int minimumX = 0;
    int maximumX = 0;
    int minimumZ = 0;
    int maximumZ = 0;
    if (!checkedInt(
            static_cast<std::int64_t>(cellX) *
                    CaveGenerator::EntranceCellBlocks + MinimumOffset,
            minimumX) ||
        !checkedInt(
            static_cast<std::int64_t>(cellX) *
                    CaveGenerator::EntranceCellBlocks + MaximumOffset,
            maximumX) ||
        !checkedInt(
            static_cast<std::int64_t>(cellZ) *
                    CaveGenerator::EntranceCellBlocks + MinimumOffset,
            minimumZ) ||
        !checkedInt(
            static_cast<std::int64_t>(cellZ) *
                    CaveGenerator::EntranceCellBlocks + MaximumOffset,
            maximumZ)) {
        return AdventureHorizontalBounds{};
    }
    return {true, minimumX, maximumX, minimumZ, maximumZ};
}

template <typename EntranceLookup>
bool intersectsForeignLegacyEntrance(
    int cellX, int cellZ,
    const CaveGenerator::AdventureUndergroundPlan &plan,
    const AdventureWriteColumns &columns,
    EntranceLookup &entranceForCell)
{
    const AdventureHorizontalBounds bounds =
        adventurePotentialHorizontalBounds(plan, columns);
    if (!bounds.valid) {
        return true;
    }
    for (int offsetX = -2; offsetX <= 2; ++offsetX) {
        for (int offsetZ = -2; offsetZ <= 2; ++offsetZ) {
            if (offsetX == 0 && offsetZ == 0) {
                continue;
            }
            int otherCellX = 0;
            int otherCellZ = 0;
            if (!checkedInt(static_cast<std::int64_t>(cellX) + offsetX,
                            otherCellX) ||
                !checkedInt(static_cast<std::int64_t>(cellZ) + offsetZ,
                            otherCellZ)) {
                continue;
            }
            if (!overlaps(
                    bounds,
                    legacyEntrancePotentialBoundsForCell(
                        otherCellX, otherCellZ))) {
                continue;
            }
            const CaveGenerator::NaturalEntrance otherEntrance =
                entranceForCell(otherCellX, otherCellZ);
            if (!otherEntrance.valid) {
                continue;
            }
            AdventureWriteColumns otherOldEntranceColumns;
            if (!collectLegacyEntranceCarveColumns(
                    otherEntrance, otherOldEntranceColumns)) {
                return true;
            }
            if (!overlaps(
                    bounds,
                    adventureHorizontalBounds(
                        otherOldEntranceColumns))) {
                continue;
            }
            if (columnRangesOverlap(
                    columns, otherOldEntranceColumns)) {
                return true;
            }

            // The legacy entrance pass may remove an early tunnel floor even
            // when that floor is above the ordinary noise-cave surface
            // buffer. Projection repairs every such Air floor, so reject the
            // whole v23 plan rather than silently filling another v22
            // entrance with Cobblestone outside the certified write set.
            const int perpendicularX = -plan.entrance.directionZ;
            const int perpendicularZ = plan.entrance.directionX;
            for (int step = 0;
                 step < CaveGenerator::EntranceTunnelLength; ++step) {
                const int floorY =
                    plan.entrance.anchorY - step * 3 / 4 - 1;
                for (int lateral = -1; lateral <= 1; ++lateral) {
                    int worldX = 0;
                    int worldZ = 0;
                    if (!checkedInt(
                            static_cast<std::int64_t>(
                                plan.entrance.anchorX) +
                                plan.entrance.directionX * step +
                                perpendicularX * lateral,
                            worldX) ||
                        !checkedInt(
                            static_cast<std::int64_t>(
                                plan.entrance.anchorZ) +
                                plan.entrance.directionZ * step +
                                perpendicularZ * lateral,
                            worldZ)) {
                        return true;
                    }
                    const auto *legacy =
                        otherOldEntranceColumns.find(worldX, worldZ);
                    if (legacy != nullptr &&
                        floorY >= legacy->value.minimumY &&
                        floorY <= legacy->value.maximumY) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

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
    int chunkMinimumX = 0;
    int chunkMinimumZ = 0;
    int chunkMaximumX = 0;
    int chunkMaximumZ = 0;
    if (!checkedInt(static_cast<std::int64_t>(chunkLocation.x) *
                        CHUNK_SIZE,
                    chunkMinimumX) ||
        !checkedInt(static_cast<std::int64_t>(chunkLocation.y) *
                        CHUNK_SIZE,
                    chunkMinimumZ) ||
        !checkedInt(static_cast<std::int64_t>(chunkLocation.x) *
                            CHUNK_SIZE + CHUNK_SIZE - 1,
                    chunkMaximumX) ||
        !checkedInt(static_cast<std::int64_t>(chunkLocation.y) *
                            CHUNK_SIZE + CHUNK_SIZE - 1,
                    chunkMaximumZ)) {
        return 0;
    }
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        const int worldX = chunkMinimumX + x;
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            const int worldZ = chunkMinimumZ + z;
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
    return getNaturalEntranceForCellWithPrefilter(
        cellX, cellZ, surfaceHeight, biome,
        EntranceCandidatePrefilter{});
}

CaveGenerator::NaturalEntrance
CaveGenerator::getNaturalEntranceForCellWithPrefilter(
    int cellX, int cellZ,
    const SurfaceHeightSampler &surfaceHeight,
    const BiomeSampler &biome,
    const EntranceCandidatePrefilter &candidatePrefilter) const
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
        int worldX = 0;
        int worldZ = 0;
        if (!checkedInt(
                static_cast<std::int64_t>(cellX) *
                        EntranceCellBlocks +
                    EntranceEdgeInset +
                    static_cast<int>(hash % CandidateSpan),
                worldX) ||
            !checkedInt(
                static_cast<std::int64_t>(cellZ) *
                        EntranceCellBlocks +
                    EntranceEdgeInset + static_cast<int>(
                        (hash >> 16) % CandidateSpan),
                worldZ)) {
            continue;
        }
        if (candidatePrefilter &&
            !candidatePrefilter(worldX, worldZ)) {
            continue;
        }
        const int surface = surfaceHeight(worldX, worldZ);
        if (biome(worldX, worldZ) != TerrainBiome::Mountain ||
            surface < WATER_LEVEL + EntranceMinimumRelief) {
            continue;
        }
        int directionIndex = static_cast<int>((hash >> 32) % 4);
        std::array<int, 4> targetXs{};
        std::array<int, 4> targetZs{};
        bool targetsInRange = true;
        for (int index = 0; index < 4; ++index) {
            targetsInRange = targetsInRange && checkedInt(
                static_cast<std::int64_t>(worldX) +
                    directions[index][0] * EntranceTunnelLength,
                targetXs[index]);
            targetsInRange = targetsInRange && checkedInt(
                static_cast<std::int64_t>(worldZ) +
                    directions[index][1] * EntranceTunnelLength,
                targetZs[index]);
        }
        if (!targetsInRange) {
            continue;
        }
        const int initialDirectionIndex = directionIndex;
        int greatestCover = surfaceHeight(
            targetXs[directionIndex], targetZs[directionIndex]);
        for (int index = 0; index < 4; ++index) {
            if (index == initialDirectionIndex) {
                continue;
            }
            const int cover = surfaceHeight(
                targetXs[index], targetZs[index]);
            if (cover > greatestCover) {
                directionIndex = index;
                greatestCover = cover;
            }
        }
        if (static_cast<std::int64_t>(greatestCover) <
                static_cast<std::int64_t>(surface) + 2) {
            continue;
        }
        const int targetX = targetXs[directionIndex];
        const int targetZ = targetZs[directionIndex];
        if (biome(targetX, targetZ) != TerrainBiome::Mountain) {
            continue;
        }

        entrance.valid = true;
        entrance.anchorX = worldX;
        entrance.anchorY = surface;
        entrance.anchorZ = worldZ;
        entrance.directionX = directions[directionIndex][0];
        entrance.directionZ = directions[directionIndex][1];
        if (!checkedInt(
                static_cast<std::int64_t>(surface) -
                    EntranceTunnelLength * 3 / 4,
                entrance.endY)) {
            return NaturalEntrance{};
        }
        return entrance;
    }
    return entrance;
}

CaveGenerator::AdventureUndergroundPlan
CaveGenerator::getRawAdventureUndergroundPlanForCell(
    int cellX, int cellZ,
    const NaturalEntrance &entrance,
    const SurfaceHeightSampler &surfaceHeight) const
{
    AdventureUndergroundPlan plan;
    if (m_generationVersion <
            AdventureUndergroundTerrainGenerationVersion ||
        !surfaceHeight) {
        return plan;
    }

    plan.entrance = entrance;
    if (!plan.entrance.valid) {
        return plan;
    }

    plan.stableKey = entranceHash(m_seed, cellX, cellZ, 73);
    plan.layout = (plan.stableKey & 1ull) == 0
        ? AdventureUndergroundLayout::MinerCache
        : AdventureUndergroundLayout::MossCellar;
    if (!checkedInt(
            static_cast<std::int64_t>(plan.entrance.anchorX) +
                plan.entrance.directionX * AdventureChamberDistance,
            plan.chamberX) ||
        !checkedInt(
            static_cast<std::int64_t>(plan.entrance.anchorZ) +
                plan.entrance.directionZ * AdventureChamberDistance,
            plan.chamberZ)) {
        return AdventureUndergroundPlan{};
    }
    plan.chamberAirY = plan.entrance.endY;

    const int leftX = -plan.entrance.directionZ;
    const int leftZ = plan.entrance.directionX;
    std::array<std::array<int, 2>, 3> directions{{
        {{plan.entrance.directionX, plan.entrance.directionZ}},
        {{leftX, leftZ}},
        {{-leftX, -leftZ}},
    }};
    if ((plan.stableKey & 2ull) != 0) {
        std::swap(directions[1], directions[2]);
    }

    int bestCover = std::numeric_limits<int>::min();
    for (const auto &direction : directions) {
        int destinationX = 0;
        int destinationZ = 0;
        if (!checkedInt(
                static_cast<std::int64_t>(plan.chamberX) +
                    direction[0] * AdventureDestinationDistance,
                destinationX) ||
            !checkedInt(
                static_cast<std::int64_t>(plan.chamberZ) +
                    direction[1] * AdventureDestinationDistance,
                destinationZ)) {
            return AdventureUndergroundPlan{};
        }
        const int cover = surfaceHeight(destinationX, destinationZ);
        if (cover > bestCover) {
            bestCover = cover;
            plan.riftDirectionX = direction[0];
            plan.riftDirectionZ = direction[1];
        }
    }

    if (!checkedInt(
            static_cast<std::int64_t>(plan.chamberX) +
                plan.riftDirectionX * AdventureDestinationDistance,
            plan.destinationX) ||
        !checkedInt(
            static_cast<std::int64_t>(plan.chamberZ) +
                plan.riftDirectionZ * AdventureDestinationDistance,
            plan.destinationZ)) {
        return AdventureUndergroundPlan{};
    }

    if (plan.riftDirectionX == plan.entrance.directionX &&
        plan.riftDirectionZ == plan.entrance.directionZ) {
        const int side = (plan.stableKey & 4ull) == 0 ? 1 : -1;
        plan.poolDirectionX = leftX * side;
        plan.poolDirectionZ = leftZ * side;
    }
    else {
        plan.poolDirectionX = -plan.riftDirectionX;
        plan.poolDirectionZ = -plan.riftDirectionZ;
    }
    const std::int64_t desiredRiftEnd = std::max<std::int64_t>(
        AdventureMinimumY + 1,
        std::min<std::int64_t>(
            plan.chamberAirY,
            static_cast<std::int64_t>(bestCover) -
                AdventureSurfaceBuffer - AdventureRiftHeight -
                AdventureInitialDepthMargin));
    const std::int64_t minimumRiftEnd =
        static_cast<std::int64_t>(plan.chamberAirY) -
        (AdventureRiftLength - 1);
    if (!checkedInt(std::max(desiredRiftEnd, minimumRiftEnd),
                    plan.riftEndAirY) ||
        plan.riftEndAirY < AdventureMinimumY + 1 ||
        plan.chamberAirY - plan.riftEndAirY >= AdventureRiftLength) {
        return AdventureUndergroundPlan{};
    }
    plan.valid = true;
    return plan;
}

CaveGenerator::AdventureUndergroundPlan
CaveGenerator::getAdventureUndergroundPlanForCell(
    int cellX, int cellZ,
    const SurfaceHeightSampler &surfaceHeight,
    const BiomeSampler &biome) const
{
    return getAdventureUndergroundPlanForCellWithPrefilter(
        cellX, cellZ, surfaceHeight, biome,
        EntranceCandidatePrefilter{}, false);
}

CaveGenerator::AdventureUndergroundPlan
CaveGenerator::getAdventureUndergroundPlanForCellWithPrefilter(
    int cellX, int cellZ,
    const SurfaceHeightSampler &surfaceHeight,
    const BiomeSampler &biome,
    const EntranceCandidatePrefilter &candidatePrefilter,
    bool usePlanningCache) const
{
    const NaturalEntrance targetEntrance =
        usePlanningCache
        ? getCachedNaturalEntranceForCell(
              cellX, cellZ, surfaceHeight, biome,
              candidatePrefilter)
        : getNaturalEntranceForCellWithPrefilter(
              cellX, cellZ, surfaceHeight, biome,
              candidatePrefilter);
    AdventureUndergroundPlan plan =
        usePlanningCache
        ? getCachedRawAdventureUndergroundPlanForCell(
              cellX, cellZ, surfaceHeight, biome,
              candidatePrefilter)
        : getRawAdventureUndergroundPlanForCell(
              cellX, cellZ, targetEntrance, surfaceHeight);
    if (!plan.valid) {
        return AdventureUndergroundPlan{};
    }

    // A single plan decision can revisit the same legacy entrance while it
    // checks the target, priority neighbours and each neighbour's legacy
    // conflicts. Keep those deterministic samples local to this call: the
    // samplers may be different on the next public invocation.
    constexpr int EntranceCacheRadius = 4;
    constexpr int EntranceCacheDiameter = EntranceCacheRadius * 2 + 1;
    constexpr int EntranceCacheSize =
        EntranceCacheDiameter * EntranceCacheDiameter;
    std::array<NaturalEntrance, EntranceCacheSize> entranceCache{};
    std::array<bool, EntranceCacheSize> entranceCacheOccupied{};
    constexpr std::size_t TargetEntranceCacheIndex =
        EntranceCacheRadius * EntranceCacheDiameter +
        EntranceCacheRadius;
    entranceCache[TargetEntranceCacheIndex] = targetEntrance;
    entranceCacheOccupied[TargetEntranceCacheIndex] = true;
    const auto entranceForCell =
        [&](int requestedCellX, int requestedCellZ) {
            const std::int64_t offsetX =
                static_cast<std::int64_t>(requestedCellX) - cellX;
            const std::int64_t offsetZ =
                static_cast<std::int64_t>(requestedCellZ) - cellZ;
            if (offsetX >= -EntranceCacheRadius &&
                offsetX <= EntranceCacheRadius &&
                offsetZ >= -EntranceCacheRadius &&
                offsetZ <= EntranceCacheRadius) {
                const std::size_t index = static_cast<std::size_t>(
                    (offsetZ + EntranceCacheRadius) *
                        EntranceCacheDiameter +
                    offsetX + EntranceCacheRadius);
                if (!entranceCacheOccupied[index]) {
                    entranceCache[index] =
                        usePlanningCache
                        ? getCachedNaturalEntranceForCell(
                              requestedCellX, requestedCellZ,
                              surfaceHeight, biome,
                              candidatePrefilter)
                        : getNaturalEntranceForCellWithPrefilter(
                              requestedCellX, requestedCellZ,
                              surfaceHeight, biome,
                              candidatePrefilter);
                    entranceCacheOccupied[index] = true;
                }
                return entranceCache[index];
            }
            return usePlanningCache
                ? getCachedNaturalEntranceForCell(
                      requestedCellX, requestedCellZ,
                      surfaceHeight, biome,
                      candidatePrefilter)
                : getNaturalEntranceForCellWithPrefilter(
                      requestedCellX, requestedCellZ,
                      surfaceHeight, biome,
                      candidatePrefilter);
    };

    AdventureWriteColumns columns;
    if (!collectAdventureWriteColumns(plan, columns) ||
        !certifyAdventureSurfaceSafety(
            plan, surfaceHeight, columns) ||
        intersectsForeignLegacyEntrance(
            cellX, cellZ, plan, columns, entranceForCell)) {
        return AdventureUndergroundPlan{};
    }
    const AdventureHorizontalBounds bounds =
        adventureHorizontalBounds(columns);

    for (int offsetX = -2; offsetX <= 2; ++offsetX) {
        for (int offsetZ = -2; offsetZ <= 2; ++offsetZ) {
            if (offsetX == 0 && offsetZ == 0) {
                continue;
            }
            int otherCellX = 0;
            int otherCellZ = 0;
            if (!checkedInt(static_cast<std::int64_t>(cellX) + offsetX,
                            otherCellX) ||
                !checkedInt(static_cast<std::int64_t>(cellZ) + offsetZ,
                            otherCellZ)) {
                continue;
            }

            AdventureUndergroundPlan otherPriority;
            otherPriority.entrance.cellX = otherCellX;
            otherPriority.entrance.cellZ = otherCellZ;
            otherPriority.stableKey = entranceHash(
                m_seed, otherCellX, otherCellZ, 73);
            if (adventurePlanPrecedes(plan, otherPriority)) {
                continue;
            }

            AdventureUndergroundPlan other = usePlanningCache
                ? getCachedRawAdventureUndergroundPlanForCell(
                      otherCellX, otherCellZ, surfaceHeight, biome,
                      candidatePrefilter)
                : getRawAdventureUndergroundPlanForCell(
                      otherCellX, otherCellZ,
                      entranceForCell(otherCellX, otherCellZ),
                      surfaceHeight);
            if (!other.valid ||
                !adventurePlanPrecedes(other, plan)) {
                continue;
            }
            AdventureWriteColumns rawOtherColumns;
            if (!collectAdventureWriteColumns(
                    other, rawOtherColumns) ||
                !overlaps(
                    bounds,
                    adventurePotentialHorizontalBounds(
                        other, rawOtherColumns))) {
                continue;
            }

            if (!certifyAdventureSurfaceSafety(
                    other, surfaceHeight, rawOtherColumns) ||
                !columnRangesOverlap(columns, rawOtherColumns)) {
                continue;
            }
            if (intersectsForeignLegacyEntrance(
                    otherCellX, otherCellZ, other,
                    rawOtherColumns, entranceForCell)) {
                continue;
            }
            return AdventureUndergroundPlan{};
        }
    }
    return plan;
}

CaveGenerator::AdventureUndergroundPlan
CaveGenerator::getCachedRawAdventureUndergroundPlanForCell(
    int cellX, int cellZ,
    const SurfaceHeightSampler &surfaceHeight,
    const BiomeSampler &biome,
    const EntranceCandidatePrefilter &candidatePrefilter) const
{
    const std::size_t index = static_cast<std::size_t>(
        entranceHash(m_seed, cellX, cellZ, 97) %
        AdventurePlanCacheCapacity);
    AdventurePlanCacheEntry &entry = m_adventurePlanCache[index];
    if (entry.occupied && entry.cellX == cellX &&
        entry.cellZ == cellZ && entry.hasRawPlan) {
        return entry.rawPlan;
    }

    const NaturalEntrance entrance = getCachedNaturalEntranceForCell(
        cellX, cellZ, surfaceHeight, biome, candidatePrefilter);
    const AdventureUndergroundPlan plan =
        getRawAdventureUndergroundPlanForCell(
            cellX, cellZ, entrance, surfaceHeight);

    // Reacquire the direct-mapped slot after nested lookups: a colliding
    // neighbour is allowed to replace it while this plan is calculated.
    AdventurePlanCacheEntry &destination = m_adventurePlanCache[index];
    if (!destination.occupied || destination.cellX != cellX ||
        destination.cellZ != cellZ) {
        destination = AdventurePlanCacheEntry{};
        destination.occupied = true;
        destination.cellX = cellX;
        destination.cellZ = cellZ;
    }
    destination.hasEntrance = true;
    destination.entrance = entrance;
    destination.hasRawPlan = true;
    destination.rawPlan = plan;
    return plan;
}

CaveGenerator::NaturalEntrance
CaveGenerator::getCachedNaturalEntranceForCell(
    int cellX, int cellZ,
    const SurfaceHeightSampler &surfaceHeight,
    const BiomeSampler &biome,
    const EntranceCandidatePrefilter &candidatePrefilter) const
{
    const std::size_t index = static_cast<std::size_t>(
        entranceHash(m_seed, cellX, cellZ, 97) %
        AdventurePlanCacheCapacity);
    AdventurePlanCacheEntry &entry = m_adventurePlanCache[index];
    if (entry.occupied && entry.cellX == cellX &&
        entry.cellZ == cellZ && entry.hasEntrance) {
        return entry.entrance;
    }

    const NaturalEntrance entrance =
        getNaturalEntranceForCellWithPrefilter(
            cellX, cellZ, surfaceHeight, biome,
            candidatePrefilter);
    AdventurePlanCacheEntry &destination = m_adventurePlanCache[index];
    if (!destination.occupied || destination.cellX != cellX ||
        destination.cellZ != cellZ) {
        destination = AdventurePlanCacheEntry{};
        destination.occupied = true;
        destination.cellX = cellX;
        destination.cellZ = cellZ;
    }
    destination.hasEntrance = true;
    destination.entrance = entrance;
    return entrance;
}

CaveGenerator::AdventureUndergroundPlan
CaveGenerator::getCachedAdventureUndergroundPlanForCell(
    int cellX, int cellZ,
    const SurfaceHeightSampler &surfaceHeight,
    const BiomeSampler &biome,
    const EntranceCandidatePrefilter &candidatePrefilter) const
{
    const std::size_t index = static_cast<std::size_t>(
        entranceHash(m_seed, cellX, cellZ, 97) %
        AdventurePlanCacheCapacity);
    AdventurePlanCacheEntry &entry = m_adventurePlanCache[index];
    if (entry.occupied && entry.cellX == cellX &&
        entry.cellZ == cellZ && entry.hasFinalPlan) {
        return entry.plan;
    }
    AdventureUndergroundPlan plan =
        getAdventureUndergroundPlanForCellWithPrefilter(
            cellX, cellZ, surfaceHeight, biome,
            candidatePrefilter, true);

    rememberCachedAdventureUndergroundPlanForCell(
        cellX, cellZ, plan);
    return plan;
}

void CaveGenerator::rememberCachedAdventureUndergroundPlanForCell(
    int cellX, int cellZ,
    const AdventureUndergroundPlan &plan) const
{
    const std::size_t index = static_cast<std::size_t>(
        entranceHash(m_seed, cellX, cellZ, 97) %
        AdventurePlanCacheCapacity);

    // Final planning may populate a colliding neighbour. Never retain a
    // reference across that calculation; reacquire and reset the slot here.
    AdventurePlanCacheEntry &destination = m_adventurePlanCache[index];
    if (!destination.occupied || destination.cellX != cellX ||
        destination.cellZ != cellZ) {
        destination = AdventurePlanCacheEntry{};
        destination.occupied = true;
        destination.cellX = cellX;
        destination.cellZ = cellZ;
    }
    destination.hasFinalPlan = true;
    destination.plan = plan;
}

std::size_t CaveGenerator::projectAdventureUnderground(
    Chunk &chunk, const SurfaceHeightSampler &surfaceHeight,
    const BiomeSampler &biome) const
{
    return projectAdventureUndergroundImpl(
        chunk, surfaceHeight, biome,
        EntranceCandidatePrefilter{}, false);
}

std::size_t CaveGenerator::projectAdventureUnderground(
    Chunk &chunk, const SurfaceHeightSampler &surfaceHeight,
    const BiomeSampler &biome,
    const EntranceCandidatePrefilter &candidatePrefilter) const
{
    return projectAdventureUndergroundImpl(
        chunk, surfaceHeight, biome, candidatePrefilter, true);
}

std::size_t CaveGenerator::projectAdventureUndergroundImpl(
    Chunk &chunk, const SurfaceHeightSampler &surfaceHeight,
    const BiomeSampler &biome,
    const EntranceCandidatePrefilter &candidatePrefilter,
    bool usePlanCache) const
{
    if (m_generationVersion <
            AdventureUndergroundTerrainGenerationVersion ||
        !surfaceHeight || !biome) {
        return 0;
    }

    std::size_t changed = 0;
    const glm::ivec2 chunkLocation = chunk.getLocation();
    int chunkMinimumX = 0;
    int chunkMinimumZ = 0;
    int chunkMaximumX = 0;
    int chunkMaximumZ = 0;
    if (!checkedInt(static_cast<std::int64_t>(chunkLocation.x) *
                        CHUNK_SIZE,
                    chunkMinimumX) ||
        !checkedInt(static_cast<std::int64_t>(chunkLocation.y) *
                        CHUNK_SIZE,
                    chunkMinimumZ) ||
        !checkedInt(static_cast<std::int64_t>(chunkLocation.x) *
                            CHUNK_SIZE + CHUNK_SIZE - 1,
                    chunkMaximumX) ||
        !checkedInt(static_cast<std::int64_t>(chunkLocation.y) *
                            CHUNK_SIZE + CHUNK_SIZE - 1,
                    chunkMaximumZ)) {
        return 0;
    }
    int minimumCellX = 0;
    int maximumCellX = 0;
    int minimumCellZ = 0;
    int maximumCellZ = 0;
    if (!checkedInt(floorDiv64(
                        static_cast<std::int64_t>(chunkMinimumX) -
                            AdventureUndergroundReach,
                        EntranceCellBlocks),
                    minimumCellX) ||
        !checkedInt(floorDiv64(
                        static_cast<std::int64_t>(chunkMaximumX) +
                            AdventureUndergroundReach,
                        EntranceCellBlocks),
                    maximumCellX) ||
        !checkedInt(floorDiv64(
                        static_cast<std::int64_t>(chunkMinimumZ) -
                            AdventureUndergroundReach,
                        EntranceCellBlocks),
                    minimumCellZ) ||
        !checkedInt(floorDiv64(
                        static_cast<std::int64_t>(chunkMaximumZ) +
                            AdventureUndergroundReach,
                        EntranceCellBlocks),
                    maximumCellZ)) {
        return 0;
    }

    const auto inChunk = [&](int worldX, int worldZ) {
        return worldX >= chunkMinimumX && worldX <= chunkMaximumX &&
            worldZ >= chunkMinimumZ && worldZ <= chunkMaximumZ;
    };
    const auto setBlock = [&](int worldX, int y, int worldZ,
                              BlockId block) {
        if (!inChunk(worldX, worldZ) || y < AdventureMinimumY ||
            y >= 256) {
            return;
        }
        const int localX = worldX - chunkMinimumX;
        const int localZ = worldZ - chunkMinimumZ;
        const BlockId previous = static_cast<BlockId>(
            chunk.getBlock(localX, y, localZ).id);
        if (previous == block ||
            (previous == BlockId::Water && block != BlockId::Water)) {
            return;
        }
        chunk.setBlock(localX, y, localZ, block);
        ++changed;
    };
    const auto carveColumn = [&](int worldX, int worldZ,
                                 int minimumY, int maximumY) {
        for (int y = minimumY; y <= maximumY; ++y) {
            setBlock(worldX, y, worldZ, BlockId::Air);
        }
    };
    const auto ensureDryFloor = [&](int worldX, int y, int worldZ,
                                    BlockId block) {
        if (!inChunk(worldX, worldZ) || y < AdventureMinimumY ||
            y >= 256) {
            return;
        }
        const int localX = worldX - chunkMinimumX;
        const int localZ = worldZ - chunkMinimumZ;
        if (static_cast<BlockId>(
                chunk.getBlock(localX, y, localZ).id) == BlockId::Air) {
            setBlock(worldX, y, worldZ, block);
        }
    };
    const AdventureHorizontalBounds chunkBounds{
        true, chunkMinimumX, chunkMaximumX,
        chunkMinimumZ, chunkMaximumZ};
    const auto rangeProjectedIntoChunk =
        [&](int centerX, int centerZ, int axisX, int axisZ,
            int minimum, int maximum) {
            const std::array<std::int64_t, 4> projected{{
                (static_cast<std::int64_t>(chunkMinimumX) - centerX) *
                        axisX +
                    (static_cast<std::int64_t>(chunkMinimumZ) - centerZ) *
                        axisZ,
                (static_cast<std::int64_t>(chunkMinimumX) - centerX) *
                        axisX +
                    (static_cast<std::int64_t>(chunkMaximumZ) - centerZ) *
                        axisZ,
                (static_cast<std::int64_t>(chunkMaximumX) - centerX) *
                        axisX +
                    (static_cast<std::int64_t>(chunkMinimumZ) - centerZ) *
                        axisZ,
                (static_cast<std::int64_t>(chunkMaximumX) - centerX) *
                        axisX +
                    (static_cast<std::int64_t>(chunkMaximumZ) - centerZ) *
                        axisZ,
            }};
            const auto limits = std::minmax_element(
                projected.begin(), projected.end());
            const std::int64_t clippedMinimum = std::max<std::int64_t>(
                minimum, *limits.first);
            const std::int64_t clippedMaximum = std::min<std::int64_t>(
                maximum, *limits.second);
            if (clippedMinimum > clippedMaximum) {
                return std::pair<int, int>{1, 0};
            }
            return std::pair<int, int>{
                static_cast<int>(clippedMinimum),
                static_cast<int>(clippedMaximum)};
        };
    AdventureWriteColumns fastPathCoreColumns;

    for (int cellX = minimumCellX; cellX <= maximumCellX; ++cellX) {
        for (int cellZ = minimumCellZ; cellZ <= maximumCellZ; ++cellZ) {
            AdventureUndergroundPlan cachedFinalPlan;
            bool hasCachedFinalPlan = false;
            if (usePlanCache) {
                const std::size_t finalCacheIndex =
                    static_cast<std::size_t>(
                        entranceHash(m_seed, cellX, cellZ, 97) %
                        AdventurePlanCacheCapacity);
                const AdventurePlanCacheEntry &finalCache =
                    m_adventurePlanCache[finalCacheIndex];
                if (finalCache.occupied && finalCache.cellX == cellX &&
                    finalCache.cellZ == cellZ &&
                    finalCache.hasFinalPlan) {
                    // Copy before projection: no reference may outlive a
                    // nested cache lookup that can replace this direct-
                    // mapped slot. A final plan already includes the full
                    // surface, legacy-entrance and priority decision for the
                    // immutable production samplers.
                    cachedFinalPlan = finalCache.plan;
                    hasCachedFinalPlan = true;
                    if (!cachedFinalPlan.valid) {
                        continue;
                    }
                }
            }
            bool exactCoreColumnsAvailable = false;
            if (usePlanCache && !hasCachedFinalPlan) {
                const AdventureUndergroundPlan rawPlan =
                    getCachedRawAdventureUndergroundPlanForCell(
                        cellX, cellZ, surfaceHeight, biome,
                        candidatePrefilter);
                if (!rawPlan.valid) {
                    continue;
                }
                const AdventureHorizontalBounds coreBounds =
                    adventureCoreProjectionHorizontalBounds(rawPlan);
                bool coreMayChangeChunk = true;
                if (coreBounds.valid) {
                    if (!overlaps(coreBounds, chunkBounds)) {
                        coreMayChangeChunk = false;
                    }
                    else if (collectAdventureWriteColumns(
                                 rawPlan, fastPathCoreColumns)) {
                        exactCoreColumnsAvailable = true;
                        // The endpoint box is deliberately conservative. Its
                        // corners can touch a chunk even though the oriented
                        // chamber, rift, pool, connector and destination have
                        // no write column there. Reusing the same exhaustive
                        // core-column collector as certification closes that
                        // false positive without changing public planning.
                        coreMayChangeChunk =
                            adventureColumnsIntersectHorizontalBounds(
                                fastPathCoreColumns, chunkBounds);
                    }
                }
                if (!coreMayChangeChunk) {
                    // Outside the core bound the final projection only calls
                    // ensureDryFloor for the legacy entrance's first 24
                    // steps. That helper writes Cobblestone only when the
                    // exact floor block in this chunk is Air. If none is Air,
                    // final conflict/surface certification cannot change this
                    // chunk regardless of whether the raw plan is accepted.
                    bool earlyFloorMayChangeChunk = false;
                    const int perpendicularX =
                        -rawPlan.entrance.directionZ;
                    const int perpendicularZ =
                        rawPlan.entrance.directionX;
                    for (int step = 0;
                         step < EntranceTunnelLength &&
                         !earlyFloorMayChangeChunk; ++step) {
                        const int floorY =
                            rawPlan.entrance.anchorY - step * 3 / 4 - 1;
                        if (floorY < AdventureMinimumY || floorY >= 256) {
                            continue;
                        }
                        for (int lateral = -1; lateral <= 1; ++lateral) {
                            int worldX = 0;
                            int worldZ = 0;
                            if (!checkedInt(
                                    static_cast<std::int64_t>(
                                        rawPlan.entrance.anchorX) +
                                        rawPlan.entrance.directionX * step +
                                        perpendicularX * lateral,
                                    worldX) ||
                                !checkedInt(
                                    static_cast<std::int64_t>(
                                        rawPlan.entrance.anchorZ) +
                                        rawPlan.entrance.directionZ * step +
                                        perpendicularZ * lateral,
                                    worldZ)) {
                                earlyFloorMayChangeChunk = true;
                                break;
                            }
                            if (!inChunk(worldX, worldZ)) {
                                continue;
                            }
                            const int localX = worldX - chunkMinimumX;
                            const int localZ = worldZ - chunkMinimumZ;
                            if (static_cast<BlockId>(chunk.getBlock(
                                    localX, floorY, localZ).id) ==
                                    BlockId::Air) {
                                earlyFloorMayChangeChunk = true;
                                break;
                            }
                        }
                    }
                    if (!earlyFloorMayChangeChunk) {
                        continue;
                    }
                }

                // A lower-priority raw plan can reach this chunk yet be
                // deterministically rejected by a certified higher-priority
                // neighbour. Check that one-sided proof before certifying the
                // lower plan itself. Core-column overlap is deliberately
                // stricter than the full rule, so a miss falls back to normal
                // planning while a hit has the same invalid result.
                if (exactCoreColumnsAvailable) {
                    const AdventureHorizontalBounds exactCoreBounds =
                        adventureHorizontalBounds(fastPathCoreColumns);
                    bool rejectedByHigherPriorityCore = false;
                    const auto entranceForCell =
                        [this, &surfaceHeight, &biome,
                         &candidatePrefilter](int requestedCellX,
                                              int requestedCellZ) {
                            return getCachedNaturalEntranceForCell(
                                requestedCellX, requestedCellZ,
                                surfaceHeight, biome,
                                candidatePrefilter);
                        };
                    for (int offsetX = -2;
                         offsetX <= 2 &&
                         !rejectedByHigherPriorityCore;
                         ++offsetX) {
                        for (int offsetZ = -2;
                             offsetZ <= 2;
                             ++offsetZ) {
                            if (offsetX == 0 && offsetZ == 0) {
                                continue;
                            }
                            int otherCellX = 0;
                            int otherCellZ = 0;
                            if (!checkedInt(
                                    static_cast<std::int64_t>(cellX) +
                                        offsetX,
                                    otherCellX) ||
                                !checkedInt(
                                    static_cast<std::int64_t>(cellZ) +
                                        offsetZ,
                                    otherCellZ)) {
                                continue;
                            }
                            AdventureUndergroundPlan otherPriority;
                            otherPriority.entrance.cellX = otherCellX;
                            otherPriority.entrance.cellZ = otherCellZ;
                            otherPriority.stableKey = entranceHash(
                                m_seed, otherCellX, otherCellZ, 73);
                            if (adventurePlanPrecedes(
                                    rawPlan, otherPriority)) {
                                continue;
                            }
                            const std::size_t otherCacheIndex =
                                static_cast<std::size_t>(
                                    entranceHash(
                                        m_seed, otherCellX,
                                        otherCellZ, 97) %
                                    AdventurePlanCacheCapacity);
                            const AdventurePlanCacheEntry &otherCache =
                                m_adventurePlanCache[otherCacheIndex];
                            if (!otherCache.occupied ||
                                otherCache.cellX != otherCellX ||
                                otherCache.cellZ != otherCellZ ||
                                !otherCache.hasRawPlan) {
                                // This precheck never creates work. A missing
                                // neighbour falls back to the complete planner
                                // below, which retains the original semantics.
                                continue;
                            }
                            const AdventureUndergroundPlan other =
                                otherCache.rawPlan;
                            if (!other.valid ||
                                !adventurePlanPrecedes(other, rawPlan)) {
                                continue;
                            }
                            AdventureWriteColumns otherColumns;
                            if (!collectAdventureWriteColumns(
                                    other, otherColumns) ||
                                !overlaps(
                                    exactCoreBounds,
                                    adventurePotentialHorizontalBounds(
                                        other, otherColumns)) ||
                                !columnRangesOverlap(
                                    fastPathCoreColumns,
                                    otherColumns) ||
                                !certifyAdventureSurfaceSafety(
                                    other, surfaceHeight,
                                    otherColumns) ||
                                intersectsForeignLegacyEntrance(
                                    otherCellX, otherCellZ,
                                    other, otherColumns,
                                    entranceForCell)) {
                                continue;
                            }
                            rejectedByHigherPriorityCore = true;
                            break;
                        }
                    }
                    if (rejectedByHigherPriorityCore) {
                        rememberCachedAdventureUndergroundPlanForCell(
                            cellX, cellZ,
                            AdventureUndergroundPlan{});
                        continue;
                    }
                }
            }
            const AdventureUndergroundPlan plan = hasCachedFinalPlan
                ? cachedFinalPlan
                : usePlanCache
                    ? getCachedAdventureUndergroundPlanForCell(
                          cellX, cellZ, surfaceHeight, biome,
                          candidatePrefilter)
                    : getAdventureUndergroundPlanForCellWithPrefilter(
                          cellX, cellZ, surfaceHeight, biome,
                          candidatePrefilter, false);
            if (!plan.valid) {
                continue;
            }

            const bool polish = m_generationVersion >= UndergroundPolishTerrainGenerationVersion;
            const int entrancePerpendicularX =
                -plan.entrance.directionZ;
            const int entrancePerpendicularZ =
                plan.entrance.directionX;
            const auto chamberAlongRange = rangeProjectedIntoChunk(
                plan.chamberX, plan.chamberZ,
                plan.entrance.directionX,
                plan.entrance.directionZ,
                -AdventureChamberAlongRadius,
                AdventureChamberAlongRadius);
            const auto chamberLateralRange = rangeProjectedIntoChunk(
                plan.chamberX, plan.chamberZ,
                entrancePerpendicularX,
                entrancePerpendicularZ,
                -AdventureChamberPerpendicularRadius,
                AdventureChamberPerpendicularRadius);
            for (int along = chamberAlongRange.first;
                 along <= chamberAlongRange.second; ++along) {
                for (int lateral = chamberLateralRange.first;
                     lateral <= chamberLateralRange.second;
                     ++lateral) {
                    const double horizontalNormalized =
                        static_cast<double>(along * along) /
                            (AdventureChamberAlongRadius *
                             AdventureChamberAlongRadius) +
                        static_cast<double>(lateral * lateral) /
                            (AdventureChamberPerpendicularRadius *
                             AdventureChamberPerpendicularRadius);
                    for (int vertical = -AdventureChamberVerticalRadius;
                         vertical <= AdventureChamberVerticalRadius;
                         ++vertical) {
                        const double normalized =
                            horizontalNormalized +
                            static_cast<double>(vertical * vertical) /
                                (AdventureChamberVerticalRadius *
                                 AdventureChamberVerticalRadius);
                        if (normalized > 1.0) {
                            continue;
                        }
                        const int worldX = plan.chamberX +
                            plan.entrance.directionX * along +
                            entrancePerpendicularX * lateral;
                        const int worldZ = plan.chamberZ +
                            plan.entrance.directionZ * along +
                            entrancePerpendicularZ * lateral;
                        const int y = plan.chamberAirY + 3 + vertical;
                        setBlock(worldX, y, worldZ,
                            polish && !UndergroundPolish::chamberAir(plan.stableKey, along, lateral, vertical)
                                ? BlockId::Stone : BlockId::Air);
                    }
                }
            }

            const int riftPerpendicularX = -plan.riftDirectionZ;
            const int riftPerpendicularZ = plan.riftDirectionX;
            const int riftDrop =
                plan.chamberAirY - plan.riftEndAirY;
            const auto riftDistanceRange = rangeProjectedIntoChunk(
                plan.chamberX, plan.chamberZ,
                plan.riftDirectionX, plan.riftDirectionZ,
                AdventureRiftStartDistance,
                AdventureRiftStartDistance + AdventureRiftLength - 1);
            const auto riftLateralRange = rangeProjectedIntoChunk(
                plan.chamberX, plan.chamberZ,
                riftPerpendicularX, riftPerpendicularZ,
                -AdventureRiftHalfWidth,
                AdventureRiftHalfWidth);
            for (int distance = riftDistanceRange.first;
                 distance <= riftDistanceRange.second; ++distance) {
                const int step = distance - AdventureRiftStartDistance;
                const int airY = plan.chamberAirY -
                    (riftDrop * step) /
                        (AdventureRiftLength - 1);
                for (int lateral = riftLateralRange.first;
                     lateral <= riftLateralRange.second; ++lateral) {
                    const int worldX = plan.chamberX +
                        plan.riftDirectionX * distance +
                        riftPerpendicularX * lateral;
                    const int worldZ = plan.chamberZ +
                        plan.riftDirectionZ * distance +
                        riftPerpendicularZ * lateral;
                    if (polish) {
                        const int ceiling = UndergroundPolish::riftCeiling(plan.stableKey, step, lateral);
                        for (int y=0; y<AdventureRiftHeight; ++y)
                            setBlock(worldX, airY+y, worldZ, y<=ceiling ? BlockId::Air : BlockId::Stone);
                    } else {
                        carveColumn(worldX, worldZ, airY, airY + AdventureRiftHeight - 1);
                    }
                }
            }

            const auto destinationAlongRange = rangeProjectedIntoChunk(
                plan.destinationX, plan.destinationZ,
                plan.riftDirectionX, plan.riftDirectionZ,
                -AdventureDestinationRadius,
                AdventureDestinationRadius);
            const auto destinationLateralRange = rangeProjectedIntoChunk(
                plan.destinationX, plan.destinationZ,
                riftPerpendicularX, riftPerpendicularZ,
                -AdventureDestinationRadius,
                AdventureDestinationRadius);
            for (int along = destinationAlongRange.first;
                 along <= destinationAlongRange.second; ++along) {
                for (int lateral = destinationLateralRange.first;
                     lateral <= destinationLateralRange.second; ++lateral) {
                    if (along * along + lateral * lateral >
                            AdventureDestinationRadius *
                                AdventureDestinationRadius) {
                        continue;
                    }
                    const int worldX = plan.destinationX +
                        plan.riftDirectionX * along +
                        riftPerpendicularX * lateral;
                    const int worldZ = plan.destinationZ +
                        plan.riftDirectionZ * along +
                        riftPerpendicularZ * lateral;
                    const bool mine = plan.layout == AdventureUndergroundLayout::MinerCache;
                    if (polish) {
                        const int ceiling = UndergroundPolish::roomCeiling(mine, plan.stableKey, along, lateral);
                        for (int y=0; y<AdventureRoomHeight; ++y) {
                            const bool shelf = UndergroundPolish::roomShelf(mine, along, lateral, y);
                            setBlock(worldX, plan.riftEndAirY+y, worldZ,
                                shelf ? BlockId::MossStone : y<=ceiling ? BlockId::Air : BlockId::Stone);
                        }
                    } else {
                        carveColumn(worldX, worldZ, plan.riftEndAirY,
                                    plan.riftEndAirY + AdventureRoomHeight - 1);
                    }
                    BlockId floor = BlockId::Cobblestone;
                    if (plan.layout ==
                            AdventureUndergroundLayout::MossCellar) {
                        const int patchAlong =
                            (along + AdventureDestinationRadius) / 2;
                        const int patchLateral =
                            (lateral + AdventureDestinationRadius) / 2;
                        const std::uint64_t pattern = mix(
                            plan.stableKey ^
                            mix(static_cast<std::uint64_t>(patchAlong) +
                                0x632be59bd9b4e019ull) ^
                            mix(static_cast<std::uint64_t>(patchLateral) +
                                0x8cb92baa3f3d8dd7ull));
                        switch (pattern & 3ull) {
                            case 0: floor = BlockId::MossStone; break;
                            case 1: floor = BlockId::Silt; break;
                            case 2: floor = BlockId::Gravel; break;
                            default: floor = BlockId::Stone; break;
                        }
                    }
                    if (polish) floor = UndergroundPolish::roomFloor(mine, plan.stableKey, along, lateral);
                    setBlock(worldX, plan.riftEndAirY - 1,
                             worldZ, floor);
                }
            }

            const int poolX = plan.chamberX +
                plan.poolDirectionX * 5;
            const int poolZ = plan.chamberZ +
                plan.poolDirectionZ * 5;
            const auto poolXRange = rangeProjectedIntoChunk(
                poolX, poolZ, 1, 0,
                -AdventurePoolRadius, AdventurePoolRadius);
            const auto poolZRange = rangeProjectedIntoChunk(
                poolX, poolZ, 0, 1,
                -AdventurePoolRadius, AdventurePoolRadius);
            for (int x = poolXRange.first;
                 x <= poolXRange.second; ++x) {
                for (int z = poolZRange.first;
                     z <= poolZRange.second; ++z) {
                    if (x * x + z * z >
                            AdventurePoolRadius * AdventurePoolRadius) {
                        continue;
                    }
                    const int worldX = poolX + x;
                    const int worldZ = poolZ + z;
                    setBlock(worldX, plan.chamberAirY - 3, worldZ,
                             BlockId::MossStone);
                    setBlock(worldX, plan.chamberAirY - 2, worldZ,
                             BlockId::Water);
                    carveColumn(worldX, worldZ,
                                plan.chamberAirY - 1,
                                plan.chamberAirY + 2);
                }
            }
            const auto poolBankXRange = rangeProjectedIntoChunk(
                poolX, poolZ, 1, 0,
                -AdventurePoolRadius - 1, AdventurePoolRadius + 1);
            const auto poolBankZRange = rangeProjectedIntoChunk(
                poolX, poolZ, 0, 1,
                -AdventurePoolRadius - 1, AdventurePoolRadius + 1);
            for (int x = poolBankXRange.first;
                 x <= poolBankXRange.second; ++x) {
                for (int z = poolBankZRange.first;
                     z <= poolBankZRange.second; ++z) {
                    const int radiusSquared = x * x + z * z;
                    if (radiusSquared <=
                            AdventurePoolRadius * AdventurePoolRadius ||
                        radiusSquared >
                            (AdventurePoolRadius + 1) *
                                (AdventurePoolRadius + 1)) {
                        continue;
                    }
                    const std::uint64_t bank = plan.stableKey +
                        static_cast<std::uint64_t>(x + 8) * 11ull +
                        static_cast<std::uint64_t>(z + 8) * 23ull;
                    setBlock(poolX + x, plan.chamberAirY - 2,
                             poolZ + z,
                             polish ? UndergroundPolish::poolBank(x,z,plan.stableKey) :
                             (bank & 1ull) == 0 ? BlockId::Silt : BlockId::Gravel);
                }
            }

            int outcropDirectionX = -plan.poolDirectionX;
            int outcropDirectionZ = -plan.poolDirectionZ;
            if (outcropDirectionX == plan.riftDirectionX &&
                outcropDirectionZ == plan.riftDirectionZ) {
                outcropDirectionX = plan.entrance.directionX;
                outcropDirectionZ = plan.entrance.directionZ;
            }
            const bool alongChamber =
                std::abs(outcropDirectionX) ==
                    std::abs(plan.entrance.directionX) &&
                std::abs(outcropDirectionZ) ==
                    std::abs(plan.entrance.directionZ);
            const int outcropDistance = alongChamber
                ? AdventureChamberAlongRadius
                : AdventureChamberPerpendicularRadius;
            const int outcropX = plan.chamberX +
                outcropDirectionX * outcropDistance;
            const int outcropZ = plan.chamberZ +
                outcropDirectionZ * outcropDistance;
            setBlock(outcropX, plan.chamberAirY + 3,
                     outcropZ, BlockId::CoalOre);
            setBlock(outcropX, plan.chamberAirY + 2,
                     outcropZ, BlockId::IronOre);

            int chestX = 0;
            int chestZ = 0;
            if (plan.layout == AdventureUndergroundLayout::MinerCache) {
                for (const int along : {-3, 0, 3}) {
                    for (const int side : {-1, 1}) {
                        const int worldX = plan.destinationX +
                            plan.riftDirectionX * along +
                            riftPerpendicularX * side * 4;
                        const int worldZ = plan.destinationZ +
                            plan.riftDirectionZ * along +
                            riftPerpendicularZ * side * 4;
                        for (int y = plan.riftEndAirY;
                             y < plan.riftEndAirY + (polish ? UndergroundPolish::timberHeight(side*4) : 4); ++y) {
                            setBlock(worldX, y, worldZ,
                                     BlockId::OakBark);
                        }
                    }
                    for (int lateral = -4; lateral <= 4; ++lateral) {
                        const int top = polish ? UndergroundPolish::timberHeight(lateral) : 4;
                        const int bottom = polish && std::abs(lateral)>=2 && std::abs(lateral)<=3 ? top-1 : top;
                        for (int y=bottom;y<=top;++y)
                            setBlock(plan.destinationX + plan.riftDirectionX*along + riftPerpendicularX*lateral,
                                     plan.riftEndAirY+y,
                                     plan.destinationZ + plan.riftDirectionZ*along + riftPerpendicularZ*lateral,
                                     BlockId::OakPlank);
                    }
                }
                if (polish) {
                    for (const int side : {-1,1}) for (int along=-3;along<=3;++along)
                        setBlock(plan.destinationX + plan.riftDirectionX*along + riftPerpendicularX*side*3,
                                 plan.riftEndAirY+4,
                                 plan.destinationZ + plan.riftDirectionZ*along + riftPerpendicularZ*side*3,
                                 BlockId::OakBark);
                }
                chestX = plan.destinationX +
                    plan.riftDirectionX * 2 +
                    riftPerpendicularX * 3;
                chestZ = plan.destinationZ +
                    plan.riftDirectionZ * 2 +
                    riftPerpendicularZ * 3;
                setBlock(chestX, plan.riftEndAirY, chestZ,
                         BlockId::Chest);
                for (const int side : {-1, 1}) {
                    setBlock(plan.destinationX -
                                 plan.riftDirectionX * 2 +
                                 riftPerpendicularX * side * 3,
                             plan.riftEndAirY,
                             plan.destinationZ -
                                 plan.riftDirectionZ * 2 +
                                 riftPerpendicularZ * side * 3,
                             BlockId::Torch);
                }
            }
            else {
                int resourceIndex = 0;
                for (int level = 1; level <= 2; ++level) {
                    for (int along = -2; along <= 2; ++along) {
                        if (resourceIndex >= 9) {
                            continue;
                        }
                        setBlock(
                            plan.destinationX +
                                plan.riftDirectionX * along +
                                riftPerpendicularX * 5,
                            plan.riftEndAirY + level,
                            plan.destinationZ +
                                plan.riftDirectionZ * along +
                                riftPerpendicularZ * 5,
                            resourceIndex++ < 6
                                ? BlockId::CoalOre
                                : BlockId::IronOre);
                    }
                }
                for (const int side : {-1, 1}) {
                    setBlock(plan.destinationX -
                                 plan.riftDirectionX * 3 +
                                 riftPerpendicularX * side * 2,
                             plan.riftEndAirY,
                             plan.destinationZ -
                                 plan.riftDirectionZ * 3 +
                                 riftPerpendicularZ * side * 2,
                             BlockId::Torch);
                }
            }

            const auto makeDryFooting = [&](int worldX, int airY,
                                             int worldZ) {
                ensureDryFloor(worldX, airY - 1, worldZ,
                               BlockId::Cobblestone);
                for (int y = airY; y <= airY + 2; ++y) {
                    setBlock(worldX, y, worldZ, BlockId::Air);
                }
            };
            for (int step = 0; step < EntranceTunnelLength; ++step) {
                const int airY =
                    plan.entrance.anchorY - step * 3 / 4;
                for (int lateral = -1; lateral <= 1; ++lateral) {
                    ensureDryFloor(
                        plan.entrance.anchorX +
                            plan.entrance.directionX * step +
                            entrancePerpendicularX * lateral,
                        airY - 1,
                        plan.entrance.anchorZ +
                            plan.entrance.directionZ * step +
                            entrancePerpendicularZ * lateral,
                        BlockId::Cobblestone);
                }
            }
            for (int step = EntranceTunnelLength;
                 step <= AdventureChamberDistance; ++step) {
                for (int lateral = -1; lateral <= 1; ++lateral) {
                    makeDryFooting(
                        plan.entrance.anchorX +
                            plan.entrance.directionX * step +
                            entrancePerpendicularX * lateral,
                        plan.chamberAirY,
                        plan.entrance.anchorZ +
                            plan.entrance.directionZ * step +
                            entrancePerpendicularZ * lateral);
                }
            }
            for (int step = 0; step < AdventureRiftStartDistance; ++step) {
                for (int lateral = -1; lateral <= 1; ++lateral) {
                    makeDryFooting(
                        plan.chamberX + plan.riftDirectionX * step +
                            riftPerpendicularX * lateral,
                        plan.chamberAirY,
                        plan.chamberZ + plan.riftDirectionZ * step +
                            riftPerpendicularZ * lateral);
                }
            }
            for (int step = 0; step < AdventureRiftLength; ++step) {
                const int airY = plan.chamberAirY -
                    (riftDrop * step) /
                        (AdventureRiftLength - 1);
                const int distance = AdventureRiftStartDistance + step;
                for (int lateral = -1; lateral <= 1; ++lateral) {
                    makeDryFooting(
                        plan.chamberX +
                            plan.riftDirectionX * distance +
                            riftPerpendicularX * lateral,
                        airY,
                        plan.chamberZ +
                            plan.riftDirectionZ * distance +
                            riftPerpendicularZ * lateral);
                }
            }

            if (plan.layout ==
                    AdventureUndergroundLayout::MinerCache &&
                inChunk(chestX, chestZ)) {
                ContainerInventory inventory(ChestContainer::SlotCount);
                const bool inventoryReady =
                    inventory.addItem(Material::TORCH, 4) == 4 &&
                    inventory.addItem(
                        Material::OAK_PLANK_BLOCK, 4) == 4 &&
                    inventory.addItem(Material::BREAD, 2) == 2;
                const glm::ivec3 localPosition{
                    chestX - chunkMinimumX, plan.riftEndAirY,
                    chestZ - chunkMinimumZ};
                if (static_cast<BlockId>(chunk.getBlock(
                        localPosition.x, localPosition.y,
                        localPosition.z).id) == BlockId::Chest &&
                    chunk.findBlockEntity(localPosition) == nullptr) {
                    if (!inventoryReady ||
                        !chunk.createBlockEntity({
                            localPosition,
                            ChestContainer::BlockEntityType,
                            inventory.serialize()})) {
                        setBlock(chestX, plan.riftEndAirY, chestZ,
                                 BlockId::OakPlank);
                    }
                }
            }
        }
    }
    return changed;
}

std::size_t CaveGenerator::carveNaturalEntrances(
    Chunk &chunk, const SurfaceHeightSampler &surfaceHeight,
    const BiomeSampler &biome, std::vector<NaturalEntrance> *vegetationPlans) const
{
    if (vegetationPlans) vegetationPlans->clear();
    if (m_generationVersion < MountainTerrainGenerationVersion) {
        return 0;
    }

    std::size_t carved = 0;
    const glm::ivec2 chunkLocation = chunk.getLocation();
    int chunkMinimumX = 0;
    int chunkMinimumZ = 0;
    int chunkMaximumX = 0;
    int chunkMaximumZ = 0;
    if (!checkedInt(static_cast<std::int64_t>(chunkLocation.x) *
                        CHUNK_SIZE,
                    chunkMinimumX) ||
        !checkedInt(static_cast<std::int64_t>(chunkLocation.y) *
                        CHUNK_SIZE,
                    chunkMinimumZ) ||
        !checkedInt(static_cast<std::int64_t>(chunkLocation.x) *
                            CHUNK_SIZE + CHUNK_SIZE - 1,
                    chunkMaximumX) ||
        !checkedInt(static_cast<std::int64_t>(chunkLocation.y) *
                            CHUNK_SIZE + CHUNK_SIZE - 1,
                    chunkMaximumZ)) {
        return 0;
    }
    const int reach = EntranceTunnelLength + ChamberRadius +
        (vegetationPlans ? (m_generationVersion >= VegetationPolishTerrainGenerationVersion
            ? PolishedVegetationPlanPadding : VegetationPlanPadding) : 0);
    int minimumCellX = 0;
    int maximumCellX = 0;
    int minimumCellZ = 0;
    int maximumCellZ = 0;
    if (!checkedInt(floorDiv64(
                        static_cast<std::int64_t>(chunkMinimumX) - reach,
                        EntranceCellBlocks),
                    minimumCellX) ||
        !checkedInt(floorDiv64(
                        static_cast<std::int64_t>(chunkMaximumX) + reach,
                        EntranceCellBlocks),
                    maximumCellX) ||
        !checkedInt(floorDiv64(
                        static_cast<std::int64_t>(chunkMinimumZ) - reach,
                        EntranceCellBlocks),
                    minimumCellZ) ||
        !checkedInt(floorDiv64(
                        static_cast<std::int64_t>(chunkMaximumZ) + reach,
                        EntranceCellBlocks),
                    maximumCellZ)) {
        return 0;
    }

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

            if (vegetationPlans) vegetationPlans->push_back(entrance);
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
