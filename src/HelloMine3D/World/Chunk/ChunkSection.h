#ifndef CHUNKSECTION_H_INCLUDED
#define CHUNKSECTION_H_INCLUDED

#include "../../Maths/glm.h"
#include <array>
#include <cstdint>

#include "../Block/ChunkBlock.h"
#include "../WorldConstants.h"
#include "ChunkMesh.h"
#include "IChunk.h"

#include "../../Physics/AABB.h"
#include "../Block/BlockData.h"

class World;
class SectionMeshInput;

enum class ChunkSectionMeshState {
    Dirty,
    CpuReady,
    GpuBuffered,
};

class ChunkSection : public IChunk {
    friend class Chunk;

    class Layer {
      public:
        void update(ChunkBlock previous, ChunkBlock current)
        {
            const bool wasSolid = previous.getData().isOpaque;
            const bool isSolid = current.getData().isOpaque;
            if (wasSolid == isSolid) {
                return;
            }

            m_solidBlockCount += isSolid ? 1 : -1;
        }

        bool isAllSolid() const
        {
            return m_solidBlockCount == CHUNK_AREA;
        }

      private:
        int m_solidBlockCount = 0;
    };

  public:
    ChunkSection(const glm::ivec3 &position, World &world);

    void setBlock(int x, int y, int z, ChunkBlock block) override;
    ChunkBlock getBlock(int x, int y, int z) const override;

    glm::ivec3 getLocation() const;

    bool hasMesh() const;
    bool hasBuffered() const;
    bool isMeshDirty() const;
    ChunkSectionMeshState getMeshState() const;
    void markMeshDirty();

    /// Builds a mesh when the section has visible layers. Returns false when
    /// a fully enclosed section was completed without running the builder.
    bool makeMesh();
    void markGpuBuffered();

    /// Snapshot the data a mesh build reads. Must be called under the world
    /// lock; the returned input can then be built without it.
    void captureMeshInput(SectionMeshInput &input);

    /// Install a mesh built off the world lock. Must be called under it.
    void adoptMesh(ChunkMeshCollection &built);

    /// Bumped on every block change. A mesh built off-lock is only installed
    /// when this still matches the value captured with the input, otherwise
    /// the build raced an edit and its result is stale.
    std::uint32_t getBlockRevision() const;

    const Layer &getLayer(int y) const;
    ChunkSection &getAdjacent(int dx, int dz);

    const ChunkMeshCollection &getMeshes() const
    {
        return m_meshes;
    }

    void deleteMeshes();

    const ChunkBlock *begin()
    {
        return &m_blocks[0];
    }

  private:
    glm::ivec3 toWorldPosition(int x, int y, int z) const;

    static bool outOfBounds(int value);
    static int getIndex(int x, int y, int z);

    std::array<ChunkBlock, CHUNK_VOLUME> m_blocks;
    std::array<Layer, CHUNK_SIZE> m_layers;

    ChunkMeshCollection m_meshes;
    AABB m_aabb;
    glm::ivec3 m_location;

    World *m_pWorld;

    ChunkSectionMeshState m_meshState = ChunkSectionMeshState::Dirty;
    std::uint32_t m_blockRevision = 0;
};

#endif // CHUNKSECTION_H_INCLUDED
