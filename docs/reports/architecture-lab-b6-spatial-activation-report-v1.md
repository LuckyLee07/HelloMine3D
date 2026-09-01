# B6 Spatial Activation Report v1

> Execution result: `PASS`
>
> AI gameplay result: `NOT_RUN`
>
> Human subjective claims: `NOT_CLAIMED`

## Scope and compatibility

B6 implements `docs/contracts/spatial-activation-contract-v1.md` on top of the
real B2 demand and B3-B5 streaming pipeline. It distinguishes Resident Data,
Near Representation and Simulation Requested without adding Far data/rendering,
simulation fidelity modes, another worker or a future job family.

Gameplay, resource economy, victory flow, save v12, terrain v4, settings v8,
the AL-A1 78-method World public surface and all B5 caps/watermarks/budgets
remain unchanged.

## Implementation

- `SpatialInterestModel` deterministically expands the immutable B2 demand
  snapshot into sorted cells with nested resident/near/simulation flags and the
  merged reason mask.
- Player and Camera request near representation; Player requests simulation
  within Chebyshev radius two; TeleportDestination and Preload remain resident-
  only.
- `ChunkRuntime` refreshes the snapshot on semantic demand revisions, admits
  resident work, stops resident-only paths before mesh follow-up, filters copied
  mesh publication/acknowledgement by Near interest, and protects Resident cells
  from distant unload.
- `requestsSimulation` is copied to diagnostics only. Existing fixed-tick work
  does not consume it.

## Focused evidence

```powershell
& .\tools\validate_spatial_activation.ps1 `
  -Root (Get-Location).Path -Implementation
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "B6"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

The static gate passes four classifications, radius-two Player simulation
interest, the four real demand policies, copied diagnostics, one loader, B5's
unchanged `8/8/8` consumer budgets and the absence of B7-B9/D2 behavior. The
VS2017/v141 Debug focused runtime passes `12/12`. The composed B5/B4/B3/B2/B1
static gates also pass with B6 as the only admitted extension.

## Full gate closeout

Executed command:

```powershell
& .\scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
```

The complete VS2017/v141 gate passes in both configurations. Debug rebuild
finishes with `2044 warnings / 0 errors` in `1:37.65`; Release rebuild finishes
with `2023 warnings / 0 errors` in `2:23.95`. `WorldRuntime` passes `918/918`
twice, Resource Pack `80/80`, Recipe `122/122`, startup negatives `15/15`, both
short soaks with zero failures, and both validation-only clients. The Release
executable is `8,809,984` bytes with SHA-256
`D86A532AA3673011A8CBF1993DB48EF613B9922911324D34836A7CF75FA476B6`.

The isolated package contains 104 entries. Its ZIP is `16,775,367` bytes with
SHA-256
`C8E260E00CF76C952150EBC3DC851A7EDE5E13FE63A58F98B77DC103723EFA3C`.
The final gate result is `PASS real_window=DEFERRED`; B6 is `Done`.

## Claim boundary

No focus-stealing real window is part of B6. `AI-01..AI-08` remain `NOT_RUN`;
human fun, aesthetics, comfort, retention and physical input feel remain
`NOT_CLAIMED`. This batch does not approve B7-B9, Track C/D or Extended
capabilities.
