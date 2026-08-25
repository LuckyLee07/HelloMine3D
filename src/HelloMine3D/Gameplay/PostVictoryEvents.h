#pragma once

namespace PostVictoryEvents
{
    inline constexpr int CurrentVersion = 1;
    inline constexpr int MaximumEvents = 3;
    inline constexpr int WaveCount = 2;
    inline constexpr int RewardIronIngots = 2;

    inline bool validProgress(int version, int completedEvents) noexcept
    {
        return version == CurrentVersion && completedEvents >= 0 &&
               completedEvents <= MaximumEvents;
    }

    inline int guardianCount(int event, int wave) noexcept
    {
        if (event < 1 || event > MaximumEvents || wave < 1 ||
            wave > WaveCount)
        {
            return 0;
        }
        if (event == 1)
        {
            return 1;
        }
        if (event == 2)
        {
            return wave == 1 ? 2 : 1;
        }
        return 2;
    }
}

struct PostVictoryEventSnapshot
{
    int version = PostVictoryEvents::CurrentVersion;
    int completedEvents = 0;
    int totalEvents = PostVictoryEvents::MaximumEvents;
    int activeEvent = 0;
    int wave = 0;
    int remainingGuardians = 0;
    int loadedGuardians = 0;
    bool rewardPending = false;
    bool complete = false;
};
