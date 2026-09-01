# AL-A5 Simulation Phase Metrics & Budget Vocabulary Contract v1

> Status: Frozen after AL-A5 verification
>
> Runtime behavior: observe existing work and hard limits without changing them
>
> Authoritative task status: `docs/current/todolist.md`

## 1. Purpose and approved scope

AL-A5 extends the AL-A3 last-tick snapshot with one small, copied vocabulary:

```text
elapsed   = raw wall-clock time for the completed phase in the last tick
processed = work items whose existing phase implementation actually invoked work for
deferred  = eligible work left for a later invocation specifically because an existing hard item limit stopped admission
budget    = the existing hard processed-item limit within its declared scope
scope     = Unbudgeted, PerTick or PerPopulationCycle
```

The vocabulary is diagnostic. It does not own Gameplay state, persist data,
participate in deterministic comparison or authorize runtime scheduling.

## 2. First observable metric phases

The first snapshot contains exactly four real metric identities, with no empty
slots for future systems:

| Phase | Processed unit | Deferred unit | Existing budget and scope |
| ----- | -------------- | ------------- | ------------------------- |
| `ActorSimulation` | PlayerActor tick invocation plus each live ActorManager actor actually ticked | always 0; no actor admission queue exists | unbudgeted |
| `Combat` | combat projectile steps executed | existing projectiles skipped by `CombatProjectileStepBudgetPerTick` | 32 per tick |
| `BlockRandomTick` | active random-tick sections actually processed | active sections left for later rotation after the existing section limit | 4 per tick |
| `Population` | natural-mob spawn attempts actually executed | always 0; theoretical attempts are not a retained queue | current difficulty attempt limit per eligible population cycle |

The other four AL-A3 phases retain raw timing only. Their absence from the
metric array is deliberate and must not be represented as zero-valued
registered systems.

## 3. Budget status vocabulary

`SimulationPhaseBudgetStatus` is derived from a copied metric:

- `Unbudgeted`: no hard admission limit exists for the phase;
- `WithinBudget`: a budget exists, `processed < budget`, and nothing is deferred;
- `AtBudget`: `processed == budget` and nothing is deferred;
- `WorkDeferred`: at least one eligible item remains because the hard limit was reached.

For a budgeted metric, `processed` must not exceed `budget`. `deferred` is not
an error count, a drop count, spare capacity, wait time or a time-budget
overrun. AL-A5 introduces no millisecond threshold and makes no Q1/Q3 claim.

## 4. Observation and UI

`WorldSimulationSnapshot` continues to publish only the most recently
completed tick. The four metrics are copied through the existing
`WorldDebugStats` query. The developer Simulation panel shows phase elapsed,
processed, deferred, budget scope/limit and derived status.

The metrics are reset by constructing the next snapshot for every fixed tick;
they are never accumulated implicitly across ticks. Population therefore
reports zero processed on a non-cycle tick rather than retaining the previous
cycle count.

## 5. Behavior preservation and non-goals

AL-A5 must not:

- reorder the eight AL-A3 phases or calls inside a phase;
- change 20 Hz fixed tick, catch-up, projectile, random-tick, population or any
  other Gameplay limit;
- add `SimulationScheduler`, `ISandboxSystem`, a system Registry, priority,
  jobs, cancellation or runtime deferral queues;
- add Machine, Network, AI or Transport metric slots before those systems
  exist and are independently approved;
- change save v12, resources, recipes, rendering results, input or Gameplay;
- begin A6, B1, C1, D1 or an Extended capability.

## 6. Acceptance

The contract may be marked verified only when:

1. a static A5 gate freezes the four metric identities, fields, budget scopes,
   status vocabulary, developer UI and absence of speculative systems;
2. focused behavior proves exact identity/order, last-tick reset, actor work,
   existing Combat/Random Tick/Population limits and the total status mapping;
3. AL-A1 through AL-A4 static gates remain green and AL-A3 call order is
   unchanged;
4. existing deterministic, actor, combat, random-tick, population, save,
   streaming and Gameplay regressions remain green;
5. VS2017/v141 Debug and Release complete gates pass, including the isolated
   clean package.

AI gameplay remains `NOT_RUN` unless executed separately through the active
package-only protocol. Human fun, aesthetics and physical input feel remain
`NOT_CLAIMED`.
