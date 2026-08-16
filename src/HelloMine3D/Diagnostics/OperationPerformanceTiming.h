#ifndef OPERATIONPERFORMANCETIMING_H_INCLUDED
#define OPERATIONPERFORMANCETIMING_H_INCLUDED

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <mutex>
#include <vector>

enum class RuntimeOperationKind {
    Startup,
    Catalogue,
    WorldEntry,
    Save,
    Backup,
    Restore,
    Count
};

const char *runtimeOperationKindName(RuntimeOperationKind kind) noexcept;

struct RuntimeOperationHandle {
    std::size_t slot = static_cast<std::size_t>(-1);
    std::uint64_t generation = 0;

    bool valid() const noexcept
    {
        return generation != 0;
    }
};

struct RuntimeOperationRecord {
    static constexpr std::size_t MaxPhases = 5;

    RuntimeOperationKind kind = RuntimeOperationKind::Startup;
    bool success = false;
    bool complete = false;
    std::array<double, MaxPhases> cumulativeMilliseconds{};
    std::size_t phaseCount = 0;
    double totalMilliseconds = 0.0;
    double mainThreadMaxStallMilliseconds = 0.0;
    std::size_t filesWritten = 0;
    std::size_t chunksWritten = 0;
    std::uintmax_t bytesRead = 0;
    std::uintmax_t bytesWritten = 0;
    std::size_t catalogueEntries = 0;
    std::uint64_t sequence = 0;
};

/// A fixed-capacity, renderer-independent operation timeline. When disabled,
/// begin() returns an invalid handle and no record or timestamp is retained.
/// Completed records form a bounded ring so repeated saves cannot grow memory.
class RuntimeOperationTimings {
  public:
    static constexpr std::size_t MaxRecords = 32;

    explicit RuntimeOperationTimings(bool enabled = false);

    RuntimeOperationHandle begin(RuntimeOperationKind kind);
    void mark(const RuntimeOperationHandle &handle);
    void markLatestActive(RuntimeOperationKind kind);
    void addStorageTransaction(
        const RuntimeOperationHandle &handle, double prepareCompleteMs,
        double writeCompleteMs, double flushCompleteMs,
        double validationCompleteMs, double replaceCompleteMs,
        double totalMs, std::size_t bytesWritten, bool chunkFile);
    void addStorageTransactionToLatest(
        RuntimeOperationKind kind, double prepareCompleteMs,
        double writeCompleteMs, double flushCompleteMs,
        double validationCompleteMs, double replaceCompleteMs,
        double totalMs, std::size_t bytesWritten, bool chunkFile);
    void addCounters(const RuntimeOperationHandle &handle,
                     std::size_t filesWritten, std::size_t chunksWritten,
                     std::uintmax_t bytesRead,
                     std::uintmax_t bytesWritten);
    void setCatalogueEntries(const RuntimeOperationHandle &handle,
                             std::size_t entries);
    void complete(const RuntimeOperationHandle &handle, bool success);
    void completeLatestActive(RuntimeOperationKind kind, bool success);

    bool latest(RuntimeOperationKind kind,
                RuntimeOperationRecord &record) const;
    std::vector<RuntimeOperationRecord> snapshot() const;
    void appendLatestSummary(std::ostream &output) const;

    bool enabled() const;
    std::size_t droppedRecords() const;

    /// Test/automation reset. Production callers should rely on the
    /// HELLO_PERF_CAPTURE environment switch used by the global instance.
    void reset(bool enabled);

  private:
    struct Slot {
        RuntimeOperationRecord record;
        std::array<double, RuntimeOperationRecord::MaxPhases> phaseDurations{};
        double startedMilliseconds = 0.0;
        double lastMarkMilliseconds = 0.0;
        std::uint64_t generation = 0;
        bool active = false;
    };

    static double nowMilliseconds();
    static std::size_t expectedPhaseCount(RuntimeOperationKind kind) noexcept;
    Slot *findLocked(const RuntimeOperationHandle &handle);
    const Slot *findLocked(const RuntimeOperationHandle &handle) const;
    RuntimeOperationHandle latestActiveLocked(RuntimeOperationKind kind) const;

    mutable std::mutex m_mutex;
    std::array<Slot, MaxRecords> m_slots{};
    bool m_enabled = false;
    std::size_t m_nextSlot = 0;
    std::size_t m_droppedRecords = 0;
    std::uint64_t m_nextGeneration = 1;
    std::uint64_t m_nextSequence = 1;
};

RuntimeOperationTimings &runtimeOperationTimings();

#endif // OPERATIONPERFORMANCETIMING_H_INCLUDED
