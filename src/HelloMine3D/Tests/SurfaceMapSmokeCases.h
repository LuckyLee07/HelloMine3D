#pragma once

// Included by WorldRuntimeSmokeMain after its shared fixture helpers.
namespace {
void caseSurfaceMapObservations()
{
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player,
                freshSaveDirectory("surface_map"), false, 0);
    auto& manager = world.getChunkManager();
    const auto initialCount = manager.getChunks().size();
    constexpr int fixtureChunkX = -90000;
    constexpr int fixtureChunkZ = -90000;
    const VectorXZ fixtureBlock{
        fixtureChunkX * CHUNK_SIZE + CHUNK_SIZE - 1,
        fixtureChunkZ * CHUNK_SIZE + CHUNK_SIZE - 1};
    const std::vector<VectorXZ> coordinates{
        fixtureBlock,
        {80000 * CHUNK_SIZE + 8, 80000 * CHUNK_SIZE + 8},
        {90000 * CHUNK_SIZE + 8, -90000 * CHUNK_SIZE + 8}};
    const auto unknown = manager.collectSurfaceMapSamples(coordinates);
    check("MAP/unknown-does-not-load-or-create", unknown.size() == 3 &&
          !unknown[0].known && !unknown[1].known && !unknown[2].known &&
          manager.getChunks().size() == initialCount);

    auto& negative = manager.getOrCreateChunk(fixtureChunkX, fixtureChunkZ);
    negative.transitionDataResidency(ChunkDataResidencyState::Requested);
    negative.transitionDataResidency(ChunkDataResidencyState::Loading);
    std::vector<Block_t> ids(CHUNK_VOLUME * 3, 0);
    std::vector<BlockMetadata_t> metadata(ids.size(), 0);
    negative.loadBlockData(3, ids, metadata);
    negative.setBlock(15, 20, 15, BlockId::Grass);
    negative.setBlock(15, 21, 15, BlockId::TallGrass);
    const auto ground = manager.collectSurfaceMapSamples(coordinates);
    check("MAP/negative-coordinate-skips-flora", ground[0].known &&
          ground[0].height == 20 && ground[0].material == BlockId::Grass);
    negative.setBlock(15, 29, 15, BlockId::OakLeaf);
    const auto canopy = manager.collectSurfaceMapSamples(coordinates);
    check("MAP/transparent-canopy-is-visible", canopy[0].height == 29 &&
          canopy[0].material == BlockId::OakLeaf);
    negative.setBlock(15, 35, 15, BlockId::OakPlank);
    const auto building = manager.collectSurfaceMapSamples(coordinates);
    check("MAP/player-building-overrides-canopy", building[0].height == 35 &&
          building[0].material == BlockId::OakPlank);
    negative.setBlock(15, 35, 15, BlockId::Air);
    negative.setBlock(15, 29, 15, BlockId::Air);
    negative.setBlock(15, 21, 15, BlockId::Water);
    const auto water = manager.collectSurfaceMapSamples(coordinates);
    check("MAP/stationary-edit-refreshes-water", water[0].height == 21 &&
          water[0].material == BlockId::Water);
    check("MAP/snapshot-survives-subsequent-edits", building[0].height == 35 &&
          building[0].material == BlockId::OakPlank &&
          ground[0].material == BlockId::Grass);
    negative.clearSaveDirty();
    manager.collectSurfaceMapSamples(coordinates);
    check("MAP/read-does-not-dirty-chunk", !negative.needsSave());
    std::vector<VectorXZ> fullBatch(256, fixtureBlock);
    const auto full = manager.collectSurfaceMapSamples(fullBatch);
    check("MAP/bounded-full-batch", full.size() == 256 &&
          full.front().material == BlockId::Water && full.back().material == BlockId::Water);
    bool rejected = false;
    try { manager.collectSurfaceMapSamples(std::vector<VectorXZ>(257)); }
    catch (const std::invalid_argument&) { rejected = true; }
    check("MAP/oversized-batch-rejected", rejected);
    manager.unloadChunk(fixtureChunkX, fixtureChunkZ);
    const auto evicted = manager.collectSurfaceMapSamples(coordinates);
    check("MAP/eviction-becomes-unknown-without-reload", !evicted[0].known &&
          manager.getChunks().size() == initialCount);
}
}
