#include "ChunkRuntime.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>

#include "../../Core/Camera.h"
#include "../../Diagnostics/RuntimeProfiler.h"
#include "../../Physics/AABB.h"
#include "../WorldCoordinates.h"
#include "ChunkMeshBuilder.h"
#include "ChunkUpdatePlanner.h"

namespace
{
    int chunkDistanceSquared(const VectorXZ &chunk, const VectorXZ &center)
    {
        const int dx = chunk.x - center.x;
        const int dz = chunk.z - center.z;
        return dx * dx + dz * dz;
    }

    int chunkDistanceManhattan(const VectorXZ &chunk,
                               const VectorXZ &center)
    {
        return std::abs(chunk.x - center.x) +
               std::abs(chunk.z - center.z);
    }

    float angularDifference(float left, float right)
    {
        const float difference = std::fmod(std::abs(left - right), 360.f);
        return std::min(difference, 360.f - difference);
    }
}

ChunkRuntime::ChunkRuntime(ChunkManager &chunkManager,
                           std::mutex &worldMutex, int renderDistance)
    : m_chunkManager(chunkManager)
    , m_worldMutex(worldMutex)
    , m_renderDistance(renderDistance)
{
}

ChunkRuntime::~ChunkRuntime()
{
    stopLoader();
}

std::size_t
ChunkRuntime::IVec3Hash::operator()(const glm::ivec3 &value) const noexcept
{
    std::size_t seed = 0;
    seed ^= std::hash<int>{}(value.x) + 0x9e3779b9u + (seed << 6) +
            (seed >> 2);
    seed ^= std::hash<int>{}(value.y) + 0x9e3779b9u + (seed << 6) +
            (seed >> 2);
    seed ^= std::hash<int>{}(value.z) + 0x9e3779b9u + (seed << 6) +
            (seed >> 2);
    return seed;
}

std::vector<VectorXZ>
ChunkRuntime::planMeshWork(const VectorXZ &center, int radius, int sectionY,
                           const ViewFrustum *frustum)
{
    radius = std::max(0, radius);
    sectionY = std::max(0, sectionY);

    struct PrioritisedChunk {
        VectorXZ position;
        bool inFrustum = false;
    };

    std::vector<PrioritisedChunk> prioritised;
    const int diameter = radius * 2 + 1;
    prioritised.reserve(static_cast<std::size_t>(diameter * diameter));

    for (int x = center.x - radius; x <= center.x + radius; ++x) {
        for (int z = center.z - radius; z <= center.z + radius; ++z) {
            bool inFrustum = false;
            if (frustum != nullptr) {
                AABB sectionBounds{
                    glm::vec3(static_cast<float>(CHUNK_SIZE))};
                sectionBounds.update(
                    {static_cast<float>(x * CHUNK_SIZE),
                     static_cast<float>(sectionY * CHUNK_SIZE),
                     static_cast<float>(z * CHUNK_SIZE)});
                inFrustum = frustum->isBoxInFrustum(sectionBounds);
            }
            prioritised.push_back({{x, z}, inFrustum});
        }
    }

    std::sort(prioritised.begin(), prioritised.end(),
              [&](const PrioritisedChunk &left,
                  const PrioritisedChunk &right) {
                  if (left.inFrustum != right.inFrustum) {
                      return left.inFrustum;
                  }

                  const int leftDistance =
                      chunkDistanceSquared(left.position, center);
                  const int rightDistance =
                      chunkDistanceSquared(right.position, center);
                  if (leftDistance != rightDistance) {
                      return leftDistance < rightDistance;
                  }

                  const int leftManhattan =
                      chunkDistanceManhattan(left.position, center);
                  const int rightManhattan =
                      chunkDistanceManhattan(right.position, center);
                  if (leftManhattan != rightManhattan) {
                      return leftManhattan < rightManhattan;
                  }

                  if (left.position.x != right.position.x) {
                      return left.position.x < right.position.x;
                  }
                  return left.position.z < right.position.z;
              });

    std::vector<VectorXZ> result;
    result.reserve(prioritised.size());
    for (const PrioritisedChunk &chunk : prioritised) {
        result.push_back(chunk.position);
    }
    return result;
}

