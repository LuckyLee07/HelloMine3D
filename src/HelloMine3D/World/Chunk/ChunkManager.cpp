#include "ChunkManager.h"

#include <algorithm>
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

ChunkMeshWorkResult ChunkManager::beginMeshJob(int x, int z, int maxChunkLoads,
                                               int preferredSectionY,
                                               ChunkMeshJob &job)
{
    job.valid = false;

    ChunkMeshWorkResult result;
    if (maxChunkLoads < 0) {
        maxChunkLoads = 0;
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

    const int sectionIndex = chunk->findDirtySection(preferredSectionY);
    if (sectionIndex < 0) {
        return result;
    }

    ChunkSection *section = chunk->findSection(sectionIndex);
    if (section == nullptr) {
        return result;
    }

    job.chunkPosition = {x, z};
    job.sectionIndex = sectionIndex;
    job.blockRevision = section->getBlockRevision();
    section->captureMeshInput(job.input);

    if (!job.input.needsMeshBuild()) {
        ChunkMeshCollection emptyMeshes;
        section->adoptMesh(emptyMeshes);
        result.meshSkipped = true;
        return result;
    }

    job.valid = true;

    result.meshBuilt = true;
    result.meshesBuilt = 1;
    return result;
}

bool ChunkManager::finishMeshJob(const ChunkMeshJob &job,
                                 ChunkMeshCollection &built,
                                 double buildMilliseconds)
{
    if (!job.valid) {
        return false;
    }

    Chunk *chunk = findChunk(job.chunkPosition.x, job.chunkPosition.z);
    if (chunk == nullptr) {
        // The chunk was unloaded while the mesh was being built.
        return false;
    }

    ChunkSection *section = chunk->findSection(job.sectionIndex);
    if (section == nullptr) {
        return false;
    }

    if (section->getBlockRevision() != job.blockRevision ||
        !section->isMeshDirty()) {
        // A block edit or a synchronous rebuild landed while we were building,
        // so this result is stale. The section stays dirty and gets picked up
        // again on a later pass.
        return false;
    }

    section->adoptMesh(built);
    recordMeshRebuild(section->getMeshes(), buildMilliseconds);
    return true;
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
    stats.meshBuildTotalMs = m_meshBuildTotalMs;
    stats.meshBuildLastMs = m_meshBuildLastMs;
    stats.meshBuildMaxMs = m_meshBuildMaxMs;
    stats.solidFaces = m_solidFaceCount;
    stats.transparentFaces = m_transparentFaceCount;
    stats.waterFaces = m_waterFaceCount;
    stats.floraFaces = m_floraFaceCount;
    stats.solidVertices = m_solidVertexCount;
    stats.transparentVertices = m_transparentVertexCount;
    stats.waterVertices = m_waterVertexCount;
    stats.floraVertices = m_floraVertexCount;

    return stats;
}

void ChunkManager::recordMeshRebuild(const ChunkMeshCollection &meshes,
                                     double buildMilliseconds) noexcept
{
    ++m_meshRebuildCount;
    const double elapsed = std::max(0.0, buildMilliseconds);
    m_meshBuildTotalMs += elapsed;
    m_meshBuildLastMs = elapsed;
    m_meshBuildMaxMs = std::max(m_meshBuildMaxMs, elapsed);

    const auto vertexCount = [](const ChunkMesh &mesh) {
        return mesh.getClientMesh().vertexPositions.size() / 3;
    };
    m_solidFaceCount += static_cast<std::size_t>(meshes.solidMesh.faces);
    m_transparentFaceCount +=
        static_cast<std::size_t>(meshes.transparentMesh.faces);
    m_waterFaceCount += static_cast<std::size_t>(meshes.waterMesh.faces);
    m_floraFaceCount += static_cast<std::size_t>(meshes.floraMesh.faces);
    m_solidVertexCount += vertexCount(meshes.solidMesh);
    m_transparentVertexCount += vertexCount(meshes.transparentMesh);
    m_waterVertexCount += vertexCount(meshes.waterMesh);
    m_floraVertexCount += vertexCount(meshes.floraMesh);
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

    m_world->reconcileBlockLightAfterChunkLoad(x, z);

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

    const int height =
        static_cast<int>(chunk->getSectionCount()) * CHUNK_SIZE;
    m_world->despawnNaturalMobsInChunk(x, z);
    saveChunk(*chunk);
    m_world->removeRandomTickSectionsForChunk(x, z);
    m_chunks.erase({x, z});
    m_world->reconcileBlockLightAfterChunkUnload(x, z, height);
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
