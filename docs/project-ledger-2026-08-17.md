# HelloMine3D Project Ledger (Archived 2026-08-17)

> This is the frozen pre-split project ledger. It preserves the detailed
> implementation and acceptance evidence for 61 completed milestones, the
> active tasks as they stood on 2026-08-17, the former validation matrix and
> the former iteration report template. Do not update task status here.
>
> Current work is tracked in `docs/todolist.md`; reusable validation routing is
> in `docs/validation-matrix.md`; new reports use
> `docs/iteration-report-template.md`.

This document previously served as the single execution checklist for the
project. It answered "what is done, what is next, and how is it verified".

| Document | Answers |
| -------- | ------- |
| `docs/todolist.md` (this file) | What is done, what is next, and how each item is validated. |
| `docs/sandbox-foundation-todolist.md` | Detailed record of the S0-S7 sandbox foundation milestones. |
| `docs/ogre-migration-plan.md` | The plan for moving the render backend to Ogre 1.10 (milestones E0-E5). |
| `docs/chunk-streaming-regression.md` | Diagnosis and fix of the 2026-08-07 terrain streaming regression. |
| `docs/iteration-plan.md` | Long-term architectural direction and phase ordering. |
| `docs/runtime-validation.md` | How runtime behaviour is validated and what is not covered. |
| `docs/world-catalogue-contract-v1.md`, `docs/storage-transaction-contract-v1.md`, `docs/world-backup-contract-v1.md`, `docs/world-management-contract-v1.md` | K1-K4 world identity, publication, recovery and player-facing management contracts. |
| `docs/minigame-reference.md` | Which MiniGame architecture points are worth borrowing. |

Status legend:

| Status | Meaning |
| ------ | ------- |
| Todo | Not started. |
| Doing | In progress on the current branch. |
| Verify | Code is done, validation is still pending. |
| Done | Built, verified, and documented. |
| Blocked | Needs a design decision, dependency, or missing tool. |
| Deferred | Intentionally postponed and excluded from the current execution order. |

## Current Baseline

Last refreshed 2026-08-17. Update this when a milestone closes or the active
execution scope changes.

| Area | Current state | Verification |
| ---- | ------------- | ------------ |
| Build system | Premake is the sole project generator. Platform verification wrappers generate one `HelloMine3D` client, thirteen test targets, and the Ogre/OIS static-library graph, then rebuild and test Debug and Release. | `scripts\verify_build.ps1` on Windows or `scripts/verify_build.sh` on Linux/macOS |
| Dependencies | Ogre 1.10 GL3Plus is the sole client renderer, with OIS, ImGui, optional Tracy 0.13.1 and the minimum image/font/archive dependency closure built from vendored source. Tracy is default-off and enabled only for profiling; no checked-in dependency binaries are required. | Debug and Release full-solution builds; `--with-tracy` opt-in build |
| Test targets | Thirteen first-party headless targets cover coordinates, mesh dirty propagation, save compatibility, actor lifecycle, the 351-check world stack, deterministic soak scheduling, resource-pack resolution, strict recipes/crafting, K1 catalogue, K2 transactional publication, K3 backup/restore, Q2 bounded operation timing and H1 crash policy. | The thirteen `HelloMine3D*Tests.exe` / `*Smoke.exe` / `*Soak.exe` binaries listed in `scripts\verify_build.ps1` |
| Runtime validation | `HelloMine3DWorldRuntimeSmoke` drives the real world/actor stack through 351 assertions. Its D6 scenario chains normal gameplay actions after one fixed fixture; G2 adds workbench placement/use/focus/lifecycle coverage; K1 covers version-3 identity, K2/K3 cover transactional save plus backup and Q2 proves enabled/disabled timing leaves real saves correct. | `bin\HelloMine3DWorldRuntimeSmoke.exe` -> `checks=351 failures=0` |
| Render smoke | Ogre terrain, a procedural day/night sky, glass, water, leaves, flora, actors, ore textures, outline and HUD are implemented and validation-only startup passes. Tracked GTX 1050 Ti fixtures remain regression evidence; current Apple M1 Pro captures additionally cover the procedural noon and midnight sky. | `docs/render-regression-smoke.md`, `docs/screenshots/validation-*.png` |
| Performance baseline | Ogre frame/tick/counter collection is wired and the script targets the sole client. Optional Tracy zones cover the frame, simulation, world and chunk-mesh paths for live diagnosis without replacing the CSV gate. The 2026-08-12 L4 hardware baseline records 60.10 FPS, 17.54 ms frame P95 and no frames over 33 ms. | `docs/performance-baseline.md` |
| Chunk streaming | Six-part regression from `7a229d8` diagnosed and fixed 2026-08-07, then the mesh build was moved off the world lock (M3). | `docs/chunk-streaming-regression.md` |
| Sandbox foundation | All 44 S-milestones are Done. | `docs/sandbox-foundation-todolist.md` |
| Active next scope | Stage 8 has delivered K1-K4, G1-G2 and H1, plus Q1's portable contract and Q2's portable instrumentation/fixtures. G3 tool progression is the next gameplay batch; H2-H3, Q1/Q2 Windows budgets, Q3 and G4-G6 remain. R3 still closes physical input during the final acceptance batch. | Milestones K, H, Q and G below; ordering in `Recommended Order` |

## Closed Milestones

These were tracked as `T*` items and are now delivered through the sandbox
foundation work. They are kept here as a map, not as pending work.

| Old ID | Task | Delivered by |
| ------ | ---- | ------------ |
| M0.1 | Rename target/scripts/docs to `HelloMine3D` | Done |
| M0.2 | `.gitignore` for generated output | Done |
| M0.5 | Runtime render screenshot smoke | Done, `docs/render-regression-smoke.md` |
| M0.6 | Runtime performance baseline | Done, `docs/performance-baseline.md` |
| T0.1 | `ChunkSection::Layer` solid counts | S0.4 |
| T0.2 | Explicit section dirty flags | S0.4 |
| T0.3 | Block edit mesh update path | S0.5 |
| T0.4 | Boundary neighbour sections dirty | S0.5 |
| T0.5 | No accidental chunk creation | S0.2 |
| T0.6 | Debug visibility for mesh updates | S0.4, S7.1 |
| T1.1 | Negative coordinate policy | S0.1 |
| T1.2 | Spawn chunk preloading | S0.6 |
| T1.3 | Split chunk query and creation APIs | S0.2 |
| T1.4 | Loader thread stops reading `Camera&` | S0.3 |
| T1.5 | Chunk load budget and counters | S0.3, S0.4 |
| T2.3 | `BlockDefinition` static data boundary | S3.1 |
| T2.4 | Stable string IDs for blocks | S3.1 |
| T3.5 | Explicit render pass separation | S3.2 |
| T4.1 | `metadata` on `ChunkBlock` | S3.3 |
| T4.3 | `BlockRenderInfo` | S3.2 |
| T4.5 | Plan `BlockEntity` without implementing broadly | S3.6 |
| T5.1 | World seed in config and generator | S6.1 |
| T5.2 | Dirty chunks separate from dirty sections | S2.4 |
| T5.3 | Versioned chunk file format | S2.3 |
| T5.4 | Save and load modified chunks | S2.4, S2.5 |
| T5.5 | Player position save | S2.6 |
| T6.1 | Split terrain base and decorators | S6.2 |
| T6.2 | Biome data definitions | S6.3 |
| T6.3 | Ore decorator | S6.4 |

## Milestone V: Close The Remaining Validation Gaps

