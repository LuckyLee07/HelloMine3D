#pragma once

#include <map>

namespace {

// These fixtures load production terrain through a real World/ChunkManager.
// Only the small, roofed test cavities are edited; no mask helper or replacement
// generator is used to decide the expected boundary pixels.
constexpr int CaveBoundarySectionY = 12;
constexpr int CaveBoundaryBaseY = CaveBoundarySectionY * CHUNK_SIZE;
constexpr int CaveBoundaryCenterX = -4;
constexpr int CaveBoundaryCenterZ = -4;

Config caveBoundaryConfig(int radius)
{
    Config config = makeConfig();
    config.renderDistance = radius;
    return config;
}

struct CaveBoundaryFixture {
    Config config;
    Camera camera;
    Player player;
    std::string directory;
    std::unique_ptr<World> world;

    CaveBoundaryFixture(const std::string &name, int radius = 1)
        : config(caveBoundaryConfig(radius)), camera(config),
          directory(freshSaveDirectory(name))
    {
        clearDeterministicEnv();
        setEnv("HELLOMINE3D_SEED", "42");
        setEnv("HELLOMINE3D_PLAYER_POSITION", "-56 200 -56");
        setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
        world = std::make_unique<World>(camera, config, player, directory, false, 0);
    }
};

glm::ivec3 caveBoundaryCell(int face, int y, int u)
{
    switch (face) {
    case 0: return {0, y, u};
    case 1: return {15, y, u};
    case 2: return {u, y, 0};
    default: return {u, y, 15};
    }
}

const WorldBoundaryMaskFace *caveBoundaryFind(const WorldMeshSnapshot &snapshot,
                                             glm::ivec3 location, int face)
{
    const auto found = std::find_if(snapshot.boundaryMasks.begin(), snapshot.boundaryMasks.end(),
        [&](const WorldBoundaryMaskFace &mask) { return mask.location == location && mask.face == face; });
    return found == snapshot.boundaryMasks.end() ? nullptr : &*found;
}

std::size_t caveBoundaryBitCount(const WorldBoundaryMaskFace &mask)
{
    std::size_t result = 0;
    for (std::uint16_t row : mask.rows)
        for (int bit = 0; bit < CHUNK_SIZE; ++bit) result += (row >> bit) & 1u;
    return result;
}

bool caveBoundarySameMasks(const WorldMeshSnapshot &a, const WorldMeshSnapshot &b)
{
    if (a.boundaryMasks.size() != b.boundaryMasks.size()) return false;
    for (std::size_t i = 0; i < a.boundaryMasks.size(); ++i) {
        const auto &x = a.boundaryMasks[i]; const auto &y = b.boundaryMasks[i];
        if (x.location != y.location || x.face != y.face || x.blockRevision != y.blockRevision ||
            x.incarnation != y.incarnation || x.rows != y.rows) return false;
    }
    return true;
}

bool caveBoundaryBudgetValid(const WorldMeshSnapshot &snapshot)
{
    return snapshot.boundaryMaskFacesScanned <= 8 && snapshot.boundaryMaskCellsScanned <= 2048 &&
        snapshot.boundaryMaskCellsScanned == snapshot.boundaryMaskFacesScanned * CHUNK_AREA &&
        snapshot.boundaryMaskCacheEntries <= 2048 &&
        snapshot.boundaryMasks.size() <= snapshot.boundaryMaskCacheEntries &&
        snapshot.boundaryMaskDeferred <= snapshot.boundaryMaskCandidates;
}

WorldMeshSnapshot caveBoundaryDrain(World &world, bool &budgetValid, int passes = 300)
{
    WorldMeshSnapshot result;
    for (int pass = 0; pass < passes; ++pass) {
        result = world.collectSectionMeshSnapshot();
        budgetValid &= caveBoundaryBudgetValid(result);
        if (result.boundaryMaskFacesScanned == 0) break;
    }
    return result;
}

Chunk &caveBoundaryRoofedSection(World &world, int chunkX, int chunkZ)
{
    auto &manager = world.getChunkManager();
    manager.loadChunk(chunkX, chunkZ);
    Chunk &chunk = *manager.findChunk(chunkX, chunkZ);
    for (int y = 0; y < CHUNK_SIZE; ++y)
        for (int z = 0; z < CHUNK_SIZE; ++z)
            for (int x = 0; x < CHUNK_SIZE; ++x)
                chunk.setBlock(x, CaveBoundaryBaseY + y, z, BlockId::Stone);
    for (int z = 0; z < CHUNK_SIZE; ++z)
        for (int x = 0; x < CHUNK_SIZE; ++x)
            chunk.setBlock(x, CaveBoundaryBaseY + CHUNK_SIZE, z, BlockId::Stone);
    chunk.rebuildSunlight();
    return chunk;
}

using CaveBoundaryBlocks = std::map<std::array<int, 2>,
    std::pair<std::vector<Block_t>, std::vector<BlockMetadata_t>>>;

CaveBoundaryBlocks caveBoundaryBlocks(World &world)
{
    CaveBoundaryBlocks result;
    for (const auto &entry : world.getChunkManager().getChunks()) {
        if (!entry.second.hasLoaded()) continue;
        const auto &location = entry.second.getLocation();
        auto &data = result[{location.x, location.y}];
        entry.second.collectBlockData(data.first, data.second);
    }
    return result;
}

using CaveBoundaryFiles = std::map<std::string, std::vector<char>>;
CaveBoundaryFiles caveBoundaryFiles(const std::string &directory)
{
    CaveBoundaryFiles result;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;
        std::ifstream in(entry.path(), std::ios::binary);
        result.emplace(std::filesystem::relative(entry.path(), directory).generic_string(),
            std::vector<char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()));
    }
    return result;
}

