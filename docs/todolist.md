# HelloMine3D TODO List

This document is the single execution checklist for the project. It answers
"what is done, what is next, and how is it verified".

| Document | Answers |
| -------- | ------- |
| `docs/todolist.md` (this file) | What is done, what is next, and how each item is validated. |
| `docs/sandbox-foundation-todolist.md` | Detailed record of the S0-S7 sandbox foundation milestones. |
| `docs/ogre-migration-plan.md` | The plan for moving the render backend to Ogre 1.10 (milestones E0-E5). |
| `docs/chunk-streaming-regression.md` | Diagnosis and fix of the 2026-08-07 terrain streaming regression. |
| `docs/iteration-plan.md` | Long-term architectural direction and phase ordering. |
| `docs/runtime-validation.md` | How runtime behaviour is validated and what is not covered. |
| `docs/minigame-reference.md` | Which MiniGame architecture points are worth borrowing. |

Status legend:

| Status | Meaning |
| ------ | ------- |
| Todo | Not started. |
| Doing | In progress on the current branch. |
| Verify | Code is done, validation is still pending. |
| Done | Built, verified, and documented. |
| Blocked | Needs a design decision, dependency, or missing tool. |

## Current Baseline

Last refreshed 2026-08-12. Update this when a milestone closes.

| Area | Current state | Verification |
| ---- | ------------- | ------------ |
| Build system | Premake is the sole project generator. Platform verification wrappers generate one `HelloMine3D` client, five test targets, and the Ogre/OIS static-library graph, then build and test Debug and Release. | `scripts\verify_build.ps1` on Windows or `scripts/verify_build.sh` on Linux/macOS |
| Dependencies | Ogre 1.10 GL3Plus is the sole client renderer, with OIS, ImGui and the minimum image/font/archive dependency closure built from vendored source. No checked-in binaries. | Debug and Release full-solution builds |
| Focused tests | 4 headless test targets, all passing. | `bin\HelloMine3DCoordinateTests.exe`, `bin\HelloMine3DMeshDirtyTests.exe`, `bin\HelloMine3DSaveLoadSmoke.exe`, `bin\HelloMine3DEntityLifecycleSmoke.exe` |
| Runtime validation | `HelloMine3DWorldRuntimeSmoke` drives the real world/actor stack through 223 assertions, including configured-seed determinism, generated runtime-config ownership, strict block-definition diagnostics, block-use events and behavior dispatch, frustum-priority ordering, enclosed-section skipping, opaque greedy meshing, sunlight/block-light storage, local relighting and transparent-pass rules, bounded dirty-section processing, mesh build metrics, versioned renderer mesh snapshots, stale-upload rejection, actor render snapshots and actor persistence. | `bin\HelloMine3DWorldRuntimeSmoke.exe` -> `checks=223 failures=0` |
| Render smoke | Ogre terrain, glass, water, leaves, flora, outline and HUD are implemented and validation-only startup passes. GTX 1050 Ti / OpenGL 4.6 captures include the deterministic L4 transparent fixture. | `docs/render-regression-smoke.md` |
| Performance baseline | Ogre frame/tick/counter collection is wired and the script targets the sole client. The 2026-08-12 L4 hardware baseline records 60.10 FPS, 17.54 ms frame P95 and no frames over 33 ms. | `docs/performance-baseline.md` |
| Chunk streaming | Six-part regression from `7a229d8` diagnosed and fixed 2026-08-07, then the mesh build was moved off the world lock (M3). | `docs/chunk-streaming-regression.md` |
| Sandbox foundation | 43 of 44 S-milestones Done, 1 in Verify. | `docs/sandbox-foundation-todolist.md` |

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
| V2 | Done | Validate `PlayerController` input (S5.3). | `PlayerInputState` separates OIS device collection from deterministic control application, so tests inject input without a window or mouse warp. | `V2/*` covers movement, flying jump, fly/sneak toggles, look delta and hotbar selection in `HelloMine3DWorldRuntimeSmoke`. |
| V3 | Verify | Screenshot the sandbox debug panel (S7.1). | `HELLOMINE3D_SHOW_DEBUG_INFO` enables the panel before the first frame; `run_render_capture.ps1 -ShowDebugInfo` sets it. Truthy/falsey parsing and environment wiring pass four headless assertions. | The 2026-08-09 capture attempt confirmed the switch was set but exposed only GDI Generic OpenGL 1.1 instead of the required 3.3, so a hardware-backed screenshot with live chunk/mesh/actor counters is still required. |
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
| E1 | Verify | Bring Ogre into the build (minimum subset: `ogre3d`, `ogre3d_glsupport`, `ogre3d_gl3plus`, FreeImage chain, dedicated Ogre FreeType, zlib, zzip and OIS). The solution is unified on x64, C++17, static CRT and MBCS; E5 has since made this the sole client graph. | Premake generation, Debug/Release full builds and all five test binaries pass (`checks=183 failures=0`). This session creates only GDI Generic OpenGL 1.1, so the hardware screenshot and comparable performance capture remain pending. |
| E2 | Verify | The Ogre shell owns `Root`/`RenderWindow`/`SceneManager`/`Camera`/`FrameListener`, resource config, OIS input and the built-in skybox. It was introduced as a parallel bootstrap and became `HelloMine3D.exe` in E5. | Debug/Release full builds and all five tests pass (`checks=183 failures=0`). Validation-only startup registers GL3Plus, two resource locations and OIS. Hardware-backed skybox and input checks remain pending. |
| E3 | Verify | `ChunkSectionRenderable` derives from Ogre's `SimpleRenderable` (`MovableObject` + `Renderable`), owns interleaved position/atlas-UV/repeat-UV/combined-light vertex and 32-bit index buffers, uses a local section AABB, and is attached to one scene node per section. `ChunkMeshBuilder` feeds the terrain material; `DefaultPack.png` is loaded through the Ogre resource group with `filtering none`. | Debug/Release generate a fixed-seed real world and validate every CPU mesh stream before any GPU call. Full builds and all five tests pass (`checks=192 failures=0`). The 2026-08-12 hardware terrain capture and performance baseline pass; a focused migration-closure review remains. |
| E4 | Verify | Water and flora use dedicated animated Ogre vertex programs and separate render queues. The Ogre UI platform layer feeds OIS input into ImGui, draws a persistent crosshair/hotbar, submits debug panels after the scene render queues, and preserves the F1 startup/toggle behavior. Screenshot capture uses `RenderWindow::writeContentsToFile`, and performance sampling runs from the frame listener. | At seed `20260809` and position `264 96 8`, Debug/Release validate 19 solid, 3 water and 13 flora sections, a valid 5-slot HUD, both enabled/disabled debug startup values, and three scheduled capture targets. Hardware material/UI appearance and actual Ogre PNG/CSV output remain pending. |
| E5 | Verify | `HelloMine3D.exe` is the sole Ogre client. SFML, glad, the old renderer/shader/texture/GL/input shells and coexistence switch are removed. `SandboxRuntime` is platform-independent; versioned CPU mesh snapshots keep Ogre uploads safe from loader-thread edits; selection feedback uses `OgreBlockOutline`. Scripts, README and validation commands use the single entry point. | Premake generation, Debug/Release full builds and all five tests pass; world runtime reports `checks=183 failures=0`, and validation-only startup passes. The new hardware performance baseline remains pending because this session only exposes GDI Generic OpenGL 1.1. |

