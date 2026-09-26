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
        T::update(state, T::Mode::ThirdPersonRear, base, .016f,
                  [](int, int, int) { return false; });
        bool queried = false;
        const auto first = T::update(state, T::Mode::FirstPerson, base, .016f,
            [&](int, int, int) { queried = true; return true; });
        require(!queried && first.effectiveMode == T::Mode::FirstPerson &&
                    close(first.distance, 0.f) && close(state.currentDistance, 0.f) &&
                    close(state.fallbackClearSeconds, 0.f) &&
                    !state.nearWallFallback,
                "first-person switch must reset state without world queries");
        const auto third = T::update(state, T::Mode::ThirdPersonRear, base, .016f,
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
        T::State state;
        std::size_t callbackCount = 0;
        const auto pose = T::update(state, T::Mode::ThirdPersonRear, maximum,
            .016f, [&](int, int, int) { ++callbackCount; return false; });
        require(!pose.collisionBudgetExhausted &&
                    pose.collisionQueries == callbackCount &&
                    pose.collisionQueries <= T::MaximumVoxelQueries,
                "sanitized maximum sweep must remain inside the query budget");
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
