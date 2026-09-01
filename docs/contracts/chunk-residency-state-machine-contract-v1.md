# B1 Chunk Residency State Machine Contract v1

> Status: Frozen after B1 verification
>
> Runtime behavior: make the existing lifecycle explicit without adding new
> streaming demand, scheduling or budget policy
>
> Authoritative task status: `docs/current/todolist.md`

## 1. Purpose and approved scope

B1 replaces the current implicit/combined Chunk lifecycle with three
orthogonal state machines. It freezes vocabulary, legal transitions,
ownership, debug visibility and regression evidence for the behavior that the
current loader, mesh builder and Ogre synchronizer already perform.

The three state machines are independent dimensions. No product state may
encode a cross-product such as `LoadedCpuReadyGpuBufferedSaving`.

## 2. Frozen state vocabularies

### 2.1 Data residency

`Chunk` owns its authoritative data-residency state:

```text
Absent -> Requested -> Loading -> Generating -> Resident
                       Loading -------------> Resident
Resident -> Saving -> Resident
Resident -> EvictRequested -> Absent
EvictRequested -> Saving -> EvictRequested -> Absent
EvictRequested -> Resident
```

`Loading -> Resident` is the storage-load path. `Loading -> Generating ->
Resident` is the generation fallback. A failed eviction save returns to
`Resident` and preserves dirty authoritative data. A Chunk erased from the
manager must first reach `Absent`.

### 2.2 CPU mesh lifecycle

`ChunkSection` owns its derived CPU mesh state:

```text
Clean -> Dirty -> Queued -> Building -> CpuReady -> Clean
          ^                    |
          +--------------------+
```

An edit or reset may invalidate `Clean`, `Queued`, `Building` or `CpuReady`
back to `Dirty`. `Queued` is entered only when real synchronous/background
work accepts the section. `Building` brackets input capture/build. A stale
off-lock result cannot enter `CpuReady`.

### 2.3 Ogre render residency

Ogre owns the derived render state for each live section key:

```text
NotResident -> UploadPending -> GpuResident
UploadPending -> NotResident
GpuResident -> Stale -> UploadPending
UploadPending -> Stale
Stale -> NotResident
GpuResident -> NotResident
```

World/Chunk code may publish immutable CPU snapshots and acknowledge current
revisions, but it must not own Ogre object/GPU residency. Missing live sections
destroy their Ogre objects and become `NotResident`. A newer CPU revision marks
an existing GPU visual `Stale` before replacement.

## 3. Transition enforcement

- Each vocabulary has one exhaustive `canTransition` function used by its
  mutation boundary.
- Mutation helpers assert on an illegal transition and leave state unchanged
  in builds where execution continues.
- Automated tests exhaustively cover legal and representative illegal edges
  without deliberately terminating the Debug process.
- State-to-string names are stable diagnostics and must match this contract.

## 4. Ownership and compatibility

- `ChunkManager` remains authoritative Chunk/storage ownership and coordinates
  data transitions around the existing storage/generation/save calls.
- `ChunkRuntime` continues to own the existing queue, worker and copied mesh
  publication from AL-A2. B1 does not add a second queue or worker.
- `ChunkSection` owns CPU mesh state and block revision. Mesh data remains
  derived and discardable.
- Ogre owns render records and object lifetime. `WorldDebugStats` may carry
  copied counts but not mutate renderer state.
- `World` keeps the frozen 78-method public surface. Save format v12, terrain
  v4, settings v8, resources, recipes, events and Gameplay remain compatible.

## 5. Debug visibility

The existing developer debug panel must expose counts for every represented
data, mesh and render state. `Absent`/`NotResident` are semantic states rather
than retained objects, so the panel may report transition/tracker counts and
must not invent an unbounded universe of absent coordinates.

Existing aggregate names may remain as compatibility aliases only when their
meaning stays exact. New B1 diagnostics use the frozen names above.

## 6. Safety invariants

1. Dirty authoritative Chunk data is saved before eviction.
2. A failed save cancels eviction and leaves the Chunk resident and dirty.
3. A stale off-lock mesh result is rejected by block revision.
4. Ogre never promotes an upload acknowledgement whose section/revision is no
   longer current.
5. Removing a live section destroys its render objects and state record.
6. The three state dimensions remain separately countable; no combined enum or
   speculative state slot is introduced.

## 7. Explicit non-goals

B1 does not:

- introduce B2 multi-source Streaming Demand or interest aggregation;
- introduce B3 Job Scheduler/priority classes, B4 cancellation tokens, B5
  backpressure, B6 spatial activation, or B7-B9 Extended capabilities;
- change the existing 6 ms/64-target loader pass, two-section synchronous
  rebuild budget, eight-Chunk unload budget or one-loader-thread limit;
- change generation, persistence format, resource identity, rendering output
  or gameplay balance;
- add persistence for derived mesh/render states;
- claim AI/Computer Use or human subjective acceptance.

## 8. Acceptance

B1 may be marked `Done` only when:

1. a focused static boundary validator proves the three exact vocabularies,
   separate owners, transition guards and absence of B2-B9 symbols;
2. focused World runtime assertions prove the transition matrices, normal
   storage/generation lifecycle, dirty save-before-evict, failed-save rollback,
   queued/building/CPU-ready flow and stale mesh rejection;
3. an Ogre validation path proves render transitions and stale/current revision
   behavior without a focus-stealing real window;
4. the developer debug panel exposes the three state dimensions;
5. the AL-A1 public surface and AL-A2 ownership gates still pass;
6. `scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow`
   passes Debug/Release and the isolated 104-entry package;
7. roadmap, architecture, runtime validation, tutorial, task ledger and a B1
   report are synchronized before the independent local commit.

AI gameplay remains `NOT_RUN` unless separately executed through the active
package-only protocol. Human fun, aesthetics, comfort and physical input feel
remain `NOT_CLAIMED`.