E0 is worth doing regardless of whether the migration proceeds — it removes a
real coupling problem and simplifies headless testing. Everything from E1 on is
reversible via the coexistence switch until E5.

## Milestone P: First Playable Foundation

Goal: close the three "partly met" criteria in the sandbox foundation target.
Entity rendering is the biggest single gap in the project right now.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| P1 | Verify | Render actors. | `ActorSnapshot` exposes immutable id/type/transform/bounds data. `OgreActorRenderer` mirrors live snapshots into green mob and amber item cube nodes, updates transforms every frame, and removes visuals when an actor dies or is picked up. `run_render_capture.ps1 -SpawnValidationActors` creates a deterministic mob/item pair in front of the player. | Five `P1/*` lifecycle assertions pass in Debug and Release (`checks=183 failures=0`); validation-only startup reports a valid two-actor snapshot (`mob=1`, `item=1`). The current GDI Generic OpenGL 1.1 session cannot produce the final GL3+ render capture. |
| P2 | Done | Persist entities. | World metadata version 2 stores live `ActorSaveState` records. Virtual subtype state preserves mob health/wander/drop configuration and item material/amount/pickup delay; transforms and velocity are shared. Restore rejects invalid/duplicate ids and advances the allocator. Version 1 saves load with no actors and upgrade on the next save. | Eight `P2/*` assertions save a mob and item, relaunch, compare complete subtype state, prove the next actor id does not collide, and read/upgrade a version 1 fixture (`checks=183 failures=0` in Debug and Release). |
| P3 | Verify | Add selected-block outline and pick feedback. | `BlockSelectionSystem` supplies one hit result to break/place/use interaction, the debug panel and `OgreBlockOutline`. Water is skipped and placement uses the adjacent voxel. | Five `P3/selection-*` assertions pass in Debug and Release (`checks=183 failures=0`). The 2026-08-09 capture attempt exposed only GDI Generic OpenGL 1.1, so the line outline still needs one hardware-backed screenshot. |
| P4 | Verify | Add ore textures. | Dedicated 16x16 coal and iron textures occupy atlas slots `(13,0)` and `(14,0)`; their block definitions no longer reuse stone. | Four `P4/ore-texture-*` and atlas assertions pass in Debug and Release (`checks=183 failures=0`). The assets and packed atlas were inspected directly, but the same GDI Generic OpenGL 1.1 limitation means visual distinction still needs one hardware-backed render capture. |
| P5 | Done | Implement `use` interactions and `BlockUseEvent`. | Right-click queues `use` for the selected block before preserving the existing adjacent placement action. `PlayerDigEvent` routes the action through `BlockInteractionSystem`, which rejects air/water and publishes the target position and block id. | Four `P5/*` assertions cover the interaction seam, exactly one event, target payload and air no-op. Debug/Release full builds, all five tests and validation-only startup pass (`checks=183 failures=0`). |

