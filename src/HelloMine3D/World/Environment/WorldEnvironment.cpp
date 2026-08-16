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

    const glm::vec3 dayFog(0.58f, 0.75f, 0.92f);
    const glm::vec3 nightFog(0.035f, 0.055f, 0.11f);
    const glm::vec3 twilightFog(0.78f, 0.38f, 0.25f);
    state.fogColour = mix(mix(nightFog, dayFog, dayAmount), twilightFog,
                         twilightAmount * 0.45f);
    state.skyHorizonColour = state.fogColour;

    const glm::vec3 dayZenith(0.12f, 0.46f, 0.88f);
    const glm::vec3 nightZenith(0.006f, 0.012f, 0.045f);
    const glm::vec3 twilightZenith(0.24f, 0.08f, 0.16f);
    state.skyZenithColour = mix(
        mix(nightZenith, dayZenith, dayAmount), twilightZenith,
        twilightAmount * 0.42f);

    const float angle = state.cycle * 2.f * Pi;
    state.sunDirection = glm::normalize(
        glm::vec3(std::cos(angle), std::sin(angle),
                  0.18f * std::cos(angle)));
    const glm::vec3 noonSun(1.f, 0.92f, 0.72f);
    const glm::vec3 twilightSun(1.f, 0.38f, 0.14f);
    state.sunColour = mix(noonSun, twilightSun, twilightAmount);
    state.sunIntensity = dayAmount;
    state.moonIntensity =
        1.f - smoothStep(-0.20f, 0.08f, sunHeight);
    state.starIntensity =
        1.f - smoothStep(-0.35f, -0.05f, sunHeight);
    return state;
}
