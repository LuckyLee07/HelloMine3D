#ifndef WORLD_H_INCLUDED
#define WORLD_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
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
#include "Chunk/ChunkRuntime.h"
#include "Environment/WorldEnvironment.h"
#include "Simulation/WorldSimulation.h"
#include "Storage/WorldSave.h"
#include "Storage/WorldBackup.h"

#include "Command/IWorldCommand.h"

#include "../Config.h"
#include "../Diagnostics/TerrainBufferMetrics.h"
#include "../Gameplay/AlphaJourney.h"
#include "../Gameplay/VictoryFlow.h"
#include "../Gameplay/WaystoneEncounter.h"

class Camera;
class Player;
class MobActor;
class WorldManager;

struct Entity;

struct CombatRuntimeDebugStats {
    std::size_t combatantCount = 0;
    std::size_t idleCount = 0;
    std::size_t chaseCount = 0;
    std::size_t windupCount = 0;
    std::size_t recoverCount = 0;
    std::size_t raycastsUsed = 0;
    std::size_t raycastBudget = 0;
    std::size_t raycastBudgetDenied = 0;
    std::size_t chaseStepsUsed = 0;
    std::size_t chaseStepBudget = 0;
    std::size_t chaseStepBudgetDenied = 0;
    std::size_t projectileCount = 0;
    std::size_t projectileWorldLimit = 0;
    std::size_t projectileStepsUsed = 0;
    std::size_t projectileStepBudget = 0;
    std::size_t projectileStepBudgetDenied = 0;
    std::size_t projectilesLaunched = 0;
    std::size_t projectileCapacityDenied = 0;
    std::size_t projectileHits = 0;
    std::size_t projectileGuards = 0;
    std::size_t projectileBlocks = 0;
    std::size_t projectileExpirations = 0;
    std::size_t projectileOwnerClears = 0;
    CombatProjectileId observedProjectileId = InvalidCombatProjectileId;
    CombatProjectileRemovalReason lastProjectileRemovalReason =
        CombatProjectileRemovalReason::None;
    ActorId observedActorId = InvalidActorId;
    ActorId observedTargetId = InvalidActorId;
    MobCombatState observedState = MobCombatState::Idle;
    EnemyCombatMode observedMode = EnemyCombatMode::Melee;
    MobCombatTransitionReason observedReason =
        MobCombatTransitionReason::Spawned;
    int observedStateTicksRemaining = 0;
};

struct PlayerCombatFeedbackSnapshot {
    PlayerCombatFeedbackKind kind = PlayerCombatFeedbackKind::None;
    CombatDirection direction = CombatDirection::None;
    ActorId sourceId = InvalidActorId;
    std::uint64_t epoch = 0;
    int ticksRemaining = 0;
    bool guarding = false;
    int guardRecoverTicksRemaining = 0;
};

struct ExplorationRewardSnapshot {
    int version = ExplorationRewards::LegacyVersion;
    bool ancientCompassHeld = false;
    bool raiderWardCarried = false;
    float homeDistance = 0.f;
    std::string homeDirection;
    int guardRecoverTicks = 0;
};

struct WorldDebugStats {
    ChunkDebugStats chunks;
    ChunkDemandDebugStats streamingDemand;
    SpatialInterestDebugStats spatialInterest;
    WorldJobSchedulerDebugStats worldJobs;
    ChunkBackpressureDebugStats streamingBackpressure;
    TerrainBufferMetrics terrainBuffers;
    WorldSimulationSnapshot simulation;
    std::size_t actorCount = 0;
    std::size_t naturalMobCount = 0;
    std::size_t naturalMobWorldCap = 0;
    std::size_t naturalMobLocalCap = 0;
    std::size_t naturalMobSpawnAttempts = 0;
    std::size_t naturalMobsSpawned = 0;
    std::size_t naturalMobsDespawned = 0;
    float playerHealth = 0.f;
    float playerMaxHealth = 0.f;
    int foodCooldownTicksRemaining = 0;
    int attackCooldownTicksRemaining = 0;
    CombatRuntimeDebugStats combat;
    PlayerCombatFeedbackSnapshot combatFeedback;
    std::size_t queuedChunkUpdates = 0;
    std::size_t randomTickSections = 0;
    std::size_t randomTickBlocks = 0;
    std::size_t randomTickSectionsProcessed = 0;
    std::size_t randomTicksDispatched = 0;
    int terrainSeed = 0;
    int terrainGenerationVersion = 0;
    int explorationRewardVersion = ExplorationRewards::LegacyVersion;
    int difficultyProfileVersion = CurrentDifficultyProfileVersion;
    WorldDifficulty difficulty = WorldDifficulty::Normal;
    bool difficultyChangePending = false;
    WorldDifficulty pendingDifficulty = WorldDifficulty::Normal;
    unsigned long long difficultyApplicationEpoch = 0;
    float playerOutgoingDamageMultiplier = 1.f;
    float playerIncomingDamageMultiplier = 1.f;
    int lootAmountNumerator = 1;
    int lootAmountDenominator = 1;
    int postVictoryEventVersion = PostVictoryEvents::CurrentVersion;
    int completedPostVictoryEvents = 0;
    int activePostVictoryEvent = 0;
    int postVictoryEventWave = 0;
    int postVictoryEventRemainingGuardians = 0;
    float worldTime = 0.f;
    WorldEnvironmentState environment;
};

