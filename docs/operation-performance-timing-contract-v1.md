# Operation Performance Timing Contract v1

This contract defines the renderer-independent Q2 timing channel used by the
stage-8 performance summaries. It extends the scene and metric names frozen by
`tools/performance-contract-v1.json`; it does not assign or relax an approved
performance budget.

## Activation and bounds

The production collector follows `HELLO_PERF_CAPTURE`. When the variable is
unset or false, `begin()` returns an invalid handle before reading a clock and
no operation record is retained. Call sites remain unconditional so enabling
capture cannot select a different gameplay or storage path.

When enabled, the collector owns exactly 32 slots. Completed operations reuse
the ring in sequence order. If all 32 slots are simultaneously active, a new
operation is rejected and `operation_timing_dropped_records` increments; the
collector never allocates an unbounded queue. A mutex protects begin, phase,
counter, completion, reset and snapshot operations.

## Operations and cumulative phases

All phase values are non-negative milliseconds measured from the operation
start. They are cumulative and never exceed the operation total.

| Operation | Phase keys | Boundary |
| --------- | ---------- | -------- |
| Startup | `startup_preflight_ms` | Required-resource manifest, packs, blocks and recipes are validated/frozen. |
| | `startup_ogre_ready_ms` | Ogre root, resources and render system are configured. |
| | `startup_first_window_ms` | The native render window exists. |
| | `startup_first_usable_menu_ms` | The first usable interactive frame has rendered. The Q1 key remains stable while the current direct-to-world client is later replaced by K4's menu. |
| World entry | `entry_world_metadata_ms` | World metadata is loaded or a new version-3 identity is published. |
| | `entry_spawn_resident_ms` | The spawn preload and saved-actor restore finish. |
| | `entry_first_visible_terrain_ms` | Initial terrain is built and uploaded to Ogre. |
| | `entry_first_controllable_ms` | The first controllable frame finishes. |
| Save | `save_prepare_complete_ms` | Sum of transaction prepare intervals for dirty chunks and `world.meta`. |
| | `save_write_complete_ms` | Prior cumulative value plus candidate-write intervals. |
| | `save_flush_complete_ms` | Prior cumulative value plus durable-flush intervals. |
| | `save_validation_complete_ms` | Prior cumulative value plus real-loader validation intervals. |
| | `save_replace_complete_ms` | Prior cumulative value plus atomic-replacement intervals. |
| Backup | `backup_catalogue_scan_ms` | Live files and the existing backup catalogue are validated. |
| | `backup_candidate_copy_complete_ms` | All candidate files and the strict manifest are written. |
| | `backup_validation_complete_ms` | The complete candidate is re-read and fingerprinted. |
| | `backup_publish_complete_ms` | The backup directory is published and rotation completes. |
| Restore | `restore_catalogue_scan_ms` | The selected backup manifest and complete payload validate. |
| | `restore_candidate_copy_complete_ms` | A private restore candidate is fully copied. |
| | `restore_validation_complete_ms` | The candidate is re-read through real format readers. |
| | `restore_publish_complete_ms` | Live files publish successfully, including rollback handling when needed. |

`World::save()` begins before acquiring the world lock and completes only after
the K3 backup succeeds. Its five transaction phases describe synchronous K2
publication work; `save_total_ms` also includes lock ownership and the nested
backup. This deliberately exposes the complete main-thread delay rather than
hiding recovery cost outside the save measurement.

Catalogue enumeration has no sub-phase keys. It emits
`catalogue_total_ms`, `catalogue_main_thread_max_stall_ms` and
`catalogue_entries`.

## Outcome, stall and counters

Every completed kind emits `<prefix>_success`, `<prefix>_total_ms` and
`<prefix>_main_thread_max_stall_ms`. The stall is the longest interval between
observable main-thread boundaries. A synchronous save is one continuous
main-thread operation, so its stall may equal its total. Total is always at
least the last cumulative phase and the longest stall.

Save summaries also emit files, chunks and bytes written. Restore emits the Q1
bytes-read and bytes-written counters. Backup additionally emits files and byte
counters for diagnostics, and catalogue emits its accepted entry count.

On failure, the operation still completes with `success=0`. Any phase not
reached is filled with the failure total, preserving a complete monotonic
schema without claiming that the boundary succeeded. The latest completed
record of each kind is appended to `summary.txt`; fixed three-decimal output
does not change the stream's prior formatting state.

## Verification ownership

- `HelloMine3DOperationTimingSmoke` owns 12 collector/schema assertions,
  including disabled behavior, complete success/failure summaries, monotonic
  phases, aggregate save counters, the 32-record bound and overflow reporting.
- `HelloMine3DWorldCatalogueSmoke` owns real successful and rejected catalogue
  operations (30 total assertions).
- `HelloMine3DWorldBackupSmoke` owns real successful and failed backup/restore
  operations (19 total assertions).
- `HelloMine3DWorldRuntimeSmoke` runs an actual transactional world save with
  backup, validates its Q1 summary, disables collection, then proves another
  save still succeeds without retaining records (346 total assertions).
- The macOS native window probe enables performance capture and requires the
  real startup/world-entry phase, outcome, total and stall keys. All ordinary
  headless and validation-only runs keep capture disabled.

Q2 remains `Doing` until Q1 has approved real target-Windows Release budgets
and the native comparator rejects measured startup, entry, save and restore
regressions against those approved baselines. Synthetic comparator fixtures
are contract evidence, not product performance limits.
