#include "OperationPerformanceTiming.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <ostream>
#include <string>

namespace
{
    bool isEnabledByEnvironment()
    {
        const char *value = std::getenv("HELLO_PERF_CAPTURE");
        if (value == nullptr || value[0] == '\0') {
            return false;
        }
        const std::string text(value);
        return text != "0" && text != "false" && text != "FALSE" &&
               text != "False" && text != "off" && text != "OFF";
    }

    double nonNegative(double value)
    {
        return std::max(0.0, value);
    }

    double phaseDuration(double complete, double previous)
    {
        return std::max(0.0, complete - previous);
    }

    const std::array<const char *, 5> StartupKeys{
        "startup_preflight_ms", "startup_ogre_ready_ms",
        "startup_first_window_ms", "startup_first_usable_menu_ms", ""};
    const std::array<const char *, 5> WorldEntryKeys{
        "entry_world_metadata_ms", "entry_spawn_resident_ms",
        "entry_first_visible_terrain_ms", "entry_first_controllable_ms", ""};
    const std::array<const char *, 5> SaveKeys{
        "save_prepare_complete_ms", "save_write_complete_ms",
        "save_flush_complete_ms", "save_validation_complete_ms",
        "save_replace_complete_ms"};
    const std::array<const char *, 5> BackupKeys{
        "backup_catalogue_scan_ms", "backup_candidate_copy_complete_ms",
        "backup_validation_complete_ms", "backup_publish_complete_ms", ""};
    const std::array<const char *, 5> RestoreKeys{
        "restore_catalogue_scan_ms", "restore_candidate_copy_complete_ms",
        "restore_validation_complete_ms", "restore_publish_complete_ms", ""};

    void writePhases(std::ostream &output, const RuntimeOperationRecord &record,
                     const std::array<const char *, 5> &keys)
    {
        for (std::size_t index = 0; index < record.phaseCount; ++index) {
            if (keys[index][0] != '\0') {
                output << keys[index] << '='
                       << record.cumulativeMilliseconds[index] << '\n';
            }
        }
    }

    void writeOutcome(std::ostream &output, const char *prefix,
                      const RuntimeOperationRecord &record)
    {
        output << prefix << "_success=" << (record.success ? 1 : 0) << '\n';
        output << prefix << "_main_thread_max_stall_ms="
               << record.mainThreadMaxStallMilliseconds << '\n';
    }
}

const char *runtimeOperationKindName(RuntimeOperationKind kind) noexcept
{
    switch (kind) {
    case RuntimeOperationKind::Startup:
        return "startup";
    case RuntimeOperationKind::Catalogue:
        return "catalogue";
    case RuntimeOperationKind::WorldEntry:
        return "world-entry";
    case RuntimeOperationKind::Save:
        return "save";
    case RuntimeOperationKind::Backup:
        return "backup";
    case RuntimeOperationKind::Restore:
        return "restore";
    case RuntimeOperationKind::Count:
        break;
    }
    return "unknown";
}

RuntimeOperationTimings::RuntimeOperationTimings(bool enabled)
    : m_enabled(enabled)
{
}

double RuntimeOperationTimings::nowMilliseconds()
{
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::size_t RuntimeOperationTimings::expectedPhaseCount(
    RuntimeOperationKind kind) noexcept
{
    switch (kind) {
    case RuntimeOperationKind::Startup:
    case RuntimeOperationKind::WorldEntry:
    case RuntimeOperationKind::Backup:
    case RuntimeOperationKind::Restore:
        return 4;
    case RuntimeOperationKind::Save:
        return 5;
    case RuntimeOperationKind::Catalogue:
    case RuntimeOperationKind::Count:
        return 0;
    }
    return 0;
}

RuntimeOperationHandle RuntimeOperationTimings::begin(RuntimeOperationKind kind)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled || kind == RuntimeOperationKind::Count) {
        return {};
    }

    for (std::size_t attempt = 0; attempt < MaxRecords; ++attempt) {
        const std::size_t slotIndex = (m_nextSlot + attempt) % MaxRecords;
        Slot &slot = m_slots[slotIndex];
        if (slot.active) {
            continue;
        }
        slot = Slot();
        slot.generation = m_nextGeneration++;
        if (slot.generation == 0) {
            slot.generation = m_nextGeneration++;
        }
        slot.record.kind = kind;
        slot.record.sequence = m_nextSequence++;
        slot.startedMilliseconds = nowMilliseconds();
        slot.lastMarkMilliseconds = slot.startedMilliseconds;
        slot.active = true;
        m_nextSlot = (slotIndex + 1) % MaxRecords;
        return {slotIndex, slot.generation};
    }

    ++m_droppedRecords;
    return {};
}

