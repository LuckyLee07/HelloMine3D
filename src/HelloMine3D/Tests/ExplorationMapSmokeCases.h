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

void caseExplorationMapLiveObservation()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "42");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    const auto directory = freshSaveDirectory("map_live_observation");
    Config config=makeConfig(); Camera camera(config);
    const VectorXZ coordinate{96, -48};
    {
        Player player; World world(camera,config,player,directory,false,0);
        auto& manager=world.getChunkManager();
        manager.loadChunk(6,-3);
        world.setBlock(coordinate.x,240,coordinate.z,BlockId::Sand);
        const auto before=manager.getChunks().size();
        const auto samples=world.observeSurfaceMap({coordinate,{1000000,1000000}});
        const auto archived=world.exploredSurfaceAt(coordinate.x,coordinate.z);
        check("MAP5/detail-observation-archives-resident-terrain-without-tick",
            samples.size()==2 && samples[0].known && samples[0].height==240 &&
            archived && archived->height==240 && archived->material==BlockId::Sand &&
            !samples[1].known && !world.exploredSurfaceAt(1000000,1000000) &&
            manager.getChunks().size()==before);
        world.setBlock(coordinate.x+1,245,coordinate.z+1,BlockId::Stone);
        world.observeSurfaceMap({{coordinate.x+1,coordinate.z+1}});
        check("MAP5/dense-minimap-does-not-rewrite-canonical-archive-column",
            world.exploredSurfaceAt(coordinate.x,coordinate.z)->material==BlockId::Sand);
        world.setBlock(coordinate.x,240,coordinate.z,BlockId::Water);
        const auto updated=world.observeSurfaceMap({coordinate});
        check("MAP5/live-edit-updates-both-detail-and-archive",
            updated[0].material==BlockId::Water &&
            world.exploredSurfaceAt(coordinate.x,coordinate.z)->material==BlockId::Water && world.save());
        manager.unloadChunk(6,-3);
        const auto evicted=world.observeSurfaceMap({coordinate});
        check("MAP5/unloaded-live-column-keeps-archived-history",
            !evicted[0].known && world.exploredSurfaceAt(coordinate.x,coordinate.z)->material==BlockId::Water);
    }
    {
        Player player; World world(camera,config,player,directory,false,0);
        const auto saved=world.exploredSurfaceAt(coordinate.x,coordinate.z);
        check("MAP5/paused-map-observation-persists-without-revealing-unloaded-world",
            saved && saved->height==240 && saved->material==BlockId::Water &&
            !world.getChunkManager().chunkLoadedAt(6,-3));
    }
    clearDeterministicEnv();
}

