# Runtime Validation

This document describes how sandbox foundation behaviour is validated at
runtime, without a human driving the client window.

Three layers exist. Together they are the acceptance gate for the S0-S7
milestones in `docs/sandbox-foundation-todolist.md`.

| Layer | Target | Answers |
| ----- | ------ | ------- |
| Focused headless tests | `HelloMine3DCoordinateTests`, `HelloMine3DMeshDirtyTests`, `HelloMine3DSaveLoadSmoke`, `HelloMine3DEntityLifecycleSmoke` | Do the isolated algorithms behave? |
| World runtime smoke | `HelloMine3DWorldRuntimeSmoke` | Does the real `World` / `ChunkManager` / `WorldManager` / actor stack behave? |
| Client smokes | `tools/run_render_capture.ps1`, `tools/run_perf_baseline.ps1` | Does the assembled client still render and stay within its frame budget? |

## World Runtime Smoke

`src/HelloMine3D/Tests/WorldRuntimeSmokeMain.cpp` links the whole game runtime
except `Main.cpp` and drives the real gameplay classes through scripted
scenarios. It no longer creates an SFML or OpenGL context: texture-atlas
ownership lives in `RenderMaster`, mesh UV calculation is pure data, and the
`World/` data layer has no direct SFML or OpenGL types.

```powershell
bin\HelloMine3DWorldRuntimeSmoke.exe
```

Output is one line per assertion plus a summary:

```text
[VALIDATION] PASS S0.5/chunk-boundary-marks-neighbor
[VALIDATION] FAIL S6.4/ore-decorator-produces-ore :: coal=0 iron=0
[VALIDATION] checks=89 failures=0
[VALIDATION] status=PASS
```

Check ids are `<milestone>/<behaviour>`, so a failure points straight at the
milestone that regressed. The process exits non-zero when any check fails.

### Isolation

Every scenario runs against a fresh directory under `bin/validation_runs/`,
so a validation run never reads or writes `bin/saves/`. Terrain seed, player
position and player rotation are forced per scenario through the same
`HELLOMINE3D_*` environment variables the capture scripts use.

### Coverage

| Scenario | Milestones | What it proves |
| -------- | ---------- | -------------- |
| `caseDebugPanelStartupOption` | V3, S7.1 | False/true environment values are parsed consistently and `HELLOMINE3D_SHOW_DEBUG_INFO` controls the initial panel state before capture begins. |
| `caseFixedTickScheduler` | V1, S1.5 | The runtime scheduler emits exactly 200 ticks over 10 seconds and bounds catch-up work to five ticks per frame. |
| `caseBlockTextureCoordinates` | E0 | Atlas UV generation is stable and does not require a graphics context. |
| `caseBlockSelection` | P3 | One ray result identifies the solid target and adjacent placement voxel, skips water, respects reach and reports an empty miss. |
| `casePlayerControllerInput` | V2, S5.3 | Synthetic input drives movement, flying jump, fly/sneak toggles, camera rotation and hotbar selection through the live controller application path. |
| `caseHeightMapEdits` | V4 | The cached column height matches a brute-force scan after generation, breaking the highest opaque block, and placing a block above the old top. |
| `caseBackgroundLoaderStress` | V5, S0.3 | During 240 load-center changes, concurrent block reads stay valid, chunk/section counters remain internally consistent, and the worker resumes mesh progress at a new stable center. |
| `caseSpawnPreload` | S0.6 | Spawn preloading uses chunk coordinates and produces a full 3x3 loaded neighbourhood on solid ground. |
| `caseNegativeCoordinates` | S0.1 | Negative and cross-zero world coordinates map to the right chunk and local block, both on write and read back. |
| `caseNoImplicitChunkCreation` | S0.2 | Reading or writing a block outside the loaded set does not create chunks. |
| `caseMeshDirtyPropagation` | S0.4, S0.5 | Interior edits dirty only the owning section; chunk-boundary and section-boundary edits also dirty the correct neighbour. Debug stats report the state. |
| `casePersistence` | S1.3, S2.1, S2.4, S2.5, S2.6, S6.1 | Block edits, world metadata, seed, spawn point, player transform and inventory all survive a relaunch. |
| `caseUnloadPersistence` | S2.4 | Unloading a chunk flushes it to storage first, and reloading restores the edit instead of regenerating. |
| `caseChunkFormatRejection` | S2.2, S2.3 | Chunk file paths are deterministic, and a corrupted magic is rejected with a diagnostic rather than loaded as garbage. |
| `caseTerrainDeterminism` | S6.1, S6.4 | The same seed produces identical terrain over 175k sampled blocks, and ore layout is stable. |
| `caseTerrainStructures` | S6.2, S6.3, S6.5 | Tree and plant decorators run, biomes produce varied surfaces, and structures are identical after a save/reload roundtrip. |
| `caseInteractionAndEvents` | S3.1, S3.3, S3.4, S3.5, S4.2, S4.5 | Break/place go through the interaction system, produce configured drops, consume items, publish events, and block metadata survives a roundtrip. |
| `caseChunkEvents` | S4.3 | Generate, load, save and unload each publish their chunk event. |
| `caseActors` | S4.4, S5.1, S5.2, S5.5, S5.6 | Mobs spawn, wander, take damage, die and get culled; item entities spawn and are picked up, publishing the matching events. |
| `caseWorldManager` | S1.2, S1.4, S1.5, S4.5 | World creation, lookup, save, load, same-world teleport, cross-world rejection and world-time advance. |

