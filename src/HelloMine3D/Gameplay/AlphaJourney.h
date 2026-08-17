#ifndef ALPHAJOURNEY_H_INCLUDED
#define ALPHAJOURNEY_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <string>

#include "ObjectiveState.h"
#include "ObjectiveSystem.h"

class Player;
class SandboxEventBus;

enum class AlphaJourneyStep : std::uint8_t
{
    GatherWood = 0,
    CraftWorkbench,
    PlaceWorkbench,
    CraftWoodenPickaxe,
    GatherStone,
    CraftStonePickaxe,
    GatherIronOre,
    DefeatMob,
    CollectMobLoot,
    ReopenWorld,
    Complete
};

struct AlphaJourneySnapshot
{
    AlphaJourneyStep step = AlphaJourneyStep::GatherWood;
    std::size_t completedSteps = 0;
    std::size_t totalSteps = 10;
    int progress = 0;
    int required = 0;
    std::string title;
    std::string instruction;
    std::string completionFeedback;

    bool complete() const noexcept
    {
        return step == AlphaJourneyStep::Complete;
    }
};

/// Compatibility facade for the frozen G6 ten-step view. N1 progression is
/// owned by ObjectiveSystem; this class only preserves the old public snapshot
/// and flag contract while worlds migrate from save version 4.
class AlphaJourney
{
  public:
    static constexpr int RequiredOakBark = 11;
    static constexpr int RequiredStone = 3;
    static constexpr std::size_t StepCount =
        ObjectiveState::LegacyAlphaIds.size();
    static constexpr std::uint32_t KnownFlags =
        ObjectiveState::LegacyAlphaKnownFlags;

    AlphaJourney(Player& player, SandboxEventBus& eventBus,
                 std::uint32_t persistedFlags, bool restoredWorld);
    AlphaJourney(Player& player, SandboxEventBus& eventBus,
                 const ObjectiveSaveState& objectiveState,
                 std::uint32_t persistedFlags, bool restoredWorld);

    AlphaJourney(const AlphaJourney&) = delete;
    AlphaJourney& operator=(const AlphaJourney&) = delete;

    void update(float deltaSeconds);
    AlphaJourneySnapshot snapshot() const;
    ObjectiveSnapshot objectiveSnapshot() const;
    ObjectiveSaveState objectiveSaveState() const;
    std::uint32_t flags() const noexcept;

    static constexpr bool validFlags(std::uint32_t flags) noexcept
    {
        return (flags & ~KnownFlags) == 0u;
    }

  private:
    ObjectiveSystem m_objectives;
};

#endif // ALPHAJOURNEY_H_INCLUDED
