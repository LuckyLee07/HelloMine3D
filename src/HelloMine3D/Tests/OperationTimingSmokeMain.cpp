#include "../Diagnostics/OperationPerformanceTiming.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    class TestSuite {
      public:
        void check(const std::string &id, bool passed,
                   const std::string &detail = std::string())
        {
            ++m_checks;
            if (!passed) {
                ++m_failures;
            }
            std::cout << "[OPERATION_TIMING_TEST] "
                      << (passed ? "PASS " : "FAIL ") << id;
            if (!detail.empty()) {
                std::cout << " :: " << detail;
            }
            std::cout << '\n';
        }

        int finish() const
        {
            std::cout << "[OPERATION_TIMING_TEST] checks=" << m_checks
                      << " failures=" << m_failures << '\n';
            std::cout << "[OPERATION_TIMING_TEST] status="
                      << (m_failures == 0 ? "PASS" : "FAIL") << '\n';
            return m_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        }

      private:
        int m_checks = 0;
        int m_failures = 0;
    };

    bool monotonic(const RuntimeOperationRecord &record)
    {
        double previous = 0.0;
        for (std::size_t index = 0; index < record.phaseCount; ++index) {
            const double current = record.cumulativeMilliseconds[index];
            if (current < previous || current > record.totalMilliseconds) {
                return false;
            }
            previous = current;
        }
        return record.mainThreadMaxStallMilliseconds >= 0.0 &&
               record.mainThreadMaxStallMilliseconds <=
                   record.totalMilliseconds;
    }

    void completePhased(RuntimeOperationTimings &timings,
                        RuntimeOperationKind kind, std::size_t phases,
                        bool success)
    {
        const RuntimeOperationHandle handle = timings.begin(kind);
        for (std::size_t index = 0; index < phases; ++index) {
            timings.mark(handle);
        }
        timings.complete(handle, success);
    }

    bool containsAll(const std::string &text,
                     const std::vector<std::string> &needles)
    {
        return std::all_of(
            needles.begin(), needles.end(), [&](const std::string &needle) {
                return text.find(needle) != std::string::npos;
            });
    }
}

