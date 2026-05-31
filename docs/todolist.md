# HelloMine3D TODO List

This document is the execution checklist for upcoming iterations. It is intentionally more
operational than `docs/iteration-plan.md`: each item should have a clear validation path and a
baseline that can be compared after implementation.

Status legend:

| Status | Meaning |
| ------ | ------- |
| Todo | Not started. |
| Doing | In progress on the current branch. |
| Verify | Code is done, validation is still pending. |
| Done | Built, verified, and documented. |
| Blocked | Needs a design decision, dependency, or missing tool. |

## Current Baseline

Record this before a new iteration and update it when a milestone is completed.

| Area | Current state | Verification |
| ---- | ------------- | ------------ |
| Build system | Premake generates Make and Xcode projects. Target is `HelloMine3D`. | `sh scripts/build.sh`, `sh xcode.sh`, `xcodebuild -list -project build/HelloMine3D.xcodeproj` |
| Runtime binary | Debug executable outputs to `bin/HelloMine3D`. | `ls -lh bin/HelloMine3D` |
| Project naming | Source and docs use `HelloMine3D`; old generated artifacts may still exist under `bin/` and `build/`. | `rg 'Mine''Craft3D|MINE''CRAFT3D'` |
| Assets | Runtime assets are under `media/`; config/state are under `bin/`. | Manual launch and future `scripts/check_assets.sh` |
| World update | `World::updateChunk()` exists, but `Chunk::setBlock()` does not reliably trigger mesh dirty updates yet. | Manual block break/place test after T0 tasks |
| Section state | `ChunkSection::Layer` count exists but needs correctness work. | Unit-like runtime assertions or focused test harness after T0.1 |

## Milestone M0: Project Hygiene

Goal: keep the project easy to build, inspect, and compare before deeper gameplay changes.

| ID | Status | Task | Why | Validation |
| -- | ------ | ---- | --- | ---------- |
| M0.1 | Done | Rename generated target, scripts, docs, and window title to `HelloMine3D`. | Avoid mixed project identity during builds and debugging. | `rg 'Mine''Craft3D|MINE''CRAFT3D'` should only find old generated artifacts if scanning all files. |
| M0.2 | Todo | Add `.gitignore` for generated `bin/`, `build/`, `.DS_Store`, and debug artifacts. | Keep source diffs readable and avoid committing machine state. | `git status --short` shows only intentional source/docs changes after a build. |
| M0.3 | Todo | Decide whether `bin/config.txt` and `bin/info.txt` are source-controlled templates or generated runtime files. | Current `bin/` mixes executable output, config, and UI state. | Document the decision in README; clean `git status` after launch. |
| M0.4 | Todo | Add a short build verification script or README section for Make and Xcode. | Every iteration should have the same validation commands. | Fresh checkout can run the documented commands without guessing. |

## Milestone M1: Reliable Block Editing

Goal: breaking and placing blocks should update only the affected section meshes and their visible
neighbors.

This is the highest priority milestone. It unlocks later work on metadata, block behavior, light,
save/load, and performance.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| T0.1 | Todo | Fix `ChunkSection::Layer` solid counts. | Compare old block opacity and new block opacity in `setBlock()`. Increment only when air/non-opaque becomes opaque; decrement only when opaque becomes non-opaque. | Add focused assertions or a small test path for all-air, all-solid, and mixed layers. Confirm `isAllSolid()` is correct. |
| T0.2 | Todo | Add explicit section dirty flags. | Add at least `blocksDirty` and `meshDirty` or equivalent. `setBlock()` should mark block and mesh state dirty. | Debug output shows dirty section count after one block edit. |
| T0.3 | Todo | Re-enable block edit mesh update path. | `Chunk::setBlock()` should notify `World::updateChunk()` after successful mutation when loaded. Avoid notifying during initial terrain generation unless explicitly intended. | Break/place a block and confirm the visible mesh refreshes without pressing reload keys. |
| T0.4 | Todo | Mark boundary neighbor sections dirty. | If edited block is on x/z/y section boundary, enqueue the neighboring section because exposed faces may change. | Break/place on chunk and section boundaries; neighbor faces appear/disappear correctly. |
| T0.5 | Todo | Avoid accidental chunk creation during dirty marking. | Dirty marking should not create unloaded chunks unless generation is intentionally requested. Consider query-only APIs in `ChunkManager`. | Edit near an unloaded edge; chunk count does not spike unexpectedly. |
| T0.6 | Todo | Add debug visibility for mesh updates. | Start with logs or ImGui counters: dirty sections queued, sections rebuilt this frame, total loaded chunks. | During block editing, counters change predictably and return to zero. |
| T0.7 | Todo | Fix height map update after block changes. | When removing the highest opaque block, scan downward and store the new top. When placing above top, update directly. | Height queries remain correct after removing and placing top blocks. |

