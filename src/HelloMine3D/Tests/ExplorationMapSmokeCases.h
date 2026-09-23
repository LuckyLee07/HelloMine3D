#pragma once

// Included by WorldRuntimeSmokeMain after its shared fixture helpers.
namespace {
void caseExplorationMapLifecycle()
{
    const std::string directory =
        freshSaveDirectory("adventure_exploration_map");
    setEnv("HELLOMINE3D_SEED", "42");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    Config config = makeConfig();
    Camera camera(config);
    int observedX = 0;
    int observedZ = 0;
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