std::vector<ChunkDemandTarget> ChunkRuntime::planDemandWork(
    const ChunkDemandSnapshot &snapshot, int sectionY,
    const ViewFrustum *frustum, const VectorXZ &movementOrigin,
    const VectorXZ &movementDirection)
{
    sectionY = std::max(0, sectionY);
    std::unordered_map<VectorXZ, ChunkDemandTarget> merged;
    for (const ChunkDemand &demand : snapshot.demands) {
        const int radius = std::max(0, demand.radius);
        for (int x = demand.coord.x - radius;
             x <= demand.coord.x + radius; ++x) {
            for (int z = demand.coord.z - radius;
                 z <= demand.coord.z + radius; ++z) {
                const VectorXZ coord{x, z};
                const int dx = coord.x - demand.coord.x;
                const int dz = coord.z - demand.coord.z;
                const int distanceSquared = dx * dx + dz * dz;
                const int distanceManhattan =
                    std::abs(dx) + std::abs(dz);

                auto inserted = merged.emplace(
                    coord,
                    ChunkDemandTarget{
                        coord, ChunkDemandModel::reasonBit(demand.reason),
                        demand.priority, demand.epoch, distanceSquared,
                        distanceManhattan, false, 1});
                ChunkDemandTarget &target = inserted.first->second;
                if (!inserted.second) {
                    target.reasonMask |=
                        ChunkDemandModel::reasonBit(demand.reason);
                    target.priority =
                        std::max(target.priority, demand.priority);
                    target.newestEpoch =
                        std::max(target.newestEpoch, demand.epoch);
                    if (distanceSquared < target.distanceSquared ||
                        (distanceSquared == target.distanceSquared &&
                         distanceManhattan <
                             target.distanceManhattan)) {
                        target.distanceSquared = distanceSquared;
                        target.distanceManhattan = distanceManhattan;
                    }
                }
            }
        }
    }

    std::vector<ChunkDemandTarget> result;
    result.reserve(merged.size());
    for (auto &entry : merged) {
        ChunkDemandTarget &target = entry.second;
        if (frustum != nullptr) {
            AABB sectionBounds{
                glm::vec3(static_cast<float>(CHUNK_SIZE))};
            sectionBounds.update(
                {static_cast<float>(target.coord.x * CHUNK_SIZE),
                 static_cast<float>(sectionY * CHUNK_SIZE),
                 static_cast<float>(target.coord.z * CHUNK_SIZE)});
            target.inFrustum = frustum->isBoxInFrustum(sectionBounds);
        }

        const long long relativeX =
            static_cast<long long>(target.coord.x - movementOrigin.x);
        const long long relativeZ =
            static_cast<long long>(target.coord.z - movementOrigin.z);
        const long long motionDot =
            relativeX * movementDirection.x +
            relativeZ * movementDirection.z;
        target.motionRank = motionDot > 0 ? 2 : motionDot < 0 ? 0 : 1;
        result.push_back(target);
    }

    std::sort(result.begin(), result.end(),
              [](const ChunkDemandTarget &left,
                 const ChunkDemandTarget &right) {
                  if (left.priority != right.priority) {
                      return left.priority > right.priority;
                  }
                  if (left.inFrustum != right.inFrustum) {
                      return left.inFrustum;
                  }
                  if (left.motionRank != right.motionRank) {
                      return left.motionRank > right.motionRank;
                  }
                  if (left.newestEpoch != right.newestEpoch) {
                      return left.newestEpoch > right.newestEpoch;
                  }
                  if (left.distanceSquared != right.distanceSquared) {
                      return left.distanceSquared < right.distanceSquared;
                  }
                  if (left.distanceManhattan !=
                      right.distanceManhattan) {
                      return left.distanceManhattan <
                             right.distanceManhattan;
                  }
                  if (left.coord.x != right.coord.x) {
                      return left.coord.x < right.coord.x;
                  }
                  return left.coord.z < right.coord.z;
              });
    return result;
}

std::vector<glm::ivec3> ChunkRuntime::planSectionMeshUploads(
    const std::vector<glm::ivec3> &readySections,
    const VectorXZ &origin, std::size_t budget)
{
    std::vector<glm::ivec3> selected = readySections;
    std::sort(selected.begin(), selected.end(),
              [&origin](const glm::ivec3 &left,
                        const glm::ivec3 &right) {
                  const VectorXZ leftChunk{left.x, left.z};
                  const VectorXZ rightChunk{right.x, right.z};
                  const int leftSquared =
                      chunkDistanceSquared(leftChunk, origin);
                  const int rightSquared =
                      chunkDistanceSquared(rightChunk, origin);
                  if (leftSquared != rightSquared) {
                      return leftSquared < rightSquared;
                  }
                  const int leftManhattan =
                      chunkDistanceManhattan(leftChunk, origin);
                  const int rightManhattan =
                      chunkDistanceManhattan(rightChunk, origin);
                  if (leftManhattan != rightManhattan) {
                      return leftManhattan < rightManhattan;
                  }
                  if (left.x != right.x) {
                      return left.x < right.x;
                  }
                  if (left.z != right.z) {
                      return left.z < right.z;
                  }
                  return left.y < right.y;
              });
    if (selected.size() > budget) {
        selected.resize(budget);
    }
    return selected;
}

void ChunkRuntime::setInitialLoadCenter(const glm::vec3 &position) noexcept
{
    const VectorXZ center = WorldCoordinates::getChunkXZ(
        WorldCoordinates::toBlockCoord(position.x),
        WorldCoordinates::toBlockCoord(position.z));
    {
        std::lock_guard<std::mutex> lock(m_demandMutex);
        m_demandModel.refresh(ChunkDemandReason::Player, center,
                              std::max(1, m_renderDistance.load()));
        m_playerDemandCoord = center;
        m_playerMovement = {0, 0};
        m_playerDemandPublished = true;
        refreshSpatialInterestLocked();
        invalidateWorldJobs();
    }
    m_demandSectionY.store(std::max(
        0, WorldCoordinates::toBlockCoord(position.y) / CHUNK_SIZE));
}