Goal: retire the last three `Verify` items and the assertions that were never
written. Small, cheap, and it keeps the foundation honest.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| V1 | Done | Assert the 20Hz simulation tick rate (S1.5). | `FixedTickScheduler` is shared by `SandboxRuntime` and the headless validation. `RuntimePerformanceCapture` also records per-frame ticks and the baseline script rejects rates outside 19-21Hz. | `V1/fixed-tick-scheduler-20hz` observes exactly 200 ticks over 10 seconds; `V1/fixed-tick-catchup-bounded` verifies the five-tick catch-up cap. |
| V2 | Done | Validate `PlayerController` input (S5.3). | `PlayerInputState` separates OIS device collection from deterministic control application. Frame sampling is idempotent, movement is applied once per fixed tick, horizontal velocity approaches a bounded target, diagonal input is normalized, grounded Shift is held-to-sneak, and the render camera interpolates between simulation poses. Mouse sensitivity and optional Y inversion are runtime config values. | `V2/*` covers fixed-tick movement, flying jump, held sneak, normalized diagonal/sprint speed, repeated-frame sampling, camera interpolation, look delta and hotbar selection in `HelloMine3DWorldRuntimeSmoke`. |
| V3 | Done | Screenshot the sandbox debug panel (S7.1). | `HELLOMINE3D_SHOW_DEBUG_INFO` enables the panel before the first frame; `run_render_capture.ps1 -ShowDebugInfo` sets it. Truthy/falsey parsing and environment wiring pass four headless assertions. | `docs/screenshots/validation-skybox-panel-outline.png` is a GTX 1050 Ti / OpenGL 4.6 runtime readback with live chunk, section, mesh and actor counters. |
| V4 | Done | Assert height map correctness after edits (old T0.7). | The generated cache, removal of the highest opaque block, and placement above the old top are each compared with a full vertical scan. `Chunk::getHeightAt()` is now a const query. | `V4/generated-height-matches-scan`, `V4/break-highest-updates-height`, and `V4/place-above-updates-height` pass in `HelloMine3DWorldRuntimeSmoke`. |
| V5 | Done | Add a thread-stress check for the background loader (S0.3). | `World::getBlock()` now locks chunk-map reads while mesh snapshots use a private already-locked path. The headless scenario changes the load center 240 times, performs concurrent reads, checks section-state invariants, then requires new mesh progress at a fresh stable center. | `V5/load-center-churn-completes`, `V5/concurrent-block-reads-valid`, and `V5/background-loader-makes-progress` pass in `HelloMine3DWorldRuntimeSmoke`. |

## Milestone E: Ogre Engine Migration

Goal: move the render backend from `SFML + raw OpenGL` to Ogre 1.10, reusing
HelloOgre3D's engine tree and premake definitions. Full plan, facts baseline,
design decisions and risk register live in `docs/ogre-migration-plan.md`.

Decided 2026-08-07. Scheduled **before** the lighting work, because both rewrite
the same mesh/shader code and doing it twice is waste.

| ID | Status | Task | Validation |
| -- | ------ | ---- | ---------- |
| E0 | Done | Decouple the data layer from SFML and OpenGL. Block and chunk coordinates use `glm::ivec2/ivec3`; CPU mesh data uses `float`/`std::uint32_t`; mouse, keyboard and timing stay at the sandbox boundary. | Static scan finds no renderer types in `World/`; Debug and Release builds plus all five tests pass. |
| E1 | Done | Bring Ogre into the build (minimum subset: `ogre3d`, `ogre3d_glsupport`, `ogre3d_gl3plus`, FreeImage chain, dedicated Ogre FreeType, zlib, zzip and OIS). The solution is unified on x64, C++17, static CRT and MBCS; E5 has since made this the sole client graph. | Premake generation, Debug/Release full builds, all five test binaries and the GTX 1050 Ti / OpenGL 4.6 render/performance smokes pass (`checks=239 failures=0`). |
| E2 | Done | The Ogre shell owns `Root`/`RenderWindow`/`SceneManager`/`Camera`/`FrameListener`, resource config, OIS input and the built-in skybox geometry. It was introduced as a parallel bootstrap and became `HelloMine3D.exe` in E5. The current sky fragment program renders the W1 gradient, sun, moon and procedural stars without cube textures. | Validation-only startup registers GL3Plus, two resource locations and OIS; V2 validates the controller seam. Apple M1 Pro / OpenGL 4.1 noon and midnight readbacks verify the current procedural sky, while `docs/screenshots/validation-skybox-panel-outline.png` remains historical evidence for the replaced cube-map implementation. |
| E3 | Done | `ChunkSectionRenderable` derives from Ogre's `SimpleRenderable` (`MovableObject` + `Renderable`), owns interleaved position/atlas-UV/repeat-UV/combined-light vertex and 32-bit index buffers, uses a local section AABB, and is attached to one scene node per section. `ChunkMeshBuilder` feeds the terrain material; `DefaultPack.png` is loaded through the Ogre resource group with `filtering none`. | Debug/Release validate every CPU mesh stream; the hardware terrain capture and 10-second performance baseline pass after a focused migration-closure review. |
| E4 | Done | Water and flora use dedicated animated Ogre vertex programs and separate render queues. The Ogre UI platform layer feeds OIS input into ImGui, draws a persistent crosshair/hotbar, submits debug panels after the scene render queues, and preserves the F1 startup/toggle behavior. Screenshot capture uses `RenderWindow::writeContentsToFile`, and performance sampling runs from the frame listener. | Fixed-seed Debug/Release validation covers every render layer, HUD state and capture schedule. Hardware PNGs show the HUD, debug panel, water, glass and flora; the L4 CSV records 60.10 FPS and 17.54 ms frame P95. |
| E5 | Done | `HelloMine3D.exe` is the sole Ogre client. SFML, glad, the old renderer/shader/texture/GL/input shells and coexistence switch are removed. `SandboxRuntime` is platform-independent; versioned CPU mesh snapshots keep Ogre uploads safe from loader-thread edits; selection feedback uses `OgreBlockOutline`. Scripts, README and validation commands use the single entry point. | Premake generation, Debug/Release full builds, all five tests, validation-only startup, hardware render captures and the performance baseline pass; world runtime reports `checks=239 failures=0`. |

E0 is worth doing regardless of whether the migration proceeds — it removes a
real coupling problem and simplifies headless testing. Everything from E1 on is
reversible via the coexistence switch until E5.

## Milestone P: First Playable Foundation

Goal: close the three "partly met" criteria in the sandbox foundation target.
Entity rendering is the biggest single gap in the project right now.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| P1 | Done | Render actors. | `ActorSnapshot` exposes immutable id/type/transform/bounds data. `OgreActorRenderer` mirrors live snapshots into green mob and amber item cube nodes, updates transforms every frame, and removes visuals when an actor dies or is picked up. `run_render_capture.ps1 -SpawnValidationActors` creates a deterministic mob/item pair in front of the player and freezes capture-only simulation for a stable frame. | Five `P1/*` lifecycle assertions pass in Debug and Release; validation-only startup reports `mob=1`, `item=1`. `docs/screenshots/validation-actors.png` shows both actors in an OpenGL 4.6 runtime readback. |
| P2 | Done | Persist entities. | Version 2 introduced live `ActorSaveState` records; current version 3 retains them alongside K1 catalogue identity. Virtual subtype state preserves mob health/wander/drop configuration and item material/amount/pickup delay; transforms and velocity are shared. Restore rejects invalid/duplicate ids and advances the allocator. Version 1 saves load with no actors and upgrade on the next save. | Eight `P2/*` assertions save a mob and item, relaunch, compare complete subtype state, prove the next actor id does not collide, and read/upgrade a version 1 fixture; the current full run reports `checks=351 failures=0`. |
| P3 | Done | Add selected-block outline and pick feedback. | `BlockSelectionSystem` supplies one hit result to break/place/use interaction, the debug panel and `OgreBlockOutline`. Water is skipped and placement uses the adjacent voxel. | Five `P3/selection-*` assertions pass in Debug and Release; the yellow selected-block outline is visible in `docs/screenshots/validation-skybox-panel-outline.png`. |
| P4 | Done | Add ore textures. | Dedicated 16x16 coal and iron textures occupy atlas slots `(13,0)` and `(14,0)`; their block definitions no longer reuse stone. `run_render_capture.ps1 -ShowOreFixture` places both ores in front of a pinned camera. | Four `P4/ore-texture-*` and atlas assertions pass in Debug and Release. `docs/screenshots/validation-ores.png` visibly distinguishes coal on the left from iron on the right. |
| P5 | Done | Implement `use` interactions and `BlockUseEvent`. | Right-click queues `use` for the selected block before preserving the existing adjacent placement action. `PlayerDigEvent` routes the action through `BlockInteractionSystem`, which rejects air/water and publishes the target position and block id. | Four `P5/*` assertions cover the interaction seam, exactly one event, target payload and air no-op. Debug/Release full builds, all five tests and validation-only startup pass (`checks=183 failures=0`). |

## Milestone A: Asset And Data Reliability