int main()
{
    TestSuite suite;

    suite.check(
        "Q2/operation-names-are-stable",
        std::string(runtimeOperationKindName(RuntimeOperationKind::Startup)) ==
                "startup" &&
            std::string(runtimeOperationKindName(
                RuntimeOperationKind::Catalogue)) == "catalogue" &&
            std::string(runtimeOperationKindName(
                RuntimeOperationKind::WorldEntry)) == "world-entry" &&
            std::string(runtimeOperationKindName(RuntimeOperationKind::Save)) ==
                "save" &&
            std::string(runtimeOperationKindName(
                RuntimeOperationKind::Backup)) == "backup" &&
            std::string(runtimeOperationKindName(
                RuntimeOperationKind::Restore)) == "restore");

    {
        RuntimeOperationTimings timings(false);
        const RuntimeOperationHandle handle =
            timings.begin(RuntimeOperationKind::Save);
        suite.check("Q2/disabled-collector-retains-nothing",
                    !handle.valid() && timings.snapshot().empty() &&
                        timings.droppedRecords() == 0);
    }

    {
        RuntimeOperationTimings timings(true);
        completePhased(timings, RuntimeOperationKind::Startup, 4, true);
        RuntimeOperationRecord record;
        suite.check("Q2/startup-success-has-complete-monotonic-phases",
                    timings.latest(RuntimeOperationKind::Startup, record) &&
                        record.success && record.phaseCount == 4 &&
                        monotonic(record));
    }

    {
        RuntimeOperationTimings timings(true);
        completePhased(timings, RuntimeOperationKind::WorldEntry, 1, false);
        RuntimeOperationRecord record;
        suite.check("Q2/failure-fills-a-complete-phase-summary",
                    timings.latest(RuntimeOperationKind::WorldEntry, record) &&
                        !record.success && record.phaseCount == 4 &&
                        monotonic(record) &&
                        record.cumulativeMilliseconds[1] ==
                            record.totalMilliseconds &&
                        record.cumulativeMilliseconds[3] ==
                            record.totalMilliseconds);
    }

    {
        RuntimeOperationTimings timings(true);
        const RuntimeOperationHandle save =
            timings.begin(RuntimeOperationKind::Save);
        timings.addStorageTransaction(save, 1.0, 3.0, 5.0, 7.0, 9.0,
                                      10.0, 100, true);
        timings.addStorageTransaction(save, 2.0, 4.0, 6.0, 8.0, 10.0,
                                      11.0, 200, false);
        timings.complete(save, true);
        RuntimeOperationRecord record;
        suite.check(
            "Q2/save-aggregates-transaction-phases-and-counters",
            timings.latest(RuntimeOperationKind::Save, record) &&
                record.success && record.phaseCount == 5 &&
                record.cumulativeMilliseconds[0] == 3.0 &&
                record.cumulativeMilliseconds[1] == 7.0 &&
                record.cumulativeMilliseconds[2] == 11.0 &&
                record.cumulativeMilliseconds[3] == 15.0 &&
                record.cumulativeMilliseconds[4] == 19.0 &&
                record.totalMilliseconds >= 19.0 &&
                record.mainThreadMaxStallMilliseconds >= 11.0 &&
                record.filesWritten == 2 && record.chunksWritten == 1 &&
                record.bytesWritten == 300 && monotonic(record));
    }

    {
        RuntimeOperationTimings timings(true);
        for (std::size_t index = 0;
             index < RuntimeOperationTimings::MaxRecords + 8; ++index) {
            const RuntimeOperationHandle handle =
                timings.begin(RuntimeOperationKind::Catalogue);
            timings.setCatalogueEntries(handle, index);
            timings.complete(handle, true);
        }
        RuntimeOperationRecord latest;
        suite.check("Q2/completed-record-ring-is-bounded",
                    timings.snapshot().size() ==
                            RuntimeOperationTimings::MaxRecords &&
                        timings.droppedRecords() == 0 &&
                        timings.latest(RuntimeOperationKind::Catalogue,
                                       latest) &&
                        latest.catalogueEntries ==
                            RuntimeOperationTimings::MaxRecords + 7);
    }

    {
        RuntimeOperationTimings timings(true);
        std::vector<RuntimeOperationHandle> active;
        for (std::size_t index = 0;
             index < RuntimeOperationTimings::MaxRecords + 1; ++index) {
            active.push_back(timings.begin(RuntimeOperationKind::Restore));
        }
        const bool lastRejected = !active.back().valid();
        for (const RuntimeOperationHandle &handle : active) {
            timings.complete(handle, false);
        }
        suite.check("Q2/simultaneous-overflow-is-counted-not-allocated",
                    lastRejected && timings.droppedRecords() == 1 &&
                        timings.snapshot().size() ==
                            RuntimeOperationTimings::MaxRecords);
    }

    {
        RuntimeOperationTimings timings(true);
        completePhased(timings, RuntimeOperationKind::Startup, 4, true);
        const RuntimeOperationHandle catalogue =
            timings.begin(RuntimeOperationKind::Catalogue);
        timings.setCatalogueEntries(catalogue, 3);
        timings.complete(catalogue, true);
        completePhased(timings, RuntimeOperationKind::WorldEntry, 4, true);

        const RuntimeOperationHandle save =
            timings.begin(RuntimeOperationKind::Save);
        timings.addStorageTransaction(save, 1, 2, 3, 4, 5, 6, 64, true);
        timings.complete(save, true);

        for (RuntimeOperationKind kind : {RuntimeOperationKind::Backup,
                                          RuntimeOperationKind::Restore}) {
            const RuntimeOperationHandle handle = timings.begin(kind);
            timings.addCounters(handle, 2, 0, 128, 256);
            for (int phase = 0; phase < 4; ++phase) {
                timings.mark(handle);
            }
            timings.complete(handle, kind == RuntimeOperationKind::Backup);
        }

        std::ostringstream output;
        timings.appendLatestSummary(output);
        const std::string summary = output.str();
        suite.check(
            "Q2/summary-emits-all-q1-phase-and-counter-keys",
            containsAll(
                summary,
                {"startup_preflight_ms=", "startup_ogre_ready_ms=",
                 "startup_first_window_ms=",
                 "startup_first_usable_menu_ms=",
                 "catalogue_total_ms=", "catalogue_entries=3",
                 "entry_world_metadata_ms=", "entry_spawn_resident_ms=",
                 "entry_first_visible_terrain_ms=",
                 "entry_first_controllable_ms=",
                 "save_prepare_complete_ms=", "save_write_complete_ms=",
                 "save_flush_complete_ms=",
                 "save_validation_complete_ms=",
                 "save_replace_complete_ms=", "save_total_ms=",
                 "save_main_thread_max_stall_ms=",
                 "save_files_written=1", "save_chunks_written=1",
                 "save_bytes_written=64", "backup_total_ms=",
                 "backup_success=1", "restore_catalogue_scan_ms=",
                 "restore_candidate_copy_complete_ms=",
                 "restore_validation_complete_ms=",
                 "restore_publish_complete_ms=", "restore_total_ms=",
                 "restore_main_thread_max_stall_ms=",
                 "restore_bytes_read=128", "restore_bytes_written=256",
                 "restore_success=0",
                 "operation_timing_dropped_records=0"}),
            summary);
    }

    {
        RuntimeOperationTimings timings(true);
        completePhased(timings, RuntimeOperationKind::Startup, 1, false);
        const RuntimeOperationHandle catalogue =
            timings.begin(RuntimeOperationKind::Catalogue);
        timings.complete(catalogue, false);
        completePhased(timings, RuntimeOperationKind::WorldEntry, 2, false);
        const RuntimeOperationHandle save =
            timings.begin(RuntimeOperationKind::Save);
        timings.addStorageTransaction(save, 1, 2, 2, 2, 2, 2, 7, true);
        timings.complete(save, false);
        completePhased(timings, RuntimeOperationKind::Backup, 3, false);
        completePhased(timings, RuntimeOperationKind::Restore, 2, false);

        const std::vector<RuntimeOperationRecord> records = timings.snapshot();
        const bool completeFailures =
            records.size() == 6 &&
            std::all_of(records.begin(), records.end(),
                        [](const RuntimeOperationRecord &record) {
                            const std::size_t expectedPhases =
                                record.kind == RuntimeOperationKind::Catalogue
                                    ? 0
                                    : record.kind == RuntimeOperationKind::Save
                                          ? 5
                                          : 4;
                            return record.complete && !record.success &&
                                   record.phaseCount == expectedPhases &&
                                   monotonic(record);
                        });
        suite.check("Q2/all-operation-failures-have-complete-records",
                    completeFailures);

        std::ostringstream output;
        timings.appendLatestSummary(output);
        suite.check(
            "Q2/all-operation-failures-emit-outcome-summary",
            containsAll(output.str(),
                        {"startup_success=0", "catalogue_success=0",
                         "entry_success=0", "save_success=0",
                         "backup_success=0", "restore_success=0"}),
            output.str());
    }

    {
        RuntimeOperationTimings timings(true);
        completePhased(timings, RuntimeOperationKind::Backup, 4, false);
        completePhased(timings, RuntimeOperationKind::Backup, 4, true);
        RuntimeOperationRecord record;
        suite.check("Q2/latest-summary-selects-newest-operation",
                    timings.latest(RuntimeOperationKind::Backup, record) &&
                        record.success && record.sequence == 2);

        timings.reset(false);
        suite.check("Q2/reset-clears-records-and-disables-capture",
                    !timings.enabled() && timings.snapshot().empty() &&
                        timings.droppedRecords() == 0);
    }

    return suite.finish();
}
