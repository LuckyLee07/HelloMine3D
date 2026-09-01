# HelloMine3D Architecture Lab Tutorial

> Living tutorial status: Part 00 covers the completed AL-A0 baseline and AL-A1
> responsibility map; Part 01 covers the completed AL-A2 Chunk runtime boundary.
> Later Parts are added only after their
> implementation batch is approved and verified; this file does not pre-create
> empty Sprint chapters.

## Part 00 — From a playable clone to an architecture lab

HelloMine3D already has a complete playable path: create a world, gather,
craft, upgrade tools, smelt, recover, fight, explore, reach the Waystone ending,
continue after victory, save and reopen. That game is the source of architecture
problems, not decoration around an engine exercise.

AL-A0 first froze the real module, ownership, thread, persistence, tick,
snapshot, performance and validation boundaries. AL-A1 then addresses the
first visible pressure point without pretending that a diagram has already
refactored the program.

### 0.1 Why does a God Object arise naturally?

#### Problem

The earliest useful `World` answers a simple question: where should code go when
it changes the world? As real play is added, the same answer remains locally
convenient for blocks, lighting, random ticks, natural mobs, combat,
projectiles, difficulty, Waystones, saving, background loading, mesh queues and
events. The result is not one obviously bad decision; it is many reasonable
features accumulating behind one trusted facade.

The current header exposes 78 unique public method names across nine primary
responsibilities. Callers also operate at different semantic levels: some ask
questions, some submit commands, while two entries advance runtime work.

#### Naive Solution

The tempting response is either to keep adding one `World` method per feature,
or to immediately split the file into services and move code until it looks
modular. The first choice hides coupling. The second changes ownership and call
order before the existing behavior has been described, making any regression
hard to attribute.

#### Failure

A large facade becomes dangerous when method names stop revealing semantics:

- a query may unexpectedly mutate authoritative state;
- a renderer-facing caller may obtain a mutable manager;
- a subsystem may invent a second tick path;
- saving, events or mesh invalidation may be lost during a mechanical move;
- a new abstraction may be designed for future systems that do not yet exist.

The failure is not file length by itself. It is the inability to review whether
a new entry preserves ownership, mutation and time-flow rules.

#### Design Evolution

AL-A1 takes a smaller step. Every existing public method receives two orthogonal
labels:

```text
API concept:     Query | Command | Runtime Tick
Responsibility:  World Query | World Mutation | Simulation | Streaming |
                 Persistence | Actor | Combat | Progression | Diagnostics
```

The complete map stays beside the current architecture, where it can be checked
against the real header. Existing callers do not migrate. Mutable manager/player
accessors remain visible as compatibility escape hatches and are explicitly
prohibited as patterns for new APIs.

This is an admission gate rather than a final class design. A future wrapper is
allowed only when a separately approved batch has a real caller and ownership
problem to solve.

#### Implementation

`docs/current/architecture.md` contains one row per unique method and a
normalized public-surface hash. The validator extracts the public section of
`src/HelloMine3D/World/World.h`, checks the hash and compares both method sets.
It then runs at the start of `scripts/verify_build.ps1`.

The v1 map contains 45 Queries, 31 Commands and 2 Runtime Ticks. Overloads share
a row, while the hash still detects signature, overload, constant and nested
declaration changes.

#### Validation

