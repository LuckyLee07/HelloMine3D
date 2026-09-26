#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

struct AdventureUndergroundSafetyColumn {
    int minimumY = std::numeric_limits<int>::max();
    int maximumY = std::numeric_limits<int>::min();
};

using AdventureUndergroundSafetyColumns =
    std::map<std::pair<int, int>, AdventureUndergroundSafetyColumn>;

struct AdventureUndergroundSafetySample {
    int seed = 0;
    int cellX = 0;
    int cellZ = 0;
    int quadrant = 0;
    CaveGenerator::AdventureUndergroundPlan plan;
};

struct AdventureUndergroundSafetyChunk {
    glm::ivec2 location{0};
    std::unique_ptr<Chunk> chunk;
};

struct AdventureUndergroundDiffResult {
    bool changed = false;
    bool surfaceSafe = true;
    bool preservedWater = true;
    bool chestEntitiesMatchBlocks = true;
    std::size_t changedBlocks = 0;
    glm::ivec3 firstUnsafe{0};
    glm::ivec3 firstRemovedWater{0};
    glm::ivec3 firstOrphanChest{0};
};

void addAdventureUndergroundSafetyColumn(
    AdventureUndergroundSafetyColumns &columns,
    int worldX, int worldZ, int minimumY, int maximumY)
{
    auto &column = columns[{worldX, worldZ}];
    column.minimumY = std::min(column.minimumY, minimumY);
    column.maximumY = std::max(column.maximumY, maximumY);
}

