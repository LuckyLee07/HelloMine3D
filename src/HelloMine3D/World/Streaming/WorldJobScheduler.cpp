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

bool jobPrecedes(const WorldJob &left, const WorldJob &right) noexcept
{
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
}

} // namespace

WorldJobAdmissionResult
WorldJobScheduler::admit(const WorldJobRequest &request)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return admitLocked(request);
}

bool WorldJobScheduler::submit(const WorldJobRequest &request)
{
    return admissionAccepted(admit(request));
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
    refreshPressureLocked();
    for (const WorldJobRequest &request : requests) {
        admitLocked(request);
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
    refreshPressureLocked();
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
    refreshPressureLocked();
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
    stats.pendingGenerationJobs =
        pendingCountLocked(WorldJobType::ChunkLoadOrGenerate);
    stats.pendingMeshJobs =
        pendingCountLocked(WorldJobType::ChunkMeshBuild);
    stats.inFlightJobs = m_inFlight.has_value() ? 1u : 0u;
    stats.completedResults = m_completed.size();
    stats.maxPendingJobs = MaxPendingJobs;
    stats.maxPendingGenerationJobs = MaxPendingGenerationJobs;
    stats.maxPendingMeshJobs = MaxPendingMeshJobs;
    stats.pendingHighWatermark = PendingHighWatermark;
    stats.pendingLowWatermark = PendingLowWatermark;
    stats.pressureLevel = m_pressure;
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

const char *WorldJobScheduler::admissionName(
    WorldJobAdmissionResult result) noexcept
{
    switch (result) {
    case WorldJobAdmissionResult::Accepted:
        return "Accepted";
    case WorldJobAdmissionResult::AcceptedAfterShedding:
        return "AcceptedAfterShedding";
    case WorldJobAdmissionResult::Duplicate:
        return "Duplicate";
    case WorldJobAdmissionResult::StaleGeneration:
        return "StaleGeneration";
    case WorldJobAdmissionResult::RejectedAtCapacity:
        return "RejectedAtCapacity";
    }
    return "Unknown";
}

const char *WorldJobScheduler::pressureName(
    WorldJobPressureLevel level) noexcept
{
    switch (level) {
    case WorldJobPressureLevel::Normal:
        return "Normal";
    case WorldJobPressureLevel::Elevated:
        return "Elevated";
    case WorldJobPressureLevel::Saturated:
        return "Saturated";
    }
    return "Unknown";
}

bool WorldJobScheduler::admissionAccepted(
    WorldJobAdmissionResult result) noexcept
{
    return result == WorldJobAdmissionResult::Accepted ||
           result == WorldJobAdmissionResult::AcceptedAfterShedding;
}

WorldJobAdmissionResult
WorldJobScheduler::admitLocked(const WorldJobRequest &request)
{
    if (!isCurrent(request.generation)) {
        ++m_totals.staleSubmitRejections;
        return WorldJobAdmissionResult::StaleGeneration;
    }
    if (hasPendingKeyLocked(request)) {
        ++m_totals.duplicateAdmissionRejections;
        return WorldJobAdmissionResult::Duplicate;
    }

    WorldJob job;
    static_cast<WorldJobRequest &>(job) = request;
    job.id = m_nextId;
    job.state = WorldJobState::Pending;
    job.enqueuedAt = std::chrono::steady_clock::now();

    const std::size_t typeLimit =
        request.type == WorldJobType::ChunkLoadOrGenerate
            ? MaxPendingGenerationJobs
            : MaxPendingMeshJobs;
    const bool typeAtCapacity =
        pendingCountLocked(request.type) >= typeLimit;
    const bool totalAtCapacity = m_pending.size() >= MaxPendingJobs;
    WorldJobAdmissionResult result = WorldJobAdmissionResult::Accepted;
    if (typeAtCapacity || totalAtCapacity) {
        auto worst = m_pending.end();
        for (auto candidate = m_pending.begin();
             candidate != m_pending.end(); ++candidate) {
            if (typeAtCapacity && candidate->type != request.type) {
                continue;
            }
            if (worst == m_pending.end() || jobPrecedes(*worst, *candidate)) {
                worst = candidate;
            }
        }
        if (worst == m_pending.end() || !jobPrecedes(job, *worst)) {
            ++m_totals.capacityAdmissionRejections;
            return WorldJobAdmissionResult::RejectedAtCapacity;
        }
        ++m_totals.shedPendingJobs;
        if (worst->type == WorldJobType::ChunkLoadOrGenerate) {
            ++m_totals.shedGenerationJobs;
        }
        else {
            ++m_totals.shedMeshJobs;
        }
        m_pending.erase(worst);
        ++m_totals.acceptedAfterSheddingJobs;
        result = WorldJobAdmissionResult::AcceptedAfterShedding;
    }

    ++m_nextId;
    m_pending.push_back(job);
    ++m_totals.submittedJobs;
    ++m_totals.acceptedAdmissions;
    sortPendingLocked();
    recordPendingPeaksLocked();
    refreshPressureLocked();
    return result;
}

void WorldJobScheduler::sortPendingLocked()
{
    std::sort(m_pending.begin(), m_pending.end(), jobPrecedes);
}

bool WorldJobScheduler::hasPendingKeyLocked(
    const WorldJobRequest &request) const noexcept
{
    return std::any_of(
        m_pending.begin(), m_pending.end(),
        [&request](const WorldJob &job) { return sameKey(job, request); });
}

std::size_t WorldJobScheduler::pendingCountLocked(
    WorldJobType type) const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        m_pending.begin(), m_pending.end(),
        [type](const WorldJob &job) { return job.type == type; }));
}

void WorldJobScheduler::refreshPressureLocked()
{
    WorldJobPressureLevel next = m_pressure;
    if (m_pressure == WorldJobPressureLevel::Normal) {
        if (m_pending.size() >= MaxPendingJobs) {
            next = WorldJobPressureLevel::Saturated;
        }
        else if (m_pending.size() >= PendingHighWatermark) {
            next = WorldJobPressureLevel::Elevated;
        }
    }
    else if (m_pending.size() <= PendingLowWatermark) {
        next = WorldJobPressureLevel::Normal;
    }
    else if (m_pressure == WorldJobPressureLevel::Elevated &&
             m_pending.size() >= MaxPendingJobs) {
        next = WorldJobPressureLevel::Saturated;
    }

    if (next == m_pressure) {
        return;
    }
    m_pressure = next;
    ++m_totals.pressureTransitions;
    if (next == WorldJobPressureLevel::Saturated) {
        ++m_totals.saturationEpisodes;
    }
}

void WorldJobScheduler::recordPendingPeaksLocked()
{
    m_totals.peakPendingJobs =
        std::max(m_totals.peakPendingJobs, m_pending.size());
    m_totals.peakPendingGenerationJobs = std::max(
        m_totals.peakPendingGenerationJobs,
        pendingCountLocked(WorldJobType::ChunkLoadOrGenerate));
    m_totals.peakPendingMeshJobs = std::max(
        m_totals.peakPendingMeshJobs,
        pendingCountLocked(WorldJobType::ChunkMeshBuild));
}