## Milestone A: Asset And Data Reliability

Goal: asset mistakes should fail early with clear messages.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| A1 | Done | Add `scripts/check_assets.sh`. | The Bash script derives block definitions from `BlockDatabase.cpp`, shader sources from the Ogre program script, textures from the material script, and also verifies the bundled font, resource locations and checked-in runtime templates. `--root` supports isolated fixtures without touching live assets. | The repository passes 41 checks. An isolated copy with one referenced terrain shader omitted reports its exact path and returns 1. |
| A2 | Done | Strengthen `.block` parsing diagnostics. | `BlockData` now requires every field, rejects missing values, unknown/duplicate keys, non-integers, invalid booleans/enums, block ids outside the registry, light outside `0-15` and atlas coordinates outside the 16x16 sheet. `BlockDatabase` additionally checks declared ids against registration and rejects ids reused by another source file. | Five `A2/*` fixtures cover a missing key, invalid enum, out-of-range atlas coordinate, invalid light and duplicate id; every diagnostic contains the full file path and exact key. Debug/Release full builds and all five tests pass (`checks=200 failures=0`). |
| A3 | Done | Decide config and runtime-state ownership (old M0.3/T2.5). | `bin/config.txt` is an ignored per-user file regenerated from `Config` defaults and read by the Ogre client. `Mine.cfg` and `MineResources.cfg` are the only tracked `bin/` templates; logs, executables, ImGui state, saves, captures and legacy `info.txt` stay ignored. | Three `A3/*` assertions delete an isolated config, verify the documented `8 / windowed / 1280x720 / 90 / random seed` defaults, then load customised values (`checks=183 failures=0` in Debug and Release). |
| A4 | Done | Move world seed into the config file. | `Config::worldSeed` is optional; generated configs write `seed random`, while an integer seeds new worlds. Existing saves remain authoritative and `HELLOMINE3D_SEED` stays as the automation override. | Three `A4/*` assertions load seed `20260811`, create two fresh worlds with no seed environment variable, and compare 2,299 terrain samples with zero mismatches (`checks=183 failures=0` in Debug and Release). |

