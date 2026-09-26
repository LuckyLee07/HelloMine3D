#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

#include "../Maths/glm.h"

// Render-only third-person camera geometry. The callback supplies the existing
// world's collidable-block truth; this module never owns gameplay state and
// never changes the authoritative first-person interaction ray.
namespace ThirdPersonCameraPresentation
{
enum class Mode
{
    FirstPerson,
    ThirdPersonRear
};

struct Input
{
    glm::vec3 eye{0.f};
    glm::vec3 rotation{0.f};
    float desiredDistance = 4.f;
    float radius = .18f;
};

struct State
{
    Mode requestedMode = Mode::FirstPerson;
    float currentDistance = 0.f;
    float fallbackClearSeconds = 0.f;
    bool nearWallFallback = false;
    bool initialized = false;
};

struct ClipResult
{
    float distance = 0.f;
    std::size_t queries = 0;
    bool hit = false;
    bool budgetExhausted = false;
};

struct Pose
{
    glm::vec3 position{0.f};
    glm::vec3 rotation{0.f};
    Mode effectiveMode = Mode::FirstPerson;
    float distance = 0.f;
    float distanceRatio = 0.f;
    std::size_t collisionQueries = 0;
    bool obstructionHit = false;
    bool collisionBudgetExhausted = false;
};

inline constexpr float DefaultDistance = 4.f;
inline constexpr float MaximumDistance = 8.f;
inline constexpr float DefaultRadius = .18f;
inline constexpr float MaximumRadius = .5f;
inline constexpr float CollisionPadding = .08f;
inline constexpr float FallbackEnterDistance = .65f;
inline constexpr float FallbackExitDistance = .85f;
inline constexpr float FallbackEnterRatio = .70f;
inline constexpr float FallbackExitRatio = .90f;
inline constexpr float FallbackReleaseSeconds = .10f;
inline constexpr float RecoveryRate = 12.f;
inline constexpr std::size_t MaximumVoxelQueries = 2048;

inline bool finite(const glm::vec3& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

inline glm::vec3 sanitizedRotation(const glm::vec3& rotation) noexcept
{
    if (!finite(rotation)) return glm::vec3(0.f);
    return glm::vec3(std::clamp(rotation.x, -89.f, 89.f),
                     std::remainder(rotation.y, 360.f), 0.f);
}

inline glm::vec3 forward(const glm::vec3& inputRotation) noexcept
{
    const glm::vec3 rotation = sanitizedRotation(inputRotation);
    const float yaw = glm::radians(rotation.y);
    const float pitch = glm::radians(rotation.x);
    const float horizontal = std::cos(pitch);
    return glm::normalize(glm::vec3(std::sin(yaw) * horizontal,
                                    -std::sin(pitch),
                                    -std::cos(yaw) * horizontal));
}

namespace Detail
{
inline float distance(const Input& input) noexcept
{
    return std::clamp(std::isfinite(input.desiredDistance)
                          ? input.desiredDistance
                          : DefaultDistance,
                      0.f, MaximumDistance);
}

inline float radius(const Input& input) noexcept
{
    return std::clamp(std::isfinite(input.radius) ? input.radius
                                                  : DefaultRadius,
                      0.f, MaximumRadius);
}

inline bool safeCoordinate(float value) noexcept
{
    constexpr double Margin = 4.;
    const double coordinate = static_cast<double>(value);
    return std::isfinite(value) &&
           coordinate >= static_cast<double>(std::numeric_limits<int>::min()) + Margin &&
           coordinate <= static_cast<double>(std::numeric_limits<int>::max()) - Margin;
}

inline bool segmentEntry(const glm::vec3& start, const glm::vec3& end,
                         const glm::vec3& minimum,
                         const glm::vec3& maximum,
                         float& entry) noexcept
{
    const glm::vec3 delta = end - start;
    float nearTime = 0.f;
    float farTime = 1.f;
    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::abs(delta[axis]) <= .000001f)
        {
            if (start[axis] < minimum[axis] ||
                start[axis] > maximum[axis]) return false;
            continue;
        }
        float first = (minimum[axis] - start[axis]) / delta[axis];
        float second = (maximum[axis] - start[axis]) / delta[axis];
        if (first > second) std::swap(first, second);
        nearTime = std::max(nearTime, first);
        farTime = std::min(farTime, second);
        if (nearTime > farTime) return false;
    }
    entry = std::clamp(nearTime, 0.f, 1.f);
    return farTime >= 0.f && nearTime <= 1.f;
}
} // namespace Detail