enum class FoodUseResult {
    Consumed,
    SimulationPaused,
    UiBusy,
    PlayerUnavailable,
    PlayerDead,
    CoolingDown,
    EmptyHand,
    NotFood,
    FullHealth,
    InventoryRejected
};

enum class CombatAttackResult {
    Hit,
    SimulationPaused,
    UiBusy,
    PlayerUnavailable,
    PlayerDead,
    CoolingDown,
    TargetMissing,
    TargetDead,
    OutOfReach,
    TargetRejected
};

/// @brief Massive class designed to hold multiple chunks, the player, and most game aspects.
class World : public NonCopyable {
    friend class Chunk;
    friend class ChunkSection;
    friend class ChunkManager;
    friend class WorldSimulation;
    friend class WorldManager;

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
    static constexpr const char *StalkerMobType = "hellomine:stalker";
    static constexpr const char *BruteMobType = "hellomine:brute";
    static constexpr const char *SpitterMobType = "hellomine:spitter";
    static constexpr float PlayerAttackDamage = 4.f;
    static constexpr int PlayerAttackCooldownTicks = 10;
    static constexpr float PlayerAttackReach = 3.f;
    static constexpr std::size_t CombatRaycastBudgetPerTick = 16;
    static constexpr std::size_t CombatChaseStepBudgetPerTick = 32;
    static constexpr std::size_t CombatProjectileWorldLimit = 32;
    static constexpr std::size_t CombatProjectileStepBudgetPerTick = 32;
    static constexpr int PlayerGuardRecoverTicks = 8;
    static constexpr int PlayerCombatFeedbackTicks = 10;
    static constexpr int MobPlayerHitRecoverTicks = 3;
    static constexpr float MobPlayerHitKnockback = 0.65f;
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
    std::vector<glm::ivec3> collectLoadedBlockEntityPositions(
        const std::string &type);

