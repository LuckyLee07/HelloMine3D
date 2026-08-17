#ifndef ALPHAJOURNEY_H_INCLUDED
#define ALPHAJOURNEY_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Player;
class SandboxEventBus;

enum class AlphaJourneyStep : std::uint8_t {
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

struct AlphaJourneySnapshot {
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

class AlphaJourney {
  public:
    static constexpr int RequiredOakBark = 11;
    static constexpr int RequiredStone = 3;
    static constexpr std::size_t StepCount = 10;
    static constexpr std::uint32_t KnownFlags =
        (1u << static_cast<unsigned>(StepCount)) - 1u;

    AlphaJourney(Player &player, SandboxEventBus &eventBus,
                 std::uint32_t persistedFlags, bool restoredWorld);
    ~AlphaJourney();

    AlphaJourney(const AlphaJourney &) = delete;
    AlphaJourney &operator=(const AlphaJourney &) = delete;

    void update(float deltaSeconds);
    AlphaJourneySnapshot snapshot() const;
    std::uint32_t flags() const noexcept;

    static constexpr bool validFlags(std::uint32_t flags) noexcept
    {
        return (flags & ~KnownFlags) == 0u;
    }

  private:
    void refreshInventory();
    void mark(AlphaJourneyStep step, const char *feedback);
    bool has(AlphaJourneyStep step) const noexcept;
    int inventoryCount(int materialId) const noexcept;
    AlphaJourneyStep currentStep() const noexcept;

    Player *m_player = nullptr;
    SandboxEventBus *m_eventBus = nullptr;
    std::vector<unsigned> m_subscriptions;
    std::uint32_t m_flags = 0;
    std::string m_completionFeedback;
    float m_feedbackSeconds = 0.f;
};

#endif // ALPHAJOURNEY_H_INCLUDED
