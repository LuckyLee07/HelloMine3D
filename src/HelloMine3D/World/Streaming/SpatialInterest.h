#ifndef SPATIALINTEREST_H_INCLUDED
#define SPATIALINTEREST_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../Chunk/ChunkDemand.h"

enum class SpatialInterestClass : std::uint8_t {
    Outside,
    ResidentData,
    NearRepresentation,
    SimulationRequested,
};

constexpr std::size_t SpatialInterestClassCount = 4;

struct SpatialInterest {
    bool requiresResidentData = false;
    bool requiresNearRepresentation = false;
    bool requestsSimulation = false;
    std::uint32_t reasonMask = 0;
};

struct SpatialInterestCell {
    VectorXZ coord{0, 0};
    SpatialInterest interest;
};

struct SpatialInterestSnapshot {
    std::uint64_t demandRevision = 0;
    std::vector<SpatialInterestCell> cells;
};

struct SpatialInterestDebugStats {
    std::uint64_t demandRevision = 0;
    std::size_t totalCells = 0;
    std::size_t residentDataCells = 0;
    std::size_t nearRepresentationCells = 0;
    std::size_t simulationRequestedCells = 0;
};

class SpatialInterestModel {
  public:
    static constexpr int SimulationRequestRadiusChunks = 2;

    static SpatialInterestSnapshot build(
        const ChunkDemandSnapshot &demandSnapshot);
    static SpatialInterest interestAt(
        const SpatialInterestSnapshot &snapshot,
        const VectorXZ &coord) noexcept;
    static SpatialInterestClass classify(
        const SpatialInterest &interest) noexcept;
    static const char *className(SpatialInterestClass value) noexcept;
    static bool isValid(const SpatialInterest &interest) noexcept;
    static SpatialInterestDebugStats debugStats(
        const SpatialInterestSnapshot &snapshot) noexcept;
};

#endif // SPATIALINTEREST_H_INCLUDED
