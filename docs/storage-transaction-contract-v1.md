# Storage Transaction Contract v1

This contract freezes K2 publication semantics for `world.meta` and
`chunk_<x>_<z>.hmcchunk`. It protects the last validated generation of each
file. Rotating backups, verified restore and world-management commands remain
K3/K4 work.

## Published, pending and quarantined files

For a published target `T`, the transaction owns exactly two sibling names:

- `T.pending` is the same-volume candidate. It is never used for normal load.
- `T.failed` is the single bounded quarantine slot. A newer failed candidate
  replaces the older slot; quarantine cannot grow without bound.

Publication stays synchronous and permits one writer per target. A stale
`T.pending` from an interrupted process is quarantined before a new candidate
is opened. The published target is never truncated in place.

## Publication sequence

`StorageTransaction::publish` performs these steps in order:

1. Open the sibling pending candidate.
2. Write the complete serialized payload.
3. Flush the C stream and durably flush its file descriptor (`fsync` on POSIX,
   `_commit` on Windows).
4. Close the candidate.
5. Parse and validate the candidate through the real format loader.
6. Atomically replace the published path (`rename` on POSIX,
   `MoveFileExW` with replace/write-through on Windows).
7. Best-effort flush the containing directory on POSIX after replacement.

The target changes only at step 6. Any earlier failure closes the candidate,
moves it to `T.failed`, returns `false` and leaves the previous target bytes
untouched. A replacement failure follows the same rule. Save callers retain
dirty chunks after failure, and a chunk that cannot be saved is not unloaded.

## Candidate validation

World saves always publish format 3. Validation uses the same `WorldSave`
parser as normal loading and requires the full version-3 field set, exact
inventory/actor counts, valid K1 identity and timestamps, bounded metadata,
finite numeric state and a complete end-of-file parse. Normal loading retains
version 1/2 compatibility and normal saving upgrades those generations to
version 3.

Chunk validation uses the same binary reader as normal loading. It checks the
magic, supported version (1 or 2), expected coordinates, chunk size, section
limit, exact block and metadata payload, bounded and valid block entities, and
end-of-file with no trailing bytes. Successful saves remain chunk format 2;
the existing format-1 compatibility fixture remains readable.

## Fault injection and diagnostics

`StorageFaultPoint` exposes five deterministic test-only boundaries:

- `BeforeWrite`
- `MidWrite`
- `BeforeFlush`
- `BeforeValidation`
- `BeforeReplace`

`StorageTransactionMetrics` reports elapsed milliseconds, bytes written,
flush/validation/publication completion, quarantine path and the exact error.
Successful world and chunk transactions are aggregated into
`ChunkDebugStats::saveTransactions`, `saveTotalMs` and `saveMaxMs`. The debug
panel and performance CSV/summary expose the same total and maximum duration;
Q2 owns phase-specific budgets and main-thread-stall accounting.

## Acceptance evidence

`HelloMine3DStorageTransactionSmoke` owns 16 focused assertions. For both
world and chunk data, every injected boundary proves that the operation fails,
the previous complete generation reloads unchanged, no pending file remains,
and the incomplete candidate is quarantined. Additional fixtures cover
validator rejection, stale-pending quarantine, successful durable publication
and non-zero duration/byte metrics.

The full gate also runs `HelloMine3DSaveLoadSmoke` for version-1 chunk
compatibility and `HelloMine3DWorldRuntimeSmoke` for version-1 world upgrade,
complete player/inventory/actor persistence and aggregated save timing.
