#ifndef CHUNKBLOCK_H_INCLUDED
#define CHUNKBLOCK_H_INCLUDED

#include "BlockId.h"

struct BlockDataHolder;
class BlockType;

namespace BlockMetadata {
namespace TallGrass {
constexpr BlockMetadata_t Immature = 0;
constexpr BlockMetadata_t Mature = 1;
} // namespace TallGrass
} // namespace BlockMetadata

struct ChunkBlock {
    ChunkBlock() = default;

    ChunkBlock(Block_t id);
    ChunkBlock(Block_t id, BlockMetadata_t metadata);
    ChunkBlock(BlockId id);
    ChunkBlock(BlockId id, BlockMetadata_t metadata);

    const BlockDataHolder &getData() const;
    const BlockType &getType() const;

    bool operator==(ChunkBlock other) const
    {
        return id == other.id && metadata == other.metadata;
    }

    bool operator!=(ChunkBlock other) const
    {
        return !(*this == other);
    }

    Block_t id = 0;
    BlockMetadata_t metadata = 0;
};

#endif // CHUNKBLOCK_H_INCLUDED
