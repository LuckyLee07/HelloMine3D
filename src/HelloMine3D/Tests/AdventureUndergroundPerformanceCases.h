#pragma once

// B7 headless generation timing and geometry evidence. The functions in this
// file are intentionally not part of the default smoke run: the caller must
// select one of the dedicated focus modes and provide a fresh output path.

#include "TerrainSurvey.h"

#include "../Config.h"
#include "../Core/Camera.h"
#include "../Diagnostics/TerrainBufferMetrics.h"
#include "../Player/Player.h"
#include "../World/Chunk/ChunkMeshBuilder.h"
#include "../World/Chunk/SectionMeshInput.h"
#include "../World/Generation/Terrain/CaveGenerator.h"
#include "../World/Storage/WorldSave.h"
#include "../World/World.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <sys/resource.h>
#endif

namespace {

struct B7UndergroundPerformanceFixture {
    int seed;
    int cellX;
    int cellZ;
    int anchorX;
    int anchorY;
    int anchorZ;
    int directionX;
    int directionZ;
    int endY;
};

inline constexpr std::array<B7UndergroundPerformanceFixture, 3>
    B7UndergroundPerformanceFixtures{{
        {42, 11, 2, 1139, 120, 237, 0, 1, 102},
        {20260807, -5, -5, -465, 89, -431, 0, -1, 71},
        {239701883, 3, -6, 364, 95, -507, 1, 0, 77},
    }};

// Keep the timed path independent from v23 plan discovery. Plan discovery
// queries the shared thread-local surface cache, so doing it in the measured
// process would warm v23 target columns without giving v22 the same benefit.
inline constexpr std::array<std::pair<int, int>, 16>
    B7UndergroundSeed42AffectedChunks{{
        {71, 14}, {70, 14}, {71, 15}, {72, 14},
        {70, 15}, {72, 15}, {71, 16}, {73, 14},
        {70, 16}, {72, 16}, {73, 15}, {73, 16},
        {71, 17}, {70, 17}, {72, 17}, {73, 17},
    }};
inline constexpr std::array<std::pair<int, int>, 10>
    B7UndergroundSeed20260807AffectedChunks{{
        {-30, -27}, {-30, -28}, {-29, -27}, {-29, -28},
        {-30, -29}, {-29, -29}, {-30, -30}, {-29, -30},
        {-30, -31}, {-29, -31},
    }};
inline constexpr std::array<std::pair<int, int>, 10>
    B7UndergroundSeed239701883AffectedChunks{{
        {22, -32}, {22, -33}, {23, -32}, {23, -33},
        {24, -32}, {24, -33}, {25, -32}, {25, -33},
        {26, -32}, {26, -33},
    }};

struct B7UndergroundGeneratedChunk {
    glm::ivec2 location{0};
    std::unique_ptr<Chunk> chunk;
};

struct B7UndergroundMeshMetrics {
    std::size_t targetChunks = 0;
    std::size_t haloChunks = 0;
    std::size_t sections = 0;
    std::size_t emittingSections = 0;
    std::size_t solidFaces = 0;
    std::size_t transparentFaces = 0;
    std::size_t waterFaces = 0;
    std::size_t floraFaces = 0;
    std::size_t vertices = 0;
    std::size_t indices = 0;
    std::size_t renderables = 0;
    std::size_t bufferBytes = 0;
};

struct B7UndergroundGeometryMetrics {
    std::size_t changedBlocks = 0;
    std::size_t changedToAir = 0;
    std::size_t changedToWater = 0;
    std::size_t changedToStructure = 0;
    std::size_t changedSections = 0;
    std::size_t changedChunks = 0;
    long long blockEntityDelta = 0;
    B7UndergroundMeshMetrics v22;
    B7UndergroundMeshMetrics v23;
};

struct B7UndergroundTimedChunk {
    glm::ivec2 location{0};
    std::uint64_t elapsedNanoseconds = 0;
    std::uint64_t blockHash = 0;
    std::size_t sections = 0;
    std::size_t blockEntities = 0;
};

const char *b7UndergroundCompiledConfiguration() noexcept
{
#if defined(NDEBUG)
    return "Release";
#else
    return "Debug";
#endif
}

const char *b7UndergroundRequiredEnvironment(const char *name)
{
    const char *value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        throw std::runtime_error(std::string("Missing environment: ") + name);
    }
    return value;
}

