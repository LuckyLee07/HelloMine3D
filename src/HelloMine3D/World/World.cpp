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
#include <stdexcept>
#include <utility>
#include <vector>

#include "../Actor/EnemyRegistry.h"
#include "../Actor/ItemEntity.h"
#include "../Actor/MobActor.h"
#include "../Core/Camera.h"
#include "../Diagnostics/RuntimeProfiler.h"
#include "../Diagnostics/OperationPerformanceTiming.h"
#include "../Maths/Vector2XZ.h"
#include "../Physics/AABB.h"
#include "../Player/Player.h"
#include "../Sandbox/Events/FoodEvents.h"
#include "../Sandbox/Events/EntityEvents.h"
#include "../Sandbox/Events/PlayerEvents.h"
#include "../Sandbox/Events/WaystoneEvents.h"
#include "../Util/ResourcePaths.h"
#include "../Util/Random.h"
#include "Chunk/ChunkMeshBuilder.h"
#include "Chunk/ChunkUpdatePlanner.h"
#include "Block/BlockDatabase.h"
#include "Block/FurnaceContainer.h"
#include "../Item/FoodRegistry.h"
#include "../Item/SmeltingRegistry.h"
#include "../Item/ToolRegistry.h"
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

    constexpr int MaximumSpawnSearchChunkRadius = 200;
    constexpr int MaximumLoadedSpawnCandidates = 64;
    constexpr float LegacyPlaceholderRescueMaxWorldTime = 1200.f;
    constexpr float SpawnComparisonEpsilon = 0.01f;

    bool sameBlockPosition(const glm::ivec3 &left,
                           const glm::ivec3 &right) noexcept
    {
        return left.x == right.x && left.y == right.y &&
               left.z == right.z;
    }

    bool isWaystoneGuardianType(const std::string &type) noexcept
    {
        return type == WaystoneEncounter::StalkerType ||
               type == WaystoneEncounter::BruteType;
    }

    bool approximatelyEqual(float left, float right)
    {
        return std::abs(left - right) <= SpawnComparisonEpsilon;
    }

    bool inventoryIsEmpty(const PlayerSaveState &state)
    {
        return std::all_of(
            state.inventory.begin(), state.inventory.end(),
            [](const InventorySlotState &slot) {
                return slot.materialId == Material::ID::Nothing ||
                       slot.amount <= 0;
            });
    }

    bool shouldRescueLegacyPlaceholder(const WorldSaveData &data)
    {
        if (!data.hasPlayerState || data.worldTime < 0.f ||
            data.worldTime > LegacyPlaceholderRescueMaxWorldTime ||
            data.alphaJourneyFlags != 0u ||
            !data.objectiveState.completedIds.empty() ||
            !data.objectiveState.progress.empty() ||
            !inventoryIsEmpty(data.playerState)) {
            return false;
        }

        const glm::vec3 placeholder = initialWorldSpawnPlaceholder();
        return approximatelyEqual(data.spawnPoint.x, placeholder.x) &&
               approximatelyEqual(data.spawnPoint.y, placeholder.y) &&
               approximatelyEqual(data.spawnPoint.z, placeholder.z) &&
               approximatelyEqual(data.playerState.position.x,
                                  placeholder.x) &&
               approximatelyEqual(data.playerState.position.z,
                                  placeholder.z) &&
               data.playerState.position.y <=
                   placeholder.y + SpawnComparisonEpsilon;
    }

    bool isPreferredSpawnBiome(TerrainBiome biome)
    {
        return biome == TerrainBiome::LightForest ||
               biome == TerrainBiome::TemperateForest;
    }

    bool isNaturalSpawnGround(BlockId block)
    {
        switch (block) {
            case BlockId::Grass:
            case BlockId::Dirt:
            case BlockId::Stone:
            case BlockId::Sand:
            case BlockId::CoalOre:
            case BlockId::IronOre:
                return true;
            default:
                return false;
        }
    }

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

    std::unique_ptr<MobActor> createMobActor(
        ActorId id, const std::string &type, const glm::vec3 &position)
    {
        const EnemyDefinition *definition =
            runtimeEnemyRegistry().isFrozen()
                ? runtimeEnemyRegistry().find(type)
                : nullptr;
        auto mob = std::make_unique<MobActor>(
            id, type, position,
            definition != nullptr ? definition->maxHealth : 10.f,
            definition != nullptr
                ? definition->dimensions
                : glm::vec3(0.35f, 0.9f, 0.35f));
        if (definition != nullptr) {
            mob->applyDefinition(*definition);
        }
        return mob;
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
    ensureRuntimeEnemyRegistry();

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

    bool persistInitializedSpawn = false;
    const bool hasSave = m_worldSave.load(m_worldSaveData);
    if (hasSave) {
        m_chunkManager.setTerrainIdentity(
            m_worldSaveData.seed,
            m_worldSaveData.terrainGenerationVersion);
        m_playerSpawnPoint = m_worldSaveData.spawnPoint;
        if (m_worldSaveData.hasPlayerState) {
            player.applySaveState(m_worldSaveData.playerState);
        }
        else {
            player.position = m_playerSpawnPoint;
        }

        const bool firstPlayerEntry = !m_worldSaveData.hasPlayerState;
        const bool rescuePlaceholder =
            shouldRescueLegacyPlaceholder(m_worldSaveData);
        if (firstPlayerEntry || rescuePlaceholder) {
            if (rescuePlaceholder) {
                std::cout << "Repairing unstarted placeholder world at seed "
                          << m_worldSaveData.seed << '\n';
                auto &actors = m_worldSaveData.actors;
                actors.erase(
                    std::remove_if(
                        actors.begin(), actors.end(),
                        [](const ActorSaveState &actor) {
                            return World::isNaturalMobType(actor.type);
                        }),
                    actors.end());
            }
            setSpawnPoint();
            player.position = m_playerSpawnPoint;
            player.velocity = glm::vec3(0.f);
            player.box.update(player.position);
            m_worldSaveData.spawnPoint = m_playerSpawnPoint;
            persistInitializedSpawn = true;
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
        m_chunkManager.setTerrainIdentity(
            m_worldSaveData.seed,
            m_worldSaveData.terrainGenerationVersion);

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
        m_worldSaveData.playerState.position = player.position;
        m_worldSaveData.playerState.rotation = player.rotation;
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
    if (hasSave && m_worldSaveData.hasPlayerState) {
        ActorSaveState playerActorState = m_playerActor.getSaveState();
        playerActorState.health = m_worldSaveData.playerState.health;
        playerActorState.alive = playerActorState.health > 0.f;
        m_playerActor.applySaveState(playerActorState);
        m_foodCooldownTicksRemaining =
            m_worldSaveData.playerState.foodCooldownTicks;
        m_attackCooldownTicksRemaining =
            m_worldSaveData.playerState.attackCooldownTicks;
        m_playerRespawnPending = !m_playerActor.isAlive();
    }
    ensureRuntimeObjectiveRegistry();
    m_alphaJourney = std::make_unique<AlphaJourney>(
        player, m_eventBus, m_worldSaveData.objectiveState,
        m_worldSaveData.alphaJourneyFlags, hasSave);
    m_victoryFlow = std::make_unique<VictoryFlow>(
        m_worldSaveData.worldOutcome);
    m_eventBus.subscribe(
        SandboxEventType::EntityDeath,
        [this](const SandboxEvent &event) {
            handleWaystoneGuardianDeath(event);
        });
    reconcileWaystoneEncounter();

    // Restore retained actors and objectives before persisting the repaired
    // spawn so empty runtime managers cannot erase non-natural save state.
    if (persistInitializedSpawn && !saveWorldState()) {
        throw std::runtime_error(
            "Cannot persist the initialized player spawn point.");
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

std::vector<glm::ivec3> World::collectLoadedBlockEntityPositions(
    const std::string &type)
{
    std::lock_guard<std::mutex> lock(m_mainMutex);
    std::vector<glm::ivec3> positions;
    for (const auto &entry : m_chunkManager.getChunks()) {
        const Chunk &chunk = entry.second;
        if (!chunk.hasLoaded()) {
            continue;
        }
        for (const BlockEntityRecord &record : chunk.getBlockEntities()) {
            if (record.type == type) {
                positions.emplace_back(
                    entry.first.x * CHUNK_SIZE + record.position.x,
                    record.position.y,
                    entry.first.z * CHUNK_SIZE + record.position.z);
            }
        }
    }
    return positions;
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
    m_combatRaycastsUsed = 0;
    m_combatRaycastBudgetDenied = 0;
    m_combatChaseStepsUsed = 0;
    m_combatChaseStepBudgetDenied = 0;
    if (m_playerGuardRecoverTicksRemaining > 0) {
        --m_playerGuardRecoverTicksRemaining;
    }
    if (m_playerCombatFeedback.ticksRemaining > 0) {
        --m_playerCombatFeedback.ticksRemaining;
        if (m_playerCombatFeedback.ticksRemaining == 0) {
            m_playerCombatFeedback.kind = PlayerCombatFeedbackKind::None;
            m_playerCombatFeedback.direction = CombatDirection::None;
            m_playerCombatFeedback.sourceId = InvalidActorId;
        }
    }
    if (m_player != nullptr) {
        m_playerActor.syncFromPlayer(*m_player);
    }
    m_playerActor.tick(*this, 1.f / 20.f);
    m_foodCooldownTicksRemaining =
        std::max(0, m_foodCooldownTicksRemaining - 1);
    m_attackCooldownTicksRemaining =
        std::max(0, m_attackCooldownTicksRemaining - 1);
    m_actorManager.tick(*this, 1.f / 20.f);
    reconcileWaystoneEncounter();
    runRandomTicks(worldTime);
    runNaturalMobPopulation(worldTime);
    if (runtimeSmeltingRegistry().isFrozen()) {
        FurnaceContainer::tickLoaded(*this, runtimeSmeltingRegistry());
    }
    if (m_alphaJourney != nullptr) {
        m_alphaJourney->update(1.f / 20.f);
    }
    if (m_playerRespawnPending) {
        respawnPlayer();
    }
}

bool World::attackActor(ActorId actorId)
{
    return tryAttackActor(actorId) == CombatAttackResult::Hit;
}

CombatAttackResult World::tryAttackActor(ActorId actorId,
                                         bool simulationRunning)
{
    if (!simulationRunning) {
        return CombatAttackResult::SimulationPaused;
    }
    if (m_player == nullptr) {
        return CombatAttackResult::PlayerUnavailable;
    }
    if (m_player->hasOpenContainer() || m_player->hasOpenCrafting()) {
        return CombatAttackResult::UiBusy;
    }
    if (!m_playerActor.isAlive()) {
        return CombatAttackResult::PlayerDead;
    }
    if (m_attackCooldownTicksRemaining > 0) {
        return CombatAttackResult::CoolingDown;
    }

    LivingActor *actor = dynamic_cast<LivingActor *>(
        m_actorManager.findActor(actorId));
    if (actor == nullptr) {
        return CombatAttackResult::TargetMissing;
    }
    if (!actor->isAlive()) {
        return CombatAttackResult::TargetDead;
    }

    float amount = PlayerAttackDamage;
    int cooldownTicks = PlayerAttackCooldownTicks;
    float reach = PlayerAttackReach;
    bool usesTool = false;
    if (runtimeToolRegistry().isFrozen()) {
        const ItemStack &held = m_player->getHeldItems();
        if (!held.isEmpty()) {
            const ToolDefinition *tool =
                runtimeToolRegistry().find(held.getMaterial().id);
            if (tool != nullptr) {
                amount = tool->attackDamage;
                cooldownTicks = tool->attackCooldownTicks;
                reach = tool->attackReach;
                usesTool = true;
            }
        }
    }

    const glm::vec3 separation = glm::max(
        glm::abs(actor->position - m_player->position) -
            actor->box.dimensions - m_player->box.dimensions,
        glm::vec3(0.f));
    if (glm::length(separation) > reach) {
        return CombatAttackResult::OutOfReach;
    }

    const bool accepted = actor->damage(
        *this, amount, DefaultPlayerActorId);
    if (!accepted) {
        return CombatAttackResult::TargetRejected;
    }
    actor->setDamageInvulnerabilityRemaining(
        static_cast<float>(cooldownTicks) / 20.f);
    m_attackCooldownTicksRemaining = cooldownTicks;
    if (actor->isAlive()) {
        if (auto *mob = dynamic_cast<MobActor *>(actor)) {
            mob->interruptByPlayerHit(
                *this, m_player->position, MobPlayerHitKnockback,
                MobPlayerHitRecoverTicks);
        }
    }
    if (accepted && usesTool) {
        m_player->damageHeldTool();
    }
    return CombatAttackResult::Hit;
}

bool World::attackActor(ActorId actorId, float amount)
{
    LivingActor *actor = dynamic_cast<LivingActor *>(
        m_actorManager.findActor(actorId));
    const bool accepted = actor != nullptr &&
        actor->damage(*this, amount, DefaultPlayerActorId);
    if (accepted && actor->isAlive() && m_player != nullptr) {
        if (auto *mob = dynamic_cast<MobActor *>(actor)) {
            mob->interruptByPlayerHit(
                *this, m_player->position, MobPlayerHitKnockback,
                MobPlayerHitRecoverTicks);
        }
    }
    return accepted;
}

bool World::damagePlayer(float amount, ActorId sourceId)
{
    if (m_player == nullptr) {
        return false;
    }

    m_playerActor.syncFromPlayer(*m_player);
    const bool accepted = m_playerActor.damage(*this, amount, sourceId);
    if (accepted) {
        const Actor *source = m_actorManager.findActor(sourceId);
        recordPlayerCombatFeedback(
            PlayerCombatFeedbackKind::Damage, sourceId,
            source != nullptr ? source->position : m_player->position);
    }
    if (accepted && !m_playerActor.isAlive()) {
        m_playerRespawnPending = true;
        abandonWaystoneEncounter();
    }
    return accepted;
}

FoodUseResult World::useHeldFood(bool simulationRunning)
{
    if (!simulationRunning) {
        return FoodUseResult::SimulationPaused;
    }
    if (m_player == nullptr) {
        return FoodUseResult::PlayerUnavailable;
    }
    if (m_player->hasOpenContainer() || m_player->hasOpenCrafting()) {
        return FoodUseResult::UiBusy;
    }
    if (!m_playerActor.isAlive()) {
        return FoodUseResult::PlayerDead;
    }
    if (m_foodCooldownTicksRemaining > 0) {
        return FoodUseResult::CoolingDown;
    }

    const ItemStack &held = m_player->getHeldItems();
    if (held.isEmpty()) {
        return FoodUseResult::EmptyHand;
    }
    const Material::ID materialId = held.getMaterial().id;
    const FoodDefinition *food = runtimeFoodRegistry().find(materialId);
    if (food == nullptr) {
        return FoodUseResult::NotFood;
    }
    if (m_playerActor.getHealth() >= m_playerActor.getMaxHealth()) {
        return FoodUseResult::FullHealth;
    }
    if (!m_player->removeHeldItem()) {
        return FoodUseResult::InventoryRejected;
    }

    const float restored = m_playerActor.heal(food->healthRestored);
    if (restored <= 0.f) {
        m_player->addItem(Material::toMaterial(materialId), 1);
        return FoodUseResult::InventoryRejected;
    }
    m_foodCooldownTicksRemaining = food->cooldownTicks;
    m_eventBus.publish(FoodConsumedEvent(
        DefaultPlayerActorId, materialId, restored,
        m_playerActor.getHealth(), m_player->position));
    m_eventBus.publish(PlayerInventoryChangedEvent(
        DefaultPlayerActorId, materialId, -1, "food_consumed"));
    return FoodUseResult::Consumed;
}

float World::getPlayerHealth() const
{
    return m_playerActor.getHealth();
}

float World::getPlayerMaxHealth() const
{
    return m_playerActor.getMaxHealth();
}

int World::getFoodCooldownTicksRemaining() const noexcept
{
    return m_foodCooldownTicksRemaining;
}

int World::getAttackCooldownTicksRemaining() const noexcept
{
    return m_attackCooldownTicksRemaining;
}

glm::vec3 World::getPlayerSpawnPoint() const
{
    return m_playerSpawnPoint;
}

AlphaJourneySnapshot World::getAlphaJourneySnapshot() const
{
    return m_alphaJourney != nullptr
               ? m_alphaJourney->snapshot()
               : AlphaJourneySnapshot{};
}

ObjectiveSnapshot World::getObjectiveSnapshot() const
{
    return m_alphaJourney != nullptr
               ? m_alphaJourney->objectiveSnapshot()
               : ObjectiveSnapshot{};
}

WorldOutcomeSnapshot World::getWorldOutcomeSnapshot() const noexcept
{
    return m_victoryFlow != nullptr
               ? m_victoryFlow->snapshot()
               : WorldOutcomeSnapshot{};
}

WaystoneEncounterSnapshot World::getWaystoneEncounterSnapshot() const
{
    WaystoneEncounterSnapshot result;
    result.wave = m_waystoneEncounterState.wave;
    result.remainingGuardians =
        m_waystoneEncounterState.remainingGuardians;
    result.loadedGuardians =
        m_actorManager.countActorsByType(WaystoneEncounter::StalkerType) +
        m_actorManager.countActorsByType(WaystoneEncounter::BruteType);
    if (m_waystoneAnchor.has_value()) {
        result.anchorKnown = true;
        result.anchor = *m_waystoneAnchor;
    }
    return result;
}

bool World::initializeWaystone(const glm::ivec3 &position)
{
    if (static_cast<BlockId>(getBlock(position.x, position.y,
                                      position.z).id) !=
        BlockId::WaystoneCore) {
        return false;
    }
    const auto existing = getBlockEntity(position);
    if (existing.has_value()) {
        WaystoneEncounterState state;
        return existing->type == WaystoneEncounter::BlockEntityType &&
               WaystoneEncounter::deserialize(existing->payload, state);
    }
    return createBlockEntity(
        position, WaystoneEncounter::BlockEntityType,
        WaystoneEncounter::serialize(WaystoneEncounterState{}));
}

bool World::readWaystoneState(const glm::ivec3 &position,
                              WaystoneEncounterState &state)
{
    const auto record = getBlockEntity(position);
    return record.has_value() &&
           record->type == WaystoneEncounter::BlockEntityType &&
           WaystoneEncounter::deserialize(record->payload, state);
}

bool World::writeWaystoneState(const glm::ivec3 &position,
                               const WaystoneEncounterState &state)
{
    return WaystoneEncounter::validState(state) &&
           updateBlockEntity(position,
                             WaystoneEncounter::serialize(state));
}

void World::setWaystoneFeedback(WaystoneActionResult result)
{
    m_waystoneFeedbackKey = WaystoneEncounter::feedbackKey(result);
}

std::string World::consumeWaystoneFeedbackKey()
{
    std::string result = std::move(m_waystoneFeedbackKey);
    m_waystoneFeedbackKey.clear();
    return result;
}

WaystoneActionResult World::useWaystone(const glm::ivec3 &position,
                                        Player &player,
                                        bool simulationRunning)
{
    const auto finish = [this](WaystoneActionResult result) {
        setWaystoneFeedback(result);
        return result;
    };
    if (!simulationRunning) {
        return finish(WaystoneActionResult::SimulationPaused);
    }
    if (m_player == nullptr || &player != m_player) {
        return finish(WaystoneActionResult::PlayerUnavailable);
    }
    if (!m_playerActor.isAlive()) {
        return finish(WaystoneActionResult::PlayerDead);
    }
    if (player.hasOpenContainer() || player.hasOpenCrafting()) {
        return finish(WaystoneActionResult::UiBusy);
    }
    const glm::vec3 coreCenter =
        glm::vec3(position) + glm::vec3(0.5f);
    const glm::vec3 distance = player.position - coreCenter;
    if (glm::dot(distance, distance) >
        WaystoneEncounter::InteractionReach *
            WaystoneEncounter::InteractionReach) {
        return finish(WaystoneActionResult::OutOfReach);
    }
    if (!initializeWaystone(position)) {
        return finish(WaystoneActionResult::InvalidCore);
    }

    WaystoneEncounterState persisted;
    if (!readWaystoneState(position, persisted) ||
        m_victoryFlow == nullptr) {
        return finish(WaystoneActionResult::InvalidCore);
    }
    const WorldOutcomeSnapshot outcome = m_victoryFlow->snapshot();
    if (outcome.phase == WorldOutcomePhase::Unstarted) {
        if (persisted.wave != 0) {
            return finish(WaystoneActionResult::Rejected);
        }
        if (m_alphaJourney != nullptr) {
            m_alphaJourney->update(0.f);
        }
        const std::vector<InventorySlotState> ritual = {{
            Material::ID::IronIngot,
            WaystoneEncounter::ActivationIronIngots, 0}};
        const std::uint64_t revision = player.getInventoryRevision();
        if (!player.canConsumeInventory(ritual)) {
            return finish(WaystoneActionResult::MissingMaterials);
        }
        if (!player.consumeInventory(ritual, revision)) {
            return finish(WaystoneActionResult::Rejected);
        }
        if (m_victoryFlow->activate() !=
            VictoryTransitionResult::Applied) {
            player.addItem(Material::IRON_INGOT,
                           WaystoneEncounter::ActivationIronIngots);
            return finish(WaystoneActionResult::Rejected);
        }
        m_waystoneAnchor = position;
        m_waystoneEncounterState = {};
        m_eventBus.publish(PlayerInventoryChangedEvent(
            DefaultPlayerActorId, Material::ID::IronIngot,
            -WaystoneEncounter::ActivationIronIngots,
            "waystone_activation"));
        m_eventBus.publish(WaystoneActivatedEvent(
            DefaultPlayerActorId, position));
        return finish(WaystoneActionResult::Activated);
    }
    if (outcome.phase == WorldOutcomePhase::Activated) {
        m_waystoneAnchor = position;
        if (persisted.wave != 0) {
            persisted = {};
            if (!writeWaystoneState(position, persisted)) {
                return finish(WaystoneActionResult::Rejected);
            }
        }
        std::vector<glm::vec3> positions;
        if (!findWaystoneSpawnPositions(
                position, WaystoneEncounter::FirstWaveGuardians,
                positions)) {
            return finish(WaystoneActionResult::SpawnBlocked);
        }
        if (m_victoryFlow->beginEncounter() !=
            VictoryTransitionResult::Applied) {
            return finish(WaystoneActionResult::Rejected);
        }
        m_waystoneEncounterState = {
            1, WaystoneEncounter::FirstWaveGuardians, 0};
        if (!writeWaystoneState(position, m_waystoneEncounterState) ||
            !spawnWaystoneGuardians(
                1, WaystoneEncounter::FirstWaveGuardians)) {
            abandonWaystoneEncounter();
            return finish(WaystoneActionResult::SpawnBlocked);
        }
        return finish(WaystoneActionResult::EncounterStarted);
    }
    if (outcome.phase == WorldOutcomePhase::Encounter) {
        return finish(
            m_waystoneAnchor.has_value() &&
                    sameBlockPosition(*m_waystoneAnchor, position)
                ? WaystoneActionResult::EncounterInProgress
                : WaystoneActionResult::Rejected);
    }
    if (outcome.phase == WorldOutcomePhase::Victorious) {
        return claimWaystoneReward(simulationRunning);
    }
    if (outcome.phase == WorldOutcomePhase::RewardClaimed) {
        return finish(WaystoneActionResult::RewardAlreadyClaimed);
    }
    return finish(WaystoneActionResult::Rejected);
}

WaystoneActionResult World::claimWaystoneReward(bool simulationRunning)
{
    const auto finish = [this](WaystoneActionResult result) {
        setWaystoneFeedback(result);
        return result;
    };
    if (!simulationRunning) {
        return finish(WaystoneActionResult::SimulationPaused);
    }
    if (m_player == nullptr || m_victoryFlow == nullptr) {
        return finish(WaystoneActionResult::PlayerUnavailable);
    }
    if (!m_playerActor.isAlive()) {
        return finish(WaystoneActionResult::PlayerDead);
    }
    const WorldOutcomeSnapshot outcome = m_victoryFlow->snapshot();
    if (outcome.phase == WorldOutcomePhase::RewardClaimed) {
        return finish(WaystoneActionResult::RewardAlreadyClaimed);
    }
    if (outcome.phase != WorldOutcomePhase::Victorious ||
        outcome.rewardEpoch != WaystoneEncounter::RewardEpoch) {
        return finish(WaystoneActionResult::Rejected);
    }
    const Material &reward = Material::IRON_INGOT;
    if (m_player->getInventoryCapacity(reward) <
        WaystoneEncounter::RewardIronIngots) {
        return finish(WaystoneActionResult::InventoryFull);
    }
    const int rewarded = m_player->addItem(
        reward, WaystoneEncounter::RewardIronIngots);
    if (rewarded != WaystoneEncounter::RewardIronIngots) {
        if (rewarded > 0) {
            const std::vector<InventorySlotState> rollback = {{
                Material::ID::IronIngot, rewarded, 0}};
            m_player->consumeInventory(
                rollback, m_player->getInventoryRevision());
        }
        return finish(WaystoneActionResult::InventoryFull);
    }
    if (m_victoryFlow->claimReward(outcome.rewardEpoch) !=
        VictoryTransitionResult::Applied) {
        const std::vector<InventorySlotState> rollback = {{
            Material::ID::IronIngot,
            WaystoneEncounter::RewardIronIngots, 0}};
        m_player->consumeInventory(
            rollback, m_player->getInventoryRevision());
        return finish(WaystoneActionResult::Rejected);
    }
    if (m_waystoneAnchor.has_value()) {
        WaystoneEncounterState claimed = {
            3, 0, WaystoneEncounter::RewardEpoch};
        if (readWaystoneState(*m_waystoneAnchor,
                              m_waystoneEncounterState)) {
            writeWaystoneState(*m_waystoneAnchor, claimed);
        }
        m_waystoneEncounterState = claimed;
    }
    m_eventBus.publish(PlayerInventoryChangedEvent(
        DefaultPlayerActorId, Material::ID::IronIngot,
        WaystoneEncounter::RewardIronIngots, "waystone_victory_reward"));
    m_eventBus.publish(VictoryRewardClaimedEvent(
        DefaultPlayerActorId, outcome.rewardEpoch));
    return finish(WaystoneActionResult::RewardClaimed);
}

bool World::findWaystoneSpawnPositions(
    const glm::ivec3 &anchor, int count,
    std::vector<glm::vec3> &positions)
{
    positions.clear();
    if (count <= 0 ||
        count > static_cast<int>(WaystoneEncounter::MaximumLoadedGuardians)) {
        return false;
    }
    static const std::array<glm::ivec2, 12> offsets = {{
        {6, 0}, {-6, 0}, {0, 6}, {0, -6},
        {5, 5}, {-5, 5}, {5, -5}, {-5, -5},
        {7, 2}, {-7, 2}, {2, 7}, {2, -7}}};
    const std::vector<ActorSnapshot> actors =
        m_actorManager.collectSnapshots();
    for (const glm::ivec2 &offset : offsets) {
        glm::vec3 candidate{0.f};
        if (!findSafeNaturalMobPosition(anchor.x + offset.x,
                                        anchor.z + offset.y,
                                        candidate)) {
            continue;
        }
        bool occupied = false;
        for (const ActorSnapshot &actor : actors) {
            if (glm::distance(actor.position, candidate) <= 1.5f) {
                occupied = true;
                break;
            }
        }
        for (const glm::vec3 &selected : positions) {
            if (glm::distance(selected, candidate) <= 1.5f) {
                occupied = true;
                break;
            }
        }
        if (occupied) {
            continue;
        }
        positions.push_back(candidate);
        if (static_cast<int>(positions.size()) == count) {
            return true;
        }
    }
    positions.clear();
    return false;
}

bool World::spawnWaystoneGuardians(int wave, int count)
{
    if (!m_waystoneAnchor.has_value() || count <= 0) {
        return false;
    }
    std::vector<glm::vec3> positions;
    if (!findWaystoneSpawnPositions(*m_waystoneAnchor, count,
                                    positions)) {
        return false;
    }
    const char *type = WaystoneEncounter::actorTypeForWave(wave);
    if (type[0] == '\0') {
        return false;
    }
    for (const glm::vec3 &position : positions) {
        const ActorId id = spawnMob(type, position);
        if (id == InvalidActorId) {
            return false;
        }
        m_waystoneGuardianIds.emplace(id);
    }
    return true;
}

void World::removeWaystoneGuardians()
{
    m_actorManager.removeActorsIf([](const Actor &actor) {
        return isWaystoneGuardianType(actor.getType());
    });
    m_waystoneGuardianIds.clear();
}

void World::abandonWaystoneEncounter()
{
    if (m_victoryFlow == nullptr ||
        m_victoryFlow->snapshot().phase != WorldOutcomePhase::Encounter) {
        return;
    }
    if (m_waystoneAnchor.has_value()) {
        const WaystoneEncounterState reset;
        writeWaystoneState(*m_waystoneAnchor, reset);
    }
    m_victoryFlow->abandonEncounter();
    m_waystoneEncounterState = {};
    removeWaystoneGuardians();
}

void World::onWaystoneBroken(const glm::ivec3 &position)
{
    if (!m_waystoneAnchor.has_value() ||
        !sameBlockPosition(*m_waystoneAnchor, position)) {
        return;
    }
    abandonWaystoneEncounter();
    m_waystoneAnchor.reset();
}

void World::handleWaystoneGuardianDeath(const SandboxEvent &event)
{
    if (event.type != SandboxEventType::EntityDeath ||
        m_victoryFlow == nullptr || !m_waystoneAnchor.has_value() ||
        m_victoryFlow->snapshot().phase != WorldOutcomePhase::Encounter) {
        return;
    }
    const auto &death = static_cast<const EntityDeathEvent &>(event);
    const char *expected = WaystoneEncounter::actorTypeForWave(
        m_waystoneEncounterState.wave);
    if (death.type != expected ||
        m_waystoneGuardianIds.erase(death.id) == 0 ||
        m_waystoneEncounterState.remainingGuardians <= 0) {
        return;
    }

    WaystoneEncounterState next = m_waystoneEncounterState;
    --next.remainingGuardians;
    if (next.remainingGuardians > 0) {
        if (writeWaystoneState(*m_waystoneAnchor, next)) {
            m_waystoneEncounterState = next;
        }
        return;
    }
    if (next.wave == 1) {
        next = {2, WaystoneEncounter::SecondWaveGuardians, 0};
        if (!writeWaystoneState(*m_waystoneAnchor, next)) {
            return;
        }
        m_waystoneEncounterState = next;
        spawnWaystoneGuardians(2, next.remainingGuardians);
        return;
    }
    if (next.wave == 2) {
        next = {3, 0, WaystoneEncounter::RewardEpoch};
        if (!writeWaystoneState(*m_waystoneAnchor, next)) {
            return;
        }
        m_waystoneEncounterState = next;
        m_victoryFlow->resolveVictory(WaystoneEncounter::RewardEpoch);
    }
}

void World::reconcileWaystoneEncounter()
{
    if (m_victoryFlow == nullptr) {
        return;
    }
    const WorldOutcomeSnapshot outcome = m_victoryFlow->snapshot();
    if (outcome.phase != WorldOutcomePhase::Encounter) {
        removeWaystoneGuardians();
        if (outcome.phase == WorldOutcomePhase::Activated &&
            m_waystoneAnchor.has_value()) {
            const VectorXZ anchorChunk = getChunkXZ(
                m_waystoneAnchor->x, m_waystoneAnchor->z);
            if (m_chunkManager.chunkLoadedAt(anchorChunk.x,
                                             anchorChunk.z)) {
                const WaystoneEncounterState reset;
                writeWaystoneState(*m_waystoneAnchor, reset);
                m_waystoneEncounterState = reset;
            }
        }
        return;
    }

    if (!m_waystoneAnchor.has_value()) {
        std::vector<glm::ivec3> candidates =
            collectLoadedBlockEntityPositions(
                WaystoneEncounter::BlockEntityType);
        std::sort(candidates.begin(), candidates.end(),
                  [](const glm::ivec3 &left, const glm::ivec3 &right) {
                      if (left.x != right.x) return left.x < right.x;
                      if (left.y != right.y) return left.y < right.y;
                      return left.z < right.z;
                  });
        for (const glm::ivec3 &candidate : candidates) {
            WaystoneEncounterState state;
            if (readWaystoneState(candidate, state) && state.wave >= 1) {
                m_waystoneAnchor = candidate;
                m_waystoneEncounterState = state;
                break;
            }
        }
    }
    if (!m_waystoneAnchor.has_value()) {
        removeWaystoneGuardians();
        return;
    }
    const VectorXZ anchorChunk = getChunkXZ(
        m_waystoneAnchor->x, m_waystoneAnchor->z);
    if (!m_chunkManager.chunkLoadedAt(anchorChunk.x, anchorChunk.z)) {
        removeWaystoneGuardians();
        return;
    }
    if (static_cast<BlockId>(getBlock(
            m_waystoneAnchor->x, m_waystoneAnchor->y,
            m_waystoneAnchor->z).id) != BlockId::WaystoneCore ||
        !readWaystoneState(*m_waystoneAnchor,
                           m_waystoneEncounterState)) {
        abandonWaystoneEncounter();
        m_waystoneAnchor.reset();
        return;
    }
    if (m_waystoneEncounterState.wave == 3) {
        removeWaystoneGuardians();
        m_victoryFlow->resolveVictory(
            m_waystoneEncounterState.rewardEpoch);
        return;
    }
    const char *expected = WaystoneEncounter::actorTypeForWave(
        m_waystoneEncounterState.wave);
    if (expected[0] == '\0') {
        abandonWaystoneEncounter();
        return;
    }

    m_actorManager.removeActorsIf([expected](const Actor &actor) {
        return isWaystoneGuardianType(actor.getType()) &&
               actor.getType() != expected;
    });
    std::vector<ActorId> loadedIds;
    for (const ActorSnapshot &actor : m_actorManager.collectSnapshots()) {
        if (actor.type == expected) {
            loadedIds.push_back(actor.id);
        }
    }
    std::sort(loadedIds.begin(), loadedIds.end());
    if (static_cast<int>(loadedIds.size()) >
        m_waystoneEncounterState.remainingGuardians) {
        std::unordered_set<ActorId> extras(
            loadedIds.begin() +
                m_waystoneEncounterState.remainingGuardians,
            loadedIds.end());
        m_actorManager.removeActorsIf(
            [&extras](const Actor &actor) {
                return extras.find(actor.getId()) != extras.end();
            });
        loadedIds.resize(static_cast<std::size_t>(
            m_waystoneEncounterState.remainingGuardians));
    }
    m_waystoneGuardianIds.clear();
    m_waystoneGuardianIds.insert(loadedIds.begin(), loadedIds.end());
    const int missing = m_waystoneEncounterState.remainingGuardians -
                        static_cast<int>(loadedIds.size());
    if (missing > 0) {
        spawnWaystoneGuardians(m_waystoneEncounterState.wave, missing);
    }
}

void World::setPlayerGuarding(bool requested) noexcept
{
    m_playerGuardRequested = requested;
}

bool World::playerHoldsGuardWeapon() const noexcept
{
    if (m_player == nullptr || !runtimeToolRegistry().isFrozen()) {
        return false;
    }
    const ItemStack &held = m_player->getHeldItems();
    if (held.isEmpty()) {
        return false;
    }
    const ToolDefinition *tool =
        runtimeToolRegistry().find(held.getMaterial().id);
    return tool != nullptr && tool->miningClass == MiningClass::Weapon;
}

bool World::isPlayerGuarding() const noexcept
{
    return m_playerGuardRequested && m_player != nullptr &&
           m_playerActor.isAlive() &&
           m_playerGuardRecoverTicksRemaining == 0 &&
           !m_player->hasOpenContainer() && !m_player->hasOpenCrafting() &&
           playerHoldsGuardWeapon();
}

bool World::isCombatTargetAvailable(ActorId actorId) const noexcept
{
    if (actorId == DefaultPlayerActorId) {
        return m_player != nullptr && m_playerActor.isAlive();
    }
    const Actor *actor = m_actorManager.findActor(actorId);
    return actor != nullptr && actor->isAlive();
}

bool World::tryConsumeCombatChaseStep() noexcept
{
    if (m_combatChaseStepsUsed >= CombatChaseStepBudgetPerTick) {
        ++m_combatChaseStepBudgetDenied;
        return false;
    }
    ++m_combatChaseStepsUsed;
    return true;
}

bool World::canOccupyCombatPosition(const MobActor &mob,
                                    const glm::vec3 &candidate)
{
    if (!std::isfinite(candidate.x) || !std::isfinite(candidate.y) ||
        !std::isfinite(candidate.z)) {
        return false;
    }

    constexpr float Epsilon = 0.01f;
    const int minimumX = toBlockCoord(candidate.x - mob.box.dimensions.x +
                                      Epsilon);
    const int maximumX = toBlockCoord(candidate.x + mob.box.dimensions.x -
                                      Epsilon);
    const int minimumY = toBlockCoord(candidate.y + Epsilon);
    const int maximumY = toBlockCoord(
        candidate.y + mob.box.dimensions.y * 2.f - Epsilon);
    const int minimumZ = toBlockCoord(candidate.z - mob.box.dimensions.z +
                                      Epsilon);
    const int maximumZ = toBlockCoord(candidate.z + mob.box.dimensions.z -
                                      Epsilon);
    for (int x = minimumX; x <= maximumX; ++x) {
        for (int y = minimumY; y <= maximumY; ++y) {
            for (int z = minimumZ; z <= maximumZ; ++z) {
                const ChunkBlock block = getBlock(x, y, z);
                const BlockDefinition &definition =
                    BlockDatabase::get().getDefinition(
                        static_cast<BlockId>(block.id));
                if (definition.collidable) {
                    return false;
                }
            }
        }
    }
    return true;
}

void World::publishCombatWindup(const MobActor &attacker, ActorId targetId,
                                int windupTicks)
{
    m_eventBus.publish(CombatWindupEvent(
        attacker.getId(), targetId, attacker.position, windupTicks));
}

bool World::hasCombatLineOfSight(const MobActor &attacker,
                                 const Entity &target)
{
    if (m_combatRaycastsUsed >= CombatRaycastBudgetPerTick) {
        ++m_combatRaycastBudgetDenied;
        return false;
    }
    ++m_combatRaycastsUsed;

    const glm::vec3 origin = attacker.position +
        glm::vec3(0.f, attacker.box.dimensions.y, 0.f);
    const glm::vec3 destination = target.position +
        glm::vec3(0.f, target.box.dimensions.y, 0.f);
    const glm::vec3 offset = destination - origin;
    const float distance = glm::length(offset);
    if (!std::isfinite(distance) || distance <= 0.000001f) {
        return true;
    }
    const int steps = std::clamp(
        static_cast<int>(std::ceil(distance * 4.f)), 1, 32);
    for (int step = 1; step < steps; ++step) {
        const float fraction = static_cast<float>(step) /
                               static_cast<float>(steps);
        const glm::vec3 sample = origin + offset * fraction;
        const ChunkBlock block = getBlock(
            toBlockCoord(sample.x), toBlockCoord(sample.y),
            toBlockCoord(sample.z));
        if (BlockDatabase::get().getDefinition(
                static_cast<BlockId>(block.id)).collidable) {
            return false;
        }
    }
    return true;
}

CombatDirection World::directionFromPlayerTo(
    const glm::vec3 &sourcePosition) const noexcept
{
    if (m_player == nullptr) {
        return CombatDirection::None;
    }
    glm::vec2 sourceDirection(sourcePosition.x - m_player->position.x,
                              sourcePosition.z - m_player->position.z);
    const float length = glm::length(sourceDirection);
    if (!std::isfinite(length) || length <= 0.000001f) {
        return CombatDirection::None;
    }
    sourceDirection /= length;
    const float yaw = glm::radians(m_player->rotation.y);
    const glm::vec2 forward(std::sin(yaw), -std::cos(yaw));
    const glm::vec2 right(std::cos(yaw), std::sin(yaw));
    const float forwardDot = glm::dot(sourceDirection, forward);
    const float rightDot = glm::dot(sourceDirection, right);
    if (std::abs(forwardDot) >= std::abs(rightDot)) {
        return forwardDot >= 0.f ? CombatDirection::Front
                                 : CombatDirection::Back;
    }
    return rightDot >= 0.f ? CombatDirection::Right
                           : CombatDirection::Left;
}

bool World::canGuardSource(const glm::vec3 &sourcePosition) const noexcept
{
    if (m_player == nullptr) {
        return false;
    }
    glm::vec2 sourceDirection(sourcePosition.x - m_player->position.x,
                              sourcePosition.z - m_player->position.z);
    const float length = glm::length(sourceDirection);
    if (!std::isfinite(length) || length <= 0.000001f) {
        return false;
    }
    sourceDirection /= length;
    const float yaw = glm::radians(m_player->rotation.y);
    const glm::vec2 forward(std::sin(yaw), -std::cos(yaw));
    return glm::dot(sourceDirection, forward) >= 0.5f;
}

void World::recordPlayerCombatFeedback(PlayerCombatFeedbackKind kind,
                                       ActorId sourceId,
                                       const glm::vec3 &sourcePosition)
{
    ++m_playerCombatFeedback.epoch;
    m_playerCombatFeedback.kind = kind;
    m_playerCombatFeedback.direction =
        directionFromPlayerTo(sourcePosition);
    m_playerCombatFeedback.sourceId = sourceId;
    m_playerCombatFeedback.ticksRemaining = PlayerCombatFeedbackTicks;
}

void World::applyPlayerKnockback(const glm::vec3 &sourcePosition,
                                 float strength)
{
    if (m_player == nullptr || !std::isfinite(strength) || strength <= 0.f) {
        return;
    }
    glm::vec2 direction(m_player->position.x - sourcePosition.x,
                        m_player->position.z - sourcePosition.z);
    const float length = glm::length(direction);
    if (!std::isfinite(length) || length <= 0.000001f) {
        return;
    }
    direction /= length;
    m_player->velocity.x += direction.x * strength;
    m_player->velocity.z += direction.y * strength;
    m_player->velocity.y = std::max(
        m_player->velocity.y, std::min(1.5f, strength * 0.35f));
}

MobMeleeAttackResult World::resolveMobMeleeAttack(
    const MobActor &attacker, ActorId targetId)
{
    if (m_player == nullptr || targetId != DefaultPlayerActorId) {
        return MobMeleeAttackResult::TargetMissing;
    }
    if (!m_playerActor.isAlive()) {
        return MobMeleeAttackResult::TargetDead;
    }

    const glm::vec3 separation = glm::max(
        glm::abs(attacker.position - m_player->position) -
            attacker.box.dimensions - m_player->box.dimensions,
        glm::vec3(0.f));
    if (glm::length(separation) >
        attacker.getCombatProfile().attackRange + 0.001f) {
        return MobMeleeAttackResult::OutOfRange;
    }
    const std::size_t raycastsBefore = m_combatRaycastsUsed;
    if (!hasCombatLineOfSight(attacker, *m_player)) {
        return raycastsBefore >= CombatRaycastBudgetPerTick
            ? MobMeleeAttackResult::RayBudgetExhausted
            : MobMeleeAttackResult::Occluded;
    }

    const CombatDirection direction = directionFromPlayerTo(attacker.position);
    if (isPlayerGuarding() && canGuardSource(attacker.position)) {
        m_player->damageHeldTool();
        m_playerGuardRecoverTicksRemaining = PlayerGuardRecoverTicks;
        recordPlayerCombatFeedback(PlayerCombatFeedbackKind::Guard,
                                   attacker.getId(), attacker.position);
        m_eventBus.publish(CombatGuardEvent(
            DefaultPlayerActorId, attacker.getId(), m_player->position,
            direction));
        return MobMeleeAttackResult::Guarded;
    }

    if (!damagePlayer(attacker.getContactDamage(), attacker.getId())) {
        return MobMeleeAttackResult::TargetRejected;
    }
    applyPlayerKnockback(attacker.position,
                         attacker.getCombatProfile().knockback);
    return MobMeleeAttackResult::Hit;
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
    m_playerGuardRequested = false;
    m_playerGuardRecoverTicksRemaining = 0;
    m_playerCombatFeedback.kind = PlayerCombatFeedbackKind::None;
    m_playerCombatFeedback.direction = CombatDirection::None;
    m_playerCombatFeedback.sourceId = InvalidActorId;
    m_playerCombatFeedback.ticksRemaining = 0;
    m_playerRespawnPending = false;
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

bool World::isNaturalMobType(const std::string &type)
{
    if (type == NaturalMobType) {
        return true;
    }
    const EnemyDefinition *definition =
        runtimeEnemyRegistry().isFrozen()
            ? runtimeEnemyRegistry().find(type)
            : nullptr;
    return definition != nullptr && definition->natural;
}

const char *World::naturalMobTypeForBiome(TerrainBiome biome) noexcept
{
    switch (biome) {
        case TerrainBiome::Desert:
            return BruteMobType;
        case TerrainBiome::Grassland:
        case TerrainBiome::LightForest:
        case TerrainBiome::TemperateForest:
        case TerrainBiome::Ocean:
            return StalkerMobType;
    }
    return StalkerMobType;
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

    const std::vector<ActorSnapshot> initialSnapshots =
        m_actorManager.collectSnapshots();
    std::size_t worldCount = 0;
    std::size_t localCount = 0;
    for (const ActorSnapshot &snapshot : initialSnapshots) {
        if (!isNaturalMobType(snapshot.type)) {
            continue;
        }
        ++worldCount;
        const float dx = snapshot.position.x - m_player->position.x;
        const float dz = snapshot.position.z - m_player->position.z;
        if (dx * dx + dz * dz <=
            NaturalMobLocalRadius * NaturalMobLocalRadius) {
            ++localCount;
        }
    }
    if (worldCount >= NaturalMobWorldCap ||
        localCount >= NaturalMobLocalCap) {
        return;
    }

    const std::vector<const EnemyDefinition *> naturalEnemies =
        runtimeEnemyRegistry().isFrozen()
            ? runtimeEnemyRegistry().naturalEnemies()
            : std::vector<const EnemyDefinition *>{};
    if (naturalEnemies.empty()) {
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
                                        spawnPosition)) {
            continue;
        }
        bool occupied = false;
        for (const ActorSnapshot &snapshot :
             m_actorManager.collectSnapshots()) {
            if (isNaturalMobType(snapshot.type) &&
                glm::distance(snapshot.position, spawnPosition) <= 1.5f) {
                occupied = true;
                break;
            }
        }
        if (occupied) {
            continue;
        }

        const TerrainBiome biome =
            m_chunkManager.getTerrainGenerator().getBiomeAtWorld(
                toBlockCoord(spawnPosition.x),
                toBlockCoord(spawnPosition.z));
        const EnemyDefinition *selectedEnemy =
            runtimeEnemyRegistry().find(naturalMobTypeForBiome(biome));
        if (selectedEnemy == nullptr || !selectedEnemy->natural) {
            const std::size_t typeIndex = static_cast<std::size_t>(
                naturalMobSelection(m_chunkManager.getTerrainSeed(),
                                    spawnEpoch, attempt + 193)) %
                naturalEnemies.size();
            selectedEnemy = naturalEnemies[typeIndex];
        }
        const std::string &type = selectedEnemy->type;

        if (spawnMob(type, spawnPosition) != InvalidActorId) {
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
            if (!World::isNaturalMobType(actor.getType())) {
                return false;
            }
            const VectorXZ actorChunk = World::getChunkXZ(
                World::toBlockCoord(actor.position.x),
                World::toBlockCoord(actor.position.z));
            return actorChunk.x == chunkX && actorChunk.z == chunkZ;
        });
    m_naturalMobsDespawned += removed;
    if (m_waystoneAnchor.has_value()) {
        const VectorXZ anchorChunk = getChunkXZ(
            m_waystoneAnchor->x, m_waystoneAnchor->z);
        if (anchorChunk.x == chunkX && anchorChunk.z == chunkZ) {
            removeWaystoneGuardians();
        }
    }
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
    const std::vector<ActorSnapshot> actorSnapshots =
        m_actorManager.collectSnapshots();
    stats.naturalMobCount = static_cast<std::size_t>(std::count_if(
        actorSnapshots.begin(), actorSnapshots.end(),
        [](const ActorSnapshot &snapshot) {
            return World::isNaturalMobType(snapshot.type);
        }));
    stats.naturalMobWorldCap = NaturalMobWorldCap;
    stats.naturalMobLocalCap = NaturalMobLocalCap;
    stats.naturalMobSpawnAttempts = m_naturalMobSpawnAttempts;
    stats.naturalMobsSpawned = m_naturalMobsSpawned;
    stats.naturalMobsDespawned = m_naturalMobsDespawned;
    stats.combat.raycastBudget = CombatRaycastBudgetPerTick;
    stats.combat.raycastsUsed = m_combatRaycastsUsed;
    stats.combat.raycastBudgetDenied = m_combatRaycastBudgetDenied;
    stats.combat.chaseStepBudget = CombatChaseStepBudgetPerTick;
    stats.combat.chaseStepsUsed = m_combatChaseStepsUsed;
    stats.combat.chaseStepBudgetDenied = m_combatChaseStepBudgetDenied;
    int observedPriority = -1;
    for (const ActorSnapshot &snapshot : actorSnapshots) {
        if (!snapshot.combatant) {
            continue;
        }
        ++stats.combat.combatantCount;
        int priority = 0;
        switch (snapshot.combatState) {
            case MobCombatState::Idle:
                ++stats.combat.idleCount;
                priority = 0;
                break;
            case MobCombatState::Chase:
                ++stats.combat.chaseCount;
                priority = 1;
                break;
            case MobCombatState::Windup:
                ++stats.combat.windupCount;
                priority = 3;
                break;
            case MobCombatState::Recover:
                ++stats.combat.recoverCount;
                priority = 2;
                break;
        }
        if (priority > observedPriority) {
            observedPriority = priority;
            stats.combat.observedActorId = snapshot.id;
            stats.combat.observedTargetId = snapshot.combatTargetId;
            stats.combat.observedState = snapshot.combatState;
            stats.combat.observedReason = snapshot.combatTransitionReason;
            stats.combat.observedStateTicksRemaining =
                snapshot.combatStateTicksRemaining;
        }
    }
    stats.playerHealth = m_playerActor.getHealth();
    stats.playerMaxHealth = m_playerActor.getMaxHealth();
    stats.foodCooldownTicksRemaining = m_foodCooldownTicksRemaining;
    stats.attackCooldownTicksRemaining = m_attackCooldownTicksRemaining;
    stats.combatFeedback = m_playerCombatFeedback;
    stats.combatFeedback.guarding = isPlayerGuarding();
    stats.combatFeedback.guardRecoverTicksRemaining =
        m_playerGuardRecoverTicksRemaining;
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
    stats.terrainGenerationVersion =
        m_chunkManager.getTerrainGenerationVersion();
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
    auto mob = createMobActor(
        m_actorManager.allocateActorId(), type, position);
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
    m_worldSaveData.terrainGenerationVersion =
        m_chunkManager.getTerrainGenerationVersion();
    m_worldSaveData.spawnPoint = m_playerSpawnPoint;
    if (m_player != nullptr) {
        m_worldSaveData.playerState = m_player->getSaveState();
        m_worldSaveData.playerState.health = m_playerActor.getHealth();
        m_worldSaveData.playerState.foodCooldownTicks =
            m_foodCooldownTicksRemaining;
        m_worldSaveData.playerState.attackCooldownTicks =
            m_attackCooldownTicksRemaining;
        m_worldSaveData.hasPlayerState = true;
    }
    m_worldSaveData.alphaJourneyFlags =
        m_alphaJourney != nullptr ? m_alphaJourney->flags() : 0u;
    m_worldSaveData.objectiveState =
        m_alphaJourney != nullptr
            ? m_alphaJourney->objectiveSaveState()
            : ObjectiveSaveState{};
    m_worldSaveData.worldOutcome =
        m_victoryFlow != nullptr
            ? m_victoryFlow->state()
            : WorldOutcomeState{};
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
            if (isNaturalMobType(state.type)) {
                const VectorXZ actorChunk = getChunkXZ(
                    toBlockCoord(state.position.x),
                    toBlockCoord(state.position.z));
                bool duplicateNaturalPosition = false;
                for (const ActorSnapshot &snapshot :
                     m_actorManager.collectSnapshots()) {
                    if (isNaturalMobType(snapshot.type) &&
                        glm::distance(snapshot.position, state.position) <=
                            1.5f) {
                        duplicateNaturalPosition = true;
                        break;
                    }
                }
                if (!m_chunkManager.chunkLoadedAt(actorChunk.x,
                                                  actorChunk.z) ||
                    duplicateNaturalPosition) {
                    continue;
                }
            }
            auto mob = createMobActor(
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
    const TerrainGenerator &generator =
        m_chunkManager.getTerrainGenerator();
    const int minimumHeight = generator.getMinimumSpawnHeight();
    int loadedCandidates = 0;
    glm::vec3 selected{0.f};
    bool found = false;
    bool treeBacked = false;

    const auto findSafeColumn = [&](int chunkX, int chunkZ,
                                    glm::vec3 &candidate) {
        Chunk &chunk = m_chunkManager.getChunk(chunkX, chunkZ);
        std::vector<glm::ivec2> columns;
        columns.reserve((CHUNK_SIZE - 2) * (CHUNK_SIZE - 2));
        for (int x = 1; x < CHUNK_SIZE - 1; ++x) {
            for (int z = 1; z < CHUNK_SIZE - 1; ++z) {
                columns.push_back({x, z});
            }
        }
        const glm::ivec2 center{CHUNK_SIZE / 2, CHUNK_SIZE / 2};
        std::sort(columns.begin(), columns.end(),
                  [&](const glm::ivec2 &left,
                      const glm::ivec2 &right) {
                      const glm::ivec2 leftOffset = left - center;
                      const glm::ivec2 rightOffset = right - center;
                      const int leftDistance =
                          leftOffset.x * leftOffset.x +
                          leftOffset.y * leftOffset.y;
                      const int rightDistance =
                          rightOffset.x * rightOffset.x +
                          rightOffset.y * rightOffset.y;
                      if (leftDistance != rightDistance) {
                          return leftDistance < rightDistance;
                      }
                      if (left.x != right.x) {
                          return left.x < right.x;
                      }
                      return left.y < right.y;
                  });

        for (const glm::ivec2 &column : columns) {
            const int surfaceY = chunk.getHeightAt(column.x, column.y);
            if (surfaceY <= minimumHeight) {
                continue;
            }
            const BlockId ground = static_cast<BlockId>(
                chunk.getBlock(column.x, surfaceY, column.y).id);
            if (!isNaturalSpawnGround(ground) ||
                chunk.getBlock(column.x, surfaceY + 1, column.y) !=
                    BlockId::Air ||
                chunk.getBlock(column.x, surfaceY + 2, column.y) !=
                    BlockId::Air) {
                continue;
            }

            const int north = chunk.getHeightAt(column.x, column.y - 1);
            const int south = chunk.getHeightAt(column.x, column.y + 1);
            const int west = chunk.getHeightAt(column.x - 1, column.y);
            const int east = chunk.getHeightAt(column.x + 1, column.y);
            if (std::abs(north - surfaceY) > 2 ||
                std::abs(south - surfaceY) > 2 ||
                std::abs(west - surfaceY) > 2 ||
                std::abs(east - surfaceY) > 2) {
                continue;
            }

            const int worldX = chunkX * CHUNK_SIZE + column.x;
            const int worldZ = chunkZ * CHUNK_SIZE + column.y;
            if (generator.getBiomeAtWorld(worldX, worldZ) ==
                TerrainBiome::Ocean) {
                continue;
            }
            candidate = {static_cast<float>(worldX) + 0.5f,
                         static_cast<float>(surfaceY) + 2.f,
                         static_cast<float>(worldZ) + 0.5f};
            return true;
        }
        return false;
    };

    const auto chunkContainsOak = [&](int chunkX, int chunkZ) {
        const Chunk &chunk = m_chunkManager.getChunk(chunkX, chunkZ);
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                const int highest = chunk.getHeightAt(x, z);
                const int lowest = std::max(0, highest - 12);
                for (int y = highest; y >= lowest; --y) {
                    if (chunk.getBlock(x, y, z) == BlockId::OakBark) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    const auto searchPass = [&](bool preferredOnly) {
        for (int radius = 0;
             radius <= MaximumSpawnSearchChunkRadius &&
             loadedCandidates < MaximumLoadedSpawnCandidates;
             ++radius) {
            const auto inspect = [&](int chunkX, int chunkZ) {
                if (found ||
                    loadedCandidates >= MaximumLoadedSpawnCandidates) {
                    return;
                }
                const int hintX = chunkX * CHUNK_SIZE + CHUNK_SIZE / 2;
                const int hintZ = chunkZ * CHUNK_SIZE + CHUNK_SIZE / 2;
                const TerrainBiome biome =
                    generator.getBiomeAtWorld(hintX, hintZ);
                if (biome == TerrainBiome::Ocean ||
                    (preferredOnly && !isPreferredSpawnBiome(biome)) ||
                    generator.getSurfaceHeightAtWorld(hintX, hintZ) <=
                        minimumHeight) {
                    return;
                }

                ++loadedCandidates;
                m_chunkManager.loadChunk(chunkX, chunkZ);
                glm::vec3 candidate{0.f};
                const bool safe =
                    findSafeColumn(chunkX, chunkZ, candidate);
                const bool hasOak = safe && chunkContainsOak(chunkX, chunkZ);
                if (safe && (!preferredOnly || hasOak)) {
                    selected = candidate;
                    treeBacked = hasOak;
                    found = true;
                    return;
                }
                m_chunkManager.unloadChunk(chunkX, chunkZ);
            };

            if (radius == 0) {
                inspect(0, 0);
                continue;
            }
            for (int x = -radius; x <= radius && !found; ++x) {
                inspect(x, -radius);
                inspect(x, radius);
            }
            for (int z = -radius + 1; z < radius && !found; ++z) {
                inspect(-radius, z);
                inspect(radius, z);
            }
            if (found) {
                return;
            }
        }
    };

    searchPass(true);
    if (!found) {
        loadedCandidates = 0;
        std::cout << "Spawn search did not find nearby oak; using bounded "
                     "safe-land fallback.\n";
        searchPass(false);
    }
    if (!found) {
        throw std::runtime_error(
            "Unable to find a safe spawn within the bounded search area.");
    }

    m_playerSpawnPoint = selected;
    preloadChunksAround(m_playerSpawnPoint);

    std::cout << "Spawn found! Loaded candidates: " << loadedCandidates
              << " Nearby oak: " << (treeBacked ? "yes" : "fallback")
              << " Time Taken: "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                               start)
                     .count()
              << " seconds\n";
}
