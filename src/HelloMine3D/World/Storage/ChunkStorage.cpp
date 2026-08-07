#include "ChunkStorage.h"

#include "ChunkStorageData.h"
#include "../Chunk/Chunk.h"
#include "../../Util/ResourcePaths.h"

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

    chunk.loadBlockData(data.sectionCount, data.blockIds, data.metadata);
    chunk.loadBlockEntities(std::move(data.blockEntities));
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