int b7UndergroundParseInteger(const char *name, int minimum, int maximum)
{
    const std::string text = b7UndergroundRequiredEnvironment(name);
    std::size_t consumed = 0;
    long long value = 0;
    try {
        value = std::stoll(text, &consumed, 10);
    }
    catch (const std::exception &) {
        throw std::runtime_error(std::string("Invalid integer in ") + name);
    }
    if (consumed != text.size() || value < minimum || value > maximum) {
        throw std::runtime_error(std::string("Out-of-range integer in ") + name);
    }
    return static_cast<int>(value);
}

std::string b7UndergroundExecutableHash()
{
    std::string value = b7UndergroundRequiredEnvironment(
        "HELLOMINE3D_UNDERGROUND_EXECUTABLE_SHA256");
    if (value.size() != 64 ||
        !std::all_of(value.begin(), value.end(), [](unsigned char character) {
            return std::isxdigit(character) != 0;
        })) {
        throw std::runtime_error("Executable SHA-256 must contain 64 hex digits");
    }
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

std::string b7UndergroundConfiguration()
{
    const std::string value = b7UndergroundRequiredEnvironment(
        "HELLOMINE3D_UNDERGROUND_PERF_CONFIG");
    if (value != "Debug" && value != "Release") {
        throw std::runtime_error("Performance configuration must be Debug or Release");
    }
    if (value != b7UndergroundCompiledConfiguration()) {
        throw std::runtime_error(
            "Performance configuration does not match the compiled binary");
    }
    return value;
}

void b7UndergroundSetEnvironment(const char *name, const std::string &value)
{
#if defined(_WIN32)
    if (_putenv_s(name, value.c_str()) != 0) {
        throw std::runtime_error(std::string("Unable to set environment: ") + name);
    }
#else
    if (setenv(name, value.c_str(), 1) != 0) {
        throw std::runtime_error(std::string("Unable to set environment: ") + name);
    }
#endif
}

std::string b7UndergroundCsvField(const std::string &value)
{
    if (value.find_first_of(",\"\r\n") == std::string::npos) {
        return value;
    }
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (const char character : value) {
        if (character == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(character);
    }
    escaped.push_back('"');
    return escaped;
}

std::ofstream b7UndergroundOpenCsv(const std::filesystem::path &path)
{
    std::ofstream output(path, std::ios::out | std::ios::trunc);
    output.exceptions(std::ios::failbit | std::ios::badbit);
    return output;
}

std::filesystem::path b7UndergroundCreateFreshDirectory(const char *variable)
{
    const std::filesystem::path path =
        b7UndergroundRequiredEnvironment(variable);
    std::error_code error;
    if (std::filesystem::exists(path, error) || error) {
        throw std::runtime_error(
            std::string("Evidence directory must not exist: ") + path.string());
    }
    if (!std::filesystem::create_directories(path, error) || error) {
        throw std::runtime_error(
            std::string("Unable to create evidence directory: ") + path.string());
    }
    return path;
}

std::uint64_t b7UndergroundPeakRssBytes() noexcept
{
#if defined(_WIN32)
    return 0;
#else
    struct rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) != 0 || usage.ru_maxrss < 0) {
        return 0;
    }
    const std::uint64_t value = static_cast<std::uint64_t>(usage.ru_maxrss);
#if defined(__APPLE__)
    return value;
#else
    if (value > std::numeric_limits<std::uint64_t>::max() / 1024ull) {
        return 0;
    }
    return value * 1024ull;
#endif
#endif
}

const B7UndergroundPerformanceFixture &
b7UndergroundFixtureForSeed(int seed)
{
    const auto found = std::find_if(
        B7UndergroundPerformanceFixtures.begin(),
        B7UndergroundPerformanceFixtures.end(),
        [seed](const B7UndergroundPerformanceFixture &fixture) {
            return fixture.seed == seed;
        });
    if (found == B7UndergroundPerformanceFixtures.end()) {
        throw std::runtime_error("Seed is not part of the frozen B7 protocol");
    }
    return *found;
}