AdventureUndergroundSafetyColumns
adventureUndergroundSafetyColumns(
    const CaveGenerator::AdventureUndergroundPlan &plan,
    const ClassicOverWorldGenerator &generator,
    bool includeEntranceFloorRepairs = true)
{
    AdventureUndergroundSafetyColumns columns;
    if (!plan.valid || !plan.entrance.valid) {
        return columns;
    }

    const int entrancePerpendicularX = -plan.entrance.directionZ;
    const int entrancePerpendicularZ = plan.entrance.directionX;
    const int riftPerpendicularX = -plan.riftDirectionZ;
    const int riftPerpendicularZ = plan.riftDirectionX;
    const int riftDrop = plan.chamberAirY - plan.riftEndAirY;

    for (int along = -CaveGenerator::AdventureChamberAlongRadius;
         along <= CaveGenerator::AdventureChamberAlongRadius; ++along) {
        for (int lateral =
                 -CaveGenerator::AdventureChamberPerpendicularRadius;
             lateral <= CaveGenerator::AdventureChamberPerpendicularRadius;
             ++lateral) {
            for (int vertical =
                     -CaveGenerator::AdventureChamberVerticalRadius;
                 vertical <= CaveGenerator::AdventureChamberVerticalRadius;
                 ++vertical) {
                const double normalized =
                    static_cast<double>(along * along) /
                        (CaveGenerator::AdventureChamberAlongRadius *
                         CaveGenerator::AdventureChamberAlongRadius) +
                    static_cast<double>(lateral * lateral) /
                        (CaveGenerator::AdventureChamberPerpendicularRadius *
                         CaveGenerator::AdventureChamberPerpendicularRadius) +
                    static_cast<double>(vertical * vertical) /
                        (CaveGenerator::AdventureChamberVerticalRadius *
                         CaveGenerator::AdventureChamberVerticalRadius);
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
                addAdventureUndergroundSafetyColumn(
                    columns, worldX, worldZ, y, y);
            }
        }
    }

    for (int step = 0; step < CaveGenerator::AdventureRiftLength; ++step) {
        const int airY = plan.chamberAirY -
            riftDrop * step /
                (CaveGenerator::AdventureRiftLength - 1);
        const int distance = 6 + step;
        for (int lateral = -CaveGenerator::AdventureRiftHalfWidth;
             lateral <= CaveGenerator::AdventureRiftHalfWidth; ++lateral) {
            addAdventureUndergroundSafetyColumn(
                columns,
                plan.chamberX + plan.riftDirectionX * distance +
                    riftPerpendicularX * lateral,
                plan.chamberZ + plan.riftDirectionZ * distance +
                    riftPerpendicularZ * lateral,
                airY - 1,
                airY + CaveGenerator::AdventureRiftHeight - 1);
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
            addAdventureUndergroundSafetyColumn(
                columns,
                plan.destinationX + plan.riftDirectionX * along +
                    riftPerpendicularX * lateral,
                plan.destinationZ + plan.riftDirectionZ * along +
                    riftPerpendicularZ * lateral,
                plan.riftEndAirY - 1, plan.riftEndAirY + 6);
        }
    }

    const int poolX = plan.chamberX + plan.poolDirectionX * 5;
    const int poolZ = plan.chamberZ + plan.poolDirectionZ * 5;
    for (int x = -CaveGenerator::AdventurePoolRadius;
         x <= CaveGenerator::AdventurePoolRadius; ++x) {
        for (int z = -CaveGenerator::AdventurePoolRadius;
             z <= CaveGenerator::AdventurePoolRadius; ++z) {
            if (x * x + z * z >
                    CaveGenerator::AdventurePoolRadius *
                        CaveGenerator::AdventurePoolRadius) {
                continue;
            }
            addAdventureUndergroundSafetyColumn(
                columns, poolX + x, poolZ + z,
                plan.chamberAirY - 3, plan.chamberAirY + 2);
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
            addAdventureUndergroundSafetyColumn(
                columns, poolX + x, poolZ + z,
                plan.chamberAirY - 2, plan.chamberAirY - 2);
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
    addAdventureUndergroundSafetyColumn(
        columns,
        plan.chamberX + outcropDirectionX * outcropDistance,
        plan.chamberZ + outcropDirectionZ * outcropDistance,
        plan.chamberAirY + 2, plan.chamberAirY + 3);

    if (plan.layout ==
            CaveGenerator::AdventureUndergroundLayout::MossCellar) {
        int resourceIndex = 0;
        for (int level = 1; level <= 2; ++level) {
            for (int along = -2; along <= 2; ++along) {
                if (resourceIndex++ >= 9) {
                    continue;
                }
                addAdventureUndergroundSafetyColumn(
                    columns,
                    plan.destinationX + plan.riftDirectionX * along +
                        riftPerpendicularX * 5,
                    plan.destinationZ + plan.riftDirectionZ * along +
                        riftPerpendicularZ * 5,
                    plan.riftEndAirY + level,
                    plan.riftEndAirY + level);
            }
        }
    }

    for (int step = CaveGenerator::EntranceTunnelLength;
         step <= 29; ++step) {
        for (int lateral = -1; lateral <= 1; ++lateral) {
            addAdventureUndergroundSafetyColumn(
                columns,
                plan.entrance.anchorX +
                    plan.entrance.directionX * step +
                    entrancePerpendicularX * lateral,
                plan.entrance.anchorZ +
                    plan.entrance.directionZ * step +
                    entrancePerpendicularZ * lateral,
                plan.chamberAirY - 1, plan.chamberAirY + 2);
        }
    }
    for (int step = 0; step < 6; ++step) {
        for (int lateral = -1; lateral <= 1; ++lateral) {
            addAdventureUndergroundSafetyColumn(
                columns,
                plan.chamberX + plan.riftDirectionX * step +
                    riftPerpendicularX * lateral,
                plan.chamberZ + plan.riftDirectionZ * step +
                    riftPerpendicularZ * lateral,
                plan.chamberAirY - 1, plan.chamberAirY + 2);
        }
    }
    if (includeEntranceFloorRepairs) {
        for (int step = 0;
             step < CaveGenerator::EntranceTunnelLength; ++step) {
            const int floorY = plan.entrance.anchorY - step * 3 / 4 - 1;
            for (int lateral = -1; lateral <= 1; ++lateral) {
                const int worldX = plan.entrance.anchorX +
                    plan.entrance.directionX * step +
                    entrancePerpendicularX * lateral;
                const int worldZ = plan.entrance.anchorZ +
                    plan.entrance.directionZ * step +
                    entrancePerpendicularZ * lateral;
                const int chamberAlong =
                    CaveGenerator::EntranceTunnelLength - step;
                const bool terminalChamberCarvesFloor =
                    chamberAlong * chamberAlong + lateral * lateral <= 9 &&
                    floorY >= plan.entrance.endY - 1 &&
                    floorY <= plan.entrance.endY + 3;
                if (terminalChamberCarvesFloor ||
                    floorY <= generator.getSurfaceHeightAtWorld(
                                      worldX, worldZ) - 5) {
                    addAdventureUndergroundSafetyColumn(
                        columns, worldX, worldZ, floorY, floorY);
                }
            }
        }
    }
    return columns;
}

bool certifyAdventureUndergroundSafetyColumns(
    const AdventureUndergroundSafetyColumns &columns,
    const ClassicOverWorldGenerator &generator,
    glm::ivec3 &firstFailure, int &failureSurface)
{
    for (const auto &entry : columns) {
        const int surface = generator.getSurfaceHeightAtWorld(
            entry.first.first, entry.first.second);
        if (entry.second.minimumY < 8 ||
            entry.second.maximumY > surface - 5) {
            firstFailure = {entry.first.first,
                            entry.second.maximumY,
                            entry.first.second};
            failureSurface = surface;
            return false;
        }
    }
    return !columns.empty();
}

AdventureUndergroundSafetyColumns
adventureUndergroundLegacyEntranceColumns(
    const CaveGenerator::NaturalEntrance &entrance)
{
    AdventureUndergroundSafetyColumns columns;
    if (!entrance.valid) {
        return columns;
    }
    const int perpendicularX = -entrance.directionZ;
    const int perpendicularZ = entrance.directionX;
    for (int step = 0;
         step <= CaveGenerator::EntranceTunnelLength; ++step) {
        const int airY = entrance.anchorY - step * 3 / 4;
        for (int lateral = -1; lateral <= 1; ++lateral) {
            addAdventureUndergroundSafetyColumn(
                columns,
                entrance.anchorX + entrance.directionX * step +
                    perpendicularX * lateral,
                entrance.anchorZ + entrance.directionZ * step +
                    perpendicularZ * lateral,
                airY - 1, airY + 2);
        }
    }
    const int endX = entrance.anchorX +
        entrance.directionX * CaveGenerator::EntranceTunnelLength;
    const int endZ = entrance.anchorZ +
        entrance.directionZ * CaveGenerator::EntranceTunnelLength;
    for (int offsetX = -3; offsetX <= 3; ++offsetX) {
        for (int offsetZ = -3; offsetZ <= 3; ++offsetZ) {
            if (offsetX * offsetX + offsetZ * offsetZ > 9) {
                continue;
            }
            addAdventureUndergroundSafetyColumn(
                columns, endX + offsetX, endZ + offsetZ,
                entrance.endY - 1, entrance.endY + 3);
        }
    }
    return columns;
}

bool adventureUndergroundSafetyColumnsOverlap(
    const AdventureUndergroundSafetyColumns &left,
    const AdventureUndergroundSafetyColumns &right,
    glm::ivec2 &firstPosition,
    AdventureUndergroundSafetyColumn &firstLeft,
    AdventureUndergroundSafetyColumn &firstRight)
{
    for (const auto &entry : left) {
        const auto other = right.find(entry.first);
        if (other == right.end() ||
            entry.second.minimumY > other->second.maximumY ||
            other->second.minimumY > entry.second.maximumY) {
            continue;
        }
        firstPosition = {entry.first.first, entry.first.second};
        firstLeft = entry.second;
        firstRight = other->second;
        return true;
    }
    return false;
}

bool adventureUndergroundSafetyBoundsOverlap(
    const AdventureUndergroundSafetyColumns &left,
    const AdventureUndergroundSafetyColumns &right)
{
    if (left.empty() || right.empty()) {
        return false;
    }
    const auto bounds = [](const auto &columns) {
        std::array<int, 4> result{{
            std::numeric_limits<int>::max(),
            std::numeric_limits<int>::min(),
            std::numeric_limits<int>::max(),
            std::numeric_limits<int>::min()}};
        for (const auto &entry : columns) {
            result[0] = std::min(result[0], entry.first.first);
            result[1] = std::max(result[1], entry.first.first);
            result[2] = std::min(result[2], entry.first.second);
            result[3] = std::max(result[3], entry.first.second);
        }
        return result;
    };
    const auto leftBounds = bounds(left);
    const auto rightBounds = bounds(right);
    return leftBounds[0] <= rightBounds[1] &&
        rightBounds[0] <= leftBounds[1] &&
        leftBounds[2] <= rightBounds[3] &&
        rightBounds[2] <= leftBounds[3];
}

bool adventureUndergroundEntranceBiomePoint(
    const CaveGenerator::NaturalEntrance &entrance,
    int worldX, int worldZ)
{
    return entrance.valid &&
        ((worldX == entrance.anchorX && worldZ == entrance.anchorZ) ||
         (worldX == entrance.anchorX +
                 entrance.directionX *
                     CaveGenerator::EntranceTunnelLength &&
          worldZ == entrance.anchorZ +
                 entrance.directionZ *
                     CaveGenerator::EntranceTunnelLength));
}

std::vector<glm::ivec2> adventureUndergroundSafetyLocations(
    const AdventureUndergroundSafetyColumns &columns,
    const CaveGenerator::AdventureUndergroundPlan *routePlan = nullptr)
{
    std::set<std::pair<int, int>> unique;
    for (const auto &entry : columns) {
        unique.emplace(
            WorldCoordinates::floorDiv(entry.first.first, CHUNK_SIZE),
            WorldCoordinates::floorDiv(entry.first.second, CHUNK_SIZE));
    }
    if (routePlan != nullptr && routePlan->valid) {
        const int perpendicularX = -routePlan->entrance.directionZ;
        const int perpendicularZ = routePlan->entrance.directionX;
        for (int step = 0;
             step <= CaveGenerator::EntranceTunnelLength; ++step) {
            for (int lateral = -1; lateral <= 1; ++lateral) {
                const int x = routePlan->entrance.anchorX +
                    routePlan->entrance.directionX * step +
                    perpendicularX * lateral;
                const int z = routePlan->entrance.anchorZ +
                    routePlan->entrance.directionZ * step +
                    perpendicularZ * lateral;
                unique.emplace(
                    WorldCoordinates::floorDiv(x, CHUNK_SIZE),
                    WorldCoordinates::floorDiv(z, CHUNK_SIZE));
            }
        }
    }
    std::vector<glm::ivec2> locations;
    locations.reserve(unique.size());
    for (const auto &location : unique) {
        locations.emplace_back(location.first, location.second);
    }
    return locations;
}

std::vector<AdventureUndergroundSafetyChunk>
generateAdventureUndergroundSafetyChunks(
    World &world, ClassicOverWorldGenerator &generator,
    std::vector<glm::ivec2> locations, bool reverse)
{
    if (reverse) {
        std::reverse(locations.begin(), locations.end());
    }
    std::vector<AdventureUndergroundSafetyChunk> chunks;
    chunks.reserve(locations.size());
    for (const glm::ivec2 &location : locations) {
        auto chunk = std::make_unique<Chunk>(world, location, false);
        generator.generateTerrainFor(*chunk);
        chunks.push_back({location, std::move(chunk)});
    }
    return chunks;
}

const Chunk *adventureUndergroundSafetyChunkAt(
    const std::vector<AdventureUndergroundSafetyChunk> &chunks,
    int worldX, int worldZ)
{
    const glm::ivec2 location{
        WorldCoordinates::floorDiv(worldX, CHUNK_SIZE),
        WorldCoordinates::floorDiv(worldZ, CHUNK_SIZE)};
    const auto found = std::find_if(
        chunks.begin(), chunks.end(), [&location](const auto &entry) {
            return entry.location == location;
        });
    return found == chunks.end() ? nullptr : found->chunk.get();
}

BlockId adventureUndergroundSafetyBlock(
    const std::vector<AdventureUndergroundSafetyChunk> &chunks,
    int worldX, int y, int worldZ)
{
    const Chunk *chunk = adventureUndergroundSafetyChunkAt(
        chunks, worldX, worldZ);
    return chunk == nullptr
        ? BlockId::Air
        : static_cast<BlockId>(chunk->getBlock(
              WorldCoordinates::floorMod(worldX, CHUNK_SIZE), y,
              WorldCoordinates::floorMod(worldZ, CHUNK_SIZE)).id);
}

bool adventureUndergroundSafetyHashesMatch(
    const std::vector<AdventureUndergroundSafetyChunk> &forward,
    const std::vector<AdventureUndergroundSafetyChunk> &reverse)
{
    if (forward.size() != reverse.size()) {
        return false;
    }
    for (const auto &entry : forward) {
        const auto found = std::find_if(
            reverse.begin(), reverse.end(), [&entry](const auto &other) {
                return other.location == entry.location;
            });
        if (found == reverse.end() ||
            TerrainSurvey::blockHash(*entry.chunk) !=
                TerrainSurvey::blockHash(*found->chunk)) {
            return false;
        }
    }
    return true;
}

bool adventureUndergroundChestEntitiesMatchBlocks(
    const std::vector<AdventureUndergroundSafetyChunk> &chunks,
    glm::ivec3 &firstFailure)
{
    for (const auto &entry : chunks) {
        for (const BlockEntityRecord &entity :
             entry.chunk->getBlockEntities()) {
            if (entity.type != ChestContainer::BlockEntityType) {
                continue;
            }
            if (static_cast<BlockId>(entry.chunk->getBlock(
                    entity.position.x, entity.position.y,
                    entity.position.z).id) != BlockId::Chest) {
                firstFailure = {
                    entry.location.x * CHUNK_SIZE + entity.position.x,
                    entity.position.y,
                    entry.location.y * CHUNK_SIZE + entity.position.z};
                return false;
            }
        }
    }
    return true;
}

AdventureUndergroundDiffResult verifyAdventureUndergroundActualDiff(
    World &world, int seed,
    const CaveGenerator::AdventureUndergroundPlan &plan)
{
    AdventureUndergroundDiffResult result;
    ClassicOverWorldGenerator legacy(
        seed, LandmarkWorkshopTerrainGenerationVersion);
    ClassicOverWorldGenerator current(
        seed, AdventureUndergroundTerrainGenerationVersion);
    const AdventureUndergroundSafetyColumns columns =
        adventureUndergroundSafetyColumns(plan, current);
    const std::vector<glm::ivec2> locations =
        adventureUndergroundSafetyLocations(columns);
    auto before = generateAdventureUndergroundSafetyChunks(
        world, legacy, locations, false);
    auto after = generateAdventureUndergroundSafetyChunks(
        world, current, locations, false);

    for (const auto &entry : after) {
        const auto old = std::find_if(
            before.begin(), before.end(), [&entry](const auto &candidate) {
                return candidate.location == entry.location;
            });
        if (old == before.end()) {
            continue;
        }
        for (int localX = 0; localX < CHUNK_SIZE; ++localX) {
            for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ) {
                const int worldX = entry.location.x * CHUNK_SIZE + localX;
                const int worldZ = entry.location.y * CHUNK_SIZE + localZ;
                const int surface =
                    current.getSurfaceHeightAtWorld(worldX, worldZ);
                for (int y = 0; y < 256; ++y) {
                    const BlockId oldBlock = static_cast<BlockId>(
                        old->chunk->getBlock(localX, y, localZ).id);
                    const BlockId newBlock = static_cast<BlockId>(
                        entry.chunk->getBlock(localX, y, localZ).id);
                    if (oldBlock == newBlock) {
                        continue;
                    }
                    result.changed = true;
                    ++result.changedBlocks;
                    if (result.surfaceSafe &&
                        (y < 8 || y > surface - 5)) {
                        result.surfaceSafe = false;
                        result.firstUnsafe = {worldX, y, worldZ};
                    }
                    if (result.preservedWater &&
                        oldBlock == BlockId::Water &&
                        newBlock != BlockId::Water) {
                        result.preservedWater = false;
                        result.firstRemovedWater = {worldX, y, worldZ};
                    }
                }
            }
        }
    }
    result.chestEntitiesMatchBlocks =
        adventureUndergroundChestEntitiesMatchBlocks(
            after, result.firstOrphanChest);
    return result;
}

bool adventureUndergroundConflictRouteIsDry(
    const CaveGenerator::AdventureUndergroundPlan &plan,
    const std::vector<AdventureUndergroundSafetyChunk> &chunks,
    bool reverse, glm::ivec3 &firstFailure)
{
    struct Station {
        int x;
        int y;
        int z;
        int perpendicularX;
        int perpendicularZ;
    };
    std::vector<Station> route;
    const int entrancePerpendicularX = -plan.entrance.directionZ;
    const int entrancePerpendicularZ = plan.entrance.directionX;
    for (int step = 0;
         step <= CaveGenerator::EntranceTunnelLength; ++step) {
        route.push_back({
            plan.entrance.anchorX + plan.entrance.directionX * step,
            plan.entrance.anchorY - step * 3 / 4,
            plan.entrance.anchorZ + plan.entrance.directionZ * step,
            entrancePerpendicularX, entrancePerpendicularZ});
    }
    for (int step = CaveGenerator::EntranceTunnelLength + 1;
         step <= 29; ++step) {
        route.push_back({
            plan.entrance.anchorX + plan.entrance.directionX * step,
            plan.chamberAirY,
            plan.entrance.anchorZ + plan.entrance.directionZ * step,
            entrancePerpendicularX, entrancePerpendicularZ});
    }
    const int riftPerpendicularX = -plan.riftDirectionZ;
    const int riftPerpendicularZ = plan.riftDirectionX;
    for (int step = 1; step < 6; ++step) {
        route.push_back({
            plan.chamberX + plan.riftDirectionX * step,
            plan.chamberAirY,
            plan.chamberZ + plan.riftDirectionZ * step,
            riftPerpendicularX, riftPerpendicularZ});
    }
    const int riftDrop = plan.chamberAirY - plan.riftEndAirY;
    for (int step = 0; step < CaveGenerator::AdventureRiftLength; ++step) {
        const int distance = 6 + step;
        route.push_back({
            plan.chamberX + plan.riftDirectionX * distance,
            plan.chamberAirY - riftDrop * step /
                (CaveGenerator::AdventureRiftLength - 1),
            plan.chamberZ + plan.riftDirectionZ * distance,
            riftPerpendicularX, riftPerpendicularZ});
    }
    if (reverse) {
        std::reverse(route.begin(), route.end());
    }

    int previousY = route.empty() ? 0 : route.front().y;
    for (const Station &station : route) {
        if (std::abs(station.y - previousY) > 1) {
            firstFailure = {station.x, station.y, station.z};
            return false;
        }
        previousY = station.y;
        for (int lateral = -1; lateral <= 1; ++lateral) {
            const int worldX =
                station.x + station.perpendicularX * lateral;
            const int worldZ =
                station.z + station.perpendicularZ * lateral;
            const BlockId floor = adventureUndergroundSafetyBlock(
                chunks, worldX, station.y - 1, worldZ);
            if (floor == BlockId::Air || floor == BlockId::Water) {
                firstFailure = {worldX, station.y - 1, worldZ};
                return false;
            }
            for (int y = station.y; y <= station.y + 2; ++y) {
                if (adventureUndergroundSafetyBlock(
                        chunks, worldX, y, worldZ) != BlockId::Air) {
                    firstFailure = {worldX, y, worldZ};
                    return false;
                }
            }
        }
    }
    return true;
}

void caseAdventureUndergroundSafetyV23()
{
    setEnv("HELLOMINE3D_SEED", "0");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    Config config = makeConfig();
    Camera camera(config);
    Player owner;
    World sampleWorld(camera, config, owner,
        freshSaveDirectory("adventure_underground_safety"), false, 0);

    for (const glm::ivec2 counterexample : {
             glm::ivec2{-5, 5}, glm::ivec2{8, -5}}) {
        constexpr int seed = 8;
        ClassicOverWorldGenerator generator(
            seed, AdventureUndergroundTerrainGenerationVersion);
        CaveGenerator caves(
            seed, AdventureUndergroundTerrainGenerationVersion);
        const auto surface = [&generator](int x, int z) {
            return generator.getSurfaceHeightAtWorld(x, z);
        };
        const auto biome = [&generator](int x, int z) {
            return generator.getBiomeAtWorld(x, z);
        };
        const auto plan = caves.getAdventureUndergroundPlanForCell(
            counterexample.x, counterexample.y, surface, biome);
        bool safe = !plan.valid;
        AdventureUndergroundDiffResult diff;
        glm::ivec3 plannerFailure{0};
        int failureSurface = 0;
        if (plan.valid) {
            const auto columns =
                adventureUndergroundSafetyColumns(plan, generator);
            safe = certifyAdventureUndergroundSafetyColumns(
                columns, generator, plannerFailure, failureSurface);
            diff = verifyAdventureUndergroundActualDiff(
                sampleWorld, seed, plan);
            safe = safe && diff.changed && diff.surfaceSafe &&
                diff.preservedWater && diff.chestEntitiesMatchBlocks;
        }
        check("ADVENTURE-UNDERGROUND-SAFETY/fixed-surface-counterexample-" +
                  std::to_string(counterexample.x) + "-" +
                  std::to_string(counterexample.y),
              safe,
              "valid=" + std::to_string(plan.valid ? 1 : 0) +
                  " planner=" + std::to_string(plannerFailure.x) + "," +
                  std::to_string(plannerFailure.y) + "," +
                  std::to_string(plannerFailure.z) +
                  " surface=" + std::to_string(failureSurface) +
                  " diff=" + std::to_string(diff.changedBlocks) +
                  " unsafe=" + std::to_string(diff.firstUnsafe.x) + "," +
                  std::to_string(diff.firstUnsafe.y) + "," +
                  std::to_string(diff.firstUnsafe.z));
    }

    {
        constexpr int seed = 0;
        ClassicOverWorldGenerator generator(
            seed, AdventureUndergroundTerrainGenerationVersion);
        CaveGenerator caves(
            seed, AdventureUndergroundTerrainGenerationVersion);
        const auto surface = [&generator](int x, int z) {
            return generator.getSurfaceHeightAtWorld(x, z);
        };
        const auto biome = [&generator](int x, int z) {
            return generator.getBiomeAtWorld(x, z);
        };
        const auto firstEntrance = caves.getNaturalEntranceForCell(
            -23, 21, surface, biome);
        const auto secondEntrance = caves.getNaturalEntranceForCell(
            -23, 22, surface, biome);
        const auto firstOnlyBiome = [&firstEntrance](int x, int z) {
            return adventureUndergroundEntranceBiomePoint(
                       firstEntrance, x, z)
                ? TerrainBiome::Mountain
                : TerrainBiome::Ocean;
        };
        const auto secondOnlyBiome = [&secondEntrance](int x, int z) {
            return adventureUndergroundEntranceBiomePoint(
                       secondEntrance, x, z)
                ? TerrainBiome::Mountain
                : TerrainBiome::Ocean;
        };
        const auto pairBiome =
            [&firstEntrance, &secondEntrance](int x, int z) {
                return adventureUndergroundEntranceBiomePoint(
                           firstEntrance, x, z) ||
                        adventureUndergroundEntranceBiomePoint(
                           secondEntrance, x, z)
                    ? TerrainBiome::Mountain
                    : TerrainBiome::Ocean;
            };

        const auto firstCandidate =
            caves.getAdventureUndergroundPlanForCell(
                -23, 21, surface, firstOnlyBiome);
        const auto secondCandidate =
            caves.getAdventureUndergroundPlanForCell(
                -23, 22, surface, secondOnlyBiome);
        const auto firstColumns =
            adventureUndergroundSafetyColumns(
                firstCandidate, generator);
        const auto secondColumns =
            adventureUndergroundSafetyColumns(
                secondCandidate, generator);
        glm::ivec3 firstSafetyFailure{0};
        glm::ivec3 secondSafetyFailure{0};
        int firstFailureSurface = 0;
        int secondFailureSurface = 0;
        const bool firstSafe = firstCandidate.valid &&
            certifyAdventureUndergroundSafetyColumns(
                firstColumns, generator,
                firstSafetyFailure, firstFailureSurface);
        const bool secondSafe = secondCandidate.valid &&
            certifyAdventureUndergroundSafetyColumns(
                secondColumns, generator,
                secondSafetyFailure, secondFailureSurface);
        glm::ivec2 overlapPosition{0};
        AdventureUndergroundSafetyColumn firstOverlap;
        AdventureUndergroundSafetyColumn secondOverlap;
        const bool overlaps = adventureUndergroundSafetyColumnsOverlap(
            firstColumns, secondColumns, overlapPosition,
            firstOverlap, secondOverlap);
        const auto firstLegacyColumns =
            adventureUndergroundLegacyEntranceColumns(firstEntrance);
        const auto secondLegacyColumns =
            adventureUndergroundLegacyEntranceColumns(secondEntrance);
        glm::ivec2 legacyOverlapPosition{0};
        AdventureUndergroundSafetyColumn legacyLeftOverlap;
        AdventureUndergroundSafetyColumn legacyRightOverlap;
        const bool firstHitsSecondLegacy =
            adventureUndergroundSafetyColumnsOverlap(
                firstColumns, secondLegacyColumns,
                legacyOverlapPosition, legacyLeftOverlap,
                legacyRightOverlap);
        const bool secondHitsFirstLegacy =
            adventureUndergroundSafetyColumnsOverlap(
                secondColumns, firstLegacyColumns,
                legacyOverlapPosition, legacyLeftOverlap,
                legacyRightOverlap);
        const bool frozenConflict =
            firstCandidate.stableKey == 9471544586121437099ull &&
            secondCandidate.stableKey == 2484126533934838539ull &&
            overlapPosition == glm::ivec2(-2130, 2087) &&
            firstOverlap.minimumY == 94 &&
            firstOverlap.maximumY == 103 &&
            secondOverlap.minimumY == 95 &&
            secondOverlap.maximumY == 104;
        check("ADVENTURE-UNDERGROUND-SAFETY/fixed-new-new-candidates",
              firstEntrance.valid && secondEntrance.valid &&
                  firstSafe && secondSafe && overlaps && frozenConflict &&
                  !firstHitsSecondLegacy && !secondHitsFirstLegacy,
              "entrances=" +
                  std::to_string(firstEntrance.valid ? 1 : 0) + "/" +
                  std::to_string(secondEntrance.valid ? 1 : 0) +
                  " safe=" + std::to_string(firstSafe ? 1 : 0) + "/" +
                  std::to_string(secondSafe ? 1 : 0) +
                  " keys=" + std::to_string(firstCandidate.stableKey) +
                  "/" + std::to_string(secondCandidate.stableKey) +
                  " overlap=" + std::to_string(overlapPosition.x) + "," +
                  std::to_string(overlapPosition.y) + " y=" +
                  std::to_string(firstOverlap.minimumY) + ".." +
                  std::to_string(firstOverlap.maximumY) + "/" +
                  std::to_string(secondOverlap.minimumY) + ".." +
                  std::to_string(secondOverlap.maximumY) +
                  " legacy=" +
                  std::to_string(firstHitsSecondLegacy ? 1 : 0) + "/" +
                  std::to_string(secondHitsFirstLegacy ? 1 : 0));

        const auto isolatedFirst =
            caves.getAdventureUndergroundPlanForCell(
                -23, 21, surface, pairBiome);
        const auto isolatedSecond =
            caves.getAdventureUndergroundPlanForCell(
                -23, 22, surface, pairBiome);
        const auto first = caves.getAdventureUndergroundPlanForCell(
            -23, 21, surface, biome);
        const auto second = caves.getAdventureUndergroundPlanForCell(
            -23, 22, surface, biome);
        const int retained = static_cast<int>(isolatedFirst.valid) +
            static_cast<int>(isolatedSecond.valid);
        check("ADVENTURE-UNDERGROUND-SAFETY/fixed-conflict-single-winner",
              retained == 1 && isolatedFirst.valid &&
                  !isolatedSecond.valid && first.valid && !second.valid,
              "isolated=" +
                  std::to_string(isolatedFirst.valid ? 1 : 0) + "/" +
                  std::to_string(isolatedSecond.valid ? 1 : 0) +
                  " world=" + std::to_string(first.valid ? 1 : 0) + "/" +
                  std::to_string(second.valid ? 1 : 0));
        if (retained == 1 && first.valid) {
            const auto &plan = first;
            const auto columns =
                adventureUndergroundSafetyColumns(plan, generator);
            const auto locations = adventureUndergroundSafetyLocations(
                columns, &plan);
            ClassicOverWorldGenerator forwardGenerator(
                seed, AdventureUndergroundTerrainGenerationVersion);
            ClassicOverWorldGenerator reverseGenerator(
                seed, AdventureUndergroundTerrainGenerationVersion);
            auto forward = generateAdventureUndergroundSafetyChunks(
                sampleWorld, forwardGenerator, locations, false);
            auto reverse = generateAdventureUndergroundSafetyChunks(
                sampleWorld, reverseGenerator, locations, true);
            glm::ivec3 orphanChest{0};
            const bool chestEntitiesMatch =
                adventureUndergroundChestEntitiesMatchBlocks(
                    forward, orphanChest);
            glm::ivec3 outwardFailure{0};
            glm::ivec3 returnFailure{0};
            const bool outward = adventureUndergroundConflictRouteIsDry(
                plan, forward, false, outwardFailure);
            const bool returning = adventureUndergroundConflictRouteIsDry(
                plan, forward, true, returnFailure);
            check("ADVENTURE-UNDERGROUND-SAFETY/fixed-conflict-route",
                  outward && returning,
                  "out/return=" + std::to_string(outward ? 1 : 0) + "/" +
                      std::to_string(returning ? 1 : 0) + " failures=" +
                      std::to_string(outwardFailure.x) + "," +
                      std::to_string(outwardFailure.y) + "," +
                      std::to_string(outwardFailure.z) + "/" +
                      std::to_string(returnFailure.x) + "," +
                      std::to_string(returnFailure.y) + "," +
                      std::to_string(returnFailure.z));
            check("ADVENTURE-UNDERGROUND-SAFETY/fixed-conflict-order",
                  adventureUndergroundSafetyHashesMatch(forward, reverse),
                  "chunks=" + std::to_string(locations.size()));
            check("ADVENTURE-UNDERGROUND-SAFETY/fixed-conflict-entities",
                  chestEntitiesMatch,
                  "orphan=" + std::to_string(orphanChest.x) + "," +
                      std::to_string(orphanChest.y) + "," +
                      std::to_string(orphanChest.z));
        }
    }

    {
        constexpr int seed = 0;
        ClassicOverWorldGenerator generator(
            seed, AdventureUndergroundTerrainGenerationVersion);
        CaveGenerator caves(
            seed, AdventureUndergroundTerrainGenerationVersion);
        const auto surface = [&generator](int x, int z) {
            return generator.getSurfaceHeightAtWorld(x, z);
        };
        const auto biome = [&generator](int x, int z) {
            return generator.getBiomeAtWorld(x, z);
        };
        const auto currentEntrance = caves.getNaturalEntranceForCell(
            7, -5, surface, biome);
        const auto legacyEntrance = caves.getNaturalEntranceForCell(
            8, -5, surface, biome);
        const auto currentOnlyBiome = [&currentEntrance](int x, int z) {
            return adventureUndergroundEntranceBiomePoint(
                       currentEntrance, x, z)
                ? TerrainBiome::Mountain
                : TerrainBiome::Ocean;
        };
        const auto pairBiome =
            [&currentEntrance, &legacyEntrance](int x, int z) {
                return adventureUndergroundEntranceBiomePoint(
                           currentEntrance, x, z) ||
                        adventureUndergroundEntranceBiomePoint(
                           legacyEntrance, x, z)
                    ? TerrainBiome::Mountain
                    : TerrainBiome::Ocean;
            };
        const auto candidate = caves.getAdventureUndergroundPlanForCell(
            7, -5, surface, currentOnlyBiome);
        const auto candidateColumns =
            adventureUndergroundSafetyColumns(candidate, generator);
        glm::ivec3 safetyFailure{0};
        int failureSurface = 0;
        const bool candidateSafe = candidate.valid &&
            certifyAdventureUndergroundSafetyColumns(
                candidateColumns, generator,
                safetyFailure, failureSurface);
        const auto legacyColumns =
            adventureUndergroundLegacyEntranceColumns(legacyEntrance);
        glm::ivec2 overlapPosition{0};
        AdventureUndergroundSafetyColumn newOverlap;
        AdventureUndergroundSafetyColumn oldOverlap;
        const bool overlaps = adventureUndergroundSafetyColumnsOverlap(
            candidateColumns, legacyColumns, overlapPosition,
            newOverlap, oldOverlap);
        const bool frozenConflict =
            candidate.stableKey == 7031345879554050546ull &&
            overlapPosition == glm::ivec2(779, -425) &&
            newOverlap.minimumY == 124 &&
            newOverlap.maximumY == 133 &&
            oldOverlap.minimumY == 124 &&
            oldOverlap.maximumY == 128;
        const auto blocked = caves.getAdventureUndergroundPlanForCell(
            7, -5, surface, pairBiome);
        const auto worldBlocked = caves.getAdventureUndergroundPlanForCell(
            7, -5, surface, biome);
        check("ADVENTURE-UNDERGROUND-SAFETY/fixed-new-old-rejected",
              currentEntrance.valid && legacyEntrance.valid &&
                  candidateSafe && overlaps && frozenConflict &&
                  !blocked.valid && !worldBlocked.valid,
              "entrances=" +
                  std::to_string(currentEntrance.valid ? 1 : 0) + "/" +
                  std::to_string(legacyEntrance.valid ? 1 : 0) +
                  " candidate=" +
                  std::to_string(candidate.valid ? 1 : 0) +
                  " safe=" + std::to_string(candidateSafe ? 1 : 0) +
                  " overlap=" + std::to_string(overlapPosition.x) + "," +
                  std::to_string(overlapPosition.y) + " y=" +
                  std::to_string(newOverlap.minimumY) + ".." +
                  std::to_string(newOverlap.maximumY) + "/" +
                  std::to_string(oldOverlap.minimumY) + ".." +
                  std::to_string(oldOverlap.maximumY) +
                  " blocked=" + std::to_string(blocked.valid ? 1 : 0) +
                  "/" + std::to_string(worldBlocked.valid ? 1 : 0));
    }

    {
        // This fixture uses the production entrance carver to create the
        // counterexample.  The low surface column is an early floor of the
        // first entrance, while the second entrance's terminal chamber carves
        // that floor to Air.  The surface/biome samplers deliberately expose
        // only those two entrances and do not reproduce the planner's floor
        // repair predicate.
        constexpr int seed = 15;
        constexpr glm::ivec3 foreignCarvedFloor{65, 110, 91};
        constexpr glm::ivec2 affectedChunk{4, 5};
        constexpr glm::ivec3 affectedLocal{1, 110, 11};
        const auto surface = [](int x, int z) {
            if (x == 65 && z == 91) return 111;
            if (x == 66 && z == 79) return 120;
            if (x == 66 && z == 103) return 140;
            if ((x == 90 && z == 79) ||
                (x == 42 && z == 79) ||
                (x == 66 && z == 55)) {
                return 120;
            }
            if (x == 62 && z == 115) return 125;
            if (x == 62 && z == 91) return 145;
            if ((x == 86 && z == 115) ||
                (x == 38 && z == 115) ||
                (x == 62 && z == 139)) {
                return 125;
            }
            return 200;
        };
        const auto isolatedBiome = [](int x, int z) {
            return (x == 66 && z == 79) ||
                    (x == 66 && z == 103)
                ? TerrainBiome::Mountain
                : TerrainBiome::Ocean;
        };
        const auto pairedBiome = [](int x, int z) {
            return (x == 66 && z == 79) ||
                    (x == 66 && z == 103) ||
                    (x == 62 && z == 115) ||
                    (x == 62 && z == 91)
                ? TerrainBiome::Mountain
                : TerrainBiome::Ocean;
        };

        CaveGenerator planner(
            seed, AdventureUndergroundTerrainGenerationVersion);
        const auto currentEntrance = planner.getNaturalEntranceForCell(
            0, 0, surface, pairedBiome);
        const auto foreignEntrance = planner.getNaturalEntranceForCell(
            0, 1, surface, pairedBiome);
        const auto isolatedPlan = planner.getAdventureUndergroundPlanForCell(
            0, 0, surface, isolatedBiome);
        const auto protectedPlan = planner.getAdventureUndergroundPlanForCell(
            0, 0, surface, pairedBiome);

        Chunk isolatedLegacyChunk(sampleWorld, affectedChunk, false);
        isolatedLegacyChunk.setBlock(
            affectedLocal.x, affectedLocal.y, affectedLocal.z,
            BlockId::Stone);
        CaveGenerator isolatedLegacy(
            seed, LandmarkWorkshopTerrainGenerationVersion);
        isolatedLegacy.carveNaturalEntrances(
            isolatedLegacyChunk, surface, isolatedBiome);
        const bool isolatedFloorRemains = static_cast<BlockId>(
            isolatedLegacyChunk.getBlock(
                affectedLocal.x, affectedLocal.y,
                affectedLocal.z).id) == BlockId::Stone;

        Chunk projectedChunk(sampleWorld, affectedChunk, false);
        projectedChunk.setBlock(
            affectedLocal.x, affectedLocal.y, affectedLocal.z,
            BlockId::Stone);
        CaveGenerator projection(
            seed, AdventureUndergroundTerrainGenerationVersion);
        projection.carveNaturalEntrances(
            projectedChunk, surface, pairedBiome);
        const bool foreignLegacyCarved = static_cast<BlockId>(
            projectedChunk.getBlock(
                affectedLocal.x, affectedLocal.y,
                affectedLocal.z).id) == BlockId::Air;
        projection.projectAdventureUnderground(
            projectedChunk, surface, pairedBiome);
        const bool projectionPreservedLegacy = static_cast<BlockId>(
            projectedChunk.getBlock(
                affectedLocal.x, affectedLocal.y,
                affectedLocal.z).id) == BlockId::Air;

        const bool frozenFixture =
            currentEntrance.valid && foreignEntrance.valid &&
            currentEntrance.anchorX == 66 &&
            currentEntrance.anchorY == 120 &&
            currentEntrance.anchorZ == 79 &&
            currentEntrance.directionX == 0 &&
            currentEntrance.directionZ == 1 &&
            foreignEntrance.anchorX == 62 &&
            foreignEntrance.anchorY == 125 &&
            foreignEntrance.anchorZ == 115 &&
            foreignEntrance.directionX == 0 &&
            foreignEntrance.directionZ == -1 &&
            foreignCarvedFloor.x ==
                currentEntrance.anchorX - 1 &&
            foreignCarvedFloor.y ==
                currentEntrance.anchorY - 12 * 3 / 4 - 1 &&
            foreignCarvedFloor.z ==
                currentEntrance.anchorZ + 12 &&
            foreignCarvedFloor.y <= surface(
                foreignCarvedFloor.x, foreignCarvedFloor.z) &&
            foreignCarvedFloor.y > surface(
                foreignCarvedFloor.x, foreignCarvedFloor.z) - 5;
        check(
            "ADVENTURE-UNDERGROUND-SAFETY/foreign-legacy-carved-floor-rejected",
            frozenFixture && isolatedFloorRemains &&
                foreignLegacyCarved && isolatedPlan.valid &&
                !protectedPlan.valid && projectionPreservedLegacy,
            "entrances=" +
                std::to_string(currentEntrance.valid ? 1 : 0) + "/" +
                std::to_string(foreignEntrance.valid ? 1 : 0) +
                " isolated-floor=" +
                std::to_string(isolatedFloorRemains ? 1 : 0) +
                " legacy-carved=" +
                std::to_string(foreignLegacyCarved ? 1 : 0) +
                " plans=" +
                std::to_string(isolatedPlan.valid ? 1 : 0) + "/" +
                std::to_string(protectedPlan.valid ? 1 : 0) +
                " preserved=" +
                std::to_string(projectionPreservedLegacy ? 1 : 0));
    }

    {
        constexpr int seed = 138;
        ClassicOverWorldGenerator generator(
            seed, AdventureUndergroundTerrainGenerationVersion);
        CaveGenerator caves(
            seed, AdventureUndergroundTerrainGenerationVersion);
        const auto surface = [&generator](int x, int z) {
            return generator.getSurfaceHeightAtWorld(x, z);
        };
        const auto biome = [&generator](int x, int z) {
            return generator.getBiomeAtWorld(x, z);
        };
        const auto lowerEntrance = caves.getNaturalEntranceForCell(
            -17, 17, surface, biome);
        const auto higherEntrance = caves.getNaturalEntranceForCell(
            -16, 17, surface, biome);
        const auto lowerOnlyBiome = [&lowerEntrance](int x, int z) {
            return adventureUndergroundEntranceBiomePoint(
                       lowerEntrance, x, z)
                ? TerrainBiome::Mountain
                : TerrainBiome::Ocean;
        };
        const auto higherOnlyBiome = [&higherEntrance](int x, int z) {
            return adventureUndergroundEntranceBiomePoint(
                       higherEntrance, x, z)
                ? TerrainBiome::Mountain
                : TerrainBiome::Ocean;
        };
        const auto pairBiome =
            [&lowerEntrance, &higherEntrance](int x, int z) {
                return adventureUndergroundEntranceBiomePoint(
                           lowerEntrance, x, z) ||
                        adventureUndergroundEntranceBiomePoint(
                           higherEntrance, x, z)
                    ? TerrainBiome::Mountain
                    : TerrainBiome::Ocean;
            };
        const auto lowerCandidate =
            caves.getAdventureUndergroundPlanForCell(
                -17, 17, surface, lowerOnlyBiome);
        const auto higherCandidate =
            caves.getAdventureUndergroundPlanForCell(
                -16, 17, surface, higherOnlyBiome);
        const auto lowerColumns =
            adventureUndergroundSafetyColumns(
                lowerCandidate, generator);
        const auto higherColumns =
            adventureUndergroundSafetyColumns(
                higherCandidate, generator);
        const auto higherCoreColumns =
            adventureUndergroundSafetyColumns(
                higherCandidate, generator, false);
        glm::ivec3 lowerSafetyFailure{0};
        glm::ivec3 higherSafetyFailure{0};
        int lowerFailureSurface = 0;
        int higherFailureSurface = 0;
        const bool lowerSafe = lowerCandidate.valid &&
            certifyAdventureUndergroundSafetyColumns(
                lowerColumns, generator,
                lowerSafetyFailure, lowerFailureSurface);
        const bool higherSafe = higherCandidate.valid &&
            certifyAdventureUndergroundSafetyColumns(
                higherColumns, generator,
                higherSafetyFailure, higherFailureSurface);
        glm::ivec2 overlapPosition{0};
        AdventureUndergroundSafetyColumn lowerOverlap;
        AdventureUndergroundSafetyColumn higherOverlap;
        const bool fullOverlap =
            adventureUndergroundSafetyColumnsOverlap(
                lowerColumns, higherColumns, overlapPosition,
                lowerOverlap, higherOverlap);
        const bool rawCoreBoundsOverlap =
            adventureUndergroundSafetyBoundsOverlap(
                lowerColumns, higherCoreColumns);
        const auto lowerLegacyColumns =
            adventureUndergroundLegacyEntranceColumns(lowerEntrance);
        const auto higherLegacyColumns =
            adventureUndergroundLegacyEntranceColumns(higherEntrance);
        glm::ivec2 legacyOverlapPosition{0};
        AdventureUndergroundSafetyColumn legacyLeftOverlap;
        AdventureUndergroundSafetyColumn legacyRightOverlap;
        const bool lowerHitsHigherLegacy =
            adventureUndergroundSafetyColumnsOverlap(
                lowerColumns, higherLegacyColumns,
                legacyOverlapPosition, legacyLeftOverlap,
                legacyRightOverlap);
        const bool higherHitsLowerLegacy =
            adventureUndergroundSafetyColumnsOverlap(
                higherColumns, lowerLegacyColumns,
                legacyOverlapPosition, legacyLeftOverlap,
                legacyRightOverlap);
        const auto isolatedLower =
            caves.getAdventureUndergroundPlanForCell(
                -17, 17, surface, pairBiome);
        const auto isolatedHigher =
            caves.getAdventureUndergroundPlanForCell(
                -16, 17, surface, pairBiome);
        const auto worldLower =
            caves.getAdventureUndergroundPlanForCell(
                -17, 17, surface, biome);
        const auto worldHigher =
            caves.getAdventureUndergroundPlanForCell(
                -16, 17, surface, biome);
        const bool frozenConflict =
            lowerCandidate.stableKey == 14467031152860841006ull &&
            higherCandidate.stableKey == 18206620541733642499ull &&
            overlapPosition == glm::ivec2(-1514, 1686) &&
            lowerOverlap.minimumY == 125 &&
            lowerOverlap.maximumY == 134 &&
            higherOverlap.minimumY == 134 &&
            higherOverlap.maximumY == 134;
        check("ADVENTURE-UNDERGROUND-SAFETY/fixed-floor-only-legacy-guard",
              lowerEntrance.valid && higherEntrance.valid &&
                  lowerSafe && higherSafe && fullOverlap &&
                  !rawCoreBoundsOverlap &&
                  lowerHitsHigherLegacy && !higherHitsLowerLegacy &&
                  frozenConflict && !isolatedLower.valid &&
                  isolatedHigher.valid && !worldLower.valid &&
                  worldHigher.valid,
              "safe=" + std::to_string(lowerSafe ? 1 : 0) + "/" +
                  std::to_string(higherSafe ? 1 : 0) +
                  " keys=" + std::to_string(lowerCandidate.stableKey) +
                  "/" + std::to_string(higherCandidate.stableKey) +
                  " overlap=" + std::to_string(overlapPosition.x) + "," +
                  std::to_string(overlapPosition.y) + " y=" +
                  std::to_string(lowerOverlap.minimumY) + ".." +
                  std::to_string(lowerOverlap.maximumY) + "/" +
                  std::to_string(higherOverlap.minimumY) + ".." +
                  std::to_string(higherOverlap.maximumY) +
                  " raw-bounds=" +
                  std::to_string(rawCoreBoundsOverlap ? 1 : 0) +
                  " legacy=" +
                  std::to_string(lowerHitsHigherLegacy ? 1 : 0) + "/" +
                  std::to_string(higherHitsLowerLegacy ? 1 : 0) +
                  " isolated=" +
                  std::to_string(isolatedLower.valid ? 1 : 0) + "/" +
                  std::to_string(isolatedHigher.valid ? 1 : 0) +
                  " world=" + std::to_string(worldLower.valid ? 1 : 0) +
                  "/" + std::to_string(worldHigher.valid ? 1 : 0));
    }

    std::array<int, 4> validByQuadrant{{0, 0, 0, 0}};
    std::array<std::size_t, 4> scannedByQuadrant{{0, 0, 0, 0}};
    std::vector<AdventureUndergroundSafetySample> actualSamples;
    std::set<int> actualSeeds;
    std::size_t publicValid = 0;
    std::size_t certifiedColumns = 0;
    bool allPlannerSafe = true;
    AdventureUndergroundSafetySample firstPlannerFailure;
    glm::ivec3 firstPlannerFailurePosition{0};
    int firstPlannerFailureSurface = 0;

    for (int seed = 0; seed < 64; ++seed) {
        ClassicOverWorldGenerator generator(
            seed, AdventureUndergroundTerrainGenerationVersion);
        CaveGenerator caves(
            seed, AdventureUndergroundTerrainGenerationVersion);
        const auto surface = [&generator](int x, int z) {
            return generator.getSurfaceHeightAtWorld(x, z);
        };
        const auto biome = [&generator](int x, int z) {
            return generator.getBiomeAtWorld(x, z);
        };
        for (int quadrant = 0; quadrant < 4; ++quadrant) {
            const int signX = quadrant == 1 || quadrant == 2 ? -1 : 1;
            const int signZ = quadrant >= 2 ? -1 : 1;
            int retainedInSeedQuadrant = 0;
            for (int absoluteX = 2;
                 absoluteX <= 12 && retainedInSeedQuadrant < 2;
                 ++absoluteX) {
                for (int absoluteZ = 2;
                     absoluteZ <= 12 && retainedInSeedQuadrant < 2;
                     ++absoluteZ) {
                    ++scannedByQuadrant[quadrant];
                    const int cellX = signX * absoluteX;
                    const int cellZ = signZ * absoluteZ;
                    const auto plan = caves.getAdventureUndergroundPlanForCell(
                        cellX, cellZ, surface, biome);
                    if (!plan.valid) {
                        continue;
                    }
                    ++publicValid;
                    ++validByQuadrant[quadrant];
                    ++retainedInSeedQuadrant;
                    const auto columns =
                        adventureUndergroundSafetyColumns(plan, generator);
                    certifiedColumns += columns.size();
                    glm::ivec3 failure{0};
                    int failureSurface = 0;
                    const bool safe =
                        certifyAdventureUndergroundSafetyColumns(
                            columns, generator, failure, failureSurface);
                    if (!safe && allPlannerSafe) {
                        allPlannerSafe = false;
                        firstPlannerFailure =
                            {seed, cellX, cellZ, quadrant, plan};
                        firstPlannerFailurePosition = failure;
                        firstPlannerFailureSurface = failureSurface;
                    }
                    if (actualSamples.size() < 12 &&
                        actualSeeds.insert(seed).second) {
                        actualSamples.push_back(
                            {seed, cellX, cellZ, quadrant, plan});
                    }
                }
            }
        }
    }

    std::ostringstream scanDetail;
    scanDetail << "seeds=64 public=" << publicValid
               << " quadrants=" << validByQuadrant[0] << '/'
               << validByQuadrant[1] << '/' << validByQuadrant[2] << '/'
               << validByQuadrant[3] << " scanned=" <<
               scannedByQuadrant[0] << '/' << scannedByQuadrant[1] << '/' <<
               scannedByQuadrant[2] << '/' << scannedByQuadrant[3] <<
               " columns=" << certifiedColumns;
    check("ADVENTURE-UNDERGROUND-SAFETY/broad-public-plan-count",
          publicValid >= 128 &&
              std::all_of(validByQuadrant.begin(), validByQuadrant.end(),
                          [](int count) { return count > 0; }),
          scanDetail.str());
    check("ADVENTURE-UNDERGROUND-SAFETY/broad-exact-column-certificate",
          allPlannerSafe,
          scanDetail.str() + " first=" +
              std::to_string(firstPlannerFailure.seed) + ':' +
              std::to_string(firstPlannerFailure.cellX) + ',' +
              std::to_string(firstPlannerFailure.cellZ) + "@" +
              std::to_string(firstPlannerFailurePosition.x) + ',' +
              std::to_string(firstPlannerFailurePosition.y) + ',' +
              std::to_string(firstPlannerFailurePosition.z) +
              " surface=" + std::to_string(firstPlannerFailureSurface));
    check("ADVENTURE-UNDERGROUND-SAFETY/actual-sample-count",
          actualSamples.size() >= 12,
          "samples=" + std::to_string(actualSamples.size()));

    bool allActualSafe = actualSamples.size() >= 12;
    std::string firstActualFailure;
    std::size_t totalChanged = 0;
    for (const auto &sample : actualSamples) {
        const AdventureUndergroundDiffResult result =
            verifyAdventureUndergroundActualDiff(
                sampleWorld, sample.seed, sample.plan);
        totalChanged += result.changedBlocks;
        const bool safe = result.changed && result.surfaceSafe &&
            result.preservedWater && result.chestEntitiesMatchBlocks;
        if (!safe && firstActualFailure.empty()) {
            firstActualFailure =
                "seed=" + std::to_string(sample.seed) + " cell=" +
                std::to_string(sample.cellX) + ',' +
                std::to_string(sample.cellZ) + " changed=" +
                std::to_string(result.changedBlocks) + " unsafe=" +
                std::to_string(result.firstUnsafe.x) + ',' +
                std::to_string(result.firstUnsafe.y) + ',' +
                std::to_string(result.firstUnsafe.z) + " water=" +
                std::to_string(result.firstRemovedWater.x) + ',' +
                std::to_string(result.firstRemovedWater.y) + ',' +
                std::to_string(result.firstRemovedWater.z) + " chest=" +
                std::to_string(result.firstOrphanChest.x) + ',' +
                std::to_string(result.firstOrphanChest.y) + ',' +
                std::to_string(result.firstOrphanChest.z);
        }
        allActualSafe = allActualSafe && safe;
    }
    check("ADVENTURE-UNDERGROUND-SAFETY/actual-v22-v23-diff",
          allActualSafe,
          "samples=" + std::to_string(actualSamples.size()) +
              " changed=" + std::to_string(totalChanged) +
              (firstActualFailure.empty()
                   ? std::string()
                   : " first=" + firstActualFailure));
}

} // namespace
