#include "../../src/HelloMine3D/World/Exploration/ExplorationNavigation.h"

#include <climits>
#include <limits>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* label)
{
    if (!condition) throw std::runtime_error(label);
}
}

int main()
{
    try {
        constexpr int dx[] = {0, 10, 10, 10, 0, -10, -10, -10};
        constexpr int dz[] = {-10, -10, 0, 10, 10, 10, 0, -10};
        for (int octant = 0; octant < 8; ++octant) {
            const auto bearing = ExplorationNavigation::toward(
                0, 0, dx[octant], dz[octant]);
            require(bearing.octant == octant &&
                    bearing.metres == (octant % 2 == 0 ? 10u : 14u),
                    "north-up direction or rounded distance differs");
        }
        for (int yaw = 0; yaw < 360; yaw += 45) {
            const auto heading = ExplorationNavigation::heading(float(yaw));
            const auto bearing = ExplorationNavigation::toward(0,0,
                int(std::round(heading.x * 100.f)),
                int(std::round(heading.z * 100.f)));
            require(bearing.octant == yaw / 45 &&
                    std::abs(std::hypot(heading.x, heading.z) - 1.f) < .0001f,
                "player heading disagrees with north-up map bearing");
        }
        const auto wrapped = ExplorationNavigation::heading(-270.f);
        const auto invalid = ExplorationNavigation::heading(
            std::numeric_limits<float>::quiet_NaN());
        require(std::abs(wrapped.x - 1.f) < .0001f &&
                invalid.x == 0.f && invalid.z == -1.f,
                "wrapped or invalid player yaw produced an invalid pointer");
        const auto same = ExplorationNavigation::toward(-18, 42, -18, 42);
        require(same.metres == 0 && same.octant == 0,
                "arrival changed direction or distance");
        const auto extreme = ExplorationNavigation::toward(
            INT_MIN, INT_MAX, INT_MAX, INT_MIN);
        require(extreme.octant == 1 && extreme.metres > 6000000000ull,
                "extreme old-world coordinates overflowed");
        require(!ExplorationNavigation::knownWaystoneTask(
                    "finale.defeat_brute", false) &&
                    !ExplorationNavigation::knownWaystoneTask(
                    "exploration.recover_waystone", true) &&
                    !ExplorationNavigation::knownWaystoneTask(
                    "exploration.restore_waystone", true),
                "unrevealed task acquired a generated destination");
        require(ExplorationNavigation::knownWaystoneTask(
                    "finale.prepare_ritual", true) &&
                    ExplorationNavigation::knownWaystoneTask(
                    "finale.defeat_brute", true) &&
                    ExplorationNavigation::knownWaystoneTask(
                    "finale.claim_reward", true),
                "known Waystone stage lost its observed destination");
        std::cout << "[EXPLORATION_NAVIGATION] checks=21 failures=0\n";
    }
    catch (const std::exception& error) {
        std::cerr << "[EXPLORATION_NAVIGATION] FAIL " << error.what() << '\n';
        return 1;
    }
}
