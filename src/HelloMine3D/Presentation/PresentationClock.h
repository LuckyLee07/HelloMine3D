#pragma once

#include <algorithm>
#include <cmath>

// Cosmetic phase only: do not use this clock for simulation, cooldowns,
// captions or performance measurements. Resume without replaying a long stall.
inline double advancePresentationClock(double elapsedSeconds, float frameSeconds)
{
    if (!std::isfinite(frameSeconds) || frameSeconds <= 0.f)
        return elapsedSeconds;
    return elapsedSeconds + std::min(double(frameSeconds), .25);
}
