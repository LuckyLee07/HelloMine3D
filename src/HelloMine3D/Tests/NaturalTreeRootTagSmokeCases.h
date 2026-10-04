#pragma once

#include "../World/Chunk/NaturalTreeRootTag.h"

namespace {
// Deliberately adjacent equal-material branches isolate ownership from atlas,
// lighting and AO. Capture and meshing still use the production paths.
class RootTagFixtureGenerator final : public TerrainGenerator {
  public:
    std::vector<NaturalTreeOwnershipBlock> records;
    bool supported = true;
    void generateTerrainFor(Chunk &) override {}
    int getMinimumSpawnHeight() const noexcept override { return 75; }
    int getGenerationVersion() const noexcept override { return CurrentTerrainGenerationVersion; }
    TerrainBiome getBiomeAtWorld(int, int) const noexcept override { return TerrainBiome::Grassland; }
    int getSurfaceHeightAtWorld(int, int) const noexcept override { return 74; }
    bool visitNaturalTreeOwnership(int, int, int,
        const NaturalTreeOwnershipVisitor &visitor) const override
    {
        for (const auto &record : records) visitor(record);
        return supported;
    }
};

void caseNaturalTreeRootTag()
{
    const std::array<float, 12> face{{0,1,0, 0,1,1, 1,1,1, 1,1,0}};
    const std::array<float, 8> uv{{0,0, 0,1, 1,1, 1,0}};
    const std::array<float, 4> light{{.8f,.8f,.8f,.8f}};
    constexpr float firstTag = 331.f; // root (4,4) relative to the column
    constexpr float secondTag = 363.f; // root (5,4)
    ChunkMesh shared;
    shared.beginSharedFaces();
    shared.addSharedFace(face, uv, {0,0,0}, {1,1,1}, light, false, uv, nullptr, firstTag);
    shared.addSharedFace(face, uv, {0,0,0}, {1,1,1}, light, false, uv, nullptr, secondTag);
    shared.addSharedFace(face, uv, {0,0,0}, {1,1,1}, light, false, uv, nullptr, firstTag);
    const auto &tags = shared.getRootTags();
    check("TREE_TAG/shared-vertices-retain-distinct-root-identities",
        tags.size() == 8 && shared.getClientMesh().indices.size() == 18 &&
        std::count(tags.begin(), tags.end(), firstTag) == 4 &&
        std::count(tags.begin(), tags.end(), secondTag) == 4);
    ChunkMesh adopted;
    const auto expectedTags = tags;
    adopted.adoptClientData(shared);
    const bool adoptedTags = adopted.getRootTags() == expectedTags && shared.getRootTags().empty();
    adopted.clearClientData();
    const bool clearedTags = adopted.getRootTags().empty();
    adopted.addFace(face, uv, {0,0,0}, {1,1,1}, .8f);
    check("TREE_TAG/adopt-clear-and-manual-mesh-zero-sentinel",
        adoptedTags && clearedTags && adopted.getRootTags() == std::vector<float>(4, 0.f));

    const glm::ivec3 lower{-17,12,-31}, upper{-17,15,-31};
    ChunkMesh a, b;
    a.addFace(face, uv, lower, {2,3,4}, .8f, 1.f, 1.f, nullptr, 1.f);
    b.addFace(face, uv, upper, {5,2,8}, .6f, 1.f, 1.f, nullptr, 892.f);
    const auto packed = packTerrainRenderBatch({{lower,&a},{upper,&b}}, lower);
    const auto decode = [](float tag, glm::ivec3 column) {
        const int code = static_cast<int>(tag) - 1;
        return glm::ivec2(column.x * CHUNK_SIZE + code / 32 - 6,
                         column.z * CHUNK_SIZE + code % 32 - 6);
    };
    check("TREE_TAG/negative-column-vertical-batch-preserves-world-roots",
        packed.vertices.size() == 8 && packed.vertices.front().rootTag == 1.f &&
        packed.vertices.back().rootTag == 892.f &&
        decode(packed.vertices.front().rootTag, lower) == glm::ivec2(-278,-502) &&
        decode(packed.vertices.back().rootTag, lower) == glm::ivec2(-251,-475) &&
        packed.vertices.back().y == 51.f && sizeof(TerrainRenderVertex) == 44);
    const auto rejectsTag = [&](float value, bool truncate = false) {
        auto copy = a;
        auto &stream = const_cast<std::vector<float> &>(copy.getRootTags());
        if (truncate) stream.pop_back(); else stream.front() = value;
        try { (void)packTerrainRenderBatch({{lower,&copy}}, lower); return false; }
        catch (const std::exception &) { return true; }
    };
    check("TREE_TAG/packing-rejects-missing-nonfinite-negative-and-fractional-tags",
        rejectsTag(0.f, true) && rejectsTag(std::numeric_limits<float>::quiet_NaN()) &&
        rejectsTag(std::numeric_limits<float>::infinity()) && rejectsTag(-1.f) && rejectsTag(1.5f));
    bool unusedCodesRejected = true;
    for (int unused = 28; unused < 32; ++unused)
        unusedCodesRejected &= rejectsTag(static_cast<float>(1 + unused)) &&
                               rejectsTag(static_cast<float>(1 + unused * 32));
    check("TREE_TAG/packing-rejects-unused-five-bit-axis-codes", unusedCodesRejected && rejectsTag(1024.f));
    const auto ordinary = packTerrainRenderBatch({{{0,0,0},&adopted}}, {0,0,0});
    a.clearClientData(); b.clearClientData();
    check("TREE_TAG/packed-roots-own-source-copy-and-zero-remains-ordinary",
        packed.vertices.front().rootTag == 1.f && packed.vertices.back().rootTag == 892.f &&
        std::all_of(ordinary.vertices.begin(), ordinary.vertices.end(),
                    [](const TerrainRenderVertex &vertex) { return vertex.rootTag == 0.f; }));

    setEnv("HELLOMINE3D_SEED", "42");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    Config config = makeConfig(); Camera camera(config); Player player;
    World world(camera, config, player, freshSaveDirectory("natural_tree_root_tags"), false, 0);
    const glm::ivec3 location{-2,12,3};
    const glm::ivec3 origin = location * CHUNK_SIZE;
    ChunkSection section(location, world, false);
    RootTagFixtureGenerator fixture;
    for (int x : {4,5}) for (int z : {4,5}) {
        const ChunkBlock block(BlockId::OakBark, BlockMetadata::Tree::Oak);
        section.setBlock(x,8,z,block);
        fixture.records.push_back({{origin.x+x,origin.y+8,origin.z+z}, block,
                                   {origin.x+x,origin.z+4}});
    }
    SectionMeshInput initial;
    initial.capture(section, fixture, 42);
    check("TREE_TAG/snapshot-matches-exact-source-in-signed-column",
        initial.getNaturalTreeRootTag(4,8,4) == firstTag &&
        initial.getNaturalTreeRootTag(5,8,5) == secondTag &&
        initial.getNaturalTreeRootTag(-1,8,4) == 0 &&
        initial.getNaturalTreeRootTag(16,8,4) == 0);

    ChunkMeshCollection ownedMeshes;
    ChunkMeshBuilder(initial, ownedMeshes, false).buildMesh();
    const auto &mesh = ownedMeshes.solidMesh.getClientMesh();
    const auto &meshTags = ownedMeshes.solidMesh.getRootTags();
    bool identities = meshTags.size() == mesh.vertexPositions.size()/3;
    double exposedArea = 0;
    int topTriangles = 0;
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
        glm::vec3 p[3];
        float triangleTag = meshTags[mesh.indices[i]];
        for (int j = 0; j < 3; ++j) {
            const auto vertex = mesh.indices[i+j];
            identities &= meshTags[vertex] == triangleTag;
            p[j] = glm::vec3(mesh.vertexPositions[vertex*3], mesh.vertexPositions[vertex*3+1],
                             mesh.vertexPositions[vertex*3+2]) - glm::vec3(origin);
        }
        const auto cross = glm::cross(p[1]-p[0], p[2]-p[0]);
        const float length = glm::length(cross);
        exposedArea += length*.5;
        topTriangles += cross.y > 0.f;
        if (length <= .0001f) { identities = false; continue; }
        const auto normal = cross/length;
        // A merged quad crossing either root would assign its provoking
        // vertex's flat tag to samples inside the other tree's branch.
        for (const glm::vec3 weights : {glm::vec3(.19f,.34f,.47f),
             glm::vec3(.61f,.23f,.16f), glm::vec3(.12f,.67f,.21f)}) {
            const auto point = p[0]*weights.x+p[1]*weights.y+p[2]*weights.z;
            const auto inner = glm::ivec3(glm::floor(point-normal*.002f));
            identities &= initial.getBlock(inner.x,inner.y,inner.z) == BlockId::OakBark &&
                initial.getNaturalTreeRootTag(inner.x,inner.y,inner.z) == triangleTag;
        }
    }
    check("TREE_TAG/greedy-triangles-cannot-cross-equal-material-root-boundary",
        identities && topTriangles == 4 && std::abs(exposedArea-16.0) < .001,
        "top_triangles="+std::to_string(topTriangles)+" area="+std::to_string(exposedArea));
    fixture.supported = false;
    SectionMeshInput reused = initial;
    reused.capture(section, fixture, 42);
    ChunkMeshCollection ordinaryMeshes;
    ChunkMeshBuilder(reused, ordinaryMeshes, false).buildMesh();
    const auto &plain = ordinaryMeshes.solidMesh.getClientMesh();
    int ordinaryTopTriangles = 0;
    for (std::size_t i=0; i<plain.indices.size(); i+=3) {
        const auto y0=plain.vertexPositions[plain.indices[i]*3+1];
        const auto y1=plain.vertexPositions[plain.indices[i+1]*3+1];
        const auto y2=plain.vertexPositions[plain.indices[i+2]*3+1];
        ordinaryTopTriangles += y0==origin.y+9 && y1==y0 && y2==y0;
    }
    check("TREE_TAG/unsupported-generator-clears-reused-snapshot-and-keeps-greedy",
        reused.getNaturalTreeRootTag(4,8,4) == 0 && ordinaryTopTriangles == 2 &&
        std::all_of(ordinaryMeshes.solidMesh.getRootTags().begin(), ordinaryMeshes.solidMesh.getRootTags().end(),
                    [](float value) { return value == 0.f; }));
    fixture.supported = true;
    section.setBlock(4,8,4,ChunkBlock(BlockId::OakBark, BlockMetadata::Tree::Spruce));
    section.setBlock(4,8,5,BlockId::Stone);
    section.setBlock(8,8,8,BlockId::OakLeaf);
    SectionMeshInput edited;
    edited.capture(section, fixture, 42);
    check("TREE_TAG/edits-filter-id-and-metadata-without-changing-old-snapshot",
        edited.getNaturalTreeRootTag(4,8,4) == 0 && edited.getNaturalTreeRootTag(4,8,5) == 0 &&
        edited.getNaturalTreeRootTag(8,8,8) == 0 && edited.getNaturalTreeRootTag(5,8,5) == secondTag &&
        initial.getNaturalTreeRootTag(4,8,4) == firstTag && initial.getNaturalTreeRootTag(4,8,5) == firstTag);