template <std::size_t Size>
std::vector<glm::ivec2> b7UndergroundChunkLocations(
    const std::array<std::pair<int, int>, Size> &locations)
{
    std::vector<glm::ivec2> result;
    result.reserve(Size);
    for (const auto &location : locations) {
        result.emplace_back(location.first, location.second);
    }
    return result;
}

std::vector<glm::ivec2> b7UndergroundFrozenAffectedLocations(int seed)
{
    switch (seed) {
    case 42:
        return b7UndergroundChunkLocations(
            B7UndergroundSeed42AffectedChunks);
    case 20260807:
        return b7UndergroundChunkLocations(
            B7UndergroundSeed20260807AffectedChunks);
    case 239701883:
        return b7UndergroundChunkLocations(
            B7UndergroundSeed239701883AffectedChunks);
    default:
        throw std::runtime_error(
            "Seed has no frozen B7 affected chunk order");
    }
}

CaveGenerator::AdventureUndergroundPlan b7UndergroundFrozenPlan(
    const B7UndergroundPerformanceFixture &fixture)
{
    ClassicOverWorldGenerator terrain(
        fixture.seed, AdventureUndergroundTerrainGenerationVersion);
    CaveGenerator cave(
        fixture.seed, AdventureUndergroundTerrainGenerationVersion);
    const auto plan = cave.getAdventureUndergroundPlanForCell(
        fixture.cellX, fixture.cellZ,
        [&terrain](int x, int z) {
            return terrain.getSurfaceHeightAtWorld(x, z);
        },
        [&terrain](int x, int z) {
            return terrain.getBiomeAtWorld(x, z);
        });
    if (!plan.valid || !plan.entrance.valid ||
        plan.entrance.anchorX != fixture.anchorX ||
        plan.entrance.anchorY != fixture.anchorY ||
        plan.entrance.anchorZ != fixture.anchorZ ||
        plan.entrance.directionX != fixture.directionX ||
        plan.entrance.directionZ != fixture.directionZ ||
        plan.entrance.endY != fixture.endY) {
        throw std::runtime_error("Frozen B7 entrance identity changed");
    }
    return plan;
}

std::vector<glm::ivec2> b7UndergroundAffectedLocations(
    const CaveGenerator::AdventureUndergroundPlan &plan)
{
    const int poolX = plan.chamberX + plan.poolDirectionX * 5;
    const int poolZ = plan.chamberZ + plan.poolDirectionZ * 5;
    const int minimumX = std::min({plan.entrance.anchorX,
        plan.chamberX - 10, plan.destinationX - 6, poolX - 4});
    const int maximumX = std::max({plan.entrance.anchorX,
        plan.chamberX + 10, plan.destinationX + 6, poolX + 4});
    const int minimumZ = std::min({plan.entrance.anchorZ,
        plan.chamberZ - 10, plan.destinationZ - 6, poolZ - 4});
    const int maximumZ = std::max({plan.entrance.anchorZ,
        plan.chamberZ + 10, plan.destinationZ + 6, poolZ + 4});

    std::vector<glm::ivec2> locations;
    for (int chunkX = WorldCoordinates::floorDiv(minimumX, CHUNK_SIZE);
         chunkX <= WorldCoordinates::floorDiv(maximumX, CHUNK_SIZE);
         ++chunkX) {
        for (int chunkZ = WorldCoordinates::floorDiv(minimumZ, CHUNK_SIZE);
             chunkZ <= WorldCoordinates::floorDiv(maximumZ, CHUNK_SIZE);
             ++chunkZ) {
            locations.emplace_back(chunkX, chunkZ);
        }
    }

    const glm::ivec2 origin{
        WorldCoordinates::floorDiv(plan.entrance.anchorX, CHUNK_SIZE),
        WorldCoordinates::floorDiv(plan.entrance.anchorZ, CHUNK_SIZE)};
    std::sort(locations.begin(), locations.end(), [&origin](
        const glm::ivec2 &left, const glm::ivec2 &right) {
        const auto key = [&origin](const glm::ivec2 &value) {
            const int dx = std::abs(value.x - origin.x);
            const int dz = std::abs(value.y - origin.y);
            return std::make_tuple(std::max(dx, dz), dx + dz,
                                   value.x, value.y);
        };
        return key(left) < key(right);
    });
    if (locations.empty() || locations.size() > 64u ||
        std::adjacent_find(locations.begin(), locations.end()) !=
            locations.end()) {
        throw std::runtime_error("Invalid B7 affected chunk set");
    }
    return locations;
}

