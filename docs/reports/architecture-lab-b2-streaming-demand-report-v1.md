# B2 Streaming Demand Model Report v1

> Execution result: Done
>
> AI gameplay result: `NOT_RUN`
>
> Human subjective claims: `NOT_CLAIMED`

## Scope and compatibility

B2 implements the frozen
`docs/contracts/streaming-demand-model-contract-v1.md`. It replaces the single
implicit loading center with four bounded demand reasons: Player, Camera,
TeleportDestination and Preload. It does not implement B3 scheduling, B4
cancellation, B5 backpressure, B6 spatial interest or B7-B9 Extended work.

Gameplay, resource economy, victory flow, save v12, terrain v4, settings v8,
the AL-A1 78-method World public surface, the B1 state owners and the existing
loader/mesh/unload budgets remain unchanged.

## Implementation

- `ChunkDemand.*` owns four optional reason slots, stable priority/lifetime
  policy, monotonic epochs/revisions and exact expiry.
- `ChunkRuntime` expands active demand radii, de-duplicates coordinates and
  sorts them by reason priority, frustum, movement direction, age, distance and
  stable coordinates before feeding the existing single loader.
- `World::update` publishes Player and Camera demand. Existing preload remains
  synchronous while also publishing Preload demand.
- `WorldManager` publishes TeleportDestination only for a successful same-world
  teleport through a private World bridge; rejection does not mutate demand.
- The developer panel shows epoch, revision, active/reason counts, expired
  count and the last de-duplicated plan size.

## Automated evidence

The focused commands are:

```powershell
& .\tools\validate_streaming_demand_model.ps1 `
  -Root (Get-Location).Path
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "B2"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

The static gate passes with `reasons=4 slots=4
priorities=400/300/200/100 expiry=epoch merge=deduplicated post_b2=absent`.
The focused runtime passes `26/26`, including twelve B2 checks for policy,
bounded refresh/replacement, overlap merge, teleport priority, forward motion,
camera-turn replanning, exact expiry and World/WorldManager integration.

The AL-A1 public map still reports 78 methods and frozen SHA-256
`8B2CDDF30B70DA91D5EF4944D7E1397BC9434EB0E129B9313DA471143F653EC4`.
The B1 gate still reports `data_states=7 mesh_states=5 render_states=4`.

## Full gate closeout

Required command:

```powershell
& .\scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
```

The 2026-09-01 closeout passes with:

- VS2017/v141 Debug rebuild: `2044 warnings / 0 errors`;
- VS2017/v141 Release rebuild: `2023 warnings / 0 errors`;
- Debug and Release WorldRuntime: `875/875` each;
- ResourcePack: `80/80`; Recipe: `122/122`; startup negatives: `15/15`;
- nominal and stress short soaks: zero failures;
- clean Release package: `104` entries, archive SHA-256
  `9955E51AD94C7E5600325329031432E16C1859F936FA880F68F65F3B80369503`;
- Release `HelloMine3D.exe`: `8,772,608` bytes, SHA-256
  `DDDD01A00D9640298E0C2675F15DC389178553BFAED3A92D1A303FCF7397A61F`;
- final gate status: `PASS`, `real_window=DEFERRED`.

## Claim boundary

No focus-stealing real window is part of this batch. `AI-01..AI-08` remain
`NOT_RUN`; human fun, aesthetics, retention, physical input feel and other
subjective experience remain `NOT_CLAIMED`. B2 completion is an automated
engineering claim only and does not approve B3 or any Extended capability.
