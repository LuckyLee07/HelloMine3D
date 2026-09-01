#ifndef WORLDJOBSCHEDULER_H_INCLUDED
#define WORLDJOBSCHEDULER_H_INCLUDED

#include <chrono>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

#include "../../Maths/Vector2XZ.h"
#include "../../Util/NonCopyable.h"

enum class WorldJobType : std::uint8_t {
    ChunkLoadOrGenerate,
    ChunkMeshBuild,
};

enum class WorldJobState : std::uint8_t {
    Pending,
    InFlight,
    Completed,
};

enum class WorldJobOutcome : std::uint8_t {
    DidWork,
    NoWork,
    CommitRejected,
    Cancelled,
};

struct WorldJobGenerationToken {
    std::uint64_t value = 0;
};

struct WorldJobRequest {
    WorldJobType type = WorldJobType::ChunkLoadOrGenerate;
    VectorXZ target{0, 0};
    int priority = 0;
    std::uint64_t demandEpoch = 0;
    std::size_t planOrder = 0;
    WorldJobGenerationToken generation;
};

struct WorldJob : WorldJobRequest {
    std::uint64_t id = 0;
    WorldJobState state = WorldJobState::Pending;
    std::chrono::steady_clock::time_point enqueuedAt{};
    double queueLatencyMilliseconds = 0.0;
};

struct WorldJobCompletion {
    WorldJob job;
    WorldJobOutcome outcome = WorldJobOutcome::NoWork;
    double workerMilliseconds = 0.0;
    double commitMilliseconds = 0.0;
};

struct WorldJobSchedulerDebugStats {
    std::size_t pendingJobs = 0;
    std::size_t inFlightJobs = 0;
    std::size_t completedResults = 0;
    std::uint64_t submittedJobs = 0;
    std::uint64_t startedJobs = 0;
    std::uint64_t completedJobs = 0;
    std::uint64_t didWorkJobs = 0;
    std::uint64_t noWorkJobs = 0;
    std::uint64_t commitRejectedJobs = 0;
    std::uint64_t cancelledJobs = 0;
    std::uint64_t chunkLoadOrGenerateJobs = 0;
    std::uint64_t chunkMeshBuildJobs = 0;
    std::uint64_t replacedPendingJobs = 0;
    std::uint64_t currentGeneration = 1;
    std::uint64_t generationInvalidations = 0;
    std::uint64_t cancelledPendingJobs = 0;
    std::uint64_t staleSubmitRejections = 0;
    std::uint64_t stalePlanRejections = 0;
    double lastQueueLatencyMilliseconds = 0.0;
    double lastWorkerMilliseconds = 0.0;
    double lastCommitMilliseconds = 0.0;
};

class WorldJobScheduler final : public NonCopyable {
  public:
    bool submit(const WorldJobRequest &request);
    bool replacePending(const std::vector<WorldJobRequest> &requests,
                        WorldJobGenerationToken generation);
    WorldJobGenerationToken currentGenerationToken() const noexcept;
    WorldJobGenerationToken invalidateGeneration();
    bool isCurrent(WorldJobGenerationToken generation) const noexcept;
    bool takeNext(WorldJob &job);
    bool complete(const WorldJob &job, WorldJobOutcome outcome,
                  double workerMilliseconds, double commitMilliseconds);
    bool popCompleted(WorldJobCompletion &completion);

    WorldJobSchedulerDebugStats debugStats() const;

    static const char *typeName(WorldJobType type) noexcept;
    static const char *stateName(WorldJobState state) noexcept;
    static const char *outcomeName(WorldJobOutcome outcome) noexcept;

  private:
    bool submitLocked(const WorldJobRequest &request);
    void sortPendingLocked();
    bool hasPendingKeyLocked(const WorldJobRequest &request) const noexcept;

    mutable std::mutex m_mutex;
    std::vector<WorldJob> m_pending;
    std::optional<WorldJob> m_inFlight;
    std::deque<WorldJobCompletion> m_completed;
    std::uint64_t m_nextId = 1;
    std::atomic<std::uint64_t> m_generation{1};
    WorldJobSchedulerDebugStats m_totals;
};

#endif // WORLDJOBSCHEDULER_H_INCLUDED
