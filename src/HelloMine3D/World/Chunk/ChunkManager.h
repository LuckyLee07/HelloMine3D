#ifndef CHUNKMANAGER_H_INCLUDED
#define CHUNKMANAGER_H_INCLUDED

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "../../Maths/Vector2XZ.h"
#include "../Generation/Terrain/TerrainGenerator.h"
#include "../../Gameplay/ExplorationRewards.h"
#include "../Storage/ChunkStorage.h"
#include "Chunk.h"
#include "SectionMeshInput.h"

class World;

using ChunkMap = std::unordered_map<VectorXZ, Chunk>;

struct ChunkDebugStats {
    std::size_t existingChunks = 0;
    std::size_t loadedChunks = 0;
    std::size_t dataAbsentChunks = 0;
    std::size_t dataRequestedChunks = 0;
    std::size_t dataLoadingChunks = 0;
    std::size_t dataGeneratingChunks = 0;
    std::size_t dataResidentChunks = 0;
    std::size_t dataEvictRequestedChunks = 0;
    std::size_t dataSavingChunks = 0;
    std::size_t saveDirtyChunks = 0;
    std::size_t saveTransactions = 0;
    double saveTotalMs = 0.0;
    double saveMaxMs = 0.0;
    std::size_t sections = 0;
    std::size_t meshCleanSections = 0;
    std::size_t meshDirtySections = 0;
    std::size_t meshQueuedSections = 0;
    std::size_t meshBuildingSections = 0;
    std::size_t cpuReadySections = 0;
    std::size_t renderNotResidentSections = 0;
    std::size_t renderUploadPendingSections = 0;
    std::size_t gpuResidentSections = 0;
    // Compatibility alias for existing performance-capture schemas.
    std::size_t gpuBufferedSections = 0;
    std::size_t renderStaleSections = 0;
    std::size_t meshRebuilds = 0;
    double meshBuildTotalMs = 0.0;
    double meshBuildLastMs = 0.0;
    double meshBuildMaxMs = 0.0;
    std::size_t solidFaces = 0;
    std::size_t transparentFaces = 0;
    std::size_t waterFaces = 0;
    std::size_t floraFaces = 0;
    std::size_t solidVertices = 0;
    std::size_t transparentVertices = 0;
    std::size_t waterVertices = 0;
    std::size_t floraVertices = 0;
};

struct ChunkMeshWorkResult {
    bool loadedChunk = false;
    bool meshBuilt = false;
    bool meshSkipped = false;
    bool neighborhoodReady = false;
    int chunksLoaded = 0;
    int meshesBuilt = 0;
};

struct ChunkNeighborhoodWorkResult {
    bool loadedChunk = false;
    bool neighborhoodReady = false;
    int chunksLoaded = 0;
};

struct ChunkLoadJob {
    bool valid = false;
    bool prepared = false;
    bool loadedFromStorage = false;
    bool generated = false;
    VectorXZ chunkPosition{0, 0};
    std::unique_ptr<Chunk> candidate;
};

struct ChunkNeighborhoodLoadJobResult {
    bool neighborhoodReady = false;
    bool jobPrepared = false;
};

/// @brief One section mesh build, split so the expensive part runs off-lock.
///
/// `beginMeshJob()` fills this while the world lock is held, the caller builds
/// the meshes without the lock, then `finishMeshJob()` installs them under it.
/// Fully enclosed sections are completed inside `beginMeshJob()` without
/// producing a valid job.
struct ChunkMeshJob {
    bool valid = false;
    VectorXZ chunkPosition{0, 0};
    int sectionIndex = -1;
    std::uint32_t blockRevision = 0;
    SectionMeshInput input;
};

/// @brief Dynamic chunk manager that affects chunk and block placement.
class ChunkManager {
  public:
    explicit ChunkManager(World &world);
    ChunkManager(World &world, std::string chunkRootDirectory);

    Chunk &getChunk(int x, int z);
    Chunk &getOrCreateChunk(int x, int z);
    Chunk *findChunk(int x, int z);
    const Chunk *findChunk(int x, int z) const;
    ChunkMap &getChunks();

    /// Both halves must be called with the world lock held; the mesh build
    /// between them must not be.
    ChunkNeighborhoodWorkResult prepareChunkNeighborhood(
        int x, int z, int maxChunkLoads);
    ChunkNeighborhoodLoadJobResult beginChunkNeighborhoodLoadJob(
        int x, int z, int maxChunkLoads, ChunkLoadJob &job);
    bool prepareChunkLoadJob(ChunkLoadJob &job);
    bool finishChunkLoadJob(ChunkLoadJob &job);
    bool cancelChunkLoadJob(const ChunkLoadJob &job);
    ChunkMeshWorkResult beginMeshJob(int x, int z, int maxChunkLoads,
                                     int preferredSectionY, ChunkMeshJob &job);
    bool finishMeshJob(const ChunkMeshJob &job, ChunkMeshCollection &built,
                       double buildMilliseconds);
    bool cancelMeshJob(const ChunkMeshJob &job);

    bool chunkLoadedAt(int x, int z) const;
    bool chunkExistsAt(int x, int z) const;
    ChunkDebugStats collectDebugStats() const;
    void recordMeshRebuild(const ChunkMeshCollection &meshes,
                           double buildMilliseconds) noexcept;

    void loadChunk(int x, int z);
    bool unloadChunk(int x, int z);
    bool saveChunk(Chunk &chunk);
    bool saveDirtyChunks();

    void deleteMeshes();

    const TerrainGenerator &getTerrainGenerator() const noexcept;
    int getTerrainSeed() const noexcept;
    int getTerrainGenerationVersion() const noexcept;
    int getExplorationRewardVersion() const noexcept;
    void setTerrainSeed(int seed);
    void setTerrainIdentity(int seed, int generationVersion,
                            int explorationRewardVersion);

  private:
    ChunkMap m_chunks;
    std::unique_ptr<TerrainGenerator> m_terrainGenerator;
    std::mutex m_terrainGeneratorMutex;
    ChunkStorage m_chunkStorage;
    int m_terrainSeed = 0;
    int m_terrainGenerationVersion = CurrentTerrainGenerationVersion;
    int m_explorationRewardVersion =
        ExplorationRewards::CurrentVersion;
    std::size_t m_meshRebuildCount = 0;
    std::size_t m_saveTransactionCount = 0;
    double m_saveTotalMs = 0.0;
    double m_saveMaxMs = 0.0;
    double m_meshBuildTotalMs = 0.0;
    double m_meshBuildLastMs = 0.0;
    double m_meshBuildMaxMs = 0.0;
    std::size_t m_solidFaceCount = 0;
    std::size_t m_transparentFaceCount = 0;
    std::size_t m_waterFaceCount = 0;
    std::size_t m_floraFaceCount = 0;
    std::size_t m_solidVertexCount = 0;
    std::size_t m_transparentVertexCount = 0;
    std::size_t m_waterVertexCount = 0;
    std::size_t m_floraVertexCount = 0;

    World *m_world;
};

#endif // CHUNKMANAGER_H_INCLUDED
