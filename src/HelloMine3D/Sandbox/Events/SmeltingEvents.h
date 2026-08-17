#pragma once

#include "SandboxEventBus.h"
#include "../../Item/Material.h"
#include "../../Maths/glm.h"

struct SmeltCompletedEvent : public SandboxEvent
{
    SmeltCompletedEvent(Material::ID input, Material::ID output,
                        int completedAmount,
                        const glm::ivec3 &furnacePosition)
        : SandboxEvent(SandboxEventType::SmeltCompleted)
        , inputMaterialId(input)
        , outputMaterialId(output)
        , amount(completedAmount)
        , position(furnacePosition)
    {
    }

    Material::ID inputMaterialId = Material::ID::Nothing;
    Material::ID outputMaterialId = Material::ID::Nothing;
    int amount = 0;
    glm::ivec3 position{0};
};
