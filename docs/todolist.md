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

Last refreshed 2026-08-09. Update this when a milestone closes.

| Area | Current state | Verification |
| ---- | ------------- | ------------ |
| Build system | Premake is the sole project generator. `vs2022.bat` generates one `HelloMine3D` client, five test targets, and the Ogre/OIS static-library graph for VS2022 x64. | `vs2022.bat`, `MSBuild build\HelloMine3D.sln /p:Configuration=Debug /p:Platform=x64` |
| Dependencies | Ogre 1.10 GL3Plus is the sole client renderer, with OIS, ImGui and the minimum image/font/archive dependency closure built from vendored source. No checked-in binaries. | Debug and Release full-solution builds |
| Focused tests | 4 headless test targets, all passing. | `bin\HelloMine3DCoordinateTests.exe`, `bin\HelloMine3DMeshDirtyTests.exe`, `bin\HelloMine3DSaveLoadSmoke.exe`, `bin\HelloMine3DEntityLifecycleSmoke.exe` |
| Runtime validation | `HelloMine3DWorldRuntimeSmoke` drives the real world/actor stack through 127 assertions, including versioned renderer mesh snapshots and stale-upload rejection. | `bin\HelloMine3DWorldRuntimeSmoke.exe` -> `checks=127 failures=0` |
| Render smoke | Ogre terrain, water, flora, outline and HUD are implemented and validation-only startup passes. The current automation session exposes only GDI Generic OpenGL 1.1, so a hardware GL3+ capture remains pending. | `docs/render-regression-smoke.md` |
| Performance baseline | Ogre frame/tick/counter collection is wired and the script targets the sole client. A comparable hardware GL3+ baseline remains pending; the archived 2026-08-07 figures predate the renderer migration. | `docs/performance-baseline.md` |
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
| E1 | Verify | Bring Ogre into the build (minimum subset: `ogre3d`, `ogre3d_glsupport`, `ogre3d_gl3plus`, FreeImage chain, dedicated Ogre FreeType, zlib, zzip and OIS). The solution is unified on x64, C++17, static CRT and MBCS; E5 has since made this the sole client graph. | Premake generation, Debug/Release full builds and all five test binaries pass (`checks=127 failures=0`). This session creates only GDI Generic OpenGL 1.1, so the hardware screenshot and comparable performance capture remain pending. |
| E2 | Verify | The Ogre shell owns `Root`/`RenderWindow`/`SceneManager`/`Camera`/`FrameListener`, resource config, OIS input and the built-in skybox. It was introduced as a parallel bootstrap and became `HelloMine3D.exe` in E5. | Debug/Release full builds and all five tests pass (`checks=127 failures=0`). Validation-only startup registers GL3Plus, two resource locations and OIS. Hardware-backed skybox and input checks remain pending. |
| E3 | Verify | `ChunkSectionRenderable` derives from Ogre's `SimpleRenderable` (`MovableObject` + `Renderable`), owns interleaved position/UV/cardinal-light vertex and 32-bit index buffers, uses a local section AABB, and is attached to one scene node per section. The unchanged `ChunkMeshBuilder` feeds the terrain material; `DefaultPack.png` is loaded through the Ogre resource group with `filtering none`. | Debug/Release generate a fixed-seed real world and validate the CPU meshes before any GPU call. Full builds and all five tests pass (`checks=127 failures=0`). Hardware terrain capture and the comparable performance baseline remain pending. |
| E4 | Verify | Water and flora use dedicated animated Ogre vertex programs and separate render queues. The Ogre UI platform layer feeds OIS input into ImGui, draws a persistent crosshair/hotbar, submits debug panels after the scene render queues, and preserves the F1 startup/toggle behavior. Screenshot capture uses `RenderWindow::writeContentsToFile`, and performance sampling runs from the frame listener. | At seed `20260809` and position `264 96 8`, Debug/Release validate 19 solid, 3 water and 13 flora sections, a valid 5-slot HUD, both enabled/disabled debug startup values, and three scheduled capture targets. Hardware material/UI appearance and actual Ogre PNG/CSV output remain pending. |
| E5 | Verify | `HelloMine3D.exe` is the sole Ogre client. SFML, glad, the old renderer/shader/texture/GL/input shells and coexistence switch are removed. `SandboxRuntime` is platform-independent; versioned CPU mesh snapshots keep Ogre uploads safe from loader-thread edits; selection feedback uses `OgreBlockOutline`. Scripts, README and validation commands use the single entry point. | Premake generation, Debug/Release full builds and all five tests pass; world runtime reports `checks=127 failures=0`, and validation-only startup passes. The new hardware performance baseline remains pending because this session only exposes GDI Generic OpenGL 1.1. |

