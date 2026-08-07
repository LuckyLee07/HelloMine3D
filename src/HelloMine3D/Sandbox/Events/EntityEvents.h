#ifndef ENTITYEVENTS_H_INCLUDED
#define ENTITYEVENTS_H_INCLUDED

#include "../../Actor/ActorTypes.h"
#include "../../Item/Material.h"
#include "../../Maths/glm.h"
#include "SandboxEventBus.h"

#include <string>
#include <utility>

struct EntitySpawnEvent : public SandboxEvent {
    EntitySpawnEvent(ActorId actorId, std::string actorType,
                     const glm::vec3 &actorPosition)
        : SandboxEvent(SandboxEventType::EntitySpawn)
        , id(actorId)
        , type(std::move(actorType))
        , position(actorPosition)
    {
    }

    ActorId id = InvalidActorId;
    std::string type;
    glm::vec3 position{0.f};
};

struct EntityDamageEvent : public SandboxEvent {
    EntityDamageEvent(ActorId damagedActorId, ActorId damageSourceId,
                      float damageAmount, float healthAfterDamage,
                      const glm::vec3 &actorPosition)
        : SandboxEvent(SandboxEventType::EntityDamage)
        , id(damagedActorId)
        , sourceId(damageSourceId)
        , amount(damageAmount)
        , healthAfter(healthAfterDamage)
        , position(actorPosition)
    {
    }

    ActorId id = InvalidActorId;
    ActorId sourceId = InvalidActorId;
    float amount = 0.f;
    float healthAfter = 0.f;
    glm::vec3 position{0.f};
};

struct EntityDeathEvent : public SandboxEvent {
    EntityDeathEvent(ActorId deadActorId, ActorId killerActorId,
                     const glm::vec3 &actorPosition)
        : SandboxEvent(SandboxEventType::EntityDeath)
        , id(deadActorId)
        , killerId(killerActorId)
        , position(actorPosition)
    {
    }

    ActorId id = InvalidActorId;
    ActorId killerId = InvalidActorId;
    glm::vec3 position{0.f};
};

struct ItemPickupEvent : public SandboxEvent {
    ItemPickupEvent(ActorId pickerActorId, ActorId itemActorId,
                    Material::ID pickedMaterialId, int pickedAmount,
                    const glm::vec3 &pickupPosition)
        : SandboxEvent(SandboxEventType::ItemPickup)
        , pickerId(pickerActorId)
        , itemId(itemActorId)
        , materialId(pickedMaterialId)
        , amount(pickedAmount)
        , position(pickupPosition)
    {
    }

    ActorId pickerId = InvalidActorId;
    ActorId itemId = InvalidActorId;
    Material::ID materialId = Material::ID::Nothing;
    int amount = 0;
    glm::vec3 position{0.f};
};

#endif // ENTITYEVENTS_H_INCLUDED
