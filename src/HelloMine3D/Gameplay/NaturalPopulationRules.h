#pragma once

#include <cmath>
#include <cstdint>

#include "../World/Environment/WorldEnvironment.h"

// Runtime surface-population policy. It never edits terrain or saved actors.
namespace NaturalPopulationRules
{
    inline constexpr int NightStartTick = 12000;
    inline constexpr int BrightBlockLight = 8;
    inline constexpr float ChaseBuffer = 4.f;
    inline constexpr float MinimumCandidateRadius = 24.f;
    inline constexpr float MaximumCandidateRadius = 30.f;

    inline bool surfaceSpawnTime(int tick) noexcept
    {
        return tick > 0 && tick % WorldEnvironment::TicksPerDay >= NightStartTick;
    }

    inline glm::ivec2 offset(std::uint32_t first, std::uint32_t second) noexcept
    {
        const int x = static_cast<int>(first % 45u) - 22;
        const int z = static_cast<int>(second % 45u) - 22;
        if (x == 0 && z == 0) return {static_cast<int>(MinimumCandidateRadius), 0};
        const float length = std::sqrt(static_cast<float>(x * x + z * z));
        constexpr auto radiusChoices = static_cast<std::uint32_t>(MaximumCandidateRadius - MinimumCandidateRadius) + 1u;
        const float radius = MinimumCandidateRadius + static_cast<float>((first >> 8) % radiusChoices);
        return {static_cast<int>(std::lround(x * radius / length)),
                static_cast<int>(std::lround(z * radius / length))};
    }

    inline bool outsideChaseRange(const glm::vec3& player,
        const glm::vec3& candidate, float chaseRadius) noexcept
    {
        const float dx = candidate.x - player.x;
        const float dz = candidate.z - player.z;
        const float minimum = chaseRadius + ChaseBuffer;
        return std::isfinite(dx) && std::isfinite(dz) &&
            std::isfinite(chaseRadius) && chaseRadius > 0.f &&
            dx * dx + dz * dz >= minimum * minimum;
    }
}