std::vector<B7UndergroundGeneratedChunk> b7UndergroundGenerateChunks(
    World &world, int seed, int version,
    const std::vector<glm::ivec2> &locations)
{
    ClassicOverWorldGenerator generator(seed, version);
    std::vector<B7UndergroundGeneratedChunk> generated;
    generated.reserve(locations.size());
    for (const glm::ivec2 &location : locations) {
        auto chunk = std::make_unique<Chunk>(world, location, false);
        generator.generateTerrainFor(*chunk);
        generated.push_back({location, std::move(chunk)});
    }
    return generated;
}

bool b7UndergroundWriteWorldIdentity(
    const std::filesystem::path &directory, int seed, int version,
    const std::string &suffix)
{
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        return false;
    }
    WorldSaveData data;
    data.worldId = "b7-underground-perf-" + suffix;
    data.worldName = data.worldId;
    data.seed = seed;
    data.createdUtc = LegacyWorldTimestampUtc;
    data.lastPlayedUtc = LegacyWorldTimestampUtc;
    data.lastBuildIdentity = "b7-underground-performance";
    data.terrainGenerationVersion = version;
    data.hasPlayerState = false;
    return WorldSave(directory.string()).save(data);
}

void b7UndergroundAddMesh(
    const ChunkMesh &mesh, TerrainBufferMetrics &buffers,
    std::size_t &vertices, std::size_t &indices)
{
    const Mesh &client = mesh.getClientMesh();
    const std::size_t meshVertices = client.vertexPositions.size() / 3;
    if (client.vertexPositions.size() % 3 != 0 ||
        client.textureCoords.size() != meshVertices * 2 ||
        client.textureRepeatCoords.size() != meshVertices * 2 ||
        mesh.getLight().size() != meshVertices) {
        throw std::runtime_error("B7 mesh payload is internally inconsistent");
    }
    vertices += meshVertices;
    indices += client.indices.size();
    buffers.add(meshVertices, client.indices.size());
}

