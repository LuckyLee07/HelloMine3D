#include "WorldEnvironment.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float Pi = 3.14159265358979323846f;

    float smoothStep(float edge0, float edge1, float value)
    {
        const float amount = std::clamp(
            (value - edge0) / (edge1 - edge0), 0.f, 1.f);
        return amount * amount * (3.f - 2.f * amount);
    }

    glm::vec3 mix(const glm::vec3 &from, const glm::vec3 &to,
                  float amount)
    {
        return from + (to - from) * amount;
    }
}

WorldEnvironmentState WorldEnvironment::evaluate(float worldTime)
{
    const float ticksPerDay = static_cast<float>(TicksPerDay);
    float wrappedTime = std::fmod(worldTime, ticksPerDay);
    if (wrappedTime < 0.f) {
        wrappedTime += ticksPerDay;
    }

    WorldEnvironmentState state;
    state.cycle = wrappedTime / ticksPerDay;

    // The cycle starts at dawn, reaches noon at 0.25, dusk at 0.5 and
    // midnight at 0.75. Twilight stays broad enough to avoid abrupt changes.
    const float sunHeight = std::sin(state.cycle * 2.f * Pi);
    const float dayAmount = smoothStep(-0.18f, 0.22f, sunHeight);
    const float twilightAmount =
        1.f - smoothStep(0.f, 0.38f, std::abs(sunHeight));

    state.daylight = 0.18f + 0.82f * dayAmount;
    state.fogDensity = 0.006f + (0.0015f - 0.006f) * dayAmount;

    const glm::vec3 daySky(1.f, 1.f, 1.f);
    const glm::vec3 nightSky(0.12f, 0.18f, 0.34f);
    const glm::vec3 twilightSky(1.f, 0.48f, 0.28f);
    state.skyTint = mix(mix(nightSky, daySky, dayAmount), twilightSky,
                        twilightAmount * 0.55f);

    const glm::vec3 dayFog(0.58f, 0.75f, 0.92f);
    const glm::vec3 nightFog(0.035f, 0.055f, 0.11f);
    const glm::vec3 twilightFog(0.78f, 0.38f, 0.25f);
    state.fogColour = mix(mix(nightFog, dayFog, dayAmount), twilightFog,
                         twilightAmount * 0.45f);
    return state;
}
