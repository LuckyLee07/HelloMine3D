#pragma once

#include "../World/Generation/Ecology/AdventureEcologyPlanner.h"
#include "../World/Storage/ChunkStorageData.h"

#include <map>
#include <set>

namespace {
struct RockV30Site {
    int seed, x, z;
};

std::vector<glm::ivec2> rockV30SquareLocations(int centerX, int centerZ)
{
    const int cx = WorldCoordinates::floorDiv(centerX, CHUNK_SIZE);
    const int cz = WorldCoordinates::floorDiv(centerZ, CHUNK_SIZE);
    std::vector<glm::ivec2> locations;
    for (int x = cx - 1; x <= cx + 1; ++x)
        for (int z = cz - 1; z <= cz + 1; ++z) locations.emplace_back(x, z);
    return locations;
}

// Store complete chunks, including the ring used by the walking test. A read
// outside this explicit region is unknown, never air or a traversable cell.
class RockV30Region {
  public:
    RockV30Region(World &world, ClassicOverWorldGenerator &generator,
                  int centerX, int centerZ)
        : RockV30Region(world, generator, rockV30SquareLocations(centerX, centerZ)) {}

    RockV30Region(World &world, ClassicOverWorldGenerator &generator,
                  std::vector<glm::ivec2> locations, bool reverse = false)
    {
        minimumX = minimumZ = std::numeric_limits<int>::max();
        maximumX = maximumZ = std::numeric_limits<int>::min();
        if (reverse) std::reverse(locations.begin(), locations.end());
        for (const auto location : locations) {
            minimumX = std::min(minimumX, location.x * CHUNK_SIZE);
            maximumX = std::max(maximumX, (location.x + 1) * CHUNK_SIZE - 1);
            minimumZ = std::min(minimumZ, location.y * CHUNK_SIZE);
            maximumZ = std::max(maximumZ, (location.y + 1) * CHUNK_SIZE - 1);
            auto chunk = std::make_unique<Chunk>(world, location, false);
            generator.generateTerrainFor(*chunk);
            chunks.emplace(std::array<int, 2>{location.x, location.y}, std::move(chunk));
        }
    }

    ChunkBlock block(int x, int y, int z) const
    {
        if (y < 0 || y >= 256) return BlockId::NUM_TYPES;
        const int cx = WorldCoordinates::floorDiv(x, CHUNK_SIZE);
        const int cz = WorldCoordinates::floorDiv(z, CHUNK_SIZE);
        const auto found = chunks.find({cx, cz});
        if (found == chunks.end()) return BlockId::NUM_TYPES;
        return found->second->getBlock(x - cx * CHUNK_SIZE, y,
                                      z - cz * CHUNK_SIZE);
    }

