#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include "../../Block/BlockId.h"

// v28: local coordinates inside the existing v23 certified write columns.
// No sampler, mutable random stream, world access, new resource or wider reach.
namespace UndergroundPolish {
inline bool chamberAir(std::uint64_t key, int along, int lateral, int vertical) noexcept
{
    const int side = (key & 8u) ? -1 : 1;
    // An offset shoulder and a low rock shelf interrupt the symmetric ellipsoid.
    // The three-wide walking axes are restored by the existing dry-route pass.
    if (vertical >= 2 && lateral * side >= 3 && along >= -3) return false;
    if (vertical <= -1 && lateral * side <= -4 && along >= 1) return false;
    return true;
}
inline int riftCeiling(std::uint64_t key, int step, int lateral) noexcept
{
    constexpr int heights[6]{6,5,7,8,6,5};
    const int phase = static_cast<int>((key >> 4) % 6u);
    const int height = heights[(step / 4 + phase) % 6];
    return std::max(4, height - (lateral != 0 && height >= 7 ? 1 : 0));
}
inline int roomCeiling(bool mine, std::uint64_t key, int along, int lateral) noexcept
{
    const int side = (key & 8u) ? -1 : 1;
    const int edge = std::max(0, std::abs(lateral) - 2);
    const int back = along >= 3 ? 1 : 0;
    const int shoulder = !mine && lateral * side < -1 && along < 0 ? 1 : 0;
    return std::clamp(6 - edge - back - shoulder, mine ? 4 : 3, 6);
}
inline bool roomShelf(bool mine, int along, int lateral, int vertical) noexcept
{
    // A low wet ledge opposite the ore wall; never occupies the central aisle,
    // existing torches or the mine's chest and six timber uprights.
    return !mine && lateral <= -3 && along >= 1 && vertical == 0;
}
inline BlockId roomFloor(bool mine, std::uint64_t key, int along, int lateral) noexcept
{
    if (mine) {
        if (std::abs(lateral) <= 1)
            return along == -3 || along == 3 ? BlockId::OakPlank : BlockId::Gravel;
        if (lateral >= 2 && along <= -1) return BlockId::Cobblestone;
        return lateral <= -3 ? BlockId::Gravel : BlockId::Stone;
    }
    const int center = (key & 16u) ? 1 : -1;
    const int a = along - center, b = lateral + 3;
    // One coherent damp pocket, a silt core and its gravel fringe. Adjacent
    // blocks describe the same patch instead of independently choosing tiles.
    const int wetDistance = a*a + 2*b*b;
    if (wetDistance <= 2) return BlockId::Silt;
    if (wetDistance <= 15) return BlockId::MossStone;
    if (wetDistance <= 21) return BlockId::Gravel;
    return BlockId::Stone;
}
inline int timberHeight(int lateral) noexcept
{
    return std::abs(lateral) >= 4 ? 3 : std::abs(lateral) == 3 ? 4 : 5;
}
inline BlockId poolBank(int x, int z, std::uint64_t key) noexcept
{
    return x + 2*z + static_cast<int>((key >> 5) & 1u) > 0
        ? BlockId::Gravel : BlockId::Silt;
}
} // namespace UndergroundPolish
