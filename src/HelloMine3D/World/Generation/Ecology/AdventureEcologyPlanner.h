#pragma once

#include "../Terrain/LocalTerrainPlanner.h"
#include "../../Block/ChunkBlock.h"

enum class AdventureTreeKind : std::uint8_t { None, Oak, Birch, Spruce, Willow, Cactus, Palm };

// v18 composes regional ecology over the unchanged v17 height/water plan.
// Pure signed-coordinate queries; no resident chunks or mutable random stream.
class AdventureEcologyPlanner {
  public:
    static constexpr int TreeCellSize = 7;
    struct Sample {
        TerrainFoundation::Column column;
        AdventureRegion region = AdventureRegion::Meadow;
        double grove = 0;
        double moisture = 0;
        double snowLine = 128;
        bool shore = false;
    };
    struct Tree {
        AdventureTreeKind kind = AdventureTreeKind::None;
        int randomSeed = 0;
        int height = 0;
        int stature = 0; // v24: 0 low edge, 1 transitional, 2 grove interior.
    };
    explicit AdventureEcologyPlanner(int seed,
        int version = AdventureEcologyTerrainGenerationVersion) noexcept
        : m_seed(seed), m_water(seed,version), m_local(seed,version),
          m_banks(version >= WaterbankPolishTerrainGenerationVersion),
          m_relief(version >= LocalReliefTerrainGenerationVersion),
          m_polished(version >= VegetationPolishTerrainGenerationVersion) {}
    // v18-v24 retain the v17 water shape; v25 adds local relief to height.
    // Pure shape queries can avoid computing grove, patch and snow
    // fields while full ecology sampling still owns the surface material.
    TerrainFoundation::Column sampleWaterColumn(int x, int z) const noexcept;
    Sample sample(int x, int z) const noexcept;
    bool treeAnchor(int x, int z) const noexcept;
    Tree tree(int x, int z, const Sample &sample) const noexcept;
    struct GroundCover {
        BlockId id; BlockMetadata_t metadata;
        GroundCover(BlockId block = BlockId::Air, BlockMetadata_t data = 0) : id(block), metadata(data) {}
    };
    GroundCover groundCover(int x, int z, const Sample &sample) const noexcept;
    static bool supportsPlant(BlockId ground, BlockId plant) noexcept;

  private:
    std::uint64_t hash(std::int64_t x, std::int64_t z, std::uint64_t salt) const noexcept;
    double noise(int x, int z, double scale, std::uint64_t salt) const noexcept;
    int m_seed = 0;
    AdventureWaterPlanner m_water;
    LocalTerrainPlanner m_local;
    bool m_banks = false;
    bool m_relief = false;
    bool m_polished = false;
};
