#ifndef HELLOMINE3D_TERRAIN_SURVEY_H
#define HELLOMINE3D_TERRAIN_SURVEY_H

// Offline evidence over production generation. Never called by the client.
#include "../World/Generation/Terrain/ClassicOverWorldGenerator.h"
#include "../World/Chunk/Chunk.h"
#include "../World/WorldCoordinates.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <stdexcept>

namespace TerrainSurvey {
inline constexpr std::array<int, 8> Seeds{{
    0, 1, 42, 424, 20260807, 20260809, 8675309, 325322}};
inline constexpr std::array<int, 12> Boundaries{{
    -161, -160, -159, -17, -16, -15, -1, 0, 1, 15, 16, 17}};

inline void hashByte(std::uint64_t &hash, std::uint8_t value)
{
    hash ^= value;
    hash *= 1099511628211ull;
}

inline std::uint64_t blockHash(const Chunk &chunk)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int y = 0; y < 256; ++y) {
                const auto block = chunk.getBlock(x, y, z);
                hashByte(hash, block.id);
                hashByte(hash, block.metadata);
            }
        }
    }
    for (const auto &entity : chunk.getBlockEntities()) {
        for (const int coordinate : {entity.position.x, entity.position.y,
                                     entity.position.z}) {
            const auto value = static_cast<std::uint32_t>(coordinate);
            for (int byte = 0; byte < 4; ++byte) {
                hashByte(hash, static_cast<std::uint8_t>(value >> (byte * 8)));
            }
        }
        for (const unsigned char value : entity.type) { hashByte(hash, value); }
        hashByte(hash, 0);
        for (const unsigned char value : entity.payload) { hashByte(hash, value); }
        hashByte(hash, 0);
    }
    return hash;
}

inline std::size_t write(World &world, const std::filesystem::path &directory,
                         int version)
{
    if (version < LegacyTerrainGenerationVersion ||
        version > CurrentTerrainGenerationVersion ||
        std::filesystem::exists(directory)) {
        throw std::runtime_error("Survey requires a supported version and new output directory");
    }
    std::filesystem::create_directories(directory);
    std::ofstream samples(directory / "samples.csv");
    std::ofstream chunks(directory / "chunks.csv");
    std::ofstream timings(directory / "timings.csv");
    samples.exceptions(std::ios::failbit | std::ios::badbit);
    chunks.exceptions(std::ios::failbit | std::ios::badbit);
    timings.exceptions(std::ios::failbit | std::ios::badbit);
    samples << "set,seed,version,x,z,height,biome,dx,dz\n";
    chunks << "seed,version,chunk_x,chunk_z,block_hash,coal,iron,wood,plants\n";
    timings << "seed,version,survey_ms,generation_ms\n" << std::setprecision(10);
    std::size_t count = 0;
    for (const int seed : Seeds) {
        ClassicOverWorldGenerator generator(seed, version);
        const auto start = std::chrono::steady_clock::now();
        const auto emit = [&](const char *set, int x, int z) {
            const int height = generator.getSurfaceHeightAtWorld(x, z);
            samples << set << ',' << seed << ',' << version << ',' << x << ','
                    << z << ',' << height << ','
                    << static_cast<int>(generator.getBiomeAtWorld(x, z)) << ','
                    << generator.getSurfaceHeightAtWorld(x + 1, z) - height << ','
                    << generator.getSurfaceHeightAtWorld(x, z + 1) - height << '\n';
            ++count;
        };
        for (int z = -2048; z <= 2048; z += 32) {
            for (int x = -2048; x <= 2048; x += 32) { emit("macro", x, z); }
        }
        for (int z = -64; z <= 64; ++z) {
            for (int x = -64; x <= 64; ++x) { emit("local", x, z); }
        }
        for (const int boundary : Boundaries) {
            for (int other = -512; other <= 512; ++other) {
                emit("axis_x", boundary, other);
                emit("axis_z", other, boundary);
            }
        }
        const auto sampled = std::chrono::steady_clock::now();
        double generationMs = 0.0;
        for (const glm::ivec2 location : {glm::ivec2(2, 2), glm::ivec2(-3, 2),
                                         glm::ivec2(2, -3), glm::ivec2(-3, -3)}) {
            Chunk chunk(world, location, false);
            const auto before = std::chrono::steady_clock::now();
            generator.generateTerrainFor(chunk);
            generationMs += std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - before).count();
            std::array<int, 4> resources{{0, 0, 0, 0}};
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    for (int y = 0; y < 256; ++y) {
                        const auto id = static_cast<BlockId>(chunk.getBlock(x, y, z).id);
                        resources[0] += id == BlockId::CoalOre ? 1 : 0;
                        resources[1] += id == BlockId::IronOre ? 1 : 0;
                        resources[2] += id == BlockId::OakBark ? 1 : 0;
                        resources[3] += id == BlockId::TallGrass ? 1 : 0;
                    }
                }
            }
            chunks << seed << ',' << version << ',' << location.x << ',' << location.y
                   << ',' << blockHash(chunk);
            for (const int amount : resources) { chunks << ',' << amount; }
            chunks << '\n';
        }
        timings << seed << ',' << version << ','
                << std::chrono::duration<double, std::milli>(sampled - start).count()
                << ',' << generationMs << '\n';
    }
    return count;
}

