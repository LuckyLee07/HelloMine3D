#pragma once

#include "../World/Generation/Structures/LandmarkExpedition.h"

namespace {

BlockId generatedLandmarkBlock(
    const std::vector<GeneratedStructureChunkSample> &chunks,
    int x, int y, int z)
{
    const int cx = WorldCoordinates::floorDiv(x, CHUNK_SIZE);
    const int cz = WorldCoordinates::floorDiv(z, CHUNK_SIZE);
    const auto found = std::find_if(chunks.begin(), chunks.end(),
        [=](const GeneratedStructureChunkSample &chunk) {
            return chunk.x == cx && chunk.z == cz;
        });
    if (found == chunks.end() || y < 0)
        return BlockId::NUM_TYPES;
    const std::size_t index =
        static_cast<std::size_t>(y / CHUNK_SIZE) * CHUNK_VOLUME +
        static_cast<std::size_t>(y % CHUNK_SIZE) * CHUNK_SIZE * CHUNK_SIZE +
        static_cast<std::size_t>(WorldCoordinates::floorMod(z, CHUNK_SIZE)) *
            CHUNK_SIZE +
        static_cast<std::size_t>(WorldCoordinates::floorMod(x, CHUNK_SIZE));
    return index < found->blocks.size()
        ? static_cast<BlockId>(found->blocks[index])
        : BlockId::NUM_TYPES;
}

void caseAdventureLandmarkLayouts()
{
    check("ADVENTURE-LANDMARK/v20-appends-without-save-bump",
        AdventureExplorationTerrainGenerationVersion == 19 &&
        LandmarkExpeditionTerrainGenerationVersion == 20 &&
        CurrentTerrainGenerationVersion == 20 &&
        WorldSaveFormatVersion == 12);

    constexpr int seed = 42;
    ClassicOverWorldGenerator current(seed,
        LandmarkExpeditionTerrainGenerationVersion);
    ClassicOverWorldGenerator previous(seed,
        AdventureExplorationTerrainGenerationVersion);
    const DeterministicStructurePlanner selector(seed,
        LandmarkExpeditionTerrainGenerationVersion,
        [&current](int x, int z) {
            return current.getSurfaceHeightAtWorld(x, z);
        },
        [&current](int x, int z) {
            return current.getBiomeAtWorld(x, z);
        });
    std::array<std::array<StructurePlanSnapshot, 2>, 3> layouts{};
    for (int radius = 0; radius <= 20; ++radius) {
        for (int cellX = -radius; cellX <= radius; ++cellX) {
            for (int cellZ = -radius; cellZ <= radius; ++cellZ) {
                if (std::max(std::abs(cellX), std::abs(cellZ)) != radius)
                    continue;
                const StructureType type =
                    selector.selectedStructureTypeForCell(cellX, cellZ);
                const auto plan = current.getStructurePlanForCell(
                    type, cellX, cellZ);
                if (!plan.valid) continue;
                auto &selected = layouts[static_cast<std::size_t>(type)]
                    [static_cast<std::size_t>(LandmarkExpedition::layoutFor(plan))];
                if (selected.valid) continue;
                const auto centerChunk = current.getStructurePlansForChunk(
                    WorldCoordinates::floorDiv(plan.anchor.x, CHUNK_SIZE),
                    WorldCoordinates::floorDiv(plan.anchor.z, CHUNK_SIZE));
                if (std::any_of(centerChunk.begin(), centerChunk.end(),
                        [&plan](const StructurePlanSnapshot &candidate) {
                            return candidate.key == plan.key;
                        }))
                    selected = plan;
            }
        }
        bool foundAll = true;
        for (const auto &pair : layouts)
            foundAll &= pair[0].valid && pair[1].valid;
        if (foundAll) break;
    }

    bool allSix = true;
    for (const auto &pair : layouts)
        allSix &= pair[0].valid && pair[1].valid;
    check("ADVENTURE-LANDMARK/six-real-selected-layouts", allSix);
    if (!allSix) return;

    setEnv("HELLOMINE3D_SEED", "0");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player,
        freshSaveDirectory("adventure_landmark_v20"), false, 0);

