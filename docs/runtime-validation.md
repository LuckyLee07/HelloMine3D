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
[VALIDATION] checks=255 failures=0
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
| `caseRuntimeConfigOwnership` | A3 | A missing per-user config is regenerated with the documented defaults, and edited window, FOV, fullscreen and render-distance values load without touching repository-owned templates. |
| `caseConfiguredWorldSeed` | A4 | An integer seed loaded from an isolated config becomes the seed of two new worlds and reproduces every sampled terrain block without an environment override. |
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
| `caseSectionMeshInput` | M1, M3 | The 18x18x18 block halo matches direct world reads, edits advance the section revision, and accepted mesh builds accumulate timing plus solid/glass/water/flora face and vertex metrics. |
| `caseTransparentBlockRules` | L4 | Glass definitions and materials round-trip, transparent cubes use the glass pass, leaves use static cutout and flora keeps its animated pass. Shared glass faces are removed within and across chunks while opaque/glass boundaries retain exactly one face. |
| `caseBlockBehaviorDispatch` | C1 | Every definition owns a behavior; default drops delegate to static definitions, while glass registers a no-drop policy. The real break path consumes that policy without adding an item or spawning an entity. |
| `caseMetadataBackedBehavior` | C2 | Tall grass uses one block id with named immature/mature metadata. Direct strategy dispatch, biome output and real break interactions prove immature grass has no drop while mature grass drops the configured item. |
| `caseRandomTickScheduling` | C7 | Only immature tall grass opts into random ticks. Five indexed sections prove the four-per-tick round-robin budget, then unload removes the index and storage reload rebuilds it before growth resumes. |
| `caseResourceDrivenBlockShapes` | C3 | Flora definitions resolve a shared `Cross` shape, exact face vertices come from the resource, an isolated single-quad fixture parses without builder changes, and a real section emits the expected flora mesh. |
| `caseSunlightStorage` | L1 | Sunlight conversion stays within 0-15, open and roofed columns differ, the full 18x18x18 halo carries sunlight, mesh brightness and greedy boundaries respect light values, and storage reload rebuilds derived sunlight. |
| `caseBlockLightStorage` | L2 | A data-driven level-14 emitter falls off by one per voxel, opaque blocks stop propagation, the halo carries block light, meshes choose the stronger light source, and storage reload rebuilds derived values. |
| `caseLocalRelightAfterEdits` | L3 | Edits update one sunlight column and locally remove/refill block light across chunk boundaries; mesh revisions and a bounded section queue track light-only changes, while chunk load/unload reconciles boundary light. |
| `caseSectionMeshUploadSnapshot` | E5 | Ogre-facing CPU mesh snapshots include live section identity and block revision; stale upload acknowledgements cannot overwrite a newer edit. |
| `caseUnloadPersistence` | S2.4 | Unloading a chunk flushes it to storage first, and reloading restores the edit instead of regenerating. |
| `caseChunkFormatRejection` | S2.2, S2.3 | Chunk file paths are deterministic, and a corrupted magic is rejected with a diagnostic rather than loaded as garbage. |
| `caseTerrainDeterminism` | S6.1, S6.4, C4 | The same seed produces identical terrain over 175k sampled blocks, ore layout is stable, underground cave indices repeat exactly, caves are non-empty, and protected surface layers remain solid. |
| `caseTerrainStructures` | S6.2, S6.3, S6.5, C5 | Tree and plant decorators run, biomes produce varied surfaces, structures cross chunk boundaries, generation ignores adjacent-chunk load order, and structures are identical after a save/reload roundtrip. |
| `caseInteractionAndEvents` | S3.1, S3.3, S3.4, S3.5, S4.2, S4.5, P5 | Break/place/use go through the interaction system, produce configured drops, consume items, publish events with the target identity, and block metadata survives a roundtrip. |
| `caseChunkEvents` | S4.3 | Generate, load, save and unload each publish their chunk event. |
| `caseActors` | P1, S4.4, S5.1, S5.2, S5.5, S5.6, C6 | Mobs spawn, chase nearby players, wander outside chase range, reject repeated damage during invulnerability, die and get culled; item entities spawn and are picked up, publishing the matching events. Immutable actor snapshots track transforms and omit dead or picked-up ids. |
| `caseWorldManager` | S1.2, S1.4, S1.5, S4.5 | World creation, lookup, save, load, same-world teleport, cross-world rejection and world-time advance. |

