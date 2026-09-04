# HelloMine3D Architecture Lab Tutorial

> Living tutorial status: Part 00 covers completed Track A and Part 01 covers
> implemented B1 through B10 Core. Later Parts are added only with the first verified
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
B2|01|1.2|docs/reports/architecture-lab-b2-streaming-demand-report-v1.md
B3|01|1.3|docs/reports/architecture-lab-b3-world-job-scheduler-report-v1.md
B4|01|1.4|docs/reports/architecture-lab-b4-world-job-cancellation-report-v1.md
B5|01|1.5|docs/reports/architecture-lab-b5-streaming-backpressure-report-v1.md
B6|01|1.6|docs/reports/architecture-lab-b6-spatial-activation-report-v1.md
B10|01|1.7|docs/reports/architecture-lab-b10-large-world-stress-report-v1.md
C1|02|2.1|docs/reports/architecture-lab-c1-block-capability-report-v1.md
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

### 1.2 Why is streaming an expiring set of demands instead of one center?

#### Problem

The existing loader treated one Player-derived center as the whole definition
of needed world data. Real gameplay already had more sources: the camera can
look away from movement, teleport needs a destination immediately, and spawn,
respawn or explicit preparation preload an area. With one mutable center these
sources overwrite each other implicitly and leave no evidence of priority or
lifetime.

#### Naive Solution

One option is to keep adding special cases around `m_loadCenterX/Z`: teleport
can force a synchronous load, camera visibility can alter the distance score,
and preload can temporarily move the center before restoring it. Another is to
append every request to an unbounded priority queue and rely on duplicate
filtering when the loader eventually reaches it.

#### Failure

Special cases make the last caller the hidden owner of streaming intent. A
camera turn or old teleport can continue to influence pending work without a
clear expiry rule. An append-only queue records history rather than current
demand, grows under repeated refreshes and reaches B5 backpressure questions
before the demand vocabulary itself is defined.

#### Design Evolution

B2 represents each implemented source as one replaceable slot containing a
coordinate, reason, priority, epoch, expiry and radius. Player and Camera are
short-lived and refreshed each World update; TeleportDestination and Preload
live longer but expire exactly after their frozen epochs. Overlapping squares
merge into one target carrying a reason-bit union, highest priority and newest
epoch.

The planner orders current targets by reason priority, frustum visibility,
Player motion direction, recency, distance and stable coordinates. A semantic
demand change or camera/frustum change discards the derived pending plan. This
is replacement of not-yet-started planning data, not B4 cancellation of work
already executing.

#### Implementation

`World/Chunk/ChunkDemand.*` owns the four-slot value model and stable policy.
`ChunkRuntime` owns the model lock, motion sample and deterministic expansion /
merge / sort operation, then feeds the unchanged single loader. `World::update`
publishes Player and Camera; `preloadAround` records Preload while preserving
its synchronous behavior; a private bridge lets `WorldManager` publish a
TeleportDestination only after a same-world teleport is accepted.

The one worker, 6 ms pass, 64-target pass, one load per target, two-section mesh
budget and eight-Chunk unload budget remain unchanged. Demand is derived state:
it is not serialized and cannot mutate save v12 or B1 lifecycle ownership.

#### Validation

`tools/validate_streaming_demand_model.ps1` freezes the four reasons, exact
priorities/lifetimes, owners, publication sites, panel fields and twelve
behavior identities. The current composed gate admits the separately
contracted B3-B5 extensions while continuing to reject B6-B9 concepts. It
composes with AL-A1 through B1 in the
full VS2017 gate.

`HELLOMINE3D_WORLD_SMOKE_FOCUS=B2` passes 26 checks. They prove stable bounded
refresh, reason replacement, overlap merge, teleport priority, forward-motion
ordering, camera-turn replanning, exact expiry, World Player/Camera/Preload
publication and accepted/rejected teleport behavior. The existing WorldManager
scenario remains in the focused set to protect synchronous gameplay effects.

#### Trade-offs

Four slots intentionally describe only current implemented sources. Machine,
Actor and debug-camera reasons are not reserved for hypothetical systems. A
square radius remains coarser than a future spatial-interest volume. B3 now
uses the resulting plan as typed work, but does not reinterpret the four-slot
model as cancellation or queue-pressure policy.

