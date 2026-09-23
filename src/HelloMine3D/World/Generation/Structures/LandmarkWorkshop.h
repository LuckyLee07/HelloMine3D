#ifndef HELLOMINE3D_LANDMARK_WORKSHOP_H
#define HELLOMINE3D_LANDMARK_WORKSHOP_H

#include "LandmarkApproach.h"
#include "../../WorldCoordinates.h"
#include "../../WorldConstants.h"
#include <algorithm>
#include <cstdlib>

namespace LandmarkWorkshop {

struct Site {
    bool valid = false;
    LandmarkApproach::Style style = LandmarkApproach::Style::Open;
    int side = 1;
    int minimumX = 0;
    int minimumZ = 0;
    int baseY = 0;

    int worldX(int across) const noexcept
    {
        return side > 0 ? minimumX + across : minimumX + 2 - across;
    }
    int worldZ(int depth) const noexcept { return minimumZ + depth; }
    int machineX() const noexcept { return worldX(1); }
    int machineZ() const noexcept { return worldZ(2); }
};

template<class HeightAt, class BiomeAt>
Site select(const StructurePlanSnapshot &plan, HeightAt heightAt,
            BiomeAt biomeAt)
{
    if (!plan.valid) return {};
    const auto style = LandmarkApproach::styleFor(plan, biomeAt);
    if (style == LandmarkApproach::Style::Open) return {};
    const int minimumZ = plan.footprint.minimumZ - 4;
    if (WorldCoordinates::floorDiv(minimumZ, CHUNK_SIZE) !=
        WorldCoordinates::floorDiv(minimumZ + 2, CHUNK_SIZE)) return {};
    const int preferred = (plan.selectionHash & 1ull) != 0 ? -1 : 1;
    for (const int side : {preferred, -preferred}) {
        const int minimumX = side > 0
            ? plan.anchor.x + 2 : plan.anchor.x - 4;
        if (WorldCoordinates::floorDiv(minimumX, CHUNK_SIZE) !=
            WorldCoordinates::floorDiv(minimumX + 2, CHUNK_SIZE)) continue;
        int low = 255, high = 0;
        bool dry = true;
        for (int x = minimumX; x <= minimumX + 2; ++x)
            for (int z = minimumZ; z <= minimumZ + 2; ++z) {
                const int y = heightAt(x, z);
                dry &= y >= WATER_LEVEL + 2 && y <= 251 &&
                    std::abs(y - plan.anchor.y) <= 4;
                low = std::min(low, y);
                high = std::max(high, y);
            }
        if (dry && high - low <= 1)
            return {true, style, side, minimumX, minimumZ, high};
    }
    return {};
}

inline bool clearsTreeSource(const Site &site, int x, int z) noexcept
{
    return site.valid && x >= site.minimumX - 3 &&
           x <= site.minimumX + 5 && z >= site.minimumZ - 3 &&
           z <= site.minimumZ + 5;
}

inline BlockId machine(LandmarkApproach::Style style) noexcept
{
    switch (style) {
        case LandmarkApproach::Style::Forest: return BlockId::Chest;
        case LandmarkApproach::Style::Riverbank: return BlockId::Furnace;
        case LandmarkApproach::Style::Highland: return BlockId::Crusher;
        case LandmarkApproach::Style::Open: return BlockId::Air;
    }
    return BlockId::Air;
}

inline BlockId blockAt(LandmarkApproach::Style style,
                       int across, int depth, int level) noexcept
{
    if (style == LandmarkApproach::Style::Open) return BlockId::Air;
    if (level == 0) {
        if (style == LandmarkApproach::Style::Forest)
            return across == 1 && depth == 1
                ? BlockId::ForestFloor : BlockId::MossStone;
        if (style == LandmarkApproach::Style::Riverbank)
            return depth == 0 ? BlockId::Gravel
                : (across == 1 ? BlockId::Silt : BlockId::Clay);
        return across == 1 ? BlockId::Gravel : BlockId::Stone;
    }
    if (level == 1 && across == 1 && depth == 2)
        return machine(style);
    const bool post = (across == 2 && (depth == 0 || depth == 2)) ||
                      (across == 0 && depth == 2);
    if ((level == 1 || level == 2) && post)
        return style == LandmarkApproach::Style::Forest
            ? BlockId::OakBark
            : style == LandmarkApproach::Style::Riverbank
                ? BlockId::Clay : BlockId::Stone;
    if (level == 3 && (depth == 2 || across == 2))
        return style == LandmarkApproach::Style::Riverbank
            ? BlockId::Clay : BlockId::OakPlank;
    return BlockId::Air;
}

} // namespace LandmarkWorkshop

#endif
