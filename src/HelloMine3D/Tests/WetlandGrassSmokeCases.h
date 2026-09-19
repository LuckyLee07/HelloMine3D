#pragma once

// Uses real generated biome snapshots and the production mesher. The isolated
// high-air sections keep existing plants from obscuring topology assertions.
void caseWetlandGrassPresentation()
{
    struct Site { int seed, x, z; };
    for (const Site site : {Site{0,576,-1152}, Site{1,960,-96}, Site{42,288,-1248}})
    {
        setEnv("HELLOMINE3D_SEED", std::to_string(site.seed));
        setEnv("HELLOMINE3D_PLAYER_POSITION", std::to_string(site.x) + " 200 " + std::to_string(site.z));
        setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
        Config config = makeConfig(); Camera camera(config); Player player;
        World world(camera, config, player, freshSaveDirectory("wetland_grass_mesh"), false, 0);
        const glm::ivec3 position(site.x, 200, site.z);
        auto &chunks = world.getChunkManager();
        const auto biome = chunks.getTerrainGenerator().getBiomeAtWorld(site.x, site.z);
        check("WETLAND_GRASS/production-wetland-fixture", biome == TerrainBiome::Wetland,
              "seed=" + std::to_string(site.seed));
        for (const bool mature : {false, true})
        {
            const ChunkBlock block(BlockId::TallGrass, mature ? BlockMetadata::TallGrass::Mature : BlockMetadata::TallGrass::Immature);
            world.setBlock(position.x, position.y, position.z, block);
            auto *chunk = chunks.findChunk(World::floorDiv(position.x, CHUNK_SIZE),
                                          World::floorDiv(position.z, CHUNK_SIZE));
            auto *section = chunk ? chunk->findSection(position.y / CHUNK_SIZE) : nullptr;
            check("WETLAND_GRASS/real-section-exists", section != nullptr);
            if (!section) continue;
            SectionMeshInput input; section->captureMeshInput(input);
            ChunkMeshCollection meshes; ChunkMeshBuilder(input, meshes).buildMesh();
            const auto &mesh = meshes.floraMesh.getClientMesh();
            const auto faces = blockSurfaceGeometry(BlockDatabase::get().getDefinition(BlockId::TallGrass),
                block, position, biome, site.seed);
            const std::size_t count = mature ? 8u : 6u;
            check("WETLAND_GRASS/production-topology-and-budget",
                faces.size() == count && meshes.floraMesh.faces == static_cast<int>(count) &&
                mesh.vertexPositions.size() == count * 12 && mesh.indices.size() == count * 6 &&
                meshes.solidMesh.faces == 0 && meshes.waterMesh.faces == 0 && meshes.transparentMesh.faces == 0);
            bool aligned = mesh.vertexPositions.size() == count * 12 && faces.size() == count;
            if (aligned) for (std::size_t f = 0; f < count; ++f)
            {
                const auto uv = BlockTextureCoordinates::get(faces[f].tile.x, faces[f].tile.y);
                for (std::size_t v = 0; v < 12; ++v)
                    aligned &= mesh.vertexPositions[f*12+v] == faces[f].positions[v] + static_cast<float>(position[v%3]);
                for (std::size_t v = 0; v < 8; ++v)
                    aligned &= mesh.textureCoords[f*8+v] == uv[v] &&
                               mesh.textureRepeatCoords[f*8+v] == faces[f].repeat[v];
            }
            check("WETLAND_GRASS/highlight-shape-tile-and-wind-match-real-mesh", aligned);
            check("WETLAND_GRASS/mesh-build-keeps-block-metadata",
                  world.getBlock(position.x, position.y, position.z) == block);
            ChunkMeshCollection again; ChunkMeshBuilder(input, again).buildMesh();
            check("WETLAND_GRASS/snapshot-rebuild-deterministic",
                  mesh.vertexPositions == again.floraMesh.getClientMesh().vertexPositions &&
                  mesh.textureRepeatCoords == again.floraMesh.getClientMesh().textureRepeatCoords &&
                  mesh.indices == again.floraMesh.getClientMesh().indices);
        }
    }
}