## Milestone B: Build And Platform Reliability

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| B1 | Done | Retire the stale nested CMake cache risk. | E5 removed the SFML external project; all active dependencies are generated directly by Premake. | `vs2022.bat` produces no `External/sfml` target or CMake cache. |
| B2 | Done | Document and script build verification (old M0.4). | `verify_build.ps1` uses bundled Premake, locates VS2022 MSBuild, builds Debug/Release x64 and runs all five tests after each build. `verify_build.sh` applies the same contract through the existing Premake/Make path on Linux and macOS. README lists one command and its host prerequisites per platform. | The Windows wrapper regenerates projects, completes both full builds and ten test executions with `[BUILD_VERIFY] status=PASS`. This Windows-only session has no Bash runtime, so the Unix wrapper was reviewed but not executed here; native macOS Xcode verification remains B3. |
| B3 | Todo | Verify the macOS path. | Every milestone note so far records Windows verification only. The Xcode generator and Ogre GL3Plus/OIS client are unproven on macOS. | `sh xcode.sh` then `xcodebuild` builds and the client launches. |
| B5 | Done | Fix the render capture script race. | Runtime polling validates the PNG signature, all chunk boundaries and terminal `IEND` before accepting a file, so a non-empty partial write cannot make the script stop the client. Relative output/save paths are normalized against the repository root. The native process handle is primed immediately and an exited process is fully joined before its exit code is read. A clean exit still receives a two-second visibility grace period; non-zero exits and incomplete files fail. | The 2026-08-12 hardware runs exposed a truncated 40 KB PNG and PowerShell 5 losing `Handle/ExitCode` for a fast process. `-ValidateCapturePolling` rejects the incomplete fixture and passes ten partial-to-complete writer races. GTX 1050 Ti / OpenGL 4.6 then completes ten consecutive captures, each independently decoded by ffmpeg (`46,343-49,604` bytes; `runs=10 status=PASS`). |
| B4 | Done | Remove dead build artifacts. | Removed ignored outputs for the deleted `HelloMine3DFoundationRuntimeSmoke` target and the pre-E5 standalone `HelloMine3DOgreBootstrap` target. Both verification wrappers now reject any `HelloMine3D*` executable outside the current client-plus-five-tests allowlist. | After Debug/Release regeneration and builds, the Windows wrapper reports `executable inventory valid`; `bin/` contains exactly `HelloMine3D.exe` and the five current test executables. |

## Milestone M: Mesh Pipeline And Performance

Goal: make section mesh rebuilds measurable and then faster. The performance
baseline tool already exists, so every item here has a before/after number.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| M1 | Done | Add mesh build metrics. | Both background streaming and synchronous edit rebuilds measure accepted per-section build time. Debug stats accumulate total/latest/max milliseconds plus solid/water/flora face and vertex counts; the Ogre debug panel, frame CSV and summary expose them, including average build time. | Five `M1/*` assertions verify rebuild, timing and geometry counters in Debug and Release (`checks=183 failures=0`); validation-only startup and performance-script syntax pass. A fresh hardware performance capture can consume the new columns without changing the script. |
| M2 | Done | Convert dirty sections into a bounded queue. | Dirty section coordinates enter a deduplicated FIFO rather than an unordered pointer batch. `World::updateChunks()` resolves each coordinate safely and processes at most two entries per frame, so unloaded chunks leave no dangling pointers and edit bursts spread their mesh work across frames. | Five `M2/*` assertions queue five independent sections, preserve FIFO order, cap the first frame at two rebuilds, drain as `2 + 2 + 1`, and prove duplicate edits/empty updates add no work (`checks=183 failures=0` in Debug and Release). |
| M3 | Done | Add an 18x18x18 halo cache for mesh building. | `SectionMeshInput` snapshots the neighbourhood under the world lock; `ChunkMeshBuilder` then builds from the snapshot with no world access. Stale results are rejected by a per-section block revision. Also fixed a block pointer desync that rendered underground blocks at the surface, throttled the unload scan to camera chunk changes, and removed the dead `makeMesh`/`makeMeshes` chain. | `HelloMine3DWorldRuntimeSmoke` `M3/*` (93 checks total); `update_p95_ms` 0.004 ms in Release; 2013 mesh rebuilds vs 1598 in a same-conditions A/B. |
| M4 | Verify | Implement opaque cube greedy meshing. | Six directional masks merge only matching opaque cube blocks in the terrain pass. Merged quads carry a separate repeat-UV stream so each atlas tile retains block-scale sampling; transparent cubes, water and crossed flora keep their original topology. | Six `M4/*` assertions reduce an `8x1x8` stone slab from 160 naive faces to 6, preserve a stone/dirt boundary at 10 faces, and keep water/flora separate. Debug/Release full builds and all tests pass (`checks=183 failures=0`); validation-only terrain falls from 20,796 to 8,396 vertices while water/flora counts remain unchanged. A hardware GL3+ capture is still required to confirm identical atlas appearance. |
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
| C2 | Todo | Add one metadata-backed block behaviour. | Same block id renders or behaves differently by metadata. |
| C3 | Todo | Resource-drive block shapes. | Non-cube blocks are no longer hard-coded in the mesh builder. |
| C4 | Todo | Add a cave generation pass. | Caves are deterministic and do not break surface generation. |
| C5 | Todo | Support cross-chunk structures. | Structures near chunk edges neither disappear nor duplicate. |
| C6 | Todo | Add mob chase AI and damage invulnerability frames. | A mob reacts to the player and cannot be damaged twice in one frame. |

