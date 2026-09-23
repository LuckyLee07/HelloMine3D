#ifndef EXPLORATIONNAVIGATION_H_INCLUDED
#define EXPLORATIONNAVIGATION_H_INCLUDED

#include <cmath>
#include <cstdint>

// A north-up bearing to a location the player has already chosen or found.
// World coordinates are widened before subtraction so old extreme-coordinate
// saves cannot overflow the HUD direction or distance calculation.
namespace ExplorationNavigation {
struct Bearing {
    std::uint64_t metres = 0;
    // Clockwise from north: N, NE, E, SE, S, SW, W, NW.
    int octant = 0;
};

inline Bearing toward(int fromX, int fromZ, int toX, int toZ) noexcept
{
    const std::int64_t dx = static_cast<std::int64_t>(toX) - fromX;
    const std::int64_t dz = static_cast<std::int64_t>(toZ) - fromZ;
    if (dx == 0 && dz == 0) return {};
    const double distance = std::hypot(static_cast<double>(dx),
                                       static_cast<double>(dz));
    constexpr double pi = 3.14159265358979323846;
    const double angle = std::atan2(static_cast<double>(dx),
                                    -static_cast<double>(dz));
    const int octant = static_cast<int>(std::floor(
        angle * (4.0 / pi) + .5));
    return {static_cast<std::uint64_t>(std::llround(distance)),
            (octant + 8) % 8};
}
} // namespace ExplorationNavigation

#endif // EXPLORATIONNAVIGATION_H_INCLUDED