Manual validation checklist for M1:

1. Build debug: `sh scripts/build.sh`.
2. Launch: `sh scripts/run.sh`.
3. Break a visible solid block in the middle of a section.
4. Place a block in the same hole.
5. Repeat on x, y, and z section boundaries.
6. Confirm no full-world reload is needed and no obvious mesh holes remain.
7. Record dirty section count and rebuild count once debug counters exist.

## Milestone M2: Coordinate and Loading Stability

Goal: chunk addressing and loading should be predictable before infinite-world and save work.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| T1.1 | Todo | Define negative coordinate policy. | Prefer floor division and positive local block coordinates for chunk/block mapping. If not supporting negative coordinates yet, clamp deliberately and document it. | Moving across x/z zero does not produce negative local block indices or wrong chunk access. |
| T1.2 | Todo | Fix spawn chunk preloading. | Current spawn preload loops use world block coordinates where chunk coordinates are expected. Convert to chunk coordinates. | Spawn area consistently appears around the player without delayed holes. |
| T1.3 | Todo | Split chunk query and chunk creation APIs. | Add methods like `findChunk()`/`tryGetChunk()` beside `getOrCreateChunk()`. | Read-only mesh/build paths do not create chunks by accident. |
| T1.4 | Todo | Stop background loading thread from reading `Camera&` directly. | Main thread should submit target chunk position or load center; worker consumes immutable requests. | Code review shows no unsynchronized camera reads from worker threads. |
| T1.5 | Todo | Add chunk load budget and counters. | Track chunks loaded, sections meshed, and time spent per frame. | Debug UI/logs show stable per-frame budget instead of large stalls. |

## Milestone M3: Asset and Data Reliability

Goal: asset mistakes should fail early with clear messages, and block data should be ready for
behavior and metadata.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| T2.1 | Todo | Add `scripts/check_assets.sh`. | Check block files, shader files, textures, fonts, and config templates. | Script returns non-zero on missing referenced files. |
| T2.2 | Todo | Strengthen `.block` parsing diagnostics. | Include filename, missing key, bad enum/value, invalid atlas coordinate, and duplicate ID. | Introduce a temporary bad block file and confirm the error points to the exact file/key. |
| T2.3 | Todo | Define `BlockDefinition` as the static data boundary. | Keep compatibility with current `BlockDataHolder`, but make room for hardness, collision, render type, light opacity, and drops. | Existing blocks still load; new fields can be defaulted. |
| T2.4 | Todo | Add stable string IDs for blocks. | Numeric IDs are useful at runtime, but string IDs make data migration and debugging easier. | Logs/debug UI can show both numeric ID and string ID for a selected block. |
| T2.5 | Todo | Decide config ownership. | Move defaults into code or a template file; keep user state separate from generated executable output. | Deleting runtime config either regenerates a documented default or fails with an actionable message. |

MiniGame reference to use here:

| MiniGame idea | HelloMine3D version |
| ------------- | ------------------- |
| `BlockDef` data + `BlockMaterial` behavior | `BlockDefinition` + later `BlockBehavior` |
| Large CSV content tables | Small `.block` or TOML/JSON definitions first |
| `BlockMaterialMgr` lookup tables | Startup-built block registry and validation reports |

## Milestone M4: Mesh Pipeline and Performance

