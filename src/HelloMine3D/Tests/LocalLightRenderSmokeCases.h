// Included by the world smoke runner, using its real world and check helpers.
void caseLocalLightRendering()
{
    VertexLightCornerSamples samples;
    samples.centre = 15; samples.sideU = 0; samples.sideV = 9; samples.diagonal = 255;
    check("LOCAL_LIGHT/raw-source-average-clamps-without-brightness-floor",
          std::abs(VertexLighting::sourceStrength(samples) - .65f) < 1e-6f);
    samples.sideUOccludes = samples.diagonalOccludes = true;
    check("LOCAL_LIGHT/source-average-excludes-opaque-neighbours",
          std::abs(VertexLighting::sourceStrength(samples) - .8f) < 1e-6f);
    const std::array<float,12> face{0,1,0, 0,1,1, 1,1,1, 1,1,0};
    const std::array<float,8> uv{0,0, 0,1, 1,1, 1,0};
    const std::array<float,4> light{.8f,.8f,.8f,.8f};
    const FaceLightSources sky{glm::vec2(1,0),glm::vec2(1,0),glm::vec2(1,0),glm::vec2(1,0)};
    const FaceLightSources torch{glm::vec2(0,1),glm::vec2(0,1),glm::vec2(0,1),glm::vec2(0,1)};
    ChunkMesh mesh;
    mesh.beginSharedFaces();
    mesh.addSharedFace(face,uv,{0,0,0},{1,1,1},light,false,uv,&sky);
    mesh.addSharedFace(face,uv,{0,0,0},{1,1,1},light,false,uv,&torch);
    mesh.addSharedFace(face,uv,{0,0,0},{1,1,1},light,false,uv,&sky);
    check("LOCAL_LIGHT/shared-vertices-distinguish-sources-with-equal-combined-light",
          mesh.getLight().size() == 8 && mesh.getLightSources()[0] == glm::vec2(1,0) &&
          mesh.getLightSources()[4] == glm::vec2(0,1));
    const auto packed = packTerrainRenderBatch({{{0,0,0},&mesh}},{0,0,0});
    check("LOCAL_LIGHT/packed-source-order-tag-and-size",
          sizeof(TerrainRenderVertex) == 44 && packed.vertices[0].skySource == 1 &&
          packed.vertices[0].blockSource == 1 && packed.vertices[4].skySource == 0 &&
          packed.vertices[4].blockSource == 2 && packed.vertices[4].light == .8f);
    ChunkMesh moved;
    moved.adoptClientData(mesh);
    check("LOCAL_LIGHT/adopt-owns-sources-and-clear-releases-them",
          moved.getLightSources().size() == 8 && mesh.getLightSources().empty());
    moved.clearClientData();
    moved.addFace(face,uv,{0,0,0},{1,1,1},.8f);
    const auto legacy = packTerrainRenderBatch({{{0,0,0},&moved}},{0,0,0});
    check("LOCAL_LIGHT/manual-mesh-retains-legacy-tag", moved.getLightSources().size() == 4 &&
          legacy.vertices[0].skySource == -1 && legacy.vertices[0].blockSource == 0);
    const auto invalid = [&](int mutation) {
        auto copy = moved;
        auto& sources = const_cast<std::vector<glm::vec2>&>(copy.getLightSources());
        if (mutation == 0) sources.pop_back();
        else if (mutation == 1) sources[0] = {0,1.1f};
        else if (mutation == 2) sources[0] = {-1,0};
        else sources[0] = {0,std::numeric_limits<float>::quiet_NaN()};
        try { packTerrainRenderBatch({{{0,0,0},&copy}},{0,0,0}); return false; }
        catch (const std::exception&) { return true; }
    };
    check("LOCAL_LIGHT/reject-missing-out-of-range-mixed-tag-and-nan-sources",
          invalid(0) && invalid(1) && invalid(2) && invalid(3));

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "16 202 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    const auto directory = freshSaveDirectory("local_light_render");
    Config config = makeConfig(); Camera camera(config); Player player;
    World world(camera, config, player, directory, false, 1);
    world.getChunkManager().loadChunk(0,0); world.getChunkManager().loadChunk(1,0);
    for (int x=12;x<=20;++x) for (int z=4;z<=12;++z) for (int y=200;y<=204;++y)
        world.setBlock(x,y,z,(x==12 || x==20 || z==4 || z==12 || y==200 || y==204)
            ? BlockId::Stone : BlockId::Air);
    world.setBlock(16,201,8,BlockId::Torch);
    auto* west = world.getChunkManager().findChunk(0,0)->findSection(12);
    auto* east = world.getChunkManager().findChunk(1,0)->findSection(12);
    check("LOCAL_LIGHT/two-section-enclosed-fixture-available", west && east);
    if (!west || !east) return;
    SectionMeshInput beforeWest, beforeEast;
    west->captureMeshInput(beforeWest); east->captureMeshInput(beforeEast);
    const auto build = [](const SectionMeshInput& input) {
        ChunkMeshCollection result; ChunkMeshBuilder(input,result).buildMesh(); return result;
    };
    const auto litWest = build(beforeWest), litEast = build(beforeEast);
    const auto hasTorchOnly = [](const ChunkMesh& m) {
        return std::any_of(m.getLightSources().begin(), m.getLightSources().end(),
            [](glm::vec2 v) { return v.x == 0 && v.y > .2f; });
    };
    check("LOCAL_LIGHT/real-torch-propagates-independent-source-across-chunk-border",
          hasTorchOnly(litWest.solidMesh) && hasTorchOnly(litEast.solidMesh));
    // Shared floor corners must sample the same halo on both section sides.
    bool found = false, seamEqual = true;
    const auto& a = litWest.solidMesh; const auto& b = litEast.solidMesh;
    for (std::size_t i=0;i<a.getLight().size();++i) {
        const auto& p = a.getClientMesh().vertexPositions;
        if (p[i*3]!=16 || p[i*3+1]!=201 || p[i*3+2]<6 || p[i*3+2]>10) continue;
        for (std::size_t j=0;j<b.getLight().size();++j) {
            const auto& q = b.getClientMesh().vertexPositions;
            if (q[j*3]==p[i*3] && q[j*3+1]==p[i*3+1] && q[j*3+2]==p[i*3+2]) {
                found = true; seamEqual &= glm::length(a.getLightSources()[i]-b.getLightSources()[j]) < 1e-6f;
            }
        }
    }
    check("LOCAL_LIGHT/shared-floor-corners-match-at-chunk-seam", found && seamEqual);
    world.setBlock(16,201,8,BlockId::Air);
    SectionMeshInput darkWest; west->captureMeshInput(darkWest);
    const auto darkMesh = build(darkWest), retained = build(beforeWest);
    check("LOCAL_LIGHT/removal-rebuild-clears-source-but-copied-snapshot-is-stable",
          !hasTorchOnly(darkMesh.solidMesh) && hasTorchOnly(retained.solidMesh) &&
          retained.solidMesh.getLightSources() == litWest.solidMesh.getLightSources());
    for (int x=13;x<=19;++x) for (int z=5;z<=11;++z) world.setBlock(x,204,z,BlockId::Air);
    SectionMeshInput open; west->captureMeshInput(open); const auto unlitOpen = build(open);
    world.setBlock(16,201,8,BlockId::Torch);
    SectionMeshInput mixed; west->captureMeshInput(mixed); const auto litOpen = build(mixed);
    bool mixedFound = false;
    for (const auto v : litOpen.solidMesh.getLightSources()) mixedFound |= v.x == 1 && v.y > .1f;
    check("LOCAL_LIGHT/greedy-keeps-local-gradient-under-full-skylight",
          mixedFound && litOpen.solidMesh.faces > unlitOpen.solidMesh.faces,
          "without/with="+std::to_string(unlitOpen.solidMesh.faces)+"/"+std::to_string(litOpen.solidMesh.faces));
}