E0 is worth doing regardless of whether the migration proceeds — it removes a
real coupling problem and simplifies headless testing. Everything from E1 on is
reversible via the coexistence switch until E5.

## Milestone P: First Playable Foundation

Goal: close the three "partly met" criteria in the sandbox foundation target.
Entity rendering is the biggest single gap in the project right now.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| P1 | Todo | Render actors. | Ogre currently uploads chunk terrain only. Mobs and item entities tick correctly but are invisible, so S5.5/S5.6 cannot be confirmed visually. Start with a simple cube/billboard movable object. | Render capture shows a spawned mob and a dropped item. |
| P2 | Todo | Persist entities. | `ActorSaveState` exists but `WorldSaveData` has no entity list, so mobs and drops vanish on relaunch. | Spawn a mob, relaunch, mob is still there. |
| P3 | Verify | Add selected-block outline and pick feedback. | `BlockSelectionSystem` supplies one hit result to break/place interaction, the debug panel and `OgreBlockOutline`. Water is skipped and placement uses the adjacent voxel. | Five `P3/selection-*` assertions pass in Debug and Release (`checks=127 failures=0`). The 2026-08-09 capture attempt exposed only GDI Generic OpenGL 1.1, so the line outline still needs one hardware-backed screenshot. |
| P4 | Verify | Add ore textures. | Dedicated 16x16 coal and iron textures occupy atlas slots `(13,0)` and `(14,0)`; their block definitions no longer reuse stone. | Four `P4/ore-texture-*` and atlas assertions pass in Debug and Release (`checks=127 failures=0`). The assets and packed atlas were inspected directly, but the same GDI Generic OpenGL 1.1 limitation means visual distinction still needs one hardware-backed render capture. |
| P5 | Todo | Implement `use` interactions and `BlockUseEvent`. | The event type and the interaction seam already exist; only the behaviour is missing. Criterion 3 of the foundation target depends on it. | Right-click on a target block publishes `BlockUseEvent`. |

## Milestone A: Asset And Data Reliability

Goal: asset mistakes should fail early with clear messages.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| A1 | Todo | Add `scripts/check_assets.sh`. | Check block files, shader files, textures, fonts and config templates. | Script returns non-zero on a missing referenced file. |
| A2 | Todo | Strengthen `.block` parsing diagnostics. | `BlockData.cpp` only throws when the file cannot be opened. Report filename, missing key, bad enum, invalid atlas coordinate and duplicate id. | A deliberately broken block file names the exact file and key. |
| A3 | Todo | Decide config and runtime-state ownership (old M0.3/T2.5). | `.gitignore` currently ignores all of `bin/`, so `config.txt` and `info.txt` are effectively generated. `loadConfig()` already regenerates a default config; document that and drop the ambiguity. | Deleting `bin/config.txt` regenerates a documented default. |
| A4 | Todo | Move world seed into the config file. | Seed is only reachable through `HELLOMINE3D_SEED`. `Config` has window, FOV and render distance but no seed. | Setting a seed in `bin/config.txt` reproduces a world. |

