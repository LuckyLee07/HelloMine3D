#ifndef HELLOMINE3D_LANDMARK_ARCHITECTURE_H
#define HELLOMINE3D_LANDMARK_ARCHITECTURE_H

#include "StructurePlanning.h"
#include "../../Block/BlockId.h"
#include <cstdlib>

namespace LandmarkArchitecture {
// One bounded blueprint cell per write. Axes are relative to the plan anchor;
// negative Z is the entrance. Mirroring does not move the chest or entry.
inline BlockId blockAt(const StructurePlanSnapshot &plan, int x, int y, int z)
{
    if ((plan.selectionHash & 1ull) != 0) { x = -x; }
    const int ax = std::abs(x), az = std::abs(z);
    const bool camp = plan.key.type == StructureType::RaiderCamp;
    const BlockId masonry = y == 2 ? BlockId::Cobblestone : BlockId::Stone;
    const BlockId campGround = plan.biome == TerrainBiome::Desert
        ? BlockId::Sand : BlockId::Dirt;
    if (y <= 0) {
        if (camp && y == 0 && (ax == 5 || az == 4) &&
            plan.biome != TerrainBiome::Desert) { return BlockId::Grass; }
        return camp ? campGround : BlockId::Stone;
    }

    if (plan.key.type == StructureType::Waystone) {
        if (y == 1) {
            // Four low approach steps around the raised central pedestal.
            return ((ax == 2 && az <= 1) || (az == 2 && ax <= 1))
                ? BlockId::Air : masonry;
        }
        if (y == 2 && ax + az == 1) { return BlockId::CoalOre; }
        if (x == 0 && z == 0) {
            if (y == 2) { return BlockId::IronOre; }
            if (y == 3) { return BlockId::WaystoneCore; }
            if (y == 4) { return BlockId::Glass; }
            if (y <= 6) { return BlockId::Stone; }
        }
        // Rear buttresses frame the luminous core without hiding its front.
        if (ax == 1 && z == 1 && y <= 4) { return masonry; }
        if (z == 1 && ax <= 1 && y == 5) { return BlockId::Stone; }
        if (ax == 2 && az == 2 && y == 2) { return BlockId::Cobblestone; }
        return BlockId::Air;
    }

    if (plan.key.type == StructureType::Ruin) {
        if (y == 1) { return z == -4 && ax <= 1 ? BlockId::Air : masonry; }
        if (x == 0 && z == 0 && y == 2) { return BlockId::Chest; }
        if (ax == 2 && az == 2 && y == 2) { return BlockId::IronOre; }
        // A supported portal and an incomplete rear wall give the ruin a
        // legible entrance and an asymmetric skyline instead of four posts.
        if (z == -3 && ((ax == 2 && y <= 5) ||
                        (ax <= 2 && y == 5) || (x == 0 && y == 6))) {
            return masonry;
        }
        if (z == 4) {
            if (y == 3 && (x == -2 || x == -1 || x == 1 || x == 2)) {
                return BlockId::Glass;
            }
            const int top = x <= 1 ? 4 : (x == 2 ? 3 : 2);
            if (y <= top) { return masonry; }
        }
        if (x == -4 && y <= (z >= 1 ? 4 : (z == -4 ? 3 : 2))) { return masonry; }
        if (x == 4 && y <= (z >= 2 ? 3 : 2)) { return masonry; }
        if (z == -4 && ax >= 3 && y == 2) { return masonry; }
        return BlockId::Air;
    }

    if (camp) {
        if (y == 1) {
            if (z == -4 && ax <= 1) { return BlockId::Air; }
            if ((ax == 5 || az == 4) && !(ax == 5 && az == 4) &&
                !(ax == 2 && z == -4)) { return BlockId::Air; }
            return ax <= 1 ? BlockId::Stone : campGround;
        }
        if (x == 0 && z == 1 && y == 2) { return BlockId::Chest; }
        // Two lean-to shelters leave the centre open; the lower eaves are at
        // the perimeter, so the central aisle retains full head clearance.
        if (ax >= 2 && ax <= 4 && z >= -1 && z <= 3) {
            const int roof = ax == 2 ? 5 : 4;
            if (y == roof) { return BlockId::OakPlank; }
            if ((z == -1 || z == 3) && (ax == 2 || ax == 4) && y < roof) {
                return BlockId::OakBark;
            }
        }
        if (x == -3 && z == -2 && y == 2) { return BlockId::CoalOre; }
        if (y == 2 && ((x == -2 && z == -2) || (x == -4 && z == -2) ||
                       (x == -3 && (z == -1 || z == -3)))) { return BlockId::Stone; }
        if (((ax == 5 && az == 4) || (ax == 2 && z == -4)) && y == 2) {
            return BlockId::OakBark;
        }
        if (ax == 2 && z == -4 && y == 3) { return BlockId::OakBark; }
        return BlockId::Air;
    }
    return BlockId::Air;
}
} // namespace LandmarkArchitecture
#endif
