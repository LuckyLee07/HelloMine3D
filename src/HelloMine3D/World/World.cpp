#include "World.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <future>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <utility>
#include <vector>

#include "../Actor/ItemEntity.h"
#include "../Actor/MobActor.h"
#include "../Core/Camera.h"
#include "../Diagnostics/RuntimeProfiler.h"
#include "../Diagnostics/OperationPerformanceTiming.h"
#include "../Maths/Vector2XZ.h"
#include "../Physics/AABB.h"
#include "../Player/Player.h"
#include "../Sandbox/Events/PlayerEvents.h"
#include "../Util/ResourcePaths.h"
#include "../Util/Random.h"
#include "Chunk/ChunkMeshBuilder.h"
#include "Chunk/ChunkUpdatePlanner.h"
#include "Block/BlockDatabase.h"
#include "Storage/WorldCatalogue.h"
#include "WorldCoordinates.h"

#ifndef HELLOMINE3D_BUILD_ID
#define HELLOMINE3D_BUILD_ID "development"
#endif

namespace
{
    const std::array<glm::ivec3, 6> LightOffsets = {
        glm::ivec3{1, 0, 0},  glm::ivec3{-1, 0, 0},
        glm::ivec3{0, 1, 0},  glm::ivec3{0, -1, 0},
        glm::ivec3{0, 0, 1},  glm::ivec3{0, 0, -1},
    };

    LightLevel blockEmission(ChunkBlock block)
    {
        const int value =
            BlockDatabase::get()
                .getDefinition(static_cast<BlockId>(block.id))
                .light;
        return clampLightLevel(static_cast<LightLevel>(value));
    }

    std::uint32_t randomTickSelection(int terrainSeed, int worldTime,
                                      const glm::ivec3 &section,
                                      std::size_t attempt)
    {
        std::uint32_t value = 2166136261u;
        const auto mix = [&value](int component) {
            value ^= static_cast<std::uint32_t>(component);
            value *= 16777619u;
            value ^= value >> 13;
        };
        mix(terrainSeed);
        mix(worldTime);
        mix(section.x);
        mix(section.y);
        mix(section.z);
        mix(static_cast<int>(attempt));
        return value;
    }

    std::uint32_t naturalMobSelection(int terrainSeed, int spawnEpoch,
                                      std::size_t attempt)
    {
        std::uint32_t value = 0x9e3779b9u;
        const auto mix = [&value](std::uint32_t component) {
            value ^= component + 0x85ebca6bu + (value << 6) + (value >> 2);
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
        };
        mix(static_cast<std::uint32_t>(terrainSeed));
        mix(static_cast<std::uint32_t>(spawnEpoch));
        mix(static_cast<std::uint32_t>(attempt));
        return value;
    }

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

    std::int64_t currentUtcSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    std::string createWorldId()
    {
        static std::atomic<std::uint64_t> sequence{0};
        const std::uint64_t high =
            static_cast<std::uint64_t>(
                std::chrono::system_clock::now().time_since_epoch().count()) ^
            (++sequence * 0x9e3779b97f4a7c15ULL);
        std::uint64_t low = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        try {
            std::random_device source;
            low ^= (static_cast<std::uint64_t>(source()) << 32u) ^ source();
        }
        catch (...) {
            low ^= high * 0xbf58476d1ce4e5b9ULL;
        }
        std::ostringstream output;
        output << "world-" << std::hex << std::setfill('0')
               << std::setw(16) << high << std::setw(16) << low;
        return output.str();
    }

    std::string initialWorldName(const std::string &saveDirectory)
    {
        const std::filesystem::path path(resolveSaveDirectory(saveDirectory));
        const std::string candidate = path.filename().string();
        return WorldCatalogue::isValidDisplayName(candidate) ? candidate
                                                              : "World";
    }

