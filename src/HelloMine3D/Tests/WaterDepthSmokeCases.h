#pragma once

// Included after the WorldRuntime shared fixture helpers.
namespace {
// Read the actual triangle interpolation so a test also observes greedy merges.
float shoreMeshShade(const ChunkMesh& source, const glm::vec3& point,
                     int normalAxis, float normalSign)
{
    const auto& mesh = source.getClientMesh();
    const auto& lights = source.getLight();
    const int u = (normalAxis + 1) % 3, v = (normalAxis + 2) % 3;
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
        glm::vec3 p[3];
        for (int j = 0; j < 3; ++j) {
            const auto index = mesh.indices[i + j] * 3;
            p[j] = {mesh.vertexPositions[index], mesh.vertexPositions[index + 1],
                    mesh.vertexPositions[index + 2]};
        }
        if (p[0][normalAxis] != point[normalAxis] ||
            p[1][normalAxis] != point[normalAxis] ||
            p[2][normalAxis] != point[normalAxis] ||
            glm::cross(p[1] - p[0], p[2] - p[0])[normalAxis] * normalSign <= 0.f)
            continue;
        const glm::vec3 a = p[1] - p[0], b = p[2] - p[0], q = point - p[0];
        const float determinant = a[u] * b[v] - a[v] * b[u];
        const float one = (q[u] * b[v] - q[v] * b[u]) / determinant;
        const float two = (a[u] * q[v] - a[v] * q[u]) / determinant;
        if (one >= -.0001f && two >= -.0001f && one + two <= 1.0001f)
            return (1.f - one - two) * lights[mesh.indices[i]] +
                   one * lights[mesh.indices[i + 1]] + two * lights[mesh.indices[i + 2]];
    }
    return -1.f;
}

