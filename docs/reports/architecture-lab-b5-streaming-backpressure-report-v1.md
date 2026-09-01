# B5 Streaming Backpressure Report v1

> Execution result: `PASS`
>
> AI gameplay result: `NOT_RUN`
>
> Human subjective claims: `NOT_CLAIMED`

## Scope and compatibility

B5 implements `docs/contracts/streaming-backpressure-contract-v1.md` for the
two real B3 job types and the existing mesh/upload/unload consumers. It does
not add another worker, a main-thread completion dispatcher, Spatial Interest,
Far representation, a generic scheduler or a future job family.

Gameplay, resource economy, victory flow, save v12, terrain v4, settings v8,
the AL-A1 78-method World public surface and the B3/B4 lifecycle/order/
generation boundaries remain unchanged.

## Implementation

- `WorldJobScheduler` freezes a 128 shared/per-type pending cap, 96/48
  high/low watermarks, `Normal/Elevated/Saturated` pressure and five explicit
  admission outcomes. A strictly higher B3-ranked request may shed the one
  deterministic worst pending request; in-flight work is never shed.
- `ChunkRuntime` retains the immutable current B2-derived plan and admits only
  a 96-job window. The one loader refills from 48 toward 96, publishes real
  follow-up work before refill and clears deferred plan state on B4 generation
  invalidation.
- Each loader pass enters at most eight authoritative commit intervals.
  CPU-ready section publication deterministically offers at most eight meshes
  per render frame using the Player demand origin. Distant unload retains its
  existing eight-per-update eligibility and now exposes truthful backlog.
- Copied scheduler/runtime metrics and the Ogre developer panel expose pending
  type/peak/limit values, pressure/admission/shedding, deferred demand-plan
  work and all three consumer budgets without affecting Gameplay.

## Focused evidence

```powershell
& .\tools\validate_streaming_backpressure.ps1 `
  -Root (Get-Location).Path -Implementation
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "B5"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

The B5 static gate passes with caps `128/128/128`, watermarks `96/48`, consumer
budgets `8/8/8` and exactly one loader declaration. The focused runtime passes
`12/12`: vocabulary, below-watermark admission, duplicate/stale separation,
hard-cap rejection, deterministic shedding, pressure hysteresis, invalidation,
oversized-plan refill, live RD8 bounding, commit budget, upload choice and
unload backlog. B4/B3/B2/B1 focused regressions pass `10/10`, `9/9`, `26/26`
and `38/38`.

## Full gate closeout

Executed command:

```powershell
& .\scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
```

The complete VS2017/v141 gate passes in both configurations. Debug rebuild
finishes with `2044 warnings / 0 errors` in `1:39.73`; Release rebuild finishes
with `2023 warnings / 0 errors` in `2:18.97`. `WorldRuntime` passes `906/906`
twice, Resource Pack `80/80`, Recipe `122/122`, startup negatives `15/15`,
both short soaks with zero failures, and both validation-only clients. The
Release executable is `8,801,280` bytes with SHA-256
`3377D89FF0E33D53142FA7F0F3405B8955A6B095C8E6FFEB29FB0AB74E6F1204`.

The isolated package contains 104 entries. Its ZIP is `16,770,390` bytes with
SHA-256
`7D126B31B78F3A4E8F8C90A5D769028EC686C0D4F708D1F6B2E2979BD164050B`.
The final gate result is `PASS real_window=DEFERRED`; B5 is `Done`.

## Claim boundary

No focus-stealing real window is part of B5. `AI-01..AI-08` remain `NOT_RUN`;
human fun, aesthetics, comfort, retention and physical input feel remain
`NOT_CLAIMED`. B5 completion satisfies the approved entry condition for B6,
but never approves B7-B9, Track C/D or Extended capabilities.
