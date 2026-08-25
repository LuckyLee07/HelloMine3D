#ifndef STRUCTUREPLANNING_H_INCLUDED
#define STRUCTUREPLANNING_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "../../../Maths/glm.h"

enum class StructureType : std::uint8_t {
    Waystone = 0
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
    int projectionPriority = 0;
    int selectedCandidate = -1;
    std::size_t plannedBlockCount = 0;
    std::uint64_t selectionHash = 0;
};

class DeterministicStructurePlanner {
  public:
    using SurfaceHeightSampler = std::function<int(int, int)>;

    static constexpr int WaystoneMinimumTerrainVersion = 2;
    static constexpr int WaystoneCellChunks = 4;
    static constexpr int WaystoneRadius = 2;
    static constexpr int WaystoneHeight = 6;
    static constexpr int WaystoneCandidateCount = 8;
    static constexpr int WaystoneProjectionPriority = 100;
    static constexpr int WaystoneMinimumAnchorSpacing = 7;
    static constexpr std::size_t WaystonePlannedBlockCount = 195;
    static constexpr std::size_t MaximumPlansPerChunk = 4;

    DeterministicStructurePlanner(int seed, int terrainGenerationVersion,
                                  SurfaceHeightSampler surfaceHeight);

    StructurePlanSnapshot planForCell(StructureType type, int cellX,
                                      int cellZ) const;
    std::vector<StructurePlanSnapshot> plansForChunk(int chunkX,
                                                     int chunkZ) const;

    static std::vector<StructurePlanSnapshot> resolveOverlaps(
        std::vector<StructurePlanSnapshot> candidates);

  private:
    StructurePlanSnapshot planWaystoneForCell(int cellX, int cellZ) const;

    int m_seed = 0;
    int m_terrainGenerationVersion = 0;
    SurfaceHeightSampler m_surfaceHeight;
};

#endif // STRUCTUREPLANNING_H_INCLUDED