The priority values are fixed B2 policy rather than a claim of ideal feel. They
make ordering deterministic and observable while leaving B3 job arbitration,
B4 in-flight cancellation, B5 pressure control and B6 representation interest
as separate problems with separate contracts.

Related evidence:

- `docs/contracts/streaming-demand-model-contract-v1.md`
- `docs/reports/architecture-lab-b2-streaming-demand-report-v1.md`
- `tools/validate_streaming_demand_model.ps1`

### 1.3 Why should the loader schedule typed work instead of coordinates?

#### Problem

After B2, the loader had a deterministic list of demanded coordinates but its
private deque still hid what work each coordinate represented. Loading or
generating a 3x3 neighborhood and building one copied CPU mesh were selected by
branches inside the loop, so queue latency, lifecycle and stale-commit outcome
could not be observed as one coherent pipeline. B4 cancellation and B5 pressure
control would have had no stable job boundary to target.

#### Naive Solution

The smallest-looking change is to add more booleans beside each queued
coordinate or immediately create separate IO, generation, lighting, FarLOD and
simulation jobs. Another tempting response is to add a worker pool and a
cancellation token at the same time so the scheduler appears feature-complete
on its first revision.

#### Failure

Coordinate flags still leave transitions implicit and make metrics infer
meaning from whichever branch happened to run. Conversely, separate IO and
generation jobs would be fictional today: `ChunkManager` deliberately performs
storage load with deterministic generation fallback as one compatibility
operation. A worker pool, cancellation and watermarks would also change
concurrency and policy before B3 had established observable semantics.

#### Design Evolution

B3 models only the two work families the game actually executes:
`ChunkLoadOrGenerate` and `ChunkMeshBuild`. Every job has a monotonic id, B2
priority/epoch/plan order, target, enqueue time and exactly one legal lifecycle:
`Pending -> InFlight -> Completed`. Pending order is priority, plan order,
newest demand epoch, type and id. Replanning replaces only pending work and
explicitly preserves the single in-flight job; that is not B4 cancellation.

Completion freezes one of `DidWork`, `NoWork` or `CommitRejected` plus queue,
worker and commit timings. The loader immediately drains the completion and
publishes the next real step. This supplies a concrete boundary for later
cancellation and pressure without pre-registering any later job family.

#### Implementation

`World/Streaming/WorldJobScheduler.*` owns the pending vector, one optional
in-flight value, completion deque, ids and copied metrics under one mutex.
`ChunkRuntime` converts B2 targets into load/generate jobs, executes them under
the existing World mutex and follows a ready neighborhood with a mesh job.
Mesh input is still copied under the lock, built off-lock and committed through
the B1 revision check under the lock.

`ChunkManager::prepareChunkNeighborhood` factors the existing bounded 3x3 load
step out of `beginMeshJob`; it loads at most one Chunk per execution and does
not move data ownership into the scheduler. The one worker, 6 ms/64-job pass,
two synchronous section rebuilds and eight unloads per frame are unchanged.
The developer panel copies queue counts, lifecycle/outcome/type totals and last
queue/worker/commit milliseconds.

#### Validation

At B3 closeout, `tools/validate_world_job_scheduler.ps1` froze two types, three
states, three outcomes, exact ordering, one worker, real pipeline calls,
unchanged budgets, diagnostic fields and test identities. The current composed
gate admits B4's separately contracted token/`Cancelled` outcome and B5's
admission/pressure extension while continuing to reject B6-B9.
`HELLOMINE3D_WORLD_SMOKE_FOCUS=B3` passes 9 checks for duplicate rejection,
ordering, lifecycle, invalid completion, pending replacement, timing/counters
and actual execution of both job types by a live background World.

B2 remains `26/26` and B1 remains `38/38` in the same VS2017/v141 Debug build.
The complete VS2017/v141 gate passes `884/884` WorldRuntime in Debug and
Release, `80/80` resource-pack checks, `122/122` recipe checks, `15/15` startup
negatives and two zero-failure short soaks. The clean 104-entry package hashes
to `B983889B5553FF0DBFEAF6C14D2E349CC81AD1205C314CC39C0894C2D1CD9459`.
The report does not promote this headless result into AI gameplay.

#### Trade-offs

