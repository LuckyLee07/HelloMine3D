#ifndef WORLD_H_INCLUDED
#define WORLD_H_INCLUDED

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../Actor/ActorManager.h"
#include "../Item/Material.h"
#include "../Maths/glm.h"
#include "../Sandbox/Events/SandboxEventBus.h"
#include "../Util/NonCopyable.h"
#include "Chunk/Chunk.h"
#include "Chunk/ChunkManager.h"
#include "Storage/WorldSave.h"

#include "Event/IWorldEvent.h"

#include "../Config.h"

class Camera;
class Player;

struct Entity;

struct WorldDebugStats {
    ChunkDebugStats chunks;
    std::size_t actorCount = 0;
    std::size_t queuedChunkUpdates = 0;
    int terrainSeed = 0;
    float worldTime = 0.f;
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
    friend class ChunkSection;

  public:
    World(const Camera &camera, const Config &config, Player &player,
          std::string saveDirectory = "",
          bool startBackgroundLoader = true,
          int initialPreloadRadius = 1);
    ~World();

    ChunkBlock getBlock(int x, int y, int z);
    void setBlock(int x, int y, int z, ChunkBlock block);

    void tick(int worldTime);
    void update(const Camera &camera);
    void resetChunkMeshes();
    void updateChunk(int blockX, int blockY, int blockZ);
    bool save();
    float getWorldTime() const;
    WorldDebugStats collectDebugStats();
    WorldMeshSnapshot collectSectionMeshSnapshot();
    void acknowledgeSectionMeshUploads(
        const std::vector<WorldSectionMeshVersion> &versions);
    void preloadAround(const glm::vec3 &position);
    void startBackgroundLoader();
    ActorId spawnItemEntity(Material::ID materialId, int amount,
                            const glm::vec3 &position,
                            const glm::vec3 &initialVelocity = glm::vec3(0.f));
    ActorId spawnMob(const std::string &type, const glm::vec3 &position);

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
    void loadChunks();
    void unloadDistantChunks(const Camera &camera);
    void setChunkLoadCenter(const Camera &camera);
    void queueChunkUpdate(int blockX, int blockY, int blockZ);
    void updateChunks();
    void preloadChunksAround(const glm::vec3 &position, int radius = 1);
    bool saveWorldState();
    void setSpawnPoint();

    ChunkManager m_chunkManager;
    ActorManager m_actorManager;
    SandboxEventBus m_eventBus;
    Player *m_player = nullptr;
    WorldSave m_worldSave;
    WorldSaveData m_worldSaveData;

    std::vector<std::unique_ptr<IWorldEvent>> m_events;
    std::unordered_map<glm::ivec3, ChunkSection *, IVec3Hash> m_chunkUpdates;

    std::atomic<bool> m_isRunning{true};
    std::vector<std::thread> m_chunkLoadThreads;

    // Mutex classes invoked to protect data from shared threads

    std::mutex m_mainMutex;
    std::mutex m_genMutex;

    std::atomic<int> m_loadCenterX{0};
    std::atomic<int> m_loadCenterSectionY{0};
    std::atomic<int> m_loadCenterZ{0};
    std::atomic<int> m_chunkLoadRevision{0};
    std::atomic<int> m_loadDistance{2};
    const int m_renderDistance;

    VectorXZ m_lastUnloadScanChunk{0, 0};
    bool m_unloadScanValid = false;
    bool m_unloadBacklog = false;

    glm::vec3 m_playerSpawnPoint;
};

#endif // WORLD_H_INCLUDED
