# HelloMine3D Architecture Lab Tutorial

> Living tutorial status: Part 00 covers completed Track A and Part 01 begins
> Track B with verified B1. Later Parts are added only with the first verified
> batch of their owning Track; this file does not pre-create empty Sprint chapters.

<!-- ARCHITECTURE-LAB-TUTORIAL-MANIFEST-BEGIN -->
AL-A0|00|0.1|docs/reports/architecture-lab-baseline-v1.md
AL-A1|00|0.1|docs/contracts/world-responsibility-map-contract-v1.md
AL-A2|00|0.2|docs/reports/architecture-lab-a2-chunk-runtime-report-v1.md
AL-A3|00|0.3|docs/reports/architecture-lab-a3-world-simulation-report-v1.md
AL-A4|00|0.4|docs/reports/architecture-lab-a4-event-command-query-report-v1.md
AL-A5|00|0.5|docs/reports/architecture-lab-a5-simulation-metrics-report-v1.md
AL-A6|00|0.6|docs/contracts/architecture-lab-documentation-pipeline-contract-v1.md
B1|01|1.1|docs/reports/architecture-lab-b1-chunk-residency-report-v1.md
<!-- ARCHITECTURE-LAB-TUTORIAL-MANIFEST-END -->

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

### 0.2 Chunk is not just an array

#### Problem

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

#### Naive Solution

One tempting split is to introduce a new `ChunkResidency` state machine and a
generic job system while moving the code. Another is to give `ChunkRuntime` its
own mutex and copy selected manager state into it. Both produce an attractive
diagram quickly.

#### Failure

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

#### Design Evolution

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

##### Data structure: derived data versus authoritative data

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

#### Implementation

`src/HelloMine3D/World/Chunk/ChunkRuntime.*` now contains the moved queue,
planner, worker and mesh-publication code. The original constants remain: two
synchronous section rebuilds per update, eight unloads per scan, a 6 ms/64
target worker pass, one load per target, and 1 ms active / 10 ms idle sleeps.

#### Validation

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

#### Trade-offs

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

### 0.3 Fixed Tick is the simulation time spine

#### Problem

The playable game already advances many kinds of authoritative state at 20 Hz:
pending difficulty, player and mob actors, combat projectiles, the Waystone
encounter, random block ticks, population, furnaces, objectives and respawn.
Before AL-A3, their order was encoded as one long body in `World::tick`. The
order was real gameplay behavior, but it had no named observation boundary.

That makes two ordinary questions unnecessarily hard: which part of a tick is
expensive, and where may a new simulation feature join without silently moving
combat before actors or population before random ticks?

#### Naive Solution

A generic scheduler can appear to solve both problems at once: define an
`ISandboxSystem`, register every system, attach priorities and budgets, then let
the scheduler decide what runs. It also looks attractive to move all gameplay
state into the new runtime while touching the call order.

#### Failure

The current game has not demonstrated a shared scheduling problem across three
independent systems. Introducing interfaces, registries, budgets and deferred
work now would encode hypothetical needs. At the same time, moving ownership or
reordering calls would change cooldown, attack, furnace, objective and respawn
semantics under the cover of a refactor. A second pause flag would create two
sources of truth for whether simulation may advance.

#### Design Evolution

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

##### Data structure: Tick Context, phases and raw timing

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

#### Implementation

`src/HelloMine3D/World/Simulation/WorldSimulation.*` contains the coordinator.
`World::collectDebugStats` copies its last snapshot, and the Ogre developer
panel prints the whole tick plus all phase rows.

#### Validation

The focused static gate is:

```powershell
& .\tools\validate_world_simulation_boundary.ps1 -Root (Get-Location).Path
```

The `AL-A3` WorldRuntime focus checks phase identity/order, context propagation,
finite non-negative samples, caller-owned pause and exact single-tick resume.
The full smoke keeps deterministic random ticks, Actor/combat/population,
furnace, progression, save and replay-sensitive behavior under the same gate.

#### Trade-offs

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

### 0.4 An event system is not a global broadcast

#### Problem

The game already had two mechanisms called “event”. Input queued a
`PlayerDigEvent` for `World::update`, even though Break, Use and Place were
requests that might be rejected. Separately, `SandboxEventBus` synchronously
published facts such as `BlockBreakEvent`, `EntityDeathEvent` and
`CraftCompletedEvent` after outcomes occurred.

