#ifndef PLAYEREVENTS_H_INCLUDED
#define PLAYEREVENTS_H_INCLUDED

#include "../../Actor/ActorTypes.h"
#include "../../Item/Material.h"
#include "../../Maths/glm.h"
#include "SandboxEventBus.h"

#include <string>
#include <utility>

struct PlayerSpawnEvent : public SandboxEvent {
    PlayerSpawnEvent(ActorId playerActorId, int spawnedWorldId,
                     const glm::vec3 &spawnPosition)
        : SandboxEvent(SandboxEventType::PlayerSpawn)
        , playerId(playerActorId)
        , worldId(spawnedWorldId)
        , position(spawnPosition)
    {
    }

    ActorId playerId = DefaultPlayerActorId;
    int worldId = 0;
    glm::vec3 position{0.f};
};

struct PlayerTeleportEvent : public SandboxEvent {
    PlayerTeleportEvent(ActorId playerActorId, int sourceWorldId,
                        int destinationWorldId,
                        const glm::vec3 &sourcePosition,
                        const glm::vec3 &destinationPosition)
        : SandboxEvent(SandboxEventType::PlayerTeleport)
        , playerId(playerActorId)
        , fromWorldId(sourceWorldId)
        , toWorldId(destinationWorldId)
        , fromPosition(sourcePosition)
        , toPosition(destinationPosition)
    {
    }

    ActorId playerId = DefaultPlayerActorId;
    int fromWorldId = 0;
    int toWorldId = 0;
    glm::vec3 fromPosition{0.f};
    glm::vec3 toPosition{0.f};
};

struct PlayerInventoryChangedEvent : public SandboxEvent {
    PlayerInventoryChangedEvent(ActorId playerActorId,
                                Material::ID changedMaterialId,
                                int changedAmount, std::string changeReason)
        : SandboxEvent(SandboxEventType::PlayerInventoryChanged)
        , playerId(playerActorId)
        , materialId(changedMaterialId)
        , amountDelta(changedAmount)
        , reason(std::move(changeReason))
    {
    }

    ActorId playerId = DefaultPlayerActorId;
    Material::ID materialId = Material::ID::Nothing;
    int amountDelta = 0;
    std::string reason;
};

#endif // PLAYEREVENTS_H_INCLUDED
