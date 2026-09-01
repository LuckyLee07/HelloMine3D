# HelloMine3D Architecture Lab Tutorial

> Living tutorial status: Part 00 covers the completed AL-A0 baseline and AL-A1
> responsibility map; Part 01 covers the completed AL-A2 Chunk runtime boundary;
> Part 02 covers the completed AL-A3 Simulation runtime boundary; Part 03
> covers the completed AL-A4 Event / Command / Query boundary.
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

## Part 02 — Fixed Tick is the simulation time spine

### 2.1 Problem Scenario

The playable game already advances many kinds of authoritative state at 20 Hz:
pending difficulty, player and mob actors, combat projectiles, the Waystone
encounter, random block ticks, population, furnaces, objectives and respawn.
Before AL-A3, their order was encoded as one long body in `World::tick`. The
order was real gameplay behavior, but it had no named observation boundary.

That makes two ordinary questions unnecessarily hard: which part of a tick is
expensive, and where may a new simulation feature join without silently moving
combat before actors or population before random ticks?

### 2.2 Naive Solution

A generic scheduler can appear to solve both problems at once: define an
`ISandboxSystem`, register every system, attach priorities and budgets, then let
the scheduler decide what runs. It also looks attractive to move all gameplay
state into the new runtime while touching the call order.

### 2.3 Why that fails

The current game has not demonstrated a shared scheduling problem across three
independent systems. Introducing interfaces, registries, budgets and deferred
work now would encode hypothetical needs. At the same time, moving ownership or
reordering calls would change cooldown, attack, furnace, objective and respawn
semantics under the cover of a refactor. A second pause flag would create two
sources of truth for whether simulation may advance.

### 2.4 Design Evolution

AL-A3 introduces one concrete coordinator and keeps the compatibility chain:

```text
GameApplicationFlow pause gate
  -> SandboxRuntime / FixedTickScheduler
  -> WorldManager::tick
  -> World::tick(int)
  -> WorldSimulation::fixedTick(WorldTickContext)
```

`World` owns `WorldSimulation`; the runtime only keeps a non-owning `World&`.
Existing state and phase implementations stay where they are. This is an
orchestration boundary, not an ownership transfer.

### 2.5 Tick Context, phases and raw timing

The context contains only the caller tick and the existing `1/20` second delta.
The frozen order is:

```text
TickPreparation -> ActorSimulation -> Combat -> Encounter
  -> BlockRandomTick -> Population -> BlockEntitySimulation
  -> GameplayRuntime
```

Every completed tick replaces one copied `WorldSimulationSnapshot`: completed
count, tick, delta, whole-tick milliseconds and one `steady_clock` sample per
phase. The values are developer observations. They are not saved, replayed or
used to change behavior, and they do not yet define averages, budgets,
overruns, priorities or deferred work.

### 2.6 Implementation and Validation

`src/HelloMine3D/World/Simulation/WorldSimulation.*` contains the coordinator.
`World::collectDebugStats` copies its last snapshot, and the Ogre developer
panel prints the whole tick plus all phase rows. The focused static gate is:

```powershell
& .\tools\validate_world_simulation_boundary.ps1 -Root (Get-Location).Path
```

The `AL-A3` WorldRuntime focus checks phase identity/order, context propagation,
finite non-negative samples, caller-owned pause and exact single-tick resume.
The full smoke keeps deterministic random ticks, Actor/combat/population,
furnace, progression, save and replay-sensitive behavior under the same gate.

### 2.7 Trade-offs

`WorldSimulation` currently uses friendship to call private legacy
implementations. That coupling is explicit and narrow, but it is not the final
module boundary. Raw timers add observation overhead and one last-tick sample
cannot support performance policy. Both limitations are preferable to
inventing A5 before evidence exists. A4 may later clarify command/query/event
semantics, and A5 may define metrics and budgets, but each requires separate
approval and a frozen contract.

Related evidence:

- `docs/contracts/world-simulation-boundary-contract-v1.md`
- `docs/reports/architecture-lab-a3-world-simulation-report-v1.md`
- `tools/validate_world_simulation_boundary.ps1`

## Part 03 — An event system is not a global broadcast

### 3.1 Problem Scenario