That naming collision hides causality. It also leaves a harder problem: event
handlers can modify objectives or Waystone state and can indirectly publish
more events. With no declared effect or recursion rule, a convenient global
broadcast can become an invisible command graph.

#### Naive Solution

One response is to make every operation an event and let subscribers decide
what happens. Another is to ban every mutation and nested publication from
handlers. The first removes any visible authority for state change. The second
cannot express the real Waystone flow, where handling a guardian death may
spawn the next wave and therefore publish actor facts.

#### Failure

An unbounded broadcast graph has several concrete failure modes:

- an observer can publish another fact and silently become a command;
- `Event -> mutation -> Event` can recurse until stack exhaustion;
- unsubscribing during vector iteration can invalidate the active traversal;
- diagnostic telemetry can accidentally advance Gameplay;
- Audio, UI, future Network and Machine code can become coupled through a bus
  whose handlers have no documented effects.

Conversely, a blanket prohibition would move legitimate domain reactions back
into `World` and erase the architectural boundary the game actually needs.

#### Design Evolution

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

#### Implementation

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

#### Validation

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

#### Trade-offs

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

### 0.5 Timing alone does not explain bounded simulation work

#### Problem

AL-A3 made all eight fixed-tick phases visible, but each row only answered
“how long did the last invocation take?” A `0.20 ms` random-tick phase could
mean one section was processed, four sections reached the hard limit, or no
eligible work existed. Timing alone cannot distinguish healthy cheap work from
a queue whose admission limit is intentionally deferring items.

The game already had real limits: 32 projectile steps per tick, four random-
tick sections per tick, and a difficulty-dependent number of natural-spawn
attempts per population cycle. Actor ticking had no admission limit. The
architecture gap was vocabulary and observation, not the absence of a generic
scheduler.

#### Naive Solution

A tempting response is to introduce `ISandboxSystem`, register every phase,
assign every row a time budget and let one scheduler own priorities. Another
shortcut is to append `processed=0, deferred=0` to all eight rows and promise
to define them later.

#### Failure

Both approaches manufacture architecture ahead of need. The generic scheduler
would own behavior that the game has not asked to change. Empty rows would make
“no metric contract exists” indistinguishable from “the system processed zero
items”, and a millisecond threshold would turn one noisy last-tick sample into
a false performance conclusion.

#### Design Evolution

AL-A5 keeps the eight AL-A3 timing rows and adds exactly four metric rows:

```text
Actor Simulation    elapsed + processed; unbudgeted
Combat             elapsed + processed + deferred; 32 per tick
Block Random Tick  elapsed + processed + deferred; 4 per tick
Population         elapsed + processed; difficulty limit per cycle
```

`processed` means the phase actually invoked the defined work unit.
`deferred` means an eligible retained item remains for later specifically
because the existing hard admission limit stopped work. It is not failure,
drop count, unused capacity or elapsed time.

#### Implementation

##### Data structure and status vocabulary

```cpp
struct SimulationPhaseMetrics {
    WorldSimulationPhase phase;
    double elapsedMilliseconds;
    std::size_t processed;
    std::size_t deferred;
    std::size_t budget;
    SimulationPhaseBudgetScope budgetScope;
};
```

The four scopes/status terms are deliberately small. A phase is unbudgeted, is
within its item limit, reaches the limit exactly, or leaves real work deferred.
Budgeted phases must never report `processed > budget`. Population uses
`PerPopulationCycle`, so a non-cycle tick publishes zero instead of repeating
the previous cycle. No implicit accumulation is hidden in the snapshot.

##### Runtime flow and ownership

```text
WorldSimulation::fixedTick
  -> constructs a fresh last-tick snapshot
  -> runs the unchanged AL-A3 phase sequence
  -> existing phase code exposes actual work counts
  -> the same RawPhaseTimer writes timing and matching metric elapsed
  -> WorldDebugStats copies the completed snapshot
  -> developer Simulation panel formats scope and derived status
```

ActorManager returns the number of live actors it actually invoked without
changing its loop. Combat reads the already-reset projectile used/denied
counters. Random Tick compares processed active sections with the remaining
active rotation. Population takes a delta of the existing cumulative attempt
counter and uses the current difficulty limit. None of these counters decides
what runs.

#### Validation

