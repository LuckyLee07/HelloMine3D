#pragma once

#include "../Actor/WildlifeActor.h"
#include "../Actor/WildlifePresentation.h"

namespace {
// Optional test-only handoff. Both snapshots and every movement segment are
// published by real WildlifeActor ticks against this World, not reconstructed
// from a renderer formula or inserted into a client diagnostic gallery.
void exportWildlifeVisualOracle(World& world,
    const std::array<ActorSnapshot, 6>& samples)
{
    const char* path = std::getenv("HELLOMINE3D_WILDLIFE_VISUAL_ORACLE");
    if (path == nullptr || *path == '\0') return;
    const auto stone = world.getBlock(10,201,10);
    const bool valid = samples[1].wildlifeMotionHistory.count == 1 &&
        samples[3].wildlifeMotionHistory.count == 2 &&
        samples[5].wildlifeMotionHistory.count == 5 &&
        stone.id == static_cast<Block_t>(BlockId::Stone) &&
        stone.getData().isCollidable && !std::filesystem::exists(path);
    check("WILDLIFE-VISUAL/real-world-oracle-export-preconditions",valid);
    if (!valid) return;
    std::ofstream output(path,std::ios::binary);
    output.precision(9);
    output << "HMWILDLIFE_ORACLE 2\nBLOCK 10 201 10 1\n";
    for (const char* type : {WildlifeSpecies::Sheep,WildlifeSpecies::Rabbit,
                            WildlifeSpecies::MarshBird}) {
        const WildlifeActor shape(900050,type,{0,201,0});
        const auto dimensions = shape.getSnapshot().dimensions;
        output << "SPECIES " << type << ' ' << dimensions.x << ' ' << dimensions.y
               << ' ' << dimensions.z << '\n';
    }
    const auto writeSnapshot = [&](const ActorSnapshot& s) {
        const auto& history = s.wildlifeMotionHistory;
        output << "SNAPSHOT " << s.id << ' ' << s.type << ' '
            << s.position.x << ' ' << s.position.y << ' ' << s.position.z << ' '
            << s.rotation.y << ' ' << s.dimensions.x << ' ' << s.dimensions.y << ' '
            << s.dimensions.z << ' ' << s.wildlifeActivity << ' '
            << s.wildlifeMotionSeconds << ' ' << history.count << ' '
            << history.newestSequence << '\n';
        for (std::size_t i = 0; i < history.count; ++i) {
            const auto& segment = history.segments[i];
            output << "SEG " << segment.sequence << ' '
                << segment.from.x << ' ' << segment.from.y << ' ' << segment.from.z << ' '
                << segment.to.x << ' ' << segment.to.y << ' ' << segment.to.z << ' '
                << segment.seconds << ' ' << int(segment.kind) << '\n';
        }
    };
    const char* names[] = {"ASCENT", "DESCENT", "MULTI_FALL"};
    for (std::size_t i = 0; i < 3; ++i) {
        output << "CASE " << names[i] << '\n';
        writeSnapshot(samples[i * 2]);
        writeSnapshot(samples[i * 2 + 1]);
    }
    output << "END\n";
    output.close();
    check("WILDLIFE-VISUAL/real-world-oracle-exported",bool(output));
}

void caseWildlifeMotionHistory()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "42");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 201 8");
    Config config = makeConfig(); Camera camera(config); Player player;
    World world(camera, config, player,
        freshSaveDirectory("wildlife_motion_history"), false, 0);
    for (int x = 0; x < 16; ++x) for (int z = 0; z < 16; ++z) {
        world.setBlock(x, 200, z, BlockId::Stone);
        for (int y = 201; y <= 210; ++y) world.setBlock(x, y, z, BlockId::Air);
    }
    world.setBlock(10, 201, 10, BlockId::Stone);
    std::array<ActorSnapshot, 6> samples;
    WildlifeActor rabbit(910001, WildlifeSpecies::Rabbit, {11.24f,201.f,10.5f});
    samples[0] = rabbit.getSnapshot();
    player.position = rabbit.position + glm::vec3(1,0,0);
    world.tick(1); rabbit.tick(world, .20f);
    samples[1] = rabbit.getSnapshot();
    const auto& rise = samples[1].wildlifeMotionHistory;
    check("WILDLIFE-PATH/actor-publishes-real-accepted-ascent",
        rise.count == 1 && rise.newestSequence == 1 &&
        rise.segments[0].kind == WildlifeMotionPath::SupportRise &&
        rise.segments[0].from == samples[0].position &&
        rise.segments[0].to == samples[1].position &&
        samples[1].position.y == 202.f && rise.segments[0].seconds == .20f);
    samples[2] = samples[1];
    player.position = rabbit.position - glm::vec3(1,0,0);
    world.tick(2); rabbit.tick(world, .20f);
    samples[3] = rabbit.getSnapshot();
    const auto& descent = samples[3].wildlifeMotionHistory;
    check("WILDLIFE-PATH/actor-publishes-connected-real-descent",
        descent.count == 2 && descent.newestSequence == 2 &&
        descent.segments[1].sequence == 2 &&
        descent.segments[1].kind == WildlifeMotionPath::SupportDescent &&
        descent.segments[1].from == samples[2].position &&
        descent.segments[1].to == samples[3].position && samples[3].position.y == 201.f);
    const auto settled = rabbit.position;
    for (int y = 201; y <= 205; ++y) world.setBlock(12,y,10,BlockId::Stone);
    player.position = rabbit.position - glm::vec3(1,0,0);
    world.tick(3); rabbit.tick(world, .20f);
    const auto blocked = rabbit.getSnapshot();
    check("WILDLIFE-PATH/blocked-motion-does-not-publish-a-segment",
        blocked.position == settled && blocked.wildlifeMotionHistory.newestSequence == 2);
    bool denied = false;
    glm::vec3 unused;
    for (int i = 0; i < 48 && !denied; ++i)
        denied = world.tryWildlifeStep({3.5f,201.f,3.5f}, {3.5f,201.f,3.5f},
            {.22f,.27f,.22f}, unused) == World::WildlifeStepResult::BudgetDenied;
    rabbit.tick(world, .05f);
    const auto budget = rabbit.getSnapshot();
    check("WILDLIFE-PATH/budget-denial-does-not-invent-motion",
        denied && budget.position == settled && budget.wildlifeMotionHistory.newestSequence == 2);
    rabbit.tick(world, 0.f);
    check("WILDLIFE-PATH/paused-actor-keeps-history",
        rabbit.getSnapshot().wildlifeMotionHistory.newestSequence == 2);

    WildlifeActor falling(910002, WildlifeSpecies::Sheep, {5.5f,208.f,5.5f});
    samples[4] = falling.getSnapshot();
    player.position = {14,201,14};
    for (int i = 0; i < 5; ++i) { world.tick(4+i); falling.tick(world, .20f); }
    samples[5] = falling.getSnapshot();
    const auto& falls = samples[5].wildlifeMotionHistory;
    bool actualFalls = falls.count == 5 && falls.newestSequence == 5;
    for (std::size_t i = 0; i < falls.count; ++i) {
        const auto& step = falls.segments[i];
        actualFalls &= step.sequence == i+1 && step.kind == WildlifeMotionPath::AirborneFall &&
            step.from.x == step.to.x && step.from.z == step.to.z &&
            step.from.y-step.to.y > 0.f && step.from.y-step.to.y <= .8001f &&
            (i == 0 ? step.from == samples[4].position : step.from == falls.segments[i-1].to);
    }
    check("WILDLIFE-PATH/skipped-frame-copies-five-actual-falls", actualFalls);
    check("WILDLIFE-PATH/multi-fall-is-not-a-support-step",
        actualFalls && samples[4].position.y-samples[5].position.y > 3.f);
    // The earlier snapshot is a value copy, not a live/consuming view.
    check("WILDLIFE-PATH/snapshot-copy-keeps-original-history",
        samples[1].wildlifeMotionHistory.count == 1 &&
        samples[2].wildlifeMotionHistory.newestSequence == 1 &&
        samples[4].wildlifeMotionHistory.count == 0);
    exportWildlifeVisualOracle(world, samples);
    for (int i = 5; i < 14; ++i) { world.tick(4+i); falling.tick(world, .20f); }
    const auto retained = falling.getSnapshot().wildlifeMotionHistory;
    bool bounded = retained.count == WildlifeMotionHistory::MaximumSegments &&
        retained.newestSequence > WildlifeMotionHistory::MaximumSegments;
    for (std::size_t i = 0; i < retained.count; ++i)
        bounded &= retained.segments[i].sequence == retained.newestSequence-retained.count+i+1 &&
            (i == 0 || retained.segments[i].from == retained.segments[i-1].to);
    check("WILDLIFE-PATH/actual-history-is-bounded-and-chronological", bounded);
    check("WILDLIFE-PATH/retained-export-remains-a-value-copy",
        samples[5].wildlifeMotionHistory.count == 5 &&
        samples[5].wildlifeMotionHistory.newestSequence == 5);
    clearDeterministicEnv();
}

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