B3 intentionally keeps a single worker and a small sorted vector rather than
claiming scalable parallel throughput. The completion queue is real but the
current loader drains it immediately; no second main-thread mutation route is
introduced. Replanning cannot stop in-flight work, and no hard cap, watermark,
admission result or shedding policy exists; B4 later supplies cancellation,
while B5 remains the separate pressure-control batch.

Scheduler ids and timings are diagnostic derived state: they are not saved,
compared for deterministic equality or used to change Gameplay. Load and
generation remain one type until a real storage pipeline creates a meaningful
phase boundary. FarLOD, lighting, simulation, Actor, machine and network slots
remain absent instead of appearing as empty architecture demonstrations.

Related evidence:

- `docs/contracts/world-job-scheduler-contract-v1.md`
- `docs/reports/architecture-lab-b3-world-job-scheduler-report-v1.md`
- `tools/validate_world_job_scheduler.ps1`

### 1.4 Why must asynchronous work be allowed to finish and still be discarded?

#### Problem

A voxel streamer makes decisions from a moving view. While one worker reads or
generates a Chunk, the player can cross a Chunk boundary, teleport, reduce the
render distance or reset meshes. The CPU work may still finish successfully,
but success no longer means its result belongs to the current plan. B1's mesh
revision catches edits to the same authoritative section; it cannot say that a
whole streaming plan has become obsolete.

The danger is at publication. If an old job loads a distant Chunk or installs a
CPU mesh after replanning, obsolete work becomes authoritative again. Clearing
only the pending queue does not help because B3 deliberately preserves its one
in-flight record.

#### Naive Solution

One response is to kill the worker or attempt to interrupt storage,
generation and mesh construction at arbitrary instructions. Another is to
check a boolean once before work begins. A third is to reuse the B2 demand
epoch or B1 block revision as if either identity represented every kind of
staleness.

These approaches look immediate, especially with one worker, but they mix
thread lifetime, scheduling intent and authoritative data identity. They also
make the expensive algorithms responsible for understanding every caller's
current plan.

#### Failure

Forced interruption cannot safely stop C++ code while it owns temporary
containers or is inside a storage library. A check only at job start misses a
plan change during the expensive portion. A check immediately before commit is
still racy if invalidation can occur between that check and publication.

Demand epoch orders demand samples but does not identify the scheduler plan
that accepted a job. Block revision detects mutation within one Chunk section,
not a camera move that makes an otherwise valid result unwanted. Conflating
them either rejects useful work or accepts obsolete work.

#### Design Evolution

B4 adds a copied `WorldJobGenerationToken` beginning at 1. Semantic plan
changes advance generation and clear obsolete pending jobs. The in-flight job
is not forcibly interrupted: it may perform white work, but it must check its
token before detached work, after detached work and before publishing a
follow-up. A stale job still completes the B3 lifecycle with the new outcome
`Cancelled`; cancellation is not a fourth state.

The final token check and authoritative mutation run under the shared World
mutex plus a generation/commit mutex. Invalidation uses the same commit mutex,
so it is impossible to slip between validation and publication. Generation
answers “is this plan current?” while B1 revision independently answers “is
this same-generation data current?”.

#### Implementation

`WorldJobScheduler` owns the atomic uint64 generation, stale-request/plan
rejection and cancellation counters. `ChunkRuntime` invalidates on Player or
Camera semantic demand changes, Teleport/Preload lifecycle changes, render
distance, mesh reset and loader shutdown; camera/frustum priority-only reorder
does not cancel valid work.

Load/generate now reserves one `Loading` placeholder under the World lock,
hydrates or generates a detached candidate outside it, then commits or rolls
back through `ChunkManager`. Candidate sections suppress live random-tick index
notifications until commit, and mutable terrain generation is serialized.
Cancellation uses exactly one new B1 edge, `Loading -> Absent`. Mesh
cancellation returns the exact still-matching `Building` section to `Dirty`
without adopting output or incrementing rebuild metrics.

#### Validation

`tools/validate_world_job_cancellation.ps1` freezes token/outcome vocabulary,
six invalidation call sites, detached ownership, random-tick isolation,
linearized commit and mesh rollback. The current composed gate additionally
recognizes B5's separately contracted admission/pressure vocabulary while
continuing to reject B6 and later capabilities.
`HELLOMINE3D_WORLD_SMOKE_FOCUS=B4` passes 10 checks for monotonic invalidation,
stale admission, unchanged B3 ordering, exact metrics, candidate cancel/commit,
mesh cancel, 300 concurrent replans/invalidations and a live World generation
advance.

