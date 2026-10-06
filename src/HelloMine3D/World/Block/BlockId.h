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
    Torch = 21,
    OakPlank = 22,
    Cobblestone = 23,
    OakDoorClosed = 24,
    OakDoorOpen = 25,
    Crusher = 26,

    Snow = 27,
    Gravel = 28,
    Clay = 29,
    ForestFloor = 30,
    MossStone = 31,
    Silt = 32,

    StoneStep = 33,
    StoneWindowFrame = 34,
    StoneBrick = 35,
    StoneSlab = 36,
    StoneCornice = 37,
    ClayTileStep = 38,
    ClayTileEave = 39,
    TimberBeam = 40,
    TimberRailing = 41,
    StoneWindowSill = 42,
    StonePlanter = 43,
    Lantern = 44,

    NUM_TYPES
};

// Registered single-cell shape v2 identities; legacy metadata is unchanged.
inline bool isArchitecturalBlock(BlockId id) noexcept
{
    return id >= BlockId::StoneStep && id <= BlockId::Lantern;
}

#endif // BLOCKID_H_INCLUDED
