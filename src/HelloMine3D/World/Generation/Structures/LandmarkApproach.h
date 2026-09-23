#ifndef HELLOMINE3D_LANDMARK_APPROACH_H
#define HELLOMINE3D_LANDMARK_APPROACH_H

#include "StructurePlanning.h"
#include "../../Block/BlockId.h"
#include <cstdlib>
#include <initializer_list>

namespace LandmarkApproach {

// The approach is outside the v20 building footprint. A six-block walk-in
// fits the existing nine-block structure-query halo, including tree crowns.
inline constexpr int Length = 6;
inline constexpr int HalfWidth = 1;
inline constexpr int TreeClearance = 3;

enum class Style { Open, Forest, Riverbank, Highland };

inline bool isRiver(TerrainBiome biome) noexcept
{
    return biome == TerrainBiome::River || biome == TerrainBiome::Lake ||
           biome == TerrainBiome::Wetland;
}

template<class BiomeAt>
Style styleFor(const StructurePlanSnapshot &plan, BiomeAt biomeAt)
{
    if (plan.biome == TerrainBiome::LightForest ||
        plan.biome == TerrainBiome::TemperateForest)
        return Style::Forest;
    if (plan.biome == TerrainBiome::Mountain ||
        plan.biome == TerrainBiome::RockPlateau || plan.anchor.y >= 105)
        return Style::Highland;
    // A camp or Waystone on dry valley ground can still face a real watercourse.
    // Sample along the actual entrance axis rather than inventing a river label.
    for (int distance : {1, 3, Length})
        if (isRiver(biomeAt(plan.anchor.x,
                            plan.footprint.minimumZ - distance)))
            return Style::Riverbank;
    return Style::Open;
}

inline bool onWalkIn(const StructurePlanSnapshot &plan, int x, int z) noexcept
{
    const int distance = plan.footprint.minimumZ - z;
    return plan.valid && distance >= 1 && distance <= Length &&
           std::abs(x - plan.anchor.x) <= HalfWidth;
}

inline bool clearsTreeSource(const StructurePlanSnapshot &plan,
                             int x, int z) noexcept
{
    if (!plan.valid) return false;
    const int distance = plan.footprint.minimumZ - z;
    return distance >= 1 - TreeClearance &&
           distance <= Length + TreeClearance &&
           std::abs(x - plan.anchor.x) <= HalfWidth + TreeClearance;
}

inline BlockId surface(Style style, int distance, int lateral) noexcept
{
    const bool centre = lateral == 0;
    switch (style) {
        case Style::Forest:
            return centre && (distance == 2 || distance == 5)
                ? BlockId::MossStone : BlockId::ForestFloor;
        case Style::Riverbank:
            return centre ? BlockId::Gravel
                : (distance % 2 == 0 ? BlockId::Clay : BlockId::Silt);
        case Style::Highland:
            return centre ? BlockId::Gravel : BlockId::Stone;
        case Style::Open:
            return centre && distance % 2 == 0
                ? BlockId::Cobblestone : BlockId::Dirt;
    }
    return BlockId::Dirt;
}

inline BlockId shoulderMarker(Style style, int distance,
                              int lateral) noexcept
{
    if (std::abs(lateral) != HalfWidth ||
        (distance != 2 && distance != 5)) return BlockId::Air;
    switch (style) {
        case Style::Forest: return BlockId::OakPlank;
        case Style::Riverbank: return BlockId::Clay;
        case Style::Highland: return BlockId::Stone;
        case Style::Open: return BlockId::OakBark;
    }
    return BlockId::Air;
}

} // namespace LandmarkApproach

#endif