The A5 static gate freezes the four identities, three scopes, four statuses,
existing hard-limit constants, UI wiring and persistence exclusion:

```powershell
& .\tools\validate_simulation_metrics_boundary.ps1 `
  -Root (Get-Location).Path
```

`HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A5` checks the A3 phase contract plus exact
metric identities, total status mapping, shared elapsed values, real Actor,
Combat and Random Tick limits, and a real Population cycle followed by a
non-cycle reset. The complete WorldRuntime suite remains the proof that
instrumentation did not change gameplay.

#### Trade-offs

This is a last-tick diagnostic view, so it intentionally cannot answer P95,
trend or capacity-planning questions. The Population phase has a per-cycle
limit but no retained queue; therefore its deferred count is correctly zero.
Actor work is unbudgeted, which is an honest observation rather than a missing
constant disguised as zero.

If repeated evidence later shows scheduling pressure, D1 may change runtime
behavior under a separate contract. A5 does not infer that need from one
sample and does not create a scheduler that future systems would be forced to
fit.

Related evidence:

- `docs/contracts/simulation-phase-metrics-contract-v1.md`
- `docs/reports/architecture-lab-a5-simulation-metrics-report-v1.md`
- `tools/validate_simulation_metrics_boundary.ps1`

### 0.6 Documentation is part of the refactor safety net

#### Problem

Track A accumulated a strong contract/report discipline, but the tutorial's
physical structure still followed one Part per implementation batch. The
roadmap instead defines Parts by Track and forbids empty chapters for future
work. Without an executable boundary, a later batch could silently create a
new file, omit its trade-offs, leave an empty template, or describe a proposed
capability as though it already existed.

#### Naive Solution

The simplest response is a copied Markdown template and a reviewer checklist.
Another tempting option is one tutorial file per Sprint, with separate files
for Problem, Design, Validation and Trade-offs. Both make an individual change
easy to start because no existing chapter needs to be understood first.

#### Failure

Copied templates do not prove that their headings contain evidence, and file
proliferation separates one architectural decision from the failure and
validation that justify it. Placeholder Parts also turn the capability map
into an apparent backlog. Over time the tutorial can remain syntactically neat
while drifting away from the task ledger, contracts and implemented code.

#### Design Evolution

AL-A6 keeps one living tutorial and adds a small machine-readable manifest.
Each completed batch maps to an owning Part, a real section and one frozen
evidence path. A Part exists only after its first verified batch; multiple
batches may share a section when that produces the clearest explanation. The
stable reading path is Problem, Naive Solution, Failure, Design Evolution,
Implementation, Validation and Trade-offs.

Optional material such as data structures, runtime flow, debug methods,
benchmarks and exercises is nested under those headings only when it exists.
This preserves the richer roadmap template without manufacturing empty prose.

#### Implementation

`tools/validate_architecture_lab_documentation.ps1` parses the manifest and
cross-checks it against `docs/current/todolist.md`. Evidence paths must be
repository-relative, remain inside the repository and resolve to real files.
Every declared section must contain all seven non-empty logical headings, and
every Part must own at least one implemented manifest row.

The validator also requires one canonical tutorial file and the roadmap's
one-file, no-empty-chapter and Capability-Map-not-Backlog rules. It runs before
the build inside `scripts/verify_build.ps1`, so later code batches update their
tutorial evidence as part of the same engineering gate.

#### Validation

The focused positive run validates all Track A manifest rows, the single Part
and every implemented section. Its self-test additionally mutates isolated
document strings to prove that malformed rows, missing evidence, an empty
logical section and a placeholder Part all fail:

```powershell
& .\tools\validate_architecture_lab_documentation.ps1 `
  -Root (Get-Location).Path -SelfTest
```

The complete VS2017/v141 gate then proves that the documentation check composes
with the established Debug/Release, runtime, crash and clean-package pipeline.
It does not turn a hidden client into Computer Use evidence.

#### Trade-offs

The manifest and fixed headings add maintenance to every completed batch.
That cost is deliberate: architecture claims become reviewable alongside the
code that created them. The validator checks structure and evidence identity,
not writing quality or design wisdom, so human or AI review still decides
whether an explanation is useful.

One physical file may eventually become too large. Splitting remains possible
after at least two Tracks are complete and a separately approved refactor can
show a real navigation or validation problem; Sprint-by-Sprint fragmentation
remains prohibited.