Goal: make section mesh rebuilds measurable and then faster.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| T3.1 | Todo | Add mesh build metrics. | Track section rebuild count, solid/water/flora face counts, vertex counts, and build milliseconds. | A debug panel or log shows before/after numbers for the same scene. |
| T3.2 | Todo | Convert dirty sections into a bounded queue. | Process a fixed number or time budget per frame. | Editing many blocks does not cause a single-frame rebuild spike. |
| T3.3 | Todo | Add 18x18x18 halo cache for mesh building. | Cache neighbor blocks around a section before emitting faces. | Mesh build performs fewer cross-world lookups; output remains visually identical. |
| T3.4 | Todo | Implement opaque cube greedy meshing. | Only merge faces with same material/render pass/light constraints. Keep flora/water separate. | Face/vertex count drops on flat terrain; visual output remains correct. |
| T3.5 | Todo | Keep render pass separation explicit. | Solid, water, flora, transparent/model blocks should not be forced into one mesh path. | Glass/leaves/water changes do not regress when solid mesh is optimized. |

Comparison metrics for M4:

| Metric | Before | After |
| ------ | ------ | ----- |
| Loaded chunks | Record from debug UI | Record from debug UI |
| Dirty sections per edit | Record once T0.6 exists | Compare after queue changes |
| Section rebuild ms | Record once T3.1 exists | Compare after halo/greedy |
| Solid face count | Record once T3.1 exists | Compare after greedy |
| Frame stutter during block spam | Manual observation | Manual observation |

## Milestone M5: Block State and Behavior

Goal: move from static block IDs toward data-driven, extensible blocks.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| T4.1 | Todo | Add `metadata` to `ChunkBlock`. | Use an explicit `std::uint8_t` field first; do not bit-pack yet. | Existing worlds still run; all old blocks have metadata 0. |
| T4.2 | Todo | Add one metadata-backed block behavior. | Good candidates: log orientation, crop growth stage, or water level. | Same block ID renders/behaves differently based on metadata. |
| T4.3 | Todo | Introduce `BlockRenderInfo`. | Separate atlas/render pass/shape from core static properties. | Mesh builder reads render info instead of scattering block-type checks. |
| T4.4 | Todo | Introduce lightweight `BlockBehavior`. | Start with callbacks for placed, broken, neighbor changed, tick. | A special block can be added without editing many unrelated switch/if chains. |
| T4.5 | Todo | Plan `BlockEntity` but do not implement broadly yet. | Containers, signs, furnaces, and machines need persistent state later. | Design note exists; no heavy container system until save/load is ready. |

MiniGame reference to use here:

| MiniGame idea | HelloMine3D version |
| ------------- | ------------------- |
| `BlockMaterial` large virtual interface | Small optional behavior interface |
| `WorldContainer` for stateful blocks | Future `BlockEntity` |
| `BlockDef.Type` creates concrete behavior | Definition chooses optional behavior factory |

## Milestone M6: Save, Seed, and World Persistence

Goal: make the world reproducible and then persistent.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| T5.1 | Todo | Add world seed to config and terrain generator. | Same seed should generate the same chunks. | Delete world data, reuse seed, compare sampled heights/blocks. |
| T5.2 | Todo | Mark dirty chunks separately from dirty sections. | Mesh dirty is render state; chunk dirty means save needed. | Editing one block marks one chunk dirty for save. |
| T5.3 | Todo | Design versioned chunk file format. | Include magic, version, chunk coords, dimensions, block IDs, metadata, and optional compression. | Loader rejects wrong magic/version with clear error. |
| T5.4 | Todo | Save and load modified chunks. | Start synchronous. Move to async only after correctness is proven. | Edit a block, exit, relaunch, confirm edit persists. |
| T5.5 | Todo | Add player position save. | Keep separate from chunk data. | Relaunch restores player near last position. |

MiniGame reference to use here:

| MiniGame idea | HelloMine3D version |
| ------------- | ------------------- |
| `WorldManager` session-level data | Later `WorldSession` or `WorldSave` metadata |
| `ChunkIOMgr` async command queue | Start sync, then evolve to command/result queue |
| Region files | Consider only after simple per-chunk files work |