B7UndergroundMeshMetrics b7UndergroundCollectMeshMetrics(
    const std::filesystem::path &temporaryRoot, int seed, int version,
    const std::vector<glm::ivec2> &locations)
{
    const std::filesystem::path worldDirectory = temporaryRoot /
        ("world-" + std::to_string(seed) + "-v" + std::to_string(version));
    if (!b7UndergroundWriteWorldIdentity(
            worldDirectory, seed, version,
            std::to_string(seed) + "-v" + std::to_string(version))) {
        throw std::runtime_error("Unable to initialize B7 geometry world");
    }

    B7UndergroundMeshMetrics result;
    result.targetChunks = locations.size();
    {
        b7UndergroundSetEnvironment("HELLOMINE3D_SEED", std::to_string(seed));
        b7UndergroundSetEnvironment("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
        Config config;
        config.renderDistance = 2;
        Camera camera(config);
        Player player;
        World world(camera, config, player, worldDirectory.string(), false, 0);
        ChunkManager &manager = world.getChunkManager();
        if (manager.getTerrainGenerationVersion() != version ||
            manager.getTerrainSeed() != seed) {
            throw std::runtime_error("B7 geometry world identity mismatch");
        }

        std::set<std::pair<int, int>> halo;
        for (const glm::ivec2 &location : locations) {
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dz = -1; dz <= 1; ++dz) {
                    halo.emplace(location.x + dx, location.y + dz);
                }
            }
        }
        result.haloChunks = halo.size();
        for (const auto &location : halo) {
            manager.loadChunk(location.first, location.second);
        }

        TerrainBufferMetrics buffers;
        for (const glm::ivec2 &location : locations) {
            Chunk *chunk = manager.findChunk(location.x, location.y);
            if (chunk == nullptr || !chunk->hasLoaded()) {
                throw std::runtime_error("B7 geometry target chunk did not load");
            }
            result.sections += chunk->getSectionCount();
            for (std::size_t sectionIndex = 0;
                 sectionIndex < chunk->getSectionCount(); ++sectionIndex) {
                ChunkSection *section = chunk->findSection(
                    static_cast<int>(sectionIndex));
                if (section == nullptr) {
                    throw std::runtime_error("B7 geometry section disappeared");
                }
                SectionMeshInput input;
                section->captureMeshInput(input);
                ChunkMeshCollection meshes;
                ChunkMeshBuilder(input, meshes).buildMesh();
                result.solidFaces += static_cast<std::size_t>(
                    meshes.solidMesh.faces);
                result.transparentFaces += static_cast<std::size_t>(
                    meshes.transparentMesh.faces);
                result.waterFaces += static_cast<std::size_t>(
                    meshes.waterMesh.faces);
                result.floraFaces += static_cast<std::size_t>(
                    meshes.floraMesh.faces);
                const std::size_t indicesBefore = result.indices;
                b7UndergroundAddMesh(
                    meshes.solidMesh, buffers, result.vertices, result.indices);
                b7UndergroundAddMesh(
                    meshes.transparentMesh, buffers,
                    result.vertices, result.indices);
                b7UndergroundAddMesh(
                    meshes.waterMesh, buffers, result.vertices, result.indices);
                b7UndergroundAddMesh(
                    meshes.floraMesh, buffers, result.vertices, result.indices);
                if (result.indices != indicesBefore) {
                    ++result.emittingSections;
                }
            }
        }
        result.renderables = buffers.renderableCount;
        result.bufferBytes = buffers.totalBytes();
        if (result.bufferBytes !=
            result.vertices * TerrainBufferMetrics::VertexStrideBytes +
                result.indices * TerrainBufferMetrics::IndexStrideBytes) {
            throw std::runtime_error("B7 terrain buffer accounting mismatch");
        }
    }
    std::error_code removeError;
    std::filesystem::remove_all(worldDirectory, removeError);
    if (removeError) {
        throw std::runtime_error("Unable to remove temporary B7 geometry world");
    }
    return result;
}

B7UndergroundGeometryMetrics b7UndergroundCollectGeometry(
    World &sampleWorld, const std::filesystem::path &temporaryRoot,
    const B7UndergroundPerformanceFixture &fixture,
    const std::vector<glm::ivec2> &locations,
    std::ofstream &chunkOutput)
{
    auto before = b7UndergroundGenerateChunks(
        sampleWorld, fixture.seed,
        LandmarkWorkshopTerrainGenerationVersion, locations);
    auto after = b7UndergroundGenerateChunks(
        sampleWorld, fixture.seed,
        AdventureUndergroundTerrainGenerationVersion, locations);
    if (before.size() != after.size()) {
        throw std::runtime_error("B7 geometry versions produced different chunk sets");
    }

    B7UndergroundGeometryMetrics result;
    std::set<std::tuple<int, int, int>> changedSections;
    for (std::size_t chunkIndex = 0; chunkIndex < before.size(); ++chunkIndex) {
        if (before[chunkIndex].location != after[chunkIndex].location) {
            throw std::runtime_error("B7 geometry chunk order changed");
        }
        const Chunk &oldChunk = *before[chunkIndex].chunk;
        const Chunk &newChunk = *after[chunkIndex].chunk;
        const int height = static_cast<int>(std::max(
            oldChunk.getSectionCount(), newChunk.getSectionCount()) *
            CHUNK_SIZE);
        std::size_t changedInChunk = 0;
        std::size_t airInChunk = 0;
        std::size_t waterInChunk = 0;
        std::size_t structureInChunk = 0;
        for (int y = 0; y < height; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    const ChunkBlock oldBlock = oldChunk.getBlock(x, y, z);
                    const ChunkBlock newBlock = newChunk.getBlock(x, y, z);
                    if (oldBlock == newBlock) {
                        continue;
                    }
                    ++changedInChunk;
                    ++result.changedBlocks;
                    changedSections.emplace(
                        before[chunkIndex].location.x, y / CHUNK_SIZE,
                        before[chunkIndex].location.y);
                    const BlockId id = static_cast<BlockId>(newBlock.id);
                    if (id == BlockId::Air) {
                        ++airInChunk;
                        ++result.changedToAir;
                    }
                    else if (id == BlockId::Water) {
                        ++waterInChunk;
                        ++result.changedToWater;
                    }
                    else {
                        ++structureInChunk;
                        ++result.changedToStructure;
                    }
                }
            }
        }
        if (changedInChunk > 0) {
            ++result.changedChunks;
        }
        result.blockEntityDelta += static_cast<long long>(
            newChunk.getBlockEntities().size()) - static_cast<long long>(
            oldChunk.getBlockEntities().size());
        chunkOutput << fixture.seed << ',' << chunkIndex << ','
                    << before[chunkIndex].location.x << ','
                    << before[chunkIndex].location.y << ','
                    << changedInChunk << ',' << airInChunk << ','
                    << waterInChunk << ',' << structureInChunk << ','
                    << TerrainSurvey::blockHash(oldChunk) << ','
                    << TerrainSurvey::blockHash(newChunk) << '\n';
    }
    result.changedSections = changedSections.size();
    if (result.changedBlocks != result.changedToAir +
            result.changedToWater + result.changedToStructure) {
        throw std::runtime_error("B7 changed block categories do not sum");
    }
    result.v22 = b7UndergroundCollectMeshMetrics(
        temporaryRoot, fixture.seed,
        LandmarkWorkshopTerrainGenerationVersion, locations);
    result.v23 = b7UndergroundCollectMeshMetrics(
        temporaryRoot, fixture.seed,
        AdventureUndergroundTerrainGenerationVersion, locations);
    return result;
}