## Milestone B: Build And Platform Reliability

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| B1 | Done | Retire the stale nested CMake cache risk. | E5 removed the SFML external project; all active dependencies are generated directly by Premake. | `vs2022.bat` produces no `External/sfml` target or CMake cache. |
| B2 | Todo | Document and script build verification (old M0.4). | One documented command per platform, matching what CI would run. | A fresh checkout builds by following the README alone. |
| B3 | Todo | Verify the macOS path. | Every milestone note so far records Windows verification only. The Xcode generator and Ogre GL3Plus/OIS client are unproven on macOS. | `sh xcode.sh` then `xcodebuild` builds and the client launches. |
| B5 | Todo | Fix the render capture script race. | The process writes both PNGs and exits cleanly, but the script sometimes reports `Process exited before runtime captures completed` and returns non-zero. More likely now that shutdown is faster. See `docs/chunk-streaming-regression.md` R5. | Ten consecutive capture runs all report `status=PASS`. |
| B4 | Todo | Remove dead build artifacts. | `bin/HelloMine3DFoundationRuntimeSmoke.exe` has no source file and no premake target; it is a leftover from a deleted target and is confusing next to the live test binaries. | `bin/` only contains targets that premake still generates. |

## Milestone M: Mesh Pipeline And Performance

Goal: make section mesh rebuilds measurable and then faster. The performance
baseline tool already exists, so every item here has a before/after number.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| M1 | Todo | Add mesh build metrics. | Track per-section rebuild ms, solid/water/flora face counts and vertex counts. Today only a cumulative rebuild count exists. | Baseline summary gains face/vertex/ms columns. |
| M2 | Todo | Convert dirty sections into a bounded queue. | `World::updateChunks()` rebuilds every queued section in one frame. | Editing many blocks does not spike a single frame. |
| M3 | Done | Add an 18x18x18 halo cache for mesh building. | `SectionMeshInput` snapshots the neighbourhood under the world lock; `ChunkMeshBuilder` then builds from the snapshot with no world access. Stale results are rejected by a per-section block revision. Also fixed a block pointer desync that rendered underground blocks at the surface, throttled the unload scan to camera chunk changes, and removed the dead `makeMesh`/`makeMeshes` chain. | `HelloMine3DWorldRuntimeSmoke` `M3/*` (93 checks total); `update_p95_ms` 0.004 ms in Release; 2013 mesh rebuilds vs 1598 in a same-conditions A/B. |
| M4 | Todo | Implement opaque cube greedy meshing. | Merge only same-material, same-pass faces. Keep flora and water separate. | Face count drops on flat terrain, render capture unchanged. |
| M5 | Done | Investigate unmeshed sections. | Diagnosed as a six-part regression from `7a229d8`, not a budget choice. Fixed 2026-08-07; Release now meshes and uploads all 2013 sections inside the run. | `docs/chunk-streaming-regression.md`; baselines `perf_baseline_20260807204353319-60596` (Debug) and `perf_baseline_20260807204435048-12972` (Release). |
| M6 | Todo | Skip fully enclosed sections. | An underground section surrounded by solid blocks still gets a full mesh build that emits nothing. A non-empty/solid count skips it. See `docs/minigame-reference.md` item 7. | Mesh rebuild count drops with no visual change. |
| M7 | Todo | Prioritise mesh builds by view frustum. | Only worth doing once view distance grows past 8 (~7600 sections). Must sort, not filter, and must read a frustum snapshot published by the main thread rather than a live `Camera&`. See `docs/chunk-streaming-regression.md` R2. | Terrain in front of the player becomes visible first at view distance 16. |

## Milestone L: Light And Visual Feedback

Goal: visual quality, after the mesh pipeline is stable. Nothing here exists
yet; there is no light data in the codebase at all.

| ID | Status | Task | Validation |
| -- | ------ | ---- | ---------- |
| L1 | Todo | Add sunlight storage. | Surface and caves have visibly different brightness. |
| L2 | Todo | Add block light storage. | An emissive block lights nearby mesh vertices. |
| L3 | Todo | Add local relight after edits. | Placing an opaque block changes nearby lighting without a full rebuild. |
| L4 | Todo | Tighten transparent block rules. | Water, glass, leaves and flora render without missing faces or wrong draw order. |

## Milestone C: Content And Behaviour Expansion

| ID | Status | Task | Validation |
| -- | ------ | ---- | ---------- |
| C1 | Todo | Introduce lightweight `BlockBehavior`. | A special block can be added without editing unrelated switch chains. |
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
10. **P5, C1-C6** — `use` interactions, then content and behaviour expansion.

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
| Asset check | `sh scripts/check_assets.sh` | Asset and data changes, once A1 lands |
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
