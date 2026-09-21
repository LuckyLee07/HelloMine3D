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
    std::array<std::array<double, 3>, 13> waypoints{};
    std::size_t waypointCount = 0;

    static VisualCameraSweep parse(const char* value, bool hiddenCapture,
                                   bool hasWorld, bool otherScenario,
                                   const char* pathValue = nullptr)
    {
        VisualCameraSweep result;
        if (value == nullptr || value[0] == '\0') {
            if (pathValue != nullptr && pathValue[0] != '\0')
                throw std::runtime_error("Camera waypoints require an enabled diagnostic sweep.");
            return result;
        }
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
        if (pathValue != nullptr && pathValue[0] != '\0') {
            std::istringstream path(pathValue);
            double length = 0.0;
            while (path >> std::ws && !path.eof()) {
                if (result.waypointCount == result.waypoints.size())
                    throw std::runtime_error("Camera path permits at most 13 waypoints.");
                auto &point = result.waypoints[result.waypointCount];
                for (double &number : point)
                    if (!(path >> number) || !std::isfinite(number))
                        throw std::runtime_error("Camera waypoints require finite xyz triples.");
                if (std::hypot(point[0], point[1], point[2]) > 48.0)
                    throw std::runtime_error("Camera waypoint exceeds the 48 m capture boundary.");
                if (result.waypointCount > 0) {
                    const auto &previous = result.waypoints[result.waypointCount - 1];
                    length += std::hypot(point[0] - previous[0], point[1] - previous[1],
                                         point[2] - previous[2]);
                }
                ++result.waypointCount;
            }
            if (result.waypointCount < 2 || length > 64.0 ||
                result.waypoints.front() != std::array<double, 3>{})
                throw std::runtime_error("Camera path needs an origin and a bounded destination.");
            const auto &last = result.waypoints[result.waypointCount - 1];
            for (std::size_t axis = 0; axis < 3; ++axis)
                if (std::abs(last[axis] - result.delta[axis]) > 1e-8)
                    throw std::runtime_error("Camera path destination differs from the sweep.");
        }
        return result;
    }

    std::array<double, 5> offset(double elapsedSeconds) const
    {
        std::array<double, 5> result{};
        if (!enabled || !std::isfinite(elapsedSeconds)) return result;
        const double t = std::clamp((elapsedSeconds - WarmupSeconds) / durationSeconds, 0.0, 1.0);
        const double blend = t * t * (3.0 - 2.0 * t);
        for (std::size_t i = 0; i < result.size(); ++i) result[i] = delta[i] * blend;
        if (waypointCount > 0) {
            const double progress = blend * (waypointCount - 1);
            const auto first = std::min(static_cast<std::size_t>(progress), waypointCount - 2);
            const double fraction = progress - first;
            for (std::size_t axis = 0; axis < 3; ++axis)
                result[axis] = waypoints[first][axis] * (1.0 - fraction) +
                               waypoints[first + 1][axis] * fraction;
        }
        return result;
    }
};
