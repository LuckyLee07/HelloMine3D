#pragma once

// Included after the WorldRuntime shared fixture helpers.
namespace {
void caseWaterDepthPresentation()
{
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, freshSaveDirectory("water_depth"), false, 0);
    auto& manager = world.getChunkManager();
    constexpr int west = -80000, north = -80000;
    for (int cz = north; cz <= north + 1; ++cz) {
        for (int cx = west; cx <= west + 1; ++cx) {
            auto& chunk = manager.getOrCreateChunk(cx, cz);
            chunk.transitionDataResidency(ChunkDataResidencyState::Requested);
            chunk.transitionDataResidency(ChunkDataResidencyState::Loading);
            std::vector<Block_t> ids(CHUNK_VOLUME * 3, 0);
            std::vector<BlockMetadata_t> metadata(ids.size(), 0);
            chunk.loadBlockData(3, ids, metadata);
            for (int z = 0; z < CHUNK_SIZE; ++z)
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    chunk.setBlock(x, 13, z, BlockId::Sand);
                    for (int y = 14; y <= 18; ++y)
                        chunk.setBlock(x, y, z, BlockId::Water);
                }
        }
    }
    auto* westChunk = manager.findChunk(west, north);
    auto* eastChunk = manager.findChunk(west + 1, north);
    auto* section = westChunk->findSection(1);
    const auto count = manager.getChunks().size();
    SectionMeshInput input;
    section->captureMeshInput(input);
    check("WATER_DEPTH/reads-across-section-floor", input.getWaterDepth(8, 2, 8) == 5.f);
    check("WATER_DEPTH/air-has-zero-depth", input.getWaterDepth(8, 3, 8) == 0.f);
    for (int y = 1; y <= 18; ++y) westChunk->setBlock(4, y, 4, BlockId::Water);
    for (int y = 14; y < 18; ++y) westChunk->setBlock(5, y, 5, BlockId::Sand);
    westChunk->setBlock(7, 18, 7, BlockId::Sand);
    SectionMeshInput revised;
    section->captureMeshInput(revised);
    check("WATER_DEPTH/saturates-at-eight-metres", revised.getWaterDepth(4, 2, 4) == 8.f);
    check("WATER_DEPTH/one-metre-shallows", revised.getWaterDepth(5, 2, 5) == 1.f);
    check("WATER_DEPTH/snapshot-is-independent", input.getWaterDepth(5, 2, 5) == 5.f);
    check("WATER_DEPTH/unknown-neighbour-does-not-load",
        revised.getWaterDepth(-1, 2, 8) == 0.f && manager.getChunks().size() == count);

    ChunkMeshCollection westMeshes, eastMeshes;
    ChunkMeshBuilder(revised, westMeshes).buildMesh();
    SectionMeshInput eastInput;
    eastChunk->findSection(1)->captureMeshInput(eastInput);
    ChunkMeshBuilder(eastInput, eastMeshes).buildMesh();
    const auto attributesAt = [](const Mesh& mesh, float x, float y, float z, bool drift = false) {
        std::vector<std::pair<float, float>> values;
        const auto& coordinates = drift ? mesh.textureCoords : mesh.textureRepeatCoords;
        for (std::size_t i = 0; i < mesh.vertexPositions.size() / 3; ++i)
            if (mesh.vertexPositions[i * 3] == x && mesh.vertexPositions[i * 3 + 1] == y &&
                mesh.vertexPositions[i * 3 + 2] == z)
                values.emplace_back(coordinates[i * 2], coordinates[i * 2 + 1]);
        return values;
    };
    const float seamX = (west + 1) * CHUNK_SIZE;
    const float seamZ = north * CHUNK_SIZE + 8;
    const auto a = attributesAt(westMeshes.waterMesh.getClientMesh(), seamX, 19, seamZ);
    const auto b = attributesAt(eastMeshes.waterMesh.getClientMesh(), seamX, 19, seamZ);
    const auto allDepthFive = [](const auto& values) {
        return !values.empty() && std::all_of(values.begin(), values.end(), [](const auto& value) {
            return value.first == 5.f && value.second == 0.f;
        });
    };
    check("WATER_DEPTH/negative-chunk-seam-agrees", allDepthFive(a) && allDepthFive(b));
    SectionMeshInput lowerInput;
    westChunk->findSection(0)->captureMeshInput(lowerInput);
    ChunkMeshCollection lowerMeshes;
    ChunkMeshBuilder(lowerInput, lowerMeshes).buildMesh();
    const auto lower = attributesAt(lowerMeshes.waterMesh.getClientMesh(),
        west * CHUNK_SIZE, 16, north * CHUNK_SIZE + 8);
    const auto upper = attributesAt(westMeshes.waterMesh.getClientMesh(),
        west * CHUNK_SIZE, 16, north * CHUNK_SIZE + 8);
    check("WATER_DEPTH/vertical-section-seam-agrees", !lower.empty() && !upper.empty() &&
        std::all_of(lower.begin(), lower.end(), [&](const auto& value) { return value == upper.front(); }) &&
        std::all_of(upper.begin(), upper.end(), [&](const auto& value) { return value == lower.front(); }));
    const auto shore = attributesAt(westMeshes.waterMesh.getClientMesh(),
        west * CHUNK_SIZE + 7, 19, north * CHUNK_SIZE + 7);
    check("WATER_DEPTH/shore-weight-follows-solid-bank", !shore.empty() &&
        std::all_of(shore.begin(), shore.end(), [](const auto& value) { return value.second == .25f; }));

    const auto before = section->getBlockRevision();
    // An edit below this section must invalidate the deeper water snapshot,
    // including corners shared with neighbouring columns.
    world.setBlock(west * CHUNK_SIZE + 8, 14, north * CHUNK_SIZE + 8, BlockId::Stone);
    check("WATER_DEPTH/bed-edit-invalidates-section-above", section->getBlockRevision() > before);
    SectionMeshInput edited;
    section->captureMeshInput(edited);
    check("WATER_DEPTH/bed-edit-updates-depth", edited.getWaterDepth(8, 2, 8) == 4.f);
    const auto plateau = section->getBlockRevision();
    world.setBlock(west * CHUNK_SIZE + 8, 14, north * CHUNK_SIZE + 8, BlockId::Stone);
    check("WATER_DEPTH/no-op-edit-does-not-invalidate", section->getBlockRevision() == plateau);
    check("WATER_DEPTH/bounded-query-keeps-chunk-count", manager.getChunks().size() == count);

    const auto driftWest = attributesAt(westMeshes.waterMesh.getClientMesh(), seamX, 19, seamZ, true);
    const auto driftEast = attributesAt(eastMeshes.waterMesh.getClientMesh(), seamX, 19, seamZ, true);
    const auto matchesDrift = [](const auto& values, float x, float z) {
        return !values.empty() && std::all_of(values.begin(), values.end(), [&](const auto& value) {
            return std::abs(value.first - x) < .00001f && std::abs(value.second - z) < .00001f;
        });
    };
    check("WATER_DEPTH/open-water-drift-seam-agrees",
        matchesDrift(driftWest, .8f, .6f) && matchesDrift(driftEast, .8f, .6f));
    const auto lowerDrift = attributesAt(lowerMeshes.waterMesh.getClientMesh(),
        west * CHUNK_SIZE, 16, north * CHUNK_SIZE + 8, true);
    const auto upperDrift = attributesAt(westMeshes.waterMesh.getClientMesh(),
        west * CHUNK_SIZE, 16, north * CHUNK_SIZE + 8, true);
    check("WATER_DEPTH/vertical-drift-seam-agrees", !lowerDrift.empty() && !upperDrift.empty() &&
        std::all_of(lowerDrift.begin(), lowerDrift.end(), [&](const auto& value) { return value == upperDrift.front(); }) &&
        std::all_of(upperDrift.begin(), upperDrift.end(), [&](const auto& value) { return value == lowerDrift.front(); }));
    for (int x = -1; x <= 0; ++x)
        world.setBlock(static_cast<int>(seamX) + x, 18, static_cast<int>(seamZ) - 1, BlockId::Sand);
    SectionMeshInput bankWest, bankEast;
    section->captureMeshInput(bankWest);
    eastChunk->findSection(1)->captureMeshInput(bankEast);
    ChunkMeshCollection bankWestMeshes, bankEastMeshes;
    ChunkMeshBuilder(bankWest, bankWestMeshes).buildMesh();
    ChunkMeshBuilder(bankEast, bankEastMeshes).buildMesh();
    const auto alongBankWest = attributesAt(bankWestMeshes.waterMesh.getClientMesh(), seamX, 19, seamZ, true);
    const auto alongBankEast = attributesAt(bankEastMeshes.waterMesh.getClientMesh(), seamX, 19, seamZ, true);
    check("WATER_DEPTH/drift-bends-along-real-bank",
        matchesDrift(alongBankWest, .4f, 0.f) && matchesDrift(alongBankEast, .4f, 0.f));
    check("WATER_DEPTH/bank-edit-preserves-old-snapshot", matchesDrift(driftWest, .8f, .6f));
    const auto& bankMesh = bankWestMeshes.waterMesh.getClientMesh();
    bool boundedDrift = true;
    for (std::size_t i = 0; i < bankMesh.textureCoords.size(); i += 2) {
        const float x = bankMesh.textureCoords[i], z = bankMesh.textureCoords[i + 1];
        boundedDrift = boundedDrift && std::isfinite(x) && std::isfinite(z) && x*x + z*z <= 1.00001f;
    }
    check("WATER_DEPTH/drift-bounded-and-no-extra-stream", boundedDrift &&
        bankMesh.textureCoords.size() == bankMesh.textureRepeatCoords.size() &&
        manager.getChunks().size() == count);
}
}