RuntimeOperationTimings::Slot *RuntimeOperationTimings::findLocked(
    const RuntimeOperationHandle &handle)
{
    if (!handle.valid() || handle.slot >= MaxRecords) {
        return nullptr;
    }
    Slot &slot = m_slots[handle.slot];
    return slot.active && slot.generation == handle.generation ? &slot
                                                               : nullptr;
}

const RuntimeOperationTimings::Slot *RuntimeOperationTimings::findLocked(
    const RuntimeOperationHandle &handle) const
{
    if (!handle.valid() || handle.slot >= MaxRecords) {
        return nullptr;
    }
    const Slot &slot = m_slots[handle.slot];
    return slot.generation == handle.generation ? &slot : nullptr;
}

RuntimeOperationHandle RuntimeOperationTimings::latestActiveLocked(
    RuntimeOperationKind kind) const
{
    RuntimeOperationHandle result;
    std::uint64_t latestSequence = 0;
    for (std::size_t index = 0; index < MaxRecords; ++index) {
        const Slot &slot = m_slots[index];
        if (slot.active && slot.record.kind == kind &&
            slot.record.sequence > latestSequence) {
            latestSequence = slot.record.sequence;
            result = {index, slot.generation};
        }
    }
    return result;
}

void RuntimeOperationTimings::mark(const RuntimeOperationHandle &handle)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Slot *slot = findLocked(handle);
    if (slot == nullptr) {
        return;
    }
    const std::size_t expected = expectedPhaseCount(slot->record.kind);
    if (slot->record.phaseCount >= expected) {
        return;
    }
    const double now = nowMilliseconds();
    const double elapsed = nonNegative(now - slot->startedMilliseconds);
    const double stall = nonNegative(now - slot->lastMarkMilliseconds);
    slot->record.cumulativeMilliseconds[slot->record.phaseCount++] = elapsed;
    slot->record.mainThreadMaxStallMilliseconds =
        std::max(slot->record.mainThreadMaxStallMilliseconds, stall);
    slot->lastMarkMilliseconds = now;
}

void RuntimeOperationTimings::markLatestActive(RuntimeOperationKind kind)
{
    RuntimeOperationHandle handle;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        handle = latestActiveLocked(kind);
    }
    mark(handle);
}

void RuntimeOperationTimings::addStorageTransaction(
    const RuntimeOperationHandle &handle, double prepareCompleteMs,
    double writeCompleteMs, double flushCompleteMs,
    double validationCompleteMs, double replaceCompleteMs, double totalMs,
    std::size_t bytesWritten, bool chunkFile)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Slot *slot = findLocked(handle);
    if (slot == nullptr || slot->record.kind != RuntimeOperationKind::Save) {
        return;
    }
    const double prepare = nonNegative(prepareCompleteMs);
    const double write = std::max(prepare, nonNegative(writeCompleteMs));
    const double flush = std::max(write, nonNegative(flushCompleteMs));
    const double validation =
        std::max(flush, nonNegative(validationCompleteMs));
    const double replace =
        std::max(validation, nonNegative(replaceCompleteMs));
    const double total = std::max(replace, nonNegative(totalMs));
    slot->phaseDurations[0] += prepare;
    slot->phaseDurations[1] += phaseDuration(write, prepare);
    slot->phaseDurations[2] += phaseDuration(flush, write);
    slot->phaseDurations[3] += phaseDuration(validation, flush);
    slot->phaseDurations[4] += phaseDuration(replace, validation);
    slot->record.mainThreadMaxStallMilliseconds =
        std::max(slot->record.mainThreadMaxStallMilliseconds, total);
    ++slot->record.filesWritten;
    if (chunkFile) {
        ++slot->record.chunksWritten;
    }
    slot->record.bytesWritten += bytesWritten;
}

