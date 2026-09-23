#pragma once

#include "../World/Generation/Structures/LandmarkWorkshop.h"

namespace {
void caseAdventureLandmarkWorkshops()
{
    check("ADVENTURE-WORKSHOP/v22-appends-without-save-bump",
          LandmarkApproachTerrainGenerationVersion == 21 &&
          LandmarkWorkshopTerrainGenerationVersion == 22 &&
          CurrentTerrainGenerationVersion == 22 &&
          WorldSaveFormatVersion == 12);

    setEnv("HELLOMINE3D_SEED", "0");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    Config config = makeConfig();
    Camera camera(config);
    Player owner;
    World sampleWorld(camera, config, owner,
        freshSaveDirectory("adventure_workshop_samples"), false, 0);

    struct Selected {
        int seed = 0;
        StructurePlanSnapshot plan;
        LandmarkWorkshop::Site site;
    };
    std::array<Selected, 3> selected{};
    int candidates = 0;
    for (const int seed : {42, 20260807, 239701883}) {
        ClassicOverWorldGenerator generator(seed,
            LandmarkWorkshopTerrainGenerationVersion);
        for (int radius = 0; radius <= 24; ++radius) {
            for (int cellX = -radius; cellX <= radius; ++cellX)
                for (int cellZ = -radius; cellZ <= radius; ++cellZ) {
                    if (std::max(std::abs(cellX), std::abs(cellZ)) != radius)
                        continue;
                    for (const auto type : {StructureType::Waystone,
                                            StructureType::Ruin,
                                            StructureType::RaiderCamp}) {
                        const auto plan = generator.getStructurePlanForCell(
                            type, cellX, cellZ);
                        if (!plan.valid) continue;
                        const auto site = LandmarkWorkshop::select(plan,
                            [&generator](int x, int z) {
                                return generator.getSurfaceHeightAtWorld(x, z);
                            },
                            [&generator](int x, int z) {
                                return generator.getBiomeAtWorld(x, z);
                            });
                        if (!site.valid) continue;
                        const int index = site.style ==
                                LandmarkApproach::Style::Forest ? 0
                            : site.style == LandmarkApproach::Style::Riverbank
                                ? 1 : 2;
                        if (selected[index].site.valid) continue;
                        ++candidates;
                        const int chunkX = WorldCoordinates::floorDiv(
                            site.minimumX, CHUNK_SIZE);
                        const int chunkZ = WorldCoordinates::floorDiv(
                            site.minimumZ, CHUNK_SIZE);
                        Chunk chunk(sampleWorld, {chunkX, chunkZ}, false);
                        generator.generateTerrainFor(chunk);
                        const int lx = site.machineX() - chunkX * CHUNK_SIZE;
                        const int lz = site.machineZ() - chunkZ * CHUNK_SIZE;
                        if (chunk.getBlock(lx, site.baseY + 1, lz) !=
                                LandmarkWorkshop::machine(site.style) ||
                            chunk.findBlockEntity(
                                {lx, site.baseY + 1, lz}) == nullptr)
                            continue;
                        selected[index] = {seed, plan, site};
                    }
                }
            if (std::all_of(selected.begin(), selected.end(),
                    [](const auto &item) { return item.site.valid; })) break;
        }
        if (std::all_of(selected.begin(), selected.end(),
                [](const auto &item) { return item.site.valid; })) break;
    }
    const bool threeRegions = std::all_of(selected.begin(), selected.end(),
        [](const auto &item) { return item.site.valid; });
    check("ADVENTURE-WORKSHOP/three-real-regional-sites",
          threeRegions, "candidates=" + std::to_string(candidates));
    if (!threeRegions) {
        clearDeterministicEnv();
        setEnv("HELLOMINE3D_SEED", "");
        return;
    }

    const std::array<BlockId, 3> machines = {
        BlockId::Chest, BlockId::Furnace, BlockId::Crusher};
    const std::array<std::string, 3> types = {
        ChestContainer::BlockEntityType,
        FurnaceContainer::BlockEntityType,
        CrusherContainer::BlockEntityType};
    for (int index = 0; index < 3; ++index) {
        const auto &item = selected[index];
        const auto &site = item.site;
        const auto &plan = item.plan;
        const std::string label = "ADVENTURE-WORKSHOP/style-" +
            std::to_string(index);
        ClassicOverWorldGenerator current(item.seed,
            LandmarkWorkshopTerrainGenerationVersion);
        ClassicOverWorldGenerator previous(item.seed,
            LandmarkApproachTerrainGenerationVersion);
        const auto oldPlan = previous.getStructurePlanForCell(
            plan.key.type, plan.key.cellX, plan.key.cellZ);
        check(label + "-candidate-layout-loot-preserved",
              oldPlan.valid && oldPlan.anchor == plan.anchor &&
              oldPlan.selectionHash == plan.selectionHash &&
              LandmarkExpedition::layoutFor(oldPlan) ==
                  LandmarkExpedition::layoutFor(plan) &&
              structureLootForPlan(oldPlan,
                  ExplorationRewards::CurrentVersion).entries ==
              structureLootForPlan(plan,
                  ExplorationRewards::CurrentVersion).entries);

        auto area = plan;
        area.footprint.minimumX = std::min(
            area.footprint.minimumX, site.minimumX);
        area.footprint.maximumX = std::max(
            area.footprint.maximumX, site.minimumX + 2);
        area.footprint.minimumZ = std::min(
            area.footprint.minimumZ, site.minimumZ);
        const auto forward = sampleStructureChunks(
            sampleWorld, current, area, false);
        const auto reverse = sampleStructureChunks(
            sampleWorld, current, area, true);
        const auto old = sampleStructureChunks(
            sampleWorld, previous, area, false);
        check(label + "-chunk-order-deterministic",
              sameGeneratedStructureSamples(forward, reverse));
        bool oldBuildingSame = true;
        const auto &f = plan.footprint;
        for (int x = f.minimumX; x <= f.maximumX; ++x)
            for (int z = f.minimumZ; z <= f.maximumZ; ++z)
                for (int y = f.minimumY; y <= f.maximumY; ++y)
                    oldBuildingSame &= generatedLandmarkBlock(
                        forward, x, y, z) ==
                        generatedLandmarkBlock(old, x, y, z);
        check(label + "-v21-building-unchanged", oldBuildingSame);

        const glm::ivec3 machinePosition{
            site.machineX(), site.baseY + 1, site.machineZ()};
        int styleMaterials = 0;
        for (int across = 0; across < 3; ++across)
            for (int depth = 0; depth < 3; ++depth) {
                const int x = site.worldX(across);
                const int z = site.worldZ(depth);
                const BlockId ground = generatedLandmarkBlock(
                    forward, x, site.baseY, z);
                if (index == 0)
                    styleMaterials += ground == BlockId::MossStone ||
                        ground == BlockId::ForestFloor;
                else if (index == 1)
                    styleMaterials += ground == BlockId::Clay ||
                        ground == BlockId::Silt || ground == BlockId::Gravel;
                else
                    styleMaterials += ground == BlockId::Stone ||
                        ground == BlockId::Gravel;
            }
        const int frontX = site.worldX(1);
        const int frontZ = site.worldZ(1);
        const BlockId entry = generatedLandmarkBlock(forward,
            site.worldX(1), site.baseY, site.worldZ(0));
        const BlockId centre = generatedLandmarkBlock(forward,
            frontX, site.baseY, frontZ);
        const BlockId edge = generatedLandmarkBlock(forward,
            site.worldX(0), site.baseY, site.worldZ(2));
        const BlockId roof = generatedLandmarkBlock(forward,
            site.worldX(1), site.baseY + 3, site.worldZ(2));
        const bool regionalMix = index == 0
            ? entry == BlockId::MossStone &&
              centre == BlockId::ForestFloor &&
              roof == BlockId::OakPlank
            : index == 1
                ? entry == BlockId::Gravel &&
                  centre == BlockId::Silt && edge == BlockId::Clay &&
                  roof == BlockId::Clay
                : entry == BlockId::Gravel && edge == BlockId::Stone &&
                  roof == BlockId::OakPlank;
        check(label + "-built-materials-open-workspace",
              styleMaterials == 9 && regionalMix &&
              generatedLandmarkBlock(forward,
                  machinePosition.x, machinePosition.y,
                  machinePosition.z) == machines[index] &&
              generatedLandmarkBlock(forward, frontX,
                  site.baseY + 1, frontZ) == BlockId::Air &&
              generatedLandmarkBlock(forward, frontX,
                  site.baseY + 2, frontZ) == BlockId::Air);
        int matchingRecords = 0;
        bool initiallyEmpty = false;
        for (const auto &sample : forward)
            for (const auto &record : sample.blockEntities) {
                const int x = sample.x * CHUNK_SIZE + record.position.x;
                const int z = sample.z * CHUNK_SIZE + record.position.z;
                const bool matches = x == machinePosition.x &&
                    z == machinePosition.z &&
                    record.position.y == machinePosition.y &&
                    record.type == types[index];
                matchingRecords += matches;
                if (matches) {
                    if (index == 0)
                        initiallyEmpty = record.payload ==
                            ContainerInventory(ChestContainer::SlotCount)
                                .serialize();
                    else if (index == 1)
                        initiallyEmpty = record.payload ==
                            FurnaceContainer::serialize(FurnaceState{});
                    else
                        initiallyEmpty = record.payload ==
                            CrusherContainer::serialize(CrusherState{});
                }
            }
        check(label + "-one-empty-functional-entity",
              matchingRecords == 1 && initiallyEmpty);

        const auto directory = freshSaveDirectory(
            "adventure_workshop_v22_" + std::to_string(index));
        bool lifecycle = initializeTerrainIdentity(directory,
            "adventure-workshop-v22-" + std::to_string(index),
            22, item.seed);
        {
            Player player;
            World world(camera, config, player, directory, false, 0);
            world.getChunkManager().loadChunk(
                WorldCoordinates::floorDiv(site.minimumX, CHUNK_SIZE),
                WorldCoordinates::floorDiv(site.minimumZ, CHUNK_SIZE));
            const auto entity = world.getBlockEntity(machinePosition);
            lifecycle &= entity && entity->type == types[index] &&
                world.getBlock(machinePosition.x, machinePosition.y,
                               machinePosition.z) == machines[index];
            if (index == 0)
                lifecycle &= ChestContainer::open(world, player,
                    machinePosition);
            else if (index == 1)
                lifecycle &= FurnaceContainer::open(world, player,
                    machinePosition, runtimeSmeltingRegistry());
            else
                lifecycle &= CrusherContainer::open(world, player,
                    machinePosition);
            player.closeContainer();
            lifecycle &= world.save();
        }
        {
            Player player;
            World world(camera, config, player, directory, false, 0);
            world.getChunkManager().loadChunk(
                WorldCoordinates::floorDiv(site.minimumX, CHUNK_SIZE),
                WorldCoordinates::floorDiv(site.minimumZ, CHUNK_SIZE));
            lifecycle &= world.getBlockEntity(machinePosition).has_value();
            world.setBlock(machinePosition.x, machinePosition.y,
                           machinePosition.z, BlockId::Air);
            lifecycle &= !world.getBlockEntity(machinePosition) &&
                world.save();
        }
        {
            Player player;
            World world(camera, config, player, directory, false, 0);
            world.getChunkManager().loadChunk(
                WorldCoordinates::floorDiv(site.minimumX, CHUNK_SIZE),
                WorldCoordinates::floorDiv(site.minimumZ, CHUNK_SIZE));
            lifecycle &= world.getBlock(machinePosition.x,
                    machinePosition.y, machinePosition.z) == BlockId::Air &&
                !world.getBlockEntity(machinePosition);
        }
        check(label + "-open-save-break-reopen-no-respawn", lifecycle);
    }
    const auto newDirectory = freshSaveDirectory(
        "adventure_workshop_v22_default");
    bool defaultVersion = false;
    {
        Player player;
        World world(camera, config, player, newDirectory, false, 0);
        defaultVersion = world.getChunkManager()
            .getTerrainGenerationVersion() == 22 && world.save();
    }
    {
        Player player;
        World world(camera, config, player, newDirectory, false, 0);
        defaultVersion &= world.getChunkManager()
            .getTerrainGenerationVersion() == 22;
    }
    check("ADVENTURE-WORKSHOP/default-v22-save-reopen",
          defaultVersion);
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "");
}
} // namespace
