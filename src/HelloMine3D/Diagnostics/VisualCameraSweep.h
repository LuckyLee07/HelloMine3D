#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <stdexcept>

// An opt-in render-camera diagnostic. Never supplies player input or changes
// world residency, simulation, inventory, or save state.
struct VisualCameraSweep
{
    static constexpr double WarmupSeconds = 4.0;
    bool enabled = false;
    std::array<double, 5> delta{}; // World-space x/y/z, yaw, pitch.
    double durationSeconds = 12.0;

    static VisualCameraSweep parse(const char* value, bool hiddenCapture,
                                   bool hasWorld, bool otherScenario)
    {
        VisualCameraSweep result;
        if (value == nullptr || value[0] == '\0') return result;
        if (!hiddenCapture || !hasWorld || otherScenario)
            throw std::runtime_error("Camera sweep requires a hidden render capture with a world and no other scenario.");
        std::istringstream input(value);
        for (double& number : result.delta)
            if (!(input >> number) || !std::isfinite(number))
                throw std::runtime_error("Camera sweep expects finite dx dy dz yaw pitch duration.");
        if (!(input >> result.durationSeconds) || !std::isfinite(result.durationSeconds))
            throw std::runtime_error("Camera sweep duration must be finite.");
        input >> std::ws;
        if (!input.eof() || result.durationSeconds < 1.0 || result.durationSeconds > 30.0 ||
            std::hypot(result.delta[0], result.delta[1], result.delta[2]) > 48.0 ||
            std::abs(result.delta[3]) > 180.0 || std::abs(result.delta[4]) > 45.0)
            throw std::runtime_error("Camera sweep exceeds its bounded range (48 m, 180/45 degrees, 1-30 seconds).");
        result.enabled = true;
        return result;
    }

    std::array<double, 5> offset(double elapsedSeconds) const
    {
        std::array<double, 5> result{};
        if (!enabled || !std::isfinite(elapsedSeconds)) return result;
        const double t = std::clamp((elapsedSeconds - WarmupSeconds) / durationSeconds, 0.0, 1.0);
        const double blend = t * t * (3.0 - 2.0 * t);
        for (std::size_t i = 0; i < result.size(); ++i) result[i] = delta[i] * blend;
        return result;
    }
};