void caseShoreSurfacePresentation()
{
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, freshSaveDirectory("shore_surface"), false, 0);
    auto& manager = world.getChunkManager();
    constexpr int west = -7000, north = -7000;
    for (int cx = west; cx <= west + 1; ++cx) {
        auto& chunk = manager.getOrCreateChunk(cx, north);
        chunk.transitionDataResidency(ChunkDataResidencyState::Requested);
        chunk.transitionDataResidency(ChunkDataResidencyState::Loading);
        std::vector<Block_t> ids(CHUNK_VOLUME * 3, 0);
        std::vector<BlockMetadata_t> metadata(ids.size(), 0);
        chunk.loadBlockData(3, ids, metadata);
        for (int z = 0; z < CHUNK_SIZE; ++z)
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                for (int y = 0; y < CHUNK_SIZE * 2; ++y)
                    chunk.setBlockLight(x, y, z, MAX_LIGHT_LEVEL);
                for (int bankZ = 8; bankZ <= 11; ++bankZ)
                    chunk.setBlock(x, 15, bankZ, BlockId::Sand);
            }
    }
    auto& left = *manager.findChunk(west, north);
    auto& right = *manager.findChunk(west + 1, north);
    const auto chunkCount = manager.getChunks().size();
    const auto build = [](const SectionMeshInput& input, bool ao = true) {
        ChunkMeshCollection meshes;
        ChunkMeshBuilder(input, meshes, ao).buildMesh();
        return meshes;
    };
    const auto equal = [](const ChunkMesh& a, const ChunkMesh& b) {
        const auto& x = a.getClientMesh(); const auto& y = b.getClientMesh();
        return x.vertexPositions == y.vertexPositions && x.indices == y.indices &&
            x.textureCoords == y.textureCoords && x.textureRepeatCoords == y.textureRepeatCoords &&
            a.getLight() == b.getLight();
    };
    SectionMeshInput dry;
    left.findSection(0)->captureMeshInput(dry);
    const auto dryMesh = build(dry);
    check("WATER_DEPTH/shore-dry-snapshot-skips-sampling", !dry.containsWater());
    left.setBlock(1, 1, 1, BlockId::Water);
    SectionMeshInput distant;
    left.findSection(0)->captureMeshInput(distant);
    check("WATER_DEPTH/shore-distant-water-preserves-solid-bytes",
        distant.containsWater() && equal(dryMesh.solidMesh, build(distant).solidMesh));
    left.setBlock(1, 1, 1, BlockId::Air);
    for (auto* chunk : {&left, &right})
        for (int x = 0; x < CHUNK_SIZE; ++x) chunk->setBlock(x, 15, 7, BlockId::Water);
    SectionMeshInput wet, wetRight;
    left.findSection(0)->captureMeshInput(wet);
    right.findSection(0)->captureMeshInput(wetRight);
    const auto wetMesh = build(wet), rightMesh = build(wetRight);
    const auto point = [](float x, float y, float z) {
        return glm::vec3(west * CHUNK_SIZE + x, y, north * CHUNK_SIZE + z);
    };
    const float near = shoreMeshShade(wetMesh.solidMesh, point(15.5f, 16, 8.25f), 1, 1);
    const float far = shoreMeshShade(wetMesh.solidMesh, point(15.5f, 16, 8.75f), 1, 1);
    const float original = shoreMeshShade(dryMesh.solidMesh, point(15.5f, 16, 8.25f), 1, 1);
    check("WATER_DEPTH/shore-real-water-darkens-land-gradually",
        original > 0.f && near >= .82f * original && near < .95f * original && far > near && far < original);
    check("WATER_DEPTH/shore-band-ends-one-block-inland",
        shoreMeshShade(wetMesh.solidMesh, point(15.5f, 16, 9.5f), 1, 1) ==
        shoreMeshShade(dryMesh.solidMesh, point(15.5f, 16, 9.5f), 1, 1));
    const float seam = shoreMeshShade(wetMesh.solidMesh, point(16, 16, 8.25f), 1, 1);
    check("WATER_DEPTH/shore-negative-chunk-seam-agrees", seam > 0.f &&
        std::abs(seam - shoreMeshShade(rightMesh.solidMesh, point(16, 16, 8.25f), 1, 1)) < .00001f);
    const auto noAo = build(wet, false);
    check("WATER_DEPTH/shore-no-ao-fallback-keeps-band",
        shoreMeshShade(noAo.solidMesh, point(15.5f, 16, 8.25f), 1, 1) < original);
    check("WATER_DEPTH/shore-does-not-write-stored-light-or-load",
        wet.getCombinedLight(15, 15, 8) == dry.getCombinedLight(15, 15, 8) &&
        left.getBlockLight(15, 15, 8) == MAX_LIGHT_LEVEL && manager.getChunks().size() == chunkCount);
    for (auto* chunk : {&left, &right})
        for (int x = 0; x < CHUNK_SIZE; ++x) chunk->setBlock(x, 15, 7, BlockId::Air);
    left.findSection(0)->captureMeshInput(distant);
    check("WATER_DEPTH/shore-removing-water-restores-original-mesh",
        !distant.containsWater() && equal(dryMesh.solidMesh, build(distant).solidMesh));
    check("WATER_DEPTH/shore-old-snapshot-remains-independent",
        wet.containsWater() && equal(wetMesh.solidMesh, build(wet).solidMesh));
    for (int x = 2; x <= 3; ++x) {
        left.setBlock(x, 15, 3, BlockId::Sand);
        left.setBlock(x, 16, 3, BlockId::Sand);
        left.setBlock(x, 15, 2, BlockId::Water);
    }
    SectionMeshInput lower, upper;
    left.findSection(0)->captureMeshInput(lower);
    left.findSection(1)->captureMeshInput(upper);
    const auto lowerMesh = build(lower, false), upperMesh = build(upper, false);
    const float lowerShade = shoreMeshShade(lowerMesh.solidMesh, point(2.5f, 16, 3), 2, -1);
    check("WATER_DEPTH/shore-vertical-section-seam-agrees", lowerShade > 0.f &&
        std::abs(lowerShade - shoreMeshShade(upperMesh.solidMesh, point(2.5f, 16, 3), 2, -1)) < .00001f);
    const auto& actual = wetMesh.solidMesh.getClientMesh();
    check("WATER_DEPTH/shore-mesh-attributes-and-indices-remain-valid",
        actual.textureCoords.size() == actual.textureRepeatCoords.size() &&
        actual.vertexPositions.size() / 3 == wetMesh.solidMesh.getLight().size() &&
        std::all_of(actual.indices.begin(), actual.indices.end(), [&](auto i) { return i < actual.vertexPositions.size() / 3; }));
}

void caseWaterDepthPresentation()
{
    caseShoreSurfacePresentation();
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