    void tick(int worldTime);
    void update(const Camera &camera);
    void setRenderDistance(int renderDistance) noexcept;
    int getRenderDistance() const noexcept;
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
    CombatAttackResult tryAttackActor(ActorId actorId,
                                      bool simulationRunning = true);
    bool attackActor(ActorId actorId);
    bool attackActor(ActorId actorId, float amount);
    bool damagePlayer(float amount, ActorId sourceId = InvalidActorId);
    void setPlayerGuarding(bool requested) noexcept;
    bool canPlayerGuard() const noexcept;
    bool isPlayerGuarding() const noexcept;
    bool isCombatTargetAvailable(ActorId actorId) const noexcept;
    bool tryConsumeCombatChaseStep() noexcept;
    bool canOccupyCombatPosition(const MobActor &mob,
                                 const glm::vec3 &position);
    void publishCombatWindup(const MobActor &attacker, ActorId targetId,
                             int windupTicks);
    MobMeleeAttackResult resolveMobMeleeAttack(const MobActor &attacker,
                                               ActorId targetId);
    MobRangedAttackResult launchMobProjectile(const MobActor &attacker,
                                              ActorId targetId);
    std::vector<CombatProjectileSnapshot>
    collectCombatProjectileSnapshots();
    FoodUseResult useHeldFood(bool simulationRunning = true);
    float getPlayerHealth() const;
    float getPlayerMaxHealth() const;
    int getFoodCooldownTicksRemaining() const noexcept;
    int getAttackCooldownTicksRemaining() const noexcept;
    glm::vec3 getPlayerSpawnPoint() const;
    ExplorationRewardSnapshot getExplorationRewardSnapshot() const noexcept;
    int getPlayerGuardRecoverDurationTicks() const noexcept;
    AlphaJourneySnapshot getAlphaJourneySnapshot() const;
    ObjectiveSnapshot getObjectiveSnapshot() const;
    RecipeDiscoverySnapshot getRecipeDiscoverySnapshot() const;
    bool isRecipeDiscovered(const std::string &recipeId) const noexcept;
    WorldOutcomeSnapshot getWorldOutcomeSnapshot() const noexcept;
    DifficultyRuntimeSnapshot getDifficultySnapshot() const noexcept;
    DifficultyChangeResult requestDifficulty(
        WorldDifficulty difficulty) noexcept;
    int scaleDifficultyLootAmount(int amount) const noexcept;
    PostVictoryEventSnapshot getPostVictoryEventSnapshot() const;
    WaystoneEncounterSnapshot getWaystoneEncounterSnapshot() const;
    bool initializeWaystone(const glm::ivec3 &position);
    void onWaystoneBroken(const glm::ivec3 &position);
    WaystoneActionResult useWaystone(const glm::ivec3 &position,
                                     Player &player,
                                     bool simulationRunning = true);
    WaystoneActionResult claimWaystoneReward(
        bool simulationRunning = true);
    std::string consumeWaystoneFeedbackKey();

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
    static bool isNaturalMobType(const std::string &type);
    static const char *naturalMobTypeForBiome(TerrainBiome biome) noexcept;

    /// Produces a complete, stable work order. Chunks intersecting the
    /// published frustum come first, with distance as the secondary key.
    static std::vector<VectorXZ>
    planChunkMeshWork(const VectorXZ &center, int radius, int sectionY,
                      const ViewFrustum *frustum);

    // void collisionTest(Entity &entity);

