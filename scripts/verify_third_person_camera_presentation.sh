#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIGURATION="${1:-Debug}"
case "$CONFIGURATION" in
    Debug|Release) ;;
    *) echo "Usage: $0 [Debug|Release] [new-output-directory]" >&2; exit 2 ;;
esac
LOG_DIR="${2:-$ROOT_DIR/build/third-person-camera-$(date +%Y%m%d%H%M%S)-$CONFIGURATION}"
if [[ -e "$LOG_DIR" ]]; then
    echo "Refusing to replace previous evidence: $LOG_DIR" >&2
    exit 2
fi
mkdir -p "$LOG_DIR"
HARNESS="$LOG_DIR/third_person_camera_test.cpp"
cat > "$HARNESS" <<'CPP'
#include "HelloMine3D/Presentation/ThirdPersonCameraPresentation.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>
#include <tuple>

namespace T = ThirdPersonCameraPresentation;
using Cell = std::tuple<int, int, int>;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

bool close(float first, float second, float epsilon = .001f)
{
    return std::abs(first - second) <= epsilon;
}

int main()
{
    int passed = 0;
    const T::Input base{{0.f, 2.f, 0.f}, {0.f, 0.f, 0.f}, 4.f, .18f};

    {
        T::State state;
        const auto pose = T::update(state, T::Mode::ThirdPersonRear, base,
                                    .016f, [](int, int, int) { return false; });
        require(pose.effectiveMode == T::Mode::ThirdPersonRear,
                "clear path must retain third person");
        require(close(pose.distance, 4.f) && close(pose.position.z, 4.f),
                "clear path must reach the desired rear position");
        require(!pose.obstructionHit && pose.collisionQueries > 0,
                "clear path must perform a bounded query sweep");
        ++passed;
    }

    {
        require(close(T::distanceForVerticalFov(90.f), 4.f) &&
                    close(T::distanceForVerticalFov(120.f), 2.4f) &&
                    close(T::distanceForVerticalFov(45.f), 6.f) &&
                    close(T::distanceForVerticalFov(
                        std::numeric_limits<float>::quiet_NaN()), 4.f),
                "FOV framing must keep the avatar readable with bounded fallback");
        ++passed;
    }

    {
        const float radius90 = T::radiusForProjection(90.f, 16.f / 9.f, .1f);
        const float radius120 = T::radiusForProjection(120.f, 16.f / 9.f, .1f);
        const float radiusWide = T::radiusForProjection(120.f, 3.f, .1f);
        const float radiusExtreme = T::radiusForProjection(120.f, 16.f, .1f);
        require(radius90 > .22f && radius90 < .26f &&
                    radius120 > .38f && radius120 < .40f &&
                    radiusWide > .57f && radiusWide < .59f &&
                    close(radiusExtreme, radiusWide) &&
                    T::projectionSupported(120.f, 16.f / 9.f, .1f) &&
                    T::projectionSupported(120.f, 3.f, .1f) &&
                    T::projectionSupported(120.0001f, 16.f / 9.f, .1f) &&
                    !T::projectionSupported(120.01f, 16.f / 9.f, .1f) &&
                    !T::projectionSupported(120.f, 3.01f, .1f) &&
                    !T::projectionSupported(120.f, 3.f, .11f) &&
                    !T::projectionSupported(120.f, 16.f, .1f) &&
                    close(T::radiusForProjection(
                              std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::quiet_NaN()),
                          radius90),
                "projection-aware radius must enclose supported near planes and reject unsafe extremes");
        ++passed;
    }

    {
        T::Input corner = base;
        corner.eye = {.7f, 2.f, 0.f};
        corner.radius = T::DefaultRadius;
        const auto narrow = T::clip(corner, [](int x, int y, int z) {
            return x == 1 && y == 2 && z == 3;
        });
        corner.radius = T::radiusForProjection(120.f, 16.f / 9.f, .1f);
        const auto projectionSafe = T::clip(
            corner, [](int x, int y, int z) {
                return x == 1 && y == 2 && z == 3;
            });
        require(!narrow.hit && projectionSafe.hit &&
                    projectionSafe.distance < corner.desiredDistance,
                "near-plane corner contact must clip before the camera view enters a wall");
        ++passed;
    }

    {
        T::Input invalid = base;
        invalid.shoulderOffset =
            std::numeric_limits<float>::quiet_NaN();
        invalid.verticalOffset =
            std::numeric_limits<float>::infinity();
        invalid.aimTargetDistance =
            std::numeric_limits<float>::quiet_NaN();
        T::State state;
        const auto pose = T::update(
            state, T::Mode::ThirdPersonRear, invalid, .016f,
            [](int, int, int) { return false; });
        require(T::finite(pose.position) && T::finite(pose.rotation) &&
                    close(pose.position.x, 0.f) &&
                    close(pose.position.y, invalid.eye.y) &&
                    close(pose.position.z, invalid.desiredDistance),
                "non-finite composition inputs must reduce to the safe rear camera");
        ++passed;
    }

    {
        T::Input shoulder = base;
        shoulder.shoulderOffset = T::DefaultShoulderOffset;
        shoulder.verticalOffset = T::DefaultVerticalOffset;
        shoulder.aimTargetDistance =
            T::DefaultAimConvergenceDistance;
        T::State state;
        const auto pose = T::update(
            state, T::Mode::ThirdPersonRear, shoulder, .016f,
            [](int, int, int) { return false; });
        require(close(pose.position.x, T::DefaultShoulderOffset) &&
                    close(pose.position.y,
                          shoulder.eye.y + T::DefaultVerticalOffset) &&
                    close(pose.position.z, shoulder.desiredDistance),
                "clear shoulder camera must use the full bounded composition");
        const glm::vec3 focus = shoulder.eye +
            T::forward(shoulder.rotation) *
                T::DefaultAimConvergenceDistance;
        const glm::vec3 expected = glm::normalize(focus - pose.position);
        require(glm::dot(T::forward(pose.rotation), expected) > .9999f &&
                    glm::dot(shoulder.eye - pose.position,
                             T::right(pose.rotation)) < -.25f,
                "render aim must converge on the logical ray while moving the avatar off-centre");

        for (const float targetDistance : {0.f, .35f, 1.f, 4.f, 6.f})
        {
            shoulder.aimTargetDistance = targetDistance;
            shoulder.aimTargetVisible = true;
            T::State targetState;
            const auto targeted = T::update(
                targetState, T::Mode::ThirdPersonRear, shoulder, .016f,
                [](int, int, int) { return false; });
            const glm::vec3 target = shoulder.eye +
                T::forward(shoulder.rotation) * targetDistance;
            const glm::vec3 targetDelta = target - targeted.position;
            const glm::vec3 cameraForward = T::forward(targeted.rotation);
            const glm::vec3 cameraRight = T::right(targeted.rotation);
            const glm::vec3 cameraUp = glm::normalize(
                glm::cross(cameraRight, cameraForward));
            const float depth = glm::dot(targetDelta, cameraForward);
            const float verticalScale = depth *
                std::tan(glm::radians(shoulder.verticalFovDegrees * .5f));
            const glm::vec2 expectedCrosshair(
                glm::dot(targetDelta, cameraRight) /
                    (verticalScale * shoulder.aspectRatio),
                glm::dot(targetDelta, cameraUp) / verticalScale);
            require(close(targeted.rotation.x, pose.rotation.x) &&
                        close(targeted.rotation.y, pose.rotation.y) &&
                        targeted.aimIndicatorVisible &&
                        close(targeted.aimIndicatorNdc.x,
                              std::clamp(expectedCrosshair.x, -1.f, 1.f)) &&
                        close(targeted.aimIndicatorNdc.y,
                              std::clamp(expectedCrosshair.y, -1.f, 1.f)),
                    "target depth must move the reticle without rotating the shoulder camera");
        }

        shoulder.rotation.y = 90.f;
        T::State yawState;
        const auto yawed = T::update(
            yawState, T::Mode::ThirdPersonRear, shoulder, .016f,
            [](int, int, int) { return false; });
        require(close(yawed.position.x, -shoulder.desiredDistance) &&
                    close(yawed.position.z, T::DefaultShoulderOffset),
                "shoulder offset must rotate with yaw without changing handedness");
        ++passed;
    }

    {
        T::Input pitched = base;
        pitched.shoulderOffset = T::DefaultShoulderOffset;
        pitched.verticalOffset = T::DefaultVerticalOffset;
        pitched.subjectBoundsEnabled = true;
        pitched.subjectMinimum = {-.3f, 0.f, -.3f};
        pitched.subjectMaximum = {.3f, 2.1f, .3f};
        for (const float pitch : {-82.f, 82.f})
        {
            pitched.rotation.x = pitch;
            T::State state;
            const auto pose = T::update(
                state, T::Mode::ThirdPersonRear, pitched, .016f,
                [](int, int, int) { return false; });
            require(pose.effectiveMode == T::Mode::ThirdPersonRear &&
                        T::finite(pose.position) &&
                        T::finite(pose.rotation) &&
                        close(glm::dot(pose.position - pitched.eye,
                                       T::right(pitched.rotation)),
                              T::DefaultShoulderOffset) &&
                        !pose.subjectObstruction,
                    "steep shoulder camera must remain finite and outside the subject");
        }
        ++passed;
    }

    {
        T::Input shoulder = base;
        shoulder.shoulderOffset = 1.f;
        T::State state;
        const std::set<Cell> shoulderWall{{1, 2, 3}};
        const auto pose = T::update(
            state, T::Mode::ThirdPersonRear, shoulder, .016f,
            [&](int x, int y, int z) {
                return shoulderWall.count({x, y, z}) != 0;
            });
        require(pose.obstructionHit &&
                    pose.distance < shoulder.desiredDistance &&
                    close(pose.position.x,
                          shoulder.shoulderOffset * pose.distance /
                              shoulder.desiredDistance),
                "shoulder-only wall must clip the diagonal sweep and scale the lateral offset");
        ++passed;
    }

    {
        for (float coordinate : {
                 static_cast<float>(std::numeric_limits<int>::max()),
                 static_cast<float>(std::numeric_limits<int>::min())})
        {
            T::Input extreme = base;
            extreme.eye.x = coordinate;
            std::size_t callbacks = 0;
            const auto clipped = T::clip(extreme,
                [&](int, int, int) { ++callbacks; return false; });
            require(clipped.hit && clipped.budgetExhausted &&
                        close(clipped.distance, 0.f) && clipped.queries == 0 &&
                        callbacks == 0,
                    "integer-edge coordinates must fail closed before conversion");
        }
        ++passed;
    }

    {
        std::size_t callbacks = 0;
        const auto clipped = T::clip<4>(base,
            [&](int, int, int) { ++callbacks; return false; });
        require(clipped.hit && clipped.budgetExhausted &&
                    close(clipped.distance, 0.f) && clipped.queries == 4 &&
                    callbacks == 4,
                "query budget exhaustion must fail closed before an extra callback");
        ++passed;
    }

    {
        T::State state;
        const std::set<Cell> wall{{0, 2, 2}};
        const auto pose = T::update(state, T::Mode::ThirdPersonRear, base,
            .016f, [&](int x, int y, int z) { return wall.count({x,y,z}) != 0; });
        require(pose.obstructionHit && pose.distance > 1.6f && pose.distance < 1.9f,
                "solid wall must clip the camera sphere before contact");
        ++passed;
    }

    {
        T::State state;
        std::size_t visited = 0;
        const auto pose = T::update(state, T::Mode::ThirdPersonRear, base,
            .016f, [&](int, int, int) { ++visited; return false; });
        require(!pose.obstructionHit && close(pose.distance, 4.f) &&
                    visited == pose.collisionQueries,
                "non-collidable cells must not obstruct the camera");
        ++passed;
    }

    {
        T::Input subject = base;
        subject.subjectBoundsEnabled = true;
        subject.subjectMinimum = {-.3f, 0.f, -.3f};
        subject.subjectMaximum = {.3f, 2.1f, .3f};
        T::State state;
        const auto pose = T::update(
            state, T::Mode::ThirdPersonRear, subject, .016f,
            [](int, int, int) { return false; });
        require(pose.effectiveMode == T::Mode::ThirdPersonRear &&
                    close(pose.distance, subject.desiredDistance) &&
                    !pose.subjectObstruction &&
                    !pose.subjectBoundsRejected,
                "valid subject bounds must preserve a clear horizontal camera");
        for (const float pitch : {-82.f, 82.f})
        {
            subject.rotation.x = pitch;
            T::State pitchedState;
            const auto pitched = T::update(
                pitchedState, T::Mode::ThirdPersonRear, subject, .016f,
                [](int, int, int) { return false; });
            require(pitched.effectiveMode == T::Mode::ThirdPersonRear &&
                        !pitched.subjectObstruction,
                    "clear large pitch must remain outside the subject");
        }
        ++passed;
    }

    {
        T::Input subject = base;
        subject.radius = T::MaximumRadius;
        subject.subjectBoundsEnabled = true;
        subject.subjectMinimum = {-.3f, 0.f, -.3f};
        subject.subjectMaximum = {.3f, 2.1f, .3f};
        T::State state;
        const std::set<Cell> wall{{0, 2, 1}};
        const auto pose = T::update(
            state, T::Mode::ThirdPersonRear, subject, .016f,
            [&](int x, int y, int z) {
                return wall.count({x, y, z}) != 0;
            });
        require(pose.obstructionHit && pose.subjectObstruction &&
                    pose.effectiveMode == T::Mode::FirstPerson &&
                    state.nearWallFallback && close(pose.distance, 0.f),
                "world clipping inside the subject must enter fallback");
        ++passed;
    }

    {
        T::Input steep = base;
        steep.rotation.x = -75.f;
        steep.subjectBoundsEnabled = true;
        steep.subjectMinimum = {-.3f, 0.f, -.3f};
        steep.subjectMaximum = {.3f, 2.1f, .3f};
        T::State state;
        const auto blocked = T::update(
            state, T::Mode::ThirdPersonRear, steep, .016f,
            [](int x, int y, int z) {
                return x == 0 && y == 1 && z == 0;
            });
        require(blocked.effectiveMode == T::Mode::FirstPerson &&
                    blocked.subjectObstruction,
                "steep camera clipped into the subject must fail to first person");

        T::Pose released;
        int clearFrames = 0;
        do
        {
            released = T::update(
                state, T::Mode::ThirdPersonRear, steep, .016f,
                [](int, int, int) { return false; });
            ++clearFrames;
        }
        while (released.effectiveMode == T::Mode::FirstPerson &&
               clearFrames < 60);
        const glm::vec3 expandedMinimum = steep.subjectMinimum -
            glm::vec3(steep.radius);
        const glm::vec3 expandedMaximum = steep.subjectMaximum +
            glm::vec3(steep.radius);
        const bool releasedInside =
            released.position.x >= expandedMinimum.x &&
            released.position.x <= expandedMaximum.x &&
            released.position.y >= expandedMinimum.y &&
            released.position.y <= expandedMaximum.y &&
            released.position.z >= expandedMinimum.z &&
            released.position.z <= expandedMaximum.z;
        require(released.effectiveMode == T::Mode::ThirdPersonRear &&
                    clearFrames > 1 && !releasedInside &&
                    released.distance > T::FallbackExitDistance,
                "steep fallback must release only beyond the subject bounds");
        const auto recovering = T::update(
            state, T::Mode::ThirdPersonRear, steep, .016f,
            [](int, int, int) { return false; });
        require(recovering.distance > released.distance &&
                    recovering.distance < steep.desiredDistance,
                "subject-safe release must continue smooth recovery");
        ++passed;
    }

    {
        for (int invalidCase = 0; invalidCase < 3; ++invalidCase)
        {
            T::Input invalid = base;
            invalid.subjectBoundsEnabled = true;
            invalid.subjectMinimum = {-.3f, 0.f, -.3f};
            invalid.subjectMaximum = {.3f, 2.1f, .3f};
            if (invalidCase == 0)
                invalid.subjectMinimum.x =
                    std::numeric_limits<float>::quiet_NaN();
            else if (invalidCase == 1)
                invalid.subjectMinimum.y = 3.f;
            else
                invalid.subjectMaximum.y = T::MaximumSubjectSpan + 1.f;
            T::State state;
            std::size_t callbacks = 0;
            const auto pose = T::update(
                state, T::Mode::ThirdPersonRear, invalid, .016f,
                [&](int, int, int) { ++callbacks; return false; });
            require(pose.effectiveMode == T::Mode::FirstPerson &&
                        pose.subjectBoundsRejected && callbacks == 0 &&
                        pose.collisionQueries == 0,
                    "invalid enabled subject bounds must fail closed before queries");
        }

        T::Input disabled = base;
        disabled.subjectMinimum.x =
            std::numeric_limits<float>::quiet_NaN();
        T::State state;
        std::size_t callbacks = 0;
        const auto pose = T::update(
            state, T::Mode::ThirdPersonRear, disabled, .016f,
            [&](int, int, int) { ++callbacks; return false; });
        require(pose.effectiveMode == T::Mode::ThirdPersonRear &&
                    !pose.subjectBoundsRejected && callbacks > 0 &&
                    callbacks == pose.collisionQueries,
                "disabled subject collision must ignore subject-bound data");
        ++passed;
    }

    {
        T::Input diagonal = base;
        diagonal.rotation.y = 45.f;
        T::State state;
        const std::set<Cell> corner{{-2, 2, 2}, {-1, 2, 2}};
        const auto pose = T::update(state, T::Mode::ThirdPersonRear, diagonal,
            .016f, [&](int x, int y, int z) { return corner.count({x,y,z}) != 0; });
        require(pose.obstructionHit && pose.distance < diagonal.desiredDistance,
                "expanded voxel AABBs must catch wall corners");
        ++passed;
    }

    {
        T::Input upward = base;
        upward.rotation.x = 45.f;
        T::State state;
        const std::set<Cell> ceiling{{0, 3, 1}, {-1, 3, 1}};
        const auto pose = T::update(state, T::Mode::ThirdPersonRear, upward,
            .016f, [&](int x, int y, int z) { return ceiling.count({x,y,z}) != 0; });
        require(pose.obstructionHit && pose.distance < upward.desiredDistance,
                "low ceiling must shorten an upward rear camera");
        ++passed;
    }

    {
        T::Input negative{{-10.5f, 2.f, -10.5f}, {0.f, 0.f, 0.f}, 4.f, .18f};
        T::State state;
        bool queriedNegative = false;
        const std::set<Cell> wall{{-11, 2, -9}};
        const auto pose = T::update(state, T::Mode::ThirdPersonRear, negative,
            .016f, [&](int x, int y, int z) {
                queriedNegative = queriedNegative || x < 0 || z < 0;
                return wall.count({x,y,z}) != 0;
            });
        require(queriedNegative && pose.obstructionHit && pose.distance < 2.f,
                "negative-coordinate voxels must preserve floor semantics");
        ++passed;
    }

    {
        T::State state;
        const auto clear = [](int, int, int) { return false; };
        const auto wall = [](int x, int y, int z) {
            return x == 0 && y == 2 && z == 2;
        };
        T::update(state, T::Mode::ThirdPersonRear, base, .016f, clear);
        const auto shortened = T::update(
            state, T::Mode::ThirdPersonRear, base, .016f, wall);
        const auto recovering = T::update(
            state, T::Mode::ThirdPersonRear, base, .016f, clear);
        require(shortened.distance < 1.9f,
                "new obstruction must shorten immediately");
        require(recovering.distance > shortened.distance && recovering.distance < 4.f,
                "cleared obstruction must recover smoothly instead of snapping");
        ++passed;
    }

    {
        T::State state;
        require(T::advance(state, T::Mode::ThirdPersonRear, .60f, 4.f, .016f) ==
                    T::Mode::FirstPerson,
                "near wall must enter first-person fallback");
        require(T::advance(state, T::Mode::ThirdPersonRear, 4.f, 4.f, .016f) ==
                    T::Mode::FirstPerson && state.nearWallFallback,
                "one clear frame must not release near-wall fallback");
        for (int i = 0; i < 20; ++i)
        {
            require(T::advance(state, T::Mode::ThirdPersonRear, .60f, 4.f,
                               .016f) == T::Mode::FirstPerson,
                    "alternating obstruction must keep fallback latched");
            require(T::advance(state, T::Mode::ThirdPersonRear, 4.f, 4.f,
                               .016f) == T::Mode::FirstPerson,
                    "alternating clear samples must not accumulate release time");
        }
        for (int i = 0; i < 30; ++i)
            T::advance(state, T::Mode::ThirdPersonRear, .75f, 4.f, .016f);
        require(state.nearWallFallback,
                "fallback must remain latched below the exit threshold");
        for (int i = 0; i < 30; ++i)
            T::advance(state, T::Mode::ThirdPersonRear, .90f, 4.f, .016f);
        require(!state.nearWallFallback && state.currentDistance >= .85f,
                "fallback must exit only after the upper hysteresis threshold");
        require(T::advance(state, T::Mode::ThirdPersonRear, .70f, 4.f, .016f) ==
                    T::Mode::ThirdPersonRear,
                "distance between thresholds must not re-enter fallback");
        require(T::advance(state, T::Mode::ThirdPersonRear, .60f, 4.f, .016f) ==
                    T::Mode::FirstPerson,
                "crossing the lower threshold must re-enter fallback");
        ++passed;
    }

    {
        T::State state;
        T::advance(state, T::Mode::ThirdPersonRear, .60f, 4.f, .016f);
        T::Mode mode = T::Mode::FirstPerson;
        int clearFrames = 0;
        while (mode == T::Mode::FirstPerson && clearFrames < 30)
        {
            mode = T::advance(state, T::Mode::ThirdPersonRear, 4.f, 4.f,
                              .016f);
            ++clearFrames;
        }
        require(mode == T::Mode::ThirdPersonRear && clearFrames > 1 &&
                    close(state.currentDistance, T::FallbackExitDistance),
                "stable clear path must release at the outer boundary after a hold");
        T::advance(state, T::Mode::ThirdPersonRear, 4.f, 4.f, .016f);
        require(state.currentDistance > T::FallbackExitDistance,
                "camera recovery must continue smoothly after fallback release");
        ++passed;
    }


    {
        T::State state;
        require(T::advance(state, T::Mode::ThirdPersonRear, .50f, .80f, .016f) ==
                    T::Mode::FirstPerson,
                "short-distance camera must still enter fallback near a wall");
        T::Mode mode = T::Mode::FirstPerson;
        for (int i = 0; i < 30; ++i)
            mode = T::advance(state, T::Mode::ThirdPersonRear, .80f, .80f,
                              .016f);
        require(mode == T::Mode::ThirdPersonRear && !state.nearWallFallback &&
                    state.currentDistance <= .80f,
                "short desired distance must recover without a permanent latch");
        ++passed;
    }

    {
        T::State state;
        T::Input switching = base;
        switching.shoulderOffset = T::DefaultShoulderOffset;
        switching.verticalOffset = T::DefaultVerticalOffset;
        T::update(state, T::Mode::ThirdPersonRear, switching, .016f,
                  [](int, int, int) { return false; });
        bool queried = false;
        const auto first = T::update(state, T::Mode::FirstPerson, switching, .016f,
            [&](int, int, int) { queried = true; return true; });
        require(!queried && first.effectiveMode == T::Mode::FirstPerson &&
                    close(first.distance, 0.f) && close(state.currentDistance, 0.f) &&
                    close(state.fallbackClearSeconds, 0.f) &&
                    !state.nearWallFallback,
                "first-person switch must reset state without world queries");
        const auto third = T::update(state, T::Mode::ThirdPersonRear, switching, .016f,
                                     [](int, int, int) { return false; });
        require(close(third.distance, 4.f),
                "returning to third person must initialize from the current safe target");
        ++passed;
    }

    {
        T::Input maximum = base;
        maximum.desiredDistance = 100.f;
        maximum.radius = 100.f;
        maximum.rotation = {35.264f, 45.f, 0.f};
        maximum.shoulderOffset = T::MaximumShoulderOffset;
        maximum.verticalOffset = T::MaximumVerticalOffset;
        maximum.aimTargetDistance =
            T::MaximumAimConvergenceDistance;
        maximum.subjectBoundsEnabled = true;
        maximum.subjectMinimum = {-.3f, 0.f, -.3f};
        maximum.subjectMaximum = {.3f, 2.1f, .3f};
        T::State state;
        std::size_t callbackCount = 0;
        const auto pose = T::update(state, T::Mode::ThirdPersonRear, maximum,
            .016f, [&](int, int, int) { ++callbackCount; return false; });
        require(!pose.collisionBudgetExhausted &&
                    pose.collisionQueries == callbackCount &&
                    pose.collisionQueries <= T::MaximumVoxelQueries &&
                    !pose.subjectBoundsRejected,
                "subject collision must not change the world-query budget");
        ++passed;
    }

    std::cout << "[THIRD_PERSON_CAMERA] passed=" << passed
              << " max_queries=" << T::MaximumVoxelQueries << '\n';
    return 0;
}
CPP

OPTIONS=(-std=c++17 -Wall -Wextra -Werror
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/external/glm")
if [[ "$CONFIGURATION" == Debug ]]; then
    OPTIONS+=(-O0 -g -fsanitize=undefined,float-cast-overflow
        -fno-sanitize-recover=all)
else
    OPTIONS+=(-O3 -DNDEBUG)
fi
shasum -a 256 \
    "$ROOT_DIR/src/HelloMine3D/Presentation/ThirdPersonCameraPresentation.h" \
    "$ROOT_DIR/scripts/verify_third_person_camera_presentation.sh" \
    "$HARNESS" > "$LOG_DIR/sources-sha256.txt"
clang++ "${OPTIONS[@]}" "$HARNESS" -o "$LOG_DIR/third_person_camera_test" \
    > "$LOG_DIR/build.log" 2>&1
"$LOG_DIR/third_person_camera_test" | tee "$LOG_DIR/test.log"
echo "[THIRD_PERSON_CAMERA] configuration=$CONFIGURATION logs=$LOG_DIR"
