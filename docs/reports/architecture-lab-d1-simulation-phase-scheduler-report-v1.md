# Architecture Lab D1 Simulation Phase Scheduler Report v1

> Status: Done
>
> Batch: `D1 Simulation Phase Scheduler v0`
>
> Date: 2026-09-04

## 1. Entry evidence and authority

The project owner separately approved D1 after C3. The implementation audit
proved three current fixed-tick admission problems rather than relying on the
roadmap alone: ActorManager ticked every managed actor, Random Tick owned an
embedded four-Section rotation, and Furnace/Crusher each scanned every loaded
machine. This satisfied the task-ledger rule requiring at least three real
workloads before runtime scheduling could begin.

The approved boundary excludes D2-D8, C4-C11, Network simulation, a generic
Registry/`ISandboxSystem`, wall-clock budgets, save migration and new content.

## 2. Concrete runtime result

`SimulationPhaseScheduler` has exactly three workload identities and no empty
future slots:

| Workload | Budget | Stable-set order and fairness |
| -------- | ------ | ----------------------------- |
| Managed Actors | 64 per tick | ActorManager insertion order with a rotating index |
| Random-Tick Sections | existing 4 per tick | existing World FIFO owns ordering |
| Furnace/Crusher Block Entities | 32 per tick | Furnace first, Crusher second, then X/Y/Z with a rotating index |

Each plan copies eligible, admitted, deferred, budget, first index and
`ceil(N/B)` service-window ticks. PlayerActor, player cooldowns and the other
AL-A3 phase barriers remain mandatory. Below budget, Actor insertion order and
each admitted object's one-tick result are unchanged. Block Entities replace
the old unordered-Chunk traversal with the documented canonical order needed
for reproducible round-robin service. Overloaded work performs no step until
its next turn and never receives a multiplied `dt` catch-up.

## 3. Ownership, diagnostics and compatibility

WorldSimulation remains the concrete fixed-tick coordinator. ActorManager owns
actor invocation, World owns the Random Tick FIFO, and Furnace/Crusher retain
their C2 machine state and completion effects through one-item adapters. The
scheduler stores only two transient rotating indices and retains no domain
pointer.

The completed-tick snapshot publishes three copied plans. A5 diagnostics now
have one additional real Block Entity row plus copied eligible, service-window
and scheduler-managed fields. The developer Simulation panel shows both plan
and phase values. No diagnostic value feeds back from wall time into Gameplay.

World save v12, Chunk v2, terrain v4, settings v8, Furnace payload v1 and
Crusher payload v1 remain unchanged. Scheduler cursors/plans are not persisted;
C3 topology remains derived and has no Network tick slot.

## 4. Focused and compatibility evidence

The new static gate passes:

```powershell
& .\tools\validate_simulation_phase_scheduler.ps1 `
  -Root (Get-Location).Path
```

It reports three workloads, 64/4/32 budgets, round-robin/FIFO fairness, no
wall-clock admission, save v12 and no D2+ runtime vocabulary. The composed
AL-A3, AL-A5 and C2 static gates also pass.

VS2017/v141 Debug builds the real WorldRuntime target. Focused results are:

| Focus | Result |
| ----- | ------ |
| `D1-SCHEDULER` | `24/24`; includes 10 D1 assertions plus retained A3/A5 behavior |
| `AL-A5` | `14/14` |
| `B6` | `12/12` |
| `C2-MACHINE` | `51/51` |
| `C3-TOPOLOGY` | `68/68` |

The overload fixture drives 66 real Item Actors and 34 powered Crushers. The
last two of each set do not advance on tick one, then advance exactly one
fixed step in tick two. This proves real deferral and next-window service, not
only scheduler arithmetic.

## 5. Complete-gate evidence and identity

The complete `scripts\verify_build.ps1 -VisualStudioVersion 2017
-SkipRealWindow` gate passes with zero build errors in both Debug and Release.
Both configurations pass WorldRuntime `991/991`; Recipe/economy remains
`126/126`, Resource Pack remains `80/80`, startup negatives remain `15/15`,
and both short soaks and hidden validation clients pass.

The Release executable is 8,865,280 bytes with SHA-256
`82D9EC9C831B932A588B9DC9F24EA85DCF43CC47B61EA938489DCA750EA7236F`.
The isolated Windows package contains 105 inventory entries and its archive
SHA-256 is
`0B34CD34265ED1A4F88FD5833975FD328FB026FCD6B13A0FAFE9710859F1B2F6`.
The final gate result is `PASS real_window=DEFERRED`.

## 6. Evidence boundary

The complete gate used `-SkipRealWindow`, so no real-window PASS is claimed by
this batch. `AI-01..AI-08` remain `NOT_RUN`; human fun,
aesthetics, comfort, retention and physical-input feel remain `NOT_CLAIMED`.
Headless overload evidence proves deterministic engineering behavior only.

## 7. Exit and next authority

D1 is `Done`: implementation, focused evidence, the dual-configuration gate,
documentation and an independent local commit close the approved boundary.
D1 does not approve D2-D8, C4-C11, generic scheduling interfaces, simulation
LOD or offline catch-up. D2 remains only the next candidate and still requires
separate problem evidence and owner approval.
