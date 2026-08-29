#include "TerrainAppearance.h"

#include <cstdint>

namespace {
int ecologyColumn(BlockId block, TerrainFaceKind face) noexcept
{
    switch (block) {
        case BlockId::Grass:
            if (face == TerrainFaceKind::Top) {
                return 0;
            }
            if (face == TerrainFaceKind::Side) {
                return 3;
            }
            return -1;
        case BlockId::OakLeaf:
            return 6;
        case BlockId::Water:
            return 9;
        case BlockId::TallGrass:
            return face == TerrainFaceKind::Resource ? 12 : -1;
        default:
            return -1;
    }
}

void mixHash(std::uint32_t &hash, std::uint32_t value) noexcept
{
    // FNV-1a over fixed-width integer values is fully defined for negative
    // world coordinates after the modulo conversion to uint32_t.
    for (int byte = 0; byte < 4; ++byte) {
        hash ^= (value >> (byte * 8)) & 0xffu;
        hash *= 16777619u;
    }
}

int patchCoordinate(int coordinate) noexcept
{
    const int quotient = coordinate / TerrainAppearance::VariantPatchSize;
    const int remainder = coordinate % TerrainAppearance::VariantPatchSize;
    return remainder < 0 ? quotient - 1 : quotient;
}
} // namespace

TerrainTileSelection TerrainAppearance::select(
    BlockId block, TerrainFaceKind face,
    const glm::ivec2 &baseCoordinates, TerrainBiome biome,
    int terrainSeed, const glm::ivec3 &worldPosition) noexcept
{
    TerrainTileSelection selection;
    selection.coordinates = baseCoordinates;
    selection.biome = biome;

    const int column = ecologyColumn(block, face);
    if (column < 0) {
        return selection;
    }

    selection.variant = coordinateVariant(
        terrainSeed, worldPosition, block);
    selection.coordinates = {
        column + static_cast<int>(selection.variant),
        ecologyRow(biome),
    };
    selection.mergeKey = static_cast<std::uint16_t>(
        1 + selection.coordinates.y * 16 + selection.coordinates.x);
    selection.ecologyTinted = true;
    return selection;
}

std::uint8_t TerrainAppearance::coordinateVariant(
    int terrainSeed, const glm::ivec3 &worldPosition,
    BlockId block) noexcept
{
    std::uint32_t hash = 2166136261u;
    mixHash(hash, static_cast<std::uint32_t>(terrainSeed));
    // A small coordinate cell avoids a noisy one-tile checkerboard and keeps
    // greedy meshing useful inside a deliberately shared appearance patch.
    // Floor division is explicit so negative and positive world quadrants use
    // the same four-block contract around cell boundaries.
    mixHash(hash, static_cast<std::uint32_t>(
        patchCoordinate(worldPosition.x)));
    mixHash(hash, static_cast<std::uint32_t>(
        patchCoordinate(worldPosition.y)));
    mixHash(hash, static_cast<std::uint32_t>(
        patchCoordinate(worldPosition.z)));
    mixHash(hash, static_cast<std::uint32_t>(block));

    // Final avalanche avoids visible low-bit patterns when coordinates change
    // by one while retaining a stable, allocation-free result.
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16;
    return static_cast<std::uint8_t>(
        hash % static_cast<std::uint32_t>(VariantsPerEcology));
}

bool TerrainAppearance::isEcologyTinted(
    BlockId block, TerrainFaceKind face) noexcept
{
    return ecologyColumn(block, face) >= 0;
}

int TerrainAppearance::ecologyRow(TerrainBiome biome) noexcept
{
    switch (biome) {
        case TerrainBiome::Desert:
            return EcologyRowBase;
        case TerrainBiome::Grassland:
            return EcologyRowBase + 1;
        case TerrainBiome::LightForest:
            return EcologyRowBase + 2;
        case TerrainBiome::TemperateForest:
            return EcologyRowBase + 3;
        case TerrainBiome::Ocean:
            return EcologyRowBase + 4;
        case TerrainBiome::Mountain:
            return EcologyRowBase + 3;
    }
    return EcologyRowBase + 1;
}
