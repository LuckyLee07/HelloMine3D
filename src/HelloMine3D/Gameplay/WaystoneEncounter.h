#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "../Maths/glm.h"
#include "PostVictoryEvents.h"

enum class WaystoneActionResult
{
    Activated,
    EncounterStarted,
    EncounterInProgress,
    RewardClaimed,
    RewardAlreadyClaimed,
    PostVictoryEventStarted,
    PostVictoryEventInProgress,
    PostVictoryRewardClaimed,
    PostVictoryComplete,
    PostVictoryInventoryFull,
    SimulationPaused,
    PlayerUnavailable,
    PlayerDead,
    UiBusy,
    OutOfReach,
    InvalidCore,
    MissingMaterials,
    InventoryFull,
    SpawnBlocked,
    Rejected
};

struct WaystoneEncounterState
{
    int wave = 0;
    int remainingGuardians = 0;
    std::uint32_t rewardEpoch = 0;
    int postVictoryEvent = 0;
    int postVictoryWave = 0;
    int postVictoryRemainingGuardians = 0;
};

struct WaystoneEncounterSnapshot
{
    bool anchorKnown = false;
    glm::ivec3 anchor{0};
    int wave = 0;
    int remainingGuardians = 0;
    std::size_t loadedGuardians = 0;
    int postVictoryEvent = 0;
    int postVictoryWave = 0;
    int postVictoryRemainingGuardians = 0;
};

namespace WaystoneEncounter
{
    inline constexpr const char* BlockEntityType = "hellomine:waystone";
    inline constexpr const char* StalkerType =
        "hellomine:waystone_stalker";
    inline constexpr const char* BruteType =
        "hellomine:waystone_brute";
    inline constexpr int ActivationIronIngots = 2;
    inline constexpr int RewardIronIngots = 3;
    inline constexpr int WaveCount = 2;
    inline constexpr int FirstWaveGuardians = 2;
    inline constexpr int SecondWaveGuardians = 1;
    inline constexpr std::size_t MaximumLoadedGuardians = 2;
    inline constexpr std::uint32_t RewardEpoch = 1;
    inline constexpr float InteractionReach = 6.5f;

    bool validState(const WaystoneEncounterState& state) noexcept;
    std::string serialize(const WaystoneEncounterState& state);
    bool deserialize(const std::string& payload,
                     WaystoneEncounterState& state,
                     std::string* error = nullptr);
    const char* actorTypeForWave(int wave) noexcept;
    int guardianCountForWave(int wave) noexcept;
    const char* feedbackKey(WaystoneActionResult result) noexcept;
}