## Not Recommended Yet

| Direction | Reason |
| --------- | ------ |
| Async chunk IO | Synchronous save/load is correct now. Add a command queue only once mesh performance work exposes a real IO stall. |
| Multiplayer | Changes world authority, event sync, entity sync, save format and input model. |
| Lua / UGC scripting | The C++ event bus is stable but the extension points are not exercised yet. |
| Full resource packs and hot reload | Do manifest and validation first (A1, A2). |
| D3D / Vulkan backend | Ogre GL3Plus covers the Windows/macOS target (see Milestone E). |
| Large-scale MiniGame code migration | Use it as an architectural reference, not a source. Its engine is a renamed `MINIW` fork whose API has diverged; code cannot be shared. |

## Recommended Order

1. **V1-V5** — retire the last validation gaps while the context is fresh.
2. **E0** — decouple the data layer from SFML and OpenGL. Independently
   valuable and a prerequisite for everything in Milestone E.
3. **P3, P4** — block outline and ore textures. Neither touches the render
   pipeline, so they survive the migration.
4. **E1-E5** — the Ogre migration itself.
5. **P1, P2** — entity rendering and persistence. Deliberately deferred until
   after E3: doing it on the current renderer means redoing it, whereas on Ogre
   it can use `Entity`/`SceneNode` directly.
6. **M1-M3** — mesh metrics first, then bounded queue and halo cache. Metrics
   must land before any optimization, otherwise nothing is measurable.
7. **L1-L4** — lighting.
8. **M4-M5** — greedy meshing last, because face merging has to include the
   light value in its merge criterion. Doing it before L means rewriting it.
9. **A1-A4, B1-B4** — asset and build reliability.
10. **C1-C6** — continue from the completed P5 `use` seam into content and behaviour expansion.

Two ordering constraints worth remembering, both of which cost a rewrite if
violated:

- **Migration before lighting** — both rewrite the mesh and shader layer.
- **Lighting before greedy meshing** — merged faces must share a light value.

## Validation Matrix

Every completed task should list which validations were run.

| Validation | Command | Required for |
| ---------- | ------- | ------------ |
| Windows project generation | `vs2022.bat` | Build system or file layout changes |
| Windows debug build | `MSBuild build\HelloMine3D.sln /p:Configuration=Debug /p:Platform=x64` | All code changes |
| Windows release build | same with `/p:Configuration=Release` | Milestone completion |
| macOS Xcode project | `sh xcode.sh`, then `xcodebuild` | Build system changes, once B3 lands |
| Focused headless tests | the four `bin\HelloMine3D*Tests.exe` / `*Smoke.exe` binaries | All code changes |
| World runtime smoke | `bin\HelloMine3DWorldRuntimeSmoke.exe` | Chunk, storage, interaction, event, actor or terrain changes |
| Render smoke | `tools\run_render_capture.ps1` | Renderer, shader, texture, mesh upload or frame sequencing changes |
| Performance baseline | `tools\run_perf_baseline.ps1` | Chunk loading, mesh building, update flow or render submission changes |
| Asset check | `sh scripts/check_assets.sh` | Asset and data changes |
| Interactive run | `bin\HelloMine3D.exe` | Input, camera or player-facing UI changes |

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
- [ ] Interactive run

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

Next recommended task:
-
```
