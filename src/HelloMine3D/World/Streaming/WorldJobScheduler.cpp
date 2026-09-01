#include "WorldJobScheduler.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace {

int typeRank(WorldJobType type) noexcept
{
    switch (type) {
    case WorldJobType::ChunkLoadOrGenerate:
        return 0;
    case WorldJobType::ChunkMeshBuild:
        return 1;
    }
    return 2;
}

bool sameKey(const WorldJobRequest &left,
             const WorldJobRequest &right) noexcept
{
    return left.type == right.type && left.target == right.target;
}

} // namespace

bool WorldJobScheduler::submit(const WorldJobRequest &request)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return submitLocked(request);
}

bool WorldJobScheduler::replacePending(
    const std::vector<WorldJobRequest> &requests,
    WorldJobGenerationToken generation)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!isCurrent(generation)) {
        ++m_totals.stalePlanRejections;
        return false;
    }
    const bool tokenMismatch = std::any_of(
        requests.begin(), requests.end(),
        [generation](const WorldJobRequest &request) {
            return request.generation.value != generation.value;
        });
    if (tokenMismatch) {
        ++m_totals.stalePlanRejections;
        return false;
    }
    m_totals.replacedPendingJobs += m_pending.size();
    m_pending.clear();
    for (const WorldJobRequest &request : requests) {
        submitLocked(request);
    }
    return true;
}

WorldJobGenerationToken
WorldJobScheduler::currentGenerationToken() const noexcept
{
    return {m_generation.load()};
}

WorldJobGenerationToken WorldJobScheduler::invalidateGeneration()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::uint64_t current = m_generation.load();
    if (current == std::numeric_limits<std::uint64_t>::max()) {
        throw std::logic_error("World job generation overflow");
    }
    const WorldJobGenerationToken next{current + 1};
    m_generation.store(next.value);
    ++m_totals.generationInvalidations;
    m_totals.cancelledPendingJobs += m_pending.size();
    m_pending.clear();
    return next;
}

bool WorldJobScheduler::isCurrent(
    WorldJobGenerationToken generation) const noexcept
{
    return generation.value != 0 &&
           generation.value == m_generation.load();
}

bool WorldJobScheduler::takeNext(WorldJob &job)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_inFlight.has_value() || m_pending.empty()) {
        return false;
    }

    job = m_pending.front();
    m_pending.erase(m_pending.begin());
    job.state = WorldJobState::InFlight;
    job.queueLatencyMilliseconds =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - job.enqueuedAt)
            .count();
    m_inFlight = job;
    ++m_totals.startedJobs;
    m_totals.lastQueueLatencyMilliseconds =
        job.queueLatencyMilliseconds;
    return true;
}

bool WorldJobScheduler::complete(const WorldJob &job,
                                 WorldJobOutcome outcome,
                                 double workerMilliseconds,
                                 double commitMilliseconds)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_inFlight.has_value() ||
        m_inFlight->id != job.id ||
        m_inFlight->state != WorldJobState::InFlight ||
        job.state != WorldJobState::InFlight) {
        return false;
    }

    WorldJob completed = *m_inFlight;
    completed.state = WorldJobState::Completed;
    m_completed.push_back(WorldJobCompletion{
        completed, outcome, std::max(0.0, workerMilliseconds),
        std::max(0.0, commitMilliseconds)});
    m_inFlight.reset();

    ++m_totals.completedJobs;
    switch (outcome) {
    case WorldJobOutcome::DidWork:
        ++m_totals.didWorkJobs;
        break;
    case WorldJobOutcome::NoWork:
        ++m_totals.noWorkJobs;
        break;
    case WorldJobOutcome::CommitRejected:
        ++m_totals.commitRejectedJobs;
        break;
    case WorldJobOutcome::Cancelled:
        ++m_totals.cancelledJobs;
        break;
    }
    switch (completed.type) {
    case WorldJobType::ChunkLoadOrGenerate:
        ++m_totals.chunkLoadOrGenerateJobs;
        break;
    case WorldJobType::ChunkMeshBuild:
        ++m_totals.chunkMeshBuildJobs;
        break;
    }
    m_totals.lastWorkerMilliseconds =
        std::max(0.0, workerMilliseconds);
    m_totals.lastCommitMilliseconds =
        std::max(0.0, commitMilliseconds);
    return true;
}

bool WorldJobScheduler::popCompleted(WorldJobCompletion &completion)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_completed.empty()) {
        return false;
    }
    completion = std::move(m_completed.front());
    m_completed.pop_front();
    return true;
}

WorldJobSchedulerDebugStats WorldJobScheduler::debugStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    WorldJobSchedulerDebugStats stats = m_totals;
    stats.currentGeneration = m_generation.load();
    stats.pendingJobs = m_pending.size();
    stats.inFlightJobs = m_inFlight.has_value() ? 1u : 0u;
    stats.completedResults = m_completed.size();
    return stats;
}

const char *WorldJobScheduler::typeName(WorldJobType type) noexcept
{
    switch (type) {
    case WorldJobType::ChunkLoadOrGenerate:
        return "ChunkLoadOrGenerate";
    case WorldJobType::ChunkMeshBuild:
        return "ChunkMeshBuild";
    }
    return "Unknown";
}

const char *WorldJobScheduler::stateName(WorldJobState state) noexcept
{
    switch (state) {
    case WorldJobState::Pending:
        return "Pending";
    case WorldJobState::InFlight:
        return "InFlight";
    case WorldJobState::Completed:
        return "Completed";
    }
    return "Unknown";
}

const char *WorldJobScheduler::outcomeName(
    WorldJobOutcome outcome) noexcept
{
    switch (outcome) {
    case WorldJobOutcome::DidWork:
        return "DidWork";
    case WorldJobOutcome::NoWork:
        return "NoWork";
    case WorldJobOutcome::CommitRejected:
        return "CommitRejected";
    case WorldJobOutcome::Cancelled:
        return "Cancelled";
    }
    return "Unknown";
}

bool WorldJobScheduler::submitLocked(const WorldJobRequest &request)
{
    if (!isCurrent(request.generation)) {
        ++m_totals.staleSubmitRejections;
        return false;
    }
    if (hasPendingKeyLocked(request)) {
        return false;
    }

    WorldJob job;
    static_cast<WorldJobRequest &>(job) = request;
    job.id = m_nextId++;
    job.state = WorldJobState::Pending;
    job.enqueuedAt = std::chrono::steady_clock::now();
    m_pending.push_back(job);
    ++m_totals.submittedJobs;
    sortPendingLocked();
    return true;
}

void WorldJobScheduler::sortPendingLocked()
{
    std::sort(m_pending.begin(), m_pending.end(),
              [](const WorldJob &left, const WorldJob &right) {
                  if (left.priority != right.priority) {
                      return left.priority > right.priority;
                  }
                  if (left.planOrder != right.planOrder) {
                      return left.planOrder < right.planOrder;
                  }
                  if (left.demandEpoch != right.demandEpoch) {
                      return left.demandEpoch > right.demandEpoch;
                  }
                  const int leftType = typeRank(left.type);
                  const int rightType = typeRank(right.type);
                  if (leftType != rightType) {
                      return leftType < rightType;
                  }
                  return left.id < right.id;
              });
}

bool WorldJobScheduler::hasPendingKeyLocked(
    const WorldJobRequest &request) const noexcept
{
    return std::any_of(
        m_pending.begin(), m_pending.end(),
        [&request](const WorldJob &job) { return sameKey(job, request); });
}
