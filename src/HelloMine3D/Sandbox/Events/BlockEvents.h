#ifndef BLOCKEVENTS_H_INCLUDED
#define BLOCKEVENTS_H_INCLUDED

#include "../../Maths/glm.h"
#include "../../World/Block/BlockId.h"
#include "SandboxEventBus.h"

struct BlockBreakEvent : public SandboxEvent {
    BlockBreakEvent(const glm::ivec3 &blockPosition, BlockId brokenBlock)
        : SandboxEvent(SandboxEventType::BlockBreak)
        , position(blockPosition)
        , blockId(brokenBlock)
    {
    }

    glm::ivec3 position{0};
    BlockId blockId = BlockId::Air;
};

struct BlockPlaceEvent : public SandboxEvent {
    BlockPlaceEvent(const glm::ivec3 &blockPosition, BlockId placedBlock)
        : SandboxEvent(SandboxEventType::BlockPlace)
        , position(blockPosition)
        , blockId(placedBlock)
    {
    }

    glm::ivec3 position{0};
    BlockId blockId = BlockId::Air;
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
