#include "World.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <future>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

#include "../Actor/ItemEntity.h"
#include "../Actor/MobActor.h"
#include "../Core/Camera.h"
#include "../Maths/Vector2XZ.h"
#include "../Physics/AABB.h"
#include "../Player/Player.h"
#include "../Util/ResourcePaths.h"
#include "../Util/Random.h"
#include "Chunk/ChunkMeshBuilder.h"
#include "Chunk/ChunkUpdatePlanner.h"
#include "WorldCoordinates.h"

namespace
{
    bool readIntEnv(const char *name, int &value)
    {
        const char *text = std::getenv(name);
        if (text == nullptr || text[0] == '\0') {
            return false;
        }

        char *end = nullptr;
        const long parsed = std::strtol(text, &end, 10);
        if (end == text) {
            std::cerr << "Ignoring invalid " << name << ": " << text << '\n';
            return false;
        }

        value = static_cast<int>(parsed);
        return true;
    }

    bool readVec3Env(const char *name, glm::vec3 &value)
    {
        const char *text = std::getenv(name);
        if (text == nullptr || text[0] == '\0') {
            return false;
        }

        std::string normalized(text);
        for (char &ch : normalized) {
            if (ch == ',' || ch == ';') {
                ch = ' ';
            }
        }

        std::stringstream stream(normalized);
        glm::vec3 parsed{0.f};
        if (stream >> parsed.x >> parsed.y >> parsed.z) {
            value = parsed;
            return true;
        }

        std::cerr << "Ignoring invalid " << name << ": " << text << '\n';
        return false;
    }

    std::string resolveSaveDirectory(const std::string &saveDirectory)
    {
        return saveDirectory.empty() ? ResourcePaths::bin("saves/default")
                                     : saveDirectory;
    }

    std::string chunkDirectoryForSave(const std::string &saveDirectory)
    {
        return ResourcePaths::join(resolveSaveDirectory(saveDirectory),
                                   "chunks");
    }

    int chunkDistanceSquared(const VectorXZ &chunk, const VectorXZ &center)
    {
        const int dx = chunk.x - center.x;
        const int dz = chunk.z - center.z;
        return dx * dx + dz * dz;
    }

    int chunkDistanceManhattan(const VectorXZ &chunk, const VectorXZ &center)
    {
        return std::abs(chunk.x - center.x) + std::abs(chunk.z - center.z);
    }

    float angularDifference(float left, float right)
    {
        const float difference =
            std::fmod(std::abs(left - right), 360.f);
        return std::min(difference, 360.f - difference);
    }
}