void caseWorldPreviewWorldWiring()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "42");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    const auto directory = freshSaveDirectory("world_preview_wiring");
    const std::filesystem::path previewPath =
        std::filesystem::path(directory) / "world-preview.hmp";
    Config config = makeConfig();
    Camera camera(config);
    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        const bool emptyAtlasSaved = world.save();
        check("MAP6/preview-absent-before-first-observation-save",
              emptyAtlasSaved && !std::filesystem::exists(previewPath));

        world.tick(1);
        const auto initialSurface = world.exploredSurfaceAt(8, 8);
        const bool initialSaved = world.save();
        WorldSaveData initialMetadata;
        const bool initialMetadataLoaded =
            WorldSave(directory).load(initialMetadata);
        const WorldPreviewStore::Identity initialIdentity{
            initialMetadata.worldId, initialMetadata.seed,
            initialMetadata.terrainGenerationVersion,
            initialMetadata.lastPlayedUtc};
        WorldPreviewStore::Preview initialPreview;
        std::string previewError;
        const auto initialPreviewStatus =
            WorldPreviewStore(directory).load(
                initialIdentity, initialPreview, &previewError);
        const auto& initialCenter = initialPreviewStatus ==
                    WorldPreviewStore::LoadStatus::Loaded
                ? initialPreview.at(WorldPreviewStore::Width / 2,
                                    WorldPreviewStore::Height / 2)
                : WorldPreviewStore::Cell{};
        check("MAP6/tick-save-publishes-current-world-preview",
              initialSaved && initialMetadataLoaded && initialSurface &&
                  initialPreviewStatus ==
                      WorldPreviewStore::LoadStatus::Loaded &&
                  previewError.empty() &&
                  initialPreview.centerX == 8 &&
                  initialPreview.centerZ == 8 &&
                  initialPreview.worldXAt(
                      WorldPreviewStore::Width / 2) == 8 &&
                  initialPreview.worldZAt(
                      WorldPreviewStore::Height / 2) == 8 &&
                  initialCenter.known &&
                  initialCenter.height == initialSurface->height &&
                  initialCenter.material == initialSurface->material &&
                  std::filesystem::is_regular_file(previewPath) &&
                  std::filesystem::file_size(previewPath) <=
                      WorldPreviewStore::MaxFileBytes);

        std::vector<WorldBackupInfo> initialBackups;
        const bool initialBackupsListed =
            WorldBackup(directory).listBackups(initialBackups);
        const bool previewsExcluded = initialBackupsListed &&
            !initialBackups.empty() &&
            std::all_of(initialBackups.begin(), initialBackups.end(),
                [](const WorldBackupInfo& backup) {
                    return !std::filesystem::exists(
                        std::filesystem::path(backup.directoryPath) /
                        "world-preview.hmp");
                });
        check("MAP6/world-backups-exclude-derived-preview",
              previewsExcluded);

        player.position = {20.75f, 90.f, 20.25f};
        player.rotation.y = 1.25f;
        const auto movedSurface = world.exploredSurfaceAt(20, 20);
        const bool movedSaved = world.save();
        WorldSaveData movedMetadata;
        const bool movedMetadataLoaded =
            WorldSave(directory).load(movedMetadata);
        const WorldPreviewStore::Identity movedIdentity{
            movedMetadata.worldId, movedMetadata.seed,
            movedMetadata.terrainGenerationVersion,
            movedMetadata.lastPlayedUtc};
        WorldPreviewStore::Preview movedPreview;
        const auto movedPreviewStatus = WorldPreviewStore(directory).load(
            movedIdentity, movedPreview, &previewError);
        const auto& movedCenter = movedPreviewStatus ==
                    WorldPreviewStore::LoadStatus::Loaded
                ? movedPreview.at(WorldPreviewStore::Width / 2,
                                  WorldPreviewStore::Height / 2)
                : WorldPreviewStore::Cell{};
        check("MAP6/explicit-save-refreshes-moved-preview-center",
              movedSaved && movedMetadataLoaded && movedSurface &&
                  movedPreviewStatus ==
                      WorldPreviewStore::LoadStatus::Loaded &&
                  movedPreview.centerX == 20 &&
                  movedPreview.centerZ == 20 &&
                  std::fabs(movedPreview.playerYaw - 1.25f) < 0.0001f &&
                  movedCenter.known &&
                  movedCenter.height == movedSurface->height &&
                  movedCenter.material == movedSurface->material);

        const VectorXZ mapProbe{12, 12};
        world.setBlock(mapProbe.x, 250, mapProbe.z, BlockId::Sand);
        const auto observed = world.observeSurfaceMap({mapProbe});
        std::filesystem::remove(previewPath);
        const bool blockingDirectoryCreated =
            std::filesystem::create_directory(previewPath);
        const bool blockedPreviewSave = world.save();

        WorldSaveData blockedMetadata;
        const bool blockedMetadataLoaded =
            WorldSave(directory).load(blockedMetadata);
        const ExplorationMapStore::Identity blockedMapIdentity{
            blockedMetadata.worldId, blockedMetadata.seed,
            blockedMetadata.terrainGenerationVersion};
        ExplorationAtlas liveAtlas;
        const auto liveMapStatus = ExplorationMapStore(directory).load(
            blockedMapIdentity, liveAtlas);
        const auto liveProbe = liveAtlas.surfaceAt(mapProbe.x, mapProbe.z);
        std::vector<WorldBackupInfo> blockedBackups;
        const bool blockedBackupsListed =
            WorldBackup(directory).listBackups(blockedBackups);
        WorldSaveData backupMetadata;
        ExplorationAtlas backupAtlas;
        bool backupReliable = false;
        if (blockedBackupsListed && !blockedBackups.empty()) {
            const std::string& backupDirectory =
                blockedBackups.back().directoryPath;
            const bool backupMetadataLoaded =
                WorldSave(backupDirectory).load(backupMetadata);
            const ExplorationMapStore::Identity backupIdentity{
                backupMetadata.worldId, backupMetadata.seed,
                backupMetadata.terrainGenerationVersion};
            const auto backupMapStatus =
                ExplorationMapStore(backupDirectory).load(
                    backupIdentity, backupAtlas);
            const auto backupProbe =
                backupAtlas.surfaceAt(mapProbe.x, mapProbe.z);
            backupReliable = backupMetadataLoaded &&
                backupMapStatus == ExplorationMapStore::LoadStatus::Loaded &&
                backupProbe && backupProbe->height == 250 &&
                backupProbe->material == BlockId::Sand &&
                !std::filesystem::exists(
                    std::filesystem::path(backupDirectory) /
                    "world-preview.hmp");
        }
        check("MAP6/blocked-preview-keeps-authoritative-save-and-map",
              blockingDirectoryCreated && blockedPreviewSave &&
                  observed.size() == 1 && observed.front().known &&
                  observed.front().height == 250 &&
                  observed.front().material == BlockId::Sand &&
                  blockedMetadataLoaded &&
                  liveMapStatus == ExplorationMapStore::LoadStatus::Loaded &&
                  liveProbe && liveProbe->height == 250 &&
                  liveProbe->material == BlockId::Sand &&
                  std::filesystem::is_directory(previewPath));
        check("MAP6/blocked-preview-save-still-publishes-reliable-backup",
              backupReliable);

        std::filesystem::remove_all(previewPath);
        const bool rebuilt = world.save();
        WorldSaveData rebuiltMetadata;
        const bool rebuiltMetadataLoaded =
            WorldSave(directory).load(rebuiltMetadata);
        const WorldPreviewStore::Identity rebuiltIdentity{
            rebuiltMetadata.worldId, rebuiltMetadata.seed,
            rebuiltMetadata.terrainGenerationVersion,
            rebuiltMetadata.lastPlayedUtc};
        WorldPreviewStore::Preview rebuiltPreview;
        const auto rebuiltStatus = WorldPreviewStore(directory).load(
            rebuiltIdentity, rebuiltPreview, &previewError);
        check("MAP6/writable-preview-target-rebuilds-current-cache",
              rebuilt && rebuiltMetadataLoaded &&
                  rebuiltStatus == WorldPreviewStore::LoadStatus::Loaded &&
                  rebuiltPreview.centerX == 20 &&
                  rebuiltPreview.centerZ == 20 &&
                  std::filesystem::is_regular_file(previewPath) &&
                  std::filesystem::file_size(previewPath) <=
                      WorldPreviewStore::MaxFileBytes);
    }
    clearDeterministicEnv();
}

