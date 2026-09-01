# AL-A2 Chunk Runtime Boundary Contract v1

> Status: Frozen after AL-A2 verification
>
> Runtime behavior: preserve existing semantics
>
> Authoritative task status: `docs/current/todolist.md`

## 1. Purpose and approved scope

AL-A2 extracts the already-shipping Chunk runtime coordination from `World`
into one owned `ChunkRuntime`. The batch may move only:

- the deduplicated FIFO Chunk section update queue;
- synchronous bounded mesh rebuild processing;
- background mesh-work planning and loader thread coordination;
- load-center and mesh-priority publication;
- existing preload, render-distance invalidation and distant-unload coordination;
- renderer-facing CPU mesh snapshot collection and revision acknowledgement.

`World` keeps its frozen public surface and delegates existing Streaming
commands and queries. No existing caller is required to migrate.

## 2. Explicit non-goals

AL-A2 must not:

- introduce `ChunkResidency` or any new lifecycle/state-machine vocabulary;
- change Chunk load, save, unload, generation or storage transitions;
- change save format v12 or persisted data;
- change mesh selection, priority ordering, dirty propagation or per-update
  rebuild budgets;
- add a second scheduler, job abstraction, cancellation model or backpressure;
- change Gameplay, renderer ownership, resources or package contents beyond the
  new compiled implementation files;
- begin AL-A3 or B1.

## 3. Ownership and locking

`World` owns `ChunkManager`, the existing shared world mutex, and one
`ChunkRuntime`. `ChunkRuntime` holds non-owning references to the manager and
mutex, and owns the moved queue, worker thread and loader-priority bookkeeping.

The shared mutex remains the single guard for loaded Chunk/section state.
Methods documented as `Locked` require the caller to already hold that mutex;
all other public runtime methods acquire it exactly where the previous `World`
implementation did. Moving code must not widen the expensive mesh-build lock.

## 4. Authoritative and derived data

Block data, light data, Chunk presence, save-dirty flags and stored world data
remain authoritative in `Chunk` / `ChunkManager` / storage. Meshes, mesh work
orders, priority snapshots and queued rebuild keys remain derived data. A queue
entry may be dropped when its Chunk/section is no longer resident; it must not
create authoritative data.

A block or lighting edit still uses `ChunkUpdatePlanner`, invalidates only the
necessary loaded sections, deduplicates identical section keys, and drains in
FIFO order with `World::ChunkMeshRebuildBudgetPerUpdate == 2`.

## 5. Worker commit protocol

The existing worker protocol is frozen as:

```text
Snapshot under world lock
  -> build SectionMeshInput off-lock
  -> validate block revision under world lock
  -> commit, or reject stale output
```

Concretely, `ChunkManager::beginMeshJob` and `finishMeshJob` remain the two
locked boundaries. Work planning remains complete and stable: frustum
intersection sorts first, then squared distance, Manhattan distance, X and Z;
targets are prioritized, never filtered.

## 6. Thread lifecycle and budgets

- At most one Chunk loader worker is started by the existing public command.
- `World` stops and joins it before the final dirty-Chunk/world save.
- A worker pass keeps the existing 6 ms / 64 target bounds, one Chunk load per
  target, and 1 ms active / 10 ms idle sleeps.
- Distant unload remains capped at eight Chunks per `World::update` call.
- Render-distance changes only invalidate current work and unload scans as
  before.

## 7. Compatibility and acceptance

The following evidence is required before this contract can be frozen after
verification:

1. the AL-A1 public-surface validator reports the same 78 methods and hash;
2. focused World runtime regressions prove minimal dirty propagation, FIFO
   deduplication, the two-section rebuild budget, stale mesh rejection,
   frustum priority and save-before-unload behavior;
3. VS2017/v141 Debug and Release complete gates pass, including the clean
   Release package;
4. documentation names `ChunkRuntime` as coordination/derived-data ownership,
   while `ChunkManager` remains authoritative Chunk/storage ownership;
5. no `ChunkResidency`, speculative state slot or AL-A3 implementation appears
   in the diff.

AI gameplay scenarios remain `NOT_RUN` unless separately executed through the
active package-only acceptance protocol. Human fun, aesthetics and physical
input feel remain `NOT_CLAIMED`.
