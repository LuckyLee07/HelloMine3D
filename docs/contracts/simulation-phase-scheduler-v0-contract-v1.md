# D1 Simulation Phase Scheduler v0 Contract v1

> Status: Frozen after D1 verification
>
> Workflow state: Done
>
> AI gameplay result: `NOT_RUN`
>
> Human subjective claims: `NOT_CLAIMED`

## 1. Proven problem and approved scope

D1 starts only because three current fixed-tick workloads now demonstrate the
same admission problem in real code:

1. `ActorManager` previously invoked every live managed Actor every tick;
2. active random-tick Sections already need a four-item FIFO rotation, but its
   admission limit was embedded inside `World`;
3. loaded Furnace and Crusher block entities were scanned independently and
   every machine was invoked every tick.

The approved v0 evolves the concrete `WorldSimulation` coordinator. It does
not create a generic system Registry, `ISandboxSystem`, jobs, worker threads or
future phase slots. D2-D8, C4-C11, simulation activation/LOD, offline catch-up,
network power and new Gameplay remain outside this batch.

## 2. Fixed phase barriers and three workload identities

The eight AL-A3 phases and their relative order remain frozen. Tick
Preparation, PlayerActor, player cooldowns, Combat, Encounter, Population and
Gameplay Runtime remain mandatory whenever the caller admits a fixed tick.
Only these three existing workloads use D1 admission:

| Workload | Phase | Unit | Per-tick budget | Order owner |
| -------- | ----- | ---- | --------------- | ----------- |
| `ManagedActors` | Actor Simulation | one live ActorManager actor invocation | 64, plus the mandatory PlayerActor outside this budget | D1 round-robin cursor over ActorManager insertion order |
| `RandomTickSections` | Block Random Tick | one active Section | existing `World::RandomTickSectionBudgetPerTick == 4` | existing World FIFO rotation |
| `BlockEntities` | Block Entity Simulation | one loaded Furnace or Crusher invocation | 32 | D1 round-robin cursor over a deterministic machine list |

No Network phase is registered: C3 topology has no per-tick behavior. No Chest
slot is registered because Chest has no fixed-tick work. Combat and Population
retain their A5 observed local limits but are not falsely described as D1
scheduler workloads.

## 3. Deterministic admission and fairness

Budgets count work items, never elapsed milliseconds. For a stable eligible
set of size `N` and positive budget `B`, a round-robin workload has a finite
service window of `ceil(N / B)` fixed ticks. A plan records exactly:

```text
eligible / admitted / deferred / budget / firstIndex / serviceWindowTicks
```

Managed Actors retain ActorManager insertion order. Block Entities are rebuilt
from loaded world truth each tick and sort Furnace before Crusher, then X/Y/Z.
Random Tick keeps its established FIFO order; D1 supplies only its admitted
count. Empty or changed sets normalize a cursor before use, and plans retain no
pointer or reference into mutable domain storage.

Fairness is guaranteed for a stable eligible set. When entities are added,
removed, loaded or unloaded, the next index is deterministically reduced into
the new list; D1 does not promise identity-perfect age tracking across a
mutating set. This is an explicit v0 boundary, not dropped work.

## 4. Runtime behavior and compatibility

Below each budget, Actors retain insertion order and every admitted Actor or
machine retains its existing one-tick result. Block Entities intentionally
replace the old unordered-Chunk traversal with the canonical Furnace-first,
Crusher-second, X/Y/Z order required for deterministic round-robin fairness.
Above a budget, admitted work performs exactly one existing `1/20 s`
simulation step and deferred work performs no catch-up until its next service.
D1 deliberately prefers bounded CPU work over wall-clock catch-up;
there is no multiplied `dt`, time debt or wall-time feedback that could make a
replay nondeterministic.

Furnace and Crusher continue to use their C2 `MachineRuntime` adapters and
payload v1. Random tick selection still derives from seed, world tick, Section
and attempt. Player input/cooldowns, combat resolution, encounter reconciliation,
population and save commands are never deferred by D1.

World save v12, Chunk v2, terrain v4, settings v8, Furnace payload v1 and
Crusher payload v1 remain unchanged. Scheduler cursors, plans and metrics are
transient derived state and must not be serialized. Reopening starts a fresh
cursor without modifying authoritative Gameplay state.

## 5. Observation and failure semantics

The copied `WorldSimulationSnapshot` publishes exactly three
`scheduledWorkloads`. The existing A5 metric vocabulary is extended only by a
real `BlockEntitySimulation` row and the copied fields `eligible`,
`serviceWindowTicks` and `schedulerManaged`. Actor phase budget is 65 because
its metric includes the mandatory PlayerActor plus 64 managed actors.

The developer Simulation panel shows every plan and phase metric. `deferred`
means eligible work not admitted in this tick; it is neither an error nor a
drop count. A service window is a deterministic bound for the current stable
set, not a millisecond SLA or a Q1/Q3 performance claim.

Malformed or stale machine records continue to fail closed in their existing
adapter. A planned invocation that discovers invalid domain state does not
retain a stale task. Failed tests retain their first failing output; thresholds
must not be enlarged merely to obtain PASS.

## 6. Automated acceptance

`HELLOMINE3D_WORLD_SMOKE_FOCUS=D1-SCHEDULER` must prove:

1. exactly three concrete workload identities and no future empty slot;
2. 64/4/32 item budgets, exact plan counts and stable service-window math;
3. Actor and Block Entity overload defer work on the first tick and service it
   in the next two-tick window without dropping it;
4. Random Tick retains its established FIFO order and four-Section budget;
5. PlayerActor, phase barriers, 20 Hz and save v12 remain mandatory;
6. existing Furnace/Crusher exact progression, Actor, Random Tick, Combat,
   Population, C2, C3, save/reopen and streaming regressions remain green.

`tools/validate_simulation_phase_scheduler.ps1` freezes the concrete scheduler,
three identities, deterministic policies, adapters, copied UI evidence,
non-persistence and excluded D2+/generic abstractions. It is part of the full
VS2017/v141 Debug and Release `scripts\verify_build.ps1` gate.

## 7. Exit and evidence boundary

D1 becomes `Done` only after the static gate, focused Debug and Release path,
complete VS2017/v141 gate, synchronized current documents and one independent
local commit pass. `-SkipRealWindow` is permitted for this architecture batch.

No OS Computer Use result is implied: `AI-01..AI-08` remain `NOT_RUN` unless
executed separately from a hashed clean package. Fun, aesthetics, comfort,
retention and physical-input feel remain `NOT_CLAIMED`. D1 completion does not
approve D2-D8, C4-C11 or a generic scheduling framework.