Goal: asset mistakes should fail early with clear messages.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| A1 | Done | Add `scripts/check_assets.sh`. | The Bash script derives block definitions from `BlockDatabase.cpp`, shape resources from `.block` files, shader sources from the Ogre program script, textures from the material script, and also verifies the bundled font, resource locations and checked-in runtime templates. `--root` supports isolated fixtures without touching live assets. | The repository passes 43 current checks. An isolated copy with one referenced terrain shader omitted reports its exact path and returns 1. |
| A2 | Done | Strengthen `.block` parsing diagnostics. | `BlockData` now requires every field, rejects missing values, unknown/duplicate keys, non-integers, invalid booleans/enums, block ids outside the registry, light outside `0-15` and atlas coordinates outside the 16x16 sheet. `BlockDatabase` additionally checks declared ids against registration and rejects ids reused by another source file. | Five `A2/*` fixtures cover a missing key, invalid enum, out-of-range atlas coordinate, invalid light and duplicate id; every diagnostic contains the full file path and exact key. Debug/Release full builds and all five tests pass (`checks=200 failures=0`). |
| A3 | Done | Decide config and runtime-state ownership (old M0.3/T2.5). | `bin/config.txt` is an ignored per-user file regenerated from `Config` defaults and read by the Ogre client. `Mine.cfg` and `MineResources.cfg` are the only tracked `bin/` templates; logs, executables, ImGui state, saves, captures and legacy `info.txt` stay ignored. | Three `A3/*` assertions delete an isolated config, verify the documented `8 / windowed / 1280x720 / 90 / random seed` defaults, then load customised values (`checks=183 failures=0` in Debug and Release). |
| A4 | Done | Move world seed into the config file. | `Config::worldSeed` is optional; generated configs write `seed random`, while an integer seeds new worlds. Existing saves remain authoritative and `HELLOMINE3D_SEED` stays as the automation override. | Three `A4/*` assertions load seed `20260811`, create two fresh worlds with no seed environment variable, and compare 2,299 terrain samples with zero mismatches (`checks=183 failures=0` in Debug and Release). |

## Milestone B: Build And Platform Reliability

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| B1 | Done | Retire the stale nested CMake cache risk. | E5 removed the SFML external project; all active dependencies are generated directly by Premake. | `vs2022.bat` produces no `External/sfml` target or CMake cache. |
| B2 | Done | Document and script build verification (old M0.4). | `verify_build.ps1` uses bundled Premake, locates VS2022 MSBuild, rejects OIS demo sources, builds Debug/Release x64 and runs all thirteen current tests after each build. After the Release rebuild it also runs H1's intentional-crash harness. `verify_build.sh` applies the portable build/test contract through Premake/Make on Linux and macOS. Non-Windows generation excludes the Ogre Windows resources and PCH contract; `build.sh` selects Premake's native `<config>_x64` name. README lists one command and its host prerequisites per path. | A full macOS Make Debug build produces the client plus the current tests. The last recorded Windows wrapper run completed both full builds and fourteen executions for the then-seven-test graph. The thirteen-test plus H1 controlled-dump contract requires the next Windows run; the matching macOS Xcode gate is the native portable acceptance path. |
| B3 | Done | Verify the macOS path. | Platform macros and header paths are scoped per host; Ogre core/GLSupport receive their OSX headers, OIS receives Cocoa headers, and the stale WiiMote demo is excluded. A shared version-1 contract fixes the exact 31-project workspace/on-disk inventory. The gate parses every PBX group and `ProjectRef`, then parallelizes quiet builds and rejects stale/missing projects, duplicate/multi-group references, deprecated manual ordering or any first-party compiler warning. | Nine graph/contract fixtures pass, and the H1 graph contains exactly 31 projects, 354 groups, 2,578 memberships and 54 cross-project references. The full Apple M1 Pro Debug/Release client plus thirteen-test gate, 26 executions, two validation-only starts and two real Cocoa starts passed under `build/xcode-validation-20260817075257`; target-Windows PowerShell consumes the same contract on its next run. |
| B5 | Done | Fix the render capture script race. | Runtime polling validates the PNG signature, all chunk boundaries and terminal `IEND` before accepting a file, so a non-empty partial write cannot make the script stop the client. Relative output/save paths are normalized against the repository root. The native process handle is primed immediately and an exited process is fully joined before its exit code is read. A clean exit still receives a two-second visibility grace period; non-zero exits and incomplete files fail. | The 2026-08-12 hardware runs exposed a truncated 40 KB PNG and PowerShell 5 losing `Handle/ExitCode` for a fast process. `-ValidateCapturePolling` rejects the incomplete fixture and passes ten partial-to-complete writer races. GTX 1050 Ti / OpenGL 4.6 then completes ten consecutive captures, each independently decoded by ffmpeg (`46,343-49,604` bytes; `runs=10 status=PASS`). |
| B4 | Done | Remove dead build artifacts. | Removed ignored outputs for the deleted `HelloMine3DFoundationRuntimeSmoke` target and the pre-E5 standalone `HelloMine3DOgreBootstrap` target. Both verification wrappers now reject any `HelloMine3D*` executable outside the current client-plus-thirteen-tests allowlist. | The current graph contains exactly the client and thirteen test executables. The updated Windows allowlist includes G1 RecipeSmoke, K1-K4 persistence/management, Q2 operation timing and H1 crash diagnostics coverage. |

## Milestone M: Mesh Pipeline And Performance

Goal: make section mesh rebuilds measurable and then faster. The performance
baseline tool already exists, so every item here has a before/after number.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| M1 | Done | Add mesh build metrics. | Both background streaming and synchronous edit rebuilds measure accepted per-section build time. Debug stats accumulate total/latest/max milliseconds plus solid/water/flora face and vertex counts; the Ogre debug panel, frame CSV and summary expose them, including average build time. | Five `M1/*` assertions verify rebuild, timing and geometry counters in Debug and Release (`checks=183 failures=0`); validation-only startup and performance-script syntax pass. A fresh hardware performance capture can consume the new columns without changing the script. |
| M2 | Done | Convert dirty sections into a bounded queue. | Dirty section coordinates enter a deduplicated FIFO rather than an unordered pointer batch. `World::updateChunks()` resolves each coordinate safely and processes at most two entries per frame, so unloaded chunks leave no dangling pointers and edit bursts spread their mesh work across frames. | Five `M2/*` assertions queue five independent sections, preserve FIFO order, cap the first frame at two rebuilds, drain as `2 + 2 + 1`, and prove duplicate edits/empty updates add no work (`checks=183 failures=0` in Debug and Release). |
| M3 | Done | Add an 18x18x18 halo cache for mesh building. | `SectionMeshInput` snapshots the neighbourhood under the world lock; `ChunkMeshBuilder` then builds from the snapshot with no world access. Stale results are rejected by a per-section block revision. Also fixed a block pointer desync that rendered underground blocks at the surface, throttled the unload scan to camera chunk changes, and removed the dead `makeMesh`/`makeMeshes` chain. | `HelloMine3DWorldRuntimeSmoke` `M3/*` (93 checks total); `update_p95_ms` 0.004 ms in Release; 2013 mesh rebuilds vs 1598 in a same-conditions A/B. |
| M4 | Done | Implement opaque cube greedy meshing. | Six directional masks merge only matching opaque cube blocks in the terrain pass. Merged quads carry a separate repeat-UV stream so each atlas tile retains block-scale sampling; transparent cubes, water and crossed flora keep their original topology. | Six `M4/*` assertions reduce an `8x1x8` stone slab from 160 naive faces to 6, preserve a stone/dirt boundary at 10 faces, and keep water/flora separate. Debug/Release full builds and all tests pass; validation-only terrain falls from 20,796 to 8,396 vertices while water/flora counts remain unchanged. Hardware GL3+ captures retain block-scale atlas repetition without stretched tiles. |
| M5 | Done | Investigate unmeshed sections. | Diagnosed as a six-part regression from `7a229d8`, not a budget choice. Fixed 2026-08-07; Release now meshes and uploads all 2013 sections inside the run. | `docs/chunk-streaming-regression.md`; baselines `perf_baseline_20260807204353319-60596` (Debug) and `perf_baseline_20260807204435048-12972` (Release). |
| M6 | Done | Skip fully enclosed sections. | `SectionMeshInput::needsMeshBuild()` reuses the per-layer opaque counts to detect a section sealed on all six sides. Background streaming completes it under the world lock without scheduling a builder, while synchronous edit rebuilds return without recording an empty build. Opening any boundary restores normal meshing. | Nine `M6/*` assertions verify background and synchronous skips emit no faces and do not increment metrics, then open one top neighbour and require exactly one visible face and one recorded rebuild. Debug/Release full builds, all five tests and validation-only startup pass (`checks=183 failures=0`). |
| M7 | Done | Prioritise mesh builds by view frustum. | The main thread publishes a mutex-protected `ViewFrustum` copy and only bumps the work-order revision after a 5-degree turn or vertical-section change. The worker sorts, never filters: intersecting section columns come first, followed by squared distance, Manhattan distance and stable coordinates. Missing snapshots retain the original distance order. | Four `M7/*` assertions plan all 1,089 chunks at view distance 16, place forward terrain before rear terrain, reverse the order after a 180-degree turn, and verify distance fallback. Debug/Release full builds, all five tests and validation-only startup pass (`checks=183 failures=0`). |

## Milestone L: Light And Visual Feedback

Goal: visual quality, after the mesh pipeline is stable. Sunlight, block light,
edit-time relighting and refined transparency are now complete.