    for (int typeIndex = 0; typeIndex < 3; ++typeIndex) {
        for (int layout = 0; layout < 2; ++layout) {
            const StructurePlanSnapshot &plan =
                layouts[static_cast<std::size_t>(typeIndex)]
                       [static_cast<std::size_t>(layout)];
            const auto prior = previous.getStructurePlanForCell(plan.key.type,
                plan.key.cellX, plan.key.cellZ);
            const std::string label = "ADVENTURE-LANDMARK/type-" +
                std::to_string(typeIndex) + "-layout-" +
                std::to_string(layout);
            check(label + "-stable-place-and-loot",
                prior.valid && prior.key.terrainGenerationVersion == 19 &&
                plan.key.terrainGenerationVersion == 20 &&
                prior.anchor == plan.anchor &&
                prior.footprint.minimumX == plan.footprint.minimumX &&
                prior.footprint.maximumZ == plan.footprint.maximumZ &&
                structureLootForPlan(prior, ExplorationRewards::CurrentVersion)
                    .entries ==
                structureLootForPlan(plan, ExplorationRewards::CurrentVersion)
                    .entries);

            const auto forward = sampleStructureChunks(
                world, current, plan, false);
            const auto reverse = sampleStructureChunks(
                world, current, plan, true);
            bool projection = sameGeneratedStructureSamples(
                forward, reverse);
            std::array<int, static_cast<int>(BlockId::NUM_TYPES)> counts{};
            const auto &f = plan.footprint;
            using Point = std::tuple<int, int, int>;
            std::set<Point> solids, connected;
            std::queue<Point> pending;
            for (int x = f.minimumX; x <= f.maximumX; ++x)
                for (int z = f.minimumZ; z <= f.maximumZ; ++z)
                    for (int y = f.minimumY; y <= f.maximumY; ++y) {
                        const auto actual = generatedLandmarkBlock(
                            forward, x, y, z);
                        const auto expected = LandmarkExpedition::blockAt(
                            plan, x - plan.anchor.x,
                            y - plan.anchor.y, z - plan.anchor.z);
                        projection &= actual == expected;
                        if (actual < BlockId::NUM_TYPES)
                            ++counts[static_cast<int>(actual)];
                        if (actual != BlockId::Air &&
                            actual < BlockId::NUM_TYPES) {
                            const Point point{x, y, z};
                            solids.insert(point);
                            if (y == f.minimumY) {
                                connected.insert(point);
                                pending.push(point);
                            }
                        }
                    }
            check(label + "-real-chunks-and-order", projection);
            while (!pending.empty()) {
                const Point point = pending.front();
                pending.pop();
                for (const glm::ivec3 delta : {
                         glm::ivec3(1, 0, 0), {-1, 0, 0},
                         {0, 1, 0}, {0, -1, 0},
                         {0, 0, 1}, {0, 0, -1}}) {
                    const Point next{std::get<0>(point) + delta.x,
                                     std::get<1>(point) + delta.y,
                                     std::get<2>(point) + delta.z};
                    if (solids.count(next) && connected.insert(next).second)
                        pending.push(next);
                }
            }
            check(label + "-all-raised-parts-supported",
                connected.size() == solids.size());
            check(label + "-core-or-chest-and-minerals",
                counts[static_cast<int>(BlockId::WaystoneCore)] ==
                    (typeIndex == 0 ? 1 : 0) &&
                counts[static_cast<int>(BlockId::Chest)] ==
                    (typeIndex == 0 ? 0 : 1) &&
                counts[static_cast<int>(BlockId::IronOre)] ==
                    (typeIndex == 0 ? 1 : (typeIndex == 1 ? 4 : 0)) &&
                counts[static_cast<int>(BlockId::CoalOre)] ==
                    (typeIndex == 0 ? 4 : (typeIndex == 1 ? 0 : 1)));
            const int front = f.minimumZ;
            const int end = typeIndex == 0 ? plan.anchor.z - 1
                : typeIndex == 1 ? plan.anchor.z - 1
                                 : plan.anchor.z;
            bool entrance = true;
            for (int z = front; z <= end; ++z) {
                const int floor = plan.anchor.y +
                    (typeIndex == 0 && layout == 1 ? 1
                     : z == front ? 0 : 1);
                entrance &= generatedLandmarkBlock(
                    forward, plan.anchor.x, floor, z) != BlockId::Air &&
                    generatedLandmarkBlock(forward,
                        plan.anchor.x, floor + 1, z) == BlockId::Air &&
                    generatedLandmarkBlock(forward,
                        plan.anchor.x, floor + 2, z) == BlockId::Air;
            }
            check(label + "-walkable-two-block-entrance", entrance);
            const int side = (plan.selectionHash & 1ull) != 0 ? -1 : 1;
            if (typeIndex == 2 && layout == 1)
                check(label + "-lookout-stairs-and-deck",
                    generatedLandmarkBlock(forward,
                        plan.anchor.x + side * 2, plan.anchor.y + 2,
                        plan.anchor.z - 2) == BlockId::OakPlank &&
                    generatedLandmarkBlock(forward,
                        plan.anchor.x + side * 3, plan.anchor.y + 3,
                        plan.anchor.z - 2) == BlockId::OakPlank &&
                    generatedLandmarkBlock(forward,
                        plan.anchor.x + side * 4, plan.anchor.y + 4,
                        plan.anchor.z - 1) == BlockId::OakPlank &&
                    generatedLandmarkBlock(forward,
                        plan.anchor.x + side * 4, plan.anchor.y + 5,
                        plan.anchor.z - 1) == BlockId::Air);
            if (plan.hasChest)
                check(label + "-chest-payload-preserved",
                    generatedChestMatchesLoot(forward, plan,
                        structureLootForPlan(plan,
                            ExplorationRewards::CurrentVersion)));
        }
    }
    const auto directory = freshSaveDirectory(
        "adventure_landmark_v20_reopen");
    bool persisted = true;
    std::uint64_t editedHash = 0;
    {
        Player owner;
        World created(camera, config, owner, directory, false, 0);
        created.getChunkManager().loadChunk(-1, -1);
        created.setBlock(-2, 190, -2, BlockId::OakPlank);
        editedHash = TerrainSurvey::blockHash(
            created.getChunkManager().getChunk(-1, -1));
        persisted &= created.getChunkManager()
            .getTerrainGenerationVersion() == 20 && created.save();
    }
    {
        Player owner;
        World reopened(camera, config, owner, directory, false, 0);
        reopened.getChunkManager().loadChunk(-1, -1);
        persisted &= reopened.getChunkManager()
                .getTerrainGenerationVersion() == 20 &&
            reopened.getBlock(-2, 190, -2) == BlockId::OakPlank &&
            editedHash == TerrainSurvey::blockHash(
                reopened.getChunkManager().getChunk(-1, -1));
    }
    check("ADVENTURE-LANDMARK/default-v20-edit-save-reopen", persisted);
    clearDeterministicEnv();
}
} // namespace
