#pragma once

#include "../World/Generation/Structures/LandmarkApproach.h"

namespace {
void caseAdventureLandmarkApproaches()
{
    check("ADVENTURE-APPROACH/v21-appends-without-save-bump",
          LandmarkExpeditionTerrainGenerationVersion == 20 &&
          LandmarkApproachTerrainGenerationVersion == 21 &&
          CurrentTerrainGenerationVersion == 21 &&
          WorldSaveFormatVersion == 12);

    struct Selected {
        int seed = 0;
        StructurePlanSnapshot plan;
    };
    std::array<Selected, 3> selected{};
    int examined = 0;
    for (const int seed : {42, 20260807, 239701883}) {
        ClassicOverWorldGenerator generator(seed,
            LandmarkApproachTerrainGenerationVersion);
        for (int radius = 0; radius <= 18; ++radius) {
            for (int cellX = -radius; cellX <= radius; ++cellX)
                for (int cellZ = -radius; cellZ <= radius; ++cellZ) {
                    if (std::max(std::abs(cellX), std::abs(cellZ)) != radius)
                        continue;
                    // Only one type is eligible for each structure cell.
                    for (const auto candidateType : {
                             StructureType::Waystone,
                             StructureType::Ruin,
                             StructureType::RaiderCamp}) {
                        const auto plan = generator.getStructurePlanForCell(
                            candidateType, cellX, cellZ);
                        if (!plan.valid) continue;
                        ++examined;
                        const auto style = LandmarkApproach::styleFor(plan,
                            [&generator](int x, int z) {
                                return generator.getBiomeAtWorld(x, z);
                            });
                        if (style == LandmarkApproach::Style::Open) continue;
                        const int index = style == LandmarkApproach::Style::Forest
                            ? 0 : style == LandmarkApproach::Style::Riverbank
                            ? 1 : 2;
                        if (selected[index].plan.valid) continue;
                        int suitable = 0;
                        for (int distance = 1;
                             distance <= LandmarkApproach::Length; ++distance) {
                            const int y = generator.getSurfaceHeightAtWorld(
                                plan.anchor.x,
                                plan.footprint.minimumZ - distance);
                            suitable += y >= WATER_LEVEL + 2 &&
                                std::abs(y - plan.anchor.y) <= 4;
                        }
                        if (suitable < 4) continue;
                        const auto actual = generator.getStructurePlansForChunk(
                            WorldCoordinates::floorDiv(plan.anchor.x, CHUNK_SIZE),
                            WorldCoordinates::floorDiv(plan.anchor.z, CHUNK_SIZE));
                        if (std::none_of(actual.begin(), actual.end(),
                                [&plan](const auto &candidate) {
                                    return candidate.key == plan.key;
                                })) continue;
                        selected[index] = {seed, plan};
                    }
                }
            if (std::all_of(selected.begin(), selected.end(),
                    [](const auto &item) { return item.plan.valid; })) break;
        }
        if (std::all_of(selected.begin(), selected.end(),
                [](const auto &item) { return item.plan.valid; })) break;
    }
    bool allStyles = true;
    for (const auto &item : selected) allStyles &= item.plan.valid;
    check("ADVENTURE-APPROACH/real-forest-river-highland-sites",
          allStyles, "examined=" + std::to_string(examined));
    if (!allStyles) return;

    setEnv("HELLOMINE3D_SEED", "0");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    Config config = makeConfig();
    Camera camera(config);
    Player owner;
    World world(camera, config, owner,
        freshSaveDirectory("adventure_approach_samples"), false, 0);

    for (int styleIndex = 0; styleIndex < 3; ++styleIndex) {
        const auto &item = selected[styleIndex];
        const auto &plan = item.plan;
        ClassicOverWorldGenerator current(item.seed,
            LandmarkApproachTerrainGenerationVersion);
        ClassicOverWorldGenerator previous(item.seed,
            LandmarkExpeditionTerrainGenerationVersion);
        const auto prior = previous.getStructurePlanForCell(plan.key.type,
            plan.key.cellX, plan.key.cellZ);
        const std::string label = "ADVENTURE-APPROACH/style-" +
            std::to_string(styleIndex);
        check(label + "-same-anchor-layout-and-loot",
              prior.valid && prior.anchor == plan.anchor &&
              prior.selectionHash == plan.selectionHash &&
              LandmarkExpedition::layoutFor(prior) ==
                  LandmarkExpedition::layoutFor(plan) &&
              structureLootForPlan(prior, ExplorationRewards::CurrentVersion)
                  .entries ==
              structureLootForPlan(plan, ExplorationRewards::CurrentVersion)
                  .entries);

        auto area = plan;
        area.footprint.minimumZ -= LandmarkApproach::Length;
        const auto forward = sampleStructureChunks(world, current, area, false);
        const auto reverse = sampleStructureChunks(world, current, area, true);
        const auto old = sampleStructureChunks(world, previous, area, false);
        check(label + "-chunk-order-deterministic",
              sameGeneratedStructureSamples(forward, reverse));

        bool buildingUnchanged = true;
        const auto &f = plan.footprint;
        for (int x = f.minimumX; x <= f.maximumX; ++x)
            for (int z = f.minimumZ; z <= f.maximumZ; ++z)
                for (int y = f.minimumY; y <= f.maximumY; ++y)
                    buildingUnchanged &= generatedLandmarkBlock(
                        forward, x, y, z) == generatedLandmarkBlock(
                        old, x, y, z);
        check(label + "-existing-building-stays-identical",
              buildingUnchanged);

        int painted = 0, accessible = 0, accents = 0;
        const auto style = LandmarkApproach::styleFor(plan,
            [&current](int x, int z) {
                return current.getBiomeAtWorld(x, z);
            });
        for (int distance = 1; distance <= LandmarkApproach::Length;
             ++distance) {
            const int z = f.minimumZ - distance;
            const int y = current.getSurfaceHeightAtWorld(plan.anchor.x, z);
            const BlockId block = generatedLandmarkBlock(forward,
                plan.anchor.x, y, z);
            painted += block == LandmarkApproach::surface(style, distance, 0);
            accessible += block != BlockId::Water &&
                generatedLandmarkBlock(forward, plan.anchor.x, y + 1, z) ==
                    BlockId::Air &&
                generatedLandmarkBlock(forward, plan.anchor.x, y + 2, z) ==
                    BlockId::Air;
            for (int side : {-1, 1}) {
                const int sideY = current.getSurfaceHeightAtWorld(
                    plan.anchor.x + side, z);
                const BlockId marker = LandmarkApproach::shoulderMarker(
                    style, distance, side);
                if (marker != BlockId::Air)
                    accents += generatedLandmarkBlock(forward,
                        plan.anchor.x + side, sideY + 1, z) == marker;
            }
        }
        check(label + "-grounded-readable-approach",
              painted >= 4 && accessible >= 4 && accents >= 2,
              "painted=" + std::to_string(painted) +
              " open=" + std::to_string(accessible) +
              " markers=" + std::to_string(accents));
    }

    const auto directory = freshSaveDirectory(
        "adventure_landmark_v21_reopen");
    bool persisted = true;
    std::uint64_t editedHash = 0;
    {
        Player player;
        World created(camera, config, player, directory, false, 0);
        created.getChunkManager().loadChunk(-1, -1);
        created.setBlock(-2, 190, -2, BlockId::OakPlank);
        editedHash = TerrainSurvey::blockHash(
            created.getChunkManager().getChunk(-1, -1));
        persisted &= created.getChunkManager()
            .getTerrainGenerationVersion() == 21 && created.save();
    }
    {
        Player player;
        World reopened(camera, config, player, directory, false, 0);
        reopened.getChunkManager().loadChunk(-1, -1);
        persisted &= reopened.getChunkManager()
                .getTerrainGenerationVersion() == 21 &&
            reopened.getBlock(-2, 190, -2) == BlockId::OakPlank &&
            editedHash == TerrainSurvey::blockHash(
                reopened.getChunkManager().getChunk(-1, -1));
    }
    check("ADVENTURE-APPROACH/default-v21-edit-save-reopen", persisted);
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "");
}
} // namespace
