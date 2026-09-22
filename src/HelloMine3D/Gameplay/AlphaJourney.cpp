#include "AlphaJourney.h"

#include "ObjectiveRegistry.h"

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
    result.step = AlphaJourneyStep::Complete;
    result.totalSteps = StepCount;
    const std::uint32_t completedFlags = flags();
    for (std::size_t index = 0; index < StepCount; ++index)
    {
        if ((completedFlags & (1u << static_cast<unsigned>(index))) == 0u)
        {
            result.step = static_cast<AlphaJourneyStep>(index);
            break;
        }
        ++result.completedSteps;
    }
    result.progress = objective.progress;
    result.required = objective.required;
    result.title = objective.title;
    result.instruction = objective.instruction;
    // The legacy ten-step view is independent of the active parallel route.
    // An optional reopen objective still has its original identity and bit.
    if (result.completedSteps < StepCount)
    {
        const auto* definition = runtimeObjectiveRegistry().find(
            ObjectiveState::LegacyAlphaIds[result.completedSteps]);
        if (definition != nullptr)
        {
            result.progress = m_objectives.progress(definition->id);
            result.required = definition->required;
            result.title = definition->title;
            result.instruction = definition->instruction;
        }
    }
    result.completionFeedback = objective.completionFeedback;
    return result;
}

ObjectiveSnapshot AlphaJourney::objectiveSnapshot(
    bool includeJournal, const ObjectiveGuidanceContext& guidance) const
{
    return m_objectives.snapshot(includeJournal, guidance);
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
