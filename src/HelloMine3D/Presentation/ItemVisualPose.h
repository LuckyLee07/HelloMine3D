#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include "../Maths/glm.h"

namespace ItemVisualPose {
struct Pose {
    glm::vec3 position;
    glm::quat orientation;
};

// Copied render inputs only: camera-facing presentation never turns the actor
// or changes its collision, pickup radius, age or saved rotation.
inline Pose drop(const glm::vec3& position, const glm::vec3& rotation,
                 const glm::vec3& cameraPosition, float ageSeconds,
                 std::uint64_t id, bool icon, float animationStrength)
{
    const float strength = std::isfinite(animationStrength)
        ? std::clamp(animationStrength, 0.f, 1.f) : 0.f;
    const double age = std::isfinite(ageSeconds) ? std::max(0.f, ageSeconds) : 0.;
    const double phase = age * 2. + static_cast<double>(id % 17);
    Pose pose;
    pose.position = position + glm::vec3(0.f,
        .22f + static_cast<float>(std::sin(phase)) * .025f * strength, 0.f);
    const auto turn = [](float degrees, const glm::vec3& axis) {
        return glm::angleAxis(glm::radians(degrees), axis);
    };
    if (icon) {
        const glm::vec3 towardCamera = cameraPosition - pose.position;
        const float horizontal = std::hypot(towardCamera.x, towardCamera.z);
        const float yaw = glm::degrees(std::atan2(towardCamera.x, towardCamera.z));
        const float pitch = -glm::degrees(std::atan2(towardCamera.y, horizontal));
        // Keep at least 80% of the front face projected toward the observer.
        // A small local turn still exposes the actual extruded side walls.
        const float sway = 16.f + 20.f * strength *
            static_cast<float>(std::sin(age * 1.3 + static_cast<double>(id % 17)));
        pose.orientation = turn(yaw, {0,1,0}) * turn(pitch, {1,0,0}) * turn(sway, {0,1,0});
    }
    else {
        const float spin = static_cast<float>(std::fmod(age * 35. * strength, 360.));
        pose.orientation = turn(rotation.y + spin, {0,1,0}) *
            turn(rotation.x, {1,0,0}) * turn(rotation.z, {0,0,1});
    }
    return pose;
}
}
