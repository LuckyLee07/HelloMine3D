# Runtime Validation

This document describes how sandbox foundation behaviour is validated at
runtime, without a human driving the client window.

Five implemented layers exist. Together they are the current acceptance gate
for the foundation and the completed milestones in `docs/todolist.md`.

| Layer | Target | Answers |
| ----- | ------ | ------- |
| Focused headless tests | `HelloMine3DCoordinateTests`, `HelloMine3DMeshDirtyTests`, `HelloMine3DSaveLoadSmoke`, `HelloMine3DEntityLifecycleSmoke` | Do the isolated algorithms behave? |
| World runtime smoke | `HelloMine3DWorldRuntimeSmoke` | Does the real `World` / `ChunkManager` / `WorldManager` / actor stack behave? |
| Stability and resource smokes | `HelloMine3DSoak`, `HelloMine3DResourcePackSmoke` | Do long-lived world transitions remain bounded, and is the frozen effective-resource view correct? |
| Client smokes | `tools/run_render_capture.ps1`, `tools/run_perf_baseline.ps1` | Does the assembled client still render and stay within its frame budget? |
| Distribution smoke | `tools/package_windows_release.ps1` | Can the Release client start and fail correctly from an isolated self-contained root? |

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
[VALIDATION] checks=330 failures=0
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
| `casePlayerSweptCollision` | V2 | A player falling more than ten blocks in one fixed tick cannot skip a one-block floor; resting contact remains stable and the next jump succeeds. |
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
| `casePlayableVerticalSlice` | D6 | After one deterministic fixture, only normal gameplay paths acquire/plant/grow/harvest a crop, transfer Wheat into a chest, spawn and defeat a natural Mob, pick up its physical drop, replant, save and relaunch with restored state. |
| `caseWorldManager` | S1.2, S1.4, S1.5, S4.5 | World creation, lookup, save, load, same-world teleport, cross-world rejection and world-time advance. |

### Not covered

These need a person at the keyboard or a different harness:

| Gap | Why |
| --- | --- |
| Physical OIS keyboard/mouse response | The GL3+ client creates and captures both OIS devices, and V2 deterministically verifies the resulting `PlayerInputState` controller seam. A human-at-keyboard check remains appropriate after platform/window-system changes because desktop input injection is deliberately outside the non-intrusive capture harness. |
| Formal data-race detection for the background loader (S0.3) | The V5 stress scenario exercises the real worker and covers the formerly unlocked chunk-map read, but MSVC still provides no ThreadSanitizer proof. |

## Acceptance Contracts

This table states the evidence contract. The authoritative implementation
status and remaining acceptance gaps are in `docs/todolist.md`; completed runs
are recorded in `Current Verified Runs` below.

