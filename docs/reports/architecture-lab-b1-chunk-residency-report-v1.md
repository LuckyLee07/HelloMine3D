# B1 Chunk Residency State Machine Report v1

> Execution result: Engineering Done
>
> AI gameplay result: `NOT_RUN`
>
> Human subjective claims: `NOT_CLAIMED`

## Scope and compatibility

B1 implements the frozen
`docs/contracts/chunk-residency-state-machine-contract-v1.md`. It replaces the
old combined/implicit lifecycle with separately owned Data Residency, CPU Mesh
and Ogre Render state machines. It does not implement B2 demand, B3 scheduling,
B4 cancellation, B5 backpressure, B6 spatial interest or B7-B9 Extended work.

Gameplay, resource economy, victory flow, save v12, terrain v4, settings v8,
the AL-A1 78-method World public surface and AL-A2 budgets remain unchanged.

## Implementation

- `ChunkLifecycle.*` freezes 7 Data, 5 Mesh and 4 Render states, stable names
  and exhaustive legal-transition matrices.
- `ChunkManager` advances Data state around real storage/generation/save/unload
  operations. A failed eviction save restores `Resident` and retains dirty
  authoritative data.
- `ChunkSection` and `ChunkRuntime` reuse the existing synchronous queue and
  one background loader for `Dirty -> Queued -> Building -> CpuReady -> Clean`.
  Revision mismatch rejects stale off-lock results.
- Ogre owns a section-keyed Render state map. Uploads become resident only when
  a post-acknowledgement immutable snapshot still identifies the same live
  revision; rejected and unloaded visuals are destroyed.
- The developer panel displays counts for all Data, Mesh and Render states.

## Automated evidence

The focused command is:

```powershell
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "B1"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

It passes `38/38`: 10 new B1 checks, the existing L3 light-only invalidation,
E5 stale-upload, S2.4 unload-persistence and C5 terrain structure generation /
reload coverage. The injected storage failure produces an expected diagnostic
and proves that eviction is cancelled without clearing dirty data. Direct
deterministic Chunk generation also traverses the same frozen request/load/
generate graph instead of bypassing it.

The static command:

```powershell
& .\tools\validate_chunk_residency_state_machine.ps1 `
  -Root (Get-Location).Path
```

passes with `data_states=7 mesh_states=5 render_states=4 owners=3
debug_families=3 post_b1=absent`. The AL-A1 public-map gate still reports 78
methods and the frozen SHA-256
`8B2CDDF30B70DA91D5EF4944D7E1397BC9434EB0E129B9313DA471143F653EC4`.

## Full gate closeout

Required command:

```powershell
& .\scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
```

The 2026-09-01 closeout passes with:

- VS2017/v141 Debug rebuild: `2044 warnings / 0 errors`;
- VS2017/v141 Release rebuild: `2023 warnings / 0 errors`;
- Debug and Release WorldRuntime: `863/863` each;
- ResourcePack: `80/80`; Recipe: `122/122`; startup negatives: `15/15`;
- nominal and stress short soaks: zero failures;
- clean Release package: `104` entries, archive SHA-256
  `F76F5A1429C869FDA94FDD37BF34F8266866B5F665D1E550CA04F15CCD7ECF88`;
- Release `HelloMine3D.exe`: `8,768,512` bytes, SHA-256
  `8AE39931CFE983A09CDC6FE3BDA7183260538A74E470AE407ABAA5ED5C87C3E4`;
- final gate status: `PASS`, `real_window=DEFERRED`.

## Claim boundary

No focus-stealing real window was started. `AI-01..AI-08` remain `NOT_RUN`;
human fun, aesthetics, retention, physical input feel and other subjective
experience remain `NOT_CLAIMED`. B1 completion is an automated engineering
claim only and does not approve B2 or any Extended capability.
