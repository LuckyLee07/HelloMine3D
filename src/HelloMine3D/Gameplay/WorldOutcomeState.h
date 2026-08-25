#pragma once

#include <cstdint>

enum class WorldOutcomePhase : std::uint8_t
{
    Unstarted = 0,
    Activated = 1,
    Encounter = 2,
    Victorious = 3,
    RewardClaimed = 4
};

struct WorldOutcomeState
{
    WorldOutcomePhase phase = WorldOutcomePhase::Unstarted;
    std::uint32_t rewardEpoch = 0;
    std::uint32_t claimedRewardEpoch = 0;
};

inline bool validWorldOutcomePhase(int value) noexcept
{
    return value >= static_cast<int>(WorldOutcomePhase::Unstarted) &&
           value <= static_cast<int>(WorldOutcomePhase::RewardClaimed);
}

inline bool validWorldOutcomeState(const WorldOutcomeState& state) noexcept
{
    switch (state.phase)
    {
    case WorldOutcomePhase::Unstarted:
    case WorldOutcomePhase::Activated:
    case WorldOutcomePhase::Encounter:
        return state.rewardEpoch == 0 && state.claimedRewardEpoch == 0;
    case WorldOutcomePhase::Victorious:
        return state.rewardEpoch > 0 && state.claimedRewardEpoch == 0;
    case WorldOutcomePhase::RewardClaimed:
        return state.rewardEpoch > 0 &&
               state.claimedRewardEpoch == state.rewardEpoch;
    }
    return false;
}

inline bool worldOutcomeIsVictory(const WorldOutcomeState& state) noexcept
{
    return state.phase == WorldOutcomePhase::Victorious ||
           state.phase == WorldOutcomePhase::RewardClaimed;
}

inline const char* worldOutcomePhaseName(WorldOutcomePhase phase) noexcept
{
    switch (phase)
    {
    case WorldOutcomePhase::Unstarted: return "unstarted";
    case WorldOutcomePhase::Activated: return "activated";
    case WorldOutcomePhase::Encounter: return "encounter";
    case WorldOutcomePhase::Victorious: return "victorious";
    case WorldOutcomePhase::RewardClaimed: return "reward_claimed";
    }
    return "invalid";
}
