# Sandbox Foundation TODO List

本文档用于规划 HelloMine3D 复刻 `F:\env1_trunk` 沙盒基础能力的工作。目标不是直接搬运
MiniGame 的完整 `sandboxCore`，而是在当前项目已有的 `World / Chunk / Block / Mesh / Player`
基础上，逐步补齐一个可保存、可扩展、可承载冒险玩法的轻量沙盒内核。

> 本文档是 S0-S7 沙盒基础里程碑的**详细记录**。跨阶段的执行清单和后续 backlog 见
> `docs/todolist.md`；验证方式和覆盖范围见 `docs/runtime-validation.md`。

## Scope

本阶段重点复刻沙盒基础，不进入完整冒险玩法。

包含：

- 世界管理、出生点、世界元数据、传送入口。
- 区块加载、卸载、保存、dirty 标记和 mesh 刷新。
- 方块定义、方块属性、掉落、碰撞、交互入口。
- 地形生成、生物群系、结构生成、世界 seed。
- 实体基础、玩家基础、掉落物、简单 Mob。
- C++ 事件系统，用于后续接规则和脚本。
- 存档，包括区块、玩家、世界元信息和实体。
- 调试指标，包括区块、mesh、存档和实体数量。

暂不包含：

- 完整 Lua/UGC 脚本系统。
- 多人网络、云服、反作弊。
- GameMaker 编辑器和完整规则面板。
- 复杂 UI、任务链、Boss、完整冒险结算。
- 复杂物理引擎替换。

## Reference Points

MiniGame 可参考的方向：

