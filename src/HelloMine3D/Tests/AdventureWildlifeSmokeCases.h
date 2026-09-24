#pragma once

#include "../Actor/WildlifeActor.h"
#include "../Actor/WildlifePresentation.h"

namespace {
void caseWildlifeReviewRegressions()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "42");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 201 8");
    Config config = makeConfig(); Camera camera(config); Player player;
    World world(camera, config, player,
        freshSaveDirectory("wildlife_review_regressions"), false, 0);
    for (int x = 0; x < 16; ++x) for (int z = 0; z < 16; ++z) {
        world.setBlock(x, 200, z, BlockId::Stone);
        for (int y = 201; y <= 205; ++y)
            world.setBlock(x, y, z, BlockId::Air);
    }
    for (const int population : {12, 24}) {
        world.getActorManager().removeActorsIf([](const Actor&) { return true; });
        std::vector<ActorId> ids;
        std::vector<glm::vec3> origins;
        std::vector<int> firstMoved(population, 0);
        for (int i = 0; i < population; ++i) {
            const float angle = float(i % 12) * 6.2831853f / 12.f;
            const float radius = i < 12 ? 2.5f : 4.5f;
            const glm::vec3 at{8.f + radius * std::sin(angle), 201.f,
                               8.f + radius * std::cos(angle)};
            const auto id = world.getActorManager().allocateActorId();
            ids.push_back(id); origins.push_back(at);
            world.getActorManager().addActor(std::make_unique<WildlifeActor>(
                id, WildlifeSpecies::Rabbit, at), world);
        }
        bool bounded = true;
        for (int tick = 1; tick <= 40; ++tick) {
            world.tick(1000 + tick);
            bounded &= world.collectDebugStats().wildlifeBlockQueriesUsed <=
                World::WildlifeBlockQueryBudgetPerTick;
            for (int i = 0; i < population; ++i)
                if (firstMoved[i] == 0 && glm::distance(origins[i],
                    world.getActorManager().findActor(ids[i])->position) > .01f)
                    firstMoved[i] = tick;
        }
        check("WILDLIFE-REVIEW/fair-service-" + std::to_string(population),
            bounded && std::all_of(firstMoved.begin(), firstMoved.end(),
                [](int tick) { return tick > 0 && tick <= 28; }));
    }
    world.getActorManager().removeActorsIf([](const Actor&) { return true; });
    WildlifeActor rabbit(900001, WildlifeSpecies::Rabbit, {8.5f,201.f,8.5f});
    float largestStep = 0.f; int movingTicks = 0;
    for (int tick = 1; tick <= 16; ++tick) {
        player.position = rabbit.position + glm::vec3(0,0,2);
        world.tick(1100 + tick);
        const auto before = rabbit.position;
        rabbit.tick(world, .05f);
        const float distance = glm::distance(before, rabbit.position);
        largestStep = std::max(largestStep, distance);
        movingTicks += distance > .01f;
    }
    check("WILDLIFE-REVIEW/motion-is-independent-of-decision-rate",
        movingTicks >= 10 && largestStep <= .151f,
        "steps=" + std::to_string(movingTicks) +
        " largest=" + std::to_string(largestStep));

    world.tick(1201);
    world.setBlock(6,204,6,BlockId::OakLeaf);
    glm::vec3 settled; bool grounded = false;
    check("WILDLIFE-REVIEW/clear-ground-under-canopy",
        world.tryWildlifeStep({6.5f,201.f,6.5f}, {6.6f,201.f,6.5f},
            {.22f,.27f,.22f}, settled, &grounded) ==
            World::WildlifeStepResult::Allowed && grounded && settled.y == 201.f);
    world.setBlock(10,201,10,BlockId::Stone);
    world.tick(1202);
    const bool down = world.tryWildlifeStep({10.78f,202.f,10.5f},
        {11.24f,202.f,10.5f}, {.22f,.27f,.22f}, settled) ==
        World::WildlifeStepResult::Allowed && settled.y == 201.f;
    world.tick(1203);
    const bool up = world.tryWildlifeStep({11.24f,201.f,10.5f},
        {10.78f,201.f,10.5f}, {.22f,.27f,.22f}, settled) ==
        World::WildlifeStepResult::Allowed && settled.y == 202.f;
    check("WILDLIFE-REVIEW/one-block-stairs-both-directions", down && up);
    world.setBlock(11,201,11,BlockId::Stone);
    world.setBlock(11,202,11,BlockId::Stone);
    world.tick(1204);
    check("WILDLIFE-REVIEW/diagonal-obstacle-cannot-be-skipped",
        world.tryWildlifeStep({10.75f,201.f,11.75f},
            {11.25f,201.f,12.25f}, {.22f,.27f,.22f}, settled) ==
            World::WildlifeStepResult::Blocked);
    WildlifeActor falling(900002, WildlifeSpecies::Sheep, {6.5f,201.f,6.5f});
    for (int x = 4; x <= 9; ++x) for (int z = 4; z <= 9; ++z) {
        world.setBlock(x,197,z,BlockId::Stone);
        for (int y = 198; y <= 200; ++y) world.setBlock(x,y,z,BlockId::Air);
    }
    for (int tick = 1; tick <= 60; ++tick) {
        world.tick(1201 + tick); falling.tick(world,.05f);
    }
    check("WILDLIFE-REVIEW/removed-support-falls-and-lands",
        std::abs(falling.position.y - 198.f) < .001f);

    WildlifePresentation::MotionBlend blend;
    ActorSnapshot sample; sample.position = {0,10,0};
    blend.update(sample,0.f);
    sample.position.x = .15f; sample.wildlifeMotionSeconds = .05f;
    const auto mid = blend.update(sample,.025f);
    const auto paused = blend.update(sample,0.f);
    const auto end = blend.update(sample,.025f);
    check("WILDLIFE-REVIEW/visual-interpolation-pause-and-no-overshoot",
        mid.x > 0.f && mid.x < .15f && paused == mid && end.x == .15f);
    sample.position.x = 100.f;
    check("WILDLIFE-REVIEW/teleport-resets-visual-segment",
        blend.update(sample,.01f) == sample.position);
    clearDeterministicEnv();
}

