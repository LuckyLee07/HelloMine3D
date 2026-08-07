# HelloMine3D TODO List

This document is the single execution checklist for the project. It answers
"what is done, what is next, and how is it verified".

| Document | Answers |
| -------- | ------- |
| `docs/todolist.md` (this file) | What is done, what is next, and how each item is validated. |
| `docs/sandbox-foundation-todolist.md` | Detailed record of the S0-S7 sandbox foundation milestones. |
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

Last refreshed 2026-08-07. Update this when a milestone closes.

| Area | Current state | Verification |
| ---- | ------------- | ------------ |
| Build system | Premake is the sole project generator. `vs2022.bat` then `build/HelloMine3D.sln` builds all six targets clean on VS2022 x64 Debug. | `vs2022.bat`, `MSBuild build\HelloMine3D.sln /p:Configuration=Debug /p:Platform=x64` |
| Dependencies | SFML 3 and FreeType build from vendored source into `build/External/sfml/install`. No checked-in binaries. | Full solution build from a wiped `build/External/sfml` |
| Focused tests | 4 headless test targets, all passing. | `bin\HelloMine3DCoordinateTests.exe`, `bin\HelloMine3DMeshDirtyTests.exe`, `bin\HelloMine3DSaveLoadSmoke.exe`, `bin\HelloMine3DEntityLifecycleSmoke.exe` |
| Runtime validation | `HelloMine3DWorldRuntimeSmoke` drives the real world/actor stack through 89 assertions covering S0-S6. | `bin\HelloMine3DWorldRuntimeSmoke.exe` -> `checks=89 failures=0` |
| Render smoke | Terrain, textures, water and flora render correctly. | `bin/render_capture_20260807190230074-46036`, status PASS |
| Performance baseline | 591 frames, `frame_p95_ms=16.430`, `avg_fps=67.152`, 289 loaded chunks, 449 mesh rebuilds. | `bin/perf_baseline_20260807190255313-41064` |
| Sandbox foundation | 41 of 44 S-milestones Done, 3 in Verify. | `docs/sandbox-foundation-todolist.md` |

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
| V1 | Todo | Assert the 20Hz simulation tick rate (S1.5). | Record a tick counter in `RuntimePerformanceCapture` alongside the frame counters, then check ticks/second in the summary. | Performance baseline summary reports roughly 20 ticks per second over a 10s run. |
| V2 | Todo | Validate `PlayerController` input (S5.3). | Needs one interactive run, or an input-injection mode that feeds synthetic key state without warping the mouse. | Movement, jump, fly toggle and hotbar selection all respond. |
| V3 | Todo | Screenshot the sandbox debug panel (S7.1). | Add an env override that starts the client with `show_debug_info` enabled so the capture script can photograph the panel. | Capture shows live chunk/mesh/actor counters. |
| V4 | Todo | Assert height map correctness after edits (old T0.7). | Break the highest opaque block and place above the top; compare `Chunk::getHeightAt()` against a brute-force scan. | Added to `HelloMine3DWorldRuntimeSmoke`. |
| V5 | Todo | Add a thread-stress check for the background loader (S0.3). | MSVC has no ThreadSanitizer. A long-running loader stress with an assertion-heavy debug build is the realistic option. | Loader survives sustained load-center churn without corruption. |

## Milestone P: First Playable Foundation

Goal: close the three "partly met" criteria in the sandbox foundation target.
Entity rendering is the biggest single gap in the project right now.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| P1 | Todo | Render actors. | `RenderMaster` only exposes `drawChunk()` and `drawSky()`. Mobs and item entities tick correctly but are invisible, so S5.5/S5.6 cannot be confirmed visually. Start with a simple cube/billboard pass. | Render capture shows a spawned mob and a dropped item. |
| P2 | Todo | Persist entities. | `ActorSaveState` exists but `WorldSaveData` has no entity list, so mobs and drops vanish on relaunch. | Spawn a mob, relaunch, mob is still there. |
| P3 | Todo | Add selected-block outline and pick feedback. | Reuse the existing `Ray` cast from the dig path. | Player can see which block will be edited. |
| P4 | Todo | Add ore textures. | `CoalOre` and `IronOre` currently reuse stone atlas coordinates, so ore is invisible in game. | Ore is visually distinct in a render capture. |
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
| B1 | Todo | Guard against a stale SFML CMake cache. | `build/External/sfml/CMakeCache.txt` stores absolute paths, so moving or renaming the repository breaks every later build with a confusing CMake error. Detect the mismatch in the build scripts and wipe the tree automatically. | Rename the repo directory, build, and the build still succeeds. |
| B2 | Todo | Document and script build verification (old M0.4). | One documented command per platform, matching what CI would run. | A fresh checkout builds by following the README alone. |
| B3 | Todo | Verify the macOS path. | Every milestone note so far records Windows verification only. The Xcode generator and the SFML 3 inline-constexpr patch are unproven on macOS. | `sh xcode.sh` then `xcodebuild` builds and the client launches. |
| B4 | Todo | Remove dead build artifacts. | `bin/HelloMine3DFoundationRuntimeSmoke.exe` has no source file and no premake target; it is a leftover from a deleted target and is confusing next to the live test binaries. | `bin/` only contains targets that premake still generates. |

## Milestone M: Mesh Pipeline And Performance

Goal: make section mesh rebuilds measurable and then faster. The performance
baseline tool already exists, so every item here has a before/after number.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| M1 | Todo | Add mesh build metrics. | Track per-section rebuild ms, solid/water/flora face counts and vertex counts. Today only a cumulative rebuild count exists. | Baseline summary gains face/vertex/ms columns. |
| M2 | Todo | Convert dirty sections into a bounded queue. | `World::updateChunks()` rebuilds every queued section in one frame. | Editing many blocks does not spike a single frame. |
| M3 | Todo | Add an 18x18x18 halo cache for mesh building. | Cache neighbour blocks around a section before emitting faces. | Fewer cross-section lookups, identical visual output. |
| M4 | Todo | Implement opaque cube greedy meshing. | Merge only same-material, same-pass faces. Keep flora and water separate. | Face count drops on flat terrain, render capture unchanged. |
| M5 | Todo | Investigate unmeshed sections. | The baseline shows 1564 of 2013 sections still mesh-dirty after a 13s run. Confirm this is the intended loader budget and not starvation. | Documented explanation or a fix, plus a baseline comparison. |

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
| D3D / Vulkan backend | SFML + OpenGL covers the Windows/macOS target. |
| Large-scale MiniGame code migration | Use it as an architectural reference, not a source. |

## Recommended Order

1. **V1-V5** — retire the last validation gaps while the context is fresh.
2. **P1-P5** — entity rendering, entity persistence, block outline, ore
   textures and `use` interactions. This is what turns the foundation into
   something playable, and it makes S5.5/S5.6 visually confirmable.
3. **A1-A4, B1-B4** — asset and build reliability, before the codebase grows.
4. **M1-M5** — mesh metrics first, then the optimizations, each with a
   baseline comparison.
5. **L1-L4** — lighting, after the vertex format work in M is settled.
6. **C1-C6** — content and behaviour expansion.

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