    auto &manager = world.getChunkManager();
    for (int cx : {21,22}) for (int cz : {-11,-10}) manager.loadChunk(cx,cz);
    const auto residentBefore = manager.collectDebugStats().loadedChunks;
    ClassicOverWorldGenerator generator(42, CurrentTerrainGenerationVersion);
    std::size_t knownMatches = 0, ownedVertices = 0;
    bool actualIdentity = true, homogeneousTriangles = true;
    NaturalTreeOwnershipBlock leaf;
    bool foundLeaf = false;
    SectionMeshInput leafSnapshot;
    for (int cx : {21,22}) for (int cz : {-11,-10}) for (int sy : {4,5}) {
        auto *resident = manager.getChunk(cx,cz).findSection(sy);
        if (!resident) { actualIdentity = false; continue; }
        SectionMeshInput input;
        resident->captureMeshInput(input);
        generator.visitNaturalTreeOwnership(cx,sy,cz,[&](const NaturalTreeOwnershipBlock &record) {
            const int x=record.position[0]-cx*16, y=record.position[1]-sy*16, z=record.position[2]-cz*16;
            const auto tag=input.getNaturalTreeRootTag(x,y,z);
            if (input.getBlock(x,y,z) != record.block) { actualIdentity &= tag==0; return; }
            actualIdentity &= tag>0 && decode(static_cast<float>(tag), {cx,sy,cz}) ==
                glm::ivec2(record.root[0],record.root[1]);
            if (record.root == std::array<int,2>{352,-159}) {
                ++knownMatches;
                if (!foundLeaf && record.block == BlockId::OakLeaf) {
                    leaf=record; leafSnapshot=input; foundLeaf=true;
                }
            }
        });
        ChunkMeshCollection built;
        ChunkMeshBuilder(input,built).buildMesh();
        for (const auto *part : {&built.solidMesh,&built.transparentMesh}) {
            const auto &rootStream=part->getRootTags();
            ownedVertices += std::count_if(rootStream.begin(), rootStream.end(), [](float value) { return value>0; });
            const auto &indices=part->getClientMesh().indices;
            for (std::size_t i=0;i<indices.size();i+=3)
                homogeneousTriangles &= rootStream[indices[i]]==rootStream[indices[i+1]] &&
                                        rootStream[indices[i]]==rootStream[indices[i+2]];
        }
    }
    check("TREE_TAG/real-river-tree-132-blocks-share-root-across-four-chunks-two-sections",
        actualIdentity && knownMatches==132 && manager.collectDebugStats().loadedChunks==residentBefore,
        "matched_blocks="+std::to_string(knownMatches));
    check("TREE_TAG/real-wood-and-leaf-meshes-carry-homogeneous-root-triangles",
        homogeneousTriangles && ownedVertices>0, "owned_vertices="+std::to_string(ownedVertices));
    if (foundLeaf) {
        const auto column=World::getChunkXZ(leaf.position[0],leaf.position[2]);
        auto *resident=manager.getChunk(column.x,column.z).findSection(leaf.position[1]/16);
        const int x=leaf.position[0]-column.x*16, y=leaf.position[1]%16, z=leaf.position[2]-column.z*16;
        world.setBlock(leaf.position[0],leaf.position[1],leaf.position[2],
            ChunkBlock(BlockId::OakLeaf, BlockMetadata::Tree::Spruce));
        SectionMeshInput replaced;
        resident->captureMeshInput(replaced);
        check("TREE_TAG/real-natural-leaf-metadata-edit-drops-only-new-snapshot-owner",
            leafSnapshot.getNaturalTreeRootTag(x,y,z)>0 && replaced.getNaturalTreeRootTag(x,y,z)==0 &&
            replaced.getBlock(x,y,z).metadata==BlockMetadata::Tree::Spruce);
    }
    else check("TREE_TAG/real-natural-leaf-metadata-edit-drops-only-new-snapshot-owner", false);
    clearDeterministicEnv(); setEnv("HELLOMINE3D_SEED", "");
}
} // namespace