// Production-cadence fixtures below enter ActorManager and call only World::tick.
// Test observations read actual blocks; they do not supply movement/path output.
bool wildlifeBodyIsDryAndClear(World& world, const ActorSnapshot& snapshot)
{
    constexpr float inset = .001f;
    const auto& p = snapshot.position;
    const auto& h = snapshot.dimensions;
    for (int x = World::toBlockCoord(p.x-h.x+inset);
         x <= World::toBlockCoord(p.x+h.x-inset); ++x)
        for (int z = World::toBlockCoord(p.z-h.z+inset);
             z <= World::toBlockCoord(p.z+h.z-inset); ++z)
            for (int y = World::toBlockCoord(p.y+inset);
                 y <= World::toBlockCoord(p.y+2.f*h.y-inset); ++y) {
                const auto block = world.getBlock(x,y,z);
                if (block.getData().isCollidable ||
                    block.id == static_cast<Block_t>(BlockId::Water)) return false;
            }
    return true;
}

bool wildlifeHasHighestDrySupport(World& world, const ActorSnapshot& snapshot)
{
    constexpr float inset = .001f;
    const auto& p = snapshot.position;
    const auto& h = snapshot.dimensions;
    const int feet = static_cast<int>(std::round(p.y));
    if (std::abs(p.y-float(feet)) > .0001f) return false;
    bool rootSupported = false;
    for (int x = World::toBlockCoord(p.x-h.x+inset);
         x <= World::toBlockCoord(p.x+h.x-inset); ++x)
        for (int z = World::toBlockCoord(p.z-h.z+inset);
             z <= World::toBlockCoord(p.z+h.z-inset); ++z) {
            bool cornerSupported = false;
            for (int y : {feet-1,feet-2}) {
                const auto block = world.getBlock(x,y,z);
                if (block.id == static_cast<Block_t>(BlockId::Water) ||
                    block.id == static_cast<Block_t>(BlockId::OakLeaf)) return false;
                if (block.getData().isCollidable) {
                    cornerSupported = true;
                    rootSupported |= y == feet-1;
                    break;
                }
            }
            if (!cornerSupported) return false;
        }
    return rootSupported;
}

struct WildlifeCadenceCase {
    std::string species;
    std::string activity;
    std::vector<std::array<int,3>> blocks;
    std::vector<ActorSnapshot> snapshots;
};

void exportWildlifeCadenceOracle(const std::vector<WildlifeCadenceCase>& cases)
{
    const char* path = std::getenv("HELLOMINE3D_WILDLIFE_CADENCE_ORACLE");
    if (path == nullptr || *path == '\0') return;
    const bool valid = cases.size() == 6 && !std::filesystem::exists(path) &&
        std::all_of(cases.begin(),cases.end(),[](const WildlifeCadenceCase& fixture) {
            return fixture.blocks.size() == 36 && fixture.snapshots.size() > 4 &&
                fixture.snapshots.size() <= 161 &&
                fixture.snapshots.front().wildlifeMotionHistory.newestSequence == 0;
        });
    check("WILDLIFE-CADENCE/real-world-export-preconditions",valid);
    if (!valid) return;
    std::ofstream output(path,std::ios::binary);
    output.precision(9);
    output << "HMWILDLIFE_CADENCE 1\n";
    for (const auto& fixture : cases) {
        output << "CASE " << fixture.species << ' ' << fixture.activity << ' '
               << fixture.blocks.size() << ' ' << fixture.snapshots.size() << '\n';
        for (const auto& block : fixture.blocks)
            output << "BLOCK " << block[0] << ' ' << block[1] << ' ' << block[2]
                   << " 1\n";
        for (const auto& snapshot : fixture.snapshots) {
            const auto& history = snapshot.wildlifeMotionHistory;
            output << "SNAPSHOT " << snapshot.id << ' ' << snapshot.type << ' '
                << snapshot.position.x << ' ' << snapshot.position.y << ' '
                << snapshot.position.z << ' ' << snapshot.rotation.y << ' '
                << snapshot.dimensions.x << ' ' << snapshot.dimensions.y << ' '
                << snapshot.dimensions.z << ' ' << snapshot.wildlifeActivity << ' '
                << snapshot.wildlifeMotionSeconds << ' ' << history.count << ' '
                << history.newestSequence << '\n';
            for (std::size_t i = 0; i < history.count; ++i) {
                const auto& segment = history.segments[i];
                output << "SEG " << segment.sequence << ' ' << segment.from.x << ' '
                    << segment.from.y << ' ' << segment.from.z << ' ' << segment.to.x
                    << ' ' << segment.to.y << ' ' << segment.to.z << ' '
                    << segment.seconds << ' ' << int(segment.kind) << '\n';
            }
        }
        output << "ENDCASE\n";
    }
    output << "END\n";
    output.close();
    check("WILDLIFE-CADENCE/real-world-exported",bool(output));
}

