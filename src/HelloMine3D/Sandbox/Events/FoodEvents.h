#pragma once

#include "SandboxEventBus.h"
#include "../../Actor/ActorTypes.h"
#include "../../Item/Material.h"
#include "../../Maths/glm.h"

struct FoodConsumedEvent : SandboxEvent
{
    FoodConsumedEvent(ActorId playerActorId, Material::ID foodMaterialId,
                      float restored, float healthAfter,
                      const glm::vec3 &worldPosition)
        : SandboxEvent(SandboxEventType::FoodConsumed)
        , playerId(playerActorId)
        , materialId(foodMaterialId)
        , healthRestored(restored)
        , resultingHealth(healthAfter)
        , position(worldPosition)
    {
    }

    ActorId playerId = InvalidActorId;
    Material::ID materialId = Material::ID::Nothing;
    float healthRestored = 0.f;
    float resultingHealth = 0.f;
    glm::vec3 position{0.f};
};
