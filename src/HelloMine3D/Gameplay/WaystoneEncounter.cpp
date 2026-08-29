#include "WaystoneEncounter.h"

#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace
{
    constexpr int PayloadVersion = 2;

    bool fail(std::string* error, const char* message)
    {
        if (error != nullptr)
        {
            *error = message;
        }
        return false;
    }
}

bool WaystoneEncounter::validState(
    const WaystoneEncounterState& state) noexcept
{
    bool originalStateValid = false;
    switch (state.wave)
    {
    case 0:
        originalStateValid = state.remainingGuardians == 0 &&
                             state.rewardEpoch == 0;
        break;
    case 1:
        originalStateValid = state.remainingGuardians >= 1 &&
                             state.remainingGuardians <=
                                 FirstWaveGuardians &&
                             state.rewardEpoch == 0;
        break;
    case 2:
        originalStateValid = state.remainingGuardians ==
                                 SecondWaveGuardians &&
                             state.rewardEpoch == 0;
        break;
    case 3:
        originalStateValid = state.remainingGuardians == 0 &&
                             state.rewardEpoch == RewardEpoch;
        break;
    default:
        return false;
    }
    if (!originalStateValid)
    {
        return false;
    }
    if (state.postVictoryEvent == 0)
    {
        return state.postVictoryWave == 0 &&
               state.postVictoryRemainingGuardians == 0;
    }
    if (state.wave != 3 || state.rewardEpoch != RewardEpoch ||
        state.postVictoryEvent < 1 ||
        state.postVictoryEvent > PostVictoryEvents::MaximumEvents)
    {
        return false;
    }
    if (state.postVictoryWave == 3)
    {
        return state.postVictoryRemainingGuardians == 0;
    }
    const int maximum = PostVictoryEvents::guardianCount(
        state.postVictoryEvent, state.postVictoryWave);
    return maximum > 0 && state.postVictoryRemainingGuardians >= 1 &&
           state.postVictoryRemainingGuardians <= maximum;
}

std::string WaystoneEncounter::serialize(
    const WaystoneEncounterState& state)
{
    if (!validState(state))
    {
        throw std::invalid_argument("Waystone encounter state is invalid.");
    }
    std::ostringstream output;
    output << "version " << PayloadVersion << '\n'
           << "wave " << state.wave << '\n'
           << "remaining " << state.remainingGuardians << '\n'
           << "reward_epoch " << state.rewardEpoch << '\n'
           << "post_victory_event " << state.postVictoryEvent << '\n'
           << "post_victory_wave " << state.postVictoryWave << '\n'
           << "post_victory_remaining "
           << state.postVictoryRemainingGuardians << '\n';
    return output.str();
}

bool WaystoneEncounter::deserialize(const std::string& payload,
                                    WaystoneEncounterState& state,
                                    std::string* error)
{
    std::istringstream input(payload);
    std::unordered_set<std::string> fields;
    WaystoneEncounterState parsed;
    int version = 0;
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty())
        {
            continue;
        }
        std::istringstream row(line);
        std::string key;
        std::string trailing;
        if (!(row >> key) || !fields.emplace(key).second)
        {
            return fail(error, "waystone payload has a duplicate field");
        }
        if (key == "version")
        {
            if (!(row >> version))
            {
                return fail(error, "waystone payload version is invalid");
            }
        }
        else if (key == "wave")
        {
            if (!(row >> parsed.wave))
            {
                return fail(error, "waystone payload wave is invalid");
            }
        }
        else if (key == "remaining")
        {
            if (!(row >> parsed.remainingGuardians))
            {
                return fail(error,
                            "waystone payload remaining count is invalid");
            }
        }
        else if (key == "reward_epoch")
        {
            if (!(row >> parsed.rewardEpoch))
            {
                return fail(error,
                            "waystone payload reward epoch is invalid");
            }
        }
        else if (key == "post_victory_event")
        {
            if (!(row >> parsed.postVictoryEvent))
            {
                return fail(error,
                            "waystone payload post-victory event is invalid");
            }
        }
        else if (key == "post_victory_wave")
        {
            if (!(row >> parsed.postVictoryWave))
            {
                return fail(error,
                            "waystone payload post-victory wave is invalid");
            }
        }
        else if (key == "post_victory_remaining")
        {
            if (!(row >> parsed.postVictoryRemainingGuardians))
            {
                return fail(
                    error,
                    "waystone payload post-victory remaining is invalid");
            }
        }
        else
        {
            return fail(error, "waystone payload field is unknown");
        }
        row >> trailing;
        if (!trailing.empty())
        {
            return fail(error, "waystone payload has trailing data");
        }
    }
    const bool supported =
        (version == 1 && fields.size() == 4) ||
        (version == PayloadVersion && fields.size() == 7);
    if (!supported || !validState(parsed))
    {
        return fail(error, "waystone payload state is unsupported");
    }
    state = parsed;
    return true;
}

