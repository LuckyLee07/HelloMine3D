#include "AlphaJourney.h"

#include "ObjectiveRegistry.h"

namespace
{
    AlphaJourneyStep stepForId(const std::string& id) noexcept
    {
        for (std::size_t index = 0;
             index < ObjectiveState::LegacyAlphaIds.size(); ++index)
        {
            if (id == ObjectiveState::LegacyAlphaIds[index])
            {
                return static_cast<AlphaJourneyStep>(index);
            }
        }
        return AlphaJourneyStep::Complete;
    }
}

AlphaJourney::AlphaJourney(Player& player, SandboxEventBus& eventBus,
                           std::uint32_t persistedFlags,
                           bool restoredWorld)
    : AlphaJourney(player, eventBus, ObjectiveSaveState{}, persistedFlags,
                   restoredWorld)
{
}

AlphaJourney::AlphaJourney(Player& player, SandboxEventBus& eventBus,
                           const ObjectiveSaveState& objectiveState,
                           std::uint32_t persistedFlags,
                           bool restoredWorld)
    : m_objectives(runtimeObjectiveRegistry(), player, eventBus,
                   objectiveState, persistedFlags, restoredWorld)
{
}

void AlphaJourney::update(float deltaSeconds)
{
    m_objectives.update(deltaSeconds);
}

AlphaJourneySnapshot AlphaJourney::snapshot() const
{
    const ObjectiveSnapshot objective = m_objectives.snapshot();
    AlphaJourneySnapshot result;
    result.step = objective.sessionComplete
                      ? AlphaJourneyStep::Complete
                      : stepForId(objective.currentId);
    result.totalSteps = StepCount;
    const std::uint32_t completedFlags = flags();
    for (std::size_t index = 0; index < StepCount; ++index)
    {
        if ((completedFlags & (1u << static_cast<unsigned>(index))) == 0u)
        {
            break;
        }
        ++result.completedSteps;
    }
    result.progress = objective.progress;
    result.required = objective.required;
    result.title = objective.title;
    result.instruction = objective.instruction;
    result.completionFeedback = objective.completionFeedback;
    return result;
}

ObjectiveSnapshot AlphaJourney::objectiveSnapshot() const
{
    return m_objectives.snapshot();
}

RecipeDiscoverySnapshot AlphaJourney::recipeDiscoverySnapshot() const
{
    return m_objectives.recipeDiscoverySnapshot();
}

bool AlphaJourney::isRecipeDiscovered(
    const std::string& recipeId) const noexcept
{
    return m_objectives.isRecipeDiscovered(recipeId);
}

ObjectiveSaveState AlphaJourney::objectiveSaveState() const
{
    return m_objectives.saveState();
}

std::uint32_t AlphaJourney::flags() const noexcept
{
    return m_objectives.legacyAlphaFlags();
}
