#ifndef BLOCKEVENTS_H_INCLUDED
#define BLOCKEVENTS_H_INCLUDED

#include "../../Maths/glm.h"
#include "../../World/Block/BlockId.h"
#include "SandboxEventBus.h"

struct BlockBreakEvent : public SandboxEvent {
    BlockBreakEvent(const glm::ivec3 &blockPosition, BlockId brokenBlock, BlockMetadata_t data = 0)
        : SandboxEvent(SandboxEventType::BlockBreak)
        , position(blockPosition)
        , blockId(brokenBlock)
        , metadata(data)
    {
    }

    glm::ivec3 position{0};
    BlockId blockId = BlockId::Air;
    BlockMetadata_t metadata = 0;
};

struct BlockPlaceEvent : public SandboxEvent {
    BlockPlaceEvent(const glm::ivec3 &blockPosition, BlockId placedBlock, BlockMetadata_t data = 0)
        : SandboxEvent(SandboxEventType::BlockPlace)
        , position(blockPosition)
        , blockId(placedBlock)
        , metadata(data)
    {
    }

    glm::ivec3 position{0};
    BlockId blockId = BlockId::Air;
    BlockMetadata_t metadata = 0;
};

struct BlockUseEvent : public SandboxEvent {
    BlockUseEvent(const glm::ivec3 &blockPosition, BlockId usedBlock)
        : SandboxEvent(SandboxEventType::BlockUse)
        , position(blockPosition)
        , blockId(usedBlock)
    {
    }

    glm::ivec3 position{0};
    BlockId blockId = BlockId::Air;
};

struct BlockChangedEvent : public SandboxEvent {
    BlockChangedEvent(const glm::ivec3 &blockPosition, BlockId oldBlock,
                      BlockId newBlock)
        : SandboxEvent(SandboxEventType::BlockChanged)
        , position(blockPosition)
        , previousBlockId(oldBlock)
        , currentBlockId(newBlock)
    {
    }

    glm::ivec3 position{0};
    BlockId previousBlockId = BlockId::Air;
    BlockId currentBlockId = BlockId::Air;
};

#endif // BLOCKEVENTS_H_INCLUDED
