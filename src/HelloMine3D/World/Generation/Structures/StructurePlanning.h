#ifndef STRUCTUREPLANNING_H_INCLUDED
#define STRUCTUREPLANNING_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "../../../Item/Material.h"
#include "../../../Maths/glm.h"
#include "../Terrain/TerrainGenerator.h"

enum class StructureType : std::uint8_t {
    Waystone = 0,
    Ruin = 1,
    RaiderCamp = 2
};

const char *structureTypeName(StructureType type) noexcept;

struct StructureInstanceKey {
    StructureType type = StructureType::Waystone;
    int terrainGenerationVersion = 0;
    int cellX = 0;
    int cellZ = 0;

    bool operator==(const StructureInstanceKey &other) const noexcept;
};

struct StructureFootprint {
    int minimumX = 0;
    int maximumX = -1;
    int minimumY = 0;
    int maximumY = -1;
    int minimumZ = 0;
    int maximumZ = -1;

    bool valid() const noexcept;
    int width() const noexcept;
    int height() const noexcept;
    int depth() const noexcept;
    bool overlapsHorizontal(const StructureFootprint &other) const noexcept;
    bool overlaps(const StructureFootprint &other) const noexcept;
    bool overlapsChunk(int chunkX, int chunkZ) const noexcept;
};

struct StructurePlanSnapshot {
    StructureInstanceKey key;
    bool valid = false;
    glm::ivec3 anchor{0};
    StructureFootprint footprint;
    TerrainBiome biome = TerrainBiome::Ocean;
    int projectionPriority = 0;
    int selectedCandidate = -1;
    std::size_t plannedBlockCount = 0;
    std::uint64_t selectionHash = 0;
    bool hasChest = false;
    glm::ivec3 chestPosition{0};
};

struct StructureLootEntry {
    Material::ID materialId = Material::ID::Nothing;
    int amount = 0;

    bool operator==(const StructureLootEntry &other) const noexcept;
};

struct StructureLootSnapshot {
    StructureInstanceKey key;
    bool valid = false;
    glm::ivec3 chestPosition{0};
    std::vector<StructureLootEntry> entries;
    std::uint64_t selectionHash = 0;
};

StructureLootSnapshot
structureLootForPlan(const StructurePlanSnapshot &plan);

class DeterministicStructurePlanner {
  public:
    using SurfaceHeightSampler = std::function<int(int, int)>;
    using BiomeSampler = std::function<TerrainBiome(int, int)>;

    static constexpr int WaystoneMinimumTerrainVersion = 2;
    static constexpr int ExplorationSiteMinimumTerrainVersion = 3;
    static constexpr int WaystoneCellChunks = 4;
    static constexpr int WaystoneRadius = 2;
    static constexpr int WaystoneHeight = 6;
    static constexpr int WaystoneCandidateCount = 8;
    static constexpr int WaystoneProjectionPriority = 100;
    static constexpr int WaystoneMinimumAnchorSpacing = 7;
    static constexpr std::size_t WaystonePlannedBlockCount = 195;
    static constexpr int SiteCellChunks = 4;
    static constexpr int SiteCandidateCount = 8;
    static constexpr int SiteEdgeInset = 7;
    static constexpr int MaximumSiteRelief = 2;
    static constexpr int RuinRadius = 4;
    static constexpr int RuinHeight = 6;
    static constexpr int RuinProjectionPriority = 80;
    static constexpr std::size_t RuinPlannedBlockCount = 790;
    static constexpr int CampRadiusX = 5;
    static constexpr int CampRadiusZ = 4;
    static constexpr int CampHeight = 5;
    static constexpr int CampProjectionPriority = 60;
    static constexpr std::size_t CampPlannedBlockCount = 905;
    static constexpr int MaximumHorizontalRadius = CampRadiusX;
    static constexpr std::size_t MaximumPlansPerChunk = 4;

    DeterministicStructurePlanner(int seed, int terrainGenerationVersion,
                                  SurfaceHeightSampler surfaceHeight,
                                  BiomeSampler biome = {});

    StructureType selectedStructureTypeForCell(int cellX,
                                               int cellZ) const;
    StructurePlanSnapshot planForCell(StructureType type, int cellX,
                                      int cellZ) const;
    std::vector<StructurePlanSnapshot> plansForChunk(int chunkX,
                                                     int chunkZ) const;

    static std::vector<StructurePlanSnapshot> resolveOverlaps(
        std::vector<StructurePlanSnapshot> candidates);

  private:
    StructurePlanSnapshot planWaystoneForCell(int cellX, int cellZ) const;
    StructurePlanSnapshot planExplorationSiteForCell(
        StructureType type, int cellX, int cellZ) const;

    int m_seed = 0;
    int m_terrainGenerationVersion = 0;
    SurfaceHeightSampler m_surfaceHeight;
    BiomeSampler m_biome;
};

#endif // STRUCTUREPLANNING_H_INCLUDED