std::vector<VectorXZ>
World::planChunkMeshWork(const VectorXZ &center, int radius, int sectionY,
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

World::World(const Camera &camera, const Config &config, Player &player,
             std::string saveDirectory, bool startBackgroundLoader,
             int initialPreloadRadius)
    : m_chunkManager(*this, chunkDirectoryForSave(saveDirectory))
    , m_player(&player)
    , m_worldSave(resolveSaveDirectory(saveDirectory))
    , m_renderDistance(config.renderDistance)
{
    (void)camera;

    int forcedSeed = 0;
    const bool hasForcedSeed = readIntEnv("HELLOMINE3D_SEED", forcedSeed);
    glm::vec3 forcedPlayerPosition{0.f};
    const bool hasForcedPlayerPosition =
        readVec3Env("HELLOMINE3D_PLAYER_POSITION", forcedPlayerPosition);
    glm::vec3 forcedPlayerRotation{0.f};
    const bool hasForcedPlayerRotation =
        readVec3Env("HELLOMINE3D_PLAYER_ROTATION", forcedPlayerRotation);

    const bool hasSave = m_worldSave.load(m_worldSaveData);
    if (hasSave) {
        m_chunkManager.setTerrainSeed(m_worldSaveData.seed);
        m_playerSpawnPoint = m_worldSaveData.spawnPoint;
        if (m_worldSaveData.hasPlayerState) {
            player.applySaveState(m_worldSaveData.playerState);
        }
        else {
            player.position = m_playerSpawnPoint;
        }
    }
    else {
        m_worldSaveData.seed =
            hasForcedSeed ? forcedSeed
                          : RandomSingleton::get().intInRange(424, 325322);
        m_chunkManager.setTerrainSeed(m_worldSaveData.seed);

        if (hasForcedPlayerPosition) {
            m_playerSpawnPoint = forcedPlayerPosition;
            player.position = m_playerSpawnPoint;
        }
        else {
            setSpawnPoint();
            player.position = m_playerSpawnPoint;
        }

        m_worldSaveData.spawnPoint = m_playerSpawnPoint;
        saveWorldState();
    }

    if (hasForcedPlayerPosition) {
        player.position = forcedPlayerPosition;
    }
    if (hasForcedPlayerRotation) {
        player.rotation = forcedPlayerRotation;
    }
    if (hasForcedPlayerPosition || hasForcedPlayerRotation) {
        player.velocity = glm::vec3(0.f);
        player.box.update(player.position);
        m_worldSaveData.playerState = player.getSaveState();
        m_worldSaveData.hasPlayerState = true;
    }

    preloadChunksAround(player.position, initialPreloadRadius);
    if (hasSave) {
        restoreActors(m_worldSaveData.actors);
    }

    auto playerChunk = getChunkXZ(toBlockCoord(player.position.x),
                                  toBlockCoord(player.position.z));
    m_loadCenterX.store(playerChunk.x);
    m_loadCenterSectionY.store(
        std::max(0, toBlockCoord(player.position.y) / CHUNK_SIZE));
    m_loadCenterZ.store(playerChunk.z);

    if (startBackgroundLoader) {
        this->startBackgroundLoader();
    }
}

World::~World()
{
    m_isRunning = false;
    for (auto &thread : m_chunkLoadThreads) {
        thread.join();
    }

    std::unique_lock<std::mutex> lock(m_mainMutex);
    m_chunkManager.saveDirtyChunks();
    saveWorldState();
}

// world coords into chunk column coords
ChunkBlock World::getBlock(int x, int y, int z)
{
    std::unique_lock<std::mutex> lock(m_mainMutex);
    return getBlockUnlocked(x, y, z);
}

ChunkBlock World::getBlockUnlocked(int x, int y, int z)
{
    if (y < 0) {
        return BlockId::Air;
    }

    auto bp = getBlockXZ(x, z);
    auto chunkPosition = getChunkXZ(x, z);
    Chunk *chunk = m_chunkManager.findChunk(chunkPosition.x, chunkPosition.z);
    if (chunk == nullptr || !chunk->hasLoaded()) {
        return BlockId::Air;
    }

    return chunk->getBlock(bp.x, y, bp.z);
}

void World::setBlock(int x, int y, int z, ChunkBlock block)
{
    if (y <= 0)
        return;

    std::unique_lock<std::mutex> lock(m_mainMutex);

    auto bp = getBlockXZ(x, z);
    auto chunkPosition = getChunkXZ(x, z);
    Chunk *chunk = m_chunkManager.findChunk(chunkPosition.x, chunkPosition.z);
    if (chunk == nullptr || !chunk->hasLoaded()) {
        return;
    }

    if (chunk->getBlock(bp.x, y, bp.z) == block) {
        return;
    }

    chunk->setBlock(bp.x, y, bp.z, block);
    queueChunkUpdate(x, y, z);
}

void World::tick(int worldTime)
{
    m_worldSaveData.worldTime = static_cast<float>(worldTime);
    m_actorManager.tick(*this, 1.f / 20.f);
}

// loads chunks
// make chunk meshes
void World::update(const Camera &camera)
{
    setChunkLoadCenter(camera);

    for (auto &event : m_events) {
        event->handle(*this);
    }
    m_events.clear();

    unloadDistantChunks(camera);
    updateChunks();
}

WorldMeshSnapshot World::collectSectionMeshSnapshot()
{
    std::unique_lock<std::mutex> lock(m_mainMutex);

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
            if (section->getMeshState() !=
                ChunkSectionMeshState::CpuReady) {
                continue;
            }

            WorldSectionMeshSnapshot sectionSnapshot;
            sectionSnapshot.location = section->getLocation();
            sectionSnapshot.blockRevision = section->getBlockRevision();
            sectionSnapshot.meshes = section->getMeshes();
            snapshot.cpuReadySections.push_back(
                std::move(sectionSnapshot));
        }
    }
    return snapshot;
}

