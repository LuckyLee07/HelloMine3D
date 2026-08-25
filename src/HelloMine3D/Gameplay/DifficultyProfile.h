#pragma once

#include <algorithm>
#include <cstddef>

inline constexpr int CurrentDifficultyProfileVersion = 1;

enum class WorldDifficulty : int
{
    Casual = 0,
    Normal = 1,
    Challenging = 2,
    Count
};

inline bool validWorldDifficulty(WorldDifficulty difficulty) noexcept
{
    return difficulty >= WorldDifficulty::Casual &&
           difficulty < WorldDifficulty::Count;
}

inline const char *worldDifficultyName(WorldDifficulty difficulty) noexcept
{
    switch (difficulty)
    {
        case WorldDifficulty::Casual:
            return "Casual";
        case WorldDifficulty::Normal:
            return "Normal";
        case WorldDifficulty::Challenging:
            return "Challenging";
        case WorldDifficulty::Count:
            break;
    }
    return "Invalid";
}

struct DifficultyProfile
{
    int version = CurrentDifficultyProfileVersion;
    WorldDifficulty difficulty = WorldDifficulty::Normal;
    float playerOutgoingDamageMultiplier = 1.f;
    float playerIncomingDamageMultiplier = 1.f;
    std::size_t naturalSpawnAttemptsPerCycle = 16;
    std::size_t naturalMobWorldCap = 12;
    std::size_t naturalMobLocalCap = 4;
    int lootAmountNumerator = 1;
    int lootAmountDenominator = 1;

    int scaleLootAmount(int amount) const noexcept
    {
        if (amount <= 0 || lootAmountNumerator <= 0 ||
            lootAmountDenominator <= 0)
        {
            return 0;
        }
        const int scaled = lootAmountNumerator >= lootAmountDenominator
            ? (amount * lootAmountNumerator + lootAmountDenominator - 1) /
                  lootAmountDenominator
            : (amount * lootAmountNumerator) / lootAmountDenominator;
        return std::max(1, scaled);
    }
};

inline DifficultyProfile difficultyProfile(
    WorldDifficulty difficulty) noexcept
{
    switch (difficulty)
    {
        case WorldDifficulty::Casual:
            return {CurrentDifficultyProfileVersion,
                    WorldDifficulty::Casual,
                    1.20f, 0.75f, 8, 8, 3, 5, 4};
        case WorldDifficulty::Challenging:
            return {CurrentDifficultyProfileVersion,
                    WorldDifficulty::Challenging,
                    0.90f, 1.25f, 24, 16, 6, 4, 5};
        case WorldDifficulty::Normal:
        case WorldDifficulty::Count:
            return {CurrentDifficultyProfileVersion,
                    WorldDifficulty::Normal,
                    1.f, 1.f, 16, 12, 4, 1, 1};
    }
    return {};
}

struct DifficultyRuntimeSnapshot
{
    int profileVersion = CurrentDifficultyProfileVersion;
    WorldDifficulty active = WorldDifficulty::Normal;
    bool changePending = false;
    WorldDifficulty pending = WorldDifficulty::Normal;
    unsigned long long applicationEpoch = 0;
    DifficultyProfile parameters;
};

enum class DifficultyChangeResult
{
    Queued,
    Unchanged,
    Invalid
};