void RuntimeOperationTimings::addStorageTransactionToLatest(
    RuntimeOperationKind kind, double prepareCompleteMs,
    double writeCompleteMs, double flushCompleteMs,
    double validationCompleteMs, double replaceCompleteMs, double totalMs,
    std::size_t bytesWritten, bool chunkFile)
{
    RuntimeOperationHandle handle;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        handle = latestActiveLocked(kind);
    }
    addStorageTransaction(handle, prepareCompleteMs, writeCompleteMs,
                          flushCompleteMs, validationCompleteMs,
                          replaceCompleteMs, totalMs, bytesWritten,
                          chunkFile);
}

void RuntimeOperationTimings::addCounters(
    const RuntimeOperationHandle &handle, std::size_t filesWritten,
    std::size_t chunksWritten, std::uintmax_t bytesRead,
    std::uintmax_t bytesWritten)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Slot *slot = findLocked(handle);
    if (slot == nullptr) {
        return;
    }
    slot->record.filesWritten += filesWritten;
    slot->record.chunksWritten += chunksWritten;
    slot->record.bytesRead += bytesRead;
    slot->record.bytesWritten += bytesWritten;
}

void RuntimeOperationTimings::setCatalogueEntries(
    const RuntimeOperationHandle &handle, std::size_t entries)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Slot *slot = findLocked(handle);
    if (slot != nullptr) {
        slot->record.catalogueEntries = entries;
    }
}

void RuntimeOperationTimings::complete(const RuntimeOperationHandle &handle,
                                       bool success)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Slot *slot = findLocked(handle);
    if (slot == nullptr) {
        return;
    }
    const double now = nowMilliseconds();
    double total = nonNegative(now - slot->startedMilliseconds);
    const std::size_t expected = expectedPhaseCount(slot->record.kind);
    if (slot->record.kind == RuntimeOperationKind::Save &&
        slot->record.filesWritten > 0) {
        double cumulative = 0.0;
        for (std::size_t index = 0; index < expected; ++index) {
            cumulative += slot->phaseDurations[index];
            slot->record.cumulativeMilliseconds[index] = cumulative;
        }
        slot->record.phaseCount = expected;
        total = std::max(total, cumulative);
    }
    else {
        while (slot->record.phaseCount < expected) {
            slot->record.cumulativeMilliseconds[slot->record.phaseCount++] =
                total;
        }
    }
    if (expected > 0) {
        total = std::max(
            total, slot->record.cumulativeMilliseconds[expected - 1]);
    }
    slot->record.totalMilliseconds = total;
    slot->record.mainThreadMaxStallMilliseconds = std::max(
        slot->record.mainThreadMaxStallMilliseconds,
        nonNegative(now - slot->lastMarkMilliseconds));
    slot->record.totalMilliseconds = std::max(
        slot->record.totalMilliseconds,
        slot->record.mainThreadMaxStallMilliseconds);
    slot->record.success = success;
    slot->record.complete = true;
    slot->active = false;
}

void RuntimeOperationTimings::completeLatestActive(RuntimeOperationKind kind,
                                                    bool success)
{
    RuntimeOperationHandle handle;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        handle = latestActiveLocked(kind);
    }
    complete(handle, success);
}

