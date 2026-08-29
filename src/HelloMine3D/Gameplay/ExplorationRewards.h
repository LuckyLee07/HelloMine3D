#ifndef EXPLORATIONREWARDS_H_INCLUDED
#define EXPLORATIONREWARDS_H_INCLUDED

namespace ExplorationRewards
{
    inline constexpr int LegacyVersion = 0;
    inline constexpr int CurrentVersion = 1;
    inline constexpr int RaiderWardGuardRecoverTicks = 4;

    inline bool validVersion(int version) noexcept
    {
        return version >= LegacyVersion && version <= CurrentVersion;
    }
}

#endif // EXPLORATIONREWARDS_H_INCLUDED