## Milestone M7: Terrain and Content Expansion

Goal: add world variety without turning generation into one large function.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| T6.1 | Todo | Split terrain base and decorators. | Base height/biome first; tree/plant/ore/lake/structure passes after. | Adding a new decorator does not modify the core terrain loop heavily. |
| T6.2 | Todo | Add biome data definitions. | Start with a small table: grassland, forest, desert, ocean. | Biome choice affects top/fill block and decorations. |
| T6.3 | Todo | Add ore decorator. | Use seed-stable chunk random. | Same seed and chunk coords produce same ore positions. |
| T6.4 | Todo | Add cave pass. | Do after save/dirty is stable, because caves affect many blocks. | Caves are deterministic and do not break surface generation. |

## Milestone M8: Light and Visual Feedback

Goal: add visual quality after block updates, mesh state, and data models are stable.

| ID | Status | Task | Implementation notes | Validation |
| -- | ------ | ---- | -------------------- | ---------- |
| T7.1 | Todo | Add simple selected-block outline. | Use existing raycast result if available; otherwise add focused block pick. | Player can see which block will be edited. |
| T7.2 | Todo | Add basic sunlight storage. | 4-bit sunlight is enough long term, but explicit bytes are fine first. | Surface and caves have visibly different brightness. |
| T7.3 | Todo | Add block light storage. | Keep torch/lava light separate from skylight. | A test emissive block lights nearby mesh vertices. |
| T7.4 | Todo | Add local relight after edits. | Tie light dirty to mesh dirty. | Placing/removing an opaque block changes nearby lighting without full rebuild. |
| T7.5 | Todo | Improve transparent block rules. | Water, glass, leaves, and flora need explicit render pass and face-culling behavior. | Transparent blocks render without obvious missing faces or wrong draw order in common cases. |

## Validation Matrix

Every completed task should list which validations were run.

| Validation | Command or method | Required for |
| ---------- | ----------------- | ------------ |
| Make debug build | `sh scripts/build.sh` | All code changes |
| Make release build | `sh scripts/build.sh release` | Milestone completion |
| Xcode project generation | `sh xcode.sh` | Build system or file layout changes |
| Xcode list | `xcodebuild -list -project build/HelloMine3D.xcodeproj` | Build system changes |
| Xcode debug build | `xcodebuild -project build/HelloMine3D.xcodeproj -scheme HelloMine3D -configuration Debug -derivedDataPath build/XcodeDerivedData build` | Milestone completion on macOS |
| Asset check | `sh scripts/check_assets.sh` after T2.1 exists | Asset/data changes |
| Manual smoke run | `sh scripts/run.sh` | Runtime behavior changes |
| Block edit smoke test | Break/place center and boundary blocks | M1 and later mesh/block changes |
| Save/load smoke test | Edit, exit, relaunch, confirm persistence | M6 and later save changes |

## Iteration Report Template

Use this block in PR notes, commits, or follow-up docs after each iteration.

```text
Iteration:
Date:
Scope:
Task IDs:

Changed:
- 

Validation run:
- [ ] sh scripts/build.sh
- [ ] sh scripts/build.sh release
- [ ] sh xcode.sh
- [ ] xcodebuild -list -project build/HelloMine3D.xcodeproj
- [ ] Manual run
- [ ] Asset check

Before metrics:
- Loaded chunks:
- Dirty sections per edit:
- Section rebuild ms:
- Solid faces:
- Notes:

After metrics:
- Loaded chunks:
- Dirty sections per edit:
- Section rebuild ms:
- Solid faces:
- Notes:

Known risks:
- 

Next recommended task:
- 
```

## Near-Term Recommendation

The next implementation should start with M1, specifically:

1. T0.1 `ChunkSection::Layer` solid count correctness.
2. T0.2 explicit dirty flags.
3. T0.3 `setBlock()` to mesh dirty update path.
4. T0.4 boundary neighbor dirty marking.
5. T0.6 debug counters for dirty/rebuilt sections.

Do not start save/load, lighting, or new gameplay until this block edit loop is reliable.