| Scope | Required extension | Evidence required to close |
| ----- | ------------------ | -------------------------- |
| R1 performance comparison | Compare two performance summaries only after build configuration, scene identity, vsync regime and final chunk/section residency are compatible. Apply the documented P95 +15%, P99 +20% and material `frames_over_50ms` warnings as a non-zero regression result. | Deterministic pass/fail/incomparable fixtures, a current baseline self-compare and one intentional regression fixture. Record both input run ids and the comparison result. |
| D1-D2 stateful container | Extend focused save/load and world runtime coverage for block-entity ownership, unique positions, container transfer, block removal and compatibility. Add a client fixture for container UI without bypassing the P5 use path. | Debug/Release assertions, one decoded hardware capture, and the R3 keyboard/mouse checklist covering open, transfer, close, escape and focus recovery. |
| D3-D4 live mobs and combat | Cover deterministic spawn candidates, population caps, safe placement, restore deduplication, actor targeting, accepted/suppressed damage, death/drop ordering and player respawn. Validation-only spawn helpers remain fixture-only and cannot satisfy D6. | Focused actor tests, world assertions, live/capped counters, a fixed-seed hardware capture and R3 attack/input evidence. |
| D5 crop loop | Cover support checks, metadata stages, random-tick budgets, unloaded-section exclusion, immature/mature drops, harvest/replant and persistence. | World assertions plus asset and generated-manifest checks for every new block, material, shape or texture. |
| R2 long-running soak | Use a fixed seed and versioned action schedule to repeat load-centre movement, block edits, actor/item lifecycle and save/reload while sampling memory, process handles, dirty queues and mesh progress. Developer runs may be shorter; milestone evidence is at least 30 minutes. | Timestamped summary and bounded logs outside Git. Fail non-zero on invariant errors, save corruption, stalled mesh progress or unbounded counter growth; record build configuration, seed, duration and peak/final counters. |
| R3 physical OIS input | Keep desktop input non-intrusive: a person verifies movement, look, fly/sneak toggles, hotbar, break/place/use, UI focus and window focus recovery. | Record date, commit, build configuration, GPU/window mode, each checklist result and any deviation. Repeat after input, window-system, container UI or combat changes. |
| D6 playable slice | Run the normal fixed-seed player path through crop acquisition/planting, container use, a live mob encounter, combat, loot pickup, save, relaunch and restored-state inspection. | Full Windows gate, applicable headless assertions, R1 before/after comparison, R3 checklist and tracked decoded hardware captures. No debug-only state injection after initial fixture selection. |
| R4 formal race detection | Build and run the loader churn/world smoke with Clang ThreadSanitizer on a supported host. | Toolchain/host details, exact command, clean sanitizer output and ordinary test result. Until then R4 remains `Deferred`; V5 is stress evidence, not a substitute. |
| R5 clean-root package | Validate a generated Release distribution from an isolated directory, with no access to the source/build tree. Check the packaged inventory, required notices, validation-only startup, real-window startup and negative missing/stale resource cases. | Archive inventory/hash, isolated-root path, positive startup results and distinct negative diagnostics. Generated archives and logs remain untracked. |
| X1-X3 resource packs | Exercise resolver precedence, fallback, version rejection, path-escape rejection, effective manifests and per-resource-class overrides from isolated roots. The effective view is frozen before Ogre construction. | No-pack compatibility, valid-pack Debug/Release startup, missing/stale/duplicate/path-escape failures, base/packed decoded captures, R1 comparison and R5 packaging coverage. |
| K1-K4 durable worlds | Treat catalogue reads as non-mutating, publish saves transactionally, retain bounded verified backups and route create/open/rename/recover/delete through renderer-independent commands. Inject failures at every publication boundary. | Legacy/malformed/path-escape catalogue fixtures; last-good-save preservation; bounded backup/restore evidence; state-complete recovery; and a Release physical-input record for the world screen and recoverable deletion. |
| H1-H3 local crash diagnostics | Generate one local Windows minidump for a controlled Release crash, pair it with versioned sanitized context, preserve offline symbols separately and exercise the same path from a clean package. No upload or telemetry is allowed in this milestone. | Non-empty dump, exact build identity, no personal-path leakage, at least one symbolized HelloMine3D frame, no dump on ordinary exits, deterministic package inventory without symbols, and a successful next-start local recovery prompt. |
| Q1-Q3 expanded budgets | Version scene identity and required metrics for cold start, world entry, save, restore, fast streaming and scaled gameplay. Keep R1 frame thresholds and R2 long-running invariants; add non-zero gates only after current Release baselines and thresholds are approved. | Pass/regression/invalid/incomparable fixtures for every metric; compatible before/after summaries; explicit startup/save/restore and chunk-visible budgets; scale/cap evidence; and a repeated formal soak after persistence/gameplay changes. |
| G1-G6 progression slice | Cover strict recipes, atomic crafting, workbench focus, tool class/tier/speed/durability/drop policy, settings persistence, bounded audio events and the clean-package stage-8 player journey. | Debug/Release state-conservation and persistence assertions, asset/resource-pack gates, decoded hardware captures, physical input, Q comparisons, deterministic package, save-failure recovery and the separate H controlled-crash/symbolization result. |

### Stage 8 Planned Evidence

K/H/Q/G rows above are acceptance contracts, not completed-run claims. Q1 is
now `Doing` after the schema-v2 fixtures and pre-stage-8 macOS baseline; the
remaining stage-8 rows stay `Todo` in `docs/todolist.md`. New harnesses must
first prove their own positive, negative, invalid and incomparable fixtures
before their output can be added to `Current Verified Runs`. R3 remains the
required human closure before stage-8 player-facing input or UI baselines are
accepted.

### R3 Physical Input Record

Protocol v1 is defined in `docs/manual-input-acceptance-v1.md`; its exact
machine-checkable skeleton is `docs/manual-input-record-v1.template.txt`.
The twelve ordered cases cover focus recovery, WASD/sprint, two-axis mouse
look and its `L` toggle, flight/sneak/vertical controls, number and physical
wheel hotbar selection, break, attack, place, container use/transfers,
button/Escape container close and final application/window close. The client
now maps the physical OIS wheel delta into the same bounded hotbar selection
path as the already-tested keyboard delta.