void caseExplorationMapLifecycle()
{
    caseWorldPreviewWorldWiring();
    caseExplorationMapLiveObservation();
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
                  world.exploredCellCount() == 0 &&
                  !world.explorationMapStatus().needsAttention());
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
                  world.explorationMapStatus().resetReason ==
                      ExplorationMapStatus::ResetReason::Corrupt &&
                  !world.explorationMapStatus().quarantineFailed &&
                  !std::filesystem::exists(mapPath) &&
                  std::filesystem::exists(
                      mapPath.string() + ".corrupt.failed"));
        world.tick(1);
        check("MAP5/world-save-after-map-recovery",
              world.save() && std::filesystem::exists(mapPath));
    }
    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        check("MAP5/healthy-reopen-clears-session-warning",
              !world.explorationMapStatus().needsAttention());
        world.tick(1);
        const bool changed = world.createExplorationMarker(observedX, observedZ,
            "保存检查") == ExplorationMarkers::Result::Created;
        const auto preserved = mapPath.string() + ".save-test-original";
        std::filesystem::rename(mapPath, preserved);
        std::filesystem::create_directory(mapPath);
        check("MAP5/save-error-reaches-presentation-state",
              changed && !world.save() && world.explorationMapStatus().saveFailed &&
                  std::filesystem::is_regular_file(preserved));
        std::filesystem::remove(mapPath);
        std::filesystem::rename(preserved, mapPath);
        check("MAP5/successful-retry-clears-save-warning",
              world.save() && !world.explorationMapStatus().needsAttention());
    }
    const auto validBytes = readTextFile(mapPath.string());
    { std::ofstream invalid(mapPath, std::ios::binary); invalid << "damaged-map"; }
    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        check("MAP5/occupied-isolation-slot-reports-save-block",
              world.explorationMapStatus().quarantineFailed && !world.save() &&
                  readTextFile(mapPath.string()) == "damaged-map" &&
                  std::filesystem::exists(mapPath.string() + ".corrupt.failed"));
    }
    // Only repair this isolated fixture after verifying both files survived.
    { std::ofstream restored(mapPath, std::ios::binary);
      restored.write(validBytes.data(), validBytes.size()); }
    WorldSaveData savedData;
    const bool metadataLoaded = WorldSave(directory).load(savedData);
    ExplorationMapStore store(directory);
    ExplorationAtlas fullAtlas;
    for (std::size_t tile = 0; tile < ExplorationAtlas::MaxTiles; ++tile)
        fullAtlas.observe({static_cast<int>(tile * 128), 0, 70,
                           BlockId::Stone, true});
    const ExplorationMapStore::Identity identity{savedData.worldId,
        savedData.seed, savedData.terrainGenerationVersion};
    const bool fullSaved = metadataLoaded && store.save(identity, fullAtlas,
        ExplorationMarkers{}, std::nullopt, std::nullopt);
    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        check("MAP5/full-record-reaches-ui-with-history-intact",
              fullSaved && world.explorationMapStatus().full &&
                  world.explorationMapStatus().needsAttention() &&
                  world.exploredCellCount() == ExplorationAtlas::MaxTiles &&
                  world.exploredSurfaceAt(0,0).has_value());
    }
    // Keep the ordinary small fixture; this capacity test need not leave a
    // maximum-size map or its destructor-created backup on disk.
    std::filesystem::remove_all(directory);
    const auto foreignDirectory = freshSaveDirectory("map_foreign_warning");
    const bool foreignFixture = initializeTerrainIdentity(foreignDirectory,
        "world-map-actual", CurrentTerrainGenerationVersion, 42) &&
        ExplorationMapStore(foreignDirectory).save(
            {"world-map-other",42,CurrentTerrainGenerationVersion},
            ExplorationAtlas{}, ExplorationMarkers{}, std::nullopt, std::nullopt);
    {
        Player player;
        World world(camera, config, player, foreignDirectory, false, 0);
        check("MAP5/foreign-map-has-distinct-recovery-message",
            foreignFixture && world.explorationMapStatus().resetReason ==
                ExplorationMapStatus::ResetReason::ForeignIdentity &&
                !world.explorationMapStatus().quarantineFailed);
    }
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "");
}
}