    template <typename T, typename... Args> void addCommand(Args &&... args)
    {
        static_assert(std::is_base_of<IWorldCommand, T>::value,
                      "World commands must implement IWorldCommand");
        m_commands.push_back(
            std::make_unique<T>(std::forward<Args>(args)...));
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

    struct CombatProjectile {
        CombatProjectileId id = InvalidCombatProjectileId;
        ActorId ownerId = InvalidActorId;
        std::string ownerType;
        glm::vec3 origin{0.f};
        glm::vec3 position{0.f};
        glm::vec3 velocity{0.f};
        float radius = 0.f;
        float damage = 0.f;
        float knockback = 0.f;
        int ticksRemaining = 0;
        float distanceTravelled = 0.f;
        float maximumDistance = 0.f;
        float activeRadius = 0.f;
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
    void applyPendingDifficulty() noexcept;
    void respawnPlayer();
    void tickCombatProjectiles();
    CombatProjectileRemovalReason stepCombatProjectile(
        CombatProjectile &projectile);
    void removeCombatProjectilesOwnedBy(ActorId ownerId);
    void removeCombatProjectilesInChunk(int chunkX, int chunkZ);
    void clearInvalidCombatProjectiles();
    void recordCombatProjectileRemoval(
        CombatProjectileId id, CombatProjectileRemovalReason reason);
    bool hasCombatLineOfSight(const MobActor &attacker,
                              const Entity &target);
    bool playerHoldsGuardWeapon() const noexcept;
    bool playerCarriesRaiderWard() const noexcept;
    CombatDirection directionFromPlayerTo(
        const glm::vec3 &sourcePosition) const noexcept;
    bool canGuardSource(const glm::vec3 &sourcePosition) const noexcept;
    void recordPlayerCombatFeedback(PlayerCombatFeedbackKind kind,
                                    ActorId sourceId,
                                    const glm::vec3 &sourcePosition);
    void applyPlayerKnockback(const glm::vec3 &sourcePosition,
                              float strength);
    bool findSafeNaturalMobPosition(int blockX, int blockZ,
                                    glm::vec3 &position);
    void despawnNaturalMobsInChunk(int chunkX, int chunkZ);
    void preloadAroundForTeleport(const glm::vec3 &position);
    bool saveWorldState();
    void restoreActors(const std::vector<ActorSaveState> &states);
    void setSpawnPoint();
    bool readWaystoneState(const glm::ivec3 &position,
                           WaystoneEncounterState &state);
    bool writeWaystoneState(const glm::ivec3 &position,
                            const WaystoneEncounterState &state);
    bool findWaystoneSpawnPositions(const glm::ivec3 &anchor,
                                    int count,
                                    std::vector<glm::vec3> &positions);
    bool spawnWaystoneGuardians(int wave, int count);
    WaystoneActionResult usePostVictoryWaystone(
        const glm::ivec3 &position,
        const WaystoneEncounterState &persisted);
    WaystoneActionResult claimPostVictoryReward(
        const glm::ivec3 &position,
        const WaystoneEncounterState &persisted);
    void reconcileWaystoneEncounter();
    void handleWaystoneGuardianDeath(const SandboxEvent &event);
    void abandonWaystoneEncounter();
    void removeWaystoneGuardians();
    void setWaystoneFeedback(WaystoneActionResult result);
    WaystoneActionResult triggerWaystoneResonancePulse(
        const glm::ivec3 &position);

    ChunkManager m_chunkManager;
    std::mutex m_mainMutex;
    ChunkRuntime m_chunkRuntime;
    WorldSimulation m_worldSimulation;
    ActorManager m_actorManager;
    PlayerActor m_playerActor;
    SandboxEventBus m_eventBus;
    Player *m_player = nullptr;
    WorldSave m_worldSave;
    WorldBackup m_worldBackup;
    WorldSaveData m_worldSaveData;
    std::unique_ptr<AlphaJourney> m_alphaJourney;
    std::unique_ptr<VictoryFlow> m_victoryFlow;
    std::optional<glm::ivec3> m_waystoneAnchor;
    WaystoneEncounterState m_waystoneEncounterState;
    std::unordered_set<ActorId> m_waystoneGuardianIds;
    int m_waystoneResonanceCooldownTicks = 0;
    std::string m_waystoneFeedbackKey;
    std::size_t m_worldSaveTransactionCount = 0;
    double m_worldSaveTotalMs = 0.0;
    double m_worldSaveMaxMs = 0.0;

    std::vector<std::unique_ptr<IWorldCommand>> m_commands;
    std::deque<glm::ivec3> m_randomTickSectionQueue;
    std::unordered_set<glm::ivec3, IVec3Hash> m_randomTickSections;
    std::size_t m_randomTickSectionsProcessed = 0;
    std::size_t m_randomTicksDispatched = 0;
    std::size_t m_naturalMobSpawnAttempts = 0;
    std::size_t m_naturalMobsSpawned = 0;
    std::size_t m_naturalMobsDespawned = 0;
    std::optional<WorldDifficulty> m_pendingDifficulty;
    unsigned long long m_difficultyApplicationEpoch = 0;

    glm::vec3 m_playerSpawnPoint;
    int m_foodCooldownTicksRemaining = 0;
    int m_attackCooldownTicksRemaining = 0;
    std::size_t m_combatRaycastsUsed = 0;
    std::size_t m_combatRaycastBudgetDenied = 0;
    std::size_t m_combatChaseStepsUsed = 0;
    std::size_t m_combatChaseStepBudgetDenied = 0;
    std::vector<CombatProjectile> m_combatProjectiles;
    CombatProjectileId m_nextCombatProjectileId = 1;
    std::size_t m_combatProjectileStepsUsed = 0;
    std::size_t m_combatProjectileStepBudgetDenied = 0;
    std::size_t m_combatProjectilesLaunched = 0;
    std::size_t m_combatProjectileCapacityDenied = 0;
    std::size_t m_combatProjectileHits = 0;
    std::size_t m_combatProjectileGuards = 0;
    std::size_t m_combatProjectileBlocks = 0;
    std::size_t m_combatProjectileExpirations = 0;
    std::size_t m_combatProjectileOwnerClears = 0;
    CombatProjectileId m_observedCombatProjectileId =
        InvalidCombatProjectileId;
    CombatProjectileRemovalReason m_lastCombatProjectileRemovalReason =
        CombatProjectileRemovalReason::None;
    bool m_playerGuardRequested = false;
    int m_playerGuardRecoverTicksRemaining = 0;
    PlayerCombatFeedbackSnapshot m_playerCombatFeedback;
    bool m_playerRespawnPending = false;
};

#endif // WORLD_H_INCLUDED