std::size_t caveBoundaryCandidateCount(World &world, int radius)
{
    const auto near = [radius](int x, int z) {
        return std::abs(x - CaveBoundaryCenterX) <= radius &&
               std::abs(z - CaveBoundaryCenterZ) <= radius;
    };
    std::size_t result = 0;
    for (const auto &entry : world.getChunkManager().getChunks()) {
        const auto &chunk = entry.second; const auto location = chunk.getLocation();
        if (!chunk.hasLoaded() || !near(location.x, location.y)) continue;
        const int faces = !near(location.x - 1, location.y) + !near(location.x + 1, location.y) +
            !near(location.x, location.y - 1) + !near(location.x, location.y + 1);
        result += static_cast<std::size_t>(faces) * chunk.getSectionCount();
    }
    return result;
}

void caseCaveBoundaryMasks()
{
    const std::array<glm::ivec3, 4> sides{{
        {-5, CaveBoundarySectionY, -4}, {-3, CaveBoundarySectionY, -4},
        {-4, CaveBoundarySectionY, -5}, {-4, CaveBoundarySectionY, -3}}};
    bool budgetValid = true;
    CaveBoundaryFixture fixture("cave_boundary_masks");
    World &world = *fixture.world;
    auto &manager = world.getChunkManager();
    for (int face = 0; face < 4; ++face) {
        Chunk &chunk = caveBoundaryRoofedSection(world, sides[face].x, sides[face].z);
        for (int y = 0; y < CHUNK_SIZE; ++y)
            for (int u = 0; u < CHUNK_SIZE; ++u)
                if ((y + u) % 2 == 0) {
                    const auto cell = caveBoundaryCell(face, y, u);
                    chunk.setBlock(cell.x, CaveBoundaryBaseY + cell.y, cell.z, BlockId::Air);
                }
        chunk.rebuildSunlight();
    }
    const int terrainVersion = manager.getTerrainGenerationVersion();
    check("CAVE_BOUNDARY/real-world-four-negative-coordinate-faces",
        std::all_of(sides.begin(), sides.end(), [&](const glm::ivec3 &p) {
            const auto *chunk = manager.findChunk(p.x, p.z);
            return chunk != nullptr && chunk->hasLoaded() && chunk->findSection(p.y) != nullptr;
        }));
    check("CAVE_BOUNDARY/fixture-save-before-observation", world.save());
    const auto originalBlocks = caveBoundaryBlocks(world);
    const auto originalFiles = caveBoundaryFiles(fixture.directory);
    const auto first = world.collectSectionMeshSnapshot();
    budgetValid &= caveBoundaryBudgetValid(first);
    check("CAVE_BOUNDARY/first-collection-scans-eight-faces-not-all-candidates",
        first.boundaryMaskFacesScanned == 8 && first.boundaryMaskCellsScanned == 2048 &&
        first.boundaryMaskCandidates == caveBoundaryCandidateCount(world, 1) &&
        first.boundaryMaskCandidates > 8 && first.boundaryMaskDeferred == first.boundaryMaskCandidates - 8);
    const std::array<int, 4> firstFaceOrder{{0, 2, 3, 1}};
    bool nearestHeightFirst = first.boundaryMasks.size() >= firstFaceOrder.size();
    for (std::size_t index = 0; nearestHeightFirst && index < firstFaceOrder.size(); ++index) {
        const int face = firstFaceOrder[index];
        nearestHeightFirst &= first.boundaryMasks[index].location == sides[face] &&
            first.boundaryMasks[index].face == face;
    }
    check("CAVE_BOUNDARY/initial-camera-height-nearest-faces-use-stable-coordinate-order", nearestHeightFirst);
    const auto confirm = world.collectSectionMeshSnapshot(false);
    check("CAVE_BOUNDARY/confirmation-consumer-preserves-work-and-published-values",
        confirm.boundaryMaskFacesScanned == 0 && confirm.boundaryMaskCellsScanned == 0 &&
        confirm.boundaryMaskDeferred == first.boundaryMaskDeferred && caveBoundarySameMasks(first, confirm));
    auto ready = caveBoundaryDrain(world, budgetValid);
    check("CAVE_BOUNDARY/small-loaded-boundary-converges", ready.boundaryMaskDeferred == 0);
    for (int face = 0; face < 4; ++face) {
        const auto *mask = caveBoundaryFind(ready, sides[face], face);
        bool rowsCorrect = mask != nullptr;
        if (mask != nullptr) {
            for (int y = 0; y < CHUNK_SIZE; ++y)
                rowsCorrect &= mask->rows[y] == static_cast<std::uint16_t>(y % 2 == 0 ? 0x5555 : 0xaaaa);
            const auto *chunk = manager.findChunk(sides[face].x, sides[face].z);
            rowsCorrect &= mask->blockRevision == chunk->findSection(CaveBoundarySectionY)->getBlockRevision() &&
                mask->incarnation == chunk->getIncarnation() && mask->incarnation != 0;
        }
        const std::size_t duplicates = std::count_if(ready.boundaryMasks.begin(), ready.boundaryMasks.end(),
            [&](const WorldBoundaryMaskFace &m) { return m.location == sides[face] && m.face == face; });
        check("CAVE_BOUNDARY/face-" + std::to_string(face) + "-checkerboard-one-mask-128-owned-air-bits",
            rowsCorrect && duplicates == 1 && caveBoundaryBitCount(*mask) == 128);
    }
    check("CAVE_BOUNDARY/observation-keeps-authoritative-blocks-version-and-save-bytes",
        caveBoundaryBlocks(world) == originalBlocks && caveBoundaryFiles(fixture.directory) == originalFiles &&
        manager.getTerrainGenerationVersion() == terrainVersion);
    bool sunlitOpenFacesExcluded = true;
    for (int face = 0; face < 4; ++face) {
        const auto cell = caveBoundaryCell(face, 4, 7);
        const auto p = glm::ivec3(sides[face].x, CaveBoundarySectionY + 1, sides[face].z) * CHUNK_SIZE + cell;
        sunlitOpenFacesExcluded &= world.getBlock(p.x, p.y, p.z) == BlockId::Air &&
            world.getSunlight(p.x, p.y, p.z) == MAX_LIGHT_LEVEL &&
            caveBoundaryFind(ready, {sides[face].x, CaveBoundarySectionY + 1, sides[face].z}, face) == nullptr;
    }
    check("CAVE_BOUNDARY/open-air-positive-sunlight-faces-never-publish-mask", sunlitOpenFacesExcluded);
    const auto idle = world.collectSectionMeshSnapshot();
    const auto idleAgain = world.collectSectionMeshSnapshot(false);
    check("CAVE_BOUNDARY/current-cache-is-idle-and-output-order-is-stable",
        idle.boundaryMaskFacesScanned == 0 && idle.boundaryMaskCellsScanned == 0 &&
        caveBoundarySameMasks(ready, idle) && caveBoundarySameMasks(idle, idleAgain));

    caveBoundaryRoofedSection(world, -5, -5);
    ready = caveBoundaryDrain(world, budgetValid);
    const auto solidIdle = world.collectSectionMeshSnapshot();
    check("CAVE_BOUNDARY/whole-solid-face-caches-zero-without-repeated-scanning",
        caveBoundaryFind(ready, {-5, CaveBoundarySectionY, -5}, 0) == nullptr &&
        caveBoundaryFind(ready, {-5, CaveBoundarySectionY, -5}, 2) == nullptr &&
        ready.boundaryMaskDeferred == 0 && solidIdle.boundaryMaskFacesScanned == 0 &&
        solidIdle.boundaryMaskDeferred == 0 && caveBoundarySameMasks(ready, solidIdle));

    // The inward side is a real loaded cave mouth connecting two Near chunks.
    Chunk &center = caveBoundaryRoofedSection(world, CaveBoundaryCenterX, CaveBoundaryCenterZ);
    center.setBlock(15, CaveBoundaryBaseY + 6, 3, BlockId::Air);
    auto *east = manager.findChunk(-3, -4);
    east->setBlock(0, CaveBoundaryBaseY + 6, 3, BlockId::Air);
    center.rebuildSunlight(); east->rebuildSunlight();
    ready = caveBoundaryDrain(world, budgetValid);
    check("CAVE_BOUNDARY/real-inner-near-cave-mouth-is-not-masked",
        world.getBlock(-49, CaveBoundaryBaseY + 6, -61) == BlockId::Air &&
        world.getBlock(-48, CaveBoundaryBaseY + 6, -61) == BlockId::Air &&
        world.getSunlight(-49, CaveBoundaryBaseY + 6, -61) == 0 &&
        caveBoundaryFind(ready, {CaveBoundaryCenterX, CaveBoundarySectionY, CaveBoundaryCenterZ}, 1) == nullptr &&
        caveBoundaryFind(ready, sides[1], 0) == nullptr);

    // Loading the outside chunk for ResidentData cannot remove the Near edge.
    world.preloadAround({-88.f, 200.f, -56.f});
    const auto residentOnly = world.collectSectionMeshSnapshot(false);
    check("CAVE_BOUNDARY/resident-only-outside-neighbor-still-needs-mask",
        manager.chunkLoadedAt(-6, -4) && caveBoundaryFind(residentOnly, sides[0], 0) != nullptr &&
        std::none_of(residentOnly.liveSections.begin(), residentOnly.liveSections.end(),
            [](const glm::ivec3 &p) { return p.x == -6 && p.z == -4; }));

    const auto editedCell = caveBoundaryCell(0, 6, 4);
    const glm::ivec3 editedWorld = sides[0] * CHUNK_SIZE + editedCell;
    const auto previousRevision = manager.findChunk(-5, -4)->findSection(CaveBoundarySectionY)->getBlockRevision();
    world.setBlock(editedWorld.x, editedWorld.y, editedWorld.z, BlockId::Stone);
    const auto dirty = world.collectSectionMeshSnapshot(false);
    check("CAVE_BOUNDARY/block-edit-immediately-stops-publishing-stale-mask",
        manager.findChunk(-5, -4)->findSection(CaveBoundarySectionY)->getBlockRevision() != previousRevision &&
        caveBoundaryFind(dirty, sides[0], 0) == nullptr && dirty.boundaryMaskFacesScanned == 0 &&
        dirty.boundaryMaskCellsScanned == 0 && dirty.boundaryMaskDeferred > 0);
    ready = caveBoundaryDrain(world, budgetValid);
    const auto *editedMask = caveBoundaryFind(ready, sides[0], 0);
    check("CAVE_BOUNDARY/block-edit-converges-to-exact-current-bits",
        editedMask != nullptr && (editedMask->rows[6] & (1u << 4)) == 0 && caveBoundaryBitCount(*editedMask) == 127);

    // Open the ceiling above an existing Air pixel. Actual World sunlight
    // propagation changes the lower section revision, without editing that Air.
    const auto litCell = caveBoundaryCell(0, 14, 2);
    const glm::ivec3 litWorld = sides[0] * CHUNK_SIZE + litCell;
    world.setBlock(litWorld.x, CaveBoundaryBaseY + 15, litWorld.z, BlockId::Air);
    ready = caveBoundaryDrain(world, budgetValid);
    const auto beforeLightRevision = manager.findChunk(-5, -4)->findSection(CaveBoundarySectionY)->getBlockRevision();
    std::vector<ChunkBlock> beforeLightBlocks;
    const auto *beforeLightSection = manager.findChunk(-5, -4)->findSection(CaveBoundarySectionY);
    for (int y = 0; y < CHUNK_SIZE; ++y)
        for (int z = 0; z < CHUNK_SIZE; ++z)
            for (int x = 0; x < CHUNK_SIZE; ++x) beforeLightBlocks.push_back(beforeLightSection->getBlock(x, y, z));
    world.setBlock(litWorld.x, CaveBoundaryBaseY + 16, litWorld.z, BlockId::Air);
    std::vector<ChunkBlock> afterLightBlocks;
    const auto *afterLightSection = manager.findChunk(-5, -4)->findSection(CaveBoundarySectionY);
    for (int y = 0; y < CHUNK_SIZE; ++y)
        for (int z = 0; z < CHUNK_SIZE; ++z)
            for (int x = 0; x < CHUNK_SIZE; ++x) afterLightBlocks.push_back(afterLightSection->getBlock(x, y, z));
    const auto relitDirty = world.collectSectionMeshSnapshot(false);
    check("CAVE_BOUNDARY/real-sunlight-change-invalidates-old-mask-before-scan",
        world.getBlock(litWorld.x, litWorld.y, litWorld.z) == BlockId::Air &&
        world.getSunlight(litWorld.x, litWorld.y, litWorld.z) == MAX_LIGHT_LEVEL &&
        beforeLightBlocks == afterLightBlocks &&
        manager.findChunk(-5, -4)->findSection(CaveBoundarySectionY)->getBlockRevision() != beforeLightRevision &&
        caveBoundaryFind(relitDirty, sides[0], 0) == nullptr && relitDirty.boundaryMaskFacesScanned == 0);
    ready = caveBoundaryDrain(world, budgetValid);
    const auto *litMask = caveBoundaryFind(ready, sides[0], 0);
    check("CAVE_BOUNDARY/positive-sunlight-air-is-excluded-after-convergence",
        litMask != nullptr && (litMask->rows[14] & (1u << 2)) == 0 &&
        (litMask->rows[15] & (1u << 2)) == 0 && caveBoundaryBitCount(*litMask) == 126);
    const auto waterCell = caveBoundaryCell(0, 8, 4);
    const glm::ivec3 waterWorld = sides[0] * CHUNK_SIZE + waterCell;
    world.setBlock(waterWorld.x, waterWorld.y, waterWorld.z, BlockId::Water);
    ready = caveBoundaryDrain(world, budgetValid);
    const auto *waterMask = caveBoundaryFind(ready, sides[0], 0);
    check("CAVE_BOUNDARY/non-air-water-is-excluded-even-with-zero-sunlight",
        waterMask != nullptr && world.getSunlight(waterWorld.x, waterWorld.y, waterWorld.z) == 0 &&
        (waterMask->rows[8] & (1u << 4)) == 0 && caveBoundaryBitCount(*waterMask) == 125);

    world.setRenderDistance(2);
    const auto expanded = world.collectSectionMeshSnapshot(false);
    check("CAVE_BOUNDARY/radius-expansion-immediately-removes-old-near-edge",
        std::all_of(sides.begin(), sides.end(), [&](const glm::ivec3 &p) {
            return std::none_of(expanded.boundaryMasks.begin(), expanded.boundaryMasks.end(),
                [&](const WorldBoundaryMaskFace &m) { return m.location == p; });
        }) && expanded.boundaryMaskFacesScanned == 0 && expanded.boundaryMaskCellsScanned == 0);
    world.setRenderDistance(1);
    const auto shrunk = world.collectSectionMeshSnapshot(false);
    check("CAVE_BOUNDARY/radius-shrink-does-not-resurrect-pruned-cache",
        caveBoundaryFind(shrunk, sides[0], 0) == nullptr && shrunk.boundaryMaskDeferred > 0 &&
        shrunk.boundaryMaskFacesScanned == 0);
    ready = caveBoundaryDrain(world, budgetValid);
    check("CAVE_BOUNDARY/radius-shrink-current-boundary-converges-again",
        caveBoundaryFind(ready, sides[0], 0) != nullptr && ready.boundaryMaskDeferred == 0);

    world.resetChunkMeshes();
    const auto reset = world.collectSectionMeshSnapshot(false);
    check("CAVE_BOUNDARY/mesh-reset-clears-all-mask-readiness",
        reset.boundaryMasks.empty() && reset.boundaryMaskFacesScanned == 0 &&
        reset.boundaryMaskCellsScanned == 0 && reset.boundaryMaskDeferred == reset.boundaryMaskCandidates);
    ready = caveBoundaryDrain(world, budgetValid);
    check("CAVE_BOUNDARY/reset-cache-rebuilds-from-resident-data",
        caveBoundaryFind(ready, sides[0], 0) != nullptr && ready.boundaryMaskDeferred == 0);
    check("CAVE_BOUNDARY/every-small-fixture-collection-respects-fixed-budget", budgetValid);

    // Save/reload once before observing, then reload identical serialized data
    // again: blockRevision is deliberately equal while incarnation differs.
    CaveBoundaryFixture reloadFixture("cave_boundary_equal_revision_reload");
    World &reloadWorld = *reloadFixture.world;
    auto &reloadManager = reloadWorld.getChunkManager();
    Chunk &reloadChunk = caveBoundaryRoofedSection(reloadWorld, -5, -4);
    reloadChunk.setBlock(0, CaveBoundaryBaseY + 6, 4, BlockId::Air);
    reloadChunk.rebuildSunlight();
    const bool firstUnload = reloadManager.unloadChunk(-5, -4);
    reloadManager.loadChunk(-5, -4);
    bool reloadBudgetValid = true;
    const auto beforeReload = caveBoundaryDrain(reloadWorld, reloadBudgetValid);
    const auto *beforeReloadMask = caveBoundaryFind(beforeReload, sides[0], 0);
    const std::uint32_t savedRevision = beforeReloadMask != nullptr ? beforeReloadMask->blockRevision : 0;
    const std::uint64_t savedIncarnation = beforeReloadMask != nullptr ? beforeReloadMask->incarnation : 0;
    const auto reloadedBlocks = caveBoundaryBlocks(reloadWorld);
    const bool secondUnload = reloadManager.unloadChunk(-5, -4);
    // Deliberately no collect while absent: the old cache entry must still be
    // present when an equal-revision replacement arrives at this coordinate.
    reloadManager.loadChunk(-5, -4);
    const auto *newChunk = reloadManager.findChunk(-5, -4);
    const auto *newSection = newChunk != nullptr ? newChunk->findSection(CaveBoundarySectionY) : nullptr;
    const auto beforeRescan = reloadWorld.collectSectionMeshSnapshot(false);
    check("CAVE_BOUNDARY/equal-revision-reload-rejects-old-incarnation",
        firstUnload && secondUnload && beforeReloadMask != nullptr && newSection != nullptr &&
        newSection->getBlockRevision() == savedRevision &&
        newChunk->getIncarnation() != savedIncarnation && savedIncarnation != 0 &&
        caveBoundaryBlocks(reloadWorld) == reloadedBlocks && caveBoundaryFind(beforeRescan, sides[0], 0) == nullptr &&
        beforeRescan.boundaryMaskFacesScanned == 0 && beforeRescan.boundaryMaskDeferred > 0);
    const auto afterRescan = caveBoundaryDrain(reloadWorld, reloadBudgetValid);
    const auto *newMask = caveBoundaryFind(afterRescan, sides[0], 0);
    check("CAVE_BOUNDARY/reloaded-mask-is-current-and-keeps-exact-owned-air",
        newMask != nullptr && newChunk != nullptr && newMask->incarnation == newChunk->getIncarnation() &&
        newMask->blockRevision == savedRevision && newMask->rows[6] == (1u << 4) &&
        caveBoundaryBitCount(*newMask) == 1 && reloadBudgetValid);
    const bool finalUnload = reloadManager.unloadChunk(-5, -4);
    const auto absent = reloadWorld.collectSectionMeshSnapshot(false);
    check("CAVE_BOUNDARY/unloaded-coordinate-never-publishes-retained-mask",
        finalUnload && caveBoundaryFind(absent, sides[0], 0) == nullptr && absent.boundaryMaskFacesScanned == 0);

    // At RD32 only load the actual perimeter, not all 4,225 interest cells.
    // Nine or more resident sections per edge give >2,048 candidate faces.
    CaveBoundaryFixture capFixture("cave_boundary_capacity", 32);
    World &capWorld = *capFixture.world;
    auto &capManager = capWorld.getChunkManager();
    std::set<std::array<int, 2>> perimeter;
    for (int offset = -32; offset <= 32; ++offset) {
        perimeter.insert({CaveBoundaryCenterX - 32, CaveBoundaryCenterZ + offset});
        perimeter.insert({CaveBoundaryCenterX + 32, CaveBoundaryCenterZ + offset});
        perimeter.insert({CaveBoundaryCenterX + offset, CaveBoundaryCenterZ - 32});
        perimeter.insert({CaveBoundaryCenterX + offset, CaveBoundaryCenterZ + 32});
    }
    for (const auto &location : perimeter) {
        capManager.loadChunk(location[0], location[1]);
        Chunk &chunk = *capManager.findChunk(location[0], location[1]);
        for (int z = 0; z < CHUNK_SIZE; ++z)
            for (int x = 0; x < CHUNK_SIZE; ++x) chunk.setBlock(x, 143, z, BlockId::Stone);
        chunk.rebuildSunlight();
    }
    const auto capBlocks = caveBoundaryBlocks(capWorld);
    const std::size_t capCandidates = caveBoundaryCandidateCount(capWorld, 32);
    const auto capFirst = capWorld.collectSectionMeshSnapshot();
    check("CAVE_BOUNDARY/rd32-real-perimeter-exceeds-cache-cap-with-eight-face-scan",
        perimeter.size() == 256 && capCandidates > 2048 && capFirst.boundaryMaskCandidates == capCandidates &&
        capFirst.boundaryMaskCacheEntries == 2048 && capFirst.boundaryMaskFacesScanned == 8 &&
        capFirst.boundaryMaskCellsScanned == 2048 && capFirst.boundaryMaskDeferred == capCandidates - 8);
    const auto capConfirmation = capWorld.collectSectionMeshSnapshot(false);
    check("CAVE_BOUNDARY/large-confirmation-does-not-consume-second-scan-budget",
        capConfirmation.boundaryMaskDeferred == capFirst.boundaryMaskDeferred &&
        capConfirmation.boundaryMaskFacesScanned == 0 && capConfirmation.boundaryMaskCellsScanned == 0 &&
        caveBoundarySameMasks(capFirst, capConfirmation));
    bool capBudgetValid = caveBoundaryBudgetValid(capFirst);
    const auto capReady = caveBoundaryDrain(capWorld, capBudgetValid);
    check("CAVE_BOUNDARY/cap-retains-ready-zero-records-and-defers-overflow-without-rescanning",
        capReady.boundaryMaskCacheEntries == 2048 && capReady.boundaryMaskFacesScanned == 0 &&
        capReady.boundaryMaskCellsScanned == 0 && capReady.boundaryMaskCandidates == capCandidates &&
        capReady.boundaryMaskDeferred == capCandidates - 2048 && capReady.boundaryMaskDeferred > 0 && capBudgetValid);
    bool currentPublished = true;
    std::set<std::array<int, 4>> unique;
    for (const auto &mask : capReady.boundaryMasks) {
        const auto *chunk = capManager.findChunk(mask.location.x, mask.location.z);
        const auto *section = chunk != nullptr ? chunk->findSection(mask.location.y) : nullptr;
        currentPublished &= section != nullptr && chunk->hasLoaded() && mask.face < 4 &&
            mask.blockRevision == section->getBlockRevision() && mask.incarnation == chunk->getIncarnation() &&
            caveBoundaryBitCount(mask) > 0 &&
            unique.insert({mask.location.x, mask.location.y, mask.location.z, mask.face}).second;
    }
    check("CAVE_BOUNDARY/large-output-is-unique-nonzero-and-current-resident-data",
        !capReady.boundaryMasks.empty() && currentPublished && caveBoundaryBlocks(capWorld) == capBlocks);
    const auto capIdle = capWorld.collectSectionMeshSnapshot(false);
    check("CAVE_BOUNDARY/cap-idle-output-and-deferred-count-are-stable",
        caveBoundarySameMasks(capReady, capIdle) && capIdle.boundaryMaskDeferred == capReady.boundaryMaskDeferred &&
        capIdle.boundaryMaskCacheEntries == 2048 && capIdle.boundaryMaskFacesScanned == 0);
    clearDeterministicEnv();
}

} // namespace