void caseWildlifeProductionCadence()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED","42");
    setEnv("HELLOMINE3D_PLAYER_POSITION","8 201 8");
    Config config = makeConfig(); Camera camera(config); Player player;
    World world(camera,config,player,
        freshSaveDirectory("wildlife_production_cadence"),false,0);
    for (int x = 0; x < 16; ++x) for (int z = 0; z < 16; ++z) {
        world.setBlock(x,200,z,BlockId::Stone);
        for (int y = 201; y <= 210; ++y) world.setBlock(x,y,z,BlockId::Air);
        if (x >= 6 && x <= 8) world.setBlock(x,201,z,BlockId::Stone);
    }
    std::vector<WildlifeCadenceCase> cases;
    for (const char* species : {WildlifeSpecies::Sheep,WildlifeSpecies::Rabbit,
                               WildlifeSpecies::MarshBird})
        for (const bool fleeing : {true,false}) {
            world.getActorManager().removeActorsIf([](const Actor&) { return true; });
            // 1413: phase 6, heading 1.57. Wander crosses the same real stair
            // without an alarm, a manually changed heading, or an actor tick.
            constexpr ActorId id = 1413;
            const bool registered = world.getActorManager().addActor(
                std::make_unique<WildlifeActor>(id,species,glm::vec3{5.5f,201.f,8.5f}),
                world) == id;
            const std::string label = std::string(species) + (fleeing ? "/flee" : "/wander");
            check("WILDLIFE-CADENCE/registered-"+label,registered);
            if (!registered) continue;
            WildlifeCadenceCase fixture{species,fleeing ? "FLEE" : "WANDER",{}, {}};
            bool blockIdentity = true;
            for (int x = 4; x <= 12; ++x) for (int z = 7; z <= 9; ++z)
                for (int y = 200; y <= 203; ++y) {
                    const auto block = world.getBlock(x,y,z);
                    if (block.getData().isCollidable) {
                        blockIdentity &= block.id == static_cast<Block_t>(BlockId::Stone);
                        fixture.blocks.push_back({x,y,z});
                    }
                }
            fixture.snapshots.push_back(world.getActorManager().findActor(id)->getSnapshot());
            bool bodyClear = true, highestSupport = true, correctActivity = true;
            bool chronological = true, cadence = true, budgetBounded = true;
            bool rise = false, descent = false;
            int movingTicks = 0, firstInvalidTick = 0;
            const float fleeSpeed = species == WildlifeSpecies::Rabbit ? 3.f :
                species == WildlifeSpecies::MarshBird ? 2.1f : 2.25f;
            const float wanderSpeed = species == WildlifeSpecies::Rabbit ? 1.35f :
                species == WildlifeSpecies::MarshBird ? .70f : .75f;
            for (int tick = 1; tick <= 160; ++tick) {
                world.getActorManager().removeActorsIf([](const Actor& actor) {
                    return actor.getId() != id;
                });
                const auto before = fixture.snapshots.back();
                player.position = fleeing ? before.position-glm::vec3(1,0,0)
                                           : glm::vec3{1,201,1};
                world.tick(12000+tick); // Runtime resets budget and ticks the actor at .05.
                const auto after = world.getActorManager().findActor(id)->getSnapshot();
                fixture.snapshots.push_back(after);
                const auto stats = world.collectDebugStats();
                budgetBounded &= stats.wildlifeBlockQueriesUsed <=
                    World::WildlifeBlockQueryBudgetPerTick && stats.wildlifeBlockQueriesDenied == 0;
                bodyClear &= wildlifeBodyIsDryAndClear(world,after);
                highestSupport &= wildlifeHasHighestDrySupport(world,after);
                const auto expected = tick < 4 ? WildlifeActivity::Rest :
                    fleeing ? WildlifeActivity::Flee : WildlifeActivity::Wander;
                correctActivity &= after.wildlifeActivity == static_cast<int>(expected);
                const bool moved = after.position != before.position;
                movingTicks += moved;
                const auto& history = after.wildlifeMotionHistory;
                chronological &= history.newestSequence ==
                    before.wildlifeMotionHistory.newestSequence + (moved ? 1u : 0u);
                if (moved) {
                    const auto& segment = history.segments[history.count-1];
                    chronological &= segment.from == before.position && segment.to == after.position &&
                        segment.sequence == history.newestSequence;
                    cadence &= std::abs(segment.seconds-.05f) < .000001f;
                    const float horizontal = std::hypot(after.position.x-before.position.x,
                                                        after.position.z-before.position.z);
                    cadence &= horizontal > 0.f && horizontal <=
                        (fleeing ? fleeSpeed : wanderSpeed)*.05f+.00001f;
                    const float dy = after.position.y-before.position.y;
                    const auto expectedKind = dy > 0.f ? WildlifeMotionPath::SupportRise :
                        dy < 0.f ? WildlifeMotionPath::SupportDescent : WildlifeMotionPath::GroundedLevel;
                    chronological &= segment.kind == expectedKind;
                    rise |= segment.kind == WildlifeMotionPath::SupportRise;
                    descent |= segment.kind == WildlifeMotionPath::SupportDescent;
                }
                cadence &= tick < 4 ? !moved : moved;
                if (firstInvalidTick == 0 && !(bodyClear && highestSupport && correctActivity &&
                                               chronological && cadence && budgetBounded))
                    firstInvalidTick = tick;
                if (after.position.x >= 10.f && rise && descent) break;
            }
            const auto detail = "snapshots="+std::to_string(fixture.snapshots.size())+
                " moves="+std::to_string(movingTicks)+" first_invalid_tick="+std::to_string(firstInvalidTick);
            check("WILDLIFE-CADENCE/actual-world-blocks-"+label,blockIdentity && fixture.blocks.size() == 36);
            check("WILDLIFE-CADENCE/body-clear-every-tick-"+label,bodyClear,detail);
            check("WILDLIFE-CADENCE/highest-real-dry-support-"+label,highestSupport,detail);
            check("WILDLIFE-CADENCE/real-rest-and-activity-"+label,correctActivity,detail);
            check("WILDLIFE-CADENCE/chronological-actual-path-"+label,chronological,detail);
            check("WILDLIFE-CADENCE/uninterrupted-normal-20hz-"+label,cadence && movingTicks > 20,detail);
            check("WILDLIFE-CADENCE/query-budget-without-denial-"+label,budgetBounded,detail);
            check("WILDLIFE-CADENCE/crosses-up-and-down-"+label,rise && descent &&
                fixture.snapshots.back().position.x >= 10.f && fixture.snapshots.back().position.y == 201.f,detail);
            cases.push_back(std::move(fixture));
        }
    exportWildlifeCadenceOracle(cases);
    clearDeterministicEnv();
}

