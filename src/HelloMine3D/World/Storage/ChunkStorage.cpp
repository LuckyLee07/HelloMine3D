#include "ChunkStorage.h"

#include "ChunkStorageData.h"
#include "../Block/BlockEntity.h"
#include "../Chunk/Chunk.h"
#include "../../Util/ResourcePaths.h"

#include <iostream>
#include <utility>

ChunkStorage::ChunkStorage()
    : ChunkStorage(ResourcePaths::bin("saves/default/chunks"))
{
}

ChunkStorage::ChunkStorage(std::string rootDirectory)
    : m_rootDirectory(std::move(rootDirectory))
{
}

bool ChunkStorage::loadChunk(Chunk &chunk) const
{
    const auto &location = chunk.getLocation();
    StoredChunkData data;
    if (!ChunkStorageData(m_rootDirectory)
             .loadChunkData(location.x, location.y, data)) {
        return false;
    }

    std::string validationError;
    if (!validateBlockEntityRecords(data.blockEntities, data.sectionCount,
                                    &validationError)) {
        std::cerr << "Invalid block entity data in "
                  << chunkPath(location.x, location.y) << ": "
                  << validationError << '\n';
        return false;
    }

    for (const BlockEntityRecord &record : data.blockEntities) {
        const std::size_t index =
            static_cast<std::size_t>(record.position.y) * CHUNK_AREA +
            static_cast<std::size_t>(record.position.z) * CHUNK_SIZE +
            static_cast<std::size_t>(record.position.x);
        if (index >= data.blockIds.size() ||
            data.blockIds[index] == static_cast<Block_t>(BlockId::Air)) {
            std::cerr << "Invalid block entity data in "
                      << chunkPath(location.x, location.y)
                      << ": record is not attached to a block\n";
            return false;
        }
    }

    chunk.loadBlockData(data.sectionCount, data.blockIds, data.metadata);
    if (!chunk.loadBlockEntities(std::move(data.blockEntities))) {
        return false;
    }
    return true;
}

bool ChunkStorage::saveChunk(const Chunk &chunk) const
{
    const auto &location = chunk.getLocation();
    StoredChunkData data;
    data.x = location.x;
    data.z = location.y;
    data.sectionCount = chunk.getSectionCount();
    data.blockEntities = chunk.getBlockEntities();
    chunk.collectBlockData(data.blockIds, data.metadata);

    return ChunkStorageData(m_rootDirectory).saveChunkData(data);
}

std::string ChunkStorage::chunkPath(int x, int z) const
{
    return ChunkStorageData(m_rootDirectory).chunkPath(x, z);
}

bool ChunkStorage::ensureRootDirectory() const
{
    return ChunkStorageData(m_rootDirectory).ensureRootDirectory();
}