The game already had two mechanisms called “event”. Input queued a
`PlayerDigEvent` for `World::update`, even though Break, Use and Place were
requests that might be rejected. Separately, `SandboxEventBus` synchronously
published facts such as `BlockBreakEvent`, `EntityDeathEvent` and
`CraftCompletedEvent` after outcomes occurred.

That naming collision hides causality. It also leaves a harder problem: event
handlers can modify objectives or Waystone state and can indirectly publish
more events. With no declared effect or recursion rule, a convenient global
broadcast can become an invisible command graph.

### 3.2 Naive Solution

One response is to make every operation an event and let subscribers decide
what happens. Another is to ban every mutation and nested publication from
handlers. The first removes any visible authority for state change. The second
cannot express the real Waystone flow, where handling a guardian death may
spawn the next wave and therefore publish actor facts.

### 3.3 Why that fails

An unbounded broadcast graph has several concrete failure modes:

- an observer can publish another fact and silently become a command;
- `Event -> mutation -> Event` can recurse until stack exhaustion;
- unsubscribing during vector iteration can invalidate the active traversal;
- diagnostic telemetry can accidentally advance Gameplay;
- Audio, UI, future Network and Machine code can become coupled through a bus
  whose handlers have no documented effects.

Conversely, a blanket prohibition would move legitimate domain reactions back
into `World` and erase the architectural boundary the game actually needs.

### 3.4 Design Evolution

AL-A4 separates the vocabulary first:

```text
Player input -> IWorldCommand FIFO -> authoritative mutation
             -> immutable Domain Event -> declared subscribers

Query -> copied/read-only observation; no command or domain publication
Diagnostic Event -> ObserveOnly subscribers only
```

`PlayerBlockInteractionCommand` replaces the misleading Dig Event name. The
World FIFO remains ordered and frame-owned, so this is a semantic correction,
not a second scheduler.

Each subscription declares an owner, `ObserveOnly` or `DomainMutation`, and
whether nested publication is `Forbidden` or `Bounded`. Objective progression
is a bounded mutation that cannot republish. Waystone guardian death is a
bounded mutation that may republish because spawning the next wave emits actor
facts. Feedback and Audio are observers.

### 3.5 Dispatch boundary and data structure

The bus remains synchronous and World-local. A publication copies the matching
subscription membership before invoking handlers. Subscription changes inside
a handler therefore affect later or nested publications, never the active
membership snapshot.

Nested publication is accepted only from a handler that explicitly declares
`Bounded`, with eight active dispatch frames as a hard cap. Observer republish
and depth overflow return explicit rejection results and increment a debug
snapshot; they are not converted into a hidden queue. RAII guards restore depth
and active policy even when a handler throws.

Events carry immutable type/category identity and are delivered as const facts.
A `Diagnostic` event skips and counts every `DomainMutation` subscription, so
telemetry cannot drive authoritative state through this bus.

### 3.6 Validation

The static gate checks that only the command vocabulary remains, every
production subscriber declares its effect owner, recursion/snapshot guards are
present, and future Network/Machine directories obey renderer/UI dependency
rules when they appear:

```powershell
& .\tools\validate_event_command_query_boundary.ps1 `
  -Root (Get-Location).Path
```

The `AL-A4` WorldRuntime focus exercises normal observer delivery, forbidden
and allowed nested publication, the depth cap, diagnostic isolation,
unsubscribe-during-dispatch snapshot semantics and exception cleanup. It also
runs the existing interaction and objective cases, because architecture only
has value while Break, Place, Use and progression still work.

### 3.7 Trade-offs

Copying a small subscriber list on publish spends bounded allocation/copy work
to obtain simple, deterministic membership semantics. The declarations cannot
prove that arbitrary C++ in an `ObserveOnly` handler never mutates external
state; the static inventory and code review remain part of admission. The bus
also does not yet queue work, persist events, provide replay, or define a
generic Machine/Network protocol.

Those omissions are deliberate. A4 solves the real command/fact ambiguity and
recursive dispatch risk without starting A5 metrics, B-series jobs or C-series
content ahead of approval.

Related evidence:

- `docs/contracts/event-command-query-boundary-contract-v1.md`
- `tools/validate_event_command_query_boundary.ps1`