bool RuntimeOperationTimings::latest(RuntimeOperationKind kind,
                                     RuntimeOperationRecord &record) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const Slot *latestSlot = nullptr;
    for (const Slot &slot : m_slots) {
        if (!slot.record.complete || slot.record.kind != kind) {
            continue;
        }
        if (latestSlot == nullptr ||
            slot.record.sequence > latestSlot->record.sequence) {
            latestSlot = &slot;
        }
    }
    if (latestSlot == nullptr) {
        return false;
    }
    record = latestSlot->record;
    return true;
}

std::vector<RuntimeOperationRecord> RuntimeOperationTimings::snapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<RuntimeOperationRecord> records;
    records.reserve(MaxRecords);
    for (const Slot &slot : m_slots) {
        if (slot.record.complete) {
            records.push_back(slot.record);
        }
    }
    std::sort(records.begin(), records.end(),
              [](const RuntimeOperationRecord &left,
                 const RuntimeOperationRecord &right) {
                  return left.sequence < right.sequence;
              });
    return records;
}

void RuntimeOperationTimings::appendLatestSummary(std::ostream &output) const
{
    const std::ios::fmtflags previousFlags = output.flags();
    const std::streamsize previousPrecision = output.precision();
    output << std::fixed << std::setprecision(3);

    RuntimeOperationRecord record;
    if (latest(RuntimeOperationKind::Startup, record)) {
        writePhases(output, record, StartupKeys);
        writeOutcome(output, "startup", record);
        output << "startup_total_ms=" << record.totalMilliseconds << '\n';
    }
    if (latest(RuntimeOperationKind::Catalogue, record)) {
        writeOutcome(output, "catalogue", record);
        output << "catalogue_total_ms=" << record.totalMilliseconds << '\n';
        output << "catalogue_entries=" << record.catalogueEntries << '\n';
    }
    if (latest(RuntimeOperationKind::WorldEntry, record)) {
        writePhases(output, record, WorldEntryKeys);
        writeOutcome(output, "entry", record);
        output << "entry_total_ms=" << record.totalMilliseconds << '\n';
    }
    if (latest(RuntimeOperationKind::Save, record)) {
        writePhases(output, record, SaveKeys);
        writeOutcome(output, "save", record);
        output << "save_total_ms=" << record.totalMilliseconds << '\n';
        output << "save_files_written=" << record.filesWritten << '\n';
        output << "save_chunks_written=" << record.chunksWritten << '\n';
        output << "save_bytes_written=" << record.bytesWritten << '\n';
    }
    if (latest(RuntimeOperationKind::Backup, record)) {
        writePhases(output, record, BackupKeys);
        writeOutcome(output, "backup", record);
        output << "backup_total_ms=" << record.totalMilliseconds << '\n';
        output << "backup_files_written=" << record.filesWritten << '\n';
        output << "backup_bytes_read=" << record.bytesRead << '\n';
        output << "backup_bytes_written=" << record.bytesWritten << '\n';
    }
    if (latest(RuntimeOperationKind::Restore, record)) {
        writePhases(output, record, RestoreKeys);
        writeOutcome(output, "restore", record);
        output << "restore_total_ms=" << record.totalMilliseconds << '\n';
        output << "restore_files_written=" << record.filesWritten << '\n';
        output << "restore_bytes_read=" << record.bytesRead << '\n';
        output << "restore_bytes_written=" << record.bytesWritten << '\n';
    }
    output << "operation_timing_dropped_records=" << droppedRecords() << '\n';
    output.flags(previousFlags);
    output.precision(previousPrecision);
}

bool RuntimeOperationTimings::enabled() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_enabled;
}

std::size_t RuntimeOperationTimings::droppedRecords() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_droppedRecords;
}

void RuntimeOperationTimings::reset(bool enabledValue)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_slots = {};
    m_enabled = enabledValue;
    m_nextSlot = 0;
    m_droppedRecords = 0;
    m_nextGeneration = 1;
    m_nextSequence = 1;
}

RuntimeOperationTimings &runtimeOperationTimings()
{
    static RuntimeOperationTimings timings(isEnabledByEnvironment());
    return timings;
}