void World::acknowledgeSectionMeshUploads(
    const std::vector<WorldSectionMeshVersion> &versions)
{
    std::unique_lock<std::mutex> lock(m_mainMutex);
    for (const WorldSectionMeshVersion &version : versions) {
        Chunk *chunk = m_chunkManager.findChunk(
            version.location.x, version.location.z);
        if (chunk == nullptr) {
            continue;
        }

        ChunkSection *section = chunk->findSection(version.location.y);
        if (section != nullptr &&
            section->getMeshState() == ChunkSectionMeshState::CpuReady &&
            section->getBlockRevision() == version.blockRevision) {
            section->markGpuBuffered();
        }
    }
}

void World::startBackgroundLoader()
{
    if (!m_chunkLoadThreads.empty()) {
        return;
    }

    m_chunkLoadThreads.emplace_back([this]() { loadChunks(); });
}

void World::unloadDistantChunks(const Camera &camera)
{
    constexpr std::size_t MaxUnloadsPerUpdate = 8;

    const VectorXZ cameraChunk = getChunkXZ(
        toBlockCoord(camera.position.x), toBlockCoord(camera.position.z));
    if (m_unloadScanValid && cameraChunk == m_lastUnloadScanChunk &&
        !m_unloadBacklog) {
        return;
    }
    m_lastUnloadScanChunk = cameraChunk;
    m_unloadScanValid = true;

    std::unique_lock<std::mutex> lock(m_mainMutex);
    const int minX = cameraChunk.x - m_renderDistance;
    const int minZ = cameraChunk.z - m_renderDistance;
    const int maxX = cameraChunk.x + m_renderDistance;
    const int maxZ = cameraChunk.z + m_renderDistance;

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

    m_unloadBacklog =
        chunksToUnload.size() >= MaxUnloadsPerUpdate;
    for (const VectorXZ &location : chunksToUnload) {
        m_chunkManager.unloadChunk(location.x, location.z);
    }
}

void World::resetChunkMeshes()
{
    std::unique_lock<std::mutex> lock(m_mainMutex);
    m_chunkManager.deleteMeshes();
    m_loadDistance.store(2);
    m_chunkLoadRevision.fetch_add(1);
}

