#ifndef HELLOMINE3D_TERRAIN_SURVEY_H
#define HELLOMINE3D_TERRAIN_SURVEY_H

// Offline evidence over production generation. Never called by the client.
#include "../World/Generation/Terrain/ClassicOverWorldGenerator.h"
#include "../World/Chunk/Chunk.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
} // namespace TerrainSurvey

#endif