template <std::size_t QueryBudget = MaximumVoxelQueries,
          typename IsCollidable>
ClipResult clip(const Input& input, IsCollidable&& isCollidable)
{
    ClipResult result;
    const float desiredDistance = Detail::distance(input);
    result.distance = desiredDistance;
    if (!finite(input.eye) || desiredDistance <= 0.f)
    {
        result.distance = 0.f;
        return result;
    }

    const float cameraRadius = Detail::radius(input);
    const glm::vec3 end = input.eye - forward(input.rotation) * desiredDistance;
    const glm::vec3 minimum = glm::min(input.eye, end) -
        glm::vec3(cameraRadius);
    const glm::vec3 maximum = glm::max(input.eye, end) +
        glm::vec3(cameraRadius);
    if (!Detail::safeCoordinate(minimum.x) ||
        !Detail::safeCoordinate(minimum.y) ||
        !Detail::safeCoordinate(minimum.z) ||
        !Detail::safeCoordinate(maximum.x) ||
        !Detail::safeCoordinate(maximum.y) ||
        !Detail::safeCoordinate(maximum.z))
    {
        result.distance = 0.f;
        result.hit = true;
        result.budgetExhausted = true;
        return result;
    }

    const int minX = static_cast<int>(std::floor(minimum.x));
    const int minY = static_cast<int>(std::floor(minimum.y));
    const int minZ = static_cast<int>(std::floor(minimum.z));
    const int maxX = static_cast<int>(std::floor(maximum.x));
    const int maxY = static_cast<int>(std::floor(maximum.y));
    const int maxZ = static_cast<int>(std::floor(maximum.z));
    float earliest = 1.f;

    for (int x = minX; x <= maxX; ++x)
    {
        for (int y = minY; y <= maxY; ++y)
        {
            for (int z = minZ; z <= maxZ; ++z)
            {
                if (result.queries >= QueryBudget)
                {
                    result.distance = 0.f;
                    result.hit = true;
                    result.budgetExhausted = true;
                    return result;
                }
                ++result.queries;
                if (!isCollidable(x, y, z)) continue;

                const glm::vec3 blockMinimum(
                    static_cast<float>(x) - cameraRadius,
                    static_cast<float>(y) - cameraRadius,
                    static_cast<float>(z) - cameraRadius);
                const glm::vec3 blockMaximum(
                    static_cast<float>(x + 1) + cameraRadius,
                    static_cast<float>(y + 1) + cameraRadius,
                    static_cast<float>(z + 1) + cameraRadius);
                float entry = 1.f;
                if (Detail::segmentEntry(input.eye, end, blockMinimum,
                                         blockMaximum, entry))
                {
                    earliest = std::min(earliest, entry);
                    result.hit = true;
                }
            }
        }
    }

    if (result.hit)
    {
        result.distance = std::max(
            0.f, desiredDistance * earliest - CollisionPadding);
    }
    return result;
}