void ChunkRuntime::updateLoadCenter(const glm::vec3 &playerPosition,
                                    const Camera &camera)
{
    const VectorXZ playerChunk = WorldCoordinates::getChunkXZ(
        WorldCoordinates::toBlockCoord(playerPosition.x),
        WorldCoordinates::toBlockCoord(playerPosition.z));
    const VectorXZ cameraChunk = WorldCoordinates::getChunkXZ(
        WorldCoordinates::toBlockCoord(camera.position.x),
        WorldCoordinates::toBlockCoord(camera.position.z));
    const int sectionY = std::max(
        0, WorldCoordinates::toBlockCoord(camera.position.y) / CHUNK_SIZE);
    const int radius = std::max(1, m_renderDistance.load());
    {
        std::lock_guard<std::mutex> lock(m_demandMutex);
        const std::uint64_t previousRevision =
            m_demandModel.snapshot().revision;
        m_demandModel.advanceEpoch();
        if (m_playerDemandPublished &&
            !(playerChunk == m_playerDemandCoord)) {
            m_playerMovement = {
                playerChunk.x - m_playerDemandCoord.x,
                playerChunk.z - m_playerDemandCoord.z};
        }
        m_playerDemandCoord = playerChunk;
        m_playerDemandPublished = true;
        m_demandModel.refresh(ChunkDemandReason::Player, playerChunk,
                              radius);
        m_demandModel.refresh(ChunkDemandReason::Camera, cameraChunk,
                              radius);
        if (m_demandModel.snapshot().revision != previousRevision) {
            refreshSpatialInterestLocked();
            invalidateWorldJobs();
        }
    }
    m_demandSectionY.store(sectionY);
    publishMeshPrioritySnapshot(camera, sectionY);
}

void ChunkRuntime::publishMeshPrioritySnapshot(const Camera &camera,
                                               int sectionY)
{
    constexpr float ReorderAngleDegrees = 5.f;
    const bool orientationChanged =
        !m_meshPriorityPublished ||
        angularDifference(camera.rotation.x,
                          m_lastMeshPriorityRotation.x) >=
            ReorderAngleDegrees ||
        angularDifference(camera.rotation.y,
                          m_lastMeshPriorityRotation.y) >=
            ReorderAngleDegrees;
    const bool sectionChanged = sectionY != m_lastMeshPrioritySectionY;

    {
        std::lock_guard<std::mutex> lock(m_meshPriorityMutex);
        m_meshPrioritySnapshot.frustum = camera.getFrustum();
        m_meshPrioritySnapshot.sectionY = sectionY;
        m_meshPrioritySnapshot.valid = true;
    }

    if (orientationChanged || sectionChanged) {
        m_lastMeshPriorityRotation = camera.rotation;
        m_lastMeshPrioritySectionY = sectionY;
        m_meshPriorityPublished = true;
        m_meshPriorityRevision.fetch_add(1);
    }
}

void ChunkRuntime::setRenderDistance(int renderDistance) noexcept
{
    const int previous = m_renderDistance.exchange(renderDistance);
    if (previous == renderDistance) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_demandMutex);
        const std::uint64_t previousRevision =
            m_demandModel.snapshot().revision;
        m_demandModel.updateRadius(ChunkDemandReason::Player,
                                   std::max(1, renderDistance));
        m_demandModel.updateRadius(ChunkDemandReason::Camera,
                                   std::max(1, renderDistance));
        if (m_demandModel.snapshot().revision != previousRevision) {
            refreshSpatialInterestLocked();
            invalidateWorldJobs();
        }
    }
    m_unloadScanValid = false;
    m_unloadBacklog = true;
    m_chunkLoadRevision.fetch_add(1);
}

int ChunkRuntime::getRenderDistance() const noexcept
{
    return m_renderDistance.load();
}

void ChunkRuntime::unloadDistantChunks(const Camera &camera)
{
    const VectorXZ cameraChunk = WorldCoordinates::getChunkXZ(
        WorldCoordinates::toBlockCoord(camera.position.x),
        WorldCoordinates::toBlockCoord(camera.position.z));
    if (m_unloadScanValid && cameraChunk == m_lastUnloadScanChunk &&
        !m_unloadBacklog) {
        m_lastUnloadCount.store(0);
        return;
    }
    m_lastUnloadScanChunk = cameraChunk;
    m_unloadScanValid = true;

    SpatialInterestSnapshot spatialInterest;
    {
        std::lock_guard<std::mutex> demandLock(m_demandMutex);
        spatialInterest = m_spatialInterestSnapshot;
    }

    std::unique_lock<std::mutex> lock(m_worldMutex);
    const int renderDistance = m_renderDistance.load();
    const int minX = cameraChunk.x - renderDistance;
    const int minZ = cameraChunk.z - renderDistance;
    const int maxX = cameraChunk.x + renderDistance;
    const int maxZ = cameraChunk.z + renderDistance;

    std::vector<VectorXZ> chunksToUnload;
    for (const auto &entry : m_chunkManager.getChunks()) {
        // Only Resident data is eligible for the Resident ->
        // EvictRequested -> Absent path. Loading/Generating reservations must
        // not consume one of the bounded unload slots and starve eligible
        // Chunks behind them in the map iteration order.
        if (entry.second.getDataResidencyState() !=
            ChunkDataResidencyState::Resident) {
            continue;
        }
        const glm::ivec2 location = entry.second.getLocation();
        const SpatialInterest interest = SpatialInterestModel::interestAt(
            spatialInterest, {location.x, location.y});
        if (interest.requiresResidentData) {
            continue;
        }
        if (minX > location.x || minZ > location.y || maxZ < location.y ||
            maxX < location.x) {
            chunksToUnload.push_back({location.x, location.y});
            if (chunksToUnload.size() > MaxUnloadsPerUpdate) {
                break;
            }
        }
    }

    m_unloadBacklog = chunksToUnload.size() > MaxUnloadsPerUpdate;
    if (m_unloadBacklog) {
        chunksToUnload.resize(MaxUnloadsPerUpdate);
    }
    std::size_t unloaded = 0;
    for (const VectorXZ &location : chunksToUnload) {
        unloaded += m_chunkManager.unloadChunk(location.x, location.z)
                          ? 1u
                          : 0u;
    }
    m_unloadBacklog = m_unloadBacklog || unloaded < chunksToUnload.size();
    m_lastUnloadCount.store(unloaded);
}