B3/B2/B1 focused regressions pass `9/9`, `26/26` and `38/38`. The complete
VS2017/v141 Debug/Release gate passes `894/894` WorldRuntime twice, `80/80`
resource-pack, `122/122` recipe, `15/15` startup negatives and two zero-failure
short soaks. The 104-entry isolated ZIP hashes to
`A9A7CC9AF528F3C725ACC13A62A69FB6D10718AD25F7F4796F5E47B70453C33A`;
the final result is `PASS real_window=DEFERRED`. This headless evidence does not
change `AI-01..AI-08=NOT_RUN` or human subjective `NOT_CLAIMED`.

#### Trade-offs

B4 accepts wasted CPU/IO rather than adding unsafe pre-emption inside terrain
or storage code. It keeps one worker and does not promise a hard queue cap;
replacement keeps the current finite B2 plan bounded, while explicit
watermarks, admission and shedding belong to B5. The extra commit mutex is a
small serialization point around validation and publication, not around the
expensive detached work.

A cancelled reservation remains as an explicit `Absent` Chunk object until
normal manager cleanup rather than erasing arbitrary ownership mid-flight.
Generation ids and counters remain derived diagnostics and are not saved.
No future job family, Spatial Interest policy, worker pool or generic
Simulation Scheduler is introduced.

Related evidence:

- `docs/contracts/world-job-cancellation-contract-v1.md`
- `docs/reports/architecture-lab-b4-world-job-cancellation-report-v1.md`
- `tools/validate_world_job_cancellation.ps1`

### 1.5 Why does a finite world plan still need backpressure?

#### Problem

A bounded render distance does not imply a small instantaneous work set. One
default radius-eight demand expands to 289 Chunk targets, while the supported
radius 32 expands to 4,225. Before B5, B3 could replace its pending vector with
that complete plan, and the render thread could copy and upload every observed
`CpuReady` section in one frame. Memory was finite, but queue latency and frame
cost had no stable upper bound.

#### Naive Solution

The tempting answers are to raise throughput with more workers, truncate every
plan at an arbitrary count, or drop whichever pending element happens to be at
the end of a container. It is equally tempting to label the existing six
millisecond loader pass a complete pressure policy, even though mesh upload and
unload are separate consumers on the main thread.

#### Failure

More workers increase commit contention without defining what happens during
overload. Blind truncation can permanently lose currently demanded targets,
and container-order eviction breaks the deterministic B3 priority contract.
The time budget limits one loader pass but does not bound pending jobs, the
next frame's GPU uploads or the amount of deferred demand that still needs to
enter the scheduler.

#### Design Evolution

B5 separates the current demand plan from admitted job state. The immutable
B2-derived request vector keeps a monotonic cursor, initially admits the best
96 requests, and refills from 48 toward 96. The scheduler has a hard pending
cap of 128 and explicit `Normal/Elevated/Saturated` pressure. At capacity only
a strictly better request under the existing B3 order may replace the one
deterministic worst pending request; the single in-flight job is never shed.

#### Implementation

`WorldJobScheduler::admit` returns `Accepted`, `AcceptedAfterShedding`,
`Duplicate`, `StaleGeneration` or `RejectedAtCapacity` and records copied
pressure/admission/shedding metrics. `ChunkRuntime` owns the retained plan and
cursor only on its loader stack, publishes follow-up work before refill and
clears deferred state on B4 generation invalidation. The same boundary limits
authoritative commit intervals per loader pass, CPU-ready offers per render
frame and distant unloads per update to eight each.

#### Validation

`tools/validate_streaming_backpressure.ps1` freezes caps `128/128/128`,
watermarks `96/48`, one loader, the three `8` consumer budgets, diagnostics and
twelve B5 test identities while permitting only the separately contracted B6
extension and continuing to reject B7 and later vocabulary.
`HELLOMINE3D_WORLD_SMOKE_FOCUS=B5` passes `12/12`; B4/B3/B2/B1 focused
regressions pass `10/10`, `9/9`, `26/26` and `38/38`. The complete VS2017/v141
Debug/Release gate passes `906/906` WorldRuntime twice, `80/80` Resource Pack,
`122/122` Recipe, `15/15` startup negatives and two zero-failure short soaks.
The 104-entry isolated package SHA-256 is
`7D126B31B78F3A4E8F8C90A5D769028EC686C0D4F708D1F6B2E2979BD164050B`;
the final result is `PASS real_window=DEFERRED`.

