#include "../../src/HelloMine3D/World/Exploration/ExplorationNavigation.h"

#include <climits>
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
        const auto same = ExplorationNavigation::toward(-18, 42, -18, 42);
        require(same.metres == 0 && same.octant == 0,
                "arrival changed direction or distance");
        const auto extreme = ExplorationNavigation::toward(
            INT_MIN, INT_MAX, INT_MAX, INT_MIN);
        require(extreme.octant == 1 && extreme.metres > 6000000000ull,
                "extreme old-world coordinates overflowed");
        std::cout << "[EXPLORATION_NAVIGATION] checks=10 failures=0\n";
    }
    catch (const std::exception& error) {
        std::cerr << "[EXPLORATION_NAVIGATION] FAIL " << error.what() << '\n';
        return 1;
    }
}
