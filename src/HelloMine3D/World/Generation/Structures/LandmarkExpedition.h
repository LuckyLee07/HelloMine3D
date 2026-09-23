#ifndef HELLOMINE3D_LANDMARK_EXPEDITION_H
#define HELLOMINE3D_LANDMARK_EXPEDITION_H

#include <cstdlib>

#include "LandmarkArchitecture.h"

namespace LandmarkExpedition {

// The location, footprint, chest and Waystone core stay fixed across both
// layouts. Only terrain v20 calls this blueprint; v1-v19 keep their bytes.
inline int layoutFor(const StructurePlanSnapshot &plan) noexcept
{
    return static_cast<int>((plan.selectionHash >> 7) & 1ull);
}

inline BlockId blockAt(const StructurePlanSnapshot &plan,
                       int x, int y, int z)
{
    if (layoutFor(plan) == 0) {
        // Open a real standing square in front of the old-style pedestal.
        // Move that one ore to a supported side corner; reward count stays 4.
        if (plan.key.type == StructureType::Waystone && y == 2) {
            if (x == 0 && z == -1) return BlockId::Air;
            const int cornerX = (plan.selectionHash & 1ull) != 0 ? 1 : -1;
            if (x == cornerX && z == -1) return BlockId::CoalOre;
        }
        return LandmarkArchitecture::blockAt(plan, x, y, z);
    }

    // Mirror the entire second plan without changing its front entrance.
    if ((plan.selectionHash & 1ull) != 0) x = -x;
    const int ax = std::abs(x);
    const int az = std::abs(z);
    const BlockId masonry = y == 2 ? BlockId::Cobblestone : BlockId::Stone;

    if (plan.key.type == StructureType::Waystone) {
        if (y <= 0) return BlockId::Stone;
        if (y == 1) return ax == 2 || az == 2
            ? BlockId::Stone : BlockId::Cobblestone;
        if (x == 0 && z == 0) {
            if (y == 2) return BlockId::IronOre;
            if (y == 3) return BlockId::WaystoneCore;
            if (y == 4) return BlockId::Glass;
            return y <= 6 ? BlockId::Stone : BlockId::Air;
        }
        if (y == 2 && ax == 1 && az == 1) return BlockId::CoalOre;
        // Four separated lookout piers and a rear lintel leave the core
        // visible and the front two-block approach open.
        if (ax == 2 && az == 2 && y <= 5) return masonry;
        if (z == 2 && ax <= 2 && y == 5) return BlockId::Stone;
        if (ax == 2 && z == 0 && y <= 3) return masonry;
        return BlockId::Air;
    }

    if (plan.key.type == StructureType::Ruin) {
        if (y <= 0) return BlockId::Stone;
        if (y == 1) return z == -4 && ax <= 1
            ? BlockId::Air : masonry;
        if (x == 0 && z == 0 && y == 2) return BlockId::Chest;
        if (ax == 2 && az == 2 && y == 2) return BlockId::IronOre;
        // A roofed side chamber and a broken open courtyard replace the
        // frontal portal. The central aisle and chest remain walkable.
        if (x == 3 && z >= -1 && z <= 4 && y <= (z > 1 ? 5 : 3))
            return masonry;
        if (z == 4 && x >= 1 && x <= 4 && y <= 4) return masonry;
        if (x >= 2 && x <= 4 && z >= 2 && z <= 4 && y == 5)
            return BlockId::Cobblestone;
        if (x == 1 && z == 2 && y <= 4) return masonry;
        if (x == 1 && z == 3 && y == 4) return BlockId::Stone;
        if (x == -4 && z >= -2 && z <= 2 && y <= (z >= 0 ? 4 : 3))
            return masonry;
        if (z == 2 && x >= -4 && x <= -2 && y <= 3) return masonry;
        if (z == -3 && ax == 3 && y <= 3) return masonry;
        return BlockId::Air;
    }

    if (plan.key.type == StructureType::RaiderCamp) {
        const BlockId ground = plan.biome == TerrainBiome::Desert
            ? BlockId::Sand : BlockId::Dirt;
        if (y <= 0) return ground;
        if (y == 1) {
            if (z == -4 && ax <= 1) return BlockId::Air;
            return ax <= 1 && z < 1 ? BlockId::Stone : ground;
        }
        if (x == 0 && z == 1 && y == 2) return BlockId::Chest;
        if (x == -3 && z == -2 && y == 2) return BlockId::CoalOre;
        // The front stair climbs to a roofless lookout on the right; a low
        // shelter on the left keeps a clear ground-level route to the chest.
        if (x == 2 && z == -2 && y == 2) return BlockId::OakPlank;
        if (x == 3 && z == -2 && y <= 3) return BlockId::OakPlank;
        if (x == 4 && z == -2 && y <= 4) return BlockId::OakPlank;
        if (x >= 2 && x <= 5 && z >= -1 && z <= 3 && y == 4)
            return BlockId::OakPlank;
        if ((x == 2 || x == 5) && (z == 0 || z == 3) && y <= 3)
            return BlockId::OakBark;
        if ((x == 5 || z == 3) && x >= 2 && z >= -1 && y == 5)
            return BlockId::OakBark;
        if (x >= -4 && x <= -2 && z >= 0 && z <= 3) {
            if (y == 5 || (x == -4 && y == 4)) return BlockId::OakPlank;
            if ((x == -4 || x == -2) && (z == 0 || z == 3) && y <= 4)
                return BlockId::OakBark;
        }
        if (ax == 2 && z == -4 && y >= 2 && y <= 3)
            return BlockId::OakBark;
        return BlockId::Air;
    }
    return BlockId::Air;
}

} // namespace LandmarkExpedition

#endif