///@TODO
/// Optimize for chunkPositionU usage :thinking:
void World::loadChunks()
{
    // Each target is processed in three steps: snapshot the section's
    // neighbourhood under the world lock, build the mesh without it, then
    // install the result under the lock again. Only the two short lock
    // sections contend with the render thread; the expensive build does not.
    //
    // Sleeping once per built mesh (the original behaviour) capped throughput
    // at the OS timer granularity (~15.6 ms on Windows). The wall-clock pass
    // budget replaces that: it bounds how long the worker runs before handing
    // the CPU back, without tying throughput to the timer.
    // Measured on this scene: raising the budget past 6 ms buys no extra mesh
    // throughput but does widen the worst frame, so the remaining limit is
    // elsewhere (chunk neighbourhood readiness), not here.
    constexpr auto kPassWorkBudget = std::chrono::milliseconds(6);
    constexpr int kMaxTargetsPerPass = 64;
    constexpr int kChunkLoadsPerTarget = 1;
    constexpr int kActiveSleepMs = 1;
    constexpr int kIdleSleepMs = 10;

    ChunkMeshJob job;
    ChunkMeshCollection builtMeshes;

    std::deque<VectorXZ> workQueue;
    VectorXZ lastCenter{m_loadCenterX.load(), m_loadCenterZ.load()};
    int lastRevision = m_chunkLoadRevision.load();
    int lastPriorityRevision = m_meshPriorityRevision.load();
    bool queueValid = false;

    while (m_isRunning) {
        VectorXZ loadCenter{m_loadCenterX.load(), m_loadCenterZ.load()};
        const int currentRevision = m_chunkLoadRevision.load();
        const int currentPriorityRevision = m_meshPriorityRevision.load();
        if (!queueValid || !(loadCenter == lastCenter) ||
            currentRevision != lastRevision ||
            currentPriorityRevision != lastPriorityRevision) {
            const int radius = std::max(1, m_renderDistance);
            MeshPrioritySnapshot prioritySnapshot;
            {
                std::lock_guard<std::mutex> lock(m_meshPriorityMutex);
                prioritySnapshot = m_meshPrioritySnapshot;
            }
            const ViewFrustum *frustum =
                prioritySnapshot.valid ? &prioritySnapshot.frustum : nullptr;
            const auto planned = planChunkMeshWork(
                loadCenter, radius, prioritySnapshot.sectionY, frustum);
            workQueue = std::deque<VectorXZ>(planned.begin(), planned.end());
            lastCenter = loadCenter;
            lastRevision = currentRevision;
            lastPriorityRevision = currentPriorityRevision;
            queueValid = true;
        }

        if (workQueue.empty()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(kIdleSleepMs));
            continue;
        }

        bool didWork = false;
        int processedTargets = 0;
        const auto passStart = std::chrono::steady_clock::now();
        while (m_isRunning && processedTargets < kMaxTargetsPerPass &&
               !workQueue.empty()) {
            const VectorXZ target = workQueue.front();
            workQueue.pop_front();
            ++processedTargets;

            ChunkMeshWorkResult result;
            {
                std::unique_lock<std::mutex> lock(m_mainMutex);
                result = m_chunkManager.beginMeshJob(
                    target.x, target.z, kChunkLoadsPerTarget,
                    m_loadCenterSectionY.load(), job);
            }

            if (job.valid) {
                // No world access here: the builder reads only the snapshot.
                // A rejected stale job is not adopted, so clear its temporary
                // data before reusing the allocation for the next section.
                builtMeshes.solidMesh.clearClientData();
                builtMeshes.waterMesh.clearClientData();
                builtMeshes.floraMesh.clearClientData();
                const auto buildStart = std::chrono::steady_clock::now();
                ChunkMeshBuilder(job.input, builtMeshes).buildMesh();
                const double buildMilliseconds =
                    std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - buildStart)
                        .count();

                std::unique_lock<std::mutex> lock(m_mainMutex);
                m_chunkManager.finishMeshJob(job, builtMeshes,
                                             buildMilliseconds);
            }

            didWork = didWork || result.loadedChunk || result.meshBuilt ||
                      result.meshSkipped;
            if (result.meshBuilt || result.meshSkipped) {
                // This chunk still has dirty sections. Keep working on it
                // instead of sending it to the back of a queue that is sorted
                // by distance, otherwise the nearest chunks finish last.
                workQueue.push_front(target);
            }
            else if (!result.neighborhoodReady) {
                // Waiting on neighbour chunk loads; retry after the rest of
                // the queue has had a turn.
                workQueue.push_back(target);
            }

            if (std::chrono::steady_clock::now() - passStart >=
                kPassWorkBudget) {
                break;
            }
        }

        // Measured: replacing this sleep with a bare yield drops Debug to
        // 30 fps even with the build off-lock, because `std::mutex` is unfair
        // and the snapshot still contends every target. Sleeping once per
        // pass, not once per mesh, keeps both throughput and frame rate.
        std::this_thread::sleep_for(std::chrono::milliseconds(
            didWork ? kActiveSleepMs : kIdleSleepMs));
    }
}

void World::setChunkLoadCenter(const Camera &camera)
{
    auto cameraChunk = getChunkXZ(toBlockCoord(camera.position.x),
                                  toBlockCoord(camera.position.z));
    const int sectionY =
        std::max(0, toBlockCoord(camera.position.y) / CHUNK_SIZE);
    m_loadCenterX.store(cameraChunk.x);
    m_loadCenterSectionY.store(sectionY);
    m_loadCenterZ.store(cameraChunk.z);
    publishMeshPrioritySnapshot(camera, sectionY);
}