void caseWildlifeMixedSupportBoundaries()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED","42");
    setEnv("HELLOMINE3D_PLAYER_POSITION","8 201 8");
    Config config = makeConfig(); Camera camera(config); Player player;
    World world(camera,config,player,
        freshSaveDirectory("wildlife_mixed_support_boundaries"),false,0);
    for (int x = 0; x < 16; ++x) for (int z = 0; z < 16; ++z) {
        world.setBlock(x,197,z,BlockId::Stone);
        for (int y = 198; y <= 210; ++y) world.setBlock(x,y,z,BlockId::Air);
        world.setBlock(x,200,z,BlockId::Stone);
    }
    int tick = 12200;
    const auto reset = [&]() {
        world.getActorManager().removeActorsIf([](const Actor&) { return true; });
        for (int x = 4; x <= 12; ++x) for (int z = 7; z <= 12; ++z) {
            for (int y = 198; y <= 205; ++y) world.setBlock(x,y,z,BlockId::Air);
            world.setBlock(x,200,z,BlockId::Stone);
        }
        world.tick(++tick);
    };
    const glm::vec3 from{5.5f,201.f,8.5f}, target{5.95f,201.f,8.95f};
    const glm::vec3 half{.42f,.55f,.30f};
    const auto blocked = [&](const std::string& name,const glm::vec3& start,
                             const glm::vec3& end,const glm::vec3& dimensions) {
        glm::vec3 settled{123,222,234};
        bool grounded = true;
        WildlifeMotionPath kind = WildlifeMotionPath::GroundedLevel;
        const auto result = world.tryWildlifeStep(start,end,dimensions,settled,&grounded,&kind);
        check("WILDLIFE-BOUNDARY/"+name,result == World::WildlifeStepResult::Blocked &&
            settled == glm::vec3(123,222,234) && !grounded && kind == WildlifeMotionPath::None);
    };
    reset();
    world.setBlock(6,201,8,BlockId::Stone);
    world.setBlock(6,201,9,BlockId::Stone);
    for (int x : {5,6}) for (int z : {8,9}) world.setBlock(x,204,z,BlockId::OakLeaf);
    glm::vec3 settled{0}; bool grounded = false;
    WildlifeMotionPath kind = WildlifeMotionPath::None;
    const bool highCanopy = world.tryWildlifeStep(from,target,half,settled,&grounded,&kind) ==
        World::WildlifeStepResult::Allowed && grounded && settled.y == 202.f &&
        kind == WildlifeMotionPath::SupportRise;
    check("WILDLIFE-BOUNDARY/mixed-stair-under-clear-high-canopy",highCanopy &&
        wildlifeBodyIsDryAndClear(world,WildlifeActor(1400,WildlifeSpecies::Sheep,settled).getSnapshot()));

    for (const int holes : {1,2}) {
        reset();
        for (int y = 199; y <= 201; ++y) {
            world.setBlock(6,y,9,BlockId::Air);
            if (holes == 2) world.setBlock(6,y,8,BlockId::Air);
        }
        blocked("missing-target-corners-"+std::to_string(holes),from,target,half);
    }
    reset();
    for (int y = 199; y <= 201; ++y) world.setBlock(6,y,8,BlockId::Air);
    blocked("all-air-target-is-not-grounded",from,{6.1f,201.f,8.5f},{.10f,.27f,.10f});
    for (const auto support : {BlockId::Water,BlockId::OakLeaf}) {
        reset(); world.setBlock(6,200,9,support);
        blocked(support == BlockId::Water ? "wet-target-corner" : "leaf-target-corner",
                from,target,half);
    }
    reset(); world.setBlock(6,201,9,BlockId::Water);
    blocked("water-in-target-body",from,target,half);
    reset();
    world.setBlock(6,201,8,BlockId::Stone);
    world.setBlock(5,200,9,BlockId::Air);
    world.setBlock(5,199,9,BlockId::Stone);
    blocked("real-corner-support-spread-two",from,target,half);
    reset();
    world.setBlock(6,201,8,BlockId::Stone);
    world.setBlock(6,201,9,BlockId::Stone);
    world.setBlock(5,203,8,BlockId::Stone);
    blocked("too-low-roof-on-rise",from,target,half);
    reset(); world.setBlock(5,201,8,BlockId::Stone);
    blocked("supported-source-body-embedded",from,target,half);
    reset();
    blocked("tiny-x-empty-inset-range",{5,201,8.5f},{5,201,8.5f},{.0001f,.27f,.22f});
    blocked("tiny-z-empty-inset-range",{5.5f,201,8},{5.5f,201,8},{.22f,.27f,.0001f});
    check("WILDLIFE-BOUNDARY/empty-inset-reads-no-world-blocks",
        world.collectDebugStats().wildlifeBlockQueriesUsed == 0);
    reset();
    world.setBlock(5,254,8,BlockId::Stone);
    world.setBlock(5,255,8,BlockId::Air);
    blocked("supported-body-top-outside-world",{5.5f,255.f,8.5f},
            {5.5f,255.f,8.5f},half);
    reset();
    world.setBlock(6,201,8,BlockId::Stone);
    world.setBlock(6,202,8,BlockId::Stone);
    blocked("two-block-wall",from,{5.95f,201.f,8.5f},half);
    reset();
    world.setBlock(11,201,11,BlockId::Stone);
    world.setBlock(11,202,11,BlockId::Stone);
    blocked("swept-diagonal-corner-cannot-be-skipped",{10.75f,201.f,11.75f},
            {11.25f,201.f,12.25f},{.22f,.27f,.22f});

    reset();
    // This fixture intentionally crosses (-1,0)/(0,0). The World above starts
    // with preload radius zero; prepare its real neighboring chunk explicitly.
    // Movement itself must neither load a chunk nor fabricate missing blocks.
    world.getChunkManager().loadChunk(-1,0);
    for (int x = -2; x <= 1; ++x) for (int z = 7; z <= 9; ++z) {
        world.setBlock(x,200,z,BlockId::Stone);
        for (int y = 201; y <= 205; ++y) world.setBlock(x,y,z,BlockId::Air);
    }
    world.setBlock(0,201,8,BlockId::Stone);
    const auto* negativeChunk = world.getChunkManager().findChunk(-1,0);
    check("WILDLIFE-BOUNDARY/negative-crossing-fixture-really-resident",
        negativeChunk != nullptr && negativeChunk->hasLoaded() &&
        world.getBlock(-1,200,8).id == static_cast<Block_t>(BlockId::Stone) &&
        world.getBlock(0,201,8).id == static_cast<Block_t>(BlockId::Stone));
    const auto beforeCrossing = world.collectDebugStats();
    const auto contactResult = world.tryWildlifeStep({-.42f,201,8.5f},
        {-.42f,201,8.5f},half,settled);
    const glm::vec3 contactFeet = settled;
    const bool exactContact = contactResult == World::WildlifeStepResult::Allowed && settled.y == 201.f;
    world.tick(++tick);
    const auto crossingResult = world.tryWildlifeStep({-.42f,201,8.5f},
        {-.35f,201,8.5f},half,settled,&grounded,&kind);
    const bool crossChunk = crossingResult == World::WildlifeStepResult::Allowed &&
        grounded && settled.y == 202.f && kind == WildlifeMotionPath::SupportRise;
    const auto afterCrossing = world.collectDebugStats();
    check("WILDLIFE-BOUNDARY/negative-grid-contact-and-real-chunk-crossing",
        world.getBlock(-1,200,8).id == static_cast<Block_t>(BlockId::Stone) &&
        exactContact && crossChunk &&
        beforeCrossing.chunks.loadedChunks == afterCrossing.chunks.loadedChunks &&
        beforeCrossing.chunks.existingChunks == afterCrossing.chunks.existingChunks,
        "contact_result="+std::to_string(static_cast<int>(contactResult))+
        " contact_feet="+vecToString(contactFeet)+
        " crossing_result="+std::to_string(static_cast<int>(crossingResult))+
        " crossing_feet="+vecToString(settled)+" grounded="+std::to_string(grounded)+
        " path_kind="+std::to_string(static_cast<int>(kind)));

    reset();
    glm::vec3 unused;
    bool exhausted = false;
    for (int attempt = 0; attempt < 49 && !exhausted; ++attempt)
        exhausted = world.tryWildlifeStep(from,from,half,unused) ==
            World::WildlifeStepResult::BudgetDenied;
    const auto beforeDenied = world.collectDebugStats();
    settled = {123,222,234}; grounded = true; kind = WildlifeMotionPath::SupportRise;
    const auto denied = world.tryWildlifeStep(from,target,half,settled,&grounded,&kind);
    const auto afterDenied = world.collectDebugStats();
    check("WILDLIFE-BOUNDARY/exhaustion-denies-before-mutation-or-read",exhausted &&
        denied == World::WildlifeStepResult::BudgetDenied && settled == glm::vec3(123,222,234) &&
        !grounded && kind == WildlifeMotionPath::None &&
        beforeDenied.wildlifeBlockQueriesUsed == World::WildlifeBlockQueryBudgetPerTick &&
        afterDenied.wildlifeBlockQueriesUsed == beforeDenied.wildlifeBlockQueriesUsed &&
        afterDenied.wildlifeBlockQueriesDenied == beforeDenied.wildlifeBlockQueriesDenied+1 &&
        beforeDenied.chunks.loadedChunks == afterDenied.chunks.loadedChunks &&
        beforeDenied.chunks.existingChunks == afterDenied.chunks.existingChunks);

    world.tick(++tick);
    const auto distantChunk = World::getChunkXZ(1000000,1000000);
    const auto* resident = world.getChunkManager().findChunk(distantChunk.x,distantChunk.z);
    const bool missing = resident == nullptr || !resident->hasLoaded();
    const auto beforeMissing = world.collectDebugStats();
    settled = {123,222,234}; grounded = true; kind = WildlifeMotionPath::SupportRise;
    const glm::vec3 distant{1000000.5f,201.f,1000000.5f};
    const auto unknown = world.tryWildlifeStep(distant,distant,half,settled,&grounded,&kind);
    const auto afterMissing = world.collectDebugStats();
    check("WILDLIFE-BOUNDARY/unknown-column-blocks-without-synchronous-load",missing &&
        unknown == World::WildlifeStepResult::Blocked && settled == glm::vec3(123,222,234) &&
        !grounded && kind == WildlifeMotionPath::None &&
        afterMissing.chunks.loadedChunks == beforeMissing.chunks.loadedChunks &&
        afterMissing.chunks.existingChunks == beforeMissing.chunks.existingChunks &&
        afterMissing.wildlifeBlockQueriesUsed == 1 && afterMissing.wildlifeBlockQueriesDenied == 0);
    clearDeterministicEnv();
}

