#ifndef CHUNK_H_INCLUDED
#define CHUNK_H_INCLUDED

#include "../../Util/Array2D.h"
#include "../../Util/NonCopyable.h"
#include "../Block/BlockEntity.h"
#include "ChunkSection.h"
#include <cstddef>
#include <vector>

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
    Chunk(World &world, const glm::ivec2 &location);

    /// Index of a section whose mesh needs rebuilding, searched outwards from
    /// `preferredSectionY`, or -1 when everything is up to date.
    int findDirtySection(int preferredSectionY) const;

    void setBlock(int x, int y, int z, ChunkBlock block) override;
    ChunkBlock getBlock(int x, int y, int z) const noexcept override;
    LightLevel getSunlight(int x, int y, int z) const noexcept;
    void rebuildSunlight();
    std::vector<int> rebuildSunlightColumn(int x, int z);
    LightLevel getBlockLight(int x, int y, int z) const noexcept;
    bool setBlockLight(int x, int y, int z, LightLevel level) noexcept;
    void rebuildBlockLight();
    int getHeightAt(int x, int z) const;

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

    const glm::ivec2 &getLocation() const
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
    glm::ivec2 m_location;

    World *m_pWorld;

    ChunkLoadState m_loadState = ChunkLoadState::Empty;
    bool m_saveDirty = false;
};

#endif // CHUNK_H_INCLUDED