void caseAdventureUndergroundGenerationPerformance()
{
    const std::filesystem::path outputDirectory =
        b7UndergroundCreateFreshDirectory(
            "HELLOMINE3D_UNDERGROUND_PERF_DIR");
    const int version = b7UndergroundParseInteger(
        "HELLOMINE3D_UNDERGROUND_PERF_VERSION",
        LandmarkWorkshopTerrainGenerationVersion,
        AdventureUndergroundTerrainGenerationVersion);
    if (version != LandmarkWorkshopTerrainGenerationVersion &&
        version != AdventureUndergroundTerrainGenerationVersion) {
        throw std::runtime_error("B7 performance version must be v22 or v23");
    }
    const int seed = b7UndergroundParseInteger(
        "HELLOMINE3D_UNDERGROUND_PERF_SEED",
        std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    const int round = b7UndergroundParseInteger(
        "HELLOMINE3D_UNDERGROUND_PERF_ROUND", 1, 3);
    const std::string configuration = b7UndergroundConfiguration();
    const std::string executableHash = b7UndergroundExecutableHash();
    const auto &fixture = b7UndergroundFixtureForSeed(seed);
    const auto locations = b7UndergroundFrozenAffectedLocations(fixture.seed);

    b7UndergroundSetEnvironment("HELLOMINE3D_SEED", "0");
    b7UndergroundSetEnvironment("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    Config config;
    config.renderDistance = 2;
    Camera camera(config);
    Player player;
    const std::filesystem::path worldDirectory =
        outputDirectory / "fixture-world";

    std::vector<B7UndergroundTimedChunk> samples;
    samples.reserve(locations.size());
    std::uint64_t generationSumNanoseconds = 0;
    std::uint64_t regionWallNanoseconds = 0;
    std::uint64_t peakBefore = 0;
    std::uint64_t peakAfter = 0;
    const std::int64_t startedUnixNanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    {
        World world(camera, config, player, worldDirectory.string(), false, 0);
        {
            ClassicOverWorldGenerator warmGenerator(seed, version);
            Chunk warmChunk(world, {0, 0}, false);
            warmGenerator.generateTerrainFor(warmChunk);
        }
        peakBefore = b7UndergroundPeakRssBytes();

        ClassicOverWorldGenerator generator(seed, version);
        std::vector<std::unique_ptr<Chunk>> retained;
        retained.reserve(locations.size());
        const auto regionStart = std::chrono::steady_clock::now();
        for (const glm::ivec2 &location : locations) {
            auto chunk = std::make_unique<Chunk>(world, location, false);
            const auto started = std::chrono::steady_clock::now();
            generator.generateTerrainFor(*chunk);
            const auto finished = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<
                std::chrono::nanoseconds>(finished - started).count();
            if (elapsed <= 0) {
                throw std::runtime_error("B7 generation timer did not advance");
            }
            const std::uint64_t elapsedNanoseconds =
                static_cast<std::uint64_t>(elapsed);
            if (generationSumNanoseconds >
                std::numeric_limits<std::uint64_t>::max() -
                    elapsedNanoseconds) {
                throw std::runtime_error("B7 generation timer overflow");
            }
            generationSumNanoseconds += elapsedNanoseconds;
            samples.push_back({location, elapsedNanoseconds, 0,
                               chunk->getSectionCount(),
                               chunk->getBlockEntities().size()});
            retained.push_back(std::move(chunk));
        }
        regionWallNanoseconds = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - regionStart).count());
        peakAfter = b7UndergroundPeakRssBytes();
        for (std::size_t index = 0; index < retained.size(); ++index) {
            samples[index].blockHash = TerrainSurvey::blockHash(
                *retained[index]);
        }
    }
    std::error_code removeError;
    std::filesystem::remove_all(worldDirectory, removeError);
    if (removeError) {
        throw std::runtime_error("Unable to remove B7 timing fixture world");
    }
    const std::int64_t endedUnixNanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    if (samples.size() != locations.size() ||
        regionWallNanoseconds < generationSumNanoseconds ||
        peakAfter < peakBefore || peakAfter == 0 ||
        endedUnixNanoseconds <= startedUnixNanoseconds) {
        throw std::runtime_error("B7 performance evidence is incomplete");
    }

    auto identity = b7UndergroundOpenCsv(outputDirectory / "identity.csv");
    identity << "schema,configuration,seed,terrain_version,round,clock,"
                "peak_rss_unit,executable_sha256,warmup_chunk_x,"
                "warmup_chunk_z\n";
    identity << "1," << b7UndergroundCsvField(configuration) << ','
             << seed << ',' << version << ',' << round
             << ",steady_clock,bytes,"
             << b7UndergroundCsvField(executableHash) << ",0,0\n";
    identity.close();

    auto chunks = b7UndergroundOpenCsv(outputDirectory / "chunks.csv");
    chunks << "seed,terrain_version,round,chunk_order,chunk_x,chunk_z,"
              "generate_ns,block_hash,section_count,block_entity_count\n";
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto &sample = samples[index];
        chunks << seed << ',' << version << ',' << round << ',' << index
               << ',' << sample.location.x << ',' << sample.location.y
               << ',' << sample.elapsedNanoseconds << ',' << sample.blockHash
               << ',' << sample.sections << ',' << sample.blockEntities
               << '\n';
    }
    chunks.close();

    auto run = b7UndergroundOpenCsv(outputDirectory / "run.csv");
    run << "seed,terrain_version,round,chunk_count,generation_sum_ns,"
           "region_wall_ns,peak_rss_before_bytes,peak_rss_after_bytes,"
           "started_unix_ns,ended_unix_ns\n";
    run << seed << ',' << version << ',' << round << ',' << samples.size()
        << ',' << generationSumNanoseconds << ',' << regionWallNanoseconds
        << ',' << peakBefore << ',' << peakAfter << ','
        << startedUnixNanoseconds << ',' << endedUnixNanoseconds << '\n';
    run.close();

    std::cout << "[UNDERGROUND_GENERATION_PERF] status=CAPTURED config="
              << configuration << " seed=" << seed << " version="
              << version << " round=" << round << " chunks="
              << samples.size() << " generation_ns="
              << generationSumNanoseconds << " peak_rss_bytes="
              << peakAfter << '\n';
}

