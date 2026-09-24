#pragma once

// Included by WorldRuntimeSmokeMain after its shared fixture helpers.
namespace {
void caseWaystoneMapBinding()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "42");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8.5 201 8.5");
    const auto directory = freshSaveDirectory("map_task_binding");
    Config config = makeConfig(); Camera camera(config);
    const glm::ivec3 active{9,201,8}, other{11,201,8};
    {
        Player player;
        World world(camera,config,player,directory,false,0);
        for (int x = 1; x < 16; ++x) for (int z = 1; z < 16; ++z) {
            world.setBlock(x,200,z,BlockId::Stone);
            world.setBlock(x,201,z,BlockId::Air);
            world.setBlock(x,202,z,BlockId::Air);
        }
        world.setBlock(active.x,active.y,active.z,BlockId::WaystoneCore);
        const bool initialized = world.initializeWaystone(active);
        player.addItem(Material::IRON_INGOT,WaystoneEncounter::ActivationIronIngots);
        const auto activation = world.useWaystone(active,player,true);
        const auto encounter = world.useWaystone(active,player,true);
        player.addItem(Material::WAYSTONE_CORE,1);
        for (int slot = 0; slot < 5; ++slot) {
            PlayerInputState input; input.hotbarSlot = slot; player.applyInput(input);
            if (player.getHeldItems().getMaterial().id == Material::ID::WaystoneCore)
                break;
        }
        const bool placed = BlockInteractionSystem::placeBlock(world,player,glm::vec3(other));
        const auto site = world.knownWaystoneTaskSite();
        check("MAP5/extra-placement-keeps-active-encounter-binding",
            initialized && activation == WaystoneActionResult::Activated &&
            encounter == WaystoneActionResult::EncounterStarted && placed &&
            site && site->worldX == active.x && site->worldY == active.y &&
            site->worldZ == active.z && world.save());
    }
    setEnv("HELLOMINE3D_PLAYER_POSITION", "512 201 512");
    {
        Player player;
        World world(camera,config,player,directory,false,0);
        const auto site = world.knownWaystoneTaskSite();
        check("MAP5/active-task-binding-survives-distant-reopen",
            site && site->worldX == active.x && site->worldY == active.y &&
            !world.getChunkManager().chunkLoadedAt(0,0));
    }
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8.5 201 8.5");
    {
        Player player;
        World world(camera,config,player,directory,false,0);
        const bool removedOther = BlockInteractionSystem::breakBlock(
            world,player,glm::vec3(other));
        const auto site = world.knownWaystoneTaskSite();
        check("MAP5/removing-extra-waystone-keeps-binding",
            removedOther && site && site->worldX == active.x);
        check("MAP5/removing-bound-waystone-clears-binding",
            BlockInteractionSystem::breakBlock(world,player,glm::vec3(active)) &&
            !world.knownWaystoneTaskSite() && world.save());
    }
    {
        Player player;
        World world(camera,config,player,directory,false,0);
        check("MAP5/removed-binding-stays-cleared", !world.knownWaystoneTaskSite());
    }
    clearDeterministicEnv();
}