void caseAdventureWildlife()
{
    caseWildlifeReviewRegressions();
    {
        const auto sheep = WildlifePresentation::profileFor(
            WildlifeSpecies::Sheep);
        const auto rabbit = WildlifePresentation::profileFor(
            WildlifeSpecies::Rabbit);
        const auto bird = WildlifePresentation::profileFor(
            WildlifeSpecies::MarshBird);
        const auto hasRole = [](const WildlifeVisualProfile &profile,
                                WildlifeVisualRole role) {
            for (std::size_t index = 0; index < profile.partCount; ++index)
                if (profile.parts[index].role == role) return true;
            return false;
        };
        check("ADVENTURE-WILDLIFE/three-bounded-model-silhouettes",
            sheep.partCount == 8 && rabbit.partCount == 7 &&
            bird.partCount == 8 &&
            sheep.speciesIndex == 0 && rabbit.speciesIndex == 1 &&
            bird.speciesIndex == 2 &&
            hasRole(sheep, WildlifeVisualRole::Muzzle) &&
            hasRole(rabbit, WildlifeVisualRole::Ear) &&
            hasRole(bird, WildlifeVisualRole::Beak) &&
            hasRole(bird, WildlifeVisualRole::Wing) &&
            WildlifeVisualProfile::MaximumParts * 12 == 96 &&
            World::WildlifeWorldCap * WildlifeVisualProfile::MaximumParts *
                24 <= 10000);
        ActorSnapshot snapshot;
        snapshot.type = WildlifeSpecies::Rabbit;
        snapshot.wildlifeActivity = static_cast<int>(WildlifeActivity::Flee);
        const auto fleeing = WildlifePresentation::poseFor(
            snapshot, rabbit, 1.2f, 1.f);
        const auto still = WildlifePresentation::poseFor(
            snapshot, rabbit, 1.2f, 0.f);
        bool legMoves = false, earsLift = false;
        for (std::size_t index = 0; index < rabbit.partCount; ++index) {
            if (rabbit.parts[index].role == WildlifeVisualRole::Leg)
                legMoves |= std::abs(fleeing.rotations[index].x) > 5.f;
            if (rabbit.parts[index].role == WildlifeVisualRole::Ear)
                earsLift |= fleeing.rotations[index].x > 5.f;
        }
        check("ADVENTURE-WILDLIFE/activity-drives-bounded-pose",
            legMoves && earsLift && fleeing.heightOffset > 0.f &&
            fleeing.heightOffset < 0.08f &&
            still.heightOffset == 0.f);
        snapshot.type = WildlifeSpecies::MarshBird;
        snapshot.wildlifeActivity = static_cast<int>(WildlifeActivity::Forage);
        snapshot.wildlifeMotionSeconds = 0.f;
        const auto peckA = WildlifePresentation::poseFor(
            snapshot, bird, 0.f, 1.f);
        snapshot.wildlifeMotionSeconds = 0.5f;
        const auto peckB = WildlifePresentation::poseFor(
            snapshot, bird, 0.f, 1.f);
        bool peckChanges = false;
        for (std::size_t index = 0; index < bird.partCount; ++index)
            if (bird.parts[index].role == WildlifeVisualRole::Beak)
                peckChanges = peckB.rotations[index].x >
                    peckA.rotations[index].x + 5.f;
        check("ADVENTURE-WILDLIFE/forage-is-live-not-frozen",
            peckChanges);
    }
    check("ADVENTURE-WILDLIFE/three-distinct-habitats",
        std::string(WildlifeSpecies::forBiome(TerrainBiome::Grassland)) ==
            WildlifeSpecies::Sheep &&
        std::string(WildlifeSpecies::forBiome(TerrainBiome::TemperateForest)) ==
            WildlifeSpecies::Rabbit &&
        std::string(WildlifeSpecies::forBiome(TerrainBiome::Wetland)) ==
            WildlifeSpecies::MarshBird &&
        WildlifeSpecies::forBiome(TerrainBiome::Desert) == nullptr &&
        WildlifeSpecies::forBiome(TerrainBiome::Ocean) == nullptr);
    {
        ensureRuntimeObjectiveRegistry();
        ObjectiveSaveState prepared;
        prepared.completedIds.push_back("progression.craft_iron_sword");
        Player player;
        SandboxEventBus events;
        ObjectiveSystem objectives(runtimeObjectiveRegistry(), player, events,
                                   prepared, 0u, false);
        events.publish(EntityDeathEvent(2, DefaultPlayerActorId, {},
                                        WildlifeSpecies::Sheep));
        const bool excluded =
            objectives.progress("combat.defeat_enemies") == 0;
        events.publish(EntityDeathEvent(3, DefaultPlayerActorId, {},
                                        World::StalkerMobType));
        check("ADVENTURE-WILDLIFE/defeat-objective-excludes-animals",
            excluded && objectives.progress("combat.defeat_enemies") == 1);
    }
    {
        clearDeterministicEnv();
        setEnv("HELLOMINE3D_SEED", "42");
        setEnv("HELLOMINE3D_PLAYER_POSITION", "8.5 201 8.5");
        Config config = makeConfig();
        Camera camera(config);
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("adventure_wildlife_step"), false, 0);
        const glm::vec3 feet{8.5f, 201.f, 8.5f};
        const glm::vec3 half{0.42f, 0.55f, 0.30f};
        glm::vec3 settled{0.f};
        world.setBlock(8, 200, 8, BlockId::Stone);
        world.setBlock(8, 201, 8, BlockId::Air);
        world.setBlock(8, 202, 8, BlockId::Air);
        const bool dry = world.tryWildlifeStep(feet, feet, half, settled) ==
            World::WildlifeStepResult::Allowed && settled.y == 201.f;
        world.setBlock(8, 201, 8, BlockId::Water);
        const bool wetFeet = world.tryWildlifeStep(feet, feet, half,
            settled) == World::WildlifeStepResult::Blocked;
        world.tick(1);
        world.setBlock(8, 201, 8, BlockId::Air);
        world.setBlock(8, 202, 8, BlockId::Stone);
        const bool blockedHead = world.tryWildlifeStep(feet, feet, half,
            settled) == World::WildlifeStepResult::Blocked;
        world.setBlock(8, 202, 8, BlockId::Air);
        world.setBlock(8, 200, 8, BlockId::Water);
        const bool wetGround = world.tryWildlifeStep(feet, feet, half,
            settled) == World::WildlifeStepResult::Blocked;
        world.tick(2);
        world.setBlock(8, 200, 8, BlockId::Stone);
        const bool first = world.tryWildlifeStep(feet, feet, half,
            settled) == World::WildlifeStepResult::Allowed;
        const bool second = world.tryWildlifeStep(feet, feet, half,
            settled) == World::WildlifeStepResult::Allowed;
        const auto beforeDenied = world.collectDebugStats().chunks;
        bool denied = false;
        for (int attempt = 0; attempt < 20 && !denied; ++attempt)
            denied = world.tryWildlifeStep(feet, feet, half, settled) ==
                World::WildlifeStepResult::BudgetDenied;
        const auto afterDenied = world.collectDebugStats();
        check("ADVENTURE-WILDLIFE/dry-support-and-water-head-avoidance",
            dry && wetFeet && wetGround && blockedHead);
        check("ADVENTURE-WILDLIFE/query-cap-denies-before-world-read",
            first && second && denied &&
            afterDenied.wildlifeBlockQueriesUsed ==
                World::WildlifeBlockQueryBudgetPerTick &&
            afterDenied.wildlifeBlockQueriesDenied == 1 &&
            beforeDenied.existingChunks == afterDenied.chunks.existingChunks &&
            beforeDenied.loadedChunks == afterDenied.chunks.loadedChunks);
    }

    const struct Habitat {
        const char *name;
        int x, z;
        const char *species;
    } habitats[] = {
        {"meadow", 1032, 640, WildlifeSpecies::Sheep},
        {"forest", 52, -1152, WildlifeSpecies::Rabbit},
        {"wetland", 1620, -956, WildlifeSpecies::MarshBird}
    };
    for (const Habitat &habitat : habitats) {
        clearDeterministicEnv();
        setEnv("HELLOMINE3D_SEED", "42");
        setEnv("HELLOMINE3D_PLAYER_POSITION",
            std::to_string(habitat.x) + " 200 " +
            std::to_string(habitat.z));
        const std::string saveDirectory =
            freshSaveDirectory(std::string("adventure_wildlife_") +
                               habitat.name);
        Config config = makeConfig();
        Camera camera(config);
        Player player;
        {
            World world(camera, config, player, saveDirectory, false, 3);
            const auto before = world.collectDebugStats().chunks;
            for (int tick = World::WildlifeSpawnIntervalTicks;
                 tick <= 1600;
                 tick += World::WildlifeSpawnIntervalTicks)
                world.tick(tick);
            const auto after = world.collectDebugStats();
            const auto snapshots = world.collectActorSnapshots();
            std::vector<ActorSnapshot> wildlife;
            for (const auto &snapshot : snapshots)
                if (WildlifeSpecies::isWildlife(snapshot.type))
                    wildlife.push_back(snapshot);
            const bool allCorrect = !wildlife.empty() &&
                std::all_of(wildlife.begin(), wildlife.end(),
                    [&habitat](const ActorSnapshot &snapshot) {
                        return snapshot.type == habitat.species;
                    });
            check(std::string("ADVENTURE-WILDLIFE/natural-") +
                    habitat.name,
                allCorrect && wildlife.size() <=
                    World::WildlifeLocalCap,
                "count=" + std::to_string(wildlife.size()) +
                " attempts=" +
                    std::to_string(after.wildlifeSpawnAttempts));
            check(std::string("ADVENTURE-WILDLIFE/no-population-load-") +
                    habitat.name,
                before.existingChunks == after.chunks.existingChunks &&
                before.loadedChunks == after.chunks.loadedChunks);
            check(std::string("ADVENTURE-WILDLIFE/query-budget-") +
                    habitat.name,
                after.wildlifeBlockQueriesUsed <=
                    World::WildlifeBlockQueryBudgetPerTick);

            if (!wildlife.empty()) {
                const auto first = wildlife.front();
                bool sawRest = false, sawForage = false, sawWander = false;
                bool moved = false;
                for (int tick = 1601; tick <= 1800; ++tick) {
                    world.tick(tick);
                    if (tick % 10 != 0) continue;
                    for (const auto &snapshot : world.collectActorSnapshots()) {
                        if (!WildlifeSpecies::isWildlife(snapshot.type))
                            continue;
                        sawRest |= snapshot.wildlifeActivity ==
                            static_cast<int>(WildlifeActivity::Rest);
                        sawForage |= snapshot.wildlifeActivity ==
                            static_cast<int>(WildlifeActivity::Forage);
                        sawWander |= snapshot.wildlifeActivity ==
                            static_cast<int>(WildlifeActivity::Wander);
                        if (snapshot.id == first.id)
                            moved |= glm::distance(snapshot.position,
                                                   first.position) > 0.15f;
                    }
                    check(std::string("ADVENTURE-WILDLIFE/step-budget-") +
                            habitat.name + "-" + std::to_string(tick),
                        world.collectDebugStats().wildlifeBlockQueriesUsed <=
                            World::WildlifeBlockQueryBudgetPerTick);
                }
                check(std::string("ADVENTURE-WILDLIFE/real-activity-") +
                        habitat.name,
                    sawRest && sawForage && sawWander && moved);
                const Actor *current =
                    world.getActorManager().findActor(first.id);
                player.position = (current != nullptr ? current->position :
                                   first.position) + glm::vec3(1.f, 0.f, 0.f);
                player.box.update(player.position);
                for (int tick = 1801; tick <= 1807; ++tick)
                    world.tick(tick);
                const auto *animal = dynamic_cast<const WildlifeActor *>(
                    world.getActorManager().findActor(first.id));
                check(std::string("ADVENTURE-WILDLIFE/flee-") +
                        habitat.name,
                    animal != nullptr &&
                    animal->activity() == WildlifeActivity::Flee &&
                    world.collectDebugStats().wildlifeBlockQueriesUsed <=
                        World::WildlifeBlockQueryBudgetPerTick);

                const bool killed = world.attackActor(first.id, 100.f);
                const auto afterDeath = world.collectActorSnapshots();
                const bool corpseGone = std::none_of(
                    afterDeath.begin(), afterDeath.end(),
                    [&first](const ActorSnapshot &snapshot) {
                        return snapshot.id == first.id;
                    });
                check(std::string("ADVENTURE-WILDLIFE/no-loot-") +
                        habitat.name,
                    killed && corpseGone &&
                    std::none_of(afterDeath.begin(), afterDeath.end(),
                        [](const ActorSnapshot &snapshot) {
                            return snapshot.type == "item";
                        }));
            }
            const auto remaining = world.collectActorSnapshots();
            const auto next = std::find_if(
                remaining.begin(), remaining.end(),
                [](const ActorSnapshot &snapshot) {
                    return WildlifeSpecies::isWildlife(snapshot.type);
                });
            if (next != remaining.end()) {
                const auto cell = World::getChunkXZ(
                    World::toBlockCoord(next->position.x),
                    World::toBlockCoord(next->position.z));
                std::size_t inColumn = 0;
                for (const auto &snapshot : remaining) {
                    const auto actorCell = World::getChunkXZ(
                        World::toBlockCoord(snapshot.position.x),
                        World::toBlockCoord(snapshot.position.z));
                    if (WildlifeSpecies::isWildlife(snapshot.type) &&
                        actorCell.x == cell.x && actorCell.z == cell.z)
                        ++inColumn;
                }
                const auto beforeUnload = world.collectDebugStats();
                const bool unloaded = world.getChunkManager().unloadChunk(
                    cell.x, cell.z);
                const auto afterUnload = world.collectDebugStats();
                check(std::string("ADVENTURE-WILDLIFE/unload-removes-owned-") +
                        habitat.name,
                    unloaded && inColumn > 0 &&
                    afterUnload.wildlifeCount + inColumn ==
                        beforeUnload.wildlifeCount &&
                    afterUnload.wildlifeDespawned ==
                        beforeUnload.wildlifeDespawned + inColumn);
            }
            const bool saved = world.save();
            WorldSaveData data;
            const bool loaded = WorldSave(saveDirectory).load(data);
            check(std::string("ADVENTURE-WILDLIFE/transient-save-") +
                    habitat.name,
                saved && loaded &&
                std::none_of(data.actors.begin(), data.actors.end(),
                    [](const ActorSaveState &state) {
                        return WildlifeSpecies::isWildlife(state.type);
                    }) && data.version == WorldSaveFormatVersion);
        }
        clearDeterministicEnv();
        Player restoredPlayer;
        World restored(camera, config, restoredPlayer,
                       saveDirectory, false, 2);
        const auto restoredActors = restored.collectActorSnapshots();
        check(std::string("ADVENTURE-WILDLIFE/reopen-no-duplicates-") +
                habitat.name,
            std::none_of(restoredActors.begin(), restoredActors.end(),
                [](const ActorSnapshot &snapshot) {
                    return WildlifeSpecies::isWildlife(snapshot.type);
                }));
        for (int tick = 1920; tick <= 2480;
             tick += World::WildlifeSpawnIntervalTicks)
            restored.tick(tick);
        check(std::string("ADVENTURE-WILDLIFE/reopen-regenerates-") +
                habitat.name,
            restored.collectDebugStats().wildlifeCount > 0 &&
            restored.collectDebugStats().wildlifeCount <=
                World::WildlifeLocalCap);
    }
    clearDeterministicEnv();
}
}
