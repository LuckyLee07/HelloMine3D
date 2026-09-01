#include "SpatialInterest.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

SpatialInterestSnapshot SpatialInterestModel::build(
    const ChunkDemandSnapshot &demandSnapshot)
{
    std::unordered_map<VectorXZ, SpatialInterest> merged;
    for (const ChunkDemand &demand : demandSnapshot.demands) {
        const int radius = std::max(0, demand.radius);
        for (int x = demand.coord.x - radius;
             x <= demand.coord.x + radius; ++x) {
            for (int z = demand.coord.z - radius;
                 z <= demand.coord.z + radius; ++z) {
                SpatialInterest &interest = merged[{x, z}];
                interest.requiresResidentData = true;
                interest.reasonMask |=
                    ChunkDemandModel::reasonBit(demand.reason);

                const bool nearSource =
                    demand.reason == ChunkDemandReason::Player ||
                    demand.reason == ChunkDemandReason::Camera;
                interest.requiresNearRepresentation =
                    interest.requiresNearRepresentation || nearSource;

                const int distance = std::max(
                    std::abs(x - demand.coord.x),
                    std::abs(z - demand.coord.z));
                const bool simulationSource =
                    demand.reason == ChunkDemandReason::Player &&
                    distance <= SimulationRequestRadiusChunks;
                interest.requestsSimulation =
                    interest.requestsSimulation || simulationSource;
            }
        }
    }

    SpatialInterestSnapshot result;
    result.demandRevision = demandSnapshot.revision;
    result.cells.reserve(merged.size());
    for (const auto &entry : merged) {
        result.cells.push_back({entry.first, entry.second});
    }
    std::sort(result.cells.begin(), result.cells.end(),
              [](const SpatialInterestCell &left,
                 const SpatialInterestCell &right) {
                  if (left.coord.x != right.coord.x) {
                      return left.coord.x < right.coord.x;
                  }
                  return left.coord.z < right.coord.z;
              });
    return result;
}

SpatialInterest SpatialInterestModel::interestAt(
    const SpatialInterestSnapshot &snapshot,
    const VectorXZ &coord) noexcept
{
    const auto found = std::lower_bound(
        snapshot.cells.begin(), snapshot.cells.end(), coord,
        [](const SpatialInterestCell &cell, const VectorXZ &target) {
            return cell.coord.x < target.x ||
                   (cell.coord.x == target.x &&
                    cell.coord.z < target.z);
        });
    if (found == snapshot.cells.end() || !(found->coord == coord)) {
        return {};
    }
    return found->interest;
}

SpatialInterestClass SpatialInterestModel::classify(
    const SpatialInterest &interest) noexcept
{
    if (interest.requestsSimulation) {
        return SpatialInterestClass::SimulationRequested;
    }
    if (interest.requiresNearRepresentation) {
        return SpatialInterestClass::NearRepresentation;
    }
    if (interest.requiresResidentData) {
        return SpatialInterestClass::ResidentData;
    }
    return SpatialInterestClass::Outside;
}

const char *SpatialInterestModel::className(
    SpatialInterestClass value) noexcept
{
    switch (value) {
    case SpatialInterestClass::Outside:
        return "Outside";
    case SpatialInterestClass::ResidentData:
        return "ResidentData";
    case SpatialInterestClass::NearRepresentation:
        return "NearRepresentation";
    case SpatialInterestClass::SimulationRequested:
        return "SimulationRequested";
    }
    return "Unknown";
}

bool SpatialInterestModel::isValid(
    const SpatialInterest &interest) noexcept
{
    if (interest.requestsSimulation &&
        !interest.requiresNearRepresentation) {
        return false;
    }
    if (interest.requiresNearRepresentation &&
        !interest.requiresResidentData) {
        return false;
    }
    if (!interest.requiresResidentData && interest.reasonMask != 0) {
        return false;
    }
    return true;
}

SpatialInterestDebugStats SpatialInterestModel::debugStats(
    const SpatialInterestSnapshot &snapshot) noexcept
{
    SpatialInterestDebugStats stats;
    stats.demandRevision = snapshot.demandRevision;
    stats.totalCells = snapshot.cells.size();
    for (const SpatialInterestCell &cell : snapshot.cells) {
        stats.residentDataCells +=
            cell.interest.requiresResidentData ? 1u : 0u;
        stats.nearRepresentationCells +=
            cell.interest.requiresNearRepresentation ? 1u : 0u;
        stats.simulationRequestedCells +=
            cell.interest.requestsSimulation ? 1u : 0u;
    }
    return stats;
}