| ID | Status | Task | Validation |
| -- | ------ | ---- | ---------- |
| L1 | Done | Add sunlight storage. Each section owns bounded `0-15` sunlight values; generated and loaded chunks rebuild direct column sunlight from block data, and the 18x18x18 mesh snapshot carries it across section boundaries. Terrain vertices combine directional shading with sunlight, opaque greedy faces split at light boundaries, and flora samples its own cell. Sunlight remains derived rather than serialized; edit-time relighting is intentionally deferred to L3. | Nine `L1/*` assertions cover bounds, open and roofed columns, halo propagation, surface/cave brightness, greedy boundaries, save and load rebuilds. Full VS2022 Debug/Release builds and all five tests pass (`checks=192 failures=0`). A GTX 1050 Ti / OpenGL 4.6 PNG shows bright open terrain and darker opaque occlusion; the matching 10-second baseline records 60.14 FPS, 17.75 ms frame P95, zero frames over 33 ms and 0.466/1.363 ms average/max section mesh build time. |
| L2 | Done | Add block light storage. `.block` files require a bounded `Light 0-15` field and the rose emits level 14. Each section stores block light separately from sunlight; generated and loaded chunks rebuild six-direction attenuation, the 18x18x18 snapshot carries both channels, and mesh faces use the stronger source. Greedy faces include the combined value. Storage remains derived; cross-chunk and edit-time propagation are deferred to L3. | Seven `L2/*` assertions cover data-driven emission, `14/13/12` falloff, opaque blocking, halo propagation, sunlight combination, a level-13 neighbouring mesh face and load-time rebuilding. The added A2 fixture rejects level 16. Full VS2022 Debug/Release builds and all five tests pass (`checks=200 failures=0`). The GTX 1050 Ti / OpenGL 4.6 render capture decodes cleanly; the matching baseline records 60.14 FPS, 17.69 ms frame P95, zero frames over 33 ms and 0.467/0.988 ms average/max mesh build time, with unchanged geometry counts from L1. |
| L3 | Done | Add local relight after edits. A block edit recomputes only its direct-sunlight column. Block light uses two local queues: one removes the obsolete descending gradient, then preserved or emissive sources refill reachable cells. Propagation crosses loaded chunk/section boundaries; chunk load joins boundary light and unload removes orphaned light. Changed light cells deduplicate to nearby mesh sections, whose input revision advances so an in-flight stale mesh cannot overwrite the relight. | Nine `L3/*` assertions place a level-14 emitter on a chunk edge, observe level 13 across the boundary, verify a light-only revision change and only four queued sections, place/remove an opaque blocker, place/remove a sunlight roof, reconcile after reload, and remove cross-chunk light after source unload. Full VS2022 Debug/Release builds and all five tests pass (`checks=209 failures=0`). The hardware PNG decodes cleanly; the 10-second baseline records 60.09 FPS, 17.58 ms frame P95, zero frames over 33 ms and 0.477/0.998 ms average/max mesh build time. |
| L4 | Done | Tighten transparent block rules. The two existing glass definitions are now registered blocks and inventory materials, use a dedicated alpha-blended mesh/material, disable depth writes and share Ogre's late transparent queue with water so distance sorting covers both. Leaves stay in the static alpha-cutout terrain pass, while crossed flora keeps its animated pass. Same-id cubes and both glass variants suppress shared faces, including across chunk boundaries; flora never hides an adjacent cube face. Mesh diagnostics and performance CSVs expose glass separately. | Ten `L4/*` assertions cover definitions/material roundtrips, same/mixed glass face culling, opaque/glass ownership, leaf and flora routing, water routing and cross-chunk glass. Full VS2022 Debug/Release builds and all five tests pass (`checks=219 failures=0`). Validation-only Ogre reports one transparent section with 68 vertices/102 indices. A deterministic GTX 1050 Ti / OpenGL 4.6 fixture shows glass, leaves and flora and decodes through ffmpeg. The matching 10-second baseline records 60.10 FPS, 17.54 ms frame P95, zero frames over 33 ms and 0.480/1.169 ms average/max mesh build time. |

## Milestone C: Content And Behaviour Expansion

| ID | Status | Task | Validation |
| -- | ------ | ---- | ---------- |
| C1 | Done | Introduce lightweight `BlockBehavior`. Every definition owns a behavior strategy with default drop, placement, break and use hooks. `BlockInteractionSystem` dispatches through that strategy and contains no per-special-block branch. Behaviors are owned by the registry beside their definitions; glass installs a no-drop strategy only at registration time, while ordinary blocks inherit the default definition-backed drop. | Four `C1/*` assertions prove every registered definition has a behavior, ordinary stone preserves its configured drop, glass overrides that drop without changing its static material definition, and the real break path neither adds glass to the player nor spawns an item. Full VS2022 Debug/Release builds and all five tests pass (`checks=223 failures=0`). |
| C2 | Done | Add one metadata-backed block behaviour. Tall-grass metadata now records an explicit immature or mature stage. The registry assigns a maturity-aware drop strategy to the single `TallGrass` id: immature player-placed grass drops nothing, while mature naturally generated grass uses the definition's normal drop. All grass-producing biome definitions emit the named mature state, without adding a second block id or an interaction-system branch. | Four `C2/*` assertions prove both states share one id but select different drops, a biome emits mature grass, the real immature break path creates no inventory item or actor, and mature grass drops one item. Full VS2022 Debug/Release builds and all five tests pass (`checks=227 failures=0`). |
| C3 | Done | Resource-drive block shapes. `MeshType 1` is now a generic resource mesh whose block definition names a `Shape`. Strict `.shape` parsing accepts one or more normalized four-vertex `Face` records. Tall grass, roses and dead shrubs share `Cross.shape`; the mesh builder iterates the resolved faces and no longer contains crossed-quad constants or a shape-specific branch. Cube faces and greedy merging remain on their optimized path. The asset checker now discovers shape references and also recognizes multiline block registrations. | Four `C3/*` assertions verify flora definitions resolve `Cross`, its exact two faces come from the resource, an isolated single-quad resource parses without a builder change, and the real section mesh emits two flora faces/eight vertices. The asset reference equivalent passes 45 checks. Full VS2022 Debug/Release builds and all five tests pass (`checks=231 failures=0`). |
| C4 | Done | Add a cave generation pass. `CaveGenerator` samples a seeded, continuous three-dimensional value-noise field in world coordinates, so the result does not depend on chunk generation order. Classic generation now runs base terrain, caves, ores, plants and trees as explicit passes. The carver only replaces stone and protects the top five blocks below each column surface, eight blocks below sea level and the bottom eight layers; ores are placed afterward into remaining stone. | Three `C4/*` assertions find 3,766 underground air cells across nine sampled chunks, prove the full cave index layout repeats for the same seed, and find zero air cells in the protected surface layers. Existing terrain equality, biome surface, structure persistence and deterministic coal/iron checks remain green. Full VS2022 Debug/Release builds and all five tests pass (`checks=234 failures=0`). |
| C5 | Done | Support cross-chunk structures. Structure roots are selected deterministically from the world seed and world block coordinates. Each target chunk enumerates roots in a six-block halo, reconstructs their source biome and interpolated surface height, then projects only the overlapping structure blocks into itself. Per-root random streams and a stable scan order make the result independent of adjacent-chunk load order. | Two `C5/*` assertions find 100 tree-block links across sampled chunk boundaries and compare all 20,480 blocks in each of two adjacent chunks after forward and reverse generation. The existing save/reload assertion also preserves all 100 links. Full VS2022 Debug/Release builds and all five tests pass (`checks=236 failures=0`). |
| C6 | Done | Add mob chase AI and damage invulnerability frames. Spawned and restored mobs retain a non-owning player target, move horizontally toward it inside a 12-block radius at 2.4 blocks per second, stop 0.75 blocks away, and preserve wandering outside that range. `LivingActor` starts a 0.5-second fixed-tick cooldown after accepted damage; hits during the cooldown change neither health nor events, and transient cooldown state resets on load. | Three `C6/*` assertions prove one tick reduces a four-block player distance from 4.00 to 3.88, an immediate repeat hit leaves health and the event count unchanged, and damage is accepted after eleven ticks. The focused entity lifecycle smoke covers the direct event-bus damage overload. Full VS2022 Debug/Release builds and all five tests pass (`checks=239 failures=0`). |
| C7 | Done | Add bounded random ticks. `BlockBehavior` opts individual block states into random ticks; each section indexes only those instances, and `World` rotates a deduplicated active-section queue with a four-section budget per 20Hz fixed tick. Each visit samples three deterministic, uniformly distributed voxel positions rather than guaranteeing a sparse block an immediate callback. Immature tall grass grows to the existing mature metadata state, while mature and unrelated blocks leave the queue. Chunk unload removes its index and storage reload rebuilds it from block data. The debug panel, performance CSV and summary expose active blocks/sections, latest processed sections and cumulative dispatches. | Six `C7/*` assertions construct deterministic sampled positions and cover metadata opt-in, five active sections without a world scan, the four-section budget, round-robin drain, unload cleanup and storage reload. The Windows Debug/Release gate builds the full solution and passes all five tests in each configuration (`checks=245 failures=0`). |

