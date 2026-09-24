#ifndef EXPLORATIONNAVIGATION_H_INCLUDED
#define EXPLORATIONNAVIGATION_H_INCLUDED

#include <cmath>
#include <cstdint>
#include <string_view>

// A north-up bearing to a location the player has already chosen or found.
// World coordinates are widened before subtraction so old extreme-coordinate
// saves cannot overflow the HUD direction or distance calculation.
namespace ExplorationNavigation {
struct Bearing {
    std::uint64_t metres = 0;
    // Clockwise from north: N, NE, E, SE, S, SW, W, NW.
    int octant = 0;
};

// Only stages tied to the Waystone the player actually used can point to its
// runtime anchor. Earlier exploration goals have no authoritative location.
inline bool knownWaystoneTask(std::string_view objectiveId,
                              bool anchorKnown) noexcept
{
    return anchorKnown &&
        (objectiveId == "finale.prepare_ritual" ||
         objectiveId == "finale.activate_waystone" ||
         objectiveId == "finale.defeat_stalkers" ||
         objectiveId == "finale.defeat_brute" ||
         objectiveId == "finale.claim_reward");
}

struct Heading { float x = 0.f; float z = -1.f; };

// Player yaw is degrees clockwise from north, matching the render camera.
inline Heading heading(float yawDegrees) noexcept
{
    if (!std::isfinite(yawDegrees)) return {};
    constexpr double radians = 3.14159265358979323846 / 180.;
    const double angle = std::remainder(double(yawDegrees), 360.) * radians;
    return {static_cast<float>(std::sin(angle)),
            static_cast<float>(-std::cos(angle))};
}

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
