# B3 Generic World Job Scheduler Report v1

> Execution result: Done
>
> AI gameplay result: `NOT_RUN`
>
> Human subjective claims: `NOT_CLAIMED`

## Scope and compatibility

B3 implements the frozen
`docs/contracts/world-job-scheduler-contract-v1.md`. It routes the two real
background work families, `ChunkLoadOrGenerate` and `ChunkMeshBuild`, through
one typed scheduler. The current storage-load path already performs
deterministic generation fallback atomically, so B3 does not invent separate
IO and generation jobs.

Gameplay, resource economy, victory flow, save v12, terrain v4, settings v8,
the AL-A1 78-method World public surface, B1 state ownership and the existing
loader/mesh/unload budgets remain unchanged. B3 does not implement B4
cancellation, B5 backpressure, B6 Spatial Interest or B7-B9 Extended work.

## Implementation

- `WorldJobScheduler` owns deterministic pending order, one in-flight slot,
  completed records, monotonic ids and copied diagnostics.
- Pending order preserves B2 policy: priority, plan order, demand epoch, real
  job type and id. Duplicate pending `(type,target)` keys are rejected.
- `ChunkRuntime` replaces only pending work when demand changes. It executes
  load/generate under the existing World mutex, builds copied mesh input
  off-lock and commits under the same mutex through the B1 revision guard.
- `ChunkManager::prepareChunkNeighborhood` exposes the existing one-load-at-a-
  time compatibility step without moving Chunk ownership into the scheduler.
- The developer panel exposes queue state, lifecycle/outcome/type totals and
  last queue/worker/commit timings.

## Focused evidence

```powershell
& .\tools\validate_world_job_scheduler.ps1 -Root (Get-Location).Path
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "B3"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

The static gate passes with `types=2 states=3 outcomes=3 workers=1` and exact
`priority/plan/epoch/type/id` ordering. The focused runtime passes `9/9`,
covering vocabulary, duplicate rejection, ordering, legal lifecycle, invalid
completion rejection, pending replacement with in-flight preservation,
completion values, diagnostics and execution of both types by the real World
pipeline. B2 remains `26/26` and B1 remains `38/38` in the same Debug binary.

## Full gate closeout

Required command:

```powershell
& .\scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
```

The 2026-09-01 closeout passes with:

- VS2017/v141 Debug rebuild: `2044 warnings / 0 errors`;
- VS2017/v141 Release rebuild: `2023 warnings / 0 errors`;
- Debug and Release WorldRuntime: `884/884` each;
- ResourcePack: `80/80`; Recipe: `122/122`; startup negatives: `15/15`;
- nominal and stress short soaks: zero failures;
- clean Release package: `104` entries, archive SHA-256
  `B983889B5553FF0DBFEAF6C14D2E349CC81AD1205C314CC39C0894C2D1CD9459`;
- Release `HelloMine3D.exe`: `8,785,920` bytes, SHA-256
  `CAA02BF2F507BB686C469DFF6AD3B58E1B6D8065E45E84A1E11533A0E943A844`;
- final gate status: `PASS`, `real_window=DEFERRED`.

## Claim boundary

No focus-stealing real window is part of B3. `AI-01..AI-08` remain `NOT_RUN`;
human fun, aesthetics, comfort, retention and physical input feel remain
`NOT_CLAIMED`. B3 is an automated engineering claim and does not approve B4-B9,
Track C/D or Extended capabilities.
