# B3 Generic World Job Scheduler Contract v1

> Status: Frozen after B3 verification
>
> Runtime behavior: route the existing Chunk data and CPU-mesh work through one
> typed, deterministic and observable scheduler without adding concurrency
>
> Authoritative task status: `docs/current/todolist.md`

## 1. Purpose and approved scope

B3 replaces `ChunkRuntime::runLoader`'s private coordinate deque and implicit
work selection with a reusable `WorldJobScheduler`. The scheduler coordinates
the two background work families that the game executes today:

```text
ChunkLoadOrGenerate
ChunkMeshBuild
```

The first type deliberately reflects the current atomic compatibility path:
storage load falls back to deterministic generation inside `ChunkManager`.
Splitting IO and generation into separate asynchronous phases before that
boundary exists would create fictional architecture. FarLOD, lighting,
simulation and machine job slots are not pre-registered.

## 2. Frozen job vocabulary

```cpp
enum class WorldJobType {
    ChunkLoadOrGenerate,
    ChunkMeshBuild,
};

enum class WorldJobState {
    Pending,
    InFlight,
    Completed,
};

enum class WorldJobOutcome {
    DidWork,
    NoWork,
    CommitRejected,
};
```

Each `WorldJob` contains a monotonic non-zero id, type, demand target Chunk,
B2 priority, B2 demand epoch, B2 plan order, state and enqueue timestamp owned
by the scheduler. Timing is diagnostic derived state and is never serialized.

## 3. Ordering and uniqueness

Pending order is deterministic:

1. higher B2 priority;
2. lower B2 plan order, which preserves the already-frozen frustum, motion,
   age, distance and coordinate ordering;
3. newer demand epoch;
4. `ChunkLoadOrGenerate` before `ChunkMeshBuild` for an otherwise identical
   key;
5. lower monotonic job id.

At most one pending job exists for the same `(type, target)` key. Duplicate
submission is rejected without changing ids, counters or ordering. Repeating a
load/generate or mesh job after real progress creates a new job id while
retaining the target's B2 priority, epoch and plan order.

## 4. Lifecycle and queues

The only legal lifecycle is:

```text
Pending -> InFlight -> Completed
```

`takeNext` moves the highest-ranked pending job to the single in-flight slot.
`complete` accepts only the matching in-flight id, freezes outcome plus worker
and commit milliseconds, and appends one completion record. The loader drains
that completion before taking more work. Invalid or duplicate completion is
rejected without corrupting scheduler state.

When B2 demand/frustum/config revisions change, `replacePending` replaces only
not-yet-started jobs with the new plan. It does not invalidate or stop the
in-flight job. That existing pending-plan replacement is not B4 cancellation.

## 5. Runtime integration

- `ChunkRuntime` owns one `WorldJobScheduler` and still owns exactly one loader
  thread.
- A rebuilt B2 plan publishes one `ChunkLoadOrGenerate` job per target.
- A load/generate job calls a factored `ChunkManager` neighborhood-preparation
  step under the existing World mutex, loading at most one Chunk. It repeats
  until the 3x3 neighborhood is resident, then publishes `ChunkMeshBuild`.
- A mesh job calls the existing `beginMeshJob(..., maxChunkLoads=0)`, builds a
  copied `SectionMeshInput` off-lock and calls `finishMeshJob` under the World
  mutex. More dirty sections repeat the mesh job; a missing neighborhood
  returns to load/generate.
- B1 revision validation and lifecycle owners remain authoritative. A rejected
  `finishMeshJob` is reported as `CommitRejected`; B3 does not add a token or
  new validity rule.
- The scheduler does not own `Chunk`, mesh data, persistence or Gameplay truth.

## 6. Metrics and diagnostics

The copied debug snapshot and developer panel expose:

- current pending, in-flight and completed-result queue counts;
- cumulative submitted, started, completed, did-work, no-work and
  commit-rejected counts;
- cumulative completions by the two real job types;
- last queue latency, worker time and commit time in milliseconds.

Metrics are protected by the scheduler mutex, have no gameplay effect and are
not persisted or used by deterministic performance comparison.

## 7. Frozen budgets and explicit non-goals

B3 preserves one worker, the 6 ms pass budget, at most 64 job executions per
pass, at most one Chunk load per load/generate job, the two-section synchronous
main-thread rebuild budget and eight-Chunk unload budget.

B3 intentionally has no:

- cancellation token, generation token or in-flight invalidation (B4);
- pending hard cap, high/low watermark, admission result, shedding or
  backpressure (B5);
- Spatial Interest, activation or representation policy (B6);
- worker pool or runtime-configurable worker count;
- main-thread completion dispatcher or second authoritative mutation path;
- FarLOD, Far Terrain, lighting, simulation, actor, machine or network jobs;
- save/resource/settings/Gameplay/public-World-API change.

The B2 demand model bounds the current source set, but that is not presented as
B5 queue-pressure control.

## 8. Acceptance

B3 may be marked `Done` only when:

1. a focused static validator freezes the two types, three states, three
   outcomes, owner, actual loader integration, diagnostics and B4-B9 absence;
2. deterministic scheduler checks prove priority/plan ordering, duplicate
   rejection, exact state transitions, pending replacement, in-flight
   preservation, completion FIFO and truthful metrics/timings;
3. integration checks prove the real pipeline executes both job types, retains
   one worker and still rejects stale B1 mesh commits;
4. B2 demand ordering and exact four-slot model remain green;
5. the AL-A1 through B2 gates still pass;
6. `scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow`
   passes Debug/Release and the isolated 104-entry package;
7. roadmap, architecture, runtime validation, tutorial, task ledger and a B3
   report are synchronized before the independent local commit.

AI gameplay remains `NOT_RUN` unless separately executed through the active
package-only protocol. Human fun, aesthetics, comfort and physical input feel
remain `NOT_CLAIMED`.
