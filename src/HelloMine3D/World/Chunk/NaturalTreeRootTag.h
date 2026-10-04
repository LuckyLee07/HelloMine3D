#pragma once

#include "../WorldConstants.h"

#include <cmath>
#include <cstdint>

// Presentation-only root coordinates relative to the owning section's x/z
// origin. Zero means ordinary geometry; a six-block crown can bring roots
// just outside the section into its resident tree faces.
namespace NaturalTreeRootTag {
inline constexpr int MinimumCoordinate = -6;
inline constexpr int MaximumCoordinate = CHUNK_SIZE - 1 + 6;
inline constexpr int AxisStride = 32;
inline constexpr std::uint16_t MaximumTag =
    1 + (MaximumCoordinate - MinimumCoordinate) * AxisStride +
    MaximumCoordinate - MinimumCoordinate;
static_assert(MaximumCoordinate - MinimumCoordinate < AxisStride,
              "Natural tree root coordinates must fit each five-bit axis");

constexpr std::uint16_t encode(int localX, int localZ) noexcept
{
    if (localX < MinimumCoordinate || localX > MaximumCoordinate ||
        localZ < MinimumCoordinate || localZ > MaximumCoordinate)
        return 0;
    return static_cast<std::uint16_t>(1 +
        (localX - MinimumCoordinate) * AxisStride +
        localZ - MinimumCoordinate);
}

constexpr int localX(std::uint16_t tag) noexcept
{
    return (static_cast<int>(tag) - 1) / AxisStride + MinimumCoordinate;
}

constexpr int localZ(std::uint16_t tag) noexcept
{
    return (static_cast<int>(tag) - 1) % AxisStride + MinimumCoordinate;
}

inline bool valid(float tag) noexcept
{
    if (!std::isfinite(tag) || tag < 0.f || tag > MaximumTag ||
        std::floor(tag) != tag)
        return false;
    if (tag == 0.f) return true;
    const auto encoded = static_cast<std::uint16_t>(tag);
    return localX(encoded) <= MaximumCoordinate &&
           localZ(encoded) <= MaximumCoordinate;
}
} // namespace NaturalTreeRootTag
