# AL-A3 World Simulation Boundary Contract v1

> Status: Frozen after AL-A3 verification
>
> Runtime behavior: preserve existing fixed-tick semantics
>
> Authoritative task status: `docs/current/todolist.md`

## 1. Purpose and approved scope

AL-A3 establishes one concrete `WorldSimulation` fixed-tick orchestration
boundary around the behavior already shipped by `World::tick(int)`. The batch
may only:

- introduce `WorldTickContext` with the caller-provided tick and the existing
  fixed delta of `1 / 20` seconds;
- move the existing fixed-tick call sequence from `World::tick` into an owned
  `WorldSimulation`;
- name the current phases and record their raw last-tick elapsed time;
- expose that copied timing snapshot through the existing `WorldDebugStats`
  query and developer debug panel;
- add static and behavioral regression gates for the boundary.

`World::tick(int)` keeps its public signature and remains the compatibility
entry point used by `WorldManager` and all tests.

## 2. Frozen phase order

The first implementation names the exact existing order rather than inventing
an idealized scheduler:

1. `TickPreparation` — apply pending difficulty, publish world time, reset
   per-tick combat budgets, decay feedback and synchronize the Player actor;
2. `ActorSimulation` — tick PlayerActor, decay action cooldowns, then tick the
   ActorManager;
3. `Combat` — advance combat projectiles;
4. `Encounter` — reconcile the existing Waystone encounter;
5. `BlockRandomTick` — run the existing deterministic random-tick budget;
6. `Population` — run the existing natural-mob population budget;
7. `BlockEntitySimulation` — tick loaded furnaces when the frozen smelting
   registry is available;
8. `GameplayRuntime` — update AlphaJourney and perform a pending respawn.

AL-A3 must not reorder calls inside or between these phases. A phase name is an
observation boundary, not a permission to change gameplay.

## 3. Raw timing contract

`WorldSimulationSnapshot` contains only:

- completed tick count;
- last caller tick and fixed delta;
- last whole-tick elapsed milliseconds;
- one non-negative elapsed-millisecond sample for each ordered phase.

This is raw diagnostic observation. AL-A3 does not define averages,
percentiles, budgets, overruns, priorities or deferred work. Those semantics
belong to a separately approved A5 after real phase evidence exists. Raw timing
is not persisted and is excluded from deterministic state comparisons.

## 4. Pause and ownership

`World` owns one concrete `WorldSimulation`. The runtime holds a non-owning
reference to its World and may call existing private implementation methods;
authoritative gameplay state remains owned by World, ActorManager, PlayerActor,
ChunkManager and the existing gameplay runtimes.

Pause remains a caller-side application-flow rule:

```text
GameApplicationFlow::acceptsWorldSimulation()
  -> SandboxRuntime::update / FixedTickScheduler
  -> WorldManager::tick
  -> World::tick
  -> WorldSimulation::fixedTick
```

`WorldSimulation` must not add its own pause state or advance while the existing
Ogre application gate rejects simulation. Resume must not replay paused wall
time beyond the existing bounded FixedTickScheduler behavior.

## 5. Explicit non-goals

AL-A3 must not:

- move gameplay state ownership or rewrite the existing phase implementations;
- add `SimulationScheduler`, `ISandboxSystem`, a Registry, virtual system
  interfaces, `SimulationBudget` or deferred-work slots;
- change fixed tick frequency, catch-up cap, random-tick/combat/population
  budgets or phase order;
- change save v12, terrain identity, resources, recipes, rendering or input;
- add multithreaded simulation, jobs, cancellation, backpressure or Residency
  state;
- begin A4, A5, B1 or any Extended capability.

## 6. Compatibility and acceptance

The contract may be frozen after verification only when:

1. the AL-A1 validator still reports the same 78 public methods and hash;
2. a static A3 gate proves `World::tick` delegates once, the eight phase calls
   retain their source order, raw timing is present, and forbidden abstractions
   are absent;
3. focused runtime tests prove context propagation, phase identity/order,
   exactly one sample per completed tick, finite non-negative timings and the
   caller-owned pause gate;
4. existing deterministic, replay, random-tick, Actor, combat, population,
   furnace, progression and save tests remain green;
5. VS2017/v141 Debug and Release complete gates pass, including the isolated
   clean package.

AI gameplay scenarios remain `NOT_RUN` unless separately executed through the
active package-only acceptance protocol. Human fun, aesthetics and physical
input feel remain `NOT_CLAIMED`.
