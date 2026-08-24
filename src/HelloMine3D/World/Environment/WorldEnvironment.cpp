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

    const glm::vec3 dayFog(0.56f, 0.70f, 0.80f);
    const glm::vec3 nightFog(0.025f, 0.040f, 0.085f);
    const glm::vec3 twilightFog(0.72f, 0.42f, 0.30f);
    state.fogColour = mix(mix(nightFog, dayFog, dayAmount), twilightFog,
                         twilightAmount * 0.38f);
    state.skyHorizonColour = state.fogColour;

    const glm::vec3 dayZenith(0.12f, 0.36f, 0.68f);
    const glm::vec3 nightZenith(0.006f, 0.014f, 0.050f);
    const glm::vec3 twilightZenith(0.25f, 0.10f, 0.18f);
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

    const glm::vec3 dayCloudLight(0.90f, 0.93f, 0.95f);
    const glm::vec3 nightCloudLight(0.10f, 0.14f, 0.22f);
    const glm::vec3 twilightCloudLight(0.94f, 0.60f, 0.42f);
    state.cloudLightColour = mix(
        mix(nightCloudLight, dayCloudLight, dayAmount),
        twilightCloudLight, twilightAmount * 0.34f);

    const glm::vec3 dayCloudShadow(0.42f, 0.53f, 0.60f);
    const glm::vec3 nightCloudShadow(0.018f, 0.032f, 0.075f);
    const glm::vec3 twilightCloudShadow(0.52f, 0.29f, 0.25f);
    state.cloudShadowColour = mix(
        mix(nightCloudShadow, dayCloudShadow, dayAmount),
        twilightCloudShadow, twilightAmount * 0.30f);
    state.cloudCoverage = 0.44f +
        0.04f * (0.5f + 0.5f * std::cos(angle * 2.f));

    const glm::vec3 dayWaterShallow(0.12f, 0.43f, 0.53f);
    const glm::vec3 nightWaterShallow(0.020f, 0.075f, 0.13f);
    const glm::vec3 twilightWaterShallow(0.22f, 0.27f, 0.31f);
    state.waterShallowColour = mix(
        mix(nightWaterShallow, dayWaterShallow, dayAmount),
        twilightWaterShallow, twilightAmount * 0.22f);

    const glm::vec3 dayWaterDeep(0.018f, 0.15f, 0.24f);
    const glm::vec3 nightWaterDeep(0.005f, 0.024f, 0.060f);
    const glm::vec3 twilightWaterDeep(0.085f, 0.10f, 0.15f);
    state.waterDeepColour = mix(
        mix(nightWaterDeep, dayWaterDeep, dayAmount),
        twilightWaterDeep, twilightAmount * 0.18f);
    return state;
}