// Exact v7/v8 camera-neighbourhood evidence from the production generator.
// Keep raw columns and blocks so later aggregations can be independently audited.
inline std::size_t writeE2Scenes(World &world,
                                 const std::filesystem::path &directory,
                                 int version)
{
    if (version < LegacyTerrainGenerationVersion ||
        version > CurrentTerrainGenerationVersion ||
        std::filesystem::exists(directory)) {
        throw std::runtime_error("E2 scenes require supported version and new directory");
    }
    std::filesystem::create_directories(directory);
    std::ofstream profiles(directory / "profiles.csv");
    std::ofstream surfaces(directory / "surfaces.csv");
    std::ofstream chunks(directory / "chunks.csv");
    profiles.exceptions(std::ios::failbit | std::ios::badbit);
    surfaces.exceptions(std::ios::failbit | std::ios::badbit);
    chunks.exceptions(std::ios::failbit | std::ios::badbit);
    profiles << "scene,seed,version,x,z,height,biome\n";
    surfaces << "scene,seed,version,x,z,height,biome,top,above,water_at_64\n";
    chunks << "scene,seed,version,chunk_x,chunk_z,block_hash\n";
    constexpr int seed = 20260807;
    ClassicOverWorldGenerator generator(seed, version);
    std::size_t count = 0;
    const auto line = [&](const char *name, int x0, int z0,
                          int dx, int dz, int length) {
        for (int index = 0; index < length; ++index) {
            const int x = x0 + dx * index;
            const int z = z0 + dz * index;
            profiles << name << ',' << seed << ',' << version << ','
                     << x << ',' << z << ','
                     << generator.getSurfaceHeightAtWorld(x, z) << ','
                     << static_cast<int>(generator.getBiomeAtWorld(x, z)) << '\n';
            ++count;
        }
    };
    line("dry_cross", 0, 400, 0, 1, 225);
    line("grass_shore", 128, 576, 0, 1, 225);
    line("dry_along", -128, 512, 1, 0, 257);
    line("forest_edge", 896, 1024, 1, 0, 257);
    line("grass_sand", 64, 544, 1, 0, 129);

    const struct Scene { const char *name; int chunkX; int chunkZ; } scenes[] = {
        {"dry_shore", 0, 32}, {"shallow", 0, 30},
        {"grass_shore", 8, 42},
        {"forest_edge", 64, 64}, {"grass_sand", 7, 34}};
    std::set<std::pair<int, int>> emitted;
    for (const Scene &scene : scenes) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            for (int offsetZ = -1; offsetZ <= 1; ++offsetZ) {
                const int chunkX = scene.chunkX + offsetX;
                const int chunkZ = scene.chunkZ + offsetZ;
                if (!emitted.emplace(chunkX, chunkZ).second) { continue; }
                Chunk chunk(world, {chunkX, chunkZ}, false);
                generator.generateTerrainFor(chunk);
                chunks << scene.name << ',' << seed << ',' << version << ','
                       << chunkX << ',' << chunkZ << ',' << blockHash(chunk) << '\n';
                for (int localX = 0; localX < CHUNK_SIZE; ++localX) {
                    for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ) {
                        const int x = chunkX * CHUNK_SIZE + localX;
                        const int z = chunkZ * CHUNK_SIZE + localZ;
                        const int height = generator.getSurfaceHeightAtWorld(x, z);
                        surfaces << scene.name << ',' << seed << ',' << version
                                 << ',' << x << ',' << z << ',' << height << ','
                                 << static_cast<int>(generator.getBiomeAtWorld(x, z))
                                 << ',' << static_cast<int>(chunk.getBlock(localX, height, localZ).id)
                                 << ',' << static_cast<int>(chunk.getBlock(localX, height + 1, localZ).id)
                                 << ',' << static_cast<int>(chunk.getBlock(localX, 64, localZ).id)
                                 << '\n';
                    }
                }
            }
        }
    }
    return count;
}

