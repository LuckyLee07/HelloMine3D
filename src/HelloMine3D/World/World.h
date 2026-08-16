#ifndef WORLD_H_INCLUDED
#define WORLD_H_INCLUDED

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../Actor/ActorManager.h"
#include "../Actor/PlayerActor.h"
#include "../Item/Material.h"
#include "../Maths/Frustum.h"
#include "../Maths/glm.h"
#include "../Sandbox/Events/SandboxEventBus.h"
#include "../Util/NonCopyable.h"
#include "Chunk/Chunk.h"
#include "Chunk/ChunkManager.h"
#include "Environment/WorldEnvironment.h"
#include "Storage/WorldSave.h"
#include "Storage/WorldBackup.h"

#include "Event/IWorldEvent.h"

#include "../Config.h"
#include "../Diagnostics/TerrainBufferMetrics.h"

class Camera;
class Player;

struct Entity;

struct WorldDebugStats {
    ChunkDebugStats chunks;
    TerrainBufferMetrics terrainBuffers;
    std::size_t actorCount = 0;
    std::size_t naturalMobCount = 0;
    std::size_t naturalMobWorldCap = 0;
    std::size_t naturalMobLocalCap = 0;
    std::size_t naturalMobSpawnAttempts = 0;
    std::size_t naturalMobsSpawned = 0;
    std::size_t naturalMobsDespawned = 0;
    float playerHealth = 0.f;
    float playerMaxHealth = 0.f;
    std::size_t queuedChunkUpdates = 0;
    std::size_t randomTickSections = 0;
    std::size_t randomTickBlocks = 0;
    std::size_t randomTickSectionsProcessed = 0;
    std::size_t randomTicksDispatched = 0;
    int terrainSeed = 0;
    float worldTime = 0.f;
    WorldEnvironmentState environment;
};

struct WorldSectionMeshVersion {
    glm::ivec3 location{0};
    std::uint32_t blockRevision = 0;
};

struct WorldSectionMeshSnapshot : WorldSectionMeshVersion {
    ChunkMeshCollection meshes;
};

struct WorldMeshSnapshot {
    std::vector<glm::ivec3> liveSections;
    std::vector<WorldSectionMeshSnapshot> cpuReadySections;
};

/// @brief Massive class designed to hold multiple chunks, the player, and most game aspects.
class World : public NonCopyable {
    friend class Chunk;
    friend class ChunkSection;
    friend class ChunkManager;

  public:
    static constexpr std::size_t ChunkMeshRebuildBudgetPerUpdate = 2;
    static constexpr std::size_t RandomTickSectionBudgetPerTick = 4;
    static constexpr std::size_t RandomTickAttemptsPerSection = 3;
    static constexpr int NaturalMobSpawnIntervalTicks = 20;
    static constexpr std::size_t NaturalMobSpawnAttemptsPerCycle = 16;
    static constexpr std::size_t NaturalMobWorldCap = 12;
    static constexpr std::size_t NaturalMobLocalCap = 4;
    static constexpr float NaturalMobLocalRadius = 32.f;
    static constexpr const char *NaturalMobType = "hellomine:natural_mob";
    static constexpr float PlayerAttackDamage = 4.f;
    static constexpr float MobContactDamage = 2.f;
    static constexpr const char *PlayerDeathInventoryPolicy = "retain";

    World(const Camera &camera, const Config &config, Player &player,
          std::string saveDirectory = "",
          bool startBackgroundLoader = true,
          int initialPreloadRadius = 1);
    ~World();

    ChunkBlock getBlock(int x, int y, int z);
    LightLevel getSunlight(int x, int y, int z);
    LightLevel getBlockLight(int x, int y, int z);
    void setBlock(int x, int y, int z, ChunkBlock block);
    std::optional<BlockEntityRecord>
    getBlockEntity(const glm::ivec3 &position);
    bool createBlockEntity(const glm::ivec3 &position,
                           const std::string &type, std::string payload);
    bool updateBlockEntity(const glm::ivec3 &position, std::string payload);
    std::optional<BlockEntityRecord>
    removeBlockEntity(const glm::ivec3 &position);

    void tick(int worldTime);
    void update(const Camera &camera);
    void resetChunkMeshes();
    void updateChunk(int blockX, int blockY, int blockZ);
    bool save();
    float getWorldTime() const;
    WorldDebugStats collectDebugStats();
    std::vector<ActorSnapshot> collectActorSnapshots();
    WorldMeshSnapshot collectSectionMeshSnapshot();
    void acknowledgeSectionMeshUploads(
        const std::vector<WorldSectionMeshVersion> &versions);
    void preloadAround(const glm::vec3 &position);
    void startBackgroundLoader();
    ActorId spawnItemEntity(Material::ID materialId, int amount,
                            const glm::vec3 &position,
                            const glm::vec3 &initialVelocity = glm::vec3(0.f));
    ActorId spawnMob(const std::string &type, const glm::vec3 &position);
    bool attackActor(ActorId actorId,
                     float amount = PlayerAttackDamage);
    bool damagePlayer(float amount, ActorId sourceId = InvalidActorId);
    float getPlayerHealth() const;
    float getPlayerMaxHealth() const;
    glm::vec3 getPlayerSpawnPoint() const;

    ChunkManager &getChunkManager();
    ActorManager &getActorManager();
    const ActorManager &getActorManager() const;
    SandboxEventBus &getEventBus();
    const SandboxEventBus &getEventBus() const;
    Player *getPlayer();
    const Player *getPlayer() const;