void ChunkRuntime::resetMeshes()
{
    std::unique_lock<std::mutex> lock(m_worldMutex);
    m_chunkManager.deleteMeshes();
    m_loadDistance.store(2);
    m_chunkLoadRevision.fetch_add(1);
    invalidateWorldJobs();
}

WorldMeshSnapshot ChunkRuntime::collectSectionMeshSnapshot()
{
    VectorXZ uploadOrigin{0, 0};
    SpatialInterestSnapshot spatialInterest;
    {
        std::lock_guard<std::mutex> demandLock(m_demandMutex);
        uploadOrigin = m_playerDemandCoord;
        spatialInterest = m_spatialInterestSnapshot;
    }
    std::unique_lock<std::mutex> lock(m_worldMutex);

    WorldMeshSnapshot snapshot;
    std::vector<glm::ivec3> readySections;
    for (auto &entry : m_chunkManager.getChunks()) {
        Chunk &chunk = entry.second;
        if (!chunk.hasLoaded()) {
            continue;
        }
        const glm::ivec2 chunkLocation = chunk.getLocation();
        const SpatialInterest interest = SpatialInterestModel::interestAt(
            spatialInterest, {chunkLocation.x, chunkLocation.y});
        if (!interest.requiresNearRepresentation) {
            continue;
        }

        for (std::size_t sectionIndex = 0;
             sectionIndex < chunk.getSectionCount(); ++sectionIndex) {
            ChunkSection *section =
                chunk.findSection(static_cast<int>(sectionIndex));
            if (section == nullptr) {
                continue;
            }

            snapshot.liveSections.push_back(section->getLocation());
            snapshot.liveSectionVersions.push_back(
                {section->getLocation(), section->getBlockRevision()});
            if (section->getMeshState() !=
                ChunkMeshState::CpuReady) {
                continue;
            }
            readySections.push_back(section->getLocation());
        }
    }

    const std::vector<glm::ivec3> selected = planSectionMeshUploads(
        readySections, uploadOrigin, MaxSectionUploadsPerFrame);
    snapshot.cpuReadyTotal = readySections.size();
    snapshot.cpuReadyDeferred =
        readySections.size() - selected.size();
    snapshot.cpuReadySections.reserve(selected.size());
    for (const glm::ivec3 &location : selected) {
        Chunk *chunk = m_chunkManager.findChunk(location.x, location.z);
        ChunkSection *section =
            chunk != nullptr && chunk->hasLoaded()
                ? chunk->findSection(location.y)
                : nullptr;
        if (section == nullptr ||
            section->getMeshState() != ChunkMeshState::CpuReady) {
            continue;
        }
        WorldSectionMeshSnapshot sectionSnapshot;
        sectionSnapshot.location = section->getLocation();
        sectionSnapshot.blockRevision = section->getBlockRevision();
        sectionSnapshot.meshes = section->getMeshes();
        snapshot.cpuReadySections.push_back(std::move(sectionSnapshot));
    }
    m_lastCpuReadyTotal.store(snapshot.cpuReadyTotal);
    m_lastSectionUploadsOffered.store(snapshot.cpuReadySections.size());
    m_lastSectionUploadsDeferred.store(snapshot.cpuReadyDeferred);
    return snapshot;
}

void ChunkRuntime::acknowledgeSectionMeshUploads(
    const std::vector<WorldSectionMeshVersion> &versions)
{
    SpatialInterestSnapshot spatialInterest;
    {
        std::lock_guard<std::mutex> demandLock(m_demandMutex);
        spatialInterest = m_spatialInterestSnapshot;
    }
    std::unique_lock<std::mutex> lock(m_worldMutex);
    for (const WorldSectionMeshVersion &version : versions) {
        const SpatialInterest interest = SpatialInterestModel::interestAt(
            spatialInterest, {version.location.x, version.location.z});
        if (!interest.requiresNearRepresentation) {
            continue;
        }
        Chunk *chunk = m_chunkManager.findChunk(version.location.x,
                                                version.location.z);
        if (chunk == nullptr) {
            continue;
        }

        ChunkSection *section = chunk->findSection(version.location.y);
        if (section != nullptr &&
            section->getMeshState() == ChunkMeshState::CpuReady &&
            section->getBlockRevision() == version.blockRevision) {
            section->markMeshClean();
        }
    }
}

void ChunkRuntime::queueBlockEditLocked(int blockX, int blockY, int blockZ)
{
    for (const ChunkUpdateKey &update :
         ChunkUpdatePlanner::planForBlockEdit(blockX, blockY, blockZ)) {
        queueSectionUpdateLocked({update.x, update.y, update.z});
    }
}