inline std::size_t writeE2Coasts(World &world,
                                 const std::filesystem::path &directory,
                                 int version)
{
    if (version < LegacyTerrainGenerationVersion ||
        version > CurrentTerrainGenerationVersion ||
        std::filesystem::exists(directory)) {
        throw std::runtime_error("E2 coasts require supported version and new directory");
    }
    std::filesystem::create_directories(directory);
    std::ofstream transects(directory / "transects.csv");
    std::ofstream blocks(directory / "blocks.csv");
    transects.exceptions(std::ios::failbit | std::ios::badbit);
    blocks.exceptions(std::ios::failbit | std::ios::badbit);
    transects << "seed,version,transect,z,land_x,direction,offset,x,height,biome,planned_surface\n";
    blocks << "seed,version,chunk_x,chunk_z,block_hash,x,z,height,biome,top,above\n";
    std::size_t total = 0;
    for (const int seed : Seeds) {
        ClassicOverWorldGenerator generator(seed, version);
        TerrainFoundation foundation(seed);
        int found = 0;
        for (int z = -1280; z <= 1280 && found < 12; z += 64) {
            int preceding = generator.getSurfaceHeightAtWorld(-1280, z);
            for (int x = -1279; x <= 1280; ++x) {
                const int height = generator.getSurfaceHeightAtWorld(x, z);
                const bool crossing = (preceding < 64 && height >= 64) ||
                    (preceding >= 64 && height < 64);
                preceding = height;
                if (!crossing) { continue; }
                const int direction = height >= 64 ? 1 : -1;
                const int landX = height >= 64 ? x : x - 1;
                for (int offset = -32; offset <= 32; ++offset) {
                    const int sampleX = landX + direction * offset;
                    const int sampleHeight = generator.getSurfaceHeightAtWorld(
                        sampleX, z);
                    const auto plan = version >= SurfaceCoastTerrainGenerationVersion
                        ? foundation.sampleV8(sampleX, z)
                        : foundation.sample(sampleX, z);
                    transects << seed << ',' << version << ',' << found << ','
                              << z << ',' << landX << ',' << direction << ','
                              << offset << ',' << sampleX << ',' << sampleHeight
                              << ',' << static_cast<int>(generator.getBiomeAtWorld(
                                  sampleX, z)) << ','
                              << static_cast<int>(plan.surface) << '\n';
                }
                if (found < 4) {
                    const glm::ivec2 location(
                        WorldCoordinates::floorDiv(landX, CHUNK_SIZE),
                        WorldCoordinates::floorDiv(z, CHUNK_SIZE));
                    Chunk chunk(world, location, false);
                    generator.generateTerrainFor(chunk);
                    const auto fingerprint = blockHash(chunk);
                    for (int localX = 0; localX < CHUNK_SIZE; ++localX) {
                        for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ) {
                            const int worldX = location.x * CHUNK_SIZE + localX;
                            const int worldZ = location.y * CHUNK_SIZE + localZ;
                            const int y = generator.getSurfaceHeightAtWorld(
                                worldX, worldZ);
                            blocks << seed << ',' << version << ','
                                   << location.x << ',' << location.y << ','
                                   << fingerprint << ',' << worldX << ','
                                   << worldZ << ',' << y << ','
                                   << static_cast<int>(generator.getBiomeAtWorld(
                                       worldX, worldZ)) << ','
                                   << static_cast<int>(chunk.getBlock(
                                       localX, y, localZ).id) << ','
                                   << static_cast<int>(chunk.getBlock(
                                       localX, y + 1, localZ).id) << '\n';
                        }
                    }
                }
                ++found;
                ++total;
                break; // one independently spaced shoreline per row
            }
        }
    }
    return total;
}

