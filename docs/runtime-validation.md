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
except the Ogre client shell and drives the real gameplay classes through
scripted scenarios. It creates no window or graphics context: FreeImage checks
the packed atlas, mesh UV calculation is pure data, and the `World/` data layer
has no renderer types.

```powershell
bin\HelloMine3DWorldRuntimeSmoke.exe
```

Output is one line per assertion plus a summary:

```text
[VALIDATION] PASS S0.5/chunk-boundary-marks-neighbor
[VALIDATION] FAIL S6.4/ore-decorator-produces-ore :: coal=0 iron=0
[VALIDATION] checks=177 failures=0
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
| `caseOreTextures` | P4 | Coal and iron definitions use dedicated atlas slots, the packed atlas loads at 256x256, and both tile hashes differ from stone and each other. |
| `caseBlockDataDiagnostics` | A2 | Isolated malformed definitions prove missing keys, invalid enums, out-of-range atlas coordinates and duplicate ids name both the exact file and offending key. |
| `caseBlockSelection` | P3 | One ray result identifies the solid target and adjacent placement voxel, skips water, respects reach and reports an empty miss. |
| `casePlayerControllerInput` | V2, S5.3 | Synthetic input drives movement, flying jump, fly/sneak toggles, camera rotation and hotbar selection through the live controller application path. |
| `caseHeightMapEdits` | V4 | The cached column height matches a brute-force scan after generation, breaking the highest opaque block, and placing a block above the old top. |
| `caseBackgroundLoaderStress` | V5, S0.3 | During 240 load-center changes, concurrent block reads stay valid, chunk/section counters remain internally consistent, and the worker resumes mesh progress at a new stable center. |
| `caseSpawnPreload` | S0.6 | Spawn preloading uses chunk coordinates and produces a full 3x3 loaded neighbourhood on solid ground. |
| `caseNegativeCoordinates` | S0.1 | Negative and cross-zero world coordinates map to the right chunk and local block, both on write and read back. |
| `caseNoImplicitChunkCreation` | S0.2 | Reading or writing a block outside the loaded set does not create chunks. |
| `caseMeshDirtyPropagation` | M2, S0.4, S0.5 | Interior and boundary edits dirty the correct sections. Five queued sections are deduplicated, rebuilt FIFO with a two-per-update budget, and drained across three frames. |
| `casePersistence` | P2, S1.3, S2.1, S2.4, S2.5, S2.6, S6.1 | Block edits, world metadata, seed, spawn point, player transform/inventory, mobs and item entities all survive a relaunch. Actor subtype state and id allocation are compared after restore. |
| `caseSectionMeshInput` | M1, M3 | The 18x18x18 block halo matches direct world reads, edits advance the section revision, and accepted mesh builds accumulate timing plus solid/water/flora face and vertex metrics. |
| `caseSectionMeshUploadSnapshot` | E5 | Ogre-facing CPU mesh snapshots include live section identity and block revision; stale upload acknowledgements cannot overwrite a newer edit. |
| `caseUnloadPersistence` | S2.4 | Unloading a chunk flushes it to storage first, and reloading restores the edit instead of regenerating. |
| `caseChunkFormatRejection` | S2.2, S2.3 | Chunk file paths are deterministic, and a corrupted magic is rejected with a diagnostic rather than loaded as garbage. |
| `caseTerrainDeterminism` | S6.1, S6.4 | The same seed produces identical terrain over 175k sampled blocks, and ore layout is stable. |
| `caseTerrainStructures` | S6.2, S6.3, S6.5 | Tree and plant decorators run, biomes produce varied surfaces, and structures are identical after a save/reload roundtrip. |
| `caseInteractionAndEvents` | S3.1, S3.3, S3.4, S3.5, S4.2, S4.5, P5 | Break/place/use go through the interaction system, produce configured drops, consume items, publish events with the target identity, and block metadata survives a roundtrip. |
| `caseChunkEvents` | S4.3 | Generate, load, save and unload each publish their chunk event. |
| `caseActors` | P1, S4.4, S5.1, S5.2, S5.5, S5.6 | Mobs spawn, wander, take damage, die and get culled; item entities spawn and are picked up, publishing the matching events. Immutable actor snapshots track transforms and omit dead or picked-up ids. |
| `caseWorldManager` | S1.2, S1.4, S1.5, S4.5 | World creation, lookup, save, load, same-world teleport, cross-world rejection and world-time advance. |

### Not covered

These need a person at the keyboard or a different harness:

| Gap | Why |
| --- | --- |
| Sandbox ImGui debug panel (S7.1) | `run_render_capture.ps1 -ShowDebugInfo` now enables it before frame one and the data source is asserted. The 2026-08-09 session exposed only GDI Generic OpenGL 1.1, so the rendered panel still needs a hardware-backed screenshot. |
| Selected-block outline (P3) | Picking and interaction share five passing headless assertions, and the line renderer builds in both configurations. The 2026-08-09 capture attempt again exposed only GDI Generic OpenGL 1.1, so the yellow outline still needs a hardware-backed screenshot. |
| Ore texture render (P4) | Definitions, atlas dimensions and tile identities have four passing headless assertions, and the generated tiles were inspected directly. The GDI Generic OpenGL 1.1 session cannot start the OpenGL 3.3 client, so coal and iron still need a hardware-backed in-world screenshot. |
| Actor render (P1) | Five lifecycle snapshot assertions and Ogre validation of one mob plus one item pass. `run_render_capture.ps1 -SpawnValidationActors` supplies the visual fixture, but the GL3+ screenshot still needs a hardware-backed session. |
| Formal data-race detection for the background loader (S0.3) | The V5 stress scenario exercises the real worker and covers the formerly unlocked chunk-map read, but MSVC still provides no ThreadSanitizer proof. |

## Current Verified Runs

| Layer | Command | Result |
| ----- | ------- | ------ |
| Focused headless tests | `bin\HelloMine3DCoordinateTests.exe`, `bin\HelloMine3DMeshDirtyTests.exe`, `bin\HelloMine3DSaveLoadSmoke.exe`, `bin\HelloMine3DEntityLifecycleSmoke.exe` | All pass. |
| World runtime smoke | `bin\HelloMine3DWorldRuntimeSmoke.exe` | `checks=177 failures=0` (Debug and Release, 2026-08-09) |
| A1 asset references | `sh scripts/check_assets.sh` | The repository passes 41 block/shader/texture/font/config checks. An isolated copy with `HelloMine3DTerrain.vert` omitted returns 1 and names the missing referenced shader (2026-08-09). |
| A2 block diagnostics | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseBlockDataDiagnostics`) | Four malformed fixtures verify missing `ShaderType`, invalid `MeshType`, atlas coordinate `16 0`, and duplicate id 3 all report the full source path and exact key. Debug/Release pass (2026-08-09). |
| E0 dependency boundary | `rg "SFML|sf::|GLfloat|GLuint|glad" src/HelloMine3D/World` | No matches (2026-08-09). |
| E1 engine build | `tools\premake\premake5 --os=windows --file=premake/premake.lua vs2022`, then full Debug/Release solution builds | Ogre 1.10 core, GLSupport, GL3Plus, FreeImage dependency chain, dedicated Ogre FreeType, zlib, zzip and OIS all compile with 0 errors (2026-08-09). |
| E2 bootstrap validation | `set HELLOMINE3D_VALIDATE_ONLY=1` then `bin\HelloMine3D.exe` | Debug and Release register `OpenGL 3+ Rendering Subsystem`, 2 resource locations and OIS, then shut down cleanly (2026-08-09). |
| E3 terrain bridge | Set `HELLOMINE3D_VALIDATE_ONLY=1`, `HELLOMINE3D_SEED=20260809`, `HELLOMINE3D_PLAYER_POSITION=264 96 8`, then run `bin\HelloMine3D.exe` | Debug and Release build real terrain and validate every position, atlas-UV, repeat-UV, light and index stream before GPU upload, including section-local bounds and index ranges (2026-08-09). |
| E4 water/flora bridge | Use the E3 command | Debug and Release validate 19 solid sections (8,396 vertices after M4), 3 water sections (1,736 vertices), and 13 flora sections (1,116 vertices); each path has valid CPU streams and a dedicated render queue (2026-08-09). |
| E4 Ogre diagnostics | Add `HELLO_RENDER_CAPTURE=1`, `HELLO_RENDER_CAPTURE_MS=0,250,1000`, and an isolated `HELLO_RENDER_CAPTURE_DIR` to the E4 validation command | Debug and Release report `capture_config=valid`, `capture_enabled=true`, and `capture_targets=3`. Both scripts target the sole client executable (2026-08-09). |
| E4 Ogre HUD/ImGui | Add `HELLOMINE3D_SHOW_DEBUG_INFO=1`, then repeat with `off` | Debug and Release report `hud_config=valid`, five hotbar slots, selected slot zero, and the matching enabled/disabled debug-panel state. OIS key/mouse events, F1 toggling, render-queue submission and camera input suppression compile in both configurations (2026-08-09). |
| E5 single render path | `rg -n "SFML|sf::|glad|imgui.?sfml|HelloMine3DOgreBootstrap" src/HelloMine3D premake tools` plus full Debug/Release builds and all five tests | No old client/render dependencies remain; versioned CPU mesh upload checks pass and `HelloMine3D.exe` is the only client target (2026-08-09). |
| P1 actor bridge | Add `HELLOMINE3D_SPAWN_VALIDATION_ACTORS=1` to a hardware capture, or use `tools\run_render_capture.ps1 -SpawnValidationActors` | Debug/Release validation report `actor_config=valid`, `actor_count=2`, `mob_count=1`, `item_count=1`; five P1 snapshot assertions pass (2026-08-09). |
| P2 actor persistence | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`casePersistence`) | World metadata version 2 restores a mob and item with subtype fields intact, retains ids, advances the next id, and reads/upgrades a version 1 fixture; eight P2 assertions pass (2026-08-09). |
| M1 mesh build metrics | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseSectionMeshInput`) | Five M1 assertions verify per-build timing and cumulative face/vertex counters; the performance CSV/summary and Ogre debug panel expose the same values (2026-08-09). |
| M2 bounded dirty queue | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseMeshDirtyPropagation`) | Five queued sections rebuild as `2 + 2 + 1`; duplicate edits retain one FIFO entry and empty updates add no rebuilds (2026-08-09). |
| M4 opaque greedy meshing | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseGreedyMeshing`) | Six assertions reduce an `8x1x8` single-material slab from 160 naive faces to 6, preserve material boundaries and atlas repetition, and prove water/flora keep their original topology. Debug/Release validation-only startup reduces the fixed-scene solid vertex count from 20,796 to 8,396 while water/flora remain unchanged (2026-08-09). |
| M6 enclosed-section skip | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseEnclosedSectionSkip`) | Nine assertions seal a section with opaque neighbours, prove background and synchronous paths schedule no mesh build or metrics update, then open one top neighbour and require exactly one visible face and one recorded rebuild (2026-08-09). |
| M7 frustum-priority work order | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseFrustumMeshPriority`) | Four assertions retain all 1,089 chunk targets at view distance 16, prioritise the forward half, reverse priority after turning, and preserve distance ordering when no main-thread frustum snapshot is available (2026-08-09). |
| P5 block use interaction | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseInteractionAndEvents`) | Four assertions route use through `BlockInteractionSystem`, publish one `BlockUseEvent` with the target position/id, and keep air use as a no-op. Right-click queues use before the existing adjacent placement action (2026-08-09). |
| Render smoke | see `docs/render-regression-smoke.md` | `bin/render_capture_20260807190230074-46036`, status PASS |
| Performance baseline | see `docs/performance-baseline.md` | `bin/perf_baseline_20260807190255313-41064`, `frame_p95_ms=16.430` |

The 2026-08-09 automation session exposed only the Windows GDI Generic OpenGL
1.1 implementation, below the client's required OpenGL 3.3 context. Headless
validation is therefore fully current, while the last hardware-backed render
and performance runs remain the 2026-08-07 records above. The E2-E5 window probe
reaches GL3Plus context creation and then reports that OpenGL 3.0 is unavailable;
skybox appearance, terrain/water/flora materials, Ogre culling, mouse look,
free-flight controls, HUD/debug-panel appearance, screenshot PNGs and a comparable performance CSV still need one run in a
hardware-accelerated desktop session.

## When To Run

Run the full set before closing any milestone. Run the world runtime smoke
after any change to chunk addressing, chunk storage, block interaction, the
event bus, actors, or terrain generation.
