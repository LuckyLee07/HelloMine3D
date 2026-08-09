#ifndef CHUNKMANAGER_H_INCLUDED
#define CHUNKMANAGER_H_INCLUDED

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "../../Maths/Vector2XZ.h"
#include "../Generation/Terrain/TerrainGenerator.h"
#include "../Storage/ChunkStorage.h"
#include "Chunk.h"
#include "SectionMeshInput.h"

class World;

using ChunkMap = std::unordered_map<VectorXZ, Chunk>;

struct ChunkDebugStats {
    std::size_t existingChunks = 0;
    std::size_t loadedChunks = 0;
    std::size_t saveDirtyChunks = 0;
    std::size_t sections = 0;
    std::size_t meshDirtySections = 0;
    std::size_t cpuReadySections = 0;
    std::size_t gpuBufferedSections = 0;
    std::size_t meshRebuilds = 0;
    double meshBuildTotalMs = 0.0;
    double meshBuildLastMs = 0.0;
    double meshBuildMaxMs = 0.0;
    std::size_t solidFaces = 0;
    std::size_t waterFaces = 0;
    std::size_t floraFaces = 0;
    std::size_t solidVertices = 0;
    std::size_t waterVertices = 0;
    std::size_t floraVertices = 0;
};

struct ChunkMeshWorkResult {
    bool loadedChunk = false;
    bool meshBuilt = false;
    bool neighborhoodReady = false;
    int chunksLoaded = 0;
    int meshesBuilt = 0;
};

/// @brief One section mesh build, split so the expensive part runs off-lock.
///
/// `beginMeshJob()` fills this while the world lock is held, the caller builds
/// the meshes without the lock, then `finishMeshJob()` installs them under it.
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
    ChunkMeshWorkResult beginMeshJob(int x, int z, int maxChunkLoads,
                                     int preferredSectionY, ChunkMeshJob &job);
    bool finishMeshJob(const ChunkMeshJob &job, ChunkMeshCollection &built,
                       double buildMilliseconds);

    bool chunkLoadedAt(int x, int z) const;
    bool chunkExistsAt(int x, int z) const;
    ChunkDebugStats collectDebugStats() const;
    void recordMeshRebuild(const ChunkMeshCollection &meshes,
                           double buildMilliseconds) noexcept;

    void loadChunk(int x, int z);
    void unloadChunk(int x, int z);
    bool saveChunk(Chunk &chunk);
    void saveDirtyChunks();

    void deleteMeshes();

    const TerrainGenerator &getTerrainGenerator() const noexcept;
    int getTerrainSeed() const noexcept;
    void setTerrainSeed(int seed);

  private:
    ChunkMap m_chunks;
    std::unique_ptr<TerrainGenerator> m_terrainGenerator;
    ChunkStorage m_chunkStorage;
    int m_terrainSeed = 0;
    std::size_t m_meshRebuildCount = 0;
    double m_meshBuildTotalMs = 0.0;
    double m_meshBuildLastMs = 0.0;
    double m_meshBuildMaxMs = 0.0;
    std::size_t m_solidFaceCount = 0;
    std::size_t m_waterFaceCount = 0;
    std::size_t m_floraFaceCount = 0;
    std::size_t m_solidVertexCount = 0;
    std::size_t m_waterVertexCount = 0;
    std::size_t m_floraVertexCount = 0;

    World *m_world;
};

#endif // CHUNKMANAGER_H_INCLUDED