// Fixed sites selected from paired v8/v9 production macro CSV before any of
// these generated chunks were inspected. The matching provenance and exit
// checks live in docs/reports/ecology-e3-meadow-chunk-sites-2026-09-16.json.
struct MeadowChunkSite {
    int seed;
    int chunkX;
    int chunkZ;
    const char *sign;
};

inline constexpr std::array<MeadowChunkSite, 16> MeadowChunkSites{{
    {0, 14, 74, "positive"}, {0, -124, -22, "negative"},
    {1, 66, 58, "positive"}, {1, -32, -74, "negative"},
    {42, 104, 50, "positive"}, {42, -24, -50, "negative"},
    {424, 18, 58, "positive"}, {424, -16, -16, "negative"},
    {20260807, 96, 50, "positive"}, {20260807, -56, -50, "negative"},
    {20260809, 32, 16, "positive"}, {20260809, -30, -74, "negative"},
    {8675309, 10, 32, "positive"}, {8675309, -116, -62, "negative"},
    {325322, 76, 28, "positive"}, {325322, -70, -40, "negative"}
}};

inline std::size_t writeE3MeadowChunks(
    World &world, const std::filesystem::path &directory)
{
    if (CurrentTerrainGenerationVersion < InlandMeadowTerrainGenerationVersion ||
        std::filesystem::exists(directory)) {
        throw std::runtime_error(
            "E3 meadow chunk survey requires terrain v9 and a new directory");
    }
    std::filesystem::create_directories(directory);
    std::ofstream sites(directory / "sites.csv");
    std::ofstream columns(directory / "columns.csv");
    std::ofstream plans(directory / "plans.csv");
    sites.exceptions(std::ios::failbit | std::ios::badbit);
    columns.exceptions(std::ios::failbit | std::ios::badbit);
    plans.exceptions(std::ios::failbit | std::ios::badbit);
    sites << "site,seed,sign,chunk_x,chunk_z,sample_x,sample_z,"
             "v8_block_hash,v9_block_hash,v8_forest_columns,"
             "converted_columns,height_changes,v9_converted_ground_non_grass,"
             "v9_converted_non_grass_outside_camp,"
             "v9_converted_camp_dirt,v9_converted_trunk_roots,"
             "v8_tree_roots,v9_tree_roots\n";
    columns << "site,seed,sign,chunk_x,chunk_z,x,z,v8_height,v9_height,"
               "v8_biome,v9_biome,v8_surface,v9_surface,v8_top,v9_top,"
               "v8_above,v9_above,converted,v9_raider_camp_cover\n";
    plans << "site,seed,version,type,anchor_x,anchor_y,anchor_z,"
             "minimum_x,maximum_x,minimum_y,maximum_y,"
             "minimum_z,maximum_z\n";
    for (std::size_t index = 0; index < MeadowChunkSites.size(); ++index) {
        const auto site = MeadowChunkSites[index];
        const glm::ivec2 location(site.chunkX, site.chunkZ);
        ClassicOverWorldGenerator oldGenerator(
            site.seed, SurfaceCoastTerrainGenerationVersion);
        ClassicOverWorldGenerator newGenerator(
            site.seed, InlandMeadowTerrainGenerationVersion);
        TerrainFoundation foundation(site.seed);
        Chunk oldChunk(world, location, false);
        Chunk newChunk(world, location, false);
        oldGenerator.generateTerrainFor(oldChunk);
        newGenerator.generateTerrainFor(newChunk);
        const auto oldPlans = oldGenerator.getStructurePlansForChunk(
            site.chunkX, site.chunkZ);
        const auto newPlans = newGenerator.getStructurePlansForChunk(
            site.chunkX, site.chunkZ);
        const auto emitPlans = [&](int version,
                                   const std::vector<StructurePlanSnapshot> &items) {
            for (const auto &plan : items) {
                if (!plan.valid) { continue; }
                plans << index << ',' << site.seed << ',' << version << ','
                      << static_cast<int>(plan.key.type) << ','
                      << plan.anchor.x << ',' << plan.anchor.y << ','
                      << plan.anchor.z << ','
                      << plan.footprint.minimumX << ','
                      << plan.footprint.maximumX << ','
                      << plan.footprint.minimumY << ','
                      << plan.footprint.maximumY << ','
                      << plan.footprint.minimumZ << ','
                      << plan.footprint.maximumZ << '\n';
            }
        };
        emitPlans(SurfaceCoastTerrainGenerationVersion, oldPlans);
        emitPlans(InlandMeadowTerrainGenerationVersion, newPlans);
        std::size_t forestColumns = 0;
        std::size_t convertedColumns = 0;
        std::size_t heightChanges = 0;
        std::size_t nonGrassGround = 0;
        std::size_t nonGrassOutsideCamp = 0;
        std::size_t campDirt = 0;
        std::size_t convertedTrunkRoots = 0;
        std::size_t oldTreeRoots = 0;
        std::size_t newTreeRoots = 0;
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                const int worldX = site.chunkX * CHUNK_SIZE + x;
                const int worldZ = site.chunkZ * CHUNK_SIZE + z;
                const auto oldColumn = foundation.sampleV8(worldX, worldZ);
                const auto newColumn = foundation.sampleV9(worldX, worldZ);
                const bool oldForest = oldColumn.height > 80 &&
                    oldColumn.height < 135 &&
                    (oldColumn.biome == TerrainBiome::LightForest ||
                     oldColumn.biome == TerrainBiome::TemperateForest);
                const bool converted = oldForest &&
                    newColumn.biome == TerrainBiome::Grassland &&
                    newColumn.surface == TerrainFoundation::Surface::Grass;
                const bool campCover = std::any_of(
                    newPlans.begin(), newPlans.end(),
                    [&](const StructurePlanSnapshot &plan) {
                        return plan.valid &&
                            plan.key.type == StructureType::RaiderCamp &&
                            worldX >= plan.anchor.x -
                                DeterministicStructurePlanner::CampRadiusX &&
                            worldX <= plan.anchor.x +
                                DeterministicStructurePlanner::CampRadiusX &&
                            worldZ >= plan.anchor.z -
                                DeterministicStructurePlanner::CampRadiusZ &&
                            worldZ <= plan.anchor.z +
                                DeterministicStructurePlanner::CampRadiusZ;
                    });
                forestColumns += oldForest ? 1 : 0;
                convertedColumns += converted ? 1 : 0;
                heightChanges += oldColumn.height != newColumn.height ? 1 : 0;
                const auto oldTop = oldChunk.getBlock(x, oldColumn.height, z);
                const auto newTop = newChunk.getBlock(x, newColumn.height, z);
                const auto oldAbove = oldChunk.getBlock(
                    x, oldColumn.height + 1, z);
                const auto newAbove = newChunk.getBlock(
                    x, newColumn.height + 1, z);
                oldTreeRoots += oldAbove == BlockId::OakBark ? 1 : 0;
                newTreeRoots += newAbove == BlockId::OakBark ? 1 : 0;
                if (converted) {
                    nonGrassGround += newTop != BlockId::Grass ? 1 : 0;
                    nonGrassOutsideCamp +=
                        newTop != BlockId::Grass && !campCover ? 1 : 0;
                    campDirt +=
                        newTop == BlockId::Dirt && campCover ? 1 : 0;
                    convertedTrunkRoots +=
                        newAbove == BlockId::OakBark ? 1 : 0;
                }
                columns << index << ',' << site.seed << ',' << site.sign
                        << ',' << site.chunkX << ',' << site.chunkZ << ','
                        << worldX << ',' << worldZ << ','
                        << oldColumn.height << ',' << newColumn.height << ','
                        << static_cast<int>(oldColumn.biome) << ','
                        << static_cast<int>(newColumn.biome) << ','
                        << static_cast<int>(oldColumn.surface) << ','
                        << static_cast<int>(newColumn.surface) << ','
                        << static_cast<int>(oldTop.id) << ','
                        << static_cast<int>(newTop.id) << ','
                        << static_cast<int>(oldAbove.id) << ','
                        << static_cast<int>(newAbove.id) << ','
                        << (converted ? 1 : 0) << ','
                        << (campCover ? 1 : 0) << '\n';
            }
        }
        sites << index << ',' << site.seed << ',' << site.sign << ','
              << site.chunkX << ',' << site.chunkZ << ','
              << site.chunkX * CHUNK_SIZE << ','
              << site.chunkZ * CHUNK_SIZE << ','
              << blockHash(oldChunk) << ',' << blockHash(newChunk) << ','
              << forestColumns << ',' << convertedColumns << ','
              << heightChanges << ',' << nonGrassGround << ','
              << nonGrassOutsideCamp << ',' << campDirt << ','
              << convertedTrunkRoots << ',' << oldTreeRoots << ','
              << newTreeRoots << '\n';
    }
    return MeadowChunkSites.size();
}