    int minimumX = 0, maximumX = 0, minimumZ = 0, maximumZ = 0;
    std::map<std::array<int, 2>, std::unique_ptr<Chunk>> chunks;
};

std::vector<std::uint8_t> rockV30ChunkBytes(const Chunk &chunk)
{
    std::vector<std::uint8_t> result;
    result.reserve(CHUNK_SIZE * CHUNK_SIZE * 256 * 2);
    for (int x = 0; x < CHUNK_SIZE; ++x)
        for (int z = 0; z < CHUNK_SIZE; ++z)
            for (int y = 0; y < 256; ++y) {
                const auto block = chunk.getBlock(x, y, z);
                result.push_back(block.id);
                result.push_back(block.metadata);
            }
    const auto appendInteger = [&](std::uint32_t value) {
        for (int byte = 0; byte < 4; ++byte)
            result.push_back(static_cast<std::uint8_t>(value >> (byte * 8)));
    };
    const auto appendString = [&](const std::string &value) {
        appendInteger(static_cast<std::uint32_t>(value.size()));
        result.insert(result.end(), value.begin(), value.end());
    };
    appendInteger(static_cast<std::uint32_t>(chunk.getBlockEntities().size()));
    for (const auto &entity : chunk.getBlockEntities()) {
        appendInteger(static_cast<std::uint32_t>(entity.position.x));
        appendInteger(static_cast<std::uint32_t>(entity.position.y));
        appendInteger(static_cast<std::uint32_t>(entity.position.z));
        appendString(entity.type);
        appendString(entity.payload);
    }
    return result;
}

bool rockV30SameRegionBytes(const RockV30Region &forward, const RockV30Region &reverse)
{
    if (forward.chunks.size() != reverse.chunks.size()) return false;
    for (const auto &entry : forward.chunks) {
        const auto found = reverse.chunks.find(entry.first);
        if (found == reverse.chunks.end() ||
            rockV30ChunkBytes(*entry.second) != rockV30ChunkBytes(*found->second)) return false;
    }
    return true;
}

bool rockV30SetRegionBlock(RockV30Region &region, const glm::ivec3 &position, ChunkBlock block)
{
    if (position.y < 0 || position.y >= 256) return false;
    const auto location = World::getChunkXZ(position.x, position.z);
    const auto found = region.chunks.find({location.x, location.z});
    if (found == region.chunks.end()) return false;
    found->second->setBlock(WorldCoordinates::floorMod(position.x, CHUNK_SIZE), position.y,
                           WorldCoordinates::floorMod(position.z, CHUNK_SIZE), block);
    return true;
}

bool rockV30Stone(ChunkBlock block)
{
    const auto id = static_cast<BlockId>(block.id);
    return id == BlockId::Stone || id == BlockId::CoalOre || id == BlockId::IronOre;
}

bool rockV30Clear(ChunkBlock block)
{
    const auto id = static_cast<BlockId>(block.id);
    return id != BlockId::NUM_TYPES && id != BlockId::Water &&
           !block.getData().isCollidable;
}

struct RockV30Bounds {
    int minimumX = std::numeric_limits<int>::max();
    int maximumX = std::numeric_limits<int>::min();
    int minimumZ = std::numeric_limits<int>::max();
    int maximumZ = std::numeric_limits<int>::min();
    bool valid() const { return minimumX <= maximumX && minimumZ <= maximumZ; }
    void include(int x, int z)
    {
        minimumX = std::min(minimumX, x); maximumX = std::max(maximumX, x);
        minimumZ = std::min(minimumZ, z); maximumZ = std::max(maximumZ, z);
    }
};

// The graph uses actual floors and collision clearance; pure heights only
// nominate a floor. It excludes the pillar's bounding rectangle, so a route
// cannot pass this check by climbing across the raised top.
bool rockV30HasBypass(const RockV30Region &region,
                     ClassicOverWorldGenerator &generator,
                     const RockV30Bounds &body, std::size_t &walkableCount)
{
    walkableCount = 0;
    if (!body.valid()) return false;
    constexpr int margin = 8;
    const int minX = body.minimumX - margin, maxX = body.maximumX + margin;
    const int minZ = body.minimumZ - margin, maxZ = body.maximumZ + margin;
    if (minX < region.minimumX || maxX > region.maximumX ||
        minZ < region.minimumZ || maxZ > region.maximumZ) return false;
    std::map<std::array<int, 2>, int> floors;
    for (int x = minX; x <= maxX; ++x)
        for (int z = minZ; z <= maxZ; ++z) {
            if (x >= body.minimumX && x <= body.maximumX &&
                z >= body.minimumZ && z <= body.maximumZ) continue;
            const int y = generator.getSurfaceHeightAtWorld(x, z);
            const auto floor = region.block(x, y, z);
            if (floor != BlockId::NUM_TYPES && floor != BlockId::Water &&
                floor.getData().isCollidable &&
                rockV30Clear(region.block(x, y + 1, z)) &&
                rockV30Clear(region.block(x, y + 2, z)))
                floors.emplace(std::array<int, 2>{x, z}, y);
        }
    walkableCount = floors.size();
    std::set<std::array<int, 2>> visited;
    for (const auto &start : floors) {
        if (!visited.insert(start.first).second) continue;
        std::vector<std::array<int, 2>> pending{start.first};
        std::array<bool, 4> sides{};
        for (std::size_t i = 0; i < pending.size(); ++i) {
            const auto p = pending[i];
            const int h = floors.at(p);
            sides[0] |= p[0] <= body.minimumX - 2 &&
                p[1] >= body.minimumZ && p[1] <= body.maximumZ;
            sides[1] |= p[0] >= body.maximumX + 2 &&
                p[1] >= body.minimumZ && p[1] <= body.maximumZ;
            sides[2] |= p[1] <= body.minimumZ - 2 &&
                p[0] >= body.minimumX && p[0] <= body.maximumX;
            sides[3] |= p[1] >= body.maximumZ + 2 &&
                p[0] >= body.minimumX && p[0] <= body.maximumX;
            for (const auto &d : {std::array<int, 2>{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
                const std::array<int, 2> q{p[0] + d[0], p[1] + d[1]};
                const auto next = floors.find(q);
                if (next == floors.end() || std::abs(h - next->second) > 1 ||
                    visited.count(q) != 0) continue;
                const int higher = std::max(h, next->second);
                if (!rockV30Clear(region.block(p[0], higher + 1, p[1])) ||
                    !rockV30Clear(region.block(p[0], higher + 2, p[1])) ||
                    !rockV30Clear(region.block(q[0], higher + 1, q[1])) ||
                    !rockV30Clear(region.block(q[0], higher + 2, q[1]))) continue;
                visited.insert(q); pending.push_back(q);
            }
        }
        if (std::all_of(sides.begin(), sides.end(), [](bool side) { return side; }))
            return true;
    }
    return false;
}

bool rockV30ConnectedToGround(const RockV30Region &region,
                             ClassicOverWorldGenerator &previous,
                             const std::set<std::array<int, 3>> &raised,
                             const RockV30Bounds &body)
{
    if (raised.empty() || !body.valid()) return false;
    int minimumY = 256, maximumY = 0;
    for (const auto &p : raised) {
        minimumY = std::min(minimumY, previous.getSurfaceHeightAtWorld(p[0], p[2]) - 3);
        maximumY = std::max(maximumY, p[1]);
        if (!rockV30Stone(region.block(p[0], p[1], p[2]))) return false;
    }
    std::set<std::array<int, 3>> visited{*raised.begin()};
    std::vector<std::array<int, 3>> pending{*raised.begin()};
    bool groundContact = false;
    for (std::size_t i = 0; i < pending.size(); ++i) {
        const auto p = pending[i];
        groundContact |= p[1] <= previous.getSurfaceHeightAtWorld(p[0], p[2]);
        for (int axis = 0; axis < 3; ++axis)
            for (int direction : {-1, 1}) {
                auto q = p; q[axis] += direction;
                if (q[0] < body.minimumX || q[0] > body.maximumX ||
                    q[2] < body.minimumZ || q[2] > body.maximumZ ||
                    q[1] < minimumY || q[1] > maximumY ||
                    visited.count(q) != 0 ||
                    !rockV30Stone(region.block(q[0], q[1], q[2]))) continue;
                visited.insert(q); pending.push_back(q);
            }
    }
    return groundContact && std::all_of(raised.begin(), raised.end(),
        [&](const auto &p) { return visited.count(p) != 0; });
}

void rockV30CheckSite(World &world, const RockV30Site &site)
{
    const std::string label = std::to_string(site.seed) + "-" +
        std::to_string(site.x) + "-" + std::to_string(site.z);
    ClassicOverWorldGenerator generator(site.seed, RockLandmarkTerrainGenerationVersion);
    ClassicOverWorldGenerator previous(site.seed, WorkshopCourtyardTerrainGenerationVersion);
    AdventureEcologyPlanner ecology(site.seed, RockLandmarkTerrainGenerationVersion);
    AdventureWaterPlanner water(site.seed, RockLandmarkTerrainGenerationVersion);
    LocalTerrainPlanner local(site.seed, RockLandmarkTerrainGenerationVersion);
    RockV30Region region(world, generator, site.x, site.z);
    RockV30Bounds body;
    std::set<std::array<int, 3>> raised;
    std::set<std::array<int, 2>> changedChunks;
    bool queries = true, bounded = true, protectedWater = true, top = true, plants = true;
    int changed = 0, checkedPlants = 0, checkedRoots = 0, maxRise = 0;
    for (int x = region.minimumX; x <= region.maximumX; ++x)
        for (int z = region.minimumZ; z <= region.maximumZ; ++z) {
            const auto wet = water.sample(x, z);
            const auto pure = local.sample(x, z, wet);
            const auto full = ecology.sample(x, z);
            const auto light = ecology.sampleWaterColumn(x, z);
            const int height = generator.getSurfaceHeightAtWorld(x, z);
            const int oldHeight = previous.getSurfaceHeightAtWorld(x, z);
            const int rise = height - oldHeight;
            queries &= height == pure.height && height == full.column.height &&
                height == light.height && generator.getBiomeAtWorld(x, z) == full.column.biome;
            bounded &= height >= 1 && height <= 176 && rise >= 0 && rise <= 6 &&
                pure.rockCore == (rise > 0);
            if (wet.column.height <= 67 || wet.riverInfluence > 0 || wet.lakeInfluence > 0)
                protectedWater &= rise == 0 && !pure.rockCore;
            if (rise > 0) {
                ++changed; maxRise = std::max(maxRise, rise); body.include(x, z);
                changedChunks.insert({WorldCoordinates::floorDiv(x, CHUNK_SIZE),
                                      WorldCoordinates::floorDiv(z, CHUNK_SIZE)});
                top &= full.column.surface == TerrainFoundation::Surface::Stone &&
                    full.region != AdventureRegion::Wetland && full.region != AdventureRegion::Dunes &&
                    full.column.height < full.snowLine &&
                    rockV30Stone(region.block(x, height, z)) &&
                    region.block(x, height + 1, z) == BlockId::Air &&
                    region.block(x, height + 2, z) == BlockId::Air;
                for (int y = oldHeight + 1; y <= height; ++y) raised.insert({x, y, z});
            }
            // Inspect every actual decoration, rather than checking only the
            // plant which the planner wanted to place at the queried surface.
            for (int y = 1; y < 192; ++y) {
                const auto plant = region.block(x, y, z);
                const auto id = static_cast<BlockId>(plant.id);
                if (id == BlockId::TallGrass || id == BlockId::Rose || id == BlockId::DeadShrub) {
                    ++checkedPlants;
                    const auto support = region.block(x, y - 1, z);
                    plants &= support != BlockId::NUM_TYPES &&
                        AdventureEcologyPlanner::supportsPlant(static_cast<BlockId>(support.id),
                                                               static_cast<BlockId>(plant.id));
                }
            }
        }
    std::set<std::array<int, 2>> roots;
    bool treeSupport = true;
    for (const auto &entry : region.chunks) {
        int lowest = 256, highest = 0;
        for (int x = 0; x < CHUNK_SIZE; ++x)
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                const int height = generator.getSurfaceHeightAtWorld(
                    entry.first[0] * CHUNK_SIZE + x, entry.first[1] * CHUNK_SIZE + z);
                lowest = std::min(lowest, height + 1);
                highest = std::max(highest, height + 16);
            }
        for (int sectionY = lowest / CHUNK_SIZE; sectionY <= highest / CHUNK_SIZE; ++sectionY)
            treeSupport &= generator.visitNaturalTreeOwnership(entry.first[0], sectionY, entry.first[1],
                [&](const NaturalTreeOwnershipBlock &owned) {
                    if (owned.root[0] < region.minimumX || owned.root[0] > region.maximumX ||
                        owned.root[1] < region.minimumZ || owned.root[1] > region.maximumZ ||
                        region.block(owned.position[0], owned.position[1], owned.position[2]) != owned.block)
                        return;
                    roots.insert(owned.root);
                });
    }
    for (const auto &root : roots) {
        const int groundY = generator.getSurfaceHeightAtWorld(root[0], root[1]);
        const auto trunk = region.block(root[0], groundY + 1, root[1]);
        const auto trunkId = static_cast<BlockId>(trunk.id);
        ++checkedRoots;
        const auto ground = region.block(root[0], groundY, root[1]);
        treeSupport &= (trunkId == BlockId::OakBark || trunkId == BlockId::Cactus) &&
            ground != BlockId::NUM_TYPES && ground != BlockId::Water &&
            ground.getData().isCollidable;
    }
    bool reverseBytes = true;
    ClassicOverWorldGenerator reverse(site.seed, RockLandmarkTerrainGenerationVersion);
    for (auto entry = region.chunks.rbegin(); entry != region.chunks.rend(); ++entry) {
        Chunk chunk(world, {entry->first[0], entry->first[1]}, false);
        reverse.generateTerrainFor(chunk);
        reverseBytes &= rockV30ChunkBytes(chunk) == rockV30ChunkBytes(*entry->second);
    }
    std::size_t walkable = 0;
    const bool bypass = rockV30HasBypass(region, generator, body, walkable);
    const bool connected = rockV30ConnectedToGround(region, previous, raised, body);
    // Report the observed highest stone surface of this frozen pillar. This
    // samples generated blocks rather than reproducing the planner's shape.
    int plateauY = -1;
    std::set<std::array<int, 2>> plateau;
    RockV30Bounds plateauBounds;
    for (const auto &p : raised) {
        if (!rockV30Stone(region.block(p[0], p[1], p[2])) ||
            region.block(p[0], p[1] + 1, p[2]) != BlockId::Air) continue;
        if (p[1] > plateauY) { plateauY = p[1]; plateau.clear(); }
        if (p[1] == plateauY) plateau.insert({p[0], p[2]});
    }
    std::set<std::array<int, 2>> plateauVisited;
    std::size_t plateauComponents = 0;
    for (const auto &start : plateau) {
        plateauBounds.include(start[0], start[1]);
        if (!plateauVisited.insert(start).second) continue;
        ++plateauComponents;
        std::vector<std::array<int, 2>> pending{start};
        for (std::size_t i = 0; i < pending.size(); ++i)
            for (const auto &d : {std::array<int, 2>{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
                const std::array<int, 2> q{pending[i][0] + d[0], pending[i][1] + d[1]};
                if (plateau.count(q) != 0 && plateauVisited.insert(q).second) pending.push_back(q);
            }
    }
    std::cout << "[ROCK30_PLATEAU] seed=" << site.seed << " site=" << site.x << ',' << site.z
              << " actualTopY=" << plateauY << " columns=" << plateau.size()
              << " spanX/Z=" << (plateauBounds.valid() ? plateauBounds.maximumX - plateauBounds.minimumX + 1 : 0)
              << '/' << (plateauBounds.valid() ? plateauBounds.maximumZ - plateauBounds.minimumZ + 1 : 0)
              << " fourNeighbourComponents=" << plateauComponents << '\n';
    check("ROCK30/full-region-queries-and-rise-" + label, queries && bounded && protectedWater);
    check("ROCK30/actual-stone-top-and-six-neighbour-ground-contact-" + label,
          top && connected && changed > 0 && maxRise > 0,
          "changedColumns=" + std::to_string(changed) + " raisedBlocks=" + std::to_string(raised.size()) +
          " maxRise=" + std::to_string(maxRise));
    check("ROCK30/complete-cross-chunk-body-" + label,
          region.chunks.size() == 9 && changedChunks.size() >= 2 && body.valid() &&
          body.maximumX - body.minimumX <= 12 && body.maximumZ - body.minimumZ <= 12,
          "changedChunks=" + std::to_string(changedChunks.size()));
    check("ROCK30/two-cell-clearance-real-bypass-" + label, bypass,
          "walkableColumns=" + std::to_string(walkable));
    check("ROCK30/missing-region-rejected-" + label,
          region.block(region.minimumX - 1, 100, region.minimumZ) == BlockId::NUM_TYPES &&
          region.block(region.minimumX, 100, region.maximumZ + 1) == BlockId::NUM_TYPES &&
          !rockV30Clear(region.block(region.minimumX - 1, 100, region.minimumZ)));
    check("ROCK30/actual-plant-and-tree-support-" + label, plants && treeSupport,
          "plants=" + std::to_string(checkedPlants) + " roots=" + std::to_string(checkedRoots));
    check("ROCK30/reverse-complete-block-metadata-entity-bytes-" + label, reverseBytes);
}

void rockV30CheckFutureV29(Camera &camera, Config &config, const RockV30Site &site)
{
    setEnv("HELLOMINE3D_SEED", std::to_string(site.seed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    const auto directory = freshSaveDirectory("rock30_future_v29_" + std::to_string(site.seed));
    const int cx = WorldCoordinates::floorDiv(site.x, CHUNK_SIZE);
    const int cz = WorldCoordinates::floorDiv(site.z, CHUNK_SIZE);
    const auto futurePath = ChunkStorageData(ResourcePaths::join(directory, "chunks")).chunkPath(cx, cz);
    const bool initialized = initializeTerrainIdentity(directory,
        "rock30-old-v29-" + std::to_string(site.seed), WorkshopCourtyardTerrainGenerationVersion, site.seed);
    bool saved = false, absentBeforeSave = false;
    {
        Player player;
        World created(camera, config, player, directory, false, 0);
        auto &manager = created.getChunkManager();
        manager.loadChunk(0, 0);
        created.setBlock(1, 180, 1, BlockId::OakPlank);
        absentBeforeSave = manager.findChunk(cx, cz) == nullptr && !std::filesystem::exists(futurePath);
        saved = created.save();
    }
    const bool absentAfterSave = !std::filesystem::exists(futurePath);
    bool futureV29 = false, trulyDifferent = false, editedChunk = false, absentAfterReopen = false;
    {
        Player player;
        World reopened(camera, config, player, directory, false, 0);
        auto &manager = reopened.getChunkManager();
        absentAfterReopen = manager.findChunk(cx, cz) == nullptr && !std::filesystem::exists(futurePath);
        // These detached reference chunks never load the target into the old
        // World. Its first target load below therefore exercises versioned
        // future exploration, rather than merely reading a saved v29 chunk.
        ClassicOverWorldGenerator explicitV29(site.seed, WorkshopCourtyardTerrainGenerationVersion);
        ClassicOverWorldGenerator explicitV30(site.seed, RockLandmarkTerrainGenerationVersion);
        Chunk oldReference(reopened, {cx, cz}, false), newReference(reopened, {cx, cz}, false);
        explicitV29.generateTerrainFor(oldReference);
        explicitV30.generateTerrainFor(newReference);
        const auto oldBytes = rockV30ChunkBytes(oldReference);
        trulyDifferent = oldBytes != rockV30ChunkBytes(newReference);
        manager.loadChunk(cx, cz);
        futureV29 = manager.getTerrainGenerationVersion() == WorkshopCourtyardTerrainGenerationVersion &&
            rockV30ChunkBytes(manager.getChunk(cx, cz)) == oldBytes;
        manager.loadChunk(0, 0);
        editedChunk = reopened.getBlock(1, 180, 1) == BlockId::OakPlank;
    }
    WorldSaveData identity;
    check("ROCK30/v29-reopen-first-unexplored-chunk-" + std::to_string(site.seed),
          initialized && saved && (cx != 0 || cz != 0) && absentBeforeSave && absentAfterSave &&
          absentAfterReopen && trulyDifferent && futureV29 && editedChunk &&
          WorldSave(directory).load(identity) && identity.terrainGenerationVersion == WorkshopCourtyardTerrainGenerationVersion,
          "chunk=" + std::to_string(cx) + "/" + std::to_string(cz) +
          " absentBeforeSave/afterSave/afterReopen=" + std::to_string(absentBeforeSave) + "/" +
          std::to_string(absentAfterSave) + "/" + std::to_string(absentAfterReopen) +
          " differsFromV30=" + std::to_string(trulyDifferent));
}

void rockV30CheckSpawn(Camera &camera, Config &config, int seed)
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", std::to_string(seed));
    const auto directory = freshSaveDirectory("rock30_spawn_" + std::to_string(seed));
    Player player;
    World world(camera, config, player, directory, false, 1);
    const auto spawn = world.getPlayerSpawnPoint();
    const int x = World::toBlockCoord(spawn.x), y = World::toBlockCoord(spawn.y), z = World::toBlockCoord(spawn.z);
    const auto floor = world.getBlock(x, y - 2, z);
    check("ROCK30/default-new-world-safe-real-spawn-" + std::to_string(seed),
          world.getChunkManager().getTerrainGenerationVersion() == RockLandmarkTerrainGenerationVersion &&
          floor != BlockId::Water && floor.getData().isCollidable &&
          world.getBlock(x, y - 1, z) == BlockId::Air && world.getBlock(x, y, z) == BlockId::Air,
          vecToString(spawn));
    const auto origin = World::getChunkXZ(x, z);
    ClassicOverWorldGenerator generator(seed, world.getChunkManager().getTerrainGenerationVersion());
    AdventureEcologyPlanner ecology(seed, RockLandmarkTerrainGenerationVersion);
    bool supported = true;
    int wood = 0, seeds = 0, roots = 0, resourceRadius = 0;
    for (int radius = 0; radius <= 16 && (wood == 0 || seeds == 0 || roots == 0); ++radius) {
        resourceRadius = radius;
        for (int dx = -radius; dx <= radius; ++dx)
            for (int dz = -radius; dz <= radius; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != radius) continue;
                Chunk chunk(world, {origin.x + dx, origin.z + dz}, false);
                generator.generateTerrainFor(chunk);
                for (int lx = 0; lx < CHUNK_SIZE; ++lx)
                    for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                        const int wx = (origin.x + dx) * CHUNK_SIZE + lx;
                        const int wz = (origin.z + dz) * CHUNK_SIZE + lz;
                        const int surface = generator.getSurfaceHeightAtWorld(wx, wz);
                        const auto root = chunk.getBlock(lx, surface + 1, lz);
                        if (ecology.treeAnchor(wx, wz) &&
                            root.id == static_cast<Block_t>(BlockId::OakBark)) {
                            ++roots;
                            const auto ground = chunk.getBlock(lx, surface, lz);
                            supported &= ground != BlockId::Water && ground.getData().isCollidable;
                        }
                        for (int by = 0; by < 192; ++by) {
                            const auto block = chunk.getBlock(lx, by, lz);
                            wood += block.id == static_cast<Block_t>(BlockId::OakBark);
                            const auto id = static_cast<BlockId>(block.id);
                            seeds += id == BlockId::TallGrass && block.metadata >= BlockMetadata::TallGrass::Mature;
                            if (id == BlockId::TallGrass || id == BlockId::Rose || id == BlockId::DeadShrub)
                                supported &= by > 0 && AdventureEcologyPlanner::supportsPlant(
                                    static_cast<BlockId>(chunk.getBlock(lx, by - 1, lz).id), id);
                        }
                    }
            }
    }
    check("ROCK30/normal-spawn-nearby-wood-and-seed-source-" + std::to_string(seed),
          wood > 0 && seeds > 0 && roots > 0 && supported,
          "resourceChunkRadius=" + std::to_string(resourceRadius) +
          " wood/matureGrass/actualRoots=" + std::to_string(wood) + "/" +
          std::to_string(seeds) + "/" + std::to_string(roots));
}

std::vector<glm::ivec2> rockV30StructureLocations(const StructureFootprint &area)
{
    std::vector<glm::ivec2> locations;
    for (int x = WorldCoordinates::floorDiv(area.minimumX, CHUNK_SIZE);
         x <= WorldCoordinates::floorDiv(area.maximumX, CHUNK_SIZE); ++x)
        for (int z = WorldCoordinates::floorDiv(area.minimumZ, CHUNK_SIZE);
             z <= WorldCoordinates::floorDiv(area.maximumZ, CHUNK_SIZE); ++z)
            locations.emplace_back(x, z);
    return locations;
}

std::vector<GeneratedStructureChunkSample> rockV30LandmarkSamples(const RockV30Region &region)
{
    std::vector<GeneratedStructureChunkSample> samples;
    for (const auto &entry : region.chunks) {
        GeneratedStructureChunkSample sample;
        sample.x = entry.first[0]; sample.z = entry.first[1];
        std::vector<BlockMetadata_t> metadata;
        entry.second->collectBlockData(sample.blocks, metadata);
        sample.blockEntities = entry.second->getBlockEntities();
        samples.push_back(std::move(sample));
    }
    return samples;
}

void rockV30CheckLandmarkPlan(World &world, ClassicOverWorldGenerator &generator,
                             const StructurePlanSnapshot &plan, const RockV30Site &rock, bool nearRock)
{
    const int type = static_cast<int>(plan.key.type), layout = LandmarkExpedition::layoutFor(plan);
    const std::string label = "ROCK30/LANDMARK/" + std::to_string(rock.seed) + "-" +
        std::to_string(type) + "-" + std::to_string(layout);
    const auto &footprint = plan.footprint;
    auto area = footprint;
    area.minimumX -= 8; area.maximumX += 8;
    area.minimumZ -= 8; area.maximumZ += 8;
    if (nearRock) {
        area.minimumX = std::min(area.minimumX, rock.x - 8);
        area.maximumX = std::max(area.maximumX, rock.x + 8);
        area.minimumZ = std::min(area.minimumZ, rock.z - 8);
        area.maximumZ = std::max(area.maximumZ, rock.z + 8);
    }
    const auto locations = rockV30StructureLocations(area);
    RockV30Region forward(world, generator, locations);
    ClassicOverWorldGenerator reversed(rock.seed, RockLandmarkTerrainGenerationVersion);
    RockV30Region reverse(world, reversed, locations, true);
    const auto samples = rockV30LandmarkSamples(forward);
    std::array<int, static_cast<int>(BlockId::NUM_TYPES)> counts{};
    bool complete = true;
    for (int x = footprint.minimumX; x <= footprint.maximumX; ++x)
        for (int z = footprint.minimumZ; z <= footprint.maximumZ; ++z)
            for (int y = footprint.minimumY; y <= footprint.maximumY; ++y) {
                const auto id = static_cast<BlockId>(forward.block(x, y, z).id);
                complete &= id < BlockId::NUM_TYPES;
                if (id < BlockId::NUM_TYPES) ++counts[static_cast<int>(id)];
            }
    check(label + "-complete-region-block-metadata-entity-order",
          complete && forward.chunks.size() == locations.size() && rockV30SameRegionBytes(forward, reverse),
          "chunks=" + std::to_string(locations.size()) + " nearRock=" + std::to_string(nearRock));
    const auto loot = structureLootForPlan(plan, ExplorationRewards::CurrentVersion);
    const bool resources = counts[int(BlockId::Chest)] == (type == 0 ? 0 : 1) &&
        counts[int(BlockId::WaystoneCore)] == (type == 0 ? 1 : 0) &&
        counts[int(BlockId::IronOre)] == (type == 0 ? 1 : type == 1 ? 4 : 0) &&
        counts[int(BlockId::CoalOre)] == (type == 0 ? 4 : type == 2 ? 1 : 0);
    const glm::ivec3 interaction = type == 0
        ? glm::ivec3(plan.anchor.x, plan.anchor.y + 3, plan.anchor.z) : plan.chestPosition;
    const auto interactionId = static_cast<BlockId>(forward.block(interaction.x, interaction.y, interaction.z).id);
    check(label + "-actual-interaction-and-reward-entity",
          generator.getGenerationVersion() == RockLandmarkTerrainGenerationVersion && resources &&
          interactionId == (type == 0 ? BlockId::WaystoneCore : BlockId::Chest) &&
          plan.hasChest == (type != 0) &&
          (type == 0 ? !loot.valid : generatedChestMatchesLoot(samples, plan, loot)));
    const auto reached = landmarkWalkableCells(samples, plan);
    const int targetZ = plan.anchor.z + (type == 2 ? 0 : -1);
    const LandmarkPoint target{plan.anchor.x, plan.anchor.y + 1, targetZ};
    const bool physicalFloors = std::all_of(reached.begin(), reached.end(), [&](const auto &p) {
        const auto floor = forward.block(std::get<0>(p), std::get<1>(p), std::get<2>(p));
        return floor != BlockId::NUM_TYPES && floor != BlockId::Water && floor.getData().isCollidable;
    });
    check(label + "-actual-two-headroom-entrance-to-interaction",
          reached.count(target) != 0 && physicalFloors &&
          std::abs(interaction.x - plan.anchor.x) + std::abs(interaction.z - targetZ) <= 1 &&
          std::abs(interaction.y - (plan.anchor.y + 2)) <= 1,
          "reached=" + std::to_string(reached.size()));
    if (type == 2 && layout == 1) {
        const int side = (plan.selectionHash & 1ull) ? -1 : 1;
        check(label + "-actual-lookout-route",
              reached.count({plan.anchor.x + side * 4, plan.anchor.y + 4, plan.anchor.z - 1}) != 0);
    }
    if (type == 1) {
        bool oresReachable = true;
        for (int ox : {-2, 2}) for (int oz : {-2, 2}) {
            const bool near = std::any_of(reached.begin(), reached.end(), [&](const auto &p) {
                return std::abs(std::get<0>(p) - (plan.anchor.x + ox)) +
                    std::abs(std::get<2>(p) - (plan.anchor.z + oz)) <= 1 &&
                    std::abs(std::get<1>(p) - (plan.anchor.y + 1)) <= 1;
            });
            oresReachable &= near;
        }
        check(label + "-actual-route-to-four-reward-ores", oresReachable);
    }
    std::cout << "[ROCK30_LANDMARK] seed=" << rock.seed << " type=" << type << " layout=" << layout
              << " cell=" << plan.key.cellX << ',' << plan.key.cellZ << " anchor=" << vecToString(plan.anchor)
              << " nearRock=" << nearRock << " reached=" << reached.size() << '\n';
    if (rock.seed == 42 && type == 2 && layout == 0) {
        // Modify detached actual blocks, then use the same path oracle.
        // Missing chunks retain the rejection sentinel during the path test.
        const glm::ivec3 head{plan.anchor.x, plan.anchor.y + 3, targetZ};
        const auto original = forward.block(head.x, head.y, head.z);
        const bool lowered = rockV30SetRegionBlock(forward, head, BlockId::Stone);
        check("ROCK30/LANDMARK/reject-low-headroom",
              lowered && landmarkWalkableCells(rockV30LandmarkSamples(forward), plan).count(target) == 0);
        rockV30SetRegionBlock(forward, head, original);
        std::vector<std::pair<glm::ivec3, ChunkBlock>> originals;
        bool sealed = true;
        for (int x = footprint.minimumX; x <= footprint.maximumX; ++x)
            for (int y = plan.anchor.y; y <= footprint.maximumY + 2; ++y) {
                const glm::ivec3 p{x, y, footprint.minimumZ};
                originals.push_back({p, forward.block(p.x, p.y, p.z)});
                sealed &= rockV30SetRegionBlock(forward, p, BlockId::Stone);
            }
        check("ROCK30/LANDMARK/reject-sealed-entrance",
              sealed && landmarkWalkableCells(rockV30LandmarkSamples(forward), plan).empty());
        for (const auto &entry : originals) rockV30SetRegionBlock(forward, entry.first, entry.second);
        const auto targetChunk = World::getChunkXZ(plan.anchor.x, targetZ);
        forward.chunks.erase({targetChunk.x, targetChunk.z});
        const auto missing = rockV30LandmarkSamples(forward);
        check("ROCK30/LANDMARK/reject-missing-region",
              forward.block(plan.anchor.x, plan.anchor.y + 1, targetZ) == BlockId::NUM_TYPES &&
              generatedLandmarkBlock(missing, plan.anchor.x, plan.anchor.y + 1, targetZ) == BlockId::NUM_TYPES &&
              landmarkWalkableCells(missing, plan).count(target) == 0);
    }
}

void rockV30CheckLandmarks(World &world, const RockV30Site &rock)
{
    ClassicOverWorldGenerator generator(rock.seed, RockLandmarkTerrainGenerationVersion);
    std::array<std::array<StructurePlanSnapshot, 2>, 3> layouts{};
    StructurePlanSnapshot closest;
    std::int64_t closestDistance = std::numeric_limits<std::int64_t>::max();
    const auto center = World::getChunkXZ(rock.x, rock.z);
    std::set<std::array<int, 3>> seen;
    for (int dx = -4; dx <= 4; ++dx) for (int dz = -4; dz <= 4; ++dz)
        for (const auto &plan : generator.getStructurePlansForChunk(center.x + dx, center.z + dz)) {
            if (!plan.valid || !plan.footprint.valid() ||
                !seen.insert({static_cast<int>(plan.key.type), plan.key.cellX, plan.key.cellZ}).second) continue;
            const std::int64_t x = static_cast<std::int64_t>(plan.anchor.x) - rock.x;
            const std::int64_t z = static_cast<std::int64_t>(plan.anchor.z) - rock.z;
            if (x * x + z * z < closestDistance) {
                closest = plan; closestDistance = x * x + z * z;
            }
        }
    check("ROCK30/LANDMARK/near-pillar-selected-plan-" + std::to_string(rock.seed),
          rock.seed != 20260807 || closest.valid,
          "nearbyPlans=" + std::to_string(seen.size()) + " closestDistance=" +
          (closest.valid ? std::to_string(std::sqrt(static_cast<double>(closestDistance))) : "none"));
    if (closest.valid)
        layouts[static_cast<int>(closest.key.type)][LandmarkExpedition::layoutFor(closest)] = closest;
    bool all = false;
    for (int radius = 0; radius <= 24 && !all; ++radius) {
        for (int x = -radius; x <= radius; ++x) for (int z = -radius; z <= radius; ++z) {
            if (std::max(std::abs(x), std::abs(z)) != radius) continue;
            for (int type = 0; type < 3; ++type) {
                const auto plan = generator.getStructurePlanForCell(static_cast<StructureType>(type), x, z);
                if (!plan.valid || !plan.footprint.valid()) continue;
                auto &selected = layouts[type][LandmarkExpedition::layoutFor(plan)];
                if (selected.valid) continue;
                const auto projected = generator.getStructurePlansForChunk(
                    WorldCoordinates::floorDiv(plan.anchor.x, CHUNK_SIZE),
                    WorldCoordinates::floorDiv(plan.anchor.z, CHUNK_SIZE));
                if (std::any_of(projected.begin(), projected.end(), [&](const auto &candidate) {
                        return candidate.key == plan.key;
                    })) selected = plan;
            }
        }
        all = true;
        for (const auto &pair : layouts) all &= pair[0].valid && pair[1].valid;
    }
    check("ROCK30/LANDMARK/six-current-real-layouts-" + std::to_string(rock.seed), all);
    if (!all) return;
    for (const auto &pair : layouts) for (const auto &plan : pair)
        rockV30CheckLandmarkPlan(world, generator, plan, rock, closest.valid && plan.key == closest.key);
}

struct RockV30UndergroundStep {
    glm::ivec3 position{0};
    int directionX = 0, directionZ = 0;
};

std::vector<RockV30UndergroundStep> rockV30UndergroundRoute(
    const CaveGenerator::AdventureUndergroundPlan &plan)
{
    std::vector<RockV30UndergroundStep> route;
    // These are the existing entrance/chamber/rift route coordinates, sampled
    // from the current plan. The oracle below inspects their actual solids,
    // water and clearance, rather than predicting the projected block IDs.
    for (int step = 0; step <= 29; ++step)
        route.push_back({{plan.entrance.anchorX + plan.entrance.directionX * step,
            plan.entrance.anchorY - std::min(step, 24) * 3 / 4,
            plan.entrance.anchorZ + plan.entrance.directionZ * step},
            plan.entrance.directionX, plan.entrance.directionZ});
    for (int step = 1; step <= 29; ++step)
        route.push_back({{plan.chamberX + plan.riftDirectionX * step,
            plan.chamberAirY - (plan.chamberAirY - plan.riftEndAirY) * std::max(0, step - 6) / 23,
            plan.chamberZ + plan.riftDirectionZ * step}, plan.riftDirectionX, plan.riftDirectionZ});
    return route;
}

bool rockV30DryUndergroundRoute(const RockV30Region &region,
                               const CaveGenerator::AdventureUndergroundPlan &plan,
                               glm::ivec3 &firstFailure)
{
    const auto route = rockV30UndergroundRoute(plan);
    bool valid = plan.valid && plan.entrance.valid && route.size() == 59 &&
        route.back().position == glm::ivec3(plan.destinationX, plan.riftEndAirY, plan.destinationZ);
    for (std::size_t index = 0; index < route.size(); ++index) {
        const auto &step = route[index];
        if (index > 0) {
            const auto &previous = route[index - 1].position;
            const bool continuous = std::abs(step.position.x - previous.x) +
                std::abs(step.position.z - previous.z) == 1 && std::abs(step.position.y - previous.y) <= 1;
            if (!continuous && valid) firstFailure = step.position;
            valid &= continuous;
        }
        for (int side = -1; side <= 1; ++side) {
            const int x = step.position.x - step.directionZ * side;
            const int z = step.position.z + step.directionX * side;
            const auto floor = region.block(x, step.position.y - 1, z);
            bool open = floor != BlockId::NUM_TYPES && floor != BlockId::Water && floor.getData().isCollidable;
            for (int height = 0; height < 3; ++height)
                open &= region.block(x, step.position.y + height, z) == BlockId::Air;
            if (!open && valid) firstFailure = {x, step.position.y, z};
            valid &= open;
        }
    }
    return valid;
}

bool rockV30AttachedOutcrop(const RockV30Region &region,
                           const CaveGenerator::AdventureUndergroundPlan &plan, int oreX, int oreZ)
{
    bool attached = true;
    for (const auto sample : {std::pair<int, BlockId>{3, BlockId::CoalOre}, {2, BlockId::IronOre}}) {
        const int y = plan.chamberAirY + sample.first;
        bool air = false, wall = false;
        for (const glm::ivec3 d : {glm::ivec3(1, 0, 0), {-1, 0, 0}, {0, 1, 0},
                                  {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}) {
            const auto block = region.block(oreX + d.x, y + d.y, oreZ + d.z);
            const auto id = static_cast<BlockId>(block.id);
            air |= id == BlockId::Air;
            wall |= id < BlockId::NUM_TYPES && id != BlockId::Water &&
                id != BlockId::CoalOre && id != BlockId::IronOre && block.getData().isCollidable;
        }
        attached &= region.block(oreX, y, oreZ) == sample.second && air && wall;
    }
    return attached;
}

bool rockV30SupportStone(ChunkBlock block)
{
    const auto id = static_cast<BlockId>(block.id);
    const bool stone = id == BlockId::Stone || id == BlockId::Cobblestone || id == BlockId::MossStone ||
        id == BlockId::Gravel || id == BlockId::CoalOre || id == BlockId::IronOre;
    return stone && block.getData().isCollidable;
}

struct RockV30OutcropSupport {
    bool dryFloor = false, coal = false, iron = false;
    std::size_t visited = 0;
    Block_t floorId = static_cast<Block_t>(BlockId::NUM_TYPES);
    bool connected() const { return dryFloor && coal && iron; }
};

RockV30OutcropSupport rockV30OutcropGroundConnection(const RockV30Region &region,
    const CaveGenerator::AdventureUndergroundPlan &plan, int oreX, int oreZ)
{
    RockV30OutcropSupport result;
    const std::array<int, 3> floorPoint{plan.chamberX, plan.chamberAirY - 1, plan.chamberZ};
    const auto floor = region.block(floorPoint[0], floorPoint[1], floorPoint[2]);
    result.floorId = floor.id;
    result.dryFloor = floor != BlockId::NUM_TYPES && floor != BlockId::Water && floor.getData().isCollidable;
    for (int height = 0; height < 3; ++height)
        result.dryFloor &= region.block(plan.chamberX, plan.chamberAirY + height, plan.chamberZ) == BlockId::Air;
    if (!result.dryFloor) return result;
    const int minimumY = std::max(0, plan.chamberAirY - CaveGenerator::AdventureChamberVerticalRadius);
    const int maximumY = std::min(255, plan.chamberAirY + 3 + CaveGenerator::AdventureChamberVerticalRadius);
    const std::array<int, 3> coalPoint{oreX, plan.chamberAirY + 3, oreZ};
    const std::array<int, 3> ironPoint{oreX, plan.chamberAirY + 2, oreZ};
    std::set<std::array<int, 3>> visited{floorPoint};
    std::vector<std::array<int, 3>> pending{floorPoint};
    // Face contact alone can attach an ore to another floating stone. Follow
    // actual stone/ore faces to the observed dry-route floor instead. The
    // finite region and chamber Y bounds also reject any missing-chunk path.
    for (std::size_t index = 0; index < pending.size(); ++index) {
        const auto p = pending[index];
        result.coal |= p == coalPoint;
        result.iron |= p == ironPoint;
        if (result.coal && result.iron) break;
        for (int axis = 0; axis < 3; ++axis) for (int direction : {-1, 1}) {
            auto q = p; q[axis] += direction;
            if (q[0] < region.minimumX || q[0] > region.maximumX ||
                q[2] < region.minimumZ || q[2] > region.maximumZ ||
                q[1] < minimumY || q[1] > maximumY || visited.count(q) != 0 ||
                !rockV30SupportStone(region.block(q[0], q[1], q[2]))) continue;
            visited.insert(q); pending.push_back(q);
        }
    }
    result.visited = visited.size();
    return result;
}

int rockV30CheckUnderground(World &world, const glm::ivec3 &fixture)
{
    const int seed = fixture.x;
    const std::string label = "ROCK30/UNDERGROUND/" + std::to_string(seed);
    ClassicOverWorldGenerator generator(seed, RockLandmarkTerrainGenerationVersion);
    CaveGenerator caves(seed, RockLandmarkTerrainGenerationVersion);
    const auto plan = caves.getAdventureUndergroundPlanForCell(fixture.y, fixture.z,
        [&](int x, int z) { return generator.getSurfaceHeightAtWorld(x, z); },
        [&](int x, int z) { return generator.getBiomeAtWorld(x, z); });
    check(label + "-current-frozen-cell-plan-valid", plan.valid && plan.entrance.valid &&
          generator.getGenerationVersion() == RockLandmarkTerrainGenerationVersion,
          "cell=" + std::to_string(fixture.y) + "/" + std::to_string(fixture.z));
    if (!plan.valid || !plan.entrance.valid) return -1;
    const auto locations = adventureUndergroundLocations(plan);
    RockV30Region forward(world, generator, locations);
    ClassicOverWorldGenerator reversed(seed, RockLandmarkTerrainGenerationVersion);
    RockV30Region reverse(world, reversed, locations, true);
    check(label + "-complete-region-coordinate-block-metadata-entity-order",
          !locations.empty() && forward.chunks.size() == locations.size() &&
          rockV30SameRegionBytes(forward, reverse), "chunks=" + std::to_string(locations.size()));
    glm::ivec3 firstFailure{0};
    const bool dryRoute = rockV30DryUndergroundRoute(forward, plan, firstFailure);
    check(label + "-actual-dry-three-wide-three-high-entry-chamber-destination-route",
          dryRoute, dryRoute ? "routeSteps=59" : "firstFailure=" + vecToString(firstFailure) + " floor/body/head=" +
          std::to_string(forward.block(firstFailure.x, firstFailure.y - 1, firstFailure.z).id) + "/" +
          std::to_string(forward.block(firstFailure.x, firstFailure.y, firstFailure.z).id) + "/" +
          std::to_string(forward.block(firstFailure.x, firstFailure.y + 2, firstFailure.z).id));
    const int px = -plan.riftDirectionZ, pz = plan.riftDirectionX;
    const bool mine = plan.layout == CaveGenerator::AdventureUndergroundLayout::MinerCache;
    const auto room = [&](int along, int height, int lateral) {
        return forward.block(plan.destinationX + plan.riftDirectionX * along + px * lateral,
            plan.riftEndAirY + height, plan.destinationZ + plan.riftDirectionZ * along + pz * lateral);
    };
    int wood = 0, chests = 0;
    for (int along = -5; along <= 5; ++along)
        for (int side = -5; side <= 5; ++side)
            for (int height = -1; height <= 6; ++height) {
                const auto id = static_cast<BlockId>(room(along, height, side).id);
                wood += id == BlockId::OakBark || id == BlockId::OakPlank;
                chests += id == BlockId::Chest;
            }
    int resourceIndex = 0, exposed = 0, coal = 0, iron = 0;
    bool resources = true;
    if (mine) resources = wood >= 30 && chests == 1 && room(2, 0, 3) == BlockId::Chest;
    else {
        for (int height = 1; height <= 2; ++height)
            for (int along = -2; along <= 2 && resourceIndex < 9; ++along) {
                const auto expected = resourceIndex++ < 6 ? BlockId::CoalOre : BlockId::IronOre;
                const auto actual = room(along, height, 5);
                resources &= actual == expected && room(along, height, 4) == BlockId::Air;
                coal += actual == BlockId::CoalOre;
                iron += actual == BlockId::IronOre;
                exposed += rockV30Stone(actual) && room(along, height, 4) == BlockId::Air;
            }
        resources &= coal == 6 && iron == 3 && exposed == 9 && chests == 0;
    }
    bool torches = true;
    for (int side : {-1, 1}) {
        const int along = mine ? -2 : -3, lateral = side * (mine ? 3 : 2);
        const auto support = room(along, -1, lateral);
        torches &= room(along, 0, lateral) == BlockId::Torch &&
            support != BlockId::NUM_TYPES && support != BlockId::Water && support.getData().isCollidable;
    }
    check(label + "-actual-destination-resources-and-supported-torches", resources && torches,
          "layout=" + std::to_string(static_cast<int>(plan.layout)) + " wood/chest/coal/iron/exposed=" +
          std::to_string(wood) + "/" + std::to_string(chests) + "/" + std::to_string(coal) + "/" +
          std::to_string(iron) + "/" + std::to_string(exposed));
    const glm::ivec3 chestPosition{plan.destinationX + plan.riftDirectionX * 2 + px * 3,
        plan.riftEndAirY, plan.destinationZ + plan.riftDirectionZ * 2 + pz * 3};
    int chestEntities = 0;
    bool entitiesMatch = true, loot = !mine;
    for (const auto &entry : forward.chunks)
        for (const auto &record : entry.second->getBlockEntities()) {
            if (record.type != ChestContainer::BlockEntityType) continue;
            const glm::ivec3 position{entry.first[0] * CHUNK_SIZE + record.position.x, record.position.y,
                                     entry.first[1] * CHUNK_SIZE + record.position.z};
            entitiesMatch &= forward.block(position.x, position.y, position.z) == BlockId::Chest;
            const int dx = position.x - plan.destinationX, dz = position.z - plan.destinationZ;
            const int along = dx * plan.riftDirectionX + dz * plan.riftDirectionZ;
            const int lateral = dx * px + dz * pz;
            if (std::abs(along) > 5 || std::abs(lateral) > 5 ||
                position.y < plan.riftEndAirY - 1 || position.y > plan.riftEndAirY + 6) continue;
            ++chestEntities;
            ContainerInventory inventory(ChestContainer::SlotCount);
            int occupied = 0;
            const bool decoded = ContainerInventory::deserialize(record.payload, inventory);
            for (int slot = 0; slot < inventory.getSlotCount(); ++slot)
                occupied += inventory.getSlot(slot).amount > 0;
            loot = mine && position == chestPosition && decoded && occupied == 3 &&
                inventory.count(Material::ID::Torch) == 4 && inventory.count(Material::ID::OakPlank) == 4 &&
                inventory.count(Material::ID::Bread) == 2;
        }
    check(label + "-actual-chest-block-entity-and-legal-reward", entitiesMatch && loot &&
          chestEntities == (mine ? 1 : 0));
    int outcropX = -plan.poolDirectionX, outcropZ = -plan.poolDirectionZ;
    if (outcropX == plan.riftDirectionX && outcropZ == plan.riftDirectionZ) {
        outcropX = plan.entrance.directionX; outcropZ = plan.entrance.directionZ;
    }
    const int outcropDistance = std::abs(outcropX) == std::abs(plan.entrance.directionX) &&
        std::abs(outcropZ) == std::abs(plan.entrance.directionZ)
        ? CaveGenerator::AdventureChamberAlongRadius : CaveGenerator::AdventureChamberPerpendicularRadius;
    const int oldOreX = plan.chamberX + outcropX * outcropDistance;
    const int oldOreZ = plan.chamberZ + outcropZ * outcropDistance;
    const int oreX = plan.chamberX + outcropX * (outcropDistance - 2);
    const int oreZ = plan.chamberZ + outcropZ * (outcropDistance - 2);
    const bool attached = rockV30AttachedOutcrop(forward, plan, oreX, oreZ);
    const auto groundSupport = rockV30OutcropGroundConnection(forward, plan, oreX, oreZ);
    ClassicOverWorldGenerator prior(seed, WorkshopCourtyardTerrainGenerationVersion);
    CaveGenerator priorCaves(seed, WorkshopCourtyardTerrainGenerationVersion);
    const auto priorPlan = priorCaves.getAdventureUndergroundPlanForCell(fixture.y, fixture.z,
        [&](int x, int z) { return prior.getSurfaceHeightAtWorld(x, z); },
        [&](int x, int z) { return prior.getBiomeAtWorld(x, z); });
    RockV30Region historical(world, prior, locations);
    const auto historicalSupport = rockV30OutcropGroundConnection(historical, plan, oldOreX, oldOreZ);
    const auto oreCounts = [](const RockV30Region &region) {
        std::array<int, 2> counts{};
        for (const auto &entry : region.chunks)
            for (int x = 0; x < CHUNK_SIZE; ++x)
                for (int z = 0; z < CHUNK_SIZE; ++z)
                    for (int y = 0; y < 256; ++y) {
                        const auto block = entry.second->getBlock(x, y, z);
                        counts[0] += block == BlockId::CoalOre;
                        counts[1] += block == BlockId::IronOre;
                    }
        return counts;
    };
    const auto currentOres = oreCounts(forward), historicalOres = oreCounts(historical);
    check(label + "-original-outcrop-pair-cleared",
          forward.block(oldOreX, plan.chamberAirY + 3, oldOreZ) == BlockId::Air &&
          forward.block(oldOreX, plan.chamberAirY + 2, oldOreZ) == BlockId::Air);
    check(label + "-complete-region-ore-counts-match-v29",
          sameAdventureUndergroundPlan(plan, priorPlan) && forward.chunks.size() == locations.size() &&
          historical.chunks.size() == locations.size() && currentOres == historicalOres,
          "coal/iron-v30=" + std::to_string(currentOres[0]) + "/" + std::to_string(currentOres[1]) +
          " v29=" + std::to_string(historicalOres[0]) + "/" + std::to_string(historicalOres[1]));
    std::string outcropDetail;
    if (!attached || !groundSupport.connected() || seed == 20260807) {
        const auto describe = [&](const RockV30Region &region, const char *version, int observedX, int observedZ) {
            std::ostringstream out;
            out << version;
            for (int height : {3, 2}) {
                const int y = plan.chamberAirY + height;
                const auto ore = region.block(observedX, y, observedZ);
                out << " ore=" << observedX << ',' << y << ',' << observedZ << ':' << int(ore.id) << ':' << int(ore.metadata)
                    << " neighbours(+x,-x,+y,-y,+z,-z)=";
                bool air = false, wall = false;
                for (const glm::ivec3 d : {glm::ivec3(1, 0, 0), {-1, 0, 0}, {0, 1, 0},
                                          {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}) {
                    const auto block = region.block(observedX + d.x, y + d.y, observedZ + d.z);
                    const auto id = static_cast<BlockId>(block.id);
                    air |= id == BlockId::Air;
                    wall |= id < BlockId::NUM_TYPES && id != BlockId::Water &&
                        id != BlockId::CoalOre && id != BlockId::IronOre && block.getData().isCollidable;
                    out << int(block.id) << ':' << int(block.metadata) << ',';
                }
                out << " air=" << air << " wall=" << wall;
            }
            return out.str();
        };
        outcropDetail = describe(forward, "v30", oreX, oreZ) + " | " + describe(historical, "v29", oldOreX, oldOreZ) +
            " | v29SamePlan=" + std::to_string(sameAdventureUndergroundPlan(plan, priorPlan)) +
            " v29SameCompleteRegionBytes=" + std::to_string(rockV30SameRegionBytes(forward, historical)) +
            " v30/v29GroundConnected=" + std::to_string(groundSupport.connected()) + "/" +
            std::to_string(historicalSupport.connected());
        std::cout << "[ROCK30_OUTCROP_DIAG] seed=" << seed << ' ' << outcropDetail << '\n';
        if (seed == 20260807) {
            // Frozen r4 actual failure: this legacy pair touches only air and
            // the other mineral. Keep it as a rejection example; a v30 repair
            // must not rewrite the existing v29 world or pretend it was safe.
            bool floatingPair = oldOreX == -466 && oldOreZ == -488 && plan.chamberAirY == 81;
            for (const auto sample : {std::pair<int, BlockId>{3, BlockId::CoalOre}, {2, BlockId::IronOre}}) {
                const int y = plan.chamberAirY + sample.first;
                floatingPair &= historical.block(oldOreX, y, oldOreZ) == sample.second;
                for (const glm::ivec3 d : {glm::ivec3(1, 0, 0), {-1, 0, 0}, {0, 1, 0},
                                          {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}) {
                    const bool counterpart = d.y == (sample.first == 3 ? -1 : 1);
                    const auto expected = counterpart
                        ? (sample.first == 3 ? BlockId::IronOre : BlockId::CoalOre) : BlockId::Air;
                    floatingPair &= historical.block(oldOreX + d.x, y + d.y, oldOreZ + d.z) == expected;
                }
            }
            check(label + "-reject-frozen-v29-floating-outcrop",
                  priorPlan.valid && sameAdventureUndergroundPlan(plan, priorPlan) && floatingPair &&
                  historicalSupport.dryFloor && !historicalSupport.connected() &&
                  !rockV30AttachedOutcrop(historical, plan, oldOreX, oldOreZ));
            for (const auto position : {glm::ivec3(-471, 83, -482), {-471, 84, -482}, {-470, 83, -482},
                                        {-469, 83, -482}, {-469, 83, -483}, {-472, 83, -483}, {-472, 84, -483}}) {
                const auto cameraCells = [&](const RockV30Region &region) {
                    std::ostringstream out;
                    for (int height : {-2, -1, 0})
                        out << int(region.block(position.x, position.y + height, position.z).id) << ',';
                    return out.str();
                };
                std::cout << "[ROCK30_OUTCROP_CAMERA] seed=" << seed << " eye=" << vecToString(position)
                          << " floor/body/eye-v29=" << cameraCells(historical)
                          << " floor/body/eye-v30=" << cameraCells(forward) << '\n';
            }
        }
    }
    check(label + "-actual-accessible-wall-attached-chamber-outcrop", attached, outcropDetail);
    check(label + "-actual-six-neighbour-outcrop-connected-to-dry-floor", groundSupport.connected(),
          "floor=" + vecToString(glm::ivec3(plan.chamberX, plan.chamberAirY - 1, plan.chamberZ)) +
          " floorId=" + std::to_string(groundSupport.floorId) +
          " visited=" + std::to_string(groundSupport.visited) + " coal/ironReached=" +
          std::to_string(groundSupport.coal) + "/" + std::to_string(groundSupport.iron));
    if (seed == 20260807) {
        std::vector<std::pair<glm::ivec3, ChunkBlock>> originals;
        bool contactStones = true, removed = true;
        // Both ores contact the back rib; the lower ore also contacts the
        // Stone directly below it. Remove all three observed contacts so a
        // mineral pair alone cannot satisfy either attachment oracle.
        for (const auto position : {
            glm::ivec3(oreX + outcropX, plan.chamberAirY + 3, oreZ + outcropZ),
            glm::ivec3(oreX + outcropX, plan.chamberAirY + 2, oreZ + outcropZ),
            glm::ivec3(oreX, plan.chamberAirY + 1, oreZ)}) {
            const auto original = forward.block(position.x, position.y, position.z);
            originals.push_back({position, original});
            contactStones &= original == BlockId::Stone;
            removed &= rockV30SetRegionBlock(forward, position, BlockId::Air);
        }
        check(label + "-reject-removed-ore-contact-stones",
              attached && groundSupport.connected() && contactStones && removed &&
              !rockV30AttachedOutcrop(forward, plan, oreX, oreZ) &&
              !rockV30OutcropGroundConnection(forward, plan, oreX, oreZ).connected());
        for (const auto &entry : originals) rockV30SetRegionBlock(forward, entry.first, entry.second);
    }
    std::cout << "[ROCK30_UNDERGROUND] seed=" << seed << " layout=" << int(plan.layout)
              << " entrance=" << plan.entrance.anchorX << ',' << plan.entrance.anchorY << ',' << plan.entrance.anchorZ
              << " chamber=" << plan.chamberX << ',' << plan.chamberAirY << ',' << plan.chamberZ
              << " destination=" << plan.destinationX << ',' << plan.riftEndAirY << ',' << plan.destinationZ << '\n';
    if (seed == 42) {
        const glm::ivec3 head{plan.entrance.anchorX, plan.entrance.anchorY + 1, plan.entrance.anchorZ};
        const auto original = forward.block(head.x, head.y, head.z);
        const bool lowered = rockV30SetRegionBlock(forward, head, BlockId::Stone);
        check("ROCK30/UNDERGROUND/reject-low-headroom", lowered &&
              !rockV30DryUndergroundRoute(forward, plan, firstFailure));
        rockV30SetRegionBlock(forward, head, original);
        std::vector<std::pair<glm::ivec3, ChunkBlock>> originals;
        bool sealed = true;
        for (int side = -1; side <= 1; ++side) for (int height = 0; height < 3; ++height) {
            const glm::ivec3 p{plan.entrance.anchorX - plan.entrance.directionZ * side,
                plan.entrance.anchorY + height, plan.entrance.anchorZ + plan.entrance.directionX * side};
            originals.push_back({p, forward.block(p.x, p.y, p.z)});
            sealed &= rockV30SetRegionBlock(forward, p, BlockId::Stone);
        }
        check("ROCK30/UNDERGROUND/reject-sealed-entrance", sealed &&
              !rockV30DryUndergroundRoute(forward, plan, firstFailure));
        for (const auto &entry : originals) rockV30SetRegionBlock(forward, entry.first, entry.second);
        const auto entranceChunk = World::getChunkXZ(plan.entrance.anchorX, plan.entrance.anchorZ);
        forward.chunks.erase({entranceChunk.x, entranceChunk.z});
        check("ROCK30/UNDERGROUND/reject-missing-region", forward.block(plan.entrance.anchorX,
              plan.entrance.anchorY, plan.entrance.anchorZ) == BlockId::NUM_TYPES &&
              !rockV30DryUndergroundRoute(forward, plan, firstFailure));
    }
    return static_cast<int>(plan.layout);
}

void caseRockLandmarkV30()
{
    check("ROCK30/version-and-save-boundary",
          RockLandmarkTerrainGenerationVersion == 30 && WorkshopCourtyardTerrainGenerationVersion == 29 &&
          CurrentTerrainGenerationVersion == RockLandmarkTerrainGenerationVersion &&
          ClassicOverWorldGenerator().getGenerationVersion() == RockLandmarkTerrainGenerationVersion &&
          WorldSaveFormatVersion == 12);
    // Frozen from the production planner's dense-site survey, not calculated
    // here from its anchor or shape equations. Each real rock crosses a chunk
    // boundary, and the surrounding complete chunks also contain its bypass.
    const std::array<RockV30Site, 3> sites{{
        {42, -1312, -3808}, {20260807, -2464, -1312}, {239701883, -1568, 2784}
    }};
    setEnv("HELLOMINE3D_SEED", "42");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    Config config = makeConfig();
    Camera camera(config);
    {
        Player player;
        World world(camera, config, player, freshSaveDirectory("rock30_projection"), false, 0);
        for (const auto &site : sites) rockV30CheckSite(world, site);
        for (const auto &site : sites) rockV30CheckLandmarks(world, site);
        std::set<int> undergroundLayouts;
        for (const auto fixture : {glm::ivec3(42, 11, 2), glm::ivec3(20260807, -5, -5),
                                   glm::ivec3(239701883, 3, -6)}) {
            const int layout = rockV30CheckUnderground(world, fixture);
            if (layout >= 0) undergroundLayouts.insert(layout);
        }
        check("ROCK30/UNDERGROUND/both-current-destination-layouts", undergroundLayouts.size() == 2);
    }
    for (const auto &site : sites) rockV30CheckFutureV29(camera, config, site);
    for (const auto &site : sites) rockV30CheckSpawn(camera, config, site.seed);
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "");
}
}
