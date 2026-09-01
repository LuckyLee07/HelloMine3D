#ifndef CHUNKRUNTIME_H_INCLUDED
#define CHUNKRUNTIME_H_INCLUDED

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include "../../Maths/Frustum.h"
#include "../../Maths/glm.h"
#include "../../Maths/Vector2XZ.h"
#include "../../Util/NonCopyable.h"
#include "ChunkDemand.h"
#include "ChunkManager.h"
#include "../Streaming/WorldJobScheduler.h"

class Camera;

struct WorldSectionMeshVersion {
    glm::ivec3 location{0};
    std::uint32_t blockRevision = 0;
};

struct WorldSectionMeshSnapshot : WorldSectionMeshVersion {
    ChunkMeshCollection meshes;
};

struct WorldMeshSnapshot {
    std::vector<glm::ivec3> liveSections;
    std::vector<WorldSectionMeshVersion> liveSectionVersions;
    std::vector<WorldSectionMeshSnapshot> cpuReadySections;
    std::size_t cpuReadyTotal = 0;
    std::size_t cpuReadyDeferred = 0;
};

struct ChunkBackpressureDebugStats {
    std::size_t deferredPlanJobs = 0;
    std::size_t lastAuthoritativeCommits = 0;
    std::size_t peakAuthoritativeCommits = 0;
    std::size_t maxAuthoritativeCommitsPerPass = 0;
    std::size_t lastCpuReadyTotal = 0;
    std::size_t lastSectionUploadsOffered = 0;
    std::size_t lastSectionUploadsDeferred = 0;
    std::size_t maxSectionUploadsPerFrame = 0;
    std::size_t lastUnloads = 0;
    std::size_t maxUnloadsPerUpdate = 0;
    bool unloadBacklog = false;
};

struct ChunkDemandTarget {
    VectorXZ coord{0, 0};
    std::uint32_t reasonMask = 0;
    int priority = 0;
    std::uint64_t newestEpoch = 0;
    int distanceSquared = 0;
    int distanceManhattan = 0;
    bool inFrustum = false;
    int motionRank = 1;
};

/// Owns the existing derived Chunk work queues and loader coordination.
/// ChunkManager remains the owner of authoritative Chunk and storage state.
class ChunkRuntime final : public NonCopyable {
  public:
    static constexpr std::size_t MaxAuthoritativeCommitsPerPass = 8;
    static constexpr std::size_t MaxSectionUploadsPerFrame = 8;
    static constexpr std::size_t MaxUnloadsPerUpdate = 8;

    ChunkRuntime(ChunkManager &chunkManager, std::mutex &worldMutex,
                 int renderDistance);
    ~ChunkRuntime();

    void setInitialLoadCenter(const glm::vec3 &position) noexcept;
    void updateLoadCenter(const glm::vec3 &playerPosition,
                          const Camera &camera);
    void unloadDistantChunks(const Camera &camera);

    void setRenderDistance(int renderDistance) noexcept;
    int getRenderDistance() const noexcept;
    void resetMeshes();

    void startLoader();
    void stopLoader();

    WorldMeshSnapshot collectSectionMeshSnapshot();
    void acknowledgeSectionMeshUploads(
        const std::vector<WorldSectionMeshVersion> &versions);

    /// The caller must hold the shared world mutex for all Locked methods.
    void queueBlockEditLocked(int blockX, int blockY, int blockZ);
    void queueLightingUpdatesLocked(
        const std::vector<glm::ivec3> &changedPositions);
    void processChunkUpdates(std::size_t budget);
    std::size_t queuedChunkUpdateCountLocked() const noexcept;
    void preloadAroundLocked(
        const glm::vec3 &position, int radius = 1,
        ChunkDemandReason reason = ChunkDemandReason::Preload);
    ChunkDemandDebugStats collectDemandDebugStats() const;
    WorldJobSchedulerDebugStats collectJobSchedulerDebugStats() const;
    ChunkBackpressureDebugStats collectBackpressureDebugStats() const;

    static std::vector<VectorXZ>
    planMeshWork(const VectorXZ &center, int radius, int sectionY,
                 const ViewFrustum *frustum);
    static std::vector<ChunkDemandTarget> planDemandWork(
        const ChunkDemandSnapshot &snapshot, int sectionY,
        const ViewFrustum *frustum, const VectorXZ &movementOrigin,
        const VectorXZ &movementDirection);
    static std::vector<glm::ivec3> planSectionMeshUploads(
        const std::vector<glm::ivec3> &readySections,
        const VectorXZ &origin, std::size_t budget);

  private:
    struct IVec3Hash {
        std::size_t operator()(const glm::ivec3 &value) const noexcept;
    };

    struct MeshPrioritySnapshot {
        ViewFrustum frustum;
        int sectionY = 0;
        bool valid = false;
    };

    void queueSectionUpdateLocked(const glm::ivec3 &key);
    void invalidateWorldJobs();
    void publishMeshPrioritySnapshot(const Camera &camera, int sectionY);
    void runLoader();

    ChunkManager &m_chunkManager;
    std::mutex &m_worldMutex;

    std::deque<glm::ivec3> m_chunkUpdateQueue;
    std::unordered_set<glm::ivec3, IVec3Hash> m_queuedChunkUpdates;

    std::atomic<bool> m_isRunning{true};
    std::vector<std::thread> m_chunkLoadThreads;
    std::mutex m_meshPriorityMutex;
    MeshPrioritySnapshot m_meshPrioritySnapshot;

    mutable std::mutex m_demandMutex;
    ChunkDemandModel m_demandModel;
    VectorXZ m_playerDemandCoord{0, 0};
    VectorXZ m_playerMovement{0, 0};
    bool m_playerDemandPublished = false;
    std::atomic<std::size_t> m_lastPlannedTargetCount{0};
    std::atomic<std::size_t> m_deferredPlanJobCount{0};
    std::atomic<std::size_t> m_lastAuthoritativeCommitCount{0};
    std::atomic<std::size_t> m_peakAuthoritativeCommitCount{0};
    std::atomic<std::size_t> m_lastCpuReadyTotal{0};
    std::atomic<std::size_t> m_lastSectionUploadsOffered{0};
    std::atomic<std::size_t> m_lastSectionUploadsDeferred{0};
    std::atomic<std::size_t> m_lastUnloadCount{0};
    WorldJobScheduler m_jobScheduler;
    std::mutex m_worldJobCommitMutex;

    std::atomic<int> m_demandSectionY{0};
    std::atomic<int> m_chunkLoadRevision{0};
    std::atomic<int> m_meshPriorityRevision{0};
    std::atomic<int> m_loadDistance{2};
    std::atomic<int> m_renderDistance;

    VectorXZ m_lastUnloadScanChunk{0, 0};
    bool m_unloadScanValid = false;
    bool m_unloadBacklog = false;

    glm::vec3 m_lastMeshPriorityRotation{0.f};
    int m_lastMeshPrioritySectionY = -1;
    bool m_meshPriorityPublished = false;
};

#endif // CHUNKRUNTIME_H_INCLUDED