const char* WaystoneEncounter::actorTypeForWave(int wave) noexcept
{
    if (wave == 1)
    {
        return StalkerType;
    }
    if (wave == 2)
    {
        return BruteType;
    }
    return "";
}

int WaystoneEncounter::guardianCountForWave(int wave) noexcept
{
    if (wave == 1)
    {
        return FirstWaveGuardians;
    }
    if (wave == 2)
    {
        return SecondWaveGuardians;
    }
    return 0;
}

const char* WaystoneEncounter::feedbackKey(
    WaystoneActionResult result) noexcept
{
    switch (result)
    {
    case WaystoneActionResult::Activated:
        return "waystone.feedback.activated";
    case WaystoneActionResult::EncounterStarted:
        return "waystone.feedback.encounter_started";
    case WaystoneActionResult::EncounterInProgress:
        return "waystone.feedback.encounter_in_progress";
    case WaystoneActionResult::RewardClaimed:
        return "waystone.feedback.reward_claimed";
    case WaystoneActionResult::RewardAlreadyClaimed:
        return "waystone.feedback.reward_already_claimed";
    case WaystoneActionResult::PostVictoryEventStarted:
        return "waystone.feedback.post_victory_started";
    case WaystoneActionResult::PostVictoryEventInProgress:
        return "waystone.feedback.post_victory_in_progress";
    case WaystoneActionResult::PostVictoryRewardClaimed:
        return "waystone.feedback.post_victory_reward_claimed";
    case WaystoneActionResult::PostVictoryComplete:
        return "waystone.feedback.post_victory_complete";
    case WaystoneActionResult::PostVictoryInventoryFull:
        return "waystone.feedback.post_victory_inventory_full";
    case WaystoneActionResult::SimulationPaused:
        return "waystone.feedback.paused";
    case WaystoneActionResult::PlayerUnavailable:
    case WaystoneActionResult::PlayerDead:
        return "waystone.feedback.player_unavailable";
    case WaystoneActionResult::UiBusy:
        return "waystone.feedback.ui_busy";
    case WaystoneActionResult::OutOfReach:
        return "waystone.feedback.out_of_reach";
    case WaystoneActionResult::InvalidCore:
        return "waystone.feedback.invalid_core";
    case WaystoneActionResult::MissingMaterials:
        return "waystone.feedback.missing_materials";
    case WaystoneActionResult::InventoryFull:
        return "waystone.feedback.inventory_full";
    case WaystoneActionResult::SpawnBlocked:
        return "waystone.feedback.spawn_blocked";
    case WaystoneActionResult::Rejected:
        return "waystone.feedback.rejected";
    case WaystoneActionResult::ResonancePulse:
        return "waystone.feedback.resonance_pulse";
    case WaystoneActionResult::ResonanceCharging:
        return "waystone.feedback.resonance_charging";
    case WaystoneActionResult::ResonanceNoTarget:
        return "waystone.feedback.resonance_no_target";
    }
    return "waystone.feedback.rejected";
}
