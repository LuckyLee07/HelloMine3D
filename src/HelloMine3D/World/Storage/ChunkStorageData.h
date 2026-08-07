#ifndef CHUNKSTORAGEDATA_H_INCLUDED
#define CHUNKSTORAGEDATA_H_INCLUDED

#include "../Block/BlockEntity.h"
#include "../Block/BlockId.h"

#include <cstddef>
#include <string>
#include <vector>

struct StoredChunkData {
    int x = 0;
    int z = 0;
    std::size_t sectionCount = 0;
    std::vector<Block_t> blockIds;
    std::vector<BlockMetadata_t> metadata;
    std::vector<BlockEntityRecord> blockEntities;
};

class ChunkStorageData {
  public:
    explicit ChunkStorageData(std::string rootDirectory);

    bool loadChunkData(int x, int z, StoredChunkData &data) const;
    bool saveChunkData(const StoredChunkData &data) const;

    std::string chunkPath(int x, int z) const;
    bool ensureRootDirectory() const;

  private:
    std::string m_rootDirectory;
};

#endif // CHUNKSTORAGEDATA_H_INCLUDED
