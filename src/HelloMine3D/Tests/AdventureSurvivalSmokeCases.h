#pragma once

namespace {
void caseAdventureSurvival()
{
    using namespace NaturalPopulationRules;
    check("ADVENTURE-SURVIVAL/day-night-and-next-day-surface-population",
        !surfaceSpawnTime(-1) && !surfaceSpawnTime(0) && !surfaceSpawnTime(6000) &&
        !surfaceSpawnTime(11999) && surfaceSpawnTime(12000) && surfaceSpawnTime(23999) &&
        !surfaceSpawnTime(24000) && surfaceSpawnTime(36000));
    bool ring = true, stable = true;
    for (int seed : {42, 20260807, 239701883})
        for (int epoch : {1, 601, 1200})
            for (std::size_t attempt = 0; attempt < 256; ++attempt) {
                const auto offset = World::naturalMobSpawnOffset(seed, epoch, attempt);
                const float radius = glm::length(glm::vec2(offset));
                ring &= radius >= 23.29f && radius <= 30.71f;
                stable &= offset == World::naturalMobSpawnOffset(seed, epoch, attempt);
            }
    check("ADVENTURE-SURVIVAL/candidates-stay-in-bounded-ring-for-three-frozen-seeds", ring && stable);
    check("ADVENTURE-SURVIVAL/archetype-distance-and-invalid-input-guard",
        !outsideChaseRange({0, 0, 0}, {21.9f, 200, 0}, 18.f) &&
        outsideChaseRange({0, 0, 0}, {22.f, 0, 0}, 18.f) &&
        !outsideChaseRange({0, 0, 0}, {30, 0, 0}, 64.f) &&
        !outsideChaseRange({0, 0, 0}, {30, 0, 0}, std::numeric_limits<float>::quiet_NaN()));

    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8.5 201 8.5");
    Config config = makeConfig();
    Camera camera(config);
    constexpr int SpawnTick = NightStartTick + World::NaturalMobSpawnIntervalTicks;
    const auto naturals = [](World& world) {
        std::vector<ActorSnapshot> result;
        for (const auto& actor : world.collectActorSnapshots())
            if (!actor.deathPresentation && World::isNaturalMobType(actor.type)) result.push_back(actor);
        return result;
    };
    // Controlled pads prove candidate rejection independently of natural foliage.
    for (int mode = 0; mode < 5; ++mode) {
        Player player;
        World world(camera, config, player,
            freshSaveDirectory("adventure_population_" + std::to_string(mode)), false, mode == 4 ? 0 : 2);
        std::vector<glm::ivec3> pads;
        if (mode != 4) {
            for (std::size_t attempt = 0; attempt < World::NaturalMobSpawnAttemptsPerCycle; ++attempt) {
                const auto offset = World::naturalMobSpawnOffset(kValidationSeed,
                    SpawnTick / World::NaturalMobSpawnIntervalTicks, attempt);
                const glm::ivec3 position{8 + offset.x, 201, 8 + offset.y};
                pads.push_back(position);
                const auto support = mode == 2 ? BlockId::OakLeaf : mode == 3 ? BlockId::OakBark : BlockId::Stone;
                world.setBlock(position.x, 200, position.z, support);
                world.setBlock(position.x, 201, position.z, BlockId::Air);
                world.setBlock(position.x, 202, position.z, BlockId::Air);
            }
        }
        if (mode == 1) {
            bool lit = true;
            for (const auto& pad : pads) {
                bool placed = false;
                for (const auto offset : {glm::ivec3{1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1}}) {
                    const auto lamp = pad + offset;
                    if (std::find(pads.begin(), pads.end(), lamp) != pads.end()) continue;
                    world.setBlock(lamp.x, lamp.y, lamp.z, BlockId::Torch);
                    placed = true; break;
                }
                lit &= placed;
            }
            for (const auto& pad : pads)
                lit &= world.getBlockLight(pad.x, pad.y, pad.z) >= BrightBlockLight &&
                    world.getBlock(pad.x, pad.y, pad.z) == BlockId::Air &&
                    world.getBlock(pad.x, pad.y+1, pad.z) == BlockId::Air;
            check("ADVENTURE-SURVIVAL/torch-fixture-has-light-and-clear-headroom", lit);
        }
        world.tick(6000);
        check("ADVENTURE-SURVIVAL/day-does-not-spawn-surface-enemies-"+std::to_string(mode),
            naturals(world).empty() && world.collectDebugStats().naturalMobSpawnAttempts == 0);
        const auto beforeChunks = world.collectDebugStats().chunks;
        world.tick(SpawnTick);
        const auto actors = naturals(world);
        if (mode == 0) {
            bool distance = !actors.empty();
            for (const auto& actor : actors) {
                const auto* definition = runtimeEnemyRegistry().find(actor.type);
                distance &= definition && outsideChaseRange(player.position, actor.position, definition->chaseRadius);
            }
            check("ADVENTURE-SURVIVAL/night-spawns-outside-real-chase-range-within-cap",
                distance && actors.size() <= World::NaturalMobLocalCap &&
                world.collectDebugStats().naturalMobSpawnAttempts <= World::NaturalMobSpawnAttemptsPerCycle);
            world.tick(24000);
            auto dawn = naturals(world);
            check("ADVENTURE-SURVIVAL/dawn-keeps-existing-enemies", dawn.size() == actors.size());
        } else {
            check("ADVENTURE-SURVIVAL/rejects-" + std::string(mode == 1 ? "lit-ground" : mode == 2 ? "leaves" : mode == 3 ? "trunks" : "unloaded-ground"), actors.empty());
        }
        // A readonly sample in an unloaded column must remain unloaded.
        const auto afterChunks = world.collectDebugStats().chunks;
        check("ADVENTURE-SURVIVAL/population-does-not-load-distant-columns-"+std::to_string(mode),
            afterChunks.existingChunks == beforeChunks.existingChunks &&
            afterChunks.loadedChunks == beforeChunks.loadedChunks &&
            world.getChunkManager().findChunk(100,100) == nullptr);
    }

    {
        Player player;
        World world(camera, config, player, freshSaveDirectory("adventure_soil"), false, 0);
        const glm::ivec3 crop{10,201,10};
        player.addItem(Material::WHEAT_SEEDS, 10);
        for (const auto support : {BlockId::Grass, BlockId::Dirt, BlockId::ForestFloor, BlockId::Silt,
                                    BlockId::Stone, BlockId::Sand, BlockId::Water, BlockId::OakLeaf}) {
            world.setBlock(crop.x,crop.y,crop.z,BlockId::Air);
            world.setBlock(crop.x,crop.y-1,crop.z,support);
            const bool soil = support == BlockId::Grass || support == BlockId::Dirt ||
                              support == BlockId::ForestFloor || support == BlockId::Silt;
            const int count = player.getInventoryCount(Material::ID::WheatSeeds);
            const bool planted = BlockInteractionSystem::placeBlock(world, player, glm::vec3(crop)+glm::vec3(.5f));
            check("ADVENTURE-SURVIVAL/planting-soil-and-atomic-rejection-"+std::to_string(static_cast<int>(support)),
                planted == soil && player.getInventoryCount(Material::ID::WheatSeeds) == count - (soil ? 1 : 0));
        }
        world.setBlock(crop.x,crop.y-1,crop.z,BlockId::Silt);
        world.setBlock(crop.x,crop.y,crop.z,BlockId::Rose);
        const int before = player.getInventoryCount(Material::ID::WheatSeeds);
        check("ADVENTURE-SURVIVAL/new-soil-does-not-overwrite-occupied-blocks",
            !BlockInteractionSystem::placeBlock(world, player, glm::vec3(crop)+glm::vec3(.5f)) &&
            player.getInventoryCount(Material::ID::WheatSeeds) == before &&
            world.getBlock(crop.x,crop.y,crop.z) == BlockId::Rose);
    }

    clearDeterministicEnv();
    const auto evidence = freshSaveDirectory("adventure_survival_survey");
    std::ofstream csv(std::filesystem::path(evidence)/"resources.csv");
    csv << "seed,spawn_x,spawn_y,spawn_z,mature_grass,plantable_grass,nearest_m,wheat_after_domain_farming\n";
    for (int seed : {42, 20260807, 239701883}) {
        setEnv("HELLOMINE3D_SEED", std::to_string(seed));
        Player player;
        World world(camera, config, player, freshSaveDirectory("adventure_food_"+std::to_string(seed)), false, 2);
        const auto spawn = world.getPlayerSpawnPoint();
        const int sx=World::toBlockCoord(spawn.x), sz=World::toBlockCoord(spawn.z);
        int grass = 0;
        std::vector<glm::ivec3> plantable;
        for (int x=sx-32;x<=sx+32;++x) for (int z=sz-32;z<=sz+32;++z) {
            if ((x-sx)*(x-sx)+(z-sz)*(z-sz)>32*32) continue;
            const auto chunkPosition = World::getChunkXZ(x,z);
            auto* chunk = world.getChunkManager().findChunk(chunkPosition.x,chunkPosition.z);
            if (!chunk || !chunk->hasLoaded()) continue;
            const auto local = World::getBlockXZ(x,z);
            const int top = chunk->getHeightAt(local.x,local.z);
            for (int y=top+1;y>=std::max(1,top-24);--y) {
                const auto block=chunk->getBlock(local.x,y,local.z);
                if (block.id != static_cast<Block_t>(BlockId::TallGrass) || block.metadata < BlockMetadata::TallGrass::Mature) continue;
                ++grass;
                const auto support=static_cast<BlockId>(chunk->getBlock(local.x,y-1,local.z).id);
                if (support==BlockId::Grass || support==BlockId::Dirt || support==BlockId::ForestFloor || support==BlockId::Silt)
                    plantable.push_back({x,y,z});
            }
        }
        std::sort(plantable.begin(),plantable.end(),[&](const auto& a,const auto& b) {
            return glm::distance(glm::vec3(a),spawn)<glm::distance(glm::vec3(b),spawn);
        });
        check("ADVENTURE-SURVIVAL/ordinary-spawn-has-three-seed-and-soil-sources-"+std::to_string(seed),
            plantable.size()>=3, "grass="+std::to_string(grass)+" plantable="+std::to_string(plantable.size()));
        const auto plantableCount = plantable.size();
        // Domain calls prove real generated resources and crop rules. They are NOT a normal-input route.
        bool farmed=plantable.size()>=3;
        if (farmed) {
            plantable.resize(3);
            for (const auto& position:plantable)
                farmed &= BlockInteractionSystem::breakBlock(world,player,glm::vec3(position)+glm::vec3(.5f));
            int seedSlot=-1;
            for(int slot=0;slot<player.getInventorySlotCount();++slot)
                if(player.getInventorySlot(slot).getMaterial().id==Material::ID::WheatSeeds) seedSlot=slot;
            PlayerInputState input; input.hotbarSlot=seedSlot; player.applyInput(input);
            for (const auto& position:plantable)
                farmed &= BlockInteractionSystem::placeBlock(world,player,glm::vec3(position)+glm::vec3(.5f));
            bool mature=false;
            for(int tick=1;tick<=200 && !mature;++tick) {
                world.tick(tick); mature=true;
                for(const auto& position:plantable)
                    mature &= world.getBlock(position.x,position.y,position.z)==ChunkBlock(BlockId::WheatCrop,BlockMetadata::WheatCrop::Mature);
            }
            farmed &= mature;
            for(const auto& position:plantable)
                farmed &= BlockInteractionSystem::breakBlock(world,player,glm::vec3(position)+glm::vec3(.5f));
        }
        check("ADVENTURE-SURVIVAL/generated-seeds-produce-bread-input-without-combat-"+std::to_string(seed),
            farmed && player.getInventoryCount(Material::ID::Wheat)==3 &&
            player.getInventoryCount(Material::ID::WheatSeeds)>=3 && naturals(world).empty());
        csv << seed << ',' << spawn.x << ',' << spawn.y << ',' << spawn.z << ',' << grass << ','
            << plantableCount << ',' << (plantable.empty()?-1.f:glm::distance(glm::vec3(plantable.front()),spawn)) << ','
            << player.getInventoryCount(Material::ID::Wheat) << '\n';
    }
    clearDeterministicEnv(); setEnv("HELLOMINE3D_SEED", "");
}
}
