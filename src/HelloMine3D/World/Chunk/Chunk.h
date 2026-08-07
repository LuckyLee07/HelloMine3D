#ifndef CHUNK_H_INCLUDED
#define CHUNK_H_INCLUDED

#include "../../Util/Array2D.h"
#include "../../Util/NonCopyable.h"
#include "../Block/BlockEntity.h"
#include "ChunkSection.h"
#include <cstddef>
#include <vector>

class RenderMaster;
class Camera;
class TerrainGenerator;

enum class ChunkLoadState {
    Empty,
    Generating,
    Loaded,
};

/// @brief A chunk, in other words, a large arrangement of blocks.
class Chunk : public IChunk {
  public:
    Chunk() = default;
    Chunk(World &world, const sf::Vector2i &location);

    bool makeMesh();
    int makeMeshes(int maxSections, int preferredSectionY = -1);

    void setBlock(int x, int y, int z, ChunkBlock block) override;
    ChunkBlock getBlock(int x, int y, int z) const noexcept override;
    int getHeightAt(int x, int z);

    void drawChunks(RenderMaster &renderer, const Camera &camera,
                    int &meshBufferBudget);

    bool hasLoaded() const noexcept;
    bool hasGenerated() const noexcept;
    bool needsSave() const noexcept;
    ChunkLoadState getLoadState() const noexcept;
    void clearSaveDirty() noexcept;
    std::size_t getSectionCount() const noexcept;
    std::size_t countSections(ChunkSectionMeshState state) const noexcept;
    void collectBlockData(std::vector<Block_t> &blockIds,
                          std::vector<BlockMetadata_t> &metadata) const;
    void loadBlockData(std::size_t sectionCount,
                       const std::vector<Block_t> &blockIds,
                       const std::vector<BlockMetadata_t> &metadata);
    const std::vector<BlockEntityRecord> &getBlockEntities() const;
    void loadBlockEntities(std::vector<BlockEntityRecord> blockEntities);
    void load(TerrainGenerator &generator);

    ChunkSection &getSection(int index);
    ChunkSection *findSection(int index);
    const ChunkSection *findSection(int index) const;
    bool hasSection(int index) const noexcept;

    const sf::Vector2i &getLocation() const
    {
        return m_location;
    }

    void deleteMeshes();

  private:
    void addSection();
    void addSectionsBlockTarget(int blockY);
    void addSectionsIndexTarget(int index);

    bool outOfBound(int x, int y, int z) const noexcept;

    std::vector<ChunkSection> m_chunks;
    std::vector<BlockEntityRecord> m_blockEntities;
    Array2D<int, CHUNK_SIZE> m_highestBlocks;
    sf::Vector2i m_location;

    World *m_pWorld;

    ChunkLoadState m_loadState = ChunkLoadState::Empty;
    bool m_saveDirty = false;
};

#endif // CHUNK_H_INCLUDED
