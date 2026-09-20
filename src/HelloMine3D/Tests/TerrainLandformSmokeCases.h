#pragma once

// Included after the WorldRuntime fixture helpers. Coordinates were frozen
// before visual capture from the largest four-connected macro-grid component
// of each biome, choosing its member nearest the component centroid.
namespace {
struct LandformSite { int seed, biome, x, z; };
constexpr std::array<LandformSite, 48> LandformSites{{
    {0, 0, -800, -704},
    {0, 6, 576, -1152},
    {0, 7, -992, 576},
    {0, 1, 1312, -1632},
    {0, 3, 32, 1888},
    {0, 4, 1472, 1600},
    {1, 0, 1472, 1376},
    {1, 6, 960, -96},
    {1, 7, -1280, -1568},
    {1, 1, 1728, -1728},
    {1, 3, 672, -1152},
    {1, 4, -832, 0},
    {42, 0, -384, -1120},
    {42, 6, 288, -1248},
    {42, 7, 0, 480},
    {42, 1, -1440, 384},
    {42, 3, -32, 1664},
    {42, 4, -960, 1408},
    {424, 0, -1664, -1408},
    {424, 6, 448, 32},
    {424, 7, 1184, 512},
    {424, 1, 480, 736},
    {424, 3, 320, 1312},
    {424, 4, 896, -1216},
    {20260807, 0, -384, 1696},
    {20260807, 6, 1088, -1504},
    {20260807, 7, 1408, 128},
    {20260807, 1, -736, 704},
    {20260807, 3, 160, 1024},
    {20260807, 4, -1088, -672},
    {20260809, 0, -1280, 32},
    {20260809, 6, 896, -1696},
    {20260809, 7, -128, 1312},
    {20260809, 1, 1088, 1216},
    {20260809, 3, -1408, -1440},
    {20260809, 4, 160, -672},
    {8675309, 0, 1216, -1408},
    {8675309, 6, -320, 1216},
    {8675309, 7, 832, -608},
    {8675309, 1, -992, 0},
    {8675309, 3, -1728, 544},
    {8675309, 4, 1088, 1280},
    {325322, 0, 32, -1152},
    {325322, 6, -864, 1248},
    {325322, 7, 1440, 1152},
    {325322, 1, -704, -544},
    {325322, 3, 384, -448},
    {325322, 4, 384, 480},
}};

// Export dense production columns, explicit cave/structure exclusions and
// actual blocks. A survey cannot pass by counting only its planned labels.
void writeLandformRegions()
{
    const char *directory = std::getenv("HELLOMINE3D_TERRAIN_SURVEY_DIR");
    if (!directory || std::filesystem::exists(directory))
        throw std::runtime_error("E7 region survey requires a new output directory");
    std::filesystem::create_directories(directory);
    std::ofstream columns(std::filesystem::path(directory) / "columns.csv");
    std::ofstream summaries(std::filesystem::path(directory) / "regions.csv");
    columns.exceptions(std::ios::failbit | std::ios::badbit);
    summaries.exceptions(std::ios::failbit | std::ios::badbit);
    columns << "seed,region_biome,x,z,height,biome,surface,top,above,water64,dx,dz,structure,cave,ore,matched\n";
    summaries << "seed,biome,x,z,min_height,max_height,water,dry_grass,max_step,mismatches,structure_columns,cave_columns,ore_columns,generation_ms\n";
    Config config = makeConfig(); Camera camera(config); Player owner;
    setEnv("HELLOMINE3D_SEED", "20260807");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    World world(camera, config, owner, freshSaveDirectory("e7_region_survey"), false, 0);
    for (const auto site : LandformSites) {
        if (site.seed != 0 && site.seed != 1 && site.seed != 42) continue;
        ClassicOverWorldGenerator generator(site.seed, 13);
        TerrainFoundation foundation(site.seed); CaveGenerator caves(site.seed, 13);
        const auto surface = [&generator](int x, int z) { return generator.getSurfaceHeightAtWorld(x, z); };
        const auto biome = [&generator](int x, int z) { return generator.getBiomeAtWorld(x, z); };
        int low=176, high=0, water=0, grass=0, maxStep=0, mismatches=0, structureCount=0, caveCount=0, oreCount=0;
        double generationMs=0;
        for (int cz=site.z/CHUNK_SIZE-4; cz<site.z/CHUNK_SIZE+4; ++cz)
            for (int cx=site.x/CHUNK_SIZE-4; cx<site.x/CHUNK_SIZE+4; ++cx) {
                Chunk chunk(world, {cx,cz}, false), caveMask(world, {cx,cz}, false);
                const auto before=std::chrono::steady_clock::now();
                generator.generateTerrainFor(chunk);
                generationMs += std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
                for (int x=0;x<CHUNK_SIZE;++x) for(int z=0;z<CHUNK_SIZE;++z)
                    caveMask.setBlock(x,surface(cx*CHUNK_SIZE+x,cz*CHUNK_SIZE+z),z,BlockId::Stone);
                caves.carveNaturalEntrances(caveMask,surface,biome);
                const auto structures=generator.getStructurePlansForChunk(cx,cz);
                for (int x=0;x<CHUNK_SIZE;++x) for(int z=0;z<CHUNK_SIZE;++z) {
                    const int wx=cx*CHUNK_SIZE+x,wz=cz*CHUNK_SIZE+z;
                    const auto column=foundation.sampleV13(wx,wz);
                    const int h=column.height;
                    low=std::min(low,h);high=std::max(high,h);
                    const int dx=surface(wx+1,wz)-h,dz=surface(wx,wz+1)-h;
                    maxStep=std::max({maxStep,std::abs(dx),std::abs(dz)});
                    const auto top=static_cast<BlockId>(chunk.getBlock(x,h,z).id);
                    const auto above=static_cast<BlockId>(chunk.getBlock(x,h+1,z).id);
                    const bool water64=chunk.getBlock(x,WATER_LEVEL,z)==BlockId::Water;
                    water+=water64;grass+=h>=WATER_LEVEL && top==BlockId::Grass;
                    bool structure=false;
                    for(const auto &plan:structures) {
                        const auto &f=plan.footprint;
                        structure |= plan.valid && wx>=f.minimumX && wx<=f.maximumX && wz>=f.minimumZ && wz<=f.maximumZ && h>=f.minimumY && h<=f.maximumY;
                    }
                    const bool cave=caveMask.getBlock(x,h,z)==BlockId::Air;
                    BlockId expected=BlockId::Air;
                    switch(column.surface) {
                        case TerrainFoundation::Surface::Grass:expected=BlockId::Grass;break;
                        case TerrainFoundation::Surface::Dirt:expected=BlockId::Dirt;break;
                        case TerrainFoundation::Surface::Sand:expected=BlockId::Sand;break;
                        case TerrainFoundation::Surface::Stone:expected=BlockId::Stone;break;
                        case TerrainFoundation::Surface::Original:break;
                        default:throw std::logic_error("unexpected surface in frozen terrain fixture");
                    }
                    if(column.biome==TerrainBiome::Mountain && h>=100)expected=BlockId::Stone;
                    const bool ore=expected==BlockId::Stone && (top==BlockId::CoalOre || top==BlockId::IronOre);
                    const bool matched=structure || (cave && top==BlockId::Air) || ore ||
                        (expected==BlockId::Air ? top!=BlockId::Air && top!=BlockId::Water : top==expected);
                    mismatches+=!matched;structureCount+=structure;caveCount+=cave;oreCount+=ore;
                    columns << site.seed << ',' << site.biome << ',' << wx << ',' << wz << ',' << h << ',' << static_cast<int>(column.biome)
                        << ',' << static_cast<int>(column.surface) << ',' << static_cast<int>(top) << ',' << static_cast<int>(above)
                        << ',' << water64 << ',' << dx << ',' << dz << ',' << structure << ',' << cave << ',' << ore << ',' << matched << '\n';
                }
            }
        summaries << site.seed << ',' << site.biome << ',' << site.x << ',' << site.z << ',' << low << ',' << high << ',' << water << ',' << grass
            << ',' << maxStep << ',' << mismatches << ',' << structureCount << ',' << caveCount << ',' << oreCount << ',' << generationMs << '\n';
        const bool silhouette=site.biome==0 ? high-low>=6 : site.biome==7 ? high-low>=12 : site.biome==6 ? water>0 && grass>0 : true;
        check("E7/dense-region-"+std::to_string(site.seed)+"-biome-"+std::to_string(site.biome),
              silhouette && maxStep<=6 && mismatches==0,
              "height="+std::to_string(low)+".."+std::to_string(high)+" water="+std::to_string(water)+" grass="+std::to_string(grass)+" step="+std::to_string(maxStep)+" mismatches="+std::to_string(mismatches));
    }
    clearDeterministicEnv();setEnv("HELLOMINE3D_SEED", "");
}

void caseLandformDiversityV13()
{
    check("E7/version-and-biomes-append-without-renumbering",
          VegetationMosaicTerrainGenerationVersion == 12 &&
          LandformDiversityTerrainGenerationVersion == 13 &&
          CurrentTerrainGenerationVersion >= 13 &&
          static_cast<int>(TerrainBiome::Mountain) == 5 &&
          static_cast<int>(TerrainBiome::Wetland) == 6 &&
          static_cast<int>(TerrainBiome::RockPlateau) == 7);
    setEnv("HELLOMINE3D_SEED", "20260807");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    Config config = makeConfig();
    Camera camera(config);
    Player owner;
    World world(camera, config, owner, freshSaveDirectory("e7_detached"), false, 0);
    std::array<std::array<int, 7>, TerrainSurvey::Seeds.size()> resources{};
    bool queryAgreement = true, ordered = true, suitable = true;
    int roots = 0, plants = 0;
    for (const auto site : LandformSites) {
        ClassicOverWorldGenerator forward(site.seed, 13), reverse(site.seed, 13);
        TerrainFoundation foundation(site.seed);
        const auto seedIndex = static_cast<std::size_t>(std::distance(
            TerrainSurvey::Seeds.begin(), std::find(TerrainSurvey::Seeds.begin(),
                                                    TerrainSurvey::Seeds.end(), site.seed)));
        std::array<std::uint64_t, 4> hashes{};
        for (int index = 0; index < 4; ++index) {
            const glm::ivec2 location(site.x / CHUNK_SIZE + index % 2,
                                      site.z / CHUNK_SIZE + index / 2);
            Chunk chunk(world, location, false);
            forward.generateTerrainFor(chunk);
            hashes[index] = TerrainSurvey::blockHash(chunk);
            for (int x = 0; x < CHUNK_SIZE; ++x) for (int z = 0; z < CHUNK_SIZE; ++z) {
                const int wx = location.x * CHUNK_SIZE + x, wz = location.y * CHUNK_SIZE + z;
                const auto column = foundation.sampleV13(wx, wz);
                queryAgreement &= forward.getSurfaceHeightAtWorld(wx, wz) == column.height &&
                    forward.getBiomeAtWorld(wx, wz) == column.biome;
                const auto top = static_cast<BlockId>(chunk.getBlock(x, column.height, z).id);
                const auto above = static_cast<BlockId>(chunk.getBlock(x, column.height + 1, z).id);
                const bool root = above == BlockId::OakBark;
                const bool grass = above == BlockId::TallGrass || above == BlockId::Rose;
                const bool shrub = above == BlockId::DeadShrub;
                roots += root; plants += grass || shrub;
                suitable &= (!root || (column.height >= WATER_LEVEL + 1 &&
                    top != BlockId::Air && top != BlockId::Water)) &&
                    (!grass || top == BlockId::Grass) && (!shrub || top == BlockId::Sand) &&
                    !(column.height < WATER_LEVEL && (root || grass || shrub));
                for (int y = 0; y < 256; ++y) {
                    const auto id = static_cast<BlockId>(chunk.getBlock(x, y, z).id);
                    const std::array<BlockId, 7> wanted{{BlockId::OakBark, BlockId::Stone,
                        BlockId::Sand, BlockId::Grass, BlockId::CoalOre, BlockId::IronOre, BlockId::Water}};
                    for (std::size_t i = 0; i < wanted.size(); ++i) resources[seedIndex][i] += id == wanted[i];
                }
            }
        }
        for (int index = 3; index >= 0; --index) {
            Chunk chunk(world, {site.x / CHUNK_SIZE + index % 2,
                                 site.z / CHUNK_SIZE + index / 2}, false);
            reverse.generateTerrainFor(chunk);
            ordered &= hashes[index] == TerrainSurvey::blockHash(chunk);
        }
    }
    check("E7/public-queries-match-generated-column-planning", queryAgreement);
    check("E7/forty-eight-sites-reverse-independent-chunk-generation", ordered);
    check("E7/surface-plants-are-supported-and-dry", suitable && roots > 0 && plants > 0,
          "roots=" + std::to_string(roots) + " plants=" + std::to_string(plants));
    for (std::size_t i = 0; i < resources.size(); ++i) {
        std::ostringstream detail;
        for (int value : resources[i]) detail << value << ' ';
        check("E7/seed-" + std::to_string(TerrainSurvey::Seeds[i]) + "-wood-stone-sand-grass-coal-iron-water",
              std::all_of(resources[i].begin(), resources[i].end(), [](int value) { return value > 0; }), detail.str());
    }

    clearDeterministicEnv();
    for (const int seed : TerrainSurvey::Seeds) {
        setEnv("HELLOMINE3D_SEED", std::to_string(seed));
        Player spawned;
        const auto directory = freshSaveDirectory("e7_spawn_" + std::to_string(seed));
        const bool initialized = initializeTerrainIdentity(directory, "e7-spawn-" + std::to_string(seed), 13, seed, false);
        World habitat(camera, config, spawned, directory, false, 0);
        const int x = static_cast<int>(std::floor(spawned.position.x));
        const int z = static_cast<int>(std::floor(spawned.position.z));
        const int y = static_cast<int>(spawned.position.y) - 2;
        const auto ground = static_cast<BlockId>(habitat.getBlock(x, y, z).id);
        check("E7/seed-" + std::to_string(seed) + "-safe-spawn",
              initialized && ground != BlockId::Air && ground != BlockId::Water &&
              habitat.getBlock(x, y + 1, z) == BlockId::Air && habitat.getBlock(x, y + 2, z) == BlockId::Air &&
              habitat.getChunkManager().getTerrainGenerationVersion() == 13);
        ClassicOverWorldGenerator generator(seed, 13);
        const auto surface = [&generator](int wx, int wz) { return generator.getSurfaceHeightAtWorld(wx, wz); };
        const auto biome = [&generator](int wx, int wz) { return generator.getBiomeAtWorld(wx, wz); };
        CaveGenerator caves(seed, 13);
        CaveGenerator::NaturalEntrance entrance;
        for (int radius = 0; radius <= 32 && !entrance.valid; ++radius)
            for (int cx = -radius; cx <= radius && !entrance.valid; ++cx)
                for (int cz = -radius; cz <= radius; ++cz) {
                    if (radius && std::abs(cx) != radius && std::abs(cz) != radius) continue;
                    entrance = caves.getNaturalEntranceForCell(cx, cz, surface, biome);
                    if (entrance.valid) break;
                }
        bool mouthOpen = false;
        if (entrance.valid) {
            const int cx = WorldCoordinates::floorDiv(entrance.anchorX, CHUNK_SIZE);
            const int cz = WorldCoordinates::floorDiv(entrance.anchorZ, CHUNK_SIZE);
            Chunk mouth(world, {cx, cz}, false);
            generator.generateTerrainFor(mouth);
            mouthOpen = mouth.getBlock(WorldCoordinates::floorMod(entrance.anchorX, CHUNK_SIZE),
                entrance.anchorY + 1, WorldCoordinates::floorMod(entrance.anchorZ, CHUNK_SIZE)) == BlockId::Air;
        }
        check("E7/seed-" + std::to_string(seed) + "-natural-cave-mouth", entrance.valid && mouthOpen);
        std::array<bool, 3> sites{};
        for (int cz = -16; cz <= 16; ++cz) for (int cx = -16; cx <= 16; ++cx) {
            for (const auto type : {StructureType::Waystone, StructureType::Ruin, StructureType::RaiderCamp}) {
                const auto index = static_cast<std::size_t>(type);
                if (sites[index]) continue;
                const auto plan = generator.getStructurePlanForCell(type, cx, cz);
                if (!plan.valid || !plan.footprint.valid() || surface(plan.anchor.x, plan.anchor.z) < WATER_LEVEL) continue;
                for (const auto direction : {glm::ivec2(1,0), glm::ivec2(-1,0), glm::ivec2(0,1), glm::ivec2(0,-1)}) {
                    bool approach = true;
                    int last = surface(plan.anchor.x, plan.anchor.z);
                    for (int step = 1; step <= 8; ++step) {
                        const int next = surface(plan.anchor.x + step * direction.x, plan.anchor.z + step * direction.y);
                        approach &= next >= WATER_LEVEL && std::abs(next - last) <= 3; last = next;
                    }
                    sites[index] |= approach;
                }
            }
        }
        check("E7/seed-" + std::to_string(seed) + "-three-sites-dry-eight-block-approach",
              std::all_of(sites.begin(), sites.end(), [](bool value) { return value; }));
    }
    setEnv("HELLOMINE3D_SEED", "20260807");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    const auto directory = freshSaveDirectory("e7_new_world_reopen");
    bool persisted = initializeTerrainIdentity(directory, "e7-preserved-v13", 13, 20260807);
    std::uint64_t chunkHash = 0;
    {
        Player player;
        World created(camera, config, player, directory, false, 0);
        auto &chunks = created.getChunkManager();
        chunks.loadChunk(-1, -1);
        created.setBlock(-2, 190, -2, BlockId::OakBark);
        chunkHash = TerrainSurvey::blockHash(chunks.getChunk(-1, -1));
        persisted = persisted && chunks.getTerrainGenerationVersion() == 13 && created.save();
    }
    {
        Player player;
        World reopened(camera, config, player, directory, false, 0);
        reopened.getChunkManager().loadChunk(-1, -1);
        persisted &= reopened.getChunkManager().getTerrainGenerationVersion() == 13 &&
            reopened.getBlock(-2, 190, -2) == BlockId::OakBark &&
            chunkHash == TerrainSurvey::blockHash(reopened.getChunkManager().getChunk(-1, -1));
    }
    check("E7/preserved-v13-save-edit-and-complete-chunk-reopen", persisted);
    WorldSaveData identity;
    bool rejected = WorldSave(directory).load(identity);
    identity.terrainGenerationVersion = CurrentTerrainGenerationVersion + 1;
    WorldSaveData preserved;
    rejected &= !WorldSave(directory).save(identity) && WorldSave(directory).load(preserved) && preserved.terrainGenerationVersion == 13;
    check("E7/future-terrain-version-rejected-without-overwriting-save", rejected);
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "");
}
} // namespace
