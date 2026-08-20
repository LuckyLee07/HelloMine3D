#ifndef BLOCKID_H_INCLUDED
#define BLOCKID_H_INCLUDED

#include <cstdint>

using Block_t = uint8_t;
using BlockMetadata_t = uint8_t;

/// @brief Known block ID types used in game.
enum class BlockId : Block_t {
    Air = 0,
    Grass = 1,
    Dirt = 2,
    Stone = 3,
    OakBark = 4,
    OakLeaf = 5,
    Sand = 6,
    Water = 7,
    Cactus = 8,
    Rose = 9,
    TallGrass = 10,
    DeadShrub = 11,
    CoalOre = 12,
    IronOre = 13,
    Glass = 14,
    GlassBorderless = 15,
    Chest = 16,
    WheatCrop = 17,
    Workbench = 18,
    Furnace = 19,
    WaystoneCore = 20,

    NUM_TYPES
};

#endif // BLOCKID_H_INCLUDED