void caseWildlifeAutomaticObstacleAndBudget()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED","42");
    setEnv("HELLOMINE3D_PLAYER_POSITION","8 201 8");
    Config config = makeConfig(); Camera camera(config); Player player;
    World world(camera,config,player,
        freshSaveDirectory("wildlife_automatic_obstacle_budget"),false,0);
    const auto reset = [&]() {
        world.getActorManager().removeActorsIf([](const Actor&) { return true; });
        for (int x = 0; x < 16; ++x) for (int z = 0; z < 16; ++z) {
            world.setBlock(x,197,z,BlockId::Stone);
            for (int y = 198; y <= 210; ++y) world.setBlock(x,y,z,BlockId::Air);
            world.setBlock(x,200,z,BlockId::Stone);
        }
    };
    int clock = 12400;
    for (const char* species : {WildlifeSpecies::Sheep,WildlifeSpecies::Rabbit,
                               WildlifeSpecies::MarshBird})
        for (const bool diagonal : {false,true}) {
            reset();
            if (diagonal) {
                world.setBlock(8,201,8,BlockId::Stone);
                world.setBlock(8,202,8,BlockId::Stone);
            }
            else for (int z = 0; z < 16; ++z) {
                world.setBlock(8,201,z,BlockId::Stone);
                world.setBlock(8,202,z,BlockId::Stone);
            }
            constexpr ActorId id = 1413;
            const glm::vec3 origin = diagonal ? glm::vec3{7.5f,201.f,7.5f}
                                             : glm::vec3{7.35f,201.f,8.5f};
            world.getActorManager().addActor(std::make_unique<WildlifeActor>(id,species,origin),world);
            bool clear = true, supported = true, bounded = true, fallback = false;
            int blockedTicks = 0;
            ActorSnapshot firstBlocked;
            bool haveBlocked = false;
            bool progressedAfterBlock = false;
            float realStepLength = 0.f;
            std::ostringstream trajectory;
            trajectory.precision(9);
            for (int tick = 1; tick <= 48; ++tick) {
                world.getActorManager().removeActorsIf([](const Actor& actor) {
                    return actor.getId() != id;
                });
                const auto before = world.getActorManager().findActor(id)->getSnapshot();
                player.position = before.position-(diagonal ? glm::vec3(1,0,1) : glm::vec3(1,0,0));
                world.tick(++clock);
                const auto after = world.getActorManager().findActor(id)->getSnapshot();
                clear &= wildlifeBodyIsDryAndClear(world,after);
                supported &= wildlifeHasHighestDrySupport(world,after);
                bounded &= world.collectDebugStats().wildlifeBlockQueriesUsed <=
                    World::WildlifeBlockQueryBudgetPerTick;
                const auto delta = after.position-before.position;
                blockedTicks += tick >= 4 && delta == glm::vec3(0);
                if (tick >= 4 && delta == glm::vec3(0) && !haveBlocked) {
                    firstBlocked = before;
                    haveBlocked = true;
                }
                progressedAfterBlock |= haveBlocked && delta != glm::vec3(0);
                if (realStepLength == 0.f && delta != glm::vec3(0))
                    realStepLength = std::hypot(delta.x,delta.z);
                if (tick <= 12 || tick == 48)
                    trajectory << " {tick=" << tick << " feet=" << after.position.x << ','
                        << after.position.y << ',' << after.position.z << " yaw="
                        << after.rotation.y << " sequence=" << after.wildlifeMotionHistory.newestSequence
                        << " queries=" << world.collectDebugStats().wildlifeBlockQueriesUsed << '}';
                // Existing blocked-turn logic can produce tangent steps; this
                // is bounded local behavior, not a new route finder.
                fallback |= diagonal ? std::abs(delta.x-delta.z) > .02f
                                     : std::abs(delta.z) > .02f;
            }
            const std::string label = std::string(species)+(diagonal ? "/diagonal" : "/wall");
            check("WILDLIFE-OBSTACLE/automatic-body-clear-"+label,clear && supported && bounded);
            std::ostringstream alternatives;
            alternatives.precision(9);
            if (diagonal && species == WildlifeSpecies::Sheep && haveBlocked) {
                // Diagnostics only: ask the actual World about independent
                // directions at the first real blocked position. These probes
                // do not turn the fixture actor or count as accepted actor paths.
                world.getActorManager().removeActorsIf([](const Actor&) { return true; });
                const float heading = glm::radians(firstBlocked.rotation.y);
                for (const float turn : {0.f,1.57f,-1.57f,3.14159265f}) {
                    world.tick(++clock);
                    glm::vec3 candidate = firstBlocked.position;
                    candidate.x += std::sin(heading+turn)*realStepLength;
                    candidate.z -= std::cos(heading+turn)*realStepLength;
                    glm::vec3 endpoint{123,222,234};
                    const auto result = world.tryWildlifeStep(firstBlocked.position,candidate,
                        firstBlocked.dimensions,endpoint);
                    alternatives << " {turn=" << turn << " result=" << static_cast<int>(result)
                        << " from=" << vecToString(firstBlocked.position) << " candidate="
                        << vecToString(candidate) << " endpoint=" << vecToString(endpoint) << '}';
                }
            }
            check("WILDLIFE-OBSTACLE/real-block-and-tangent-motion-"+label,
                blockedTicks > 0 && fallback,"blocked_ticks="+std::to_string(blockedTicks)+
                " real_trajectory="+trajectory.str()+" world_direction_probes="+alternatives.str());
            check("WILDLIFE-OBSTACLE/real-progress-after-first-block-"+label,
                haveBlocked && progressedAfterBlock);
        }

    // The blocked-intention restoration has a separate Wander path: unlike
    // Flee, ordinary phase 6 does not recompute a player-relative direction.
    // 4003 has heading 2.35 radians and phase 6, reaching the same asymmetric
    // corner with real slow steps and no test-written direction.
    reset();
    world.setBlock(8,201,8,BlockId::Stone);
    world.setBlock(8,202,8,BlockId::Stone);
    constexpr ActorId wanderId = 4003;
    const bool wanderRegistered = world.getActorManager().addActor(
        std::make_unique<WildlifeActor>(wanderId,WildlifeSpecies::Sheep,
            glm::vec3{7.5f,201.f,7.5f}),world) == wanderId;
    bool realWander = wanderRegistered, clearWander = wanderRegistered;
    bool wanderHistory = wanderRegistered, blockedWander = false;
    bool wanderedAfterBlock = false, tangentAfterBlock = false;
    int wanderBlockedTicks = 0;
    std::ostringstream wanderTrace;
    wanderTrace.precision(9);
    if (wanderRegistered) for (int tick = 1; tick <= 48; ++tick) {
        world.getActorManager().removeActorsIf([](const Actor& actor) {
            return actor.getId() != wanderId;
        });
        player.position = {1,201,1};
        const auto before = world.getActorManager().findActor(wanderId)->getSnapshot();
        world.tick(++clock);
        const auto after = world.getActorManager().findActor(wanderId)->getSnapshot();
        realWander &= after.wildlifeActivity == static_cast<int>(
            tick < 4 ? WildlifeActivity::Rest : WildlifeActivity::Wander);
        const auto stats = world.collectDebugStats();
        clearWander &= wildlifeBodyIsDryAndClear(world,after) &&
            wildlifeHasHighestDrySupport(world,after) &&
            stats.wildlifeBlockQueriesUsed <= World::WildlifeBlockQueryBudgetPerTick &&
            stats.wildlifeBlockQueriesDenied == 0;
        const auto delta = after.position-before.position;
        const bool moved = delta != glm::vec3(0);
        if (tick >= 4 && !moved) { blockedWander = true; ++wanderBlockedTicks; }
        wanderedAfterBlock |= blockedWander && moved;
        tangentAfterBlock |= blockedWander && moved && std::abs(delta.x-delta.z) > .02f;
        const auto& history = after.wildlifeMotionHistory;
        wanderHistory &= history.newestSequence ==
            before.wildlifeMotionHistory.newestSequence+(moved ? 1u : 0u);
        if (moved) {
            if (history.count == 0) wanderHistory = false;
            else {
                const auto& segment = history.segments[history.count-1];
                wanderHistory &= segment.from == before.position && segment.to == after.position &&
                    segment.sequence == history.newestSequence &&
                    segment.kind == WildlifeMotionPath::GroundedLevel &&
                    std::abs(segment.seconds-.05f) < .000001f;
            }
        }
        if (tick <= 12 || tick == 48)
            wanderTrace << " {tick=" << tick << " feet=" << after.position.x << ','
                << after.position.y << ',' << after.position.z << " yaw=" << after.rotation.y
                << " sequence=" << history.newestSequence << " queries="
                << stats.wildlifeBlockQueriesUsed << '}';
    }
    const std::string wanderDetail = "blocked_ticks="+std::to_string(wanderBlockedTicks)+
        " real_trajectory="+wanderTrace.str();
    check("WILDLIFE-OBSTACLE/registered-real-wander-without-alarm",realWander,wanderDetail);
    check("WILDLIFE-OBSTACLE/wander-corner-body-support-and-budget",clearWander,wanderDetail);
    check("WILDLIFE-OBSTACLE/wander-recovers-real-tangent-after-block",blockedWander &&
        wanderedAfterBlock && tangentAfterBlock,wanderDetail);
    check("WILDLIFE-OBSTACLE/wander-publishes-actual-normal-cadence-history",wanderHistory,wanderDetail);

    reset();
    constexpr ActorId fallingId = 1400; // Rest phase, outside the player's alarm range.
    world.getActorManager().addActor(std::make_unique<WildlifeActor>(fallingId,
        WildlifeSpecies::Sheep,glm::vec3{5.5f,201.f,5.5f}),world);
    player.position = {14,201,14};
    for (int tick = 0; tick < 4; ++tick) world.tick(++clock);
    world.setBlock(5,200,5,BlockId::Air);
    bool verticalFalls = false, clearFall = true;
    for (int tick = 0; tick < 60; ++tick) {
        world.getActorManager().removeActorsIf([](const Actor& actor) {
            return actor.getId() != fallingId;
        });
        const auto before = world.getActorManager().findActor(fallingId)->getSnapshot();
        world.tick(++clock);
        const auto after = world.getActorManager().findActor(fallingId)->getSnapshot();
        clearFall &= wildlifeBodyIsDryAndClear(world,after);
        if (after.wildlifeMotionHistory.newestSequence > before.wildlifeMotionHistory.newestSequence) {
            const auto& segment = after.wildlifeMotionHistory.segments[after.wildlifeMotionHistory.count-1];
            verticalFalls |= segment.kind == WildlifeMotionPath::AirborneFall &&
                segment.to.x == segment.from.x && segment.to.z == segment.from.z &&
                segment.to.y < segment.from.y;
        }
    }
    const auto landed = world.getActorManager().findActor(fallingId)->getSnapshot();
    check("WILDLIFE-OBSTACLE/automatic-removed-support-falls-and-lands",verticalFalls &&
        clearFall && landed.position.y == 198.f && wildlifeHasHighestDrySupport(world,landed));

    for (const int population : {12,24}) {
        reset();
        for (int x = 6; x <= 8; ++x) for (int z = 0; z < 16; ++z)
            world.setBlock(x,201,z,BlockId::Stone);
        std::vector<ActorId> ids;
        std::vector<int> latestMove(population,0), moveCount(population,0);
        bool registered = true;
        const char* species[] = {WildlifeSpecies::Sheep,WildlifeSpecies::Rabbit,WildlifeSpecies::MarshBird};
        for (int i = 0; i < population; ++i) {
            const ActorId id = 1413u+static_cast<ActorId>(i)*4396u;
            ids.push_back(id);
            registered &= world.getActorManager().addActor(std::make_unique<WildlifeActor>(
                id,species[i%3],glm::vec3{5.5f,201.f,8.5f}),world) == id;
        }
        bool bounded = true, bodyClear = true, supported = true, actualHistory = true;
        bool durationBounded = true, deniedUnchanged = true, sawRise = false;
        int maximumGap = 0, deniedTicks = 0, unchangedMovingTicks = 0;
        float maximumSeconds = 0.f, maximumDistance = 0.f;
        for (int tick = 1; tick <= 60; ++tick) {
            world.getActorManager().removeActorsIf([&](const Actor& actor) {
                return std::find(ids.begin(),ids.end(),actor.getId()) == ids.end();
            });
            std::vector<ActorSnapshot> before;
            float minimumX = 16.f;
            for (const auto id : ids) {
                before.push_back(world.getActorManager().findActor(id)->getSnapshot());
                minimumX = std::min(minimumX,before.back().position.x);
            }
            player.position = {minimumX-1.f,201.f,8.5f};
            world.tick(++clock);
            const auto stats = world.collectDebugStats();
            bounded &= stats.wildlifeBlockQueriesUsed <= World::WildlifeBlockQueryBudgetPerTick;
            deniedTicks += stats.wildlifeBlockQueriesDenied > 0;
            for (int i = 0; i < population; ++i) {
                const auto after = world.getActorManager().findActor(ids[i])->getSnapshot();
                bodyClear &= wildlifeBodyIsDryAndClear(world,after);
                supported &= wildlifeHasHighestDrySupport(world,after);
                const auto& history = after.wildlifeMotionHistory;
                const bool moved = after.position != before[i].position;
                actualHistory &= history.newestSequence ==
                    before[i].wildlifeMotionHistory.newestSequence+(moved ? 1u : 0u);
                if (!moved) {
                    deniedUnchanged &= history.newestSequence == before[i].wildlifeMotionHistory.newestSequence;
                    unchangedMovingTicks += tick >= 4 && after.wildlifeActivity ==
                        static_cast<int>(WildlifeActivity::Flee);
                    continue;
                }
                maximumGap = std::max(maximumGap,tick-latestMove[i]);
                latestMove[i] = tick; ++moveCount[i];
                const auto& segment = history.segments[history.count-1];
                actualHistory &= segment.from == before[i].position && segment.to == after.position &&
                    segment.sequence == history.newestSequence;
                const float horizontal = std::hypot(segment.to.x-segment.from.x,
                                                    segment.to.z-segment.from.z);
                maximumSeconds = std::max(maximumSeconds,segment.seconds);
                maximumDistance = std::max(maximumDistance,horizontal);
                durationBounded &= segment.seconds > 0.f && segment.seconds <= .20f && horizontal <= .60001f;
                sawRise |= segment.kind == WildlifeMotionPath::SupportRise;
            }
        }
        for (int i = 0; i < population; ++i)
            maximumGap = std::max(maximumGap,60-latestMove[i]);
        const std::string label = std::to_string(population);
        const auto detail = "max_unserved_ticks="+std::to_string(maximumGap)+
            " denied_ticks="+std::to_string(deniedTicks)+" max_seconds="+std::to_string(maximumSeconds)+
            " max_horizontal="+std::to_string(maximumDistance);
        check("WILDLIFE-DENSE/registered-mixed-species-"+label,registered);
        check("WILDLIFE-DENSE/real-mixed-slope-body-and-support-"+label,bodyClear && supported && sawRise,detail);
        check("WILDLIFE-DENSE/shared-48-query-budget-"+label,bounded && deniedTicks > 0,detail);
        check("WILDLIFE-DENSE/fair-bounded-progress-"+label,maximumGap <= population+3 &&
            std::all_of(moveCount.begin(),moveCount.end(),[](int count) { return count >= 3; }),detail);
        check("WILDLIFE-DENSE/accepted-duration-and-distance-bounded-"+label,durationBounded && actualHistory,detail);
        check("WILDLIFE-DENSE/no-invented-motion-during-denial-"+label,deniedUnchanged && unchangedMovingTicks > 0,detail);
    }
    clearDeterministicEnv();
}

