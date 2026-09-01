#include "ChunkRuntime.h"

#include <algorithm>
#include <chrono>
#include <cmath>

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

void ChunkRuntime::setInitialLoadCenter(const glm::vec3 &position) noexcept
{
    const VectorXZ center = WorldCoordinates::getChunkXZ(
        WorldCoordinates::toBlockCoord(position.x),
        WorldCoordinates::toBlockCoord(position.z));
    m_loadCenterX.store(center.x);
    m_loadCenterSectionY.store(std::max(
        0, WorldCoordinates::toBlockCoord(position.y) / CHUNK_SIZE));
    m_loadCenterZ.store(center.z);
}

void ChunkRuntime::updateLoadCenter(const Camera &camera)
{
    const VectorXZ cameraChunk = WorldCoordinates::getChunkXZ(
        WorldCoordinates::toBlockCoord(camera.position.x),
        WorldCoordinates::toBlockCoord(camera.position.z));
    const int sectionY = std::max(
        0, WorldCoordinates::toBlockCoord(camera.position.y) / CHUNK_SIZE);
    m_loadCenterX.store(cameraChunk.x);
    m_loadCenterSectionY.store(sectionY);
    m_loadCenterZ.store(cameraChunk.z);
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
    constexpr std::size_t MaxUnloadsPerUpdate = 8;

    const VectorXZ cameraChunk = WorldCoordinates::getChunkXZ(
        WorldCoordinates::toBlockCoord(camera.position.x),
        WorldCoordinates::toBlockCoord(camera.position.z));
    if (m_unloadScanValid && cameraChunk == m_lastUnloadScanChunk &&
        !m_unloadBacklog) {
        return;
    }
    m_lastUnloadScanChunk = cameraChunk;
    m_unloadScanValid = true;

    std::unique_lock<std::mutex> lock(m_worldMutex);
    const int renderDistance = m_renderDistance.load();
    const int minX = cameraChunk.x - renderDistance;
    const int minZ = cameraChunk.z - renderDistance;
    const int maxX = cameraChunk.x + renderDistance;
    const int maxZ = cameraChunk.z + renderDistance;

    std::vector<VectorXZ> chunksToUnload;
    for (const auto &entry : m_chunkManager.getChunks()) {
        const glm::ivec2 location = entry.second.getLocation();
        if (minX > location.x || minZ > location.y || maxZ < location.y ||
            maxX < location.x) {
            chunksToUnload.push_back({location.x, location.y});
            if (chunksToUnload.size() >= MaxUnloadsPerUpdate) {
                break;
            }
        }
    }

    m_unloadBacklog = chunksToUnload.size() >= MaxUnloadsPerUpdate;
    for (const VectorXZ &location : chunksToUnload) {
        m_chunkManager.unloadChunk(location.x, location.z);
    }
}

void ChunkRuntime::resetMeshes()
{
    std::unique_lock<std::mutex> lock(m_worldMutex);
    m_chunkManager.deleteMeshes();
    m_loadDistance.store(2);
    m_chunkLoadRevision.fetch_add(1);
}

WorldMeshSnapshot ChunkRuntime::collectSectionMeshSnapshot()
{
    std::unique_lock<std::mutex> lock(m_worldMutex);

    WorldMeshSnapshot snapshot;
    for (auto &entry : m_chunkManager.getChunks()) {
        Chunk &chunk = entry.second;
        if (!chunk.hasLoaded()) {
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

            WorldSectionMeshSnapshot sectionSnapshot;
            sectionSnapshot.location = section->getLocation();
            sectionSnapshot.blockRevision = section->getBlockRevision();
            sectionSnapshot.meshes = section->getMeshes();
            snapshot.cpuReadySections.push_back(std::move(sectionSnapshot));
        }
    }
    return snapshot;
}