## Milestone W: Windows-First Follow-Up

Goal: continue the remaining low-risk single-player directions on Windows while
the completed macOS-native B3 gate protects the shared generated-project graph.
These tasks must retain portable core boundaries even though the current
acceptance host is Windows.

| ID | Status | Task | Validation |
| -- | ------ | ---- | ---------- |
| W1 | Done | Add a time-of-day, fog and procedural-sky parameter channel without rewriting stored sunlight. The portable 24,000-tick model supplies daylight, horizon/zenith colour, fog, sun direction and sun/moon/star intensity. Ogre applies the values to terrain, water, flora, actors, scene fog and all six Ogre-generated skybox plane materials; the fragment program renders a gradient, sun disc, moon and procedural stars. `HELLOMINE3D_WORLD_TIME` / `run_render_capture.ps1 -WorldTime` pins a repeatable frame. | Ten `W1/*` assertions cover anchors, wrapping, bounds, twilight continuity, automation override, stats, fog/horizon continuity and opposite noon/midnight sun directions. The current world run passes `checks=351 failures=0`. Apple M1 Pro / OpenGL 4.1 noon and midnight readbacks were independently inspected on 2026-08-16; the tracked GTX 1050 Ti captures remain historical evidence for the prior skybox. |
| W2 | Done | Surface resource/bootstrap failures in a user-facing startup error view while preserving complete stderr diagnostics. A pre-window critical-resource check classifies missing shader, texture and block files. The outer Ogre/OIS/standard-exception boundary writes the complete diagnostic to stderr, then ordinary Windows startup presents the same text through `MessageBoxW`. Validate-only mode stays non-interactive; a dedicated test hook records and suppresses the dialog without changing its payload. | `validate_startup_errors.ps1` creates three isolated roots and omits one real required file per case. Shader, texture and block cases each return 1, name the category and exact relative path in stderr, and record `ui=MessageBoxW`, `dialog_requested=true` plus the same diagnostic. The full Windows gate repeats all three cases after both Debug and Release builds. |
| W3 | Done | Add a generated resource manifest before attempting resource packs. The deterministic generator discovers registered blocks and their shapes, Ogre shader and texture references, the font, base recipes, resource scripts and runtime templates, then writes a sorted unique inventory. Windows startup strictly parses the checked-in manifest before Ogre construction and requires every effective listed file to exist and be non-empty. | G2's appended workbench brings the current manifest to 38 entries: 19 blocks, one shape, 10 shaders, one texture, one font, one base recipe, two resource scripts and three runtime templates. Manifest check and hidden no-pack startup pass. |
| W4 | Done | Measure the current terrain vertex layout and compress it only when the bandwidth/memory evidence justifies the added shader complexity. The unchanged interleaved layout is `float3` position, `float2` atlas UV, `float2` repeat UV and `float1` combined light: 32 bytes per vertex with four-byte indices. Live Ogre renderables now feed current resident counts and byte estimates to the debug panel and performance CSV/summary. | Two `W4/*` assertions and compile-time checks fix the 32/4-byte strides and estimation arithmetic. Debug/Release validation reports 27,176 vertices, 40,764 indices and 1,032,688 bytes (0.985 MiB) for the deterministic fixture. The archived L4 cumulative geometry is a conservative 12.456 MiB upper bound while already sustaining 60.10 FPS / 17.54 ms P95 with no frames over 33 ms. A hypothetical 20-byte packing saves at most 3.934 MiB against that overestimate, so no layout or shader change is justified; the full Windows gate passes `checks=255 failures=0`. |

## Milestone D: Playable Loop

Goal: turn the verified sandbox systems into one player-facing vertical slice.
Each item must use the existing portable world, event, persistence and input
boundaries; validation-only fixtures do not count as a completed gameplay
feature.

| ID | Status | Task | Depends on | Required acceptance |
| -- | ------ | ---- | ---------- | ------------------- |
| D1 | Done | Replaced the passive record placeholder with chunk-owned create/find/update/remove operations and world-coordinate wrappers. Records use unique local positions and bounded namespaced types/payloads; invalid, duplicate, out-of-range, air-attached and oversized state is rejected atomically. Replacing a block removes its state, and dirty ownership stays with the chunk. The version 2 format remains unchanged while version 1 loads with empty state and zero metadata. | S3.6, P2, C1 | Eleven `D1/*` assertions cover creation, lookup, update, duplicate/type/position rejection, atomic failed loads, unload/reload, block replacement and safe removal. The focused save/load smoke writes and reads an explicit version 1 fixture. The full Debug/Release Windows gate passes with `checks=266 failures=0` (2026-08-13). |
| D2 | Verify | Added a registered chest block backed by the D1 record lifecycle and a versioned nine-slot payload. P5 right-click opens an ImGui container session; an open session suppresses movement, look, break and place, while Escape/Close returns to gameplay. Click transfers enforce material stack limits and publish inventory changes. Breaking returns the chest block and spills each occupied content slot as an item entity before state removal. | D1, P5, S5.4 | Twelve `D2/*` assertions cover placement/open, store/take conservation, inventory events, capacity, malformed payloads, unload/reload, world-action suppression and the spill/remove policy. The full Debug/Release gate passes with `checks=278 failures=0`; `validation-container.png` is a decoded GTX 1050 Ti / OpenGL 4.6 Release capture. Only the non-automated R3 keyboard/mouse focus-and-Escape record remains before `Done` (2026-08-13). |
| D3 | Done | Added deterministic 20-tick natural-mob population around the player with 16 bounded candidates, a four-Mob local cap, a 12-Mob world cap, collidable-ground/two-block-headroom checks and `ActorManager` lifetime ownership. Unloading a chunk despawns its natural Mob residents; world-save restore requires a loaded owner chunk and rejects spatial duplicates even when their IDs differ. Debug and performance channels expose live count, both caps, attempts, additions and removals. | P1, P2, C6 | Thirteen `D3/*` assertions cover deterministic candidates, the spawn ring, formal live spawning, safe placement, local/world caps, unload/reload, debug counters, four-Mob persistence and corrupt duplicate-state rejection. The full Debug/Release gate passes with `checks=291 failures=0`. `validation-natural-mobs.png` is a decoded GTX 1050 Ti / OpenGL 4.6 Release readback showing a naturally spawned green Mob and `4 / 12` live/capped counters. The matching 60.1 FPS sample records P95/P99 `16.693/17.633 ms`, zero long frames and an R1 comparison `PASS` (2026-08-13). |
| D4 | Verify | Added unified actor/block targeting with nearest-hit occlusion and an analytic ray/AABB actor test. Left-click attacks living actors through the existing damage/invulnerability path; chasing Mobs reach contact range and damage the player at the same bounded immunity rate. A persistent health bar and diagnostics expose player health. Damage publishes damage/death/spawn in order, Mob death retains the existing loot path, and player death immediately returns to the saved spawn with zero velocity, closes container UI and explicitly retains inventory. | D3, C6, S4.4 | Fourteen `D4/*` assertions cover targeting priority, accepted/suppressed hits, chase-to-contact damage and cooldown, Mob death/drop ordering, dead-target rejection, player death ordering, respawn, retained inventory and HUD stats. The full Debug/Release gate passes with `checks=305 failures=0`. `validation-combat-hud.png` is a decoded GTX 1050 Ti / OpenGL 4.6 Release readback showing a Mob under the crosshair and `Health 14 / 20`. A same-session D3/D4 A/B records P95/P99 `20.006/23.808 ms` versus `20.497/24.095 ms`, no >50 ms frames and R1 `PASS`. Only the R3 physical attack/input record remains before `Done` (2026-08-13). |
| D5 | Done | Added a complete wheat loop through the existing material, inventory, interaction, behavior, metadata and random-tick boundaries. Mature tall grass yields the initial seed; seeds require dirt or grass support and are consumed only after accepted placement. Four metadata stages grow one step per selected random tick and scale the resource mesh vertically. Immature crops return only seed; mature crops return wheat plus a replant seed. The random-tick section selector now indexes the tracked sparse-block list instead of a raw voxel coordinate, fixing real sparse crops that could otherwise be selected only accidentally. | D2, C2, C7 | Thirteen `D5/*` assertions cover registration, visible stages, seed acquisition, invalid/valid planting, one-step and bounded growth, immature/mature drops, harvest/replant, unload exclusion and save/reload persistence. The generated resource manifest contains 41 sorted entries. Full Debug/Release Windows verification passes with `checks=318 failures=0`; `validation-wheat-crop.png` is a decoded GTX 1050 Ti / OpenGL 4.6 Release capture showing all four stages. A same-session D4/D5 Release comparison with identical residency reports zero >50ms frames and R1 `PASS` (2026-08-13). |
| D6 | Verify | Completed the fixed-seed vertical slice through normal gameplay APIs after initial fixture setup: acquire and plant seed, grow and harvest wheat, transfer it into a chest, run natural population, defeat a Mob through attack/cooldown rules, collect its physical drop, replant, save and relaunch. | D2-D5, R1-R3 | Nine `D6/*` assertions pass in the current `checks=351 failures=0` world run. `validation-playable-loop.png` is a decoded hardware capture; the same-scene Release comparison records base/packed P95 `20.956/21.122 ms`, P99 `25.926/25.178 ms` and R1 `PASS`. R3 physical crop/container/combat input remains the only acceptance gap. |

