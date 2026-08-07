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
except `Main.cpp`, creates an offscreen `sf::Context`, and drives the real
gameplay classes through scripted scenarios.

The offscreen context is required because `BlockDatabase` builds a texture
atlas the first time it is used. Nothing is rendered; the context only exists
so block data can load.

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
| `PlayerController` keyboard/mouse input (S5.3) | Needs a real `sf::Window`; the capture scripts deliberately disable input so they do not warp the mouse. |
| `SandboxRuntime` 20Hz accumulator (S1.5) | The accumulator lives behind `sf::Window`. `WorldManager` world-time advance is asserted instead. |
| Sandbox ImGui debug panel (S7.1) | The panel is behind the F1 toggle. Its data source, `World::collectDebugStats()`, is asserted, but the rendered panel is not. |
| Thread-safety of the background chunk loader (S0.3) | No data-race tooling on this MSVC toolchain. The client smokes exercise the loader, but cannot prove race freedom. |

## Current Verified Runs

| Layer | Command | Result |
| ----- | ------- | ------ |
| Focused headless tests | `bin\HelloMine3DCoordinateTests.exe`, `bin\HelloMine3DMeshDirtyTests.exe`, `bin\HelloMine3DSaveLoadSmoke.exe`, `bin\HelloMine3DEntityLifecycleSmoke.exe` | All pass. |
| World runtime smoke | `bin\HelloMine3DWorldRuntimeSmoke.exe` | `checks=89 failures=0` |
| Render smoke | see `docs/render-regression-smoke.md` | `bin/render_capture_20260807190230074-46036`, status PASS |
| Performance baseline | see `docs/performance-baseline.md` | `bin/perf_baseline_20260807190255313-41064`, `frame_p95_ms=16.430` |

## When To Run

Run the full set before closing any milestone. Run the world runtime smoke
after any change to chunk addressing, chunk storage, block interaction, the
event bus, actors, or terrain generation.