#### Trade-offs

The active plan vector duplicates request metadata but not job ids, states or
completion records, so it is bounded derived demand rather than a hidden
scheduler. Fixed watermarks make behavior teachable and deterministic but are
not claimed to be universally optimal. B5 keeps one worker and accepts that a
current target may wait for refill; it does not add adaptive timing, Spatial
Interest, Far representation or a generic multi-system scheduler.

Related evidence:

- `docs/contracts/streaming-backpressure-contract-v1.md`
- `docs/reports/architecture-lab-b5-streaming-backpressure-report-v1.md`
- `tools/validate_streaming_backpressure.ps1`

### 1.6 Why does loaded data not imply a visible or simulated region?

#### Problem

B2 can explain why a Chunk is wanted, but B3-B5 previously sent every merged
target through the same load-to-mesh pipeline. A short-lived preload needed
block data for a safe query yet also caused CPU mesh work and renderer-visible
sections. Conversely, the existing camera-distance unload rule could evict a
Chunk that a teleport or preload demand still required. “Loaded” was carrying
data, representation and future simulation meanings at once.

#### Naive Solution

One shortcut is to add a single `active` flag to `Chunk`. Another is to invent
Near/Far/Full/Reduced/Dormant lifecycle states immediately and let rendering,
streaming and Actor code all switch on them. Both approaches look compact
because one enum appears to answer every question.

#### Failure

A combined state creates illegal coupling. A Chunk may need resident block data
without a mesh, while CPU-ready mesh and GPU residency already have independent
B1 lifecycles. Far rendering has no current consumer, and changing Actor or
random-tick fidelity would be a D-series gameplay decision. Folding those
concerns together would either do unnecessary work or silently change gameplay
before a real policy and evidence boundary exists.

#### Design Evolution

B6 derives three independent booleans from the immutable B2 demand snapshot:

```text
SimulationRequested => NearRepresentation => ResidentData
```

Player and Camera demands request resident data and near representation.
Player additionally publishes simulation interest within Chebyshev radius two.
TeleportDestination and Preload request resident data only. Overlaps merge with
logical OR and preserve the B2 reason mask; absent coordinates are `Outside`.

#### Implementation

`World/Streaming/SpatialInterest.*` builds one sorted cell per coordinate and
carries the source demand revision. `ChunkRuntime` refreshes the copied snapshot
only after semantic demand changes. Planning admits resident cells, resident-
only work stops before mesh follow-up, copied mesh snapshots and upload
acknowledgement require current Near interest, and unload protects current
Resident interest before applying the unchanged B5 eight-Chunk budget.

The developer panel reports total, Resident, Near and Simulation Requested
counts. Simulation interest is observation only: no Actor, combat, crop,
furnace, random-tick, population or objective code consumes it.

#### Validation

```powershell
& .\tools\validate_spatial_activation.ps1 `
  -Root (Get-Location).Path -Implementation
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "B6"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

The focused gate covers exact vocabulary and hierarchy, all four source
policies, deterministic merging/order/revision, radius shrink, resident-only
mesh suppression, copied-render filtering, resident protection with bounded
expiry unload, and unchanged fixed-tick Actor metrics. It also retains one
loader and the B5 `8/8/8` consumer budgets while rejecting Far and D2 fidelity
vocabulary. The focused run passes `12/12`; the complete VS2017/v141 Debug and
Release gate passes `918/918` twice, and the 104-entry isolated package SHA-256
is `C8E260E00CF76C952150EBC3DC851A7EDE5E13FE63A58F98B77DC103723EFA3C`.

#### Trade-offs

The snapshot expands bounded demand radii into a sorted coordinate vector, so
semantic demand changes pay allocation and sort cost in exchange for immutable,
deterministic reads. Preload and teleport remain synchronous legacy data loads;
B6 narrows their downstream work but does not redesign that API. Near currently
matches the full Player/Camera demand radius, and Simulation Requested has no
consumer. These are explicit first policies, not claims that Far rendering or
simulation LOD has already been implemented.

Related evidence:

- `docs/contracts/spatial-activation-contract-v1.md`
- `docs/reports/architecture-lab-b6-spatial-activation-report-v1.md`
- `tools/validate_spatial_activation.ps1`