inline Mode advance(State& state, Mode requestedMode,
                    float targetDistance, float desiredDistance,
                    float deltaSeconds) noexcept
{
    targetDistance = std::clamp(
        std::isfinite(targetDistance) ? targetDistance : 0.f,
        0.f, MaximumDistance);
    desiredDistance = std::clamp(
        std::isfinite(desiredDistance) ? desiredDistance : DefaultDistance,
        0.f, MaximumDistance);
    state.currentDistance = std::clamp(
        std::isfinite(state.currentDistance) ? state.currentDistance : 0.f,
        0.f, MaximumDistance);
    state.fallbackClearSeconds = std::clamp(
        std::isfinite(state.fallbackClearSeconds)
            ? state.fallbackClearSeconds : 0.f,
        0.f, FallbackReleaseSeconds);
    const bool switched = !state.initialized ||
        state.requestedMode != requestedMode;
    state.requestedMode = requestedMode;
    state.initialized = true;

    if (requestedMode == Mode::FirstPerson)
    {
        state.currentDistance = 0.f;
        state.fallbackClearSeconds = 0.f;
        state.nearWallFallback = false;
        return Mode::FirstPerson;
    }

    targetDistance = std::min(targetDistance, desiredDistance);
    const float elapsed = std::clamp(
        std::isfinite(deltaSeconds) ? deltaSeconds : 0.f, 0.f, .1f);
    if (switched || targetDistance <= state.currentDistance)
    {
        // Entering the mode and approaching geometry must never spend a frame
        // on the unsafe side of a wall.
        state.currentDistance = targetDistance;
    }
    else
    {
        const float amount = 1.f - std::exp(-RecoveryRate * elapsed);
        state.currentDistance +=
            (targetDistance - state.currentDistance) * amount;
    }

    const float enterDistance = std::min(
        FallbackEnterDistance, desiredDistance * FallbackEnterRatio);
    const float exitDistance = std::min(
        FallbackExitDistance, desiredDistance * FallbackExitRatio);
    if (state.nearWallFallback)
    {
        if (targetDistance >= exitDistance && exitDistance > .000001f)
            state.fallbackClearSeconds = std::min(
                FallbackReleaseSeconds,
                state.fallbackClearSeconds + elapsed);
        else
            state.fallbackClearSeconds = 0.f;

        if (state.fallbackClearSeconds < FallbackReleaseSeconds)
            state.currentDistance = std::min(state.currentDistance, exitDistance);
        else if (state.currentDistance >= exitDistance)
        {
            // Leave fallback from the outer hysteresis boundary, then let the
            // normal recovery spring continue on following frames.
            state.currentDistance = exitDistance;
            state.nearWallFallback = false;
            state.fallbackClearSeconds = 0.f;
        }
    }
    else if (state.currentDistance <= enterDistance)
    {
        state.nearWallFallback = true;
        state.fallbackClearSeconds = 0.f;
    }
    return state.nearWallFallback ? Mode::FirstPerson
                                  : Mode::ThirdPersonRear;
}

template <typename IsCollidable>
Pose update(State& state, Mode requestedMode, const Input& input,
            float deltaSeconds, IsCollidable&& isCollidable)
{
    Pose pose;
    pose.position = finite(input.eye) ? input.eye : glm::vec3(0.f);
    pose.rotation = sanitizedRotation(input.rotation);
    if (requestedMode == Mode::FirstPerson || !finite(input.eye))
    {
        pose.effectiveMode = advance(
            state, Mode::FirstPerson, 0.f, 0.f, deltaSeconds);
        return pose;
    }

    const ClipResult clipped = clip(
        input, std::forward<IsCollidable>(isCollidable));
    const float desiredDistance = Detail::distance(input);
    pose.effectiveMode = advance(state, requestedMode, clipped.distance,
                                 desiredDistance, deltaSeconds);
    pose.distance = state.currentDistance;
    pose.distanceRatio = desiredDistance > .000001f
        ? std::clamp(pose.distance / desiredDistance, 0.f, 1.f) : 0.f;
    pose.collisionQueries = clipped.queries;
    pose.obstructionHit = clipped.hit;
    pose.collisionBudgetExhausted = clipped.budgetExhausted;
    if (pose.effectiveMode == Mode::ThirdPersonRear)
    {
        pose.position = input.eye - forward(pose.rotation) * pose.distance;
    }
    return pose;
}
} // namespace ThirdPersonCameraPresentation
