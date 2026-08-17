#ifndef CRAFTINGEVENTS_H_INCLUDED
#define CRAFTINGEVENTS_H_INCLUDED

#include <string>
#include <utility>

#include "../../Item/Material.h"
#include "../../Maths/glm.h"
#include "SandboxEventBus.h"

struct CraftCompletedEvent : public SandboxEvent {
    CraftCompletedEvent(std::string completedRecipeId,
                        Material::ID completedOutputMaterialId,
                        int completedCrafts, int addedOutput,
                        const glm::vec3 &craftPosition)
        : SandboxEvent(SandboxEventType::CraftCompleted)
        , recipeId(std::move(completedRecipeId))
        , outputMaterialId(completedOutputMaterialId)
        , craftsCompleted(completedCrafts)
        , outputAdded(addedOutput)
        , position(craftPosition)
    {
    }

    std::string recipeId;
    Material::ID outputMaterialId = Material::ID::Nothing;
    int craftsCompleted = 0;
    int outputAdded = 0;
    glm::vec3 position{0.f};
};

#endif // CRAFTINGEVENTS_H_INCLUDED