void caseAdventureWildlife()
{
    caseWildlifeMotionHistory();
    caseWildlifeReviewRegressions();
    caseWildlifeProductionCadence();
    caseWildlifeMixedSupportBoundaries();
    caseWildlifeAutomaticObstacleAndBudget();
    for (const char* type : {WildlifeSpecies::Sheep, WildlifeSpecies::Rabbit,
                            WildlifeSpecies::MarshBird}) {
        const auto profile = WildlifePresentation::profileFor(type);
        ActorSnapshot sample;
        sample.type = type;
        const auto posedPoint = [&](const WildlifeVisualPose& pose,
                                    std::size_t index, glm::vec3 reference) {
            const auto relative = reference - profile.parts[index].offset;
            const float angle = glm::radians(pose.rotations[index].x);
            return profile.parts[index].offset + pose.offsets[index] + glm::vec3(
                relative.x, std::cos(angle) * relative.y - std::sin(angle) * relative.z,
                std::sin(angle) * relative.y + std::cos(angle) * relative.z);
        };
        std::size_t head = 0;
        for (std::size_t i = 0; i < profile.partCount; ++i)
            if (profile.parts[i].role == WildlifeVisualRole::Head) head = i;
        bool lowersFront = true, attached = true, earsRooted = true, neutral = true;
        for (float strength : {.35f, 1.f}) for (float seconds : {0.f, .46f, 1.3f}) {
            sample.wildlifeActivity = static_cast<int>(WildlifeActivity::Forage);
            sample.wildlifeMotionSeconds = seconds;
            const auto pose = WildlifePresentation::poseFor(sample, profile, 0.f, strength);
            const auto frontRole = profile.speciesIndex == 0 ? WildlifeVisualRole::Muzzle :
                profile.speciesIndex == 1 ? WildlifeVisualRole::Head : WildlifeVisualRole::Beak;
            for (std::size_t i = 0; i < profile.partCount; ++i) {
                const auto& part = profile.parts[i];
                if (part.role == frontRole) {
                    const auto front = part.offset + glm::vec3(0.f, 0.f, -.5f * part.scale.z);
                    lowersFront &= posedPoint(pose, i, front).y < front.y - .001f;
                }
                if (part.role != WildlifeVisualRole::Ear &&
                    part.role != WildlifeVisualRole::Muzzle &&
                    part.role != WildlifeVisualRole::Beak &&
                    part.role != WildlifeVisualRole::Neck) continue;
                // A point in the original overlapping volumes must remain
                // shared after articulation, even at the extreme feed pose.
                const auto& headPart = profile.parts[head];
                const auto lo = glm::max(part.offset - part.scale * .5f,
                                        headPart.offset - headPart.scale * .5f);
                const auto hi = glm::min(part.offset + part.scale * .5f,
                                        headPart.offset + headPart.scale * .5f);
                const auto seam = (lo + hi) * .5f;
                attached &= glm::all(glm::lessThanEqual(lo, hi)) &&
                    glm::distance(posedPoint(pose, i, seam),
                                  posedPoint(pose, head, seam)) < .00001f;
                if (part.role == WildlifeVisualRole::Ear) {
                    sample.wildlifeActivity = static_cast<int>(WildlifeActivity::Flee);
                    const auto flee = WildlifePresentation::poseFor(sample, profile, 1.f, strength);
                    const auto root = part.offset - glm::vec3(0.f, .5f * part.scale.y, 0.f);
                    earsRooted &= glm::distance(posedPoint(flee, i, root),
                                                posedPoint(flee, head, root)) < .00001f;
                    sample.wildlifeActivity = static_cast<int>(WildlifeActivity::Forage);
                }
            }
        }
        const auto off = WildlifePresentation::poseFor(sample, profile, 1.f, 0.f);
        sample.wildlifeActivity = static_cast<int>(WildlifeActivity::Rest);
        const auto rest = WildlifePresentation::poseFor(sample, profile, 1.f, 1.f);
        for (std::size_t i = 0; i < profile.partCount; ++i)
            neutral &= off.offsets[i] == glm::vec3(0.f) && off.rotations[i] == glm::vec3(0.f) &&
                       rest.offsets[i] == glm::vec3(0.f) && rest.rotations[i] == glm::vec3(0.f);
        check(std::string("WILDLIFE-POSE/forage-lowers-front/") + type, lowersFront);
        check(std::string("WILDLIFE-POSE/head-attachments-stay-connected/") + type, attached);
        if (profile.speciesIndex == 1)
            check("WILDLIFE-POSE/flee-ears-rooted-in-head", earsRooted);
        check(std::string("WILDLIFE-POSE/off-and-rest-are-neutral/") + type, neutral);
    }
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
                peckChanges = peckB.rotations[index].x <
                    peckA.rotations[index].x - 5.f;
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