### Not covered

These need a person at the keyboard or a different harness:

| Gap | Why |
| --- | --- |
| Sandbox ImGui debug panel (S7.1) | `run_render_capture.ps1 -ShowDebugInfo` now enables it before frame one and the data source is asserted. The 2026-08-09 session exposed only GDI Generic OpenGL 1.1, so the rendered panel still needs a hardware-backed screenshot. |
| Selected-block outline (P3) | Picking and interaction share five passing headless assertions, and the line renderer builds in both configurations. The 2026-08-09 capture attempt again exposed only GDI Generic OpenGL 1.1, so the yellow outline still needs a hardware-backed screenshot. |
| Formal data-race detection for the background loader (S0.3) | The V5 stress scenario exercises the real worker and covers the formerly unlocked chunk-map read, but MSVC still provides no ThreadSanitizer proof. |

## Current Verified Runs

| Layer | Command | Result |
| ----- | ------- | ------ |
| Focused headless tests | `bin\HelloMine3DCoordinateTests.exe`, `bin\HelloMine3DMeshDirtyTests.exe`, `bin\HelloMine3DSaveLoadSmoke.exe`, `bin\HelloMine3DEntityLifecycleSmoke.exe` | All pass. |
| World runtime smoke | `bin\HelloMine3DWorldRuntimeSmoke.exe` | `checks=119 failures=0` (Debug and Release, 2026-08-09) |
| E0 dependency boundary | `rg "SFML|sf::|GLfloat|GLuint|glad" src/HelloMine3D/World` | No matches (2026-08-09). |
| Render smoke | see `docs/render-regression-smoke.md` | `bin/render_capture_20260807190230074-46036`, status PASS |
| Performance baseline | see `docs/performance-baseline.md` | `bin/perf_baseline_20260807190255313-41064`, `frame_p95_ms=16.430` |

The 2026-08-09 automation session exposed only the Windows GDI Generic OpenGL
1.1 implementation, below the client's required OpenGL 3.3 context. Headless
validation is therefore fully current, while the last hardware-backed render
and performance runs remain the 2026-08-07 records above.

## When To Run

Run the full set before closing any milestone. Run the world runtime smoke
after any change to chunk addressing, chunk storage, block interaction, the
event bus, actors, or terrain generation.