- `F:\env1_trunk\client\miniSandbox\sandboxCore\worldData\WorldManager.*`
- `F:\env1_trunk\client\miniSandbox\sandboxCore\worldData\world.*`
- `F:\env1_trunk\client\miniSandbox\sandboxCore\worldData\world_types.h`
- `F:\env1_trunk\client\miniSandbox\sandboxCore\blocks\`
- `F:\env1_trunk\client\miniSandbox\sandboxCore\actors\`
- `F:\env1_trunk\client\miniSandbox\sandboxCore\terrgen\`
- `F:\env1_trunk\client\miniSandbox\sandboxCore\worldMesh\`

当前项目现有基础：

- `src/HelloMine3D/Application.cpp`
- `src/HelloMine3D/World/World.*`
- `src/HelloMine3D/World/Chunk/ChunkManager.*`
- `src/HelloMine3D/World/Chunk/Chunk.*`
- `src/HelloMine3D/World/Chunk/ChunkSection.*`
- `src/HelloMine3D/World/Chunk/ChunkMeshBuilder.*`
- `src/HelloMine3D/World/Block/BlockDatabase.*`
- `src/HelloMine3D/World/Event/PlayerDigEvent.*`
- `src/HelloMine3D/Player/Player.*`
- `src/HelloMine3D/Entity/Entity.h`

## Status Legend

| Status | Meaning |
| ------ | ------- |
| Todo | Not started. |
| Doing | In progress. |
| Verify | Implementation exists, validation pending. |
| Done | Implemented, tested, and documented. |
| Blocked | Needs a design decision or missing dependency. |

## Validation Status

2026-08-07 收敛：44 个条目此前全部停留在 `Verify`，本轮通过新增的
`HelloMine3DWorldRuntimeSmoke`（现为 97 项断言）、既有 4 个 headless 测试、渲染截图 smoke 和
性能基线全部完成运行时验证。

| 结果 | 数量 | 说明 |
| ---- | ---- | ---- |
| Done | 43 | 有自动化断言或客户端 smoke 覆盖。 |
| Verify | 1 | S7.1，见下方说明，需要调试面板截图。 |

验证入口：

```powershell
bin\HelloMine3DWorldRuntimeSmoke.exe
bin\HelloMine3DCoordinateTests.exe
bin\HelloMine3DMeshDirtyTests.exe
bin\HelloMine3DSaveLoadSmoke.exe
bin\HelloMine3DEntityLifecycleSmoke.exe
powershell -ExecutionPolicy Bypass -File tools\run_render_capture.ps1 -StopExisting -CaptureMs 4000,6000 -Seconds 12 -PlayerRotation "20 118.4 0"
powershell -ExecutionPolicy Bypass -File tools\run_perf_baseline.ps1 -StopExisting -WarmupMs 3000 -DurationMs 10000
```

## Milestone S0: Baseline Stabilization

Goal: make the current voxel world reliable before adding larger sandbox systems.

| ID | Status | Task | Validation evidence |
| -- | ------ | ---- | ------------------- |
| S0.1 | Done | Fix world/chunk/local coordinate conversion. | `S0.1/negative-chunk-mapping`, `S0.1/negative-setblock-targets-chunk`, `S0.1/negative-getblock-roundtrip`, `S0.1/cross-zero-chunk-split`, `S0.1/cross-zero-readback`; plus table-driven `HelloMine3DCoordinateTests`. |
| S0.2 | Done | Split chunk query and chunk creation APIs. | `S0.2/getblock-does-not-create-chunk`, `S0.2/setblock-does-not-create-chunk`, `S0.2/unloaded-getblock-returns-air` — chunk count is unchanged after reads and writes outside the loaded set. |
| S0.3 | Done | Stabilize background chunk loading. | Worker consumes atomics, never a `Camera&`. Client runs drive the loader to 289 loaded chunks / 449 mesh rebuilds with no stall or corruption (`bin/perf_baseline_20260807190255313-41064`). No data-race tooling exists on this MSVC toolchain; that gap is tracked in `docs/todolist.md`. |
| S0.4 | Done | Add explicit chunk and section states. | `S0.4/debug-stats-report-state`, `S0.4/debug-stats-report-seed`; the same counters appear per frame in the performance baseline CSV. |
| S0.5 | Done | Normalize mesh dirty propagation. | `S0.5/interior-edit-marks-owner`, `S0.5/interior-edit-skips-neighbor`, `S0.5/chunk-boundary-marks-owner`, `S0.5/chunk-boundary-marks-neighbor`, `S0.5/section-boundary-marks-owner`, `S0.5/section-boundary-marks-below`. |
| S0.6 | Done | Fix spawn preload coordinates. | `S0.6/spawn-preload-3x3` (9/9 chunks loaded), `S0.6/spawn-above-terrain`, `S0.6/spawn-on-solid-ground`. |

## Milestone S1: World Manager and Runtime Boundary

Goal: stop `Application` from owning gameplay directly and introduce a sandbox runtime layer.

| ID | Status | Task | Validation evidence |
| -- | ------ | ---- | ------------------- |
| S1.1 | Done | Add `SandboxRuntime`. | Every client smoke runs through `Application::on_update()` -> `SandboxRuntime`; render capture and performance baseline both pass. |
| S1.2 | Done | Add `WorldManager`. | `S1.2/active-world-is-created-world`, `S1.2/active-world-id`, `S1.2/save-world`, `S1.2/save-unknown-world-fails`, `S1.2/load-world`, `S1.2/load-world-activates`. |
| S1.3 | Done | Move spawn state into world metadata. | `S1.3/spawn-point-stable` plus `S2.1/world-meta-readable` — spawn survives a relaunch through `world.meta`. |
| S1.4 | Done | Add teleport API. | `S1.4/same-world-teleport`, `S1.4/teleport-moves-player`, `S1.4/teleport-preloads-destination`, `S1.4/cross-world-teleport-rejected`. |
| S1.5 | Done | Add fixed simulation tick. | `SandboxRuntime` uses the shared `FixedTickScheduler`; `V1/fixed-tick-scheduler-20hz` observes exactly 200 ticks over 10 seconds and `V1/fixed-tick-catchup-bounded` covers the catch-up cap. Performance capture records `simulation_ticks` and `simulation_tick_hz`, with a 19-21Hz script gate. |

## Milestone S2: Chunk Storage and World Persistence

Goal: modified terrain must survive unload and relaunch.

| ID | Status | Task | Validation evidence |
| -- | ------ | ---- | ------------------- |
| S2.1 | Done | Add `WorldSave` metadata. | `S2.1/world-save-succeeds`, `S2.1/world-meta-readable`, `S2.1/world-meta-has-seed`. |
| S2.2 | Done | Add `ChunkStorage`. | `S2.2/chunk-file-path-deterministic` — the path is derived from chunk coordinates and the file exists after a save. |
| S2.3 | Done | Define versioned chunk format. | `S2.3/corrupt-chunk-rejected` — a corrupted magic is reported and falls back to generation. `HelloMine3DSaveLoadSmoke` covers the id/metadata/block-entity roundtrip. |
| S2.4 | Done | Save dirty chunks on unload. | `S2.4/edit-marks-chunk-dirty`, `S2.4/chunk-dropped-on-unload`, `S2.4/edit-survives-unload-reload`, `S2.4/reloaded-chunk-is-clean`, `S2.4/chunk-marked-save-dirty`. |
| S2.5 | Done | Load stored chunks before procedural generation. | `S2.5/chunk-edit-survives-relaunch` — three edits in two chunks read back after a full world teardown and reconstruction. |
| S2.6 | Done | Save player state. | `S2.6/player-position-restored`, `S2.6/player-rotation-restored`, `S2.6/player-inventory-restored`. |

## Milestone S3: Block Definition and Interaction Layer

Goal: move from simple block IDs to data-driven block properties and unified interaction.

| ID | Status | Task | Validation evidence |
| -- | ------ | ---- | ------------------- |
| S3.1 | Done | Introduce `BlockDefinition`. | `S3.1/block-definition-populated` (string id, solid, default drop), `S3.1/liquid-definition-flagged`. |
| S3.2 | Done | Add `BlockRenderInfo`. | Mesh builder reads texture coordinates, render pass and shape from definitions; the render capture shows correct solid/water/flora passes. |
| S3.3 | Done | Add `ChunkBlock` metadata field. | `S3.3/block-metadata-roundtrip`; `HelloMine3DSaveLoadSmoke` covers metadata through the save format. |
| S3.4 | Done | Add `BlockInteractionSystem`. | `S3.4/break-through-interaction-system`, `S3.4/break-clears-block`, `S3.4/place-through-interaction-system`, `S3.4/place-sets-block`, `S3.4/place-consumes-item`, `S3.4/break-air-is-noop`. `use` interactions remain a later extension point. |
| S3.5 | Done | Add drop rules. | `S3.5/break-adds-configured-drop` — breaking stone yields the configured material into the inventory. |
| S3.6 | Done | Add first stateful block placeholder. | `HelloMine3DSaveLoadSmoke` asserts the block-entity record roundtrip. No container UI or runtime behaviour yet, as designed. |

## Milestone S4: Event System

Goal: expose stable gameplay events before adding rules, scripts, or adventure logic.

| ID | Status | Task | Validation evidence |
| -- | ------ | ---- | ------------------- |
| S4.1 | Done | Add `SandboxEventBus`. | The runtime smoke subscribes to all 15 event types and unsubscribes cleanly across 6 scenarios. |
| S4.2 | Done | Add block events. | `S4.2/break-publishes-events`, `S4.2/place-publishes-events` — exactly one break/place and one changed event per successful interaction, none on a failed one. `BlockUseEvent` stays reserved. |
| S4.3 | Done | Add chunk events. | `S4.3/chunk-load-events` (9 loaded, 9 generated), `S4.3/chunk-save-event`, `S4.3/chunk-unload-event`. |
| S4.4 | Done | Add entity events. | `S4.4/entity-spawn-event`, `S4.4/entity-damage-event`, `S4.4/entity-death-event`, `S4.4/item-pickup-event`. |
| S4.5 | Done | Add player events. | `S4.5/teleport-publishes-event`, `S4.5/break-publishes-inventory-event`, `S4.5/pickup-publishes-inventory-event`. |

## Milestone S5: Entity, Actor, and Player Foundation

Goal: replace the current thin `Entity` with a minimal actor model that can support mobs and adventure.

| ID | Status | Task | Validation evidence |
| -- | ------ | ---- | ------------------- |
| S5.1 | Done | Split `Entity` and `Actor`. | `S5.1/actor-count-tracked`, `S5.1/dead-actors-removed` through `World::tick()`. |
| S5.2 | Done | Add `LivingActor`. | `S5.2/mob-is-living-actor`, `S5.2/mob-takes-damage`, `S5.2/mob-dies`. Invulnerability timing is still not implemented. |
| S5.3 | Done | Add `PlayerActor` and `PlayerController`. | `PlayerInputState` feeds the same `PlayerController::applyInput()` path as live SFML collection. `V2/*` asserts movement, flying jump, fly/sneak toggles, look delta and hotbar selection without moving the real mouse. |
| S5.4 | Done | Add `Inventory` and hotbar. | `S2.6/player-inventory-restored`, `S3.5/break-adds-configured-drop`, `S3.4/place-consumes-item`, `S5.5/item-entity-picked-up`. |
| S5.5 | Done | Add `ItemEntity`. | `S5.5/item-entity-spawns`, `S5.5/item-entity-found`, `S5.5/item-entity-picked-up`. Rendering and entity persistence remain open (see `docs/todolist.md`). |
| S5.6 | Done | Add first `MobActor`. | `S5.6/mob-spawns`, `S5.6/mob-wanders-on-tick`, plus the damage/death chain above and `HelloMine3DEntityLifecycleSmoke`. |

## Milestone S6: Terrain Generation and Content Structure

Goal: keep procedural generation deterministic and extensible.

| ID | Status | Task | Validation evidence |
| -- | ------ | ---- | ------------------- |
| S6.1 | Done | Add world seed ownership. | `S6.1/same-seed-same-terrain` over 175104 sampled blocks, `S6.1/seed-restored-from-save`. |
| S6.2 | Done | Split terrain base and decorators. | `S6.2/decorators-produce-trees` (161 tree blocks over 49 chunks), `S6.2/decorators-produce-plants` (97 plant blocks). |
| S6.3 | Done | Formalize biome definitions. | `S6.3/biome-surface-variety` — 8 distinct surface block ids across the sampled region. |
| S6.4 | Done | Add ore decorator. | `S6.4/ore-decorator-produces-ore` (coal 185, iron 171), `S6.4/ore-layout-deterministic`. Dedicated ore textures are still missing; tracked in `docs/todolist.md`. |
| S6.5 | Done | Add structure placement boundary rules. | `S6.5/structures-survive-reload` — identical tree and plant counts after all 49 chunks go through a save/load roundtrip. `S6.5/surface-composition-stable`. |

## Milestone S7: Debugging and Validation

Goal: make sandbox behavior measurable while the foundation is being rewritten.

| ID | Status | Task | Validation evidence |
| -- | ------ | ---- | ------------------- |
| S7.1 | Verify | Add sandbox debug panel. | The data source `World::collectDebugStats()` is asserted by `S0.4/debug-stats-report-state` and sampled every frame by the performance baseline. The rendered ImGui panel sits behind the F1 toggle, which capture mode cannot reach. Needs one screenshot with the panel open. |
| S7.2 | Done | Add focused save/load smoke test. | `bin\HelloMine3DSaveLoadSmoke.exe` passes; full client relaunch persistence is now also covered by `casePersistence` and `caseUnloadPersistence`. |
| S7.3 | Done | Add coordinate conversion tests. | `bin\HelloMine3DCoordinateTests.exe` passes; runtime behaviour covered by `caseNegativeCoordinates`. |
| S7.4 | Done | Add mesh dirty validation scenario. | `bin\HelloMine3DMeshDirtyTests.exe` passes; runtime section state covered by `caseMeshDirtyPropagation`. |
| S7.5 | Done | Add entity lifecycle smoke test. | `bin\HelloMine3DEntityLifecycleSmoke.exe` passes; in-world lifecycle covered by `caseActors`. |

## First Playable Foundation Target

The sandbox foundation can be considered ready for adventure gameplay when all of the following are true:

| # | Criterion | Status |
| - | --------- | ------ |
| 1 | The player can move across positive and negative chunk coordinates. | Met (S0.1) |
| 2 | Chunks load, unload, save, and reload modified block data. | Met (S2.4, S2.5) |
| 3 | Block break/place/use goes through one interaction layer. | Partly met — break/place done, `use` not implemented (S3.4) |
| 4 | Mesh updates are local, automatic, and visible on chunk/section boundaries. | Met (S0.5) |
| 5 | World metadata stores seed, spawn point, and player position. | Met (S2.1, S2.6) |
| 6 | A basic inventory and hotbar exist. | Met (S5.4) |
| 7 | Dropped item entities can spawn and be picked up. | Partly met — logic works, entities are not rendered |
| 8 | At least one simple Mob actor can spawn, tick, take damage, die, and drop an item. | Partly met — logic works, mobs are not rendered |
| 9 | Debug UI or logs expose chunk, mesh, save, and entity counters. | Met (S0.4, S7.1 data path) |
| 10 | A smoke test verifies save/load and coordinate conversion. | Met (S7.2, S7.3) |

The three partly-met criteria are carried into `docs/todolist.md` as the next
closed loop; entity rendering is the single biggest blocker.

## Notes

- Keep each milestone independently buildable.
- Windows builds are generated with `vs2022.bat` and built from
  `build/HelloMine3D.sln`. The SFML external project caches absolute paths in
  `build/External/sfml/CMakeCache.txt`; moving or renaming the repository
  requires deleting that build tree before rebuilding.
- Prefer simple synchronous save/load first; add async IO only after correctness is proven.
- Prefer C++ events and JSON/TOML-style data first; add Lua only after the native extension points are stable.
- Avoid copying MiniGame subsystem complexity directly. Use it as an architectural reference, not as a code migration source.
