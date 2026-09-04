# B10 Large World Stress & Acceptance Contract v1

> Status: Frozen after B10 verification
>
> Baseline: local commit `ccb98ce` (B6 Spatial Activation)
>
> Workflow state: `Done`

## Goal

B10 closes Track B Core with reproducible, hidden, headless evidence over the
real `World`, loader worker, Chunk state machines, demand, scheduler,
cancellation, backpressure, spatial-interest, mesh publication and persistence
paths. It adds a test schedule and acceptance tooling; it does not add another
runtime abstraction or change Gameplay.

If the long run exposes a violation of an already-frozen B1-B6 invariant, B10
may repair that defect inside the existing production boundary and must retain
the failed first run as evidence. Such a repair is not permission to add a new
job family, worker, representation tier or gameplay feature.

## Frozen Core schedule

`HelloMine3DSoak.exe --profile track-b-core` is schedule version 3. A formal
Release x64 run lasts exactly 1,800 requested seconds in one process and one
save root:

| Phase | Duration | Normal-input equivalent | Required observation |
| ----- | -------- | ----------------------- | -------------------- |
| LW1 Straight Run | 600 s | Move one Chunk forward every two seconds. | New terrain keeps arriving while old residency stays bounded. |
| LW2 Teleport Storm | 300 s | Deterministic far instant moves every two seconds. | Generation invalidation, cancellation and stale-result counters stay explainable and bounded. |
| LW3 Turnaround | 300 s | Alternate between opposite destinations every two seconds. | Pending replacement does not deadlock or commit obsolete work. |
| LW4 Render Distance Churn | 300 s | Cycle `1/2/4/8` every five seconds while moving. | B5 caps and B6 hierarchy hold during demand expansion/contraction. |
| LW5 Edit & Leave | 300 s | Edit/save, publish a far demand, reopen at the edit ten times. | The edited block, Actor and item survive every completed reopen check. |

Short developer probes scale the same five phase proportions to their requested
duration. The 300-second developer schedule and the formal schedule both run
ten LW5 persistence checks; a shorter probe runs the checks that fit its scaled
LW5 interval. Persistence coverage is bounded by check count rather than a
five-second cadence so a long run does not spend its wall-clock budget copying
an ever-growing backup set. They are not formal stability evidence.

## Frozen invariants and metrics

The existing Q3 world/process budgets remain authoritative: existing Chunk
count `<=1024`, dirty-update queue `<=4096`, Actor count `<=32`, peak private
bytes `<=2 GiB`, peak handles `<=4096`, post-warmup private growth `<=256 MiB`
and handle growth `<=128`. A formal process must complete all 36,000 fixed ticks,
must not end early, crash, time out or report a failed save/reopen.

Track B adds these exact checks on every one-second world sample:

- scheduler pending total/generation/mesh each remains `<=128`, with total equal
  to the two typed counts;
- the retained single loader means in-flight and unconsumed completion counts
  are each `<=1`;
- authoritative commits, upload offers and unloads remain `<=8/8/8`;
- `SimulationRequested <= NearRepresentation <= ResidentData == total cells`;
- queue, worker and authoritative commit time remain separately reported;
- cancellation, commit rejection, stale submit/plan and generation
  invalidation counters remain visible even when a valid run records zero.
- all seven Data Residency counts sum to the manager entry count on every
  sample, and retained `Absent` entries remain exactly zero.

The B1/B5 lifecycle rules apply before the unload budget is consumed: a
cancelled detached load reaches semantic `Absent` and its manager reservation
is erased; only `Resident` Chunks are eligible for the eight-slot unload
selection. `lastUnloads` counts completed removals, while a failed eligible
removal keeps backlog truthful. Loading/Generating/Absent entries may not
repeatedly occupy the bounded slots and starve Resident data behind them.
Mesh-neighbour observation is a Query: an unavailable adjacent Chunk is
treated as the existing non-solid/air boundary and must not call
`getOrCreateChunk` or retain an `Absent` entry in the manager map.

## Determinism evidence

Two isolated short Release probes use the same seed and duration. They must both
pass and match exactly on schedule version, fixed ticks, five phase tick/action
counts, deterministic action digest and verified persistence digest. Async
queue depths, wall time and memory are intentionally excluded from equality:
comparing them byte-for-byte would turn OS scheduling noise into a false
determinism claim.

## Acceptance commands

During implementation:

```powershell
& .\tools\validate_large_world_stress_acceptance.ps1 -Implementation
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "B10"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
& .\tools\run_large_world_stress_acceptance.ps1 `
  -DurationSeconds 10 -ProbeDurationSeconds 10
```

Final B10 evidence requires:

```powershell
& .\scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
& .\tools\capture_release_candidate_performance.ps1 `
  -ClientSelection fast-streaming
& .\tools\run_large_world_stress_acceptance.ps1 -Formal
& .\tools\validate_large_world_stress_acceptance.ps1 -Evidence
```

The fast-streaming fixture identity is `rc-ring-12-chunks-v2`. Its scripted
instant moves must call the same `WorldManager::teleportPlayer` boundary as a
successful in-game same-world teleport. The retired v1 fixture directly moved
the Player and then published ordinary `Preload`; after B6 that resident-only
reason correctly stopped short of near representation and therefore no longer
described the advertised teleport path. This fixture correction does not
relax the existing 1,000 ms Chunk-visible P95 budget or add a performance
exception.

The full gate must produce and validate a clean isolated Windows package. The
formal B10 evidence is stored as summaries under
`docs/baselines/architecture-lab-b10-windows-hidden-v1/`; raw saves, CSV files
and process logs remain ignored under `bin/soak_runs/`. A failed first formal
attempt is summarized separately as `b10-first-failure.summary.txt`; it must
not be overwritten by the passing rerun and must record whether any threshold
or performance exception changed.

## Compatibility and claim boundary

Gameplay, recipes, resource economy, victory flow, World public API, save v12,
terrain v4, settings v8, one loader worker and all B1-B6 behavior stay
unchanged. B7-B9, Far Terrain, render/simulation split, simulation LOD, Track
C/D, new job families and extra workers are non-goals.

No focus-stealing real window is required. Hidden Q1 clients are engineering
performance evidence, not gameplay acceptance. `AI-01..AI-08` remain `NOT_RUN`;
human fun, aesthetics, retention, comfort and physical input feel remain
`NOT_CLAIMED`.