// Fixed v11 forest regions selected from the frozen E5 macro survey before
// terrain v12 changes any decorator rule. Each site exports a 3x3 production
// Chunk neighbourhood so density, gaps, roots and ground cover remain
// auditable without reimplementing generation in a script.
struct VegetationChunkSite {
    int seed;
    int chunkX;
    int chunkZ;
    const char *sign;
};

inline constexpr std::array<VegetationChunkSite, 16>
VegetationChunkSites{{
    {0, 14, 32, "positive"}, {0, -16, -8, "negative"},
    {1, 8, 28, "positive"}, {1, -30, -18, "negative"},
    {42, 8, 44, "positive"}, {42, -8, -12, "negative"},
    {424, 10, 8, "positive"}, {424, -12, -12, "negative"},
    {20260807, 10, 34, "positive"},
    {20260807, -8, -28, "negative"},
    {20260809, 8, 16, "positive"},
    {20260809, -20, -8, "negative"},
    {8675309, 28, 8, "positive"},
    {8675309, -8, -32, "negative"},
    {325322, 68, 14, "positive"},
    {325322, -32, -8, "negative"}
}};

inline std::size_t writeE6VegetationChunks(
    World &world, const std::filesystem::path &directory, int version)
{
    if (version < ForestEcologyTerrainGenerationVersion ||
        version > CurrentTerrainGenerationVersion ||
        std::filesystem::exists(directory)) {
        throw std::runtime_error(
            "E6 vegetation survey requires a supported forest version and new directory");
    }
    std::filesystem::create_directories(directory);
    std::ofstream sites(directory / "sites.csv");
    std::ofstream chunks(directory / "chunks.csv");
    std::ofstream columns(directory / "columns.csv");
    sites.exceptions(std::ios::failbit | std::ios::badbit);
    chunks.exceptions(std::ios::failbit | std::ios::badbit);
    columns.exceptions(std::ios::failbit | std::ios::badbit);
    sites << "site,seed,sign,version,center_chunk_x,center_chunk_z,"
             "forest_columns,dry_columns,tree_roots,bark,leaves,tall_grass,"
             "roses,dead_shrubs,cacti,min_trunk,max_trunk,unique_trunk_heights\n";
    chunks << "site,seed,sign,version,chunk_x,chunk_z,block_hash,"
              "forest_columns,dry_columns,tree_roots,bark,leaves,tall_grass,"
              "roses,dead_shrubs,cacti,min_trunk,max_trunk\n";
    columns << "site,seed,sign,version,chunk_x,chunk_z,x,z,height,biome,"
               "top,above,trunk_height\n";

    std::size_t generated = 0;
    for (std::size_t siteIndex = 0;
         siteIndex < VegetationChunkSites.size(); ++siteIndex) {
        const auto site = VegetationChunkSites[siteIndex];
        ClassicOverWorldGenerator generator(site.seed, version);
        std::array<std::size_t, 9> totals{};
        int siteMinTrunk = std::numeric_limits<int>::max();
        int siteMaxTrunk = 0;
        std::set<int> siteTrunkHeights;
        for (int offsetZ = -1; offsetZ <= 1; ++offsetZ) {
            for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                const int chunkX = site.chunkX + offsetX;
                const int chunkZ = site.chunkZ + offsetZ;
                Chunk chunk(world, {chunkX, chunkZ}, false);
                generator.generateTerrainFor(chunk);
                std::array<std::size_t, 9> counts{};
                int minTrunk = std::numeric_limits<int>::max();
                int maxTrunk = 0;
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    for (int z = 0; z < CHUNK_SIZE; ++z) {
                        const int worldX = chunkX * CHUNK_SIZE + x;
                        const int worldZ = chunkZ * CHUNK_SIZE + z;
                        const int height = generator.getSurfaceHeightAtWorld(
                            worldX, worldZ);
                        const TerrainBiome biome = generator.getBiomeAtWorld(
                            worldX, worldZ);
                        const ChunkBlock top = chunk.getBlock(x, height, z);
                        const ChunkBlock above = height + 1 < 256
                            ? chunk.getBlock(x, height + 1, z)
                            : ChunkBlock(BlockId::Air);
                        const bool forest =
                            biome == TerrainBiome::LightForest ||
                            biome == TerrainBiome::TemperateForest;
                        counts[0] += forest ? 1 : 0;
                        counts[1] += height >= 64 &&
                            top != BlockId::Air && top != BlockId::Water
                                ? 1 : 0;
                        int trunkHeight = 0;
                        if (above == BlockId::OakBark) {
                            for (int y = height + 1;
                                 y < 256 && y <= height + 16 &&
                                 chunk.getBlock(x, y, z) == BlockId::OakBark;
                                 ++y) {
                                ++trunkHeight;
                            }
                            ++counts[2];
                            minTrunk = std::min(minTrunk, trunkHeight);
                            maxTrunk = std::max(maxTrunk, trunkHeight);
                            siteTrunkHeights.insert(trunkHeight);
                        }
                        for (int y = 0; y < 256; ++y) {
                            const BlockId id = static_cast<BlockId>(
                                chunk.getBlock(x, y, z).id);
                            counts[3] += id == BlockId::OakBark ? 1 : 0;
                            counts[4] += id == BlockId::OakLeaf ? 1 : 0;
                            counts[5] += id == BlockId::TallGrass ? 1 : 0;
                            counts[6] += id == BlockId::Rose ? 1 : 0;
                            counts[7] += id == BlockId::DeadShrub ? 1 : 0;
                            counts[8] += id == BlockId::Cactus ? 1 : 0;
                        }
                        columns << siteIndex << ',' << site.seed << ','
                                << site.sign << ',' << version << ','
                                << chunkX << ',' << chunkZ << ','
                                << worldX << ',' << worldZ << ','
                                << height << ',' << static_cast<int>(biome)
                                << ',' << static_cast<int>(top.id) << ','
                                << static_cast<int>(above.id) << ','
                                << trunkHeight << '\n';
                    }
                }
                for (std::size_t index = 0; index < counts.size(); ++index) {
                    totals[index] += counts[index];
                }
                if (counts[2] > 0) {
                    siteMinTrunk = std::min(siteMinTrunk, minTrunk);
                    siteMaxTrunk = std::max(siteMaxTrunk, maxTrunk);
                }
                chunks << siteIndex << ',' << site.seed << ',' << site.sign
                       << ',' << version << ',' << chunkX << ',' << chunkZ
                       << ',' << blockHash(chunk);
                for (const auto count : counts) { chunks << ',' << count; }
                chunks << ',' << (counts[2] > 0 ? minTrunk : 0)
                       << ',' << maxTrunk << '\n';
                ++generated;
            }
        }
        sites << siteIndex << ',' << site.seed << ',' << site.sign << ','
              << version << ',' << site.chunkX << ',' << site.chunkZ;
        for (const auto total : totals) { sites << ',' << total; }
        sites << ',' << (totals[2] > 0 ? siteMinTrunk : 0)
              << ',' << siteMaxTrunk << ',' << siteTrunkHeights.size()
              << '\n';
    }
    return generated;
}
} // namespace TerrainSurvey

#endif