### 1.7 Why can a bounded unload loop still retain an unbounded world?

#### Problem

B1-B6 made every visible queue and consumer finite, but local unit scenarios
only loaded homogeneous Resident Chunks. A real long journey interleaves
Resident data with Loading reservations, cancelled generation and save work.
The important question is therefore not merely whether a loop has a numeric
limit, but whether eligible work can make progress when other lifecycle states
share the same authoritative container.

#### Naive Solution

A tempting acceptance test runs a few seconds, checks that pending jobs never
exceed 128, and treats the eight-item unload limit as sufficient proof of
bounded residency. Another shortcut raises the allowed Chunk or memory ceiling
when a long run exceeds it. Both approaches preserve green dashboards without
testing whether the intended consumer actually completes useful work.

#### Failure

The first formal schedule-v3 run exposed a cross-batch composition defect.
Cancelled detached loads transitioned their manager reservation to semantic
`Absent` but retained that coordinate in the map. The unload scan then selected
the first eight distant entries before checking whether `unloadChunk` could
legally move them from `Resident` to `EvictRequested`. A stable group of
Loading/Absent entries could consume every slot forever, while `lastUnloads`
reported attempts as progress. Resident Chunks accumulated behind them, making
the final save and backup dominate shutdown. A follow-up diagnostic also found
that synchronous mesh-neighbour reads called `getOrCreateChunk`, so an infinite
trek could retain harmless-looking but unbounded Absent edge coordinates even
after Resident eviction recovered.

#### Design Evolution

B10 keeps the B1 state machine and B5 limit, but aligns admission to the
consumer with its real eligibility. A cancelled reservation reaches `Absent`
and is erased because Absent is a semantic state, not retained world data. The
unload scan filters for `Resident` before spending a slot, and its diagnostic
count records completed removals. A failed eligible removal keeps backlog set
so a later update retries without claiming false progress.

#### Implementation

`ChunkManager::cancelChunkLoadJob` now removes its detached placeholder after
the guarded `Loading -> Absent` transition. `ChunkManager::unloadChunk` returns
whether eviction, dirty save, erase, light reconciliation and event publication
completed. `ChunkRuntime::unloadDistantChunks` protects B6 Resident interest,
filters the Data Residency state, admits at most eight candidates and derives
`lastUnloads`/backlog from actual outcomes. No second queue, worker or cleanup
path was introduced.

`ChunkSection::findAdjacent` makes mesh-neighbour inspection explicitly
non-creating. `SectionMeshInput` treats a missing neighbour as not fully solid,
matching its existing out-of-range air sampling without inserting an
authoritative Chunk. This keeps a derived snapshot Query from changing the
manager's coordinate set.

`HelloMine3DSoak --profile track-b-core` adds one deterministic five-phase
schedule over the real World, loader, mesh snapshot acknowledgement and save
root. LW1 travels forward, LW2 jumps to far destinations, LW3 reverses demand,
LW4 churns render distance, and LW5 edits, leaves, saves and reopens. The formal
run is exactly 1,800 seconds/36,000 fixed ticks; two short isolated probes
compare only deterministic schedule and persistence fields.

#### Validation

The mixed-state runtime regression creates 32 Loading reservations beside ten
distant Resident Chunks and proves that two updates complete the existing 8+2
unload budget rather than starving behind ineligible entries. A second focused
case captures mesh input beside four missing neighbours and proves the Chunk map
does not grow. The B10 static gate freezes the schedule, lifecycle repair,
production teleport fixture, thresholds and truthful evidence classifications.
The first failed formal run and its 8960-entry/2.46-GB symptom remain in the B10
report. After the two production repairs, a second attempt retained zero Absent
entries and at most 223 Chunks, but exposed a harness error: a five-second LW5
persistence cadence expanded ten developer checks into sixty full
save/reopen/backup-copy cycles and exhausted the unchanged shutdown grace. B10
now freezes ten LW5 checks by count instead of multiplying them with duration.
A 300-second diagnostic then passed with 172 maximum Chunks, 58,544,128 peak
private bytes and ten persistence checks. The final unrelaxed formal run passes
1,800 seconds/36,000 ticks with maximum 216 Chunks, zero retained Absent
entries, 137,551,872 peak private bytes and ten persistence checks; both
determinism probes match. The complete VS2017/v141 Debug/Release gate then
passes `920/920` WorldRuntime in both configurations and produces a 104-entry
isolated package with SHA-256
`E13203F8E18382A4D13ABA22DFB975DDB189B2858F74DAAE340CE5A4A8F34B14`.
The final Q1 six-scene comparison also passes without a threshold change or
performance exception; fast-streaming Chunk-visible P95 is
`204.013/192.133 ms` at the unchanged `rc-ring-12-chunks-v2` identity.