    std::string currentBuildIdentity()
    {
        const std::string configured = HELLOMINE3D_BUILD_ID;
        return WorldCatalogue::isValidBuildIdentity(configured)
                   ? configured
                   : "development";
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
    , m_worldBackup(resolveSaveDirectory(saveDirectory))
    , m_renderDistance(config.renderDistance)
{
    (void)camera;
    player.attachEventBus(m_eventBus);

    int forcedSeed = 0;
    const bool hasForcedSeed = readIntEnv("HELLOMINE3D_SEED", forcedSeed);
    glm::vec3 forcedPlayerPosition{0.f};
    const bool hasForcedPlayerPosition =
        readVec3Env("HELLOMINE3D_PLAYER_POSITION", forcedPlayerPosition);
    glm::vec3 forcedPlayerRotation{0.f};
    const bool hasForcedPlayerRotation =
        readVec3Env("HELLOMINE3D_PLAYER_ROTATION", forcedPlayerRotation);
    int forcedWorldTime = 0;
    const bool hasForcedWorldTime =
        readIntEnv("HELLOMINE3D_WORLD_TIME", forcedWorldTime);

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
        m_worldSaveData.worldId = createWorldId();
        m_worldSaveData.worldName = initialWorldName(saveDirectory);
        m_worldSaveData.seed =
            hasForcedSeed ? forcedSeed
                          : config.worldSeed.has_value()
                                ? *config.worldSeed
                                : RandomSingleton::get().intInRange(
                                      424, 325322);
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
    runtimeOperationTimings().markLatestActive(
        RuntimeOperationKind::WorldEntry);

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
    player.resetInterpolation();
    if (hasForcedWorldTime) {
        m_worldSaveData.worldTime = static_cast<float>(forcedWorldTime);
    }

    preloadChunksAround(player.position, initialPreloadRadius);
    if (hasSave) {
        restoreActors(m_worldSaveData.actors);
    }
    runtimeOperationTimings().markLatestActive(
        RuntimeOperationKind::WorldEntry);
    m_playerActor.syncFromPlayer(player);

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
    if (m_player != nullptr) {
        m_player->detachEventBus(m_eventBus);
    }
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

LightLevel World::getSunlight(int x, int y, int z)
{
    std::unique_lock<std::mutex> lock(m_mainMutex);
    return getSunlightUnlocked(x, y, z);
}

LightLevel World::getSunlightUnlocked(int x, int y, int z)
{
    if (y < 0) {
        return MIN_LIGHT_LEVEL;
    }

    const auto blockPosition = getBlockXZ(x, z);
    const auto chunkPosition = getChunkXZ(x, z);
    const Chunk *chunk =
        m_chunkManager.findChunk(chunkPosition.x, chunkPosition.z);
    if (chunk == nullptr || !chunk->hasLoaded()) {
        return MAX_LIGHT_LEVEL;
    }

    return chunk->getSunlight(blockPosition.x, y, blockPosition.z);
}

LightLevel World::getBlockLight(int x, int y, int z)
{
    std::unique_lock<std::mutex> lock(m_mainMutex);
    return getBlockLightUnlocked(x, y, z);
}

LightLevel World::getBlockLightUnlocked(int x, int y, int z)
{
    if (y < 0) {
        return MIN_LIGHT_LEVEL;
    }

    const auto blockPosition = getBlockXZ(x, z);
    const auto chunkPosition = getChunkXZ(x, z);
    const Chunk *chunk =
        m_chunkManager.findChunk(chunkPosition.x, chunkPosition.z);
    if (chunk == nullptr || !chunk->hasLoaded()) {
        return MIN_LIGHT_LEVEL;
    }

    return chunk->getBlockLight(blockPosition.x, y, blockPosition.z);
}

bool World::setBlockLightUnlocked(int x, int y, int z, LightLevel level)
{
    if (y < 0) {
        return false;
    }

    const auto blockPosition = getBlockXZ(x, z);
    const auto chunkPosition = getChunkXZ(x, z);
    Chunk *chunk =
        m_chunkManager.findChunk(chunkPosition.x, chunkPosition.z);
    return chunk != nullptr && chunk->hasLoaded() &&
           chunk->setBlockLight(blockPosition.x, y, blockPosition.z, level);
}

void World::propagateBlockLight(
    std::deque<glm::ivec3> &pending,
    std::vector<glm::ivec3> &changedPositions)
{
    while (!pending.empty()) {
        const glm::ivec3 position = pending.front();
        pending.pop_front();

        const ChunkBlock block =
            getBlockUnlocked(position.x, position.y, position.z);
        LightLevel current =
            getBlockLightUnlocked(position.x, position.y, position.z);
        LightLevel desired = blockEmission(block);
        if (!block.getData().isOpaque) {
            for (const glm::ivec3 &offset : LightOffsets) {
                const glm::ivec3 adjacent = position + offset;
                const LightLevel adjacentLight = getBlockLightUnlocked(
                    adjacent.x, adjacent.y, adjacent.z);
                if (adjacentLight > MIN_LIGHT_LEVEL) {
                    desired = std::max(
                        desired,
                        static_cast<LightLevel>(adjacentLight - 1));
                }
            }
        }

        if (desired > current) {
            if (!setBlockLightUnlocked(position.x, position.y, position.z,
                                       desired)) {
                continue;
            }
            changedPositions.push_back(position);
            current = desired;
        }
        if (current <= MIN_LIGHT_LEVEL + 1) {
            continue;
        }

        const LightLevel propagated = static_cast<LightLevel>(current - 1);
        for (const glm::ivec3 &offset : LightOffsets) {
            const glm::ivec3 adjacent = position + offset;
            const ChunkBlock adjacentBlock =
                getBlockUnlocked(adjacent.x, adjacent.y, adjacent.z);
            if (adjacentBlock.getData().isOpaque ||
                getBlockLightUnlocked(adjacent.x, adjacent.y, adjacent.z) >=
                    propagated) {
                continue;
            }
            if (setBlockLightUnlocked(adjacent.x, adjacent.y, adjacent.z,
                                      propagated)) {
                changedPositions.push_back(adjacent);
                pending.push_back(adjacent);
            }
        }
    }
}

void World::relightBlockEdit(
    const glm::ivec3 &position, LightLevel previousLight,
    std::vector<glm::ivec3> &changedPositions)
{
    std::deque<std::pair<glm::ivec3, LightLevel>> removalQueue;
    std::deque<glm::ivec3> additionQueue;

    if (previousLight > MIN_LIGHT_LEVEL) {
        if (setBlockLightUnlocked(position.x, position.y, position.z,
                                  MIN_LIGHT_LEVEL)) {
            changedPositions.push_back(position);
        }
        removalQueue.emplace_back(position, previousLight);
        removeBlockLight(removalQueue, additionQueue, changedPositions);
    }
    else {
        additionQueue.push_back(position);
        propagateBlockLight(additionQueue, changedPositions);
    }
}

void World::removeBlockLight(
    std::deque<std::pair<glm::ivec3, LightLevel>> &removalQueue,
    std::deque<glm::ivec3> &additionQueue,
    std::vector<glm::ivec3> &changedPositions)
{
    std::vector<glm::ivec3> clearedPositions;
    for (const auto &root : removalQueue) {
        clearedPositions.push_back(root.first);
    }

    while (!removalQueue.empty()) {
        const auto removal = removalQueue.front();
        removalQueue.pop_front();
        for (const glm::ivec3 &offset : LightOffsets) {
            const glm::ivec3 adjacent = removal.first + offset;
            const LightLevel adjacentLight = getBlockLightUnlocked(
                adjacent.x, adjacent.y, adjacent.z);
            if (adjacentLight == MIN_LIGHT_LEVEL) {
                continue;
            }

            if (adjacentLight < removal.second) {
                if (setBlockLightUnlocked(adjacent.x, adjacent.y, adjacent.z,
                                          MIN_LIGHT_LEVEL)) {
                    changedPositions.push_back(adjacent);
                    clearedPositions.push_back(adjacent);
                    removalQueue.emplace_back(adjacent, adjacentLight);
                }
            }
            else {
                additionQueue.push_back(adjacent);
            }
        }
    }

    for (const glm::ivec3 &cleared : clearedPositions) {
        additionQueue.push_back(cleared);
    }
    propagateBlockLight(additionQueue, changedPositions);
}

void World::reconcileBlockLightAfterChunkLoad(int chunkX, int chunkZ)
{
    Chunk *chunk = m_chunkManager.findChunk(chunkX, chunkZ);
    if (chunk == nullptr || !chunk->hasLoaded()) {
        return;
    }

    const int baseX = chunkX * CHUNK_SIZE;
    const int baseZ = chunkZ * CHUNK_SIZE;
    const int height = static_cast<int>(chunk->getSectionCount()) * CHUNK_SIZE;
    std::unordered_set<glm::ivec3, IVec3Hash> uniqueSeeds;
    for (int y = 0; y < height; ++y) {
        for (int offset = 0; offset < CHUNK_SIZE; ++offset) {
            uniqueSeeds.emplace(baseX, y, baseZ + offset);
            uniqueSeeds.emplace(baseX - 1, y, baseZ + offset);
            uniqueSeeds.emplace(baseX + CHUNK_SIZE - 1, y, baseZ + offset);
            uniqueSeeds.emplace(baseX + CHUNK_SIZE, y, baseZ + offset);
            uniqueSeeds.emplace(baseX + offset, y, baseZ);
            uniqueSeeds.emplace(baseX + offset, y, baseZ - 1);
            uniqueSeeds.emplace(baseX + offset, y,
                                baseZ + CHUNK_SIZE - 1);
            uniqueSeeds.emplace(baseX + offset, y, baseZ + CHUNK_SIZE);
        }
    }

    std::deque<glm::ivec3> pending(uniqueSeeds.begin(), uniqueSeeds.end());
    std::vector<glm::ivec3> changedPositions;
    propagateBlockLight(pending, changedPositions);
    queueLightingUpdates(changedPositions);
}

void World::reconcileBlockLightAfterChunkUnload(int chunkX, int chunkZ,
                                                int height)
{
    const int baseX = chunkX * CHUNK_SIZE;
    const int baseZ = chunkZ * CHUNK_SIZE;
    std::unordered_set<glm::ivec3, IVec3Hash> uniqueRoots;
    for (int y = 0; y < height; ++y) {
        for (int offset = 0; offset < CHUNK_SIZE; ++offset) {
            uniqueRoots.emplace(baseX - 1, y, baseZ + offset);
            uniqueRoots.emplace(baseX + CHUNK_SIZE, y, baseZ + offset);
            uniqueRoots.emplace(baseX + offset, y, baseZ - 1);
            uniqueRoots.emplace(baseX + offset, y, baseZ + CHUNK_SIZE);
        }
    }

    std::deque<std::pair<glm::ivec3, LightLevel>> removalQueue;
    std::deque<glm::ivec3> additionQueue;
    std::vector<glm::ivec3> changedPositions;
    for (const glm::ivec3 &position : uniqueRoots) {
        const LightLevel previous =
            getBlockLightUnlocked(position.x, position.y, position.z);
        if (previous == MIN_LIGHT_LEVEL) {
            continue;
        }
        if (setBlockLightUnlocked(position.x, position.y, position.z,
                                  MIN_LIGHT_LEVEL)) {
            changedPositions.push_back(position);
            removalQueue.emplace_back(position, previous);
        }
    }

    removeBlockLight(removalQueue, additionQueue, changedPositions);
    queueLightingUpdates(changedPositions);
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

    const ChunkBlock previousBlock = chunk->getBlock(bp.x, y, bp.z);
    if (previousBlock == block) {
        return;
    }

    const LightLevel previousBlockLight =
        chunk->getBlockLight(bp.x, y, bp.z);
    chunk->setBlock(bp.x, y, bp.z, block);
    std::vector<glm::ivec3> changedPositions{{x, y, z}};
    for (int changedY : chunk->rebuildSunlightColumn(bp.x, bp.z)) {
        changedPositions.emplace_back(x, changedY, z);
    }
    relightBlockEdit({x, y, z}, previousBlockLight, changedPositions);
    queueLightingUpdates(changedPositions);
}

std::optional<BlockEntityRecord>
World::getBlockEntity(const glm::ivec3 &position)
{
    std::lock_guard<std::mutex> lock(m_mainMutex);
    const VectorXZ local = getBlockXZ(position.x, position.z);
    const VectorXZ chunkPosition = getChunkXZ(position.x, position.z);
    const Chunk *chunk =
        m_chunkManager.findChunk(chunkPosition.x, chunkPosition.z);
    if (chunk == nullptr || !chunk->hasLoaded()) {
        return std::nullopt;
    }

    const BlockEntityRecord *record =
        chunk->findBlockEntity({local.x, position.y, local.z});
    if (record == nullptr) {
        return std::nullopt;
    }

    BlockEntityRecord result = *record;
    result.position = position;
    return result;
}

bool World::createBlockEntity(const glm::ivec3 &position,
                              const std::string &type,
                              std::string payload)
{
    std::lock_guard<std::mutex> lock(m_mainMutex);
    const VectorXZ local = getBlockXZ(position.x, position.z);
    const VectorXZ chunkPosition = getChunkXZ(position.x, position.z);
    Chunk *chunk = m_chunkManager.findChunk(chunkPosition.x, chunkPosition.z);
    if (chunk == nullptr || !chunk->hasLoaded()) {
        return false;
    }

    return chunk->createBlockEntity(
        {{local.x, position.y, local.z}, type, std::move(payload)});
}

bool World::updateBlockEntity(const glm::ivec3 &position,
                              std::string payload)
{
    std::lock_guard<std::mutex> lock(m_mainMutex);
    const VectorXZ local = getBlockXZ(position.x, position.z);
    const VectorXZ chunkPosition = getChunkXZ(position.x, position.z);
    Chunk *chunk = m_chunkManager.findChunk(chunkPosition.x, chunkPosition.z);
    return chunk != nullptr && chunk->hasLoaded() &&
           chunk->updateBlockEntity({local.x, position.y, local.z},
                                    std::move(payload));
}

std::optional<BlockEntityRecord>
World::removeBlockEntity(const glm::ivec3 &position)
{
    std::lock_guard<std::mutex> lock(m_mainMutex);
    const VectorXZ local = getBlockXZ(position.x, position.z);
    const VectorXZ chunkPosition = getChunkXZ(position.x, position.z);
    Chunk *chunk = m_chunkManager.findChunk(chunkPosition.x, chunkPosition.z);
    if (chunk == nullptr || !chunk->hasLoaded()) {
        return std::nullopt;
    }

    std::optional<BlockEntityRecord> removed =
        chunk->removeBlockEntity({local.x, position.y, local.z});
    if (removed) {
        removed->position = position;
    }
    return removed;
}

void World::updateRandomTickSection(const glm::ivec3 &section, bool active)
{
    if (active) {
        if (m_randomTickSections.insert(section).second) {
            m_randomTickSectionQueue.push_back(section);
        }
        return;
    }

    if (m_randomTickSections.erase(section) == 0) {
        return;
    }

    m_randomTickSectionQueue.erase(
        std::remove_if(m_randomTickSectionQueue.begin(),
                       m_randomTickSectionQueue.end(),
                       [&section](const glm::ivec3 &queued) {
                           return queued.x == section.x &&
                                  queued.y == section.y &&
                                  queued.z == section.z;
                       }),
        m_randomTickSectionQueue.end());
}

void World::removeRandomTickSectionsForChunk(int chunkX, int chunkZ)
{
    std::vector<glm::ivec3> removed;
    for (const glm::ivec3 &section : m_randomTickSections) {
        if (section.x == chunkX && section.z == chunkZ) {
            removed.push_back(section);
        }
    }
    for (const glm::ivec3 &section : removed) {
        updateRandomTickSection(section, false);
    }
}

void World::runRandomTicks(int worldTime)
{
    struct RandomTickCandidate {
        glm::ivec3 position{0};
        ChunkBlock block;
    };

    std::vector<RandomTickCandidate> candidates;
    {
        std::unique_lock<std::mutex> lock(m_mainMutex);
        m_randomTickSectionsProcessed = 0;
        const std::size_t sectionsAtStart = m_randomTickSectionQueue.size();
        const std::size_t budget = std::min(
            RandomTickSectionBudgetPerTick, sectionsAtStart);
        candidates.reserve(budget);

        for (std::size_t index = 0; index < budget; ++index) {
            const glm::ivec3 sectionKey = m_randomTickSectionQueue.front();
            m_randomTickSectionQueue.pop_front();

            if (m_randomTickSections.count(sectionKey) == 0) {
                continue;
            }

            Chunk *chunk =
                m_chunkManager.findChunk(sectionKey.x, sectionKey.z);
            ChunkSection *section =
                chunk != nullptr && chunk->hasLoaded()
                    ? chunk->findSection(sectionKey.y)
                    : nullptr;
            if (section == nullptr ||
                section->getRandomTickBlockCount() == 0) {
                m_randomTickSections.erase(sectionKey);
                continue;
            }

            for (std::size_t attempt = 0;
                 attempt < RandomTickAttemptsPerSection; ++attempt) {
                RandomTickCandidate candidate;
                if (!section->selectRandomTickBlock(
                        randomTickBlockIndex(
                            m_chunkManager.getTerrainSeed(), worldTime,
                            sectionKey, attempt),
                        candidate.position, candidate.block)) {
                    continue;
                }
                const auto &definition =
                    BlockDatabase::get().getDefinition(
                        static_cast<BlockId>(candidate.block.id));
                if (definition.behavior != nullptr &&
                    definition.behavior->receivesRandomTicks(
                        definition, candidate.block)) {
                    candidates.push_back(candidate);
                }
            }
            m_randomTickSectionQueue.push_back(sectionKey);
            ++m_randomTickSectionsProcessed;
        }
    }

    std::size_t dispatched = 0;
    for (const RandomTickCandidate &candidate : candidates) {
        const ChunkBlock current = getBlock(candidate.position.x,
                                            candidate.position.y,
                                            candidate.position.z);
        if (current != candidate.block) {
            continue;
        }

        const auto &definition = BlockDatabase::get().getDefinition(
            static_cast<BlockId>(current.id));
        if (definition.behavior == nullptr ||
            !definition.behavior->receivesRandomTicks(definition, current)) {
            continue;
        }

        definition.behavior->onRandomTick(*this, candidate.position,
                                          current);
        ++dispatched;
    }

    if (dispatched > 0) {
        std::unique_lock<std::mutex> lock(m_mainMutex);
        m_randomTicksDispatched += dispatched;
    }
}

void World::tick(int worldTime)
{
    HELLOMINE3D_PROFILE_SCOPE("World::tick");
    m_worldSaveData.worldTime = static_cast<float>(worldTime);
    if (m_player != nullptr) {
        m_playerActor.syncFromPlayer(*m_player);
    }
    m_playerActor.tick(*this, 1.f / 20.f);
    m_actorManager.tick(*this, 1.f / 20.f);
    applyMobContactDamage();
    runRandomTicks(worldTime);
    runNaturalMobPopulation(worldTime);
}

bool World::attackActor(ActorId actorId, float amount)
{
    LivingActor *actor = dynamic_cast<LivingActor *>(
        m_actorManager.findActor(actorId));
    return actor != nullptr &&
           actor->damage(*this, amount, DefaultPlayerActorId);
}

bool World::damagePlayer(float amount, ActorId sourceId)
{
    if (m_player == nullptr) {
        return false;
    }

    m_playerActor.syncFromPlayer(*m_player);
    const bool accepted = m_playerActor.damage(*this, amount, sourceId);
    if (accepted && !m_playerActor.isAlive()) {
        respawnPlayer();
    }
    return accepted;
}

float World::getPlayerHealth() const
{
    return m_playerActor.getHealth();
}

float World::getPlayerMaxHealth() const
{
    return m_playerActor.getMaxHealth();
}

glm::vec3 World::getPlayerSpawnPoint() const
{
    return m_playerSpawnPoint;
}

void World::applyMobContactDamage()
{
    if (m_player == nullptr || !m_playerActor.isAlive()) {
        return;
    }

    const glm::vec3 playerDimensions = m_player->box.dimensions;
    for (const ActorSnapshot &snapshot : m_actorManager.collectSnapshots()) {
        const Actor *actor = m_actorManager.findActor(snapshot.id);
        if (dynamic_cast<const MobActor *>(actor) == nullptr) {
            continue;
        }

        const glm::vec3 distance = glm::abs(snapshot.position -
                                            m_player->position);
        const glm::vec3 reach = snapshot.dimensions + playerDimensions;
        if (distance.x <= reach.x && distance.y <= reach.y &&
            distance.z <= reach.z) {
            damagePlayer(MobContactDamage, snapshot.id);
            return;
        }
    }
}

void World::respawnPlayer()
{
    if (m_player == nullptr) {
        return;
    }

    m_player->closeContainer();
    m_player->position = m_playerSpawnPoint;
    m_player->velocity = glm::vec3(0.f);
    m_player->box.update(m_player->position);
    m_player->resetInterpolation();
    preloadChunksAround(m_player->position);
    m_playerActor.revive();
    m_playerActor.syncFromPlayer(*m_player);
    m_eventBus.publish(PlayerSpawnEvent(
        DefaultPlayerActorId, 0, m_player->position));
}

glm::ivec2 World::naturalMobSpawnOffset(int terrainSeed, int spawnEpoch,
                                        std::size_t attempt)
{
    const std::uint32_t first =
        naturalMobSelection(terrainSeed, spawnEpoch, attempt);
    const std::uint32_t second = naturalMobSelection(
        terrainSeed ^ 0x5bd1e995, spawnEpoch, attempt + 97);
    int x = static_cast<int>(first % 45u) - 22;
    int z = static_cast<int>(second % 45u) - 22;
    if (std::abs(x) < 8 && std::abs(z) < 8) {
        x += x < 0 ? -8 : 8;
    }
    return {x, z};
}

bool World::findSafeNaturalMobPosition(int blockX, int blockZ,
                                       glm::vec3 &position)
{
    std::unique_lock<std::mutex> lock(m_mainMutex);
    const VectorXZ local = getBlockXZ(blockX, blockZ);
    const VectorXZ chunkPosition = getChunkXZ(blockX, blockZ);
    const Chunk *chunk =
        m_chunkManager.findChunk(chunkPosition.x, chunkPosition.z);
    if (chunk == nullptr || !chunk->hasLoaded()) {
        return false;
    }

    const int feetY = chunk->getHeightAt(local.x, local.z) + 1;
    if (feetY <= 1 ||
        static_cast<BlockId>(getBlockUnlocked(blockX, feetY - 1,
                                              blockZ).id) ==
            BlockId::Air ||
        !getBlockUnlocked(blockX, feetY - 1, blockZ)
             .getData()
             .isCollidable ||
        static_cast<BlockId>(getBlockUnlocked(blockX, feetY, blockZ).id) !=
            BlockId::Air ||
        static_cast<BlockId>(
            getBlockUnlocked(blockX, feetY + 1, blockZ).id) != BlockId::Air) {
        return false;
    }

    position = {static_cast<float>(blockX) + 0.5f,
                static_cast<float>(feetY),
                static_cast<float>(blockZ) + 0.5f};
    return true;
}

void World::runNaturalMobPopulation(int worldTime)
{
    if (m_player == nullptr || worldTime <= 0 ||
        worldTime % NaturalMobSpawnIntervalTicks != 0) {
        return;
    }

    std::size_t worldCount =
        m_actorManager.countActorsByType(NaturalMobType);
    std::size_t localCount = m_actorManager.countActorsByTypeNear(
        NaturalMobType, m_player->position, NaturalMobLocalRadius);
    if (worldCount >= NaturalMobWorldCap ||
        localCount >= NaturalMobLocalCap) {
        return;
    }

    const int centerX = toBlockCoord(m_player->position.x);
    const int centerZ = toBlockCoord(m_player->position.z);
    const int spawnEpoch = worldTime / NaturalMobSpawnIntervalTicks;
    for (std::size_t attempt = 0;
         attempt < NaturalMobSpawnAttemptsPerCycle &&
         worldCount < NaturalMobWorldCap && localCount < NaturalMobLocalCap;
         ++attempt) {
        ++m_naturalMobSpawnAttempts;
        const glm::ivec2 offset = naturalMobSpawnOffset(
            m_chunkManager.getTerrainSeed(), spawnEpoch, attempt);
        glm::vec3 spawnPosition{0.f};
        if (!findSafeNaturalMobPosition(centerX + offset.x,
                                        centerZ + offset.y,
                                        spawnPosition) ||
            m_actorManager.hasActorByTypeNear(
                NaturalMobType, spawnPosition, 1.5f)) {
            continue;
        }

        if (spawnMob(NaturalMobType, spawnPosition) != InvalidActorId) {
            ++worldCount;
            ++localCount;
            ++m_naturalMobsSpawned;
        }
    }
}

void World::despawnNaturalMobsInChunk(int chunkX, int chunkZ)
{
    const std::size_t removed = m_actorManager.removeActorsIf(
        [chunkX, chunkZ](const Actor &actor) {
            if (actor.getType() != NaturalMobType) {
                return false;
            }
            const VectorXZ actorChunk = World::getChunkXZ(
                World::toBlockCoord(actor.position.x),
                World::toBlockCoord(actor.position.z));
            return actorChunk.x == chunkX && actorChunk.z == chunkZ;
        });
    m_naturalMobsDespawned += removed;
}

// loads chunks
// make chunk meshes
void World::update(const Camera &camera)
{
    HELLOMINE3D_PROFILE_SCOPE("World::update");
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

void World::setRenderDistance(int renderDistance) noexcept
{
    const int previous = m_renderDistance.exchange(renderDistance);
    if (previous == renderDistance) {
        return;
    }
    m_unloadScanValid = false;
    m_unloadBacklog = true;
    m_chunkLoadRevision.fetch_add(1);
}

int World::getRenderDistance() const noexcept
{
    return m_renderDistance.load();
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
    HELLOMINE3D_PROFILE_THREAD("Chunk Loader");
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
            const int radius = std::max(1, m_renderDistance.load());
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

        HELLOMINE3D_PROFILE_SCOPE("World::loadChunks pass");
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
                builtMeshes.transparentMesh.clearClientData();
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
    RuntimeOperationTimings &operationTimings = runtimeOperationTimings();
    const RuntimeOperationHandle operation =
        operationTimings.begin(RuntimeOperationKind::Save);
    const auto finish = [&](bool result) {
        operationTimings.complete(operation, result);
        return result;
    };
    std::unique_lock<std::mutex> lock(m_mainMutex);
    const bool chunksSaved = m_chunkManager.saveDirtyChunks();
    const bool worldSaved = saveWorldState();
    if (!chunksSaved || !worldSaved) {
        return finish(false);
    }
    WorldBackupMetrics backupMetrics;
    if (!m_worldBackup.createBackup(nullptr, &backupMetrics)) {
        std::cerr << "Unable to create world backup: "
                  << backupMetrics.error << '\n';
        return finish(false);
    }
    return finish(true);
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
    stats.chunks.saveTransactions += m_worldSaveTransactionCount;
    stats.chunks.saveTotalMs += m_worldSaveTotalMs;
    stats.chunks.saveMaxMs =
        std::max(stats.chunks.saveMaxMs, m_worldSaveMaxMs);
    stats.actorCount = m_actorManager.getActorCount();
    stats.naturalMobCount =
        m_actorManager.countActorsByType(NaturalMobType);
    stats.naturalMobWorldCap = NaturalMobWorldCap;
    stats.naturalMobLocalCap = NaturalMobLocalCap;
    stats.naturalMobSpawnAttempts = m_naturalMobSpawnAttempts;
    stats.naturalMobsSpawned = m_naturalMobsSpawned;
    stats.naturalMobsDespawned = m_naturalMobsDespawned;
    stats.playerHealth = m_playerActor.getHealth();
    stats.playerMaxHealth = m_playerActor.getMaxHealth();
    stats.queuedChunkUpdates = m_chunkUpdateQueue.size();
    stats.randomTickSections = m_randomTickSections.size();
    for (const glm::ivec3 &sectionKey : m_randomTickSections) {
        const Chunk *chunk =
            m_chunkManager.findChunk(sectionKey.x, sectionKey.z);
        const ChunkSection *section =
            chunk != nullptr && chunk->hasLoaded()
                ? chunk->findSection(sectionKey.y)
                : nullptr;
        if (section != nullptr) {
            stats.randomTickBlocks += section->getRandomTickBlockCount();
        }
    }
    stats.randomTickSectionsProcessed = m_randomTickSectionsProcessed;
    stats.randomTicksDispatched = m_randomTicksDispatched;
    stats.terrainSeed = m_chunkManager.getTerrainSeed();
    stats.worldTime = m_worldSaveData.worldTime;
    stats.environment = WorldEnvironment::evaluate(stats.worldTime);
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
    mob->setChaseTarget(m_player);
    return m_actorManager.addActor(std::move(mob), *this);
}

void World::queueChunkUpdate(int blockX, int blockY, int blockZ)
{
    for (const auto &update :
         ChunkUpdatePlanner::planForBlockEdit(blockX, blockY, blockZ)) {
        queueSectionUpdate({update.x, update.y, update.z});
    }
}

void World::queueSectionUpdate(const glm::ivec3 &key)
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
        m_chunkUpdateQueue.push_back(key);
    }
}

void World::queueLightingUpdates(
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
        queueSectionUpdate(section);
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

std::size_t World::randomTickBlockIndex(int terrainSeed, int worldTime,
                                        const glm::ivec3 &section,
                                        std::size_t attempt)
{
    return randomTickSelection(terrainSeed, worldTime, section, attempt) %
           CHUNK_VOLUME;
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
    HELLOMINE3D_PROFILE_SCOPE("World::updateChunks");
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
    const std::int64_t now = currentUtcSeconds();
    if (m_worldSaveData.createdUtc < LegacyWorldTimestampUtc) {
        m_worldSaveData.createdUtc = now;
    }
    m_worldSaveData.lastPlayedUtc =
        std::max(m_worldSaveData.createdUtc, now);
    m_worldSaveData.lastBuildIdentity = currentBuildIdentity();
    m_worldSaveData.version = WorldSaveFormatVersion;
    m_worldSaveData.spawnPoint = m_playerSpawnPoint;
    if (m_player != nullptr) {
        m_worldSaveData.playerState = m_player->getSaveState();
        m_worldSaveData.hasPlayerState = true;
    }
    m_worldSaveData.actors = m_actorManager.collectSaveStates();

    StorageTransactionMetrics metrics;
    if (!m_worldSave.save(m_worldSaveData, {}, &metrics)) {
        runtimeOperationTimings().addStorageTransactionToLatest(
            RuntimeOperationKind::Save,
            metrics.prepareCompleteMilliseconds,
            metrics.writeCompleteMilliseconds,
            metrics.flushCompleteMilliseconds,
            metrics.validationCompleteMilliseconds,
            metrics.replaceCompleteMilliseconds,
            metrics.totalMilliseconds, metrics.bytesWritten, false);
        return false;
    }
    runtimeOperationTimings().addStorageTransactionToLatest(
        RuntimeOperationKind::Save, metrics.prepareCompleteMilliseconds,
        metrics.writeCompleteMilliseconds,
        metrics.flushCompleteMilliseconds,
        metrics.validationCompleteMilliseconds,
        metrics.replaceCompleteMilliseconds, metrics.totalMilliseconds,
        metrics.bytesWritten, false);
    ++m_worldSaveTransactionCount;
    const double elapsed = std::max(0.0, metrics.totalMilliseconds);
    m_worldSaveTotalMs += elapsed;
    m_worldSaveMaxMs = std::max(m_worldSaveMaxMs, elapsed);
    return true;
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
                materialId >= Material::ID::Count ||
                state.amount <= 0) {
                continue;
            }
            actor = std::make_unique<ItemEntity>(
                state.id, materialId, state.amount, state.position);
        }
        else if (state.kind == ActorSaveKind::Mob) {
            if (state.type == NaturalMobType) {
                const VectorXZ actorChunk = getChunkXZ(
                    toBlockCoord(state.position.x),
                    toBlockCoord(state.position.z));
                if (!m_chunkManager.chunkLoadedAt(actorChunk.x,
                                                  actorChunk.z) ||
                    m_actorManager.hasActorByTypeNear(
                        NaturalMobType, state.position, 1.5f)) {
                    continue;
                }
            }
            auto mob = std::make_unique<MobActor>(
                state.id, state.type, state.position);
            mob->setChaseTarget(m_player);
            actor = std::move(mob);
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
