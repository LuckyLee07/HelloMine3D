#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

struct ObjectiveProgressState
{
    std::string id;
    int value = 0;
};

struct ObjectiveSaveState
{
    static constexpr int LegacyDefinitionVersion = 1;
    static constexpr int PreviousDefinitionVersion = 2;
    static constexpr int CurrentDefinitionVersion = 3;

    int definitionVersion = CurrentDefinitionVersion;
    std::vector<std::string> completedIds;
    std::vector<ObjectiveProgressState> progress;
};

namespace ObjectiveState
{
    inline constexpr std::array<const char*, 10> LegacyAlphaIds = {
        "alpha.gather_wood",
        "alpha.craft_workbench",
        "alpha.place_workbench",
        "alpha.craft_wooden_pickaxe",
        "alpha.gather_stone",
        "alpha.craft_stone_pickaxe",
        "alpha.gather_iron_ore",
        "alpha.defeat_mob",
        "alpha.collect_mob_loot",
        "alpha.reopen_world"
    };

    inline constexpr std::uint32_t LegacyAlphaKnownFlags =
        (1u << static_cast<unsigned>(LegacyAlphaIds.size())) - 1u;

    inline bool isCanonicalId(const std::string& value) noexcept
    {
        if (value.empty() || value.size() > 80 ||
            !std::islower(static_cast<unsigned char>(value.front())))
        {
            return false;
        }
        return std::all_of(
            value.begin(), value.end(), [](unsigned char character)
            {
                return std::islower(character) || std::isdigit(character) ||
                       character == '.' || character == '_' ||
                       character == '-';
            });
    }

    inline std::vector<std::string>
    completedFromLegacyFlags(std::uint32_t flags)
    {
        std::vector<std::string> completed;
        for (std::size_t index = 0; index < LegacyAlphaIds.size(); ++index)
        {
            if ((flags & (1u << static_cast<unsigned>(index))) != 0u)
            {
                completed.emplace_back(LegacyAlphaIds[index]);
            }
        }
        return completed;
    }

    inline std::uint32_t legacyFlagsFromCompleted(
        const std::vector<std::string>& completed) noexcept
    {
        std::uint32_t flags = 0;
        for (std::size_t index = 0; index < LegacyAlphaIds.size(); ++index)
        {
            if (std::find(completed.begin(), completed.end(),
                          LegacyAlphaIds[index]) != completed.end())
            {
                flags |= 1u << static_cast<unsigned>(index);
            }
        }
        return flags;
    }
}
