#pragma once

#include "LandmarkExpedition.h"

// v27 changes only the existing bounded blueprint. Candidate identity, loot,
// the entrance axis, footprint, workshops and approaches keep their v26 rules.
namespace LandmarkPolish {
inline BlockId masonry(const StructurePlanSnapshot& plan, BlockId block,
                       int x, int y, int z) noexcept
{
    if (block != BlockId::Stone && block != BlockId::Cobblestone) return block;
    const bool forest = plan.biome == TerrainBiome::LightForest ||
                        plan.biome == TerrainBiome::TemperateForest;
    // Broad ground-contact patches rather than per-block random speckling.
    if (forest && y == 2 && (z >= 1 || x <= -2)) return BlockId::MossStone;
    return block;
}

inline BlockId blockAt(const StructurePlanSnapshot& plan, int x, int y, int z)
{
    const BlockId prior = LandmarkExpedition::blockAt(plan, x, y, z);
    // Existing resources and interactions have absolute priority.
    if (prior == BlockId::Chest || prior == BlockId::WaystoneCore ||
        prior == BlockId::IronOre || prior == BlockId::CoalOre ||
        prior == BlockId::Glass || y <= 0) return prior;
    if ((plan.selectionHash & 1ull) != 0) x = -x;
    const int ax = std::abs(x);
    const int layout = LandmarkExpedition::layoutFor(plan);
    const auto stone = [&](BlockId id = BlockId::Stone) {
        return masonry(plan, id, x, y, z);
    };

    if (plan.key.type == StructureType::RaiderCamp) {
        // A clear timber threshold reads from outside both camp layouts.
        // The central opening retains three empty blocks above its low step.
        if (z == -4 && ax <= 2 && y == 4) return BlockId::OakPlank;
        if (z == -4 && x == 0 && y == 5) return BlockId::OakBark;
        if (y == 1 && ax <= 1 && z >= -3 && z <= 0)
            return z == -2 || z == 0 ? BlockId::Cobblestone : BlockId::Gravel;
        // Keep the original floor beneath both bays and the low rear store.
        if (y == 1) return prior;
        if (layout == 0 && ax >= 2 && ax <= 4 && z >= -1 && z <= 3) {
            // Two ridged shelters with an open central aisle. End posts join
            // their actual eave, not an invisible old sloped roof.
            const int roof = ax == 3 ? 5 : 4;
            if (y == roof || (ax == 3 && y == 4 && (z == -1 || z == 3)))
                return BlockId::OakPlank;
            if ((z == -1 || z == 3) && (ax == 2 || ax == 4) && y < roof)
                return BlockId::OakBark;
            // Low stores at the rear leave the walk-through bay open.
            if (x == -3 && z == 3 && y == 2) return BlockId::OakBark;
            return BlockId::Air;
        }
        if (layout == 1) {
            // Exposed deck beams and a supported front corner clarify the
            // lookout without obstructing its three original stair treads.
            if (x == 5 && z == -1 && y <= 4) return BlockId::OakBark;
            if (x >= 3 && x <= 5 && z == 3 && y == 3) return BlockId::OakBark;
            if (x == -3 && z == 3 && y == 2) return BlockId::OakBark;
        }
        return prior;
    }

    if (plan.key.type == StructureType::Ruin) {
        // The central route is ground structure, not a painted floating prop.
        if (y == 1 && ax <= 1 && z >= -3 && z <= -1)
            return x == 0 ? BlockId::Gravel : BlockId::Cobblestone;
        if (layout == 0) {
            // Rear-left remnants make a sheltered alcove connected to the
            // courtyard. A broken parapet gives the far wall uneven mass.
            if (x >= -4 && x <= -2 && z >= 2 && z <= 4 && y == 5)
                return stone(BlockId::Cobblestone);
            if (z == 4 && (x == -3 || x == -2) && y == 6)
                return stone();
        } else {
            // One surviving half-portal makes the courtyard entrance legible;
            // its missing right span keeps a visibly broken silhouette.
            if (z == -3 && x == -2 && y <= 4) return stone();
            if (z == -3 && x >= -2 && x <= 0 && y == 5)
                return stone(BlockId::Cobblestone);
            if (z == -3 && x == 2 && y <= 3) return stone();
        }
        return masonry(plan, prior, x, y, z);
    }

    if (plan.key.type == StructureType::Waystone) {
        if (layout == 0) {
            // Outboard rear piers frame the core while the front remains open.
            if (ax == 2 && z == 1 && y <= 5) return stone();
            if (ax == 2 && z == 1 && y == 6) return stone(BlockId::Cobblestone);
            if (z == 1 && ax <= 2 && y == 5) return stone();
        } else {
            // Lower the front piers asymmetrically to expose the core from the
            // approach. The tall rear portal remains fully supported.
            if (z == -2 && ax == 2 && y > (x < 0 ? 3 : 4)) return BlockId::Air;
            if (z == 2 && ax <= 1 && y == 6) return stone(BlockId::Cobblestone);
        }
        return masonry(plan, prior, x, y, z);
    }
    return prior;
}
}