Related evidence:

- `docs/contracts/architecture-lab-documentation-pipeline-contract-v1.md`
- `docs/reports/architecture-lab-a6-documentation-pipeline-report-v1.md`
- `tools/validate_architecture_lab_documentation.ps1`

## Part 01 — Bounded large-world streaming

Track B starts from an existing playable stream: one loader worker generates or
loads chunks, builds copied CPU meshes off-lock, and lets Ogre upload them. B1
does not add more throughput policy. It first makes the lifecycle observable so
later demand, scheduling, cancellation and backpressure work has exact states to
operate on.

### 1.1 Why are chunk data, CPU meshes and GPU residency separate states?

#### Problem

The old runtime exposed `Empty/Generating/Loaded` for Chunk data and combined
CPU and GPU progress in `Dirty/CpuReady/GpuBuffered`. Ogre object existence was
implicit in a map. That was sufficient for one worker, but could not answer
whether a chunk was saving while its mesh was dirty, whether a CPU result was
ready while no GPU object existed, or which owner could legally change a state.

#### Naive Solution

A tempting fix is one large enum containing combinations such as loaded,
mesh-dirty, CPU-ready, GPU-buffered and saving. Another is to infer state from
queues, dirty flags and Ogre map membership whenever a debug panel asks. Both
avoid adding explicit transition code in the short term.

#### Failure

The combined enum grows as the cross-product of unrelated lifecycles and makes
legal partial progress hard to represent. Inference is equally fragile: a
failed save, an edit during off-lock build, or an unload during upload can leave
the inspected containers describing different moments. Neither design gives a
single owner a reviewable mutation boundary or a useful illegal-transition
assertion.

#### Design Evolution

B1 freezes three orthogonal machines. `Chunk` owns seven Data Residency states,
`ChunkSection` owns five CPU Mesh states, and Ogre owns four Render Residency
states. Each family has an exhaustive `canTransition` function and a mutation
helper that asserts before changing state. Shared diagnostics copy counts; they
do not move ownership back into `World`.

The design follows the real paths. Storage load and generation converge on
`Resident`; save returns to its prior resident/eviction state; edits invalidate
any derived mesh state; and Ogre validates an upload against a second immutable
live-revision snapshot before treating the visual as resident.

#### Implementation

`World/Chunk/ChunkLifecycle.*` contains only the three vocabularies, stable
names and transition matrices. `ChunkManager` brackets load, generate, save and
unload calls with Data transitions. `ChunkRuntime` and `ChunkSection` reuse the
existing FIFO and single worker for `Dirty -> Queued -> Building -> CpuReady ->
Clean`. Ogre keeps a separate section-keyed Render map and destroys rejected or
unloaded visuals before the next rendered frame.

The existing 6 ms/64-target loader pass, one loader, two synchronous sections
per frame and eight unloads per frame remain unchanged. Save v12, terrain v4,
Gameplay and the 78-method World public surface are unchanged.

#### Validation

`tools/validate_chunk_residency_state_machine.ps1` checks exact vocabularies,
owners, assertions, runtime orchestration, debug UI, test identities, removal
of legacy combined enums and absence of post-B1 capabilities. It composes with
the AL-A1 through AL-A6 gates in `scripts/verify_build.ps1`.

The `HELLOMINE3D_WORLD_SMOKE_FOCUS=B1` path passes 38 checks: exhaustive legal
edge counts and representative illegal edges, generated residence, separate
statistics, the mesh flow, dirty save before absence, reload, injected save
failure rollback, stale upload rejection, light-only mesh invalidation, unload
persistence and direct deterministic structure generation/reload.

#### Trade-offs

The states add explicit transitions and debug fields without yet improving
streaming throughput. Some states are deliberately short-lived, so a sampled
panel may often show zero even though tests exercise the edge. `Absent` and
`NotResident` are bounded to tracked coordinates/sections rather than pretending
the infinite world is an enumerable set.

Ogre takes a second copied snapshot after acknowledgement to keep the frozen
World facade unchanged. B2 may add demand values, but it must consume these
machines rather than merge them or reinterpret B1 as a scheduler.

Related evidence:

- `docs/contracts/chunk-residency-state-machine-contract-v1.md`
- `docs/reports/architecture-lab-b1-chunk-residency-report-v1.md`
- `tools/validate_chunk_residency_state_machine.ps1`