Run the focused gate while editing the boundary:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  tools\validate_world_responsibility_map.ps1
```

A PASS proves that every current public method is classified and that the
reviewed header identity has not drifted. The VS2017 full gate proves that this
new check composes with the established build, test and clean-package path. It
does not prove that the God Object has been decomposed, nor does it replace OS
Computer Use or human subjective evidence.

#### Trade-offs

The map adds maintenance whenever the public surface changes. That friction is
intentional: it makes growth visible while remaining much cheaper than a broad
refactor. A hash can be updated mechanically, so the contract also requires a
real-need explanation and boundary note; automation cannot judge architecture
quality on its own.

The design accepts temporary inconsistency: legacy escape hatches and a broad
facade remain. In return, AL-A2 can later move one proven Chunk runtime boundary
against a stable, reviewable baseline instead of guessing what `World` meant.

Related evidence:

- `docs/current/architecture.md`
- `docs/contracts/world-responsibility-map-contract-v1.md`
- `docs/reports/architecture-lab-baseline-v1.md`
- `tools/validate_world_responsibility_map.ps1`

## Part 01 — Chunk is not just an array

### 1.1 Problem Scenario

A playable voxel world does more than index blocks. A camera move publishes
streaming demand; edits invalidate exactly the affected sections; a bounded
foreground queue repairs immediately visible derived meshes; a worker loads
neighbours and builds more meshes; the renderer copies CPU-ready output and
acknowledges only the revision it uploaded; distant Chunks save before leaving
memory.

Before AL-A2, all of that coordination lived directly in `World`. The data
owner was already `ChunkManager`, but `World` also owned the update deque/set,
worker threads, priority snapshot, load-center revisions and unload scan. The
public facade therefore hid two different roles: authoritative world
composition and derived Chunk work scheduling.

### 1.2 Naive Solution

One tempting split is to introduce a new `ChunkResidency` state machine and a
generic job system while moving the code. Another is to give `ChunkRuntime` its
own mutex and copy selected manager state into it. Both produce an attractive
diagram quickly.

### 1.3 Why that fails

Those changes would alter more than ownership:

- a second mutex changes lock order and can race with save, light propagation
  or renderer snapshot collection;
- a new residency vocabulary invents transitions that the current game has not
  yet required;
- a generic scheduler mixes A2 refactoring with B3 cancellation/backpressure
  work;
- copying Chunk state creates a second source of truth;
- changing the queue or worker budget can improve throughput while silently
  worsening foreground frame time.

The safest boundary is therefore the smallest one already proved by gameplay.

### 1.4 Design Evolution

AL-A2 forms this ownership chain:

```text
World
  owns shared world mutex
  owns ChunkManager (authoritative Chunk and storage state)
  owns ChunkRuntime (derived work and coordination)

ChunkRuntime
  references ChunkManager + shared mutex
  owns FIFO/dedup update queue
  owns one loader worker and priority revisions
  coordinates preload, distant unload and mesh publication
```

`World` keeps the same 78 public methods. Existing callers still ask the World
facade to update, preload, set render distance, collect meshes and acknowledge
uploads; those methods now delegate. This preserves compatibility while making
the internal responsibility independently reviewable.

### 1.5 Derived data versus authoritative data

Block ids, metadata, light values, Chunk presence, save-dirty flags and world
metadata remain authoritative. Meshes, work orders, queue keys, camera priority
and renderer upload snapshots are derived: they may be rebuilt or discarded,
and they must never create a missing Chunk merely because work was queued.

The worker protocol makes that distinction executable:

```text
beginMeshJob under shared lock
  -> capture SectionMeshInput + block revision
  -> ChunkMeshBuilder runs off-lock on the value snapshot
  -> finishMeshJob under shared lock
  -> commit only if the revision is still current
```

This is the reusable pattern `Snapshot -> Worker -> Revision Validation ->
Commit`. It is useful because it names the safety property without pretending
that the project already has a universal job scheduler.

### 1.6 Implementation and Validation

`src/HelloMine3D/World/Chunk/ChunkRuntime.*` now contains the moved queue,
planner, worker and mesh-publication code. The original constants remain: two
synchronous section rebuilds per update, eight unloads per scan, a 6 ms/64
target worker pass, one load per target, and 1 ms active / 10 ms idle sleeps.

The focused architecture gate checks that `World` no longer owns the legacy
queue/worker fields, that the budgets stay present, and that
`beginMeshJob -> buildMesh -> finishMeshJob` remains ordered:

```powershell
& .\tools\validate_chunk_runtime_boundary.ps1 -Root (Get-Location).Path
```

The real World smoke then proves behavior rather than spelling: S0.5 covers
minimal section invalidation, M2 covers deduplication/FIFO/two-section budget,
M6/M7 cover mesh work and priority, E5 rejects stale upload acknowledgement,
and S2.4 proves save-before-unload round-trip.

### 1.7 Trade-offs

The shared mutex remains broad, and `ChunkManager` still calls back into
`World` for light/event coordination. Those are recorded constraints, not
hidden successes. `ChunkRuntime` also remains concrete instead of introducing
interfaces for hypothetical schedulers. B1 may later model orthogonal data,
mesh and render residency; B3-B5 may add jobs, cancellation and backpressure,
but only after separate approval and new observable pressure.

Related evidence:

- `docs/contracts/chunk-runtime-boundary-contract-v1.md`
- `docs/reports/architecture-lab-a2-chunk-runtime-report-v1.md`
- `tools/validate_chunk_runtime_boundary.ps1`