void ChunkRuntime::queueSectionUpdateLocked(const glm::ivec3 &key)
{
    Chunk *chunk = m_chunkManager.findChunk(key.x, key.z);
    if (chunk == nullptr || !chunk->hasLoaded()) {
        return;
    }

    ChunkSection *section = chunk->findSection(key.y);
    if (section == nullptr) {
        return;
    }

    section->invalidateMeshInput();
    if (m_queuedChunkUpdates.emplace(key).second) {
        section->markMeshQueued();
        m_chunkUpdateQueue.push_back(key);
    }
}

void ChunkRuntime::queueLightingUpdatesLocked(
    const std::vector<glm::ivec3> &changedPositions)
{
    std::unordered_set<glm::ivec3, IVec3Hash> sectionUpdates;
    for (const glm::ivec3 &position : changedPositions) {
        for (const ChunkUpdateKey &update :
             ChunkUpdatePlanner::planForBlockEdit(
                 position.x, position.y, position.z)) {
            sectionUpdates.emplace(update.x, update.y, update.z);
        }
    }
    for (const glm::ivec3 &section : sectionUpdates) {
        queueSectionUpdateLocked(section);
    }
}

void ChunkRuntime::processChunkUpdates(std::size_t budget)
{
    HELLOMINE3D_PROFILE_SCOPE("ChunkRuntime::processChunkUpdates");
    std::unique_lock<std::mutex> lock(m_worldMutex);
    std::size_t processed = 0;
    while (processed < budget && !m_chunkUpdateQueue.empty()) {
        const glm::ivec3 key = m_chunkUpdateQueue.front();
        m_chunkUpdateQueue.pop_front();
        m_queuedChunkUpdates.erase(key);
        ++processed;

        Chunk *chunk = m_chunkManager.findChunk(key.x, key.z);
        ChunkSection *section =
            chunk != nullptr && chunk->hasLoaded()
                ? chunk->findSection(key.y)
                : nullptr;
        if (section != nullptr &&
            (section->isMeshDirty() ||
             section->getMeshState() == ChunkMeshState::Queued)) {
            if (section->isMeshDirty()) {
                section->markMeshQueued();
            }
            const auto buildStart = std::chrono::steady_clock::now();
            const bool meshBuilt = section->makeMesh();
            const double buildMilliseconds =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - buildStart)
                    .count();
            if (meshBuilt) {
                m_chunkManager.recordMeshRebuild(section->getMeshes(),
                                                 buildMilliseconds);
            }
        }
    }
}

std::size_t ChunkRuntime::queuedChunkUpdateCountLocked() const noexcept
{
    return m_chunkUpdateQueue.size();
}

void ChunkRuntime::preloadAroundLocked(const glm::vec3 &position, int radius,
                                       ChunkDemandReason reason)
{
    const VectorXZ centerChunk = WorldCoordinates::getChunkXZ(
        WorldCoordinates::toBlockCoord(position.x),
        WorldCoordinates::toBlockCoord(position.z));
    {
        std::lock_guard<std::mutex> lock(m_demandMutex);
        const std::uint64_t previousRevision =
            m_demandModel.snapshot().revision;
        m_demandModel.refresh(reason, centerChunk, radius);
        if (m_demandModel.snapshot().revision != previousRevision) {
            refreshSpatialInterestLocked();
            invalidateWorldJobs();
        }
    }
    for (int x = centerChunk.x - radius; x <= centerChunk.x + radius; ++x) {
        for (int z = centerChunk.z - radius; z <= centerChunk.z + radius;
             ++z) {
            m_chunkManager.loadChunk(x, z);
        }
    }
}

ChunkDemandDebugStats ChunkRuntime::collectDemandDebugStats() const
{
    std::lock_guard<std::mutex> lock(m_demandMutex);
    ChunkDemandDebugStats stats = m_demandModel.debugStats();
    stats.lastPlannedTargets = m_lastPlannedTargetCount.load();
    return stats;
}

SpatialInterestDebugStats
ChunkRuntime::collectSpatialInterestDebugStats() const
{
    std::lock_guard<std::mutex> lock(m_demandMutex);
    return SpatialInterestModel::debugStats(m_spatialInterestSnapshot);
}

WorldJobSchedulerDebugStats
ChunkRuntime::collectJobSchedulerDebugStats() const
{
    WorldJobSchedulerDebugStats stats = m_jobScheduler.debugStats();
    stats.deferredPlanJobs = m_deferredPlanJobCount.load();
    return stats;
}

ChunkBackpressureDebugStats
ChunkRuntime::collectBackpressureDebugStats() const
{
    ChunkBackpressureDebugStats stats;
    stats.deferredPlanJobs = m_deferredPlanJobCount.load();
    stats.lastAuthoritativeCommits =
        m_lastAuthoritativeCommitCount.load();
    stats.peakAuthoritativeCommits =
        m_peakAuthoritativeCommitCount.load();
    stats.maxAuthoritativeCommitsPerPass =
        MaxAuthoritativeCommitsPerPass;
    stats.lastCpuReadyTotal = m_lastCpuReadyTotal.load();
    stats.lastSectionUploadsOffered =
        m_lastSectionUploadsOffered.load();
    stats.lastSectionUploadsDeferred =
        m_lastSectionUploadsDeferred.load();
    stats.maxSectionUploadsPerFrame = MaxSectionUploadsPerFrame;
    stats.lastUnloads = m_lastUnloadCount.load();
    stats.maxUnloadsPerUpdate = MaxUnloadsPerUpdate;
    stats.unloadBacklog = m_unloadBacklog;
    return stats;
}