void caseExplorationMapLifecycle()
{
    caseWaystoneMapBinding();
    const std::string directory =
        freshSaveDirectory("adventure_exploration_map");
    setEnv("HELLOMINE3D_SEED", "42");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    Config config = makeConfig();
    Camera camera(config);
    int observedX = 0;
    int observedZ = 0;
    const glm::ivec3 taskSite{8, 200, 8};
    std::uint32_t homeId = 0;
    std::optional<ExplorationAtlas::Surface> initialSurface;
    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        observedX = World::floorDiv(World::toBlockCoord(player.position.x), 4) * 4;
        observedZ = World::floorDiv(World::toBlockCoord(player.position.z), 4) * 4;
        check("MAP5/old-world-starts-without-sidecar",
              !std::filesystem::exists(
                  std::filesystem::path(directory) / "exploration.hmap") &&
                  world.exploredCellCount() == 0);
        world.tick(1);
        initialSurface = world.exploredSurfaceAt(observedX, observedZ);
        check("MAP5/resident-player-route-observed",
              initialSurface.has_value() &&
                  world.exploredCellCount() > 0 &&
                  world.exploredCellCount() <= 81 &&
                  !world.explorationMapFull());
        const auto overview = world.exploredOverviewAt(
            observedX, observedZ, 3, 8);
        check("MAP5/overview-shows-only-recorded-world-surface",
              overview.size() == 9 && overview[4].known &&
                  overview[4].worldX == observedX &&
                  overview[4].worldZ == observedZ &&
                  initialSurface.has_value() &&
                  overview[4].surface.material == initialSurface->material);
        const std::size_t chunkCount =
            world.getChunkManager().getChunks().size();
        check("MAP5/unknown-marker-does-not-generate-terrain",
              world.createExplorationMarker(1000000, 1000000,
                  "未知地点") == ExplorationMarkers::Result::Invalid &&
                  world.explorationMarkers().empty() &&
                  world.getChunkManager().getChunks().size() == chunkCount);
        check("MAP5/player-home-and-tracking-use-observed-surface",
              world.createExplorationMarker(observedX, observedZ,
                  "出发点", ExplorationMarkers::Kind::Home, &homeId) ==
                      ExplorationMarkers::Result::Created &&
                  world.renameExplorationMarker(homeId, "返航点") ==
                      ExplorationMarkers::Result::Changed &&
                  world.trackExplorationMarker(homeId) ==
                      ExplorationMarkers::Result::Changed &&
                  world.trackedExplorationMarker().has_value());
        const bool supplied = player.addItem(Material::WAYSTONE_CORE, 1) == 1;
        const bool placed = supplied && BlockInteractionSystem::placeBlock(
            world, player, glm::vec3(taskSite));
        const auto knownSite = world.knownWaystoneTaskSite();
        check("MAP5/real-waystone-placement-discovers-task-site",
              placed && knownSite &&
                  knownSite->worldX == taskSite.x &&
                  knownSite->worldY == taskSite.y &&
                  knownSite->worldZ == taskSite.z &&
                  static_cast<BlockId>(world.getBlock(
                      taskSite.x, taskSite.y, taskSite.z).id) ==
                      BlockId::WaystoneCore);
        const bool saved = world.save();
        std::vector<WorldBackupInfo> backups;
        const bool backupListed = WorldBackup(directory).listBackups(backups);
        check("MAP5/world-save-publishes-map-and-backup",
              saved && backupListed && !backups.empty() &&
                  std::filesystem::exists(
                      std::filesystem::path(directory) / "exploration.hmap") &&
                  std::filesystem::exists(
                      std::filesystem::path(backups.back().directoryPath) /
                      "exploration.hmap"));
    }
    setEnv("HELLOMINE3D_PLAYER_POSITION", "512 90 512");
    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        const auto remoteSite = world.knownWaystoneTaskSite();
        const VectorXZ siteChunk = World::getChunkXZ(taskSite.x, taskSite.z);
        check("MAP5/distant-reopen-keeps-discovered-task-site",
              remoteSite && remoteSite->worldX == taskSite.x &&
                  remoteSite->worldY == taskSite.y &&
                  remoteSite->worldZ == taskSite.z &&
                  !world.getChunkManager().chunkLoadedAt(
                      siteChunk.x, siteChunk.z));
    }
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        const auto restored = world.exploredSurfaceAt(observedX, observedZ);
        check("MAP5/reopen-recovers-real-observation",
              restored.has_value() && initialSurface.has_value() &&
                  restored->height == initialSurface->height &&
                  restored->material == initialSurface->material &&
                  world.exploredCellCount() > 0);
        const auto markers = world.explorationMarkers();
        const auto tracked = world.trackedExplorationMarker();
        check("MAP5/reopen-recovers-home-and-tracking",
              markers.size() == 1 && markers.front().id == homeId &&
                  markers.front().name == "返航点" &&
                  markers.front().kind == ExplorationMarkers::Kind::Home &&
                  tracked.has_value() && tracked->id == homeId &&
                  world.moveExplorationMarker(homeId, 1000000, 1000000) ==
                      ExplorationMarkers::Result::Invalid);
        const auto restoredSite = world.knownWaystoneTaskSite();
        check("MAP5/reopen-recovers-known-task-site",
              restoredSite && restoredSite->worldX == taskSite.x &&
                  restoredSite->worldY == taskSite.y &&
                  restoredSite->worldZ == taskSite.z);
        const bool removed = BlockInteractionSystem::breakBlock(
            world, player, glm::vec3(taskSite));
        check("MAP5/breaking-known-waystone-clears-task-site",
              removed && !world.knownWaystoneTaskSite() && world.save());
    }
    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        check("MAP5/removed-task-site-stays-unknown-after-reopen",
              !world.knownWaystoneTaskSite() &&
                  world.exploredSurfaceAt(observedX, observedZ).has_value() &&
                  world.trackedExplorationMarker().has_value());
    }

    const auto mapPath =
        std::filesystem::path(directory) / "exploration.hmap";
    std::fstream corrupt(mapPath, std::ios::in | std::ios::out | std::ios::binary);
    char first = 0;
    corrupt.read(&first, 1);
    corrupt.seekp(0);
    first ^= 1;
    corrupt.write(&first, 1);
    corrupt.close();
    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        check("MAP5/corrupt-map-does-not-block-world-entry",
              world.exploredCellCount() == 0 &&
                  !std::filesystem::exists(mapPath) &&
                  std::filesystem::exists(
                      mapPath.string() + ".corrupt.failed"));
        check("MAP5/world-save-after-map-recovery",
              world.save());
    }
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "");
}
}