#### Trade-offs

Eviction remains synchronous because dirty-save ordering is an existing B1
compatibility promise; B10 does not turn persistence into another job family.
Ten whole save/reopen checks are a coverage budget rather than a runtime
performance threshold: fixing the check count prevents the test harness from
silently changing the workload by six times when only its observation duration
changes. The formal 180-second shutdown grace remains unchanged.
The 1,024-Chunk and two-GiB ceilings are safety bounds rather than target steady
state, and deterministic probes deliberately exclude wall-time, memory and
asynchronous queue-depth equality. The headless schedule proves engineering
boundedness and recoverability, not human fun, visual quality or physical input
feel, and it does not authorize B7-B9, Track C or Track D.

Related evidence:

- `docs/contracts/large-world-stress-acceptance-contract-v1.md`
- `docs/reports/architecture-lab-b10-large-world-stress-report-v1.md`
- `tools/validate_large_world_stress_acceptance.ps1`

## Part 02 — Capabilities driven by playable blocks

### 2.1 Why is a capability useful before a machine framework exists?

#### Problem

Chest and Furnace are already real, persisted gameplay blocks, but the Ogre
container UI identified them by trying the Furnace view first and the Chest
view second. That makes each new inventory-like block another concrete probe
and duplicates the decision about what can be observed or transferred.

#### Naive Solution

One tempting response is to keep adding `if block == type` branches. The other
is to build a complete capability Registry, machine hierarchy and mechanical
port vocabulary now so every imagined future block has a place to register.
Both choices are locally easy: the first postpones a boundary, while the second
looks architecturally comprehensive.

#### Failure

Concrete UI probes make Presentation depend on every storage implementation
and allow their ordering to determine behavior. A future-first Registry fails
differently: there is no approved mechanical node, transport system or second
processor whose behavior could validate those abstractions. Empty interfaces
would encode guesses and falsely imply that C2/C3 had begun.

#### Design Evolution

C1 uses the existing `BlockDatabase` as the only definition authority.
`BlockDefinition` declares only capabilities with concrete providers:
Chest/Furnace expose `InventoryProvider`, while Furnace also exposes
`MachineProcessor`. Discovery additionally requires the loaded block and its
persisted block-entity type to agree.

#### Implementation

`BlockCapabilityAccess` returns small value handles. Inventory views copy nine
general Chest slots or the Furnace input/fuel/output roles; commands delegate
to the existing container implementations. Processor views copy progress and
fuel values. Handles cache neither payload nor inventory and revalidate on
each use, so replacement or corruption fails closed. Ogre UI consumes these
handles and no longer includes the concrete Chest/Furnace classes.

#### Validation

`HELLOMINE3D_WORLD_SMOKE_FOCUS=C1-CAP` passes `17/17` in Debug and Release: both declarations,
slot/access rules, progress, missing/mismatched/malformed records, stale
handles and save/reopen are covered. The static gate requires the real UI
consumer and rejects Registry, MechanicalPort, Machine Runtime and network/
Extended vocabulary. The complete VS2017/v141 gate passes WorldRuntime
`937/937` in both Debug and Release; its 104-entry isolated package hashes to
`1618ACD7995FE5181169B0B46A5D4F479F63FA1CCB8B533B358ED694A3846EB6`.

#### Trade-offs

The adapters still delegate to two concrete containers; this is intentional
because they remain the proven owners of serialization and gameplay rules.
The processor view exposes only current Furnace observation, not a shared tick
runtime or status machine. `MechanicalPort` is deferred until a separately
approved C3 node supplies real connection semantics. C1 therefore removes the
current UI coupling without spending design budget on unimplemented systems.

Related evidence:

- `docs/contracts/block-capability-model-contract-v1.md`
- `docs/reports/architecture-lab-c1-block-capability-report-v1.md`
- `tools/validate_block_capability_model.ps1`
