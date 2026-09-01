# B4 World Job Cancellation & Generation Token Report v1

> Execution result: PASS
>
> AI gameplay result: `NOT_RUN`
>
> Human subjective claims: `NOT_CLAIMED`

## Scope and compatibility

B4 implements
`docs/contracts/world-job-cancellation-contract-v1.md`. It extends only
`ChunkLoadOrGenerate` and `ChunkMeshBuild` with a copied generation identity
and cooperative cancellation. It does not add a worker, hard queue capacity,
watermark, admission/shedding policy, Spatial Interest, far representation or
future system job type.

Gameplay, resource economy, victory flow, save v12, terrain v4, settings v8,
the AL-A1 78-method World public surface and the AL-A2/B3 one-worker budgets
remain unchanged.

## Implementation

- `WorldJobScheduler` starts at generation 1, rejects zero/stale requests and
  stale plan replacement, clears obsolete pending jobs on invalidation and
  records one new completion outcome, `Cancelled`.
- `ChunkRuntime` advances generation on frozen semantic demand/config/reset/
  shutdown boundaries. Its World-mutex plus generation/commit mutex makes the
  final token check and authoritative commit one linearized interval.
- `ChunkManager` reserves one `Loading` placeholder, prepares a detached
  storage-or-generation candidate outside the World lock, then commits or
  cancels it under the authoritative boundary. Cancellation uses the sole new
  B1 edge `Loading -> Absent`.
- Detached population suppresses live random-tick index notifications and
  serializes access to the mutable terrain generator. A successful commit
  registers active sections exactly once before publishing Chunk facts.
- Cancelled mesh returns the exact still-current `Building` section to
  `Dirty`; it cannot adopt CPU output or increment mesh rebuild metrics.
- The developer panel exposes current generation, invalidations, pending/
  submit/plan cancellation counters and completed cancellation count.

## Focused evidence

```powershell
& .\tools\validate_world_job_cancellation.ps1 `
  -Root (Get-Location).Path -Implementation
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "B4"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

The static B4 gate passes with `token=uint64`, `outcome=Cancelled`, detached
load, linearized commit and six invalidation call sites. B4 runtime passes
`10/10`; B3, B2 and B1 focused regressions pass `9/9`, `26/26` and `38/38`.
The stress case performs 300 concurrent plan publications and generation
invalidations, finishes without deadlock or residual work and observes no more
than eight pending jobs.

## Full gate closeout

Required command:

```powershell
& .\scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
```

The complete VS2017/v141 gate passes in Debug and Release. The two full
WorldRuntime runs each pass `894/894`; resource-pack, recipe and startup-error
coverage pass `80/80`, `122/122` and `15/15`, and both short soak profiles
finish with zero failures. Debug rebuild completes with 2,044 warnings and
zero errors; Release rebuild completes with 2,023 warnings and zero errors.
The warnings remain the existing Ogre/third-party VS2017 compatibility set.

The Release executable is 8,792,064 bytes with SHA-256
`C79BEFE8C4E06478C15D12A831596BB147C38BBC7BD3F2BAD291409D5803E206`.
The clean-root package contains 104 entries; its 17,053,910-byte ZIP has
SHA-256 `A9A7CC9AF528F3C725ACC13A62A69FB6D10718AD25F7F4796F5E47B70453C33A`.
Hidden validation-only clients, all startup negatives, local crash diagnostics
and executable inventory pass. The final gate result is
`PASS real_window=DEFERRED`; B4 is `Done`.

## Claim boundary

No focus-stealing real window is part of B4. `AI-01..AI-08` remain `NOT_RUN`;
human fun, aesthetics, comfort, retention and physical input feel remain
`NOT_CLAIMED`. B4 does not approve B5-B9, Track C/D or Extended capabilities.
