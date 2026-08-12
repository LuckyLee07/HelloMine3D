#ifndef LIGHTLEVEL_H_INCLUDED
#define LIGHTLEVEL_H_INCLUDED

#include <cstdint>

using LightLevel = std::uint8_t;

constexpr LightLevel MIN_LIGHT_LEVEL = 0;
constexpr LightLevel MAX_LIGHT_LEVEL = 15;
constexpr float MIN_TERRAIN_BRIGHTNESS = 0.15f;

inline LightLevel clampLightLevel(LightLevel level)
{
    return level > MAX_LIGHT_LEVEL ? MAX_LIGHT_LEVEL : level;
}

inline float lightLevelToBrightness(LightLevel level)
{
    const LightLevel boundedLevel = clampLightLevel(level);
    const float normalized =
        static_cast<float>(boundedLevel) /
        static_cast<float>(MAX_LIGHT_LEVEL);
    return MIN_TERRAIN_BRIGHTNESS +
           (1.f - MIN_TERRAIN_BRIGHTNESS) * normalized;
}

#endif // LIGHTLEVEL_H_INCLUDED