## Milestone R: Regression And Release Hardening

Goal: make later gameplay changes measurable, repeatable and distributable.
These tasks add gates around existing behaviour; they must not silently change
simulation or rendering semantics.

| ID | Status | Task | Depends on | Required acceptance |
| -- | ------ | ---- | ---------- | ------------------- |
| R1 | Done | Added `compare_perf_baselines.ps1`. New captures record build configuration plus harness scene/VSync/window identity. The comparator rejects identity mismatches or loaded-chunk/section drift beyond 5%, then fails on frame P95 +15%, frame P99 +20%, or a long-frame increase above both two frames and 0.5% of the baseline sample. Exit codes distinguish pass, regression, incomparable and invalid input. | W4, M1 | Four deterministic fixtures cover the real L4 summary self-compare, a P95/P99/long-frame regression, an incomparable scene/residency and a missing metric. `validate_perf_comparison.ps1` passes all four cases and is part of the Windows build gate. Debug/Release builds and all five tests pass. |
| R2 | Done | Added schedule-v1 deterministic world soak at fixed 20 Hz. It repeatedly moves the load centre, edits blocks, exercises Mob/item lifecycle, acknowledges mesh snapshots, saves/reloads and records bounded world plus OS-process counters. The wrapper fails on stalls, invariant/save errors, timeout, queue/actor/chunk caps, 2 GiB memory, 4096 handles or excessive steady growth. | D1, D3, R1 | Accepted Release run `bin/soak_runs/r2_formal_accepted_v2_20260813` completes 1800 seconds/36000 ticks, 361 moves, 1800 edits, 900 actor cycles and 179 save/reloads with zero failures. Peak private memory is 22,790,144 bytes, peak handles/threads `234/4`, steady growth `4,845,568 bytes/1 handle`, child exit 0 and both summaries `PASS`. Generated evidence remains ignored. |
| R3 | Verify | Added physical-input protocol v1, a machine-checkable record template and a validator that rejects incomplete metadata, unknown results and false overall passes. Controller, fixed-tick movement, collision, break/place, combat, container conservation, focus isolation, save/relaunch and clean Cocoa shutdown are automated; the ordered physical run is retained only for OIS device/focus mapping. | V2, D2, D4 | The current world run passes 351 assertions, including swept falling collision, stable contact, input-sampling idempotence, normalized diagonal movement, sprint strafe, held sneak, camera interpolation and G2 crafting focus isolation. A person is still required only to certify physical key/mouse/focus delivery on the target Release build. |
| R4 | Done | Added a dedicated native macOS ThreadSanitizer gate for the real world target. It first requires an isolated calibration race to report and exit 66, then records host/toolchain identity, regenerates Xcode, builds the native host architecture with `ENABLE_THREAD_SANITIZER=YES`, verifies the linked TSan runtime and read/write instrumentation symbols, rejects first-party suppressions and fails on the first project report. V5 remains the Windows stress gate rather than a claimed MSVC sanitizer equivalent. | V5; supported TSan host | Apple M1 Pro arm64 with Xcode 26.2/Apple Clang 17 detects the calibration race, then runs the complete 346-check world stack and all three concurrent V5 loader checks with zero failures and zero project TSan reports. `scripts/verify_tsan.sh` preserves build/run/link/toolchain evidence under `build/tsan-validation-20260817083434`; `docs/thread-sanitizer-validation.md` freezes the boundary. |
| R5 | Done | Added a self-contained Windows x64 distribution directory and deterministic ZIP containing the Release client, 42 base resources, runtime templates, optional packs, README and required third-party notices. Saves, logs, captures, build products and developer files are excluded. The current packager includes Tracy as the fifteenth notice. | D6, W2, W3 | Clean-root exact inventory, validation-only startup, a real three-frame Ogre window, missing-shader and stale-extra-resource negatives all pass. The accepted pre-Tracy optional-pack distribution contains 61 files/14 notices; two rebuilds produced ZIP SHA-256 `F4F3C448E75031F30EB788FF72C5F22A6A32CDF6C85A90164D1E16B7F807BB69`. The next formal Windows package must establish a new inventory/hash. |

## Milestone X: Bounded Resource-Pack Support

Goal: use the completed manifest/preflight work to add a small read-only
resource override layer. This milestone explicitly excludes hot reload,
downloaded packs, executable scripts and arbitrary native plugins.

| ID | Status | Task | Depends on | Required acceptance |
| -- | ------ | ---- | ---------- | ------------------- |
| X1 | Done | Defined resource-pack contract v1: versioned directory metadata, deterministic enabled order and fallback, six allowed resource classes, canonical-path containment, traversal/symlink rejection and exact duplicate diagnostics. Archives, downloads, code and runtime reload remain out of scope. | W3, R5 | `docs/resource-pack-contract.md` records valid/invalid examples. The isolated resolver smoke covers no-pack, precedence/fallback, incompatible version, traversal, stale/empty/missing/duplicate resources and one-time freeze. |
| X2 | Done | Routed block definitions, shapes, textures, shaders, resource scripts and fonts through one startup-frozen effective view without changing gameplay registration. Unoverridden files fall back to base resources; base recipes share the view but remain non-overridable in pack v1. | X1 | Nineteen resolver assertions pass, including explicit rejection of an unversioned recipe override. Missing or invalid sources fail before Ogre construction; the current effective manifest contains 37 sorted entries and identifies `base` or the owning pack. The last formal packed Windows run predates the recipe entry and remains historical evidence. |
| X3 | Done | Integrated packs with manifest generation, startup diagnostics, render/performance capture and R5 packaging. Added the bounded `example-stone` pack, which overrides only the existing Stone definition. | X2 | Base and example-pack hardware PNGs decode with distinct SHA-256 hashes. Their identical-scene Release performance comparison passes; R5 packages the optional pack while retaining exact base-resource validation. |

## Milestone K: Durable World Management

Goal: let a player keep several long-lived worlds without a failed save,
corrupt file or destructive UI action silently removing the last valid copy.
The world-management layer owns filesystem operations; Ogre/ImGui only submits
commands and renders immutable results.

