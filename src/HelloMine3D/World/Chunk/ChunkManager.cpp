#include "ChunkManager.h"

#include <iostream>
#include <utility>

#include "../../Sandbox/Events/ChunkEvents.h"
#include "../Generation/Terrain/ClassicOverWorldGenerator.h"
#include "../Generation/Terrain/SuperFlatGenerator.h"
#include "../World.h"

ChunkManager::ChunkManager(World &world)
    : ChunkManager(world, "")
{
}

ChunkManager::ChunkManager(World &world, std::string chunkRootDirectory)
    : m_world(&world)
    , m_chunkStorage(chunkRootDirectory.empty()
                         ? ChunkStorage()
                         : ChunkStorage(std::move(chunkRootDirectory)))
{
    setTerrainSeed(m_terrainSeed);
}

Chunk &ChunkManager::getChunk(int x, int z)
{
    return getOrCreateChunk(x, z);
}

Chunk &ChunkManager::getOrCreateChunk(int x, int z)
{
    VectorXZ key{x, z};
    auto existing = m_chunks.find(key);
    if (existing == m_chunks.end()) {
        Chunk chunk{*m_world, {x, z}};
        existing = m_chunks.emplace(key, std::move(chunk)).first;
    }

    return existing->second;
}

Chunk *ChunkManager::findChunk(int x, int z)
{
    auto found = m_chunks.find({x, z});
    if (found == m_chunks.end()) {
        return nullptr;
    }

    return &found->second;
}

const Chunk *ChunkManager::findChunk(int x, int z) const
{
    auto found = m_chunks.find({x, z});
    if (found == m_chunks.end()) {
        return nullptr;
    }

    return &found->second;
}

ChunkMap &ChunkManager::getChunks()
{
    return m_chunks;
}

bool ChunkManager::makeMesh(int x, int z)
{
    for (int nx = -1; nx <= 1; nx++)
        for (int nz = -1; nz <= 1; nz++) {
            loadChunk(
                x + nx,
                z + nz); // getChunk(x + nx, z + nz).load(*m_terrainGenerator);
        }

    const bool meshBuilt = getOrCreateChunk(x, z).makeMesh();
    if (meshBuilt) {
        recordMeshRebuild();
    }

    return meshBuilt;
}

ChunkMeshWorkResult ChunkManager::processMeshTarget(int x, int z,
                                                    int maxChunkLoads,
                                                    int maxMeshBuilds,
                                                    int preferredSectionY)
{
    ChunkMeshWorkResult result;
    if (maxChunkLoads < 0) {
        maxChunkLoads = 0;
    }
    if (maxMeshBuilds < 0) {
        maxMeshBuilds = 0;
    }

    for (int nx = -1; nx <= 1; nx++) {
        for (int nz = -1; nz <= 1; nz++) {
            const int chunkX = x + nx;
            const int chunkZ = z + nz;
            if (chunkLoadedAt(chunkX, chunkZ)) {
                continue;
            }

            result.neighborhoodReady = false;
            if (result.chunksLoaded >= maxChunkLoads) {
                continue;
            }

            loadChunk(chunkX, chunkZ);
            result.loadedChunk = true;
            ++result.chunksLoaded;
        }
    }

    result.neighborhoodReady = true;
    for (int nx = -1; nx <= 1; nx++) {
        for (int nz = -1; nz <= 1; nz++) {
            if (!chunkLoadedAt(x + nx, z + nz)) {
                result.neighborhoodReady = false;
                return result;
            }
        }
    }

    Chunk *chunk = findChunk(x, z);
    if (chunk == nullptr || !chunk->hasLoaded()) {
        return result;
    }

    result.meshesBuilt = chunk->makeMeshes(maxMeshBuilds, preferredSectionY);
    result.meshBuilt = result.meshesBuilt > 0;
    for (int i = 0; i < result.meshesBuilt; ++i) {
        recordMeshRebuild();
    }

    return result;
}

bool ChunkManager::chunkLoadedAt(int x, int z) const
{
    const Chunk *chunk = findChunk(x, z);
    if (chunk == nullptr)
        return false;

    return chunk->hasLoaded();
}

bool ChunkManager::chunkExistsAt(int x, int z) const
{
    return m_chunks.find({x, z}) != m_chunks.end();
}

ChunkDebugStats ChunkManager::collectDebugStats() const
{
    ChunkDebugStats stats;
    stats.existingChunks = m_chunks.size();

    for (const auto &entry : m_chunks) {
        const auto &chunk = entry.second;
        if (chunk.hasLoaded()) {
            ++stats.loadedChunks;
        }
        if (chunk.needsSave()) {
            ++stats.saveDirtyChunks;
        }

        stats.sections += chunk.getSectionCount();
        stats.meshDirtySections +=
            chunk.countSections(ChunkSectionMeshState::Dirty);
        stats.cpuReadySections +=
            chunk.countSections(ChunkSectionMeshState::CpuReady);
        stats.gpuBufferedSections +=
            chunk.countSections(ChunkSectionMeshState::GpuBuffered);
    }
    stats.meshRebuilds = m_meshRebuildCount;

    return stats;
}

void ChunkManager::recordMeshRebuild() noexcept
{
    ++m_meshRebuildCount;
}

void ChunkManager::loadChunk(int x, int z)
{
    Chunk &chunk = getOrCreateChunk(x, z);
    if (chunk.hasLoaded()) {
        return;
    }

    const VectorXZ chunkPosition{x, z};
    const bool loadedFromStorage = m_chunkStorage.loadChunk(chunk);
    if (!loadedFromStorage) {
        chunk.load(*m_terrainGenerator);
        m_world->getEventBus().publish(ChunkGeneratedEvent(chunkPosition));
    }

    m_world->getEventBus().publish(
        ChunkLoadedEvent(chunkPosition, loadedFromStorage));
}

void ChunkManager::deleteMeshes()
{
    for (auto &chunk : m_chunks) {
        chunk.second.deleteMeshes();
    }
}

const TerrainGenerator &ChunkManager::getTerrainGenerator() const noexcept
{
    return *m_terrainGenerator;
}

int ChunkManager::getTerrainSeed() const noexcept
{
    return m_terrainSeed;
}

void ChunkManager::setTerrainSeed(int seed)
{
    m_terrainSeed = seed;
    m_terrainGenerator = std::make_unique<ClassicOverWorldGenerator>(seed);
}

void ChunkManager::unloadChunk(int x, int z)
{
    Chunk *chunk = findChunk(x, z);
    if (chunk == nullptr) {
        return;
    }

    saveChunk(*chunk);
    m_chunks.erase({x, z});
    m_world->getEventBus().publish(ChunkUnloadedEvent({x, z}));
}

bool ChunkManager::saveChunk(Chunk &chunk)
{
    if (!chunk.needsSave()) {
        return true;
    }

    if (!m_chunkStorage.saveChunk(chunk)) {
        return false;
    }

    chunk.clearSaveDirty();
    const auto &location = chunk.getLocation();
    m_world->getEventBus().publish(
        ChunkSavedEvent({location.x, location.y}));
    return true;
}

void ChunkManager::saveDirtyChunks()
{
    for (auto &entry : m_chunks) {
        saveChunk(entry.second);
    }
}