void caseAdventureUndergroundGeometryEvidence()
{
    const std::filesystem::path outputDirectory =
        b7UndergroundCreateFreshDirectory(
            "HELLOMINE3D_UNDERGROUND_GEOMETRY_DIR");
    const std::string configuration = b7UndergroundConfiguration();
    const std::string executableHash = b7UndergroundExecutableHash();
    const std::filesystem::path temporaryRoot = outputDirectory / "temporary";
    std::filesystem::create_directories(temporaryRoot);

    b7UndergroundSetEnvironment("HELLOMINE3D_SEED", "0");
    b7UndergroundSetEnvironment("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    auto chunks = b7UndergroundOpenCsv(
        outputDirectory / "geometry_chunks.csv");
    chunks << "seed,chunk_order,chunk_x,chunk_z,changed_blocks,"
              "changed_to_air,changed_to_water,changed_to_structure,"
              "v22_block_hash,v23_block_hash\n";
    auto geometry = b7UndergroundOpenCsv(outputDirectory / "geometry.csv");
    geometry << "seed,target_chunks,changed_chunks,changed_blocks,"
                "changed_to_air,changed_to_water,changed_to_structure,"
                "changed_sections,block_entity_delta,halo_chunks,"
                "v22_sections,v23_sections,v22_emitting_sections,"
                "v23_emitting_sections,v22_solid_faces,v23_solid_faces,"
                "v22_water_faces,v23_water_faces,v22_transparent_faces,"
                "v23_transparent_faces,v22_flora_faces,v23_flora_faces,"
                "v22_vertices,v23_vertices,v22_indices,v23_indices,"
                "v22_renderables,v23_renderables,v22_buffer_bytes,"
                "v23_buffer_bytes\n";

    {
        Config config;
        config.renderDistance = 2;
        Camera camera(config);
        Player player;
        World sampleWorld(camera, config, player,
            (temporaryRoot / "sample-world").string(), false, 0);

        for (const auto &fixture : B7UndergroundPerformanceFixtures) {
            const auto plan = b7UndergroundFrozenPlan(fixture);
            const auto derivedLocations =
                b7UndergroundAffectedLocations(plan);
            const auto locations =
                b7UndergroundFrozenAffectedLocations(fixture.seed);
            if (derivedLocations != locations) {
                throw std::runtime_error(
                    "Frozen B7 affected chunk order changed");
            }
            const auto result = b7UndergroundCollectGeometry(
                sampleWorld, temporaryRoot, fixture, locations, chunks);
            geometry << fixture.seed << ',' << locations.size() << ','
                     << result.changedChunks << ',' << result.changedBlocks
                     << ',' << result.changedToAir << ','
                     << result.changedToWater << ','
                     << result.changedToStructure << ','
                     << result.changedSections << ','
                     << result.blockEntityDelta << ','
                     << result.v23.haloChunks << ','
                     << result.v22.sections << ',' << result.v23.sections
                     << ',' << result.v22.emittingSections << ','
                     << result.v23.emittingSections << ','
                     << result.v22.solidFaces << ','
                     << result.v23.solidFaces << ','
                     << result.v22.waterFaces << ','
                     << result.v23.waterFaces << ','
                     << result.v22.transparentFaces << ','
                     << result.v23.transparentFaces << ','
                     << result.v22.floraFaces << ','
                     << result.v23.floraFaces << ','
                     << result.v22.vertices << ','
                     << result.v23.vertices << ',' << result.v22.indices
                     << ',' << result.v23.indices << ','
                     << result.v22.renderables << ','
                     << result.v23.renderables << ','
                     << result.v22.bufferBytes << ','
                     << result.v23.bufferBytes << '\n';
        }
    }
    chunks.close();
    geometry.close();

    auto identity = b7UndergroundOpenCsv(outputDirectory / "identity.csv");
    identity << "schema,configuration,baseline_version,candidate_version,"
                "executable_sha256,vertex_stride_bytes,index_stride_bytes\n";
    identity << "1," << b7UndergroundCsvField(configuration) << ','
             << LandmarkWorkshopTerrainGenerationVersion << ','
             << AdventureUndergroundTerrainGenerationVersion << ','
             << b7UndergroundCsvField(executableHash) << ','
             << TerrainBufferMetrics::VertexStrideBytes << ','
             << TerrainBufferMetrics::IndexStrideBytes << '\n';
    identity.close();

    std::error_code removeError;
    std::filesystem::remove_all(temporaryRoot, removeError);
    if (removeError) {
        throw std::runtime_error("Unable to remove B7 geometry temporary data");
    }
    std::cout << "[UNDERGROUND_GEOMETRY] status=CAPTURED config="
              << configuration << " seeds="
              << B7UndergroundPerformanceFixtures.size() << '\n';
}

} // namespace