void ChunkRuntime::invalidateWorldJobs()
{
    std::lock_guard<std::mutex> commitLock(m_worldJobCommitMutex);
    m_jobScheduler.invalidateGeneration();
    m_deferredPlanJobCount.store(0);
}

void ChunkRuntime::refreshSpatialInterestLocked()
{
    m_spatialInterestSnapshot =
        SpatialInterestModel::build(m_demandModel.snapshot());
    m_unloadScanValid = false;
    m_unloadBacklog = true;
}

void ChunkRuntime::startLoader()
{
    if (!m_chunkLoadThreads.empty()) {
        return;
    }
    m_chunkLoadThreads.emplace_back([this]() { runLoader(); });
}

void ChunkRuntime::stopLoader()
{
    m_isRunning.store(false);
    invalidateWorldJobs();
    for (std::thread &thread : m_chunkLoadThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_chunkLoadThreads.clear();
}

void ChunkRuntime::runLoader()
{
    HELLOMINE3D_PROFILE_THREAD("Chunk Loader");

    // Snapshot under the world lock, build without it, and then validate and
    // commit under the lock. The wall-clock pass budget preserves the measured
    // balance between mesh throughput and foreground frame time.
    constexpr auto PassWorkBudget = std::chrono::milliseconds(6);
    constexpr int MaxTargetsPerPass = 64;
    constexpr int ChunkLoadsPerTarget = 1;
    constexpr int ActiveSleepMs = 1;
    constexpr int IdleSleepMs = 10;

    int lastRevision = m_chunkLoadRevision.load();
    int lastPriorityRevision = m_meshPriorityRevision.load();
    std::uint64_t lastDemandRevision = 0;
    bool queueValid = false;
    std::vector<WorldJobRequest> activePlan;
    std::size_t nextPlanIndex = 0;

    while (m_isRunning.load()) {
        ChunkDemandSnapshot demandSnapshot;
        SpatialInterestSnapshot spatialInterestSnapshot;
        VectorXZ movementOrigin{0, 0};
        VectorXZ movementDirection{0, 0};
        WorldJobGenerationToken planGeneration;
        {
            std::lock_guard<std::mutex> lock(m_demandMutex);
            demandSnapshot = m_demandModel.snapshot();
            spatialInterestSnapshot = m_spatialInterestSnapshot;
            movementOrigin = m_playerDemandCoord;
            movementDirection = m_playerMovement;
            planGeneration = m_jobScheduler.currentGenerationToken();
        }
        const int currentRevision = m_chunkLoadRevision.load();
        const int currentPriorityRevision = m_meshPriorityRevision.load();
        if (!queueValid || currentRevision != lastRevision ||
            demandSnapshot.revision != lastDemandRevision ||
            currentPriorityRevision != lastPriorityRevision) {
            MeshPrioritySnapshot prioritySnapshot;
            {
                std::lock_guard<std::mutex> lock(m_meshPriorityMutex);
                prioritySnapshot = m_meshPrioritySnapshot;
            }
            const ViewFrustum *frustum =
                prioritySnapshot.valid ? &prioritySnapshot.frustum : nullptr;
            const auto planned = planDemandWork(
                demandSnapshot, prioritySnapshot.sectionY, frustum,
                movementOrigin, movementDirection);
            activePlan.clear();
            activePlan.reserve(planned.size());
            for (std::size_t index = 0; index < planned.size(); ++index) {
                const ChunkDemandTarget &target = planned[index];
                const SpatialInterest interest =
                    SpatialInterestModel::interestAt(
                        spatialInterestSnapshot, target.coord);
                if (!interest.requiresResidentData) {
                    continue;
                }
                activePlan.push_back(WorldJobRequest{
                    WorldJobType::ChunkLoadOrGenerate, target.coord,
                    target.priority, target.newestEpoch, index,
                    planGeneration});
            }
            const std::size_t initialCount = std::min(
                activePlan.size(),
                WorldJobScheduler::PendingHighWatermark);
            const std::vector<WorldJobRequest> initialWindow(
                activePlan.begin(), activePlan.begin() + initialCount);
            if (!m_jobScheduler.replacePending(initialWindow,
                                               planGeneration)) {
                queueValid = false;
                activePlan.clear();
                nextPlanIndex = 0;
                m_deferredPlanJobCount.store(0);
                continue;
            }
            nextPlanIndex = initialCount;
            m_deferredPlanJobCount.store(
                activePlan.size() - nextPlanIndex);
            m_lastPlannedTargetCount.store(planned.size());
            lastRevision = currentRevision;
            lastDemandRevision = demandSnapshot.revision;
            lastPriorityRevision = currentPriorityRevision;
            queueValid = true;
        }

        if (queueValid && nextPlanIndex < activePlan.size()) {
            WorldJobSchedulerDebugStats pressure =
                m_jobScheduler.debugStats();
            if (pressure.pendingJobs <=
                WorldJobScheduler::PendingLowWatermark) {
                while (nextPlanIndex < activePlan.size() &&
                       pressure.pendingJobs <
                           WorldJobScheduler::PendingHighWatermark) {
                    const WorldJobAdmissionResult result =
                        m_jobScheduler.admit(activePlan[nextPlanIndex]);
                    if (result ==
                        WorldJobAdmissionResult::StaleGeneration) {
                        queueValid = false;
                        activePlan.clear();
                        nextPlanIndex = 0;
                        m_deferredPlanJobCount.store(0);
                        break;
                    }
                    if (result ==
                        WorldJobAdmissionResult::RejectedAtCapacity) {
                        break;
                    }
                    ++nextPlanIndex;
                    pressure = m_jobScheduler.debugStats();
                }
                if (queueValid) {
                    m_deferredPlanJobCount.store(
                        activePlan.size() - nextPlanIndex);
                }
            }
        }

        if (m_jobScheduler.debugStats().pendingJobs == 0) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(IdleSleepMs));
            continue;
        }

        HELLOMINE3D_PROFILE_SCOPE("ChunkRuntime::runLoader pass");
        bool didWork = false;
        int processedTargets = 0;
        std::size_t authoritativeCommits = 0;
        const auto passStart = std::chrono::steady_clock::now();
        while (m_isRunning.load() &&
               processedTargets < MaxTargetsPerPass) {
            WorldJob scheduledJob;
            if (!m_jobScheduler.takeNext(scheduledJob)) {
                break;
            }
            ++processedTargets;

            const auto workerStart = std::chrono::steady_clock::now();
            WorldJobOutcome outcome = WorldJobOutcome::NoWork;
            bool publishFollowUp = false;
            WorldJobType followUpType = scheduledJob.type;
            double commitMilliseconds = 0.0;
            bool authoritativeCommitAttempted = false;
            const SpatialInterest scheduledInterest =
                SpatialInterestModel::interestAt(
                    spatialInterestSnapshot, scheduledJob.target);

            if (!m_jobScheduler.isCurrent(
                    scheduledJob.generation)) {
                outcome = WorldJobOutcome::Cancelled;
            }
            else if (!scheduledInterest.requiresResidentData) {
                outcome = WorldJobOutcome::Cancelled;
            }
            else if (scheduledJob.type ==
                WorldJobType::ChunkLoadOrGenerate) {
                if (!shouldPublishMeshFollowUp(scheduledInterest)) {
                    std::unique_lock<std::mutex> lock(m_worldMutex);
                    outcome = m_chunkManager.chunkLoadedAt(
                                  scheduledJob.target.x,
                                  scheduledJob.target.z)
                                  ? WorldJobOutcome::NoWork
                                  : WorldJobOutcome::CommitRejected;
                }
                else {
                    ChunkLoadJob loadJob;
                    ChunkNeighborhoodLoadJobResult neighborhood;
                    {
                        std::unique_lock<std::mutex> lock(m_worldMutex);
                        neighborhood = m_chunkManager
                            .beginChunkNeighborhoodLoadJob(
                                scheduledJob.target.x,
                                scheduledJob.target.z,
                                ChunkLoadsPerTarget, loadJob);
                    }

                    if (loadJob.valid &&
                        !m_jobScheduler.isCurrent(
                            scheduledJob.generation)) {
                        authoritativeCommitAttempted = true;
                        std::unique_lock<std::mutex> lock(m_worldMutex);
                        std::lock_guard<std::mutex> commitLock(
                            m_worldJobCommitMutex);
                        m_chunkManager.cancelChunkLoadJob(loadJob);
                        outcome = WorldJobOutcome::Cancelled;
                    }
                    else if (loadJob.valid) {
                        const bool prepared =
                            m_chunkManager.prepareChunkLoadJob(loadJob);
                        didWork = didWork || prepared;
                        bool generationCurrent = false;
                        bool commitAccepted = false;
                        const auto commitStart =
                            std::chrono::steady_clock::now();
                        authoritativeCommitAttempted = true;
                        {
                            std::unique_lock<std::mutex> lock(m_worldMutex);
                            std::lock_guard<std::mutex> commitLock(
                                m_worldJobCommitMutex);
                            generationCurrent = m_jobScheduler.isCurrent(
                                scheduledJob.generation);
                            if (generationCurrent && prepared) {
                                commitAccepted =
                                    m_chunkManager.finishChunkLoadJob(loadJob);
                            }
                            else {
                                m_chunkManager.cancelChunkLoadJob(loadJob);
                            }
                        }
                        commitMilliseconds =
                            std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() -
                                commitStart)
                                .count();
                        if (!generationCurrent) {
                            outcome = WorldJobOutcome::Cancelled;
                        }
                        else if (commitAccepted) {
                            outcome = WorldJobOutcome::DidWork;
                            publishFollowUp = true;
                            followUpType =
                                WorldJobType::ChunkLoadOrGenerate;
                        }
                        else {
                            outcome = WorldJobOutcome::CommitRejected;
                            publishFollowUp = true;
                            followUpType =
                                WorldJobType::ChunkLoadOrGenerate;
                        }
                    }
                    else {
                        outcome = WorldJobOutcome::NoWork;
                        publishFollowUp = true;
                        followUpType = neighborhood.neighborhoodReady
                                           ? WorldJobType::ChunkMeshBuild
                                           : WorldJobType::ChunkLoadOrGenerate;
                    }
                }
            }
            else if (!shouldPublishMeshFollowUp(scheduledInterest)) {
                outcome = WorldJobOutcome::NoWork;
            }
            else {
                ChunkMeshJob meshJob;
                ChunkMeshCollection builtMeshes;
                ChunkMeshWorkResult result;
                bool generationCurrentAtBegin = false;
                {
                    std::unique_lock<std::mutex> lock(m_worldMutex);
                    std::lock_guard<std::mutex> commitLock(
                        m_worldJobCommitMutex);
                    generationCurrentAtBegin =
                        m_jobScheduler.isCurrent(
                            scheduledJob.generation);
                    if (generationCurrentAtBegin) {
                        result = m_chunkManager.beginMeshJob(
                            scheduledJob.target.x,
                            scheduledJob.target.z, 0,
                            m_demandSectionY.load(), meshJob);
                    }
                }

                bool commitAccepted = false;
                bool generationCurrentAtCommit =
                    generationCurrentAtBegin;
                if (!generationCurrentAtBegin) {
                    outcome = WorldJobOutcome::Cancelled;
                }
                else if (meshJob.valid &&
                         m_jobScheduler.isCurrent(
                             scheduledJob.generation)) {
                    const auto buildStart =
                        std::chrono::steady_clock::now();
                    ChunkMeshBuilder(meshJob.input, builtMeshes)
                        .buildMesh();
                    const double buildMilliseconds =
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() -
                            buildStart)
                            .count();

                    const auto commitStart =
                        std::chrono::steady_clock::now();
                    authoritativeCommitAttempted = true;
                    {
                        std::unique_lock<std::mutex> lock(m_worldMutex);
                        std::lock_guard<std::mutex> commitLock(
                            m_worldJobCommitMutex);
                        generationCurrentAtCommit =
                            m_jobScheduler.isCurrent(
                                scheduledJob.generation);
                        if (generationCurrentAtCommit) {
                            commitAccepted =
                                m_chunkManager.finishMeshJob(
                                    meshJob, builtMeshes,
                                    buildMilliseconds);
                        }
                        else {
                            m_chunkManager.cancelMeshJob(meshJob);
                        }
                    }
                    commitMilliseconds =
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() -
                            commitStart)
                            .count();
                }
                else if (meshJob.valid) {
                    authoritativeCommitAttempted = true;
                    std::unique_lock<std::mutex> lock(m_worldMutex);
                    std::lock_guard<std::mutex> commitLock(
                        m_worldJobCommitMutex);
                    generationCurrentAtCommit =
                        m_jobScheduler.isCurrent(
                            scheduledJob.generation);
                    if (!generationCurrentAtCommit) {
                        m_chunkManager.cancelMeshJob(meshJob);
                    }
                }

                didWork = didWork || result.meshBuilt ||
                          result.meshSkipped;
                if (!generationCurrentAtBegin ||
                    !generationCurrentAtCommit) {
                    outcome = WorldJobOutcome::Cancelled;
                    publishFollowUp = false;
                }
                else if (result.meshSkipped || commitAccepted) {
                    outcome = WorldJobOutcome::DidWork;
                }
                else if (meshJob.valid) {
                    outcome = WorldJobOutcome::CommitRejected;
                }
                if (generationCurrentAtBegin &&
                    generationCurrentAtCommit) {
                    publishFollowUp = result.meshBuilt ||
                                      result.meshSkipped ||
                                      !result.neighborhoodReady;
                    followUpType = result.neighborhoodReady
                                       ? WorldJobType::ChunkMeshBuild
                                       : WorldJobType::ChunkLoadOrGenerate;
                }
            }

            const double totalWorkerMilliseconds =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - workerStart)
                    .count();
            const double workerMilliseconds = std::max(
                0.0, totalWorkerMilliseconds - commitMilliseconds);
            m_jobScheduler.complete(
                scheduledJob, outcome, workerMilliseconds,
                commitMilliseconds);
            authoritativeCommits +=
                authoritativeCommitAttempted ? 1u : 0u;

            WorldJobCompletion completion;
            if (m_jobScheduler.popCompleted(completion) &&
                publishFollowUp) {
                const WorldJobAdmissionResult followUpAdmission =
                    m_jobScheduler.admit(WorldJobRequest{
                    followUpType, completion.job.target,
                    completion.job.priority,
                    completion.job.demandEpoch,
                    completion.job.planOrder,
                    completion.job.generation});
                if (followUpAdmission ==
                    WorldJobAdmissionResult::StaleGeneration) {
                    queueValid = false;
                }
            }

            if (authoritativeCommits >=
                MaxAuthoritativeCommitsPerPass) {
                break;
            }
            if (std::chrono::steady_clock::now() - passStart >=
                PassWorkBudget) {
                break;
            }
        }

        m_lastAuthoritativeCommitCount.store(authoritativeCommits);
        std::size_t peakCommits = m_peakAuthoritativeCommitCount.load();
        while (peakCommits < authoritativeCommits &&
               !m_peakAuthoritativeCommitCount.compare_exchange_weak(
                   peakCommits, authoritativeCommits)) {
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(
            didWork ? ActiveSleepMs : IdleSleepMs));
    }
}