| ID | Status | Priority | Task | Depends on | Required acceptance |
| -- | ------ | -------- | ---- | ---------- | ------------------- |
| K1 | Done | P0 | Added the renderer-independent `WorldCatalogue` and `world.meta` version 3 contract. New worlds receive an immutable canonical id, validated UTF-8 display name, seed, creation/last-played UTC seconds and last-known build identity. Enumeration is bounded and atomic: it accepts version 1/2 through deterministic legacy sentinels without renaming, sorts stably, and rejects duplicate ids, unknown/duplicate fields, invalid UTF-8/integer/time/version boundaries, traversal and symlinks. It never creates or mutates a catalogue. | S2.1, S2.6 | `HelloMine3DWorldCatalogueSmoke` passes 30 focused assertions, including before/after filesystem snapshots, real directory/file symlinks and Q2 success/failure timings. Three real-world assertions prove version-3 creation, quoted display-name round-trip and id stability across relaunch; the existing version-1 fixture remains readable and upgrades on normal save. The contract is frozen in `docs/world-catalogue-contract-v1.md`. |
| K2 | Done | P0 | World and chunk writes now share a synchronous `StorageTransaction`: serialize to a same-volume `.pending` sibling, flush the stream and file descriptor, parse/validate through the real loader, then atomically replace the published path. Every failure moves the candidate into one bounded `.failed` quarantine and preserves the prior generation; stale pending files are quarantined before reuse. Failed dirty chunks remain dirty and cannot be unloaded. | K1, S2.1-S2.6 | `HelloMine3DStorageTransactionSmoke` passes 16 assertions in Debug/Release: all five injected boundaries preserve and reload both old world and chunk generations, remove pending state and quarantine the candidate; validator rejection, stale pending and successful metrics also pass. Existing version-1 fixtures remain compatible, the real world test exposes aggregate total/max save duration, and `docs/storage-transaction-contract-v1.md` freezes the contract. |
| K3 | Done | P0 | Added renderer-independent `WorldBackup` with a strict manifest, exact file inventory, size and content fingerprints, positive configurable limits and oldest-first rotation. Backup/restore staging reuses K2 and real format readers. A candidate is completely re-read before its directory becomes visible; policy rejection cannot misclassify a valid backup as corrupt. Corrupt backups move to one bounded slot; restore preserves the old primary in `recovery.failed`, removes newer chunks and rolls back already-published files on interruption without modifying the selected backup. Explicit successful `World::save()` now publishes a snapshot. | K2 | `HelloMine3DWorldBackupSmoke` passes 19 focused assertions in Debug/Release: the original 16 recovery cases plus complete Q2 success/failure timing records for backup and restore. The complete fixture restores player inventory, actors, chunks, crop metadata and block-entity payloads. The real world stack proves automatic backup publication, and `docs/world-backup-contract-v1.md` freezes the contract. |
| K4 | Done | P1 | Added renderer-independent structured world commands plus the `MainMenu -> WorldList -> Loading -> Playing -> Paused` shell. The ImGui screen lists, creates, opens and renames worlds, exposes backup/recovery actions and confirms destructive operations. Rename preserves id/path; deletion moves into a bounded sibling recovery area. Normal launches show the menu while existing automated save-directory launches remain direct. | K1-K3 | `HelloMine3DWorldCatalogueSmoke` now passes 44 K1/K4/Q2 assertions covering empty/legacy/corrupt/same-name/path-escape catalogues, stable identity, bounded delete/restore, backup restore rollback and valid/invalid application transitions. Debug hidden menu and direct-world smokes pass without foreground activation. The contract is frozen in `docs/world-management-contract-v1.md`; the Release physical-input record remains assigned to R3 rather than reopening K4. |

## Milestone H: Local Crash Diagnostics

Goal: turn an unexpected Windows client failure into a local, symbolizable and
privacy-bounded artifact. Stage 8 does not upload crash data and does not add an
account, telemetry backend or user identifier.

| ID | Status | Priority | Task | Depends on | Required acceptance |
| -- | ------ | -------- | ---- | ---------- | ------------------- |
| H1 | Done | P1 | Integrated a minimal local Windows minidump path after auditing Google Breakpad. The audit selected the already-linked Windows SDK DbgHelp backend and records Breakpad as not selected. The handler installs before Ogre, writes only to a separately validated crash directory, uses a pre-created writer thread, a 15-second wait and a one-shot recursion guard, and has no upload path. | B2, R5 | The portable policy/backend smoke passes 12/0 in Debug and Release. The target Windows Release harness runs the render window hidden in the background, saves the active world, exits non-zero, emits exactly one 117,741-byte dump, reopens the world and leaves no pending save state. `docs/crash-diagnostics-contract-v1.md` freezes the backend audit and evidence boundary. |
| H2 | Todo | P1 | Add a versioned, sanitized crash sidecar and offline symbols. Record exact commit/build id, configuration, OS, GPU/driver, window mode, effective resource-pack manifest, stable world id and the last bounded runtime phase/events. Remove personal absolute paths and unbounded logs. | H1, K1 | Schema fixtures reject missing/duplicate/oversized fields and path leakage. The symbol-generation and symbolization command resolves at least one HelloMine3D frame from the controlled dump and identifies mismatched symbols deterministically. |
| H3 | Todo | P1 | Integrate crash artifacts with clean-root packaging and next-start recovery UX. Keep symbols/developer files outside the distribution, include required third-party notices and offer only local open/copy/dismiss actions on next launch. | H1-H2, K3, R5 | The packaged client passes controlled-crash and ordinary-start probes from an isolated root. Inventory rejects symbols and stale crash files; no network request occurs. Relaunch detects the previous crash without blocking world backup recovery, and package generation remains deterministic. |

## Milestone Q: Expanded Performance Budgets

Goal: preserve responsiveness as saves and gameplay grow. Q extends R1/R2; it
does not replace their existing frame-time comparison or long-running world
invariants.

| ID | Status | Priority | Task | Depends on | Required acceptance |
| -- | ------ | -------- | ---- | ---------- | ------------------- |
| Q1 | Doing | P0 | Define versioned performance scenes, metric schema and approved budgets before new systems change the workload. Schema 3 now freezes cold start, world entry, save transaction, backup restore, fast streaming and scaled gameplay in one JSON contract, including required provenance/identity, metrics, cumulative phase ordering, exact population counts, residency tolerance and approved-baseline budget fields. Python and native PowerShell comparators consume the contract; the macOS gate runs its portable verifier, and the Windows steady launcher now emits complete schema-2 provenance instead of schema 1. Optional Tracy 0.13.1 marks the live hot paths while remaining default-off. | R1, R2, D6 | Eight legacy cases plus 433 derived schema-3 cases pass on macOS: every required field/metric, positive/integer/range/phase rule, absolute budget, identity, exact count, residency rule and inherited frame threshold is exercised. Six tracked baseline summaries are explicitly synthetic fixtures, not approved product limits. The accepted pre-stage-8 macOS/Rosetta sample remains documented; Q1 cannot close until the native PowerShell gate passes on target Windows and real Release captures approve every named budget profile. |
| Q2 | Doing | P0 | Added a renderer-independent, mutex-protected 32-record operation ring for startup, catalogue enumeration, world entry, save transaction, backup and restore. It emits cumulative phases, success, total, longest main-thread stall and exact file/chunk/byte counters into the existing performance summary. Disabled capture reads no clock and retains no record; simultaneous overflow increments a bounded diagnostic. Startup failures flush the summary before any modal error UI. | Q1, K1-K4 | The 12-check focused collector target covers every success/failure summary, schema key, monotonic phase, bound and overflow rule. The combined K1/K4 target passes 44/0 and K3 passes 19/0 with real successful/failed operations; the 346-check world stack proves a real save/backup summary and a second instrumentation-off save. The native macOS window probe requires real startup/world-entry summaries. `docs/operation-performance-timing-contract-v1.md` freezes the contract. Closure still requires Q1's approved target-Windows Release baselines and a native non-zero regression result against them. |
| Q3 | Todo | P1 | Add fast-movement and content-scale gates covering chunk-visible latency, queue peaks, mesh progress, memory/handles and bounded populations of crops, chests, items and Mobs. Re-run the 30-minute soak after K/G persistence changes. | Q1-Q2, G3 | Small/nominal/stress fixtures verify scale identity and cap enforcement. Nominal Release runs stay within approved budgets, stress overload fails with a specific limiting metric, and the formal soak completes without save corruption or unbounded growth. |

## Milestone G: Gameplay Progression And Product UX

Goal: turn the existing crop/container/combat slice into a recognisable early
game loop while reusing the validated interaction, inventory, persistence and
resource boundaries.