void ChunkRuntime::acknowledgeSectionMeshUploads(
    const std::vector<WorldSectionMeshVersion> &versions)
{
    std::unique_lock<std::mutex> lock(m_worldMutex);
    for (const WorldSectionMeshVersion &version : versions) {
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

void ChunkRuntime::preloadAroundLocked(const glm::vec3 &position, int radius)
{
    const VectorXZ centerChunk = WorldCoordinates::getChunkXZ(
        WorldCoordinates::toBlockCoord(position.x),
        WorldCoordinates::toBlockCoord(position.z));
    for (int x = centerChunk.x - radius; x <= centerChunk.x + radius; ++x) {
        for (int z = centerChunk.z - radius; z <= centerChunk.z + radius;
             ++z) {
            m_chunkManager.loadChunk(x, z);
        }
    }
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

    ChunkMeshJob job;
    ChunkMeshCollection builtMeshes;
    std::deque<VectorXZ> workQueue;
    VectorXZ lastCenter{m_loadCenterX.load(), m_loadCenterZ.load()};
    int lastRevision = m_chunkLoadRevision.load();
    int lastPriorityRevision = m_meshPriorityRevision.load();
    bool queueValid = false;

    while (m_isRunning.load()) {
        const VectorXZ loadCenter{m_loadCenterX.load(),
                                  m_loadCenterZ.load()};
        const int currentRevision = m_chunkLoadRevision.load();
        const int currentPriorityRevision = m_meshPriorityRevision.load();
        if (!queueValid || !(loadCenter == lastCenter) ||
            currentRevision != lastRevision ||
            currentPriorityRevision != lastPriorityRevision) {
            const int radius = std::max(1, m_renderDistance.load());
            MeshPrioritySnapshot prioritySnapshot;
            {
                std::lock_guard<std::mutex> lock(m_meshPriorityMutex);
                prioritySnapshot = m_meshPrioritySnapshot;
            }
            const ViewFrustum *frustum =
                prioritySnapshot.valid ? &prioritySnapshot.frustum : nullptr;
            const auto planned = planMeshWork(
                loadCenter, radius, prioritySnapshot.sectionY, frustum);
            workQueue = std::deque<VectorXZ>(planned.begin(), planned.end());
            lastCenter = loadCenter;
            lastRevision = currentRevision;
            lastPriorityRevision = currentPriorityRevision;
            queueValid = true;
        }

        if (workQueue.empty()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(IdleSleepMs));
            continue;
        }

        HELLOMINE3D_PROFILE_SCOPE("ChunkRuntime::runLoader pass");
        bool didWork = false;
        int processedTargets = 0;
        const auto passStart = std::chrono::steady_clock::now();
        while (m_isRunning.load() &&
               processedTargets < MaxTargetsPerPass && !workQueue.empty()) {
            const VectorXZ target = workQueue.front();
            workQueue.pop_front();
            ++processedTargets;

            ChunkMeshWorkResult result;
            {
                std::unique_lock<std::mutex> lock(m_worldMutex);
                result = m_chunkManager.beginMeshJob(
                    target.x, target.z, ChunkLoadsPerTarget,
                    m_loadCenterSectionY.load(), job);
            }

            if (job.valid) {
                builtMeshes.solidMesh.clearClientData();
                builtMeshes.transparentMesh.clearClientData();
                builtMeshes.waterMesh.clearClientData();
                builtMeshes.floraMesh.clearClientData();
                const auto buildStart = std::chrono::steady_clock::now();
                ChunkMeshBuilder(job.input, builtMeshes).buildMesh();
                const double buildMilliseconds =
                    std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - buildStart)
                        .count();

                std::unique_lock<std::mutex> lock(m_worldMutex);
                m_chunkManager.finishMeshJob(job, builtMeshes,
                                             buildMilliseconds);
            }

            didWork = didWork || result.loadedChunk || result.meshBuilt ||
                      result.meshSkipped;
            if (result.meshBuilt || result.meshSkipped) {
                workQueue.push_front(target);
            }
            else if (!result.neighborhoodReady) {
                workQueue.push_back(target);
            }

            if (std::chrono::steady_clock::now() - passStart >=
                PassWorkBudget) {
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(
            didWork ? ActiveSleepMs : IdleSleepMs));
    }
}