void World::publishMeshPrioritySnapshot(const Camera &camera, int sectionY)
{
    constexpr float kReorderAngleDegrees = 5.f;
    const bool orientationChanged =
        !m_meshPriorityPublished ||
        angularDifference(camera.rotation.x,
                          m_lastMeshPriorityRotation.x) >=
            kReorderAngleDegrees ||
        angularDifference(camera.rotation.y,
                          m_lastMeshPriorityRotation.y) >=
            kReorderAngleDegrees;
    const bool sectionChanged =
        sectionY != m_lastMeshPrioritySectionY;

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

void World::updateChunk(int blockX, int blockY, int blockZ)
{
    std::unique_lock<std::mutex> lock(m_mainMutex);
    queueChunkUpdate(blockX, blockY, blockZ);
}

bool World::save()
{
    std::unique_lock<std::mutex> lock(m_mainMutex);
    m_chunkManager.saveDirtyChunks();
    return saveWorldState();
}

float World::getWorldTime() const
{
    return m_worldSaveData.worldTime;
}

WorldDebugStats World::collectDebugStats()
{
    std::unique_lock<std::mutex> lock(m_mainMutex);

    WorldDebugStats stats;
    stats.chunks = m_chunkManager.collectDebugStats();
    stats.actorCount = m_actorManager.getActorCount();
    stats.queuedChunkUpdates = m_chunkUpdateQueue.size();
    stats.terrainSeed = m_chunkManager.getTerrainSeed();
    stats.worldTime = m_worldSaveData.worldTime;
    return stats;
}

std::vector<ActorSnapshot> World::collectActorSnapshots()
{
    std::unique_lock<std::mutex> lock(m_mainMutex);
    return m_actorManager.collectSnapshots();
}

void World::preloadAround(const glm::vec3 &position)
{
    std::unique_lock<std::mutex> lock(m_mainMutex);
    preloadChunksAround(position);
}

ActorId World::spawnItemEntity(Material::ID materialId, int amount,
                               const glm::vec3 &position,
                               const glm::vec3 &initialVelocity)
{
    if (materialId == Material::ID::Nothing || amount <= 0) {
        return InvalidActorId;
    }

    auto item = std::make_unique<ItemEntity>(m_actorManager.allocateActorId(),
                                             materialId, amount, position);
    item->velocity = initialVelocity;
    return m_actorManager.addActor(std::move(item), *this);
}

ActorId World::spawnMob(const std::string &type, const glm::vec3 &position)
{
    auto mob =
        std::make_unique<MobActor>(m_actorManager.allocateActorId(), type,
                                   position);
    return m_actorManager.addActor(std::move(mob), *this);
}

void World::queueChunkUpdate(int blockX, int blockY, int blockZ)
{
    auto addChunkToUpdateBatch = [&](const glm::ivec3 &key) {
        Chunk *chunk = m_chunkManager.findChunk(key.x, key.z);
        if (chunk == nullptr || !chunk->hasLoaded()) {
            return;
        }

        ChunkSection *section = chunk->findSection(key.y);
        if (section == nullptr) {
            return;
        }

        section->markMeshDirty();
        if (m_queuedChunkUpdates.emplace(key).second) {
            m_chunkUpdateQueue.push_back(key);
        }
    };

    for (const auto &update :
         ChunkUpdatePlanner::planForBlockEdit(blockX, blockY, blockZ)) {
        addChunkToUpdateBatch({update.x, update.y, update.z});
    }
}

ChunkManager &World::getChunkManager()
{
    return m_chunkManager;
}

ActorManager &World::getActorManager()
{
    return m_actorManager;
}

const ActorManager &World::getActorManager() const
{
    return m_actorManager;
}

SandboxEventBus &World::getEventBus()
{
    return m_eventBus;
}

const SandboxEventBus &World::getEventBus() const
{
    return m_eventBus;
}

Player *World::getPlayer()
{
    return m_player;
}

const Player *World::getPlayer() const
{
    return m_player;
}

VectorXZ World::getBlockXZ(int x, int z)
{
    return WorldCoordinates::getBlockXZ(x, z);
}

VectorXZ World::getChunkXZ(int x, int z)
{
    return WorldCoordinates::getChunkXZ(x, z);
}

int World::toBlockCoord(float value)
{
    return WorldCoordinates::toBlockCoord(value);
}

int World::floorDiv(int value, int divisor)
{
    return WorldCoordinates::floorDiv(value, divisor);
}

int World::floorMod(int value, int divisor)
{
    return WorldCoordinates::floorMod(value, divisor);
}

void World::updateChunks()
{
    std::unique_lock<std::mutex> lock(m_mainMutex);
    std::size_t processed = 0;
    while (processed < ChunkMeshRebuildBudgetPerUpdate &&
           !m_chunkUpdateQueue.empty()) {
        const glm::ivec3 key = m_chunkUpdateQueue.front();
        m_chunkUpdateQueue.pop_front();
        m_queuedChunkUpdates.erase(key);
        ++processed;

        Chunk *chunk = m_chunkManager.findChunk(key.x, key.z);
        ChunkSection *section =
            chunk != nullptr && chunk->hasLoaded()
                ? chunk->findSection(key.y)
                : nullptr;
        if (section != nullptr && section->isMeshDirty()) {
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

void World::preloadChunksAround(const glm::vec3 &position, int radius)
{
    auto centerChunk = getChunkXZ(toBlockCoord(position.x),
                                  toBlockCoord(position.z));
    for (int x = centerChunk.x - radius; x <= centerChunk.x + radius; ++x) {
        for (int z = centerChunk.z - radius; z <= centerChunk.z + radius;
             ++z) {
            m_chunkManager.loadChunk(x, z);
        }
    }
}

bool World::saveWorldState()
{
    m_worldSaveData.version = WorldSaveFormatVersion;
    m_worldSaveData.spawnPoint = m_playerSpawnPoint;
    if (m_player != nullptr) {
        m_worldSaveData.playerState = m_player->getSaveState();
        m_worldSaveData.hasPlayerState = true;
    }
    m_worldSaveData.actors = m_actorManager.collectSaveStates();

    return m_worldSave.save(m_worldSaveData);
}

void World::restoreActors(const std::vector<ActorSaveState> &states)
{
    for (const ActorSaveState &state : states) {
        if (!state.alive || state.id == InvalidActorId ||
            state.type.empty()) {
            continue;
        }

        std::unique_ptr<Actor> actor;
        if (state.kind == ActorSaveKind::Item) {
            const auto materialId =
                static_cast<Material::ID>(state.materialId);
            if (materialId <= Material::ID::Nothing ||
                materialId > Material::ID::IronOre || state.amount <= 0) {
                continue;
            }
            actor = std::make_unique<ItemEntity>(
                state.id, materialId, state.amount, state.position);
        }
        else if (state.kind == ActorSaveKind::Mob) {
            actor = std::make_unique<MobActor>(
                state.id, state.type, state.position);
        }
        else {
            continue;
        }

        actor->applySaveState(state);
        if (!actor->isAlive()) {
            continue;
        }
        if (m_actorManager.addActor(std::move(actor), *this) ==
            InvalidActorId) {
            std::cerr << "Ignoring duplicate persisted actor id: "
                      << state.id << '\n';
        }
    }
}

void World::setSpawnPoint()
{
    const auto start = std::chrono::steady_clock::now();
    std::cout << "Searching for spawn...\n";
    int attempts = 0;
    int chunkX = -1;
    int chunkZ = -1;
    int blockX = 0;
    int blockZ = 0;
    int blockY = 0;

    auto h = m_chunkManager.getTerrainGenerator().getMinimumSpawnHeight();

    while (blockY <= h) {
        m_chunkManager.unloadChunk(chunkX, chunkZ);

        chunkX = RandomSingleton::get().intInRange(100, 200);
        chunkZ = RandomSingleton::get().intInRange(100, 200);
        blockX = RandomSingleton::get().intInRange(0, 15);
        blockZ = RandomSingleton::get().intInRange(0, 15);

        m_chunkManager.loadChunk(chunkX, chunkZ);
        blockY =
            m_chunkManager.getChunk(chunkX, chunkZ).getHeightAt(blockX, blockZ);
        attempts++;
    }

    int worldX = chunkX * CHUNK_SIZE + blockX;
    int worldZ = chunkZ * CHUNK_SIZE + blockZ;

    m_playerSpawnPoint = {worldX, blockY, worldZ};

    preloadChunksAround(m_playerSpawnPoint);

    std::cout << "Spawn found! Attempts: " << attempts
              << " Time Taken: "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                               start)
                     .count()
              << " seconds\n";
}
