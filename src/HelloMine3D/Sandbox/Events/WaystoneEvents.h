#pragma once

#include <cstdint>

#include "SandboxEventBus.h"
#include "../../Actor/ActorTypes.h"
#include "../../Maths/glm.h"

struct WaystoneActivatedEvent : public SandboxEvent
{
    WaystoneActivatedEvent(ActorId activatingPlayerId,
                           const glm::ivec3& corePosition)
        : SandboxEvent(SandboxEventType::WaystoneActivated)
        , playerId(activatingPlayerId)
        , position(corePosition)
    {
    }

    ActorId playerId = DefaultPlayerActorId;
    glm::ivec3 position{0};
};

struct VictoryRewardClaimedEvent : public SandboxEvent
{
    VictoryRewardClaimedEvent(ActorId claimingPlayerId,
                              std::uint32_t claimedEpoch)
        : SandboxEvent(SandboxEventType::VictoryRewardClaimed)
        , playerId(claimingPlayerId)
        , rewardEpoch(claimedEpoch)
    {
    }

    ActorId playerId = DefaultPlayerActorId;
    std::uint32_t rewardEpoch = 0;
};