| ID | Status | Priority | Task | Depends on | Required acceptance |
| -- | ------ | -------- | ---- | ---------- | ------------------- |
| G1 | Done | P1 | Added a strict startup-only recipe registry and documented format v1. Stable material ids round-trip through `Material`; recipe ids use locale-independent lowercase ASCII; shaped recipes are normalized inside a bounded 3x3 grid; shapeless entries are sorted with bounded counts; sources/final entries are deterministically ordered; and duplicate ids/equivalent patterns fail with first-owner diagnostics. Parsing publishes atomically, freezes once and reads only base-owned `recipe` entries from the already frozen startup view. Two base recipes ship without adding recipe to resource-pack v1's six override classes. | D1, D5, X1-X3 | The complete macOS `scripts/verify_xcode.sh` gate passes in Debug and Release. Each configuration passes 40 recipe assertions and 19 resource-view assertions. Recipe coverage freezes its expected count and now includes ASCII/BOM/CRLF behavior, source/grid/entry/recipe limits and every directive-completeness boundary. The runtime smoke now passes 346/0, and the soak, validation and window probes stay green. The real client reports `effective=37` and `[RECIPE_REGISTRY] frozen=1 recipes=2` before Ogre construction; inventory code remains unchanged. |
| G2 | Done | P1 | Added renderer-independent `CraftingSession`/`CraftingPreview`, revisioned atomic inventory exchange, player 2×2 crafting, appended workbench material/block with 3×3 crafting, and an ImGui grid that only submits commands. The virtual grid never owns player items, so closing, block destruction and save/reload cannot strand hidden stacks. | G1, D2 | `HelloMine3DRecipeSmoke` passes 54 G1/G2 assertions for pure preview, exact matching, capacity after consumption, stale preview rejection, single/batch commit and reload conservation. The 351-check world stack covers workbench placement/use/focus/lifecycle and non-persisted UI state. The client freezes 38 resources and three base recipes; `docs/crafting-contract-v1.md` freezes the boundary. Physical clicks remain assigned to R3. |
| G3 | Todo | P1 | Add a small tool progression: hand plus at least two tool tiers, data-driven mining class/tier, bounded speed multiplier, durability and explicit wrong-tool drop policy. Tool state persists and breaking a tool advances the hotbar safely. | G1-G2, D4-D5 | Tests cover mining-time ordering, tier gating, accepted/rejected drops, exact durability consumption, zero durability, stack restrictions, death/respawn policy and save/reload. Same-scene performance comparison and a real-input mining/combat run pass. |
| G4 | Todo | P2 | Add pause and settings flow for display, render distance, FOV, input sensitivity and audio levels. Apply/revert uses validated bounds, persistence is versioned and pausing a local world stops simulation without corrupting save or UI focus. | K4, R3, Q1 | Config migration and invalid-value fixtures pass. Release input covers open/apply/cancel, focus recovery and return to world/menu; render-distance changes are labelled incomparable until residency stabilizes. |
| G5 | Todo | P2 | Add a minimal renderer-independent audio event channel and basic UI, block, pickup, crafting, combat and ambient feedback. Select a small portable backend separately; missing devices/assets degrade to silence with diagnostics rather than startup failure. Sound files remain base resources unless a future pack contract explicitly versions an audio class. | G2-G4, A1, W3 | Event tests prove accepted gameplay actions emit once and rejected actions emit nothing. Asset/manifest checks cover sound references, resource-pack v1 rejects unversioned sound overrides, mute/volume persistence passes, and a no-device fixture keeps the full gameplay loop operational. |
| G6 | Todo | P1 | Deliver the stage-8 vertical slice from a clean package: create/select a world, gather materials, craft a workbench and tools, mine a gated resource, fight and loot, save/exit, restore a backup after an injected failed save, then continue. Validate the controlled crash path separately without treating a crash as a gameplay action. | K1-K4, H1-H3, Q1-Q3, G1-G5 | Debug/Release gates, focused state-conservation tests, decoded hardware capture, updated physical-input record, deterministic package, controlled dump/symbolization and before/after performance comparisons all pass. No debug-only state injection is allowed after initial fixture selection. |

## Not Recommended Yet

| Direction | Reason |
| --------- | ------ |
| Async chunk IO | Keep K2 transactional saves synchronous first. Consider a command queue only if Q2 proves a material IO stall and a separate design preserves ordering, exit-save and atomic replacement. |
| Multiplayer | Changes world authority, event sync, entity sync, save format and input model. |
| Lua / UGC scripting | First exercise the C++ extension points through D1-D5 and the bounded X resource layer. Script ownership, sandboxing and debugging require a separate design. |
| Resource hot reload, downloaded packs or executable mods | X is deliberately a frozen read-only override layer. Runtime invalidation, network trust and executable extension points are separate systems. |
| D3D / Vulkan backend | Ogre GL3Plus covers the Windows/macOS target (see Milestone E). |
| Large-scale MiniGame code migration | Use it as an architectural reference, not a source. Its engine is a renamed `MINIW` fork whose API has diverged; code cannot be shared. |

## Recommended Order

The selected 13-item D/R/X implementation sequence is complete on Windows.
Stage 8 is now the selected next product milestone. Execute it in this order:

1. **R3 closure** - run protocol v1 with a person on the target Release build; this also closes D2, D4 and D6.
2. **Contracts first** - complete K1, H1 and Q1 so world identity, local crash/privacy boundaries and performance scenes are stable before player-facing expansion.
3. **Protect player data** - K2 fault-safe publication, K3 bounded verified restore and Q2 portable timing gates are complete; approve and run the target-Windows Q1/Q2 Release budgets.
4. **Expose the safe flow** - K4、G1 和 G2 已完成；配方已通过正常 UI/输入边界成为玩家可用的 2×2/3×3 制作流程。
5. **Make failures actionable and add progression** - complete H2-H3 and G3 for symbolizable local crash artifacts plus tool/mining progression.
6. **Polish under budgets** - complete G4-G5 and Q3 for pause/settings, audio feedback and scaled streaming/content performance.
7. **Close stage 8** - complete G6 from a clean package with physical input, save recovery, controlled crash diagnostics and before/after performance evidence.
8. **Sanitizer regression** - B3 and R4 native macOS acceptance are complete. Rerun `scripts/verify_tsan.sh` after background-loader or synchronization changes; it remains independent of the target-Windows gate.

Do not start a dependent item merely because its implementation is convenient;
the dependency and acceptance rows above define when it may close.

## Validation Matrix

Every completed task should list which validations were run.

| Validation | Command | Required for |
| ---------- | ------- | ------------ |
| Windows project generation | `vs2022.bat` | Build system or file layout changes |
| Windows debug build | `MSBuild build\HelloMine3D.sln /p:Configuration=Debug /p:Platform=x64` | All code changes |
| Windows release build | same with `/p:Configuration=Release` | Milestone completion |
| macOS Xcode project | `bash scripts/verify_xcode.sh` | Build system changes and native milestone closure |
| Headless tests | thirteen `bin\HelloMine3D*Tests.exe` / `*Smoke.exe` / `*Soak.exe` binaries | All code changes |
| World runtime smoke | `bin\HelloMine3DWorldRuntimeSmoke.exe` | Chunk, storage, interaction, event, actor or terrain changes |
| Render smoke | `tools\run_render_capture.ps1` | Renderer, shader, texture, mesh upload or frame sequencing changes |
| Performance baseline | `tools\run_perf_baseline.ps1` | Chunk loading, mesh building, update flow or render submission changes |
| Performance comparison | `tools\compare_perf_baselines.ps1` | Every D, R5 or X change that can alter frame timing or world residency |
| Asset check | `bash scripts/check_assets.sh` | Asset and data changes |
| Interactive run | `bin\HelloMine3D.exe` | Input, camera or player-facing UI changes |
| Physical input checklist | `docs\manual-input-acceptance-v1.md`; `tools\validate_manual_input_record.ps1 -RequirePass` | Input, window-system, container UI or combat changes |
| Long-running soak | `tools\run_world_soak.ps1` | Chunk lifecycle, actor lifecycle, persistence or background-loader changes |
| Resource-pack smoke | `tools\validate_resource_packs.ps1` | Manifest, resource lookup, pack or startup-preflight changes |
| Clean-root package smoke | `tools\package_windows_release.ps1` | Release packaging, manifest or resource resolver changes |
| World catalogue fixtures | `HelloMine3DWorldCatalogueSmoke` (30 assertions) | World discovery, metadata, names, ids or world-directory changes |
| Transactional save and restore fault injection | K2's 16-check world/chunk harness plus K3's 19-check backup/restore harness | Save publication, backup, quarantine, restore or format migration changes |
| Local crash artifact smoke | `HelloMine3DCrashDiagnosticsSmoke` (12 assertions); `tools\validate_crash_diagnostics.ps1`; H2/H3 sidecar/symbolization/package extensions remain to be added | Exception handling, crash context, symbols or packaged crash UX changes |
| Formal data-race validation | `bash scripts/verify_tsan.sh` | Background-loader, chunk-map synchronization or worker scheduling changes |
| Startup/save/restore budget | Q1 schema and Q2 bounded phase capture are present; approved target-Windows budgets remain | Catalogue, startup, world-entry, save, backup or recovery changes |
| Streaming/content-scale budget | Q3 scenario extension plus formal R2 soak | Chunk scheduling, gameplay population, persistence or scale changes |
| Stage-8 playable slice | G6 headless fixture, render capture and physical-input record | K/H/Q/G milestone closure |

## Iteration Report Template

```text
Iteration:
Date:
Scope:
Task IDs:

Changed:
-

Validation run:
- [ ] vs2022.bat + solution build
- [ ] Focused headless tests
- [ ] bin\HelloMine3DWorldRuntimeSmoke.exe
- [ ] tools\run_render_capture.ps1
- [ ] tools\run_perf_baseline.ps1
- [ ] R1 performance comparison, when available
- [ ] R2 soak, when required
- [ ] Interactive run
- [ ] R3 physical input checklist, when required
- [ ] R5 clean-root package smoke, when required

Before metrics:
- frame_p95_ms:
- loaded chunks:
- mesh rebuilds:
- validation checks/failures:

After metrics:
- frame_p95_ms:
- loaded chunks:
- mesh rebuilds:
- validation checks/failures:

Known risks:
-

Dependency status:
-

Acceptance evidence:
-

Next recommended task:
-
```