    static int toBlockCoord(float value);
    static int floorDiv(int value, int divisor);
    static int floorMod(int value, int divisor);
    static VectorXZ getBlockXZ(int x, int z);
    static VectorXZ getChunkXZ(int x, int z);
    static std::size_t randomTickBlockIndex(int terrainSeed, int worldTime,
                                            const glm::ivec3 &section,
                                            std::size_t attempt);
    static glm::ivec2 naturalMobSpawnOffset(int terrainSeed, int spawnEpoch,
                                            std::size_t attempt);

    /// Produces a complete, stable work order. Chunks intersecting the
    /// published frustum come first, with distance as the secondary key.
    static std::vector<VectorXZ>
    planChunkMeshWork(const VectorXZ &center, int radius, int sectionY,
                      const ViewFrustum *frustum);

    // void collisionTest(Entity &entity);

    template <typename T, typename... Args> void addEvent(Args &&... args)
    {
        m_events.push_back(std::make_unique<T>(std::forward<Args>(args)...));
    }

  private:
    struct IVec3Hash {
        std::size_t operator()(const glm::ivec3 &value) const noexcept
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
    };

    ChunkBlock getBlockUnlocked(int x, int y, int z);
    LightLevel getSunlightUnlocked(int x, int y, int z);
    LightLevel getBlockLightUnlocked(int x, int y, int z);
    bool setBlockLightUnlocked(int x, int y, int z, LightLevel level);
    void relightBlockEdit(const glm::ivec3 &position,
                          LightLevel previousLight,
                          std::vector<glm::ivec3> &changedPositions);
    void removeBlockLight(
        std::deque<std::pair<glm::ivec3, LightLevel>> &removalQueue,
        std::deque<glm::ivec3> &additionQueue,
        std::vector<glm::ivec3> &changedPositions);
    void propagateBlockLight(std::deque<glm::ivec3> &pending,
                             std::vector<glm::ivec3> &changedPositions);
    void reconcileBlockLightAfterChunkLoad(int chunkX, int chunkZ);
    void reconcileBlockLightAfterChunkUnload(int chunkX, int chunkZ,
                                             int height);
    void updateRandomTickSection(const glm::ivec3 &section, bool active);
    void removeRandomTickSectionsForChunk(int chunkX, int chunkZ);
    void runRandomTicks(int worldTime);
    void runNaturalMobPopulation(int worldTime);
    void applyMobContactDamage();
    void respawnPlayer();
    bool findSafeNaturalMobPosition(int blockX, int blockZ,
                                    glm::vec3 &position);
    void despawnNaturalMobsInChunk(int chunkX, int chunkZ);
    void loadChunks();
    void unloadDistantChunks(const Camera &camera);
    void setChunkLoadCenter(const Camera &camera);
    void publishMeshPrioritySnapshot(const Camera &camera, int sectionY);
    void queueChunkUpdate(int blockX, int blockY, int blockZ);
    void queueSectionUpdate(const glm::ivec3 &key);
    void queueLightingUpdates(
        const std::vector<glm::ivec3> &changedPositions);
    void updateChunks();
    void preloadChunksAround(const glm::vec3 &position, int radius = 1);
    bool saveWorldState();
    void restoreActors(const std::vector<ActorSaveState> &states);
    void setSpawnPoint();

    ChunkManager m_chunkManager;
    ActorManager m_actorManager;
    PlayerActor m_playerActor;
    SandboxEventBus m_eventBus;
    Player *m_player = nullptr;
    WorldSave m_worldSave;
    WorldBackup m_worldBackup;
    WorldSaveData m_worldSaveData;
    std::size_t m_worldSaveTransactionCount = 0;
    double m_worldSaveTotalMs = 0.0;
    double m_worldSaveMaxMs = 0.0;

    std::vector<std::unique_ptr<IWorldEvent>> m_events;
    std::deque<glm::ivec3> m_chunkUpdateQueue;
    std::unordered_set<glm::ivec3, IVec3Hash> m_queuedChunkUpdates;
    std::deque<glm::ivec3> m_randomTickSectionQueue;
    std::unordered_set<glm::ivec3, IVec3Hash> m_randomTickSections;
    std::size_t m_randomTickSectionsProcessed = 0;
    std::size_t m_randomTicksDispatched = 0;
    std::size_t m_naturalMobSpawnAttempts = 0;
    std::size_t m_naturalMobsSpawned = 0;
    std::size_t m_naturalMobsDespawned = 0;

    std::atomic<bool> m_isRunning{true};
    std::vector<std::thread> m_chunkLoadThreads;

    // Mutex classes invoked to protect data from shared threads

    std::mutex m_mainMutex;
    std::mutex m_genMutex;
    std::mutex m_meshPriorityMutex;

    struct MeshPrioritySnapshot {
        ViewFrustum frustum;
        int sectionY = 0;
        bool valid = false;
    };
    MeshPrioritySnapshot m_meshPrioritySnapshot;

    std::atomic<int> m_loadCenterX{0};
    std::atomic<int> m_loadCenterSectionY{0};
    std::atomic<int> m_loadCenterZ{0};
    std::atomic<int> m_chunkLoadRevision{0};
    std::atomic<int> m_meshPriorityRevision{0};
    std::atomic<int> m_loadDistance{2};
    const int m_renderDistance;

    VectorXZ m_lastUnloadScanChunk{0, 0};
    bool m_unloadScanValid = false;
    bool m_unloadBacklog = false;

    glm::vec3 m_lastMeshPriorityRotation{0.f};
    int m_lastMeshPrioritySectionY = -1;
    bool m_meshPriorityPublished = false;

    glm::vec3 m_playerSpawnPoint;
};

#endif // WORLD_H_INCLUDED
