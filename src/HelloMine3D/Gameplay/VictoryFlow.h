#pragma once

#include "WorldOutcomeState.h"

#include <cstdint>

enum class VictoryTransitionResult
{
    Applied,
    AlreadyApplied,
    Rejected
};

struct WorldOutcomeSnapshot
{
    WorldOutcomePhase phase = WorldOutcomePhase::Unstarted;
    std::uint32_t rewardEpoch = 0;
    bool victory = false;
    bool rewardAvailable = false;
    bool rewardClaimed = false;
};

/// Owns the bounded, renderer-independent world-outcome state machine.
/// N7B drives these commands from normal gameplay events; storage and UI only
/// consume the state/snapshot and never infer victory from objective absence.
class VictoryFlow
{
  public:
    explicit VictoryFlow(WorldOutcomeState state = {});

    const WorldOutcomeState& state() const noexcept;
    WorldOutcomeSnapshot snapshot() const noexcept;

    VictoryTransitionResult activate() noexcept;
    VictoryTransitionResult beginEncounter() noexcept;
    VictoryTransitionResult abandonEncounter() noexcept;
    VictoryTransitionResult resolveVictory(std::uint32_t rewardEpoch) noexcept;
    VictoryTransitionResult claimReward(std::uint32_t rewardEpoch) noexcept;

  private:
    WorldOutcomeState m_state;
};