### Not covered

These need a person at the keyboard or a different harness:

| Gap | Why |
| --- | --- |
| Physical OIS keyboard/mouse response | The GL3+ client creates and captures both OIS devices, and V2 deterministically verifies the resulting `PlayerInputState` controller seam. A human-at-keyboard check remains appropriate after platform/window-system changes because desktop input injection is deliberately outside the non-intrusive capture harness. |
| Formal data-race detection for the background loader (S0.3) | The V5 stress scenario exercises the real worker and covers the formerly unlocked chunk-map read, but MSVC still provides no ThreadSanitizer proof. |

## Current Verified Runs

| Layer | Command | Result |
| ----- | ------- | ------ |
| Focused headless tests | `bin\HelloMine3DCoordinateTests.exe`, `bin\HelloMine3DMeshDirtyTests.exe`, `bin\HelloMine3DSaveLoadSmoke.exe`, `bin\HelloMine3DEntityLifecycleSmoke.exe` | All pass. |
| World runtime smoke | `bin\HelloMine3DWorldRuntimeSmoke.exe` | `checks=255 failures=0` (Debug and Release, 2026-08-12) |
| A1 asset references | `sh scripts/check_assets.sh` | The repository passes 45 block/shape/shader/texture/font/config checks after discovering named shape resources and multiline block registrations. An isolated copy with `HelloMine3DTerrain.vert` omitted returns 1 and names the missing referenced shader (2026-08-09; current positive equivalent run 2026-08-12). |
| A2 block diagnostics | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseBlockDataDiagnostics`) | Five malformed fixtures verify missing `ShaderType`, invalid `MeshType`, atlas coordinate `16 0`, light level 16, and duplicate id 3 all report the full source path and exact key. Debug/Release pass (2026-08-12). |
| A3 runtime config ownership | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseRuntimeConfigOwnership`) | Three assertions delete an isolated `config.txt`, verify regeneration with the documented defaults, then load customised values. `Mine.cfg` and `MineResources.cfg` remain the only tracked `bin/` templates (2026-08-09). |
| A4 configured world seed | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseConfiguredWorldSeed`) | Three assertions load seed `20260811` from an isolated config, create two fresh worlds without `HELLOMINE3D_SEED`, and compare 2,299 terrain samples with zero mismatches. Debug/Release pass (2026-08-09). |
| B5 capture polling race | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_render_capture.ps1 -ValidateCapturePolling`; ten real captures plus `ffmpeg -v error -i <png> -f null -` | Rejects a truncated non-empty PNG, then ten writers transition from partial data to a structurally complete PNG and finish with `runs=10 status=PASS`. Ten GTX 1050 Ti / OpenGL 4.6 captures also pass the script and independent decoder validation (`46,343-49,604` bytes). The poller requires a valid signature, bounded chunks and terminal `IEND`; relative paths resolve from the repository root and the native process handle is primed before fast exit (2026-08-12). |
| B3 Xcode generation preflight | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_xcode_generation.ps1` | Generates 22 macOS Xcode projects and passes 123 checks over the workspace graph, Cocoa/OIS/OSX sources, seven frameworks, platform header paths, foreign-platform leakage and `scripts/verify_xcode.sh`. Reports `native_build=NOT_RUN` honestly on Windows (2026-08-12). |
| B3 native Xcode gate | `bash scripts/verify_xcode.sh` on macOS | Deferred while Windows-first iterations continue. When resumed, the script requires Debug/Release client and test builds, ten test executions, two validation-only bootstraps and two real-window three-frame launches before printing `status=PASS`. |
| W2 Windows startup errors | `tools\validate_startup_errors.ps1`; also part of `scripts\verify_build.ps1` after each client configuration | Three isolated roots omit the required terrain shader, atlas texture or stone block definition. Every client returns 1, names the category and exact resource in stderr, and writes the identical user-facing payload with `ui=MessageBoxW` and `dialog_requested=true`; automation suppresses the modal call after recording it. Ordinary Windows startup calls `MessageBoxW`, while validate-only runs remain non-interactive. |
| W3 generated resource manifest | `tools\generate_resource_manifest.ps1 -Check`; `tools\validate_resource_manifest.ps1`; Debug/Release validation-only client runs in `scripts\verify_build.ps1` | The checked-in manifest contains 39 sorted unique entries derived from 16 registered blocks, one referenced shape, 10 shaders, seven textures, one font, two Ogre resource scripts and two runtime templates. The positive check passes; isolated manifests missing the referenced terrain vertex shader or containing the existing-but-unreferenced `test.png` fail with distinct `MISSING_ENTRY` and `STALE_ENTRY` diagnostics. Both client configurations strictly parse the manifest and preflight all 39 non-empty resources before Ogre construction (2026-08-12). |
| W4 terrain buffer measurement | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseTerrainBufferMetrics`); Debug/Release validation-only client; debug panel and performance CSV/summary | Compile-time and two `W4/*` checks fix the current terrain layout at 32 bytes per vertex and four bytes per index. The deterministic client fixture measures 27,176 vertices, 40,764 indices and a 1,032,688-byte (0.985 MiB) payload. The archived L4 cumulative geometry gives a conservative 12.456 MiB upper bound and already sustained 60.10 FPS / 17.54 ms P95, so the layout remains uncompressed. Live runs now report current resident counts and bytes rather than relying on cumulative rebuild counters. Debug/Release pass with `checks=255 failures=0` (2026-08-12). |
| L1 sunlight storage | `bin\HelloMine3DWorldRuntimeSmoke.exe`; hardware render capture; `tools\run_perf_baseline.ps1` | Nine L1 assertions pass in Debug and Release. `bin/render_capture_l1_surface_20260812/new_01500ms.png` independently decodes and shows bright open terrain against darker opaque occlusion on a GTX 1050 Ti / OpenGL 4.6. The 10-second performance run records 601 frames, 60.14 FPS, 17.75 ms frame P95, no frames over 33 ms, and 0.466/1.363 ms average/max section mesh build time (2026-08-12). |
| L2 block-light storage | `bin\HelloMine3DWorldRuntimeSmoke.exe`; `tools\run_render_capture.ps1`; `tools\run_perf_baseline.ps1` | Seven L2 assertions plus one A2 range fixture pass in Debug and Release. The hardware PNG independently decodes. The matching 10-second run records 601 frames, 60.14 FPS, 17.69 ms frame P95, no frames over 33 ms, and 0.467/0.988 ms average/max mesh build time; geometry counts match L1 exactly (2026-08-12). |
| L3 local relight | `bin\HelloMine3DWorldRuntimeSmoke.exe`; `tools\run_render_capture.ps1`; `tools\run_perf_baseline.ps1` | Nine L3 assertions pass in Debug and Release, including cross-chunk edit propagation, opaque add/remove, single-column sunlight, bounded mesh invalidation, reload reconciliation and unload cleanup. The PNG independently decodes. The 10-second run records 601 frames, 60.09 FPS, 17.58 ms frame P95, no frames over 33 ms, and 0.477/0.998 ms average/max mesh build time (2026-08-12). |
| L4 transparent rules | `bin\HelloMine3DWorldRuntimeSmoke.exe`; `HELLOMINE3D_TRANSPARENT_FIXTURE=1` Ogre validation/capture; `tools\run_perf_baseline.ps1` | Ten L4 assertions pass in Debug and Release. Ogre validation reports one transparent section with 68 vertices/102 indices; `bin/render_capture_l4_20260812_full/new_05000ms.png` shows the deterministic glass/leaf/flora fixture and independently decodes. The 10-second run records 601 frames, 60.10 FPS, 17.54 ms frame P95, no frames over 33 ms, and 0.480/1.169 ms average/max mesh build time (2026-08-12). |
| C1 block behavior | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseBlockBehaviorDispatch`) | Four C1 assertions verify default and special drop dispatch plus the real no-drop glass break path. The registry owns behavior lifetime and the interaction system has no glass-specific branch (Debug and Release, 2026-08-12). |
| C2 metadata behavior | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseMetadataBackedBehavior`) | Four C2 assertions verify one `TallGrass` id selects different drops by maturity metadata, natural biome grass is mature, and both states behave correctly through the real break path (Debug and Release, 2026-08-12). |
| C3 resource shapes | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseResourceDrivenBlockShapes`); asset reference check | Four C3 assertions verify shared shape resolution, exact resource vertices, isolated shape extensibility and real flora mesh output. The asset equivalent reports 45/45 references present; `ChunkMeshBuilder` contains no crossed-quad geometry (Debug and Release, 2026-08-12). |
| C4 cave generation | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseTerrainDeterminism`) | Three C4 assertions find 3,766 underground air cells across nine chunks, reproduce their exact indices with the same seed, and observe zero air cells in the protected surface layers. Existing ore, biome and structure checks also pass (Debug and Release, 2026-08-12). |
| C5 cross-chunk structures | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseTerrainStructures`) | Two C5 assertions find 100 tree-block links across internal chunk boundaries and compare all 20,480 blocks in each of two adjacent chunks after forward and reverse generation order. The save/reload check also preserves all 100 links (Debug and Release, 2026-08-12). |
| C6 mob chase and damage immunity | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseActors`); `bin\HelloMine3DEntityLifecycleSmoke.exe` | Three C6 assertions move a nearby mob 0.12 blocks toward the player in one fixed tick, reject a second hit without changing health or publishing another damage event, and accept damage again after the 0.5-second immunity expires. The focused lifecycle smoke also rejects an immediate repeated event-bus hit (Debug and Release, 2026-08-12). |
| C7 bounded random ticks | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseRandomTickScheduling`) | Six C7 assertions use deterministic uniform voxel samples to prove per-metadata behavior opt-in, an active-section-only index, three attempts per visited section, a four-section fixed-tick budget, round-robin completion, unload cleanup and storage reload rebuilding. Debug/Release pass with `checks=245 failures=0`; active/processed/dispatched counters are available to the debug panel and performance capture (2026-08-12). |
| W1 time of day and fog | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseWorldEnvironment`, `caseWorldManager`); `tools\run_render_capture.ps1 -WorldTime 6000/18000` | Eight W1 assertions cover cycle anchors, positive/negative wrapping, bounded daylight and fog, twilight continuity, the automation override and debug-stat exposure. The terrain, water, flora and actor shaders consume global light and exponential fog; the skybox consumes the matching tint. Debug/Release pass with `checks=253 failures=0`; the hardware day/night comparison remains the final acceptance item. |
| E0 dependency boundary | `rg "SFML|sf::|GLfloat|GLuint|glad" src/HelloMine3D/World` | No matches (2026-08-09). |
| E1 engine build | `tools\premake\premake5 --os=windows --file=premake/premake.lua vs2022`, then full Debug/Release solution builds | Ogre 1.10 core, GLSupport, GL3Plus, FreeImage dependency chain, dedicated Ogre FreeType, zlib, zzip and OIS compile in both configurations; hardware render and performance smokes pass on a GTX 1050 Ti / OpenGL 4.6 (2026-08-12). |
| E2 bootstrap and skybox | Validation-only startup plus `tools\run_render_capture.ps1 -ShowDebugInfo` | Debug and Release register `OpenGL 3+ Rendering Subsystem`, 2 resource locations and OIS. Hardware runtime loads the six `sky.png` cube faces and `docs/screenshots/validation-skybox-panel-outline.png` confirms the rendered skybox (2026-08-12). |
| E3 terrain bridge | Set `HELLOMINE3D_VALIDATE_ONLY=1`, `HELLOMINE3D_SEED=20260809`, `HELLOMINE3D_PLAYER_POSITION=264 96 8`, then run `bin\HelloMine3D.exe` | Debug and Release build real terrain and validate every position, atlas-UV, repeat-UV, light and index stream before GPU upload, including section-local bounds and index ranges. Hardware terrain capture and the L4 performance baseline pass (2026-08-12). |
| E4 water/flora bridge | Use the E3 command | Release validates 20 solid/cutout sections (9,296 vertices), 3 water sections (1,736 vertices), and 12 flora sections (216 vertices). The L4 fixture additionally validates one glass section (68 vertices); every path has valid CPU streams and an explicit render queue (2026-08-12). |
| E4 Ogre diagnostics | Add `HELLO_RENDER_CAPTURE=1`, `HELLO_RENDER_CAPTURE_MS=0,250,1000`, and an isolated `HELLO_RENDER_CAPTURE_DIR` to the E4 validation command | Debug and Release report `capture_config=valid`, `capture_enabled=true`, and `capture_targets=3`. Both scripts target the sole client executable (2026-08-09). |
| E4 Ogre HUD/ImGui | Add `HELLOMINE3D_SHOW_DEBUG_INFO=1`, then repeat with `off` | Debug and Release report `hud_config=valid`, five hotbar slots, selected slot zero, and matching enabled/disabled state. `docs/screenshots/validation-skybox-panel-outline.png` confirms the hardware-rendered HUD and live panel (2026-08-12). |
| E5 single render path | `rg -n "SFML|sf::|glad|imgui.?sfml|HelloMine3DOgreBootstrap" src/HelloMine3D premake tools` plus full Debug/Release builds and all five tests | No old client/render dependencies remain; versioned CPU mesh upload checks pass, `HelloMine3D.exe` is the only client target, and the hardware PNG/CSV smokes pass (2026-08-12). |
| P1 actor bridge | Add `HELLOMINE3D_SPAWN_VALIDATION_ACTORS=1` to a hardware capture, or use `tools\run_render_capture.ps1 -SpawnValidationActors` | Debug/Release validation report `actor_count=2`, `mob_count=1`, `item_count=1`; five P1 snapshot assertions pass. `docs/screenshots/validation-actors.png` shows the green mob and amber item together (2026-08-12). |
| P2 actor persistence | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`casePersistence`) | World metadata version 2 restores a mob and item with subtype fields intact, retains ids, advances the next id, and reads/upgrades a version 1 fixture; eight P2 assertions pass (2026-08-09). |
| M1 mesh build metrics | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseSectionMeshInput`) | Five M1 assertions verify per-build timing and cumulative face/vertex counters; the performance CSV/summary and Ogre debug panel expose the same values (2026-08-09). |
| M2 bounded dirty queue | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseMeshDirtyPropagation`) | Five queued sections rebuild as `2 + 2 + 1`; duplicate edits retain one FIFO entry and empty updates add no rebuilds (2026-08-09). |
| P3 selection outline | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseBlockSelection`); hardware capture | Five assertions cover solid selection, placement adjacency, water skipping, reach and misses. The yellow Ogre outline is visible in `docs/screenshots/validation-skybox-panel-outline.png` (2026-08-12). |
| P4 ore textures | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseOreTextures`); `tools\run_render_capture.ps1 -ShowOreFixture` | Four assertions cover dedicated atlas slots and distinct hashes. `docs/screenshots/validation-ores.png` visibly distinguishes coal and iron in the live world (2026-08-12). |
| M4 opaque greedy meshing | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseGreedyMeshing`) | Six assertions reduce an `8x1x8` single-material slab from 160 naive faces to 6, preserve material boundaries and atlas repetition, and prove water/flora keep their original topology. Debug/Release validation-only startup reduces the fixed-scene solid vertex count from 20,796 to 8,396 while water/flora remain unchanged; hardware captures retain block-scale texture repetition (2026-08-12). |
| M6 enclosed-section skip | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseEnclosedSectionSkip`) | Nine assertions seal a section with opaque neighbours, prove background and synchronous paths schedule no mesh build or metrics update, then open one top neighbour and require exactly one visible face and one recorded rebuild (2026-08-09). |
| M7 frustum-priority work order | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseFrustumMeshPriority`) | Four assertions retain all 1,089 chunk targets at view distance 16, prioritise the forward half, reverse priority after turning, and preserve distance ordering when no main-thread frustum snapshot is available (2026-08-09). |
| P5 block use interaction | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseInteractionAndEvents`) | Four assertions route use through `BlockInteractionSystem`, publish one `BlockUseEvent` with the target position/id, and keep air use as a no-op. Right-click queues use before the existing adjacent placement action (2026-08-09). |
| Render smoke | see `docs/render-regression-smoke.md` | L4 plus tracked skybox/panel/outline, actor and ore captures all pass on OpenGL 4.6; evidence is under `docs/screenshots/validation-*.png`. |
| Performance baseline | see `docs/performance-baseline.md` | `bin/perf_baseline_l4_20260812_verified`, `frame_p95_ms=17.543` |

The current 2026-08-12 runs use an NVIDIA GTX 1050 Ti with OpenGL 4.6. The
earlier GDI Generic limitation no longer applies to these hardware-backed PNG
and performance records.

## When To Run

Run the full set before closing any milestone. Run the world runtime smoke
after any change to chunk addressing, chunk storage, block interaction, the
event bus, actors, or terrain generation.
