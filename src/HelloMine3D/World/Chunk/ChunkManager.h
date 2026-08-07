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
};

struct ChunkMeshWorkResult {
    bool loadedChunk = false;
    bool meshBuilt = false;
    bool neighborhoodReady = false;
    int chunksLoaded = 0;
    int meshesBuilt = 0;
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

    bool makeMesh(int x, int z);
    ChunkMeshWorkResult processMeshTarget(int x, int z, int maxChunkLoads,
                                          int maxMeshBuilds,
                                          int preferredSectionY);

    bool chunkLoadedAt(int x, int z) const;
    bool chunkExistsAt(int x, int z) const;
    ChunkDebugStats collectDebugStats() const;
    void recordMeshRebuild() noexcept;

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

    World *m_world;
};

#endif // CHUNKMANAGER_H_INCLUDED
