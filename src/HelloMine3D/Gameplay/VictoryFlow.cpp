#include "VictoryFlow.h"

#include <stdexcept>

VictoryFlow::VictoryFlow(WorldOutcomeState state)
    : m_state(state)
{
    if (!validWorldOutcomeState(m_state))
    {
        throw std::invalid_argument("World outcome state is invalid.");
    }
}

const WorldOutcomeState& VictoryFlow::state() const noexcept
{
    return m_state;
}

WorldOutcomeSnapshot VictoryFlow::snapshot() const noexcept
{
    WorldOutcomeSnapshot result;
    result.phase = m_state.phase;
    result.rewardEpoch = m_state.rewardEpoch;
    result.victory = worldOutcomeIsVictory(m_state);
    result.rewardAvailable =
        m_state.phase == WorldOutcomePhase::Victorious;
    result.rewardClaimed =
        m_state.phase == WorldOutcomePhase::RewardClaimed;
    return result;
}

VictoryTransitionResult VictoryFlow::activate() noexcept
{
    if (m_state.phase == WorldOutcomePhase::Activated)
    {
        return VictoryTransitionResult::AlreadyApplied;
    }
    if (m_state.phase != WorldOutcomePhase::Unstarted)
    {
        return VictoryTransitionResult::Rejected;
    }
    m_state.phase = WorldOutcomePhase::Activated;
    return VictoryTransitionResult::Applied;
}

VictoryTransitionResult VictoryFlow::beginEncounter() noexcept
{
    if (m_state.phase == WorldOutcomePhase::Encounter)
    {
        return VictoryTransitionResult::AlreadyApplied;
    }
    if (m_state.phase != WorldOutcomePhase::Activated)
    {
        return VictoryTransitionResult::Rejected;
    }
    m_state.phase = WorldOutcomePhase::Encounter;
    return VictoryTransitionResult::Applied;
}

VictoryTransitionResult VictoryFlow::abandonEncounter() noexcept
{
    if (m_state.phase == WorldOutcomePhase::Activated)
    {
        return VictoryTransitionResult::AlreadyApplied;
    }
    if (m_state.phase != WorldOutcomePhase::Encounter)
    {
        return VictoryTransitionResult::Rejected;
    }
    m_state.phase = WorldOutcomePhase::Activated;
    return VictoryTransitionResult::Applied;
}

VictoryTransitionResult VictoryFlow::resolveVictory(
    std::uint32_t rewardEpoch) noexcept
{
    if ((m_state.phase == WorldOutcomePhase::Victorious ||
         m_state.phase == WorldOutcomePhase::RewardClaimed) &&
        m_state.rewardEpoch == rewardEpoch)
    {
        return VictoryTransitionResult::AlreadyApplied;
    }
    if (m_state.phase != WorldOutcomePhase::Encounter || rewardEpoch == 0)
    {
        return VictoryTransitionResult::Rejected;
    }
    m_state.phase = WorldOutcomePhase::Victorious;
    m_state.rewardEpoch = rewardEpoch;
    m_state.claimedRewardEpoch = 0;
    return VictoryTransitionResult::Applied;
}

VictoryTransitionResult VictoryFlow::claimReward(
    std::uint32_t rewardEpoch) noexcept
{
    if (m_state.phase == WorldOutcomePhase::RewardClaimed &&
        m_state.claimedRewardEpoch == rewardEpoch)
    {
        return VictoryTransitionResult::AlreadyApplied;
    }
    if (m_state.phase != WorldOutcomePhase::Victorious || rewardEpoch == 0 ||
        rewardEpoch != m_state.rewardEpoch)
    {
        return VictoryTransitionResult::Rejected;
    }
    m_state.phase = WorldOutcomePhase::RewardClaimed;
    m_state.claimedRewardEpoch = rewardEpoch;
    return VictoryTransitionResult::Applied;
}
