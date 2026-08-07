#include "ChunkBlock.h"

#include "BlockDatabase.h"

ChunkBlock::ChunkBlock(Block_t id)
    : id(id)
{
}

ChunkBlock::ChunkBlock(Block_t id, BlockMetadata_t metadata)
    : id(id)
    , metadata(metadata)
{
}

ChunkBlock::ChunkBlock(BlockId id)
    : id(static_cast<Block_t>(id))
{
}

ChunkBlock::ChunkBlock(BlockId id, BlockMetadata_t metadata)
    : id(static_cast<Block_t>(id))
    , metadata(metadata)
{
}

const BlockDataHolder &ChunkBlock::getData() const
{
    return BlockDatabase::get().getData((BlockId)id).getBlockData();
}

const BlockType &ChunkBlock::getType() const
{
    return BlockDatabase::get().getBlock((BlockId)id);
}
