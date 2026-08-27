#ifndef TERRAINAPPEARANCE_H_INCLUDED
#define TERRAINAPPEARANCE_H_INCLUDED

#include "../../Maths/glm.h"
#include "../Generation/Terrain/TerrainGenerator.h"
#include "BlockId.h"

#include <cstdint>

enum class TerrainFaceKind : std::uint8_t {
    Bottom,
    Top,
    Side,
    Resource,
};

struct TerrainTileSelection {
    glm::ivec2 coordinates{0};
    TerrainBiome biome = TerrainBiome::Grassland;
    std::uint8_t variant = 0;
    std::uint16_t mergeKey = 0;
    bool ecologyTinted = false;
};

/// Selects world-only ecology tiles without changing block, save, terrain or
/// vertex identities. HUD/held item paths continue to use the base block tile.
class TerrainAppearance {
  public:
    static constexpr int ContractVersion = 1;
    static constexpr int VariantsPerEcology = 3;
    static constexpr int VariantPatchSize = 4;
    static constexpr int EcologyRowBase = 3;

    static TerrainTileSelection select(
        BlockId block, TerrainFaceKind face,
        const glm::ivec2 &baseCoordinates, TerrainBiome biome,
        int terrainSeed, const glm::ivec3 &worldPosition) noexcept;

    static std::uint8_t coordinateVariant(
        int terrainSeed, const glm::ivec3 &worldPosition,
        BlockId block) noexcept;

    static bool isEcologyTinted(BlockId block,
                                TerrainFaceKind face) noexcept;
    static int ecologyRow(TerrainBiome biome) noexcept;
};

#endif // TERRAINAPPEARANCE_H_INCLUDED