Records are key/value text with protocol version, date, exact commit,
configuration, GPU/driver, window mode/size, operator, every case result,
overall result and deviations. Validate a real run with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_manual_input_record.ps1 -RecordPath <record.txt> -RequirePass
```

The validator rejects missing/duplicate/unknown fields, invalid metadata,
unknown case results and any claimed overall pass with a non-pass case or
deviation. `scripts\verify_build.ps1` validates the tracked `NOT_RUN` template
schema, but only a human-operated `-RequirePass` record can close R3. No
automation may synthesize the physical input.

## Current Verified Runs

| Layer | Command | Result |
| ----- | ------- | ------ |
| Headless test targets | The seven `HelloMine3D*Tests.exe` / `*Smoke.exe` / `*Soak.exe` binaries enumerated by `scripts\verify_build.ps1` | All seven pass in Debug and Release; the world target reports `checks=330 failures=0`, while soak and resource-pack targets also retain their formal evidence below. |
| World runtime smoke | `bin\HelloMine3DWorldRuntimeSmoke.exe` | `checks=330 failures=0`; the three added V2 assertions cover swept falling collision, stable ground contact and jumping after rest (full Debug/Release Xcode gate, 2026-08-16). |
| A1 asset references | `sh scripts/check_assets.sh` | The repository passes 45 block/shape/shader/texture/font/config checks after discovering named shape resources and multiline block registrations. An isolated copy with `HelloMine3DTerrain.vert` omitted returns 1 and names the missing referenced shader (2026-08-09; current positive equivalent run 2026-08-12). |
| A2 block diagnostics | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseBlockDataDiagnostics`) | Five malformed fixtures verify missing `ShaderType`, invalid `MeshType`, atlas coordinate `16 0`, light level 16, and duplicate id 3 all report the full source path and exact key. Debug/Release pass (2026-08-12). |
| A3 runtime config ownership | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseRuntimeConfigOwnership`) | Three assertions delete an isolated `config.txt`, verify regeneration with the documented defaults, then load customised values. `Mine.cfg` and `MineResources.cfg` remain the only tracked `bin/` templates (2026-08-09). |
| A4 configured world seed | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseConfiguredWorldSeed`) | Three assertions load seed `20260811` from an isolated config, create two fresh worlds without `HELLOMINE3D_SEED`, and compare 2,299 terrain samples with zero mismatches. Debug/Release pass (2026-08-09). |
| B5 capture polling race | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_render_capture.ps1 -ValidateCapturePolling`; ten real captures plus `ffmpeg -v error -i <png> -f null -` | Rejects a truncated non-empty PNG, then ten writers transition from partial data to a structurally complete PNG and finish with `runs=10 status=PASS`. Ten GTX 1050 Ti / OpenGL 4.6 captures also pass the script and independent decoder validation (`46,343-49,604` bytes). The poller requires a valid signature, bounded chunks and terminal `IEND`; relative paths resolve from the repository root and the native process handle is primed before fast exit (2026-08-12). |
| B3 Xcode generation preflight | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_xcode_generation.ps1` | Generates 24 macOS Xcode projects and passes 133 checks over the workspace graph, Cocoa/OIS/OSX sources, seven frameworks, platform header paths, foreign-platform leakage and `scripts/verify_xcode.sh`. Reports `native_build=NOT_RUN` honestly on Windows (updated 2026-08-13). |
| B3 native Xcode gate | `bash scripts/verify_xcode.sh` on macOS | Apple M1 Pro completed Debug/Release client and seven-test builds, fourteen test executions, two validation-only bootstraps and two real Cocoa 120-frame launches. Both world runs report `checks=330 failures=0`; both window probes persist surface contact at `Y=66`. The gate reports `status=PASS` with logs under `build/xcode-validation-20260816185723` (2026-08-16). |
| W2 Windows startup errors | `tools\validate_startup_errors.ps1`; also part of `scripts\verify_build.ps1` after each client configuration | Three isolated roots omit the required terrain shader, atlas texture or stone block definition. Every client returns 1, names the category and exact resource in stderr, and writes the identical user-facing payload with `ui=MessageBoxW` and `dialog_requested=true`; automation suppresses the modal call after recording it. Ordinary Windows startup calls `MessageBoxW`, while validate-only runs remain non-interactive. |
| W3 generated resource manifest | `tools\generate_resource_manifest.ps1 -Check`; `tools\validate_resource_manifest.ps1`; Debug/Release validation-only client runs in `scripts\verify_build.ps1` | The checked-in manifest contains 42 sorted unique entries: 18 blocks, one shape, 10 shaders, seven textures, one font, two Ogre resource scripts and three runtime templates. Positive, missing-entry and stale-entry checks pass; startup preflights the frozen effective source for every entry (updated 2026-08-13). |
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
| W1 time of day and fog | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseWorldEnvironment`, `caseWorldManager`); `tools\run_render_capture.ps1 -WorldTime 6000/18000` | Eight W1 assertions cover cycle anchors, positive/negative wrapping, bounded daylight and fog, twilight continuity, the automation override and debug-stat exposure. Terrain, water, flora and actor shaders consume global light and exponential fog; the skybox consumes the matching tint. Environment parameter application explicitly loads target materials, including layers absent from the current scene. Debug/Release pass with `checks=255 failures=0`. Same-seed/position/rotation GTX 1050 Ti / OpenGL 4.6 captures pass at noon and midnight; both are 1584x861, independently inspected, have distinct SHA-256 hashes and sampled mean luma 56.096 / 12.993. The debug panel reports cycles 0.252 / 0.752 and environment light 1.000 / 0.180; evidence is tracked in `docs/screenshots/validation-day.png` and `validation-night.png` (2026-08-13). |
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
| R1 performance comparison | `tools\validate_perf_comparison.ps1`; also part of `scripts\verify_build.ps1` | Four fixtures distinguish `PASS` (real L4 self-compare), `REGRESSION` (P95/P99/long frames), `INCOMPARABLE` (scene and residency) and `INVALID` (missing metric). The comparator uses exit codes 0/2/3/4 and exact diagnostic reasons (2026-08-13). |
| D1 stateful-block lifecycle | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseBlockEntityLifecycle`); `bin\HelloMine3DSaveLoadSmoke.exe`; full `scripts\verify_build.ps1` | Eleven world assertions cover owned create/find/update/remove, unique positions, invalid type and position rejection, atomic failed loads, negative world-coordinate translation, unload/reload persistence and automatic removal when the block changes. The focused fixture proves version 1 chunks remain readable with empty block-entity state and zero metadata. Debug/Release and every startup/resource/performance fixture pass with `checks=266 failures=0` (2026-08-13). |
| D2 chest container | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseChestContainer`); `tools\run_render_capture.ps1 -ShowContainerFixture`; full `scripts\verify_build.ps1` | Twelve assertions cover P5 placement/use, bounded versioned slots, bidirectional total-preserving transfers, change events, malformed payload rejection, save/reload, input-action suppression and the explicit spill-on-break policy. Debug/Release pass with `checks=278 failures=0`. The 1584x861 GTX 1050 Ti / OpenGL 4.6 Release readback decodes cleanly and visibly shows all nine chest slots, five hotbar slots and Close/Escape guidance in `validation-container.png`. Physical focus recovery and Escape remain assigned to R3, so D2 remains `Verify` (2026-08-13). |
| D3 natural-mob population | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseNaturalMobPopulation`); fixed-seed `tools\run_render_capture.ps1`; `tools\run_perf_baseline.ps1`; R1 comparison; full `scripts\verify_build.ps1` | Thirteen assertions cover deterministic bounded candidates, safe ground/headroom, local/world caps, chunk unload/reload, counters, save restoration and spatial duplicate rejection. Debug/Release pass with `checks=291 failures=0`. `validation-natural-mobs.png` is a 1584x861 GTX 1050 Ti / OpenGL 4.6 Release readback from the normal simulation path: the green Mob is visible and the panel reports four actors, `4 / 12` natural Mobs and four additions. The matching 60.1 FPS sample records P95/P99 `16.693/17.633 ms`, zero >33/50 ms frames and R1 `PASS` (2026-08-13). |
| D4 combat, death and respawn | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseCombatAndRespawn`); `tools\run_render_capture.ps1 -ShowCombatFixture`; same-session D3/D4 performance A/B; full `scripts\verify_build.ps1` | Fourteen assertions cover nearer actor/block occlusion, accepted and invulnerability-suppressed attacks, chase-to-contact damage and cooldown, ordered Mob damage/death/drop, dead-target rejection, ordered player damage/death/spawn, saved-spawn respawn, zeroed velocity, explicit retained-inventory policy, closed container UI and HUD stats. Debug/Release pass with `checks=305 failures=0`. `validation-combat-hud.png` is a decoded 1584x861 GTX 1050 Ti / OpenGL 4.6 Release readback showing a Mob under the crosshair and `Health 14 / 20`. A freshly built D3 control and D4 candidate under the same current driver regime record P95/P99 `20.006/23.808 ms` and `20.497/24.095 ms`; both have zero >50 ms frames and R1 reports `PASS`. Physical left-click attack remains assigned to R3, so D4 remains `Verify` (2026-08-13). |
| D5 wheat crop loop | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseWheatCropLoop`); `tools\run_render_capture.ps1 -ShowCropFixture`; generated resource manifest; same-session D4/D5 performance A/B; full `scripts\verify_build.ps1` | Thirteen assertions cover block/material registration, metadata-driven render height, initial seed acquisition, rejected support without consumption, accepted planting on dirt, one-stage random-tick progress, bounded maturity, immature/mature drops, harvest/replant, unloaded-section exclusion and stage persistence. Debug/Release pass with `checks=318 failures=0`; the asset gate reports 41 manifest entries. `validation-wheat-crop.png` is a decoded 1584x861 GTX 1050 Ti / OpenGL 4.6 Release readback showing the four fixed metadata stages from 25% to 100% height, while the panel reports three immature crops in the random-tick index. The same-session D4 control and D5 candidate used identical Release scene/residency and actual uncapped pacing; both had zero >50 ms frames and R1 reports `PASS` (2026-08-13). |
| D6 playable vertical slice | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`casePlayableVerticalSlice`); `tools\run_render_capture.ps1 -ShowVerticalSliceFixture`; R1 comparison | Nine assertions drive crop, chest, natural population, combat, physical loot pickup, replanting, save and relaunch through normal gameplay APIs after fixture creation. The current full world run reports `checks=330 failures=0`. `validation-playable-loop.png` visibly records the open chest with Wheat, hotbar materials and staged world. Base/candidate P95 is `20.956/21.122 ms`, P99 is `25.926/25.178 ms`, and R1 returns `PASS`. Physical input remains R3, so D6 remains `Verify` (updated 2026-08-16). |
| R2 deterministic world soak | `tools\run_world_soak.ps1 -DurationSeconds 1800 -Seed 20260813 -Formal` | Accepted Release schedule-v1 run completes 36,000 fixed ticks, 361 load-centre moves, 1,800 block edits, 900 actor cycles and 179 save/reloads with zero failures. Peak private/working memory is `22,790,144/27,377,664` bytes, peak handles/threads `234/4`, steady growth `4,845,568 bytes/1 handle`; no timeout and both child/wrapper summaries report `PASS` (2026-08-13). |
| R3 physical OIS protocol | `tools\validate_manual_input_record.ps1 -RecordPath docs\manual-input-record-v1.template.txt -AllowNotRun` | Protocol v1 and all 12 ordered case fields pass schema validation. This is deliberately `NOT_RUN`; only a human Release record with `-RequirePass` can close R3, D2, D4 and D6. |
| R5 Windows package | `tools\package_windows_release.ps1 -IncludePack example-stone`; repeated archive build | The isolated 61-file distribution passes exact inventory, validation-only and real three-frame startup, missing-shader and stale-extra-resource negatives. Repeated deterministic ZIPs both hash to `F4F3C448E75031F30EB788FF72C5F22A6A32CDF6C85A90164D1E16B7F807BB69` (2026-08-13). |
| X1-X3 resource packs | `HelloMine3DResourcePackSmoke.exe`; `tools\validate_resource_packs.ps1`; base/packed render and performance runs | Eighteen resolver assertions pass. Actual clients emit 42-entry base and packed effective manifests with SHA-256 `A6B707578452F6A60923E00D8EE33F5EF61CB1DAEE7D43B5928CF7745F144D82` / `F143C49EE6D4139AFCC8B1B23B6EFA01FCE16B28E0EDC1F673430E59CCBCF713`. Both tracked PNGs decode; the example pack changes only Stone ownership/appearance and R1 returns `PASS` (2026-08-13). |

The current 2026-08-12 runs use an NVIDIA GTX 1050 Ti with OpenGL 4.6. The
earlier GDI Generic limitation no longer applies to these hardware-backed PNG
and performance records.

## When To Run

Run the full set before closing any milestone. Run the world runtime smoke
after any change to chunk addressing, chunk storage, block interaction, the
event bus, actors, or terrain generation. Once implemented, also run R1 after
changes that affect timing or residency, R2 after chunk/actor/persistence or
background-worker changes, R3 after player-facing input/UI changes, and R5
after manifest, resolver or packaging changes. Add each accepted run to the
table above rather than relying on an untracked local success.
