# Sandbox Foundation TODO List

本文档用于规划 HelloMine3D 复刻 `F:\env1_trunk` 沙盒基础能力的工作。目标不是直接搬运
MiniGame 的完整 `sandboxCore`，而是在当前项目已有的 `World / Chunk / Block / Mesh / Player`
基础上，逐步补齐一个可保存、可扩展、可承载冒险玩法的轻量沙盒内核。

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

## Milestone S0: Baseline Stabilization

Goal: make the current voxel world reliable before adding larger sandbox systems.

| ID | Status | Task | Notes | Validation |
| -- | ------ | ---- | ----- | ---------- |
| S0.1 | Verify | Fix world/chunk/local coordinate conversion. | Added floor-division helpers and floor-based float-to-block conversion. Removed the player positive-X/Z clamp. Runtime movement across zero still needs manual validation. | Player can cross X/Z zero; block lookup, placement, and mesh update still target the correct chunk. |
| S0.2 | Verify | Split chunk query and chunk creation APIs. | Added `findChunk()` and `getOrCreateChunk()`. World block reads, writes, and dirty marking now avoid implicit chunk creation. Remaining mesh-neighbor paths still intentionally rely on preloaded chunks. | Mesh building and block reads do not create unexpected empty chunks. |
| S0.3 | Verify | Stabilize background chunk loading. | Worker thread now reads an atomic chunk load center submitted by the main thread instead of holding a `Camera&`. Mesh build no longer depends on camera frustum in the worker. Runtime behavior still needs manual validation after the SFML link issue is cleared. | Code review shows no unsynchronized camera access from the worker thread. |
| S0.4 | Verify | Add explicit chunk and section states. | Added `ChunkLoadState`, save-dirty tracking, `ChunkSectionMeshState`, and `ChunkDebugStats` aggregation. VS2017 Debug build passes. Runtime debug-panel display is still part of S7.1. | Debug output can show state counts for loaded chunks and dirty sections. |
| S0.5 | Verify | Normalize mesh dirty propagation. | `World::setBlock()` now owns mesh dirty propagation for the edited section and boundary neighbors. Mesh rebuild clears stale CPU mesh data before rebuilding. Runtime boundary validation still pending. | Break/place blocks in the middle and on section/chunk boundaries; visible faces update correctly. |
| S0.6 | Verify | Fix spawn preload coordinates. | Spawn preloading now derives the 3x3 preload area from the spawn chunk instead of world block coordinates. Runtime startup validation still pending. | Spawn area is visible and stable immediately after entering the world. |

## Milestone S1: World Manager and Runtime Boundary

Goal: stop `Application` from owning gameplay directly and introduce a sandbox runtime layer.

| ID | Status | Task | Notes | Validation |
| -- | ------ | ---- | ----- | ---------- |
| S1.1 | Verify | Add `SandboxRuntime`. | Added `SandboxRuntime`; it owns the player, `WorldManager`, input-driven block interaction dispatch, and a 20Hz fixed tick accumulator. `Application` now delegates update/render to the runtime. Runtime smoke is still pending. | `Application::on_update()` delegates sandbox update/tick instead of directly driving world gameplay. |
| S1.2 | Verify | Add `WorldManager`. | Added lightweight single-active-world manager with `createWorld()`, `getWorld()`, `getActiveWorld()`, `saveWorld()`, `loadWorld()`, world time, and per-world save directory routing for future map ids. Runtime smoke is still pending. | Code has `createWorld()`, `getWorld()`, `getActiveWorld()`, `saveWorld()`, `loadWorld()`. |
| S1.3 | Verify | Move spawn state into world metadata. | Added `WorldSaveData::spawnPoint`; existing worlds restore spawn from `world.meta`, new worlds compute once and save it. Full `WorldManager` ownership is still tracked by S1.2. | Relaunch restores the same spawn point for an existing world. |
| S1.4 | Verify | Add teleport API. | Added `WorldManager::teleportPlayer()` for same-world teleports; target map id is present but non-active map transfer intentionally returns false until multi-world switching is implemented. Runtime smoke is still pending. | Player can be moved to a target block position through `WorldManager`. |
| S1.5 | Verify | Add fixed simulation tick. | `SandboxRuntime` now accumulates frame time and runs player/world-manager simulation at 20Hz with a per-frame catch-up cap. Runtime smoke is still pending. | Player/world/entity simulation can run independently from render frame rate. |

## Milestone S2: Chunk Storage and World Persistence

Goal: modified terrain must survive unload and relaunch.

| ID | Status | Task | Notes | Validation |
| -- | ------ | ---- | ----- | ---------- |
| S2.1 | Verify | Add `WorldSave` metadata. | Added `WorldSave` under `World/Storage`; it writes `bin/saves/default/world.meta` with world id/name, version, seed, spawn point, time, active generator, and player-state fields. Runtime relaunch smoke is still pending. | World metadata file is created and reloadable. |
| S2.2 | Verify | Add `ChunkStorage`. | Added simple per-chunk binary files under `bin/saves/default/chunks`. VS2017 Debug build passes. Runtime save/load smoke still pending. | Chunk file path is deterministic from chunk coordinates. |
| S2.3 | Verify | Define versioned chunk format. | Chunk format is now versioned with magic, chunk X/Z, chunk size, section count, block id payload, metadata payload, and reserved block-entity records. Loader still accepts version 1 block-id-only chunk files. Runtime save/load smoke is still pending. | Loader rejects invalid magic/version with clear diagnostics. |
| S2.4 | Verify | Save dirty chunks on unload. | Chunk save dirty is separate from mesh dirty. Dirty chunks are saved on view-distance unload and world shutdown. Runtime unload/reload validation still pending. | Edit a block, walk far enough to unload, return; edit persists. |
| S2.5 | Verify | Load stored chunks before procedural generation. | `ChunkManager::loadChunk()` now tries `ChunkStorage` first and falls back to procedural generation when no saved chunk exists. Runtime relaunch validation still pending. | Relaunch after block edits restores modified terrain. |
| S2.6 | Verify | Save player state. | Added `PlayerSaveState`; world save/load now persists position, rotation, selected hotbar slot, and basic inventory material ids/counts. Runtime relaunch smoke is still pending. | Relaunch restores player near last saved location. |

## Milestone S3: Block Definition and Interaction Layer

Goal: move from simple block IDs to data-driven block properties and unified interaction.

| ID | Status | Task | Notes | Validation |
| -- | ------ | ---- | ----- | ---------- |
| S3.1 | Verify | Introduce `BlockDefinition`. | Added `BlockDefinition` with id, string id, name, hardness, solid, collidable, transparent, liquid, light, default drop, and render info derived from existing `.block` data. Runtime/debug inspection is still pending. | Existing blocks load through the new definition layer with defaults. |
| S3.2 | Verify | Add `BlockRenderInfo`. | Added `BlockRenderInfo` under `BlockDefinition`; mesh builder now reads texture coordinates, render pass, and shape from definitions instead of direct `BlockDataHolder` fields. Runtime visual smoke is still pending. | Mesh builder reads render info from definitions. |
| S3.3 | Verify | Add `ChunkBlock` metadata field. | Added explicit `uint8_t` metadata to `ChunkBlock`; equality and save/load now include metadata while old chunk files default metadata to 0. Runtime save/load smoke is still pending. | Existing chunks default metadata to 0. |
| S3.4 | Verify | Add `BlockInteractionSystem`. | Added `BlockInteractionSystem`; `PlayerDigEvent` now delegates break/place behavior instead of directly mutating blocks and inventory. `use` interactions are still a later extension point. Runtime smoke is still pending. | `PlayerDigEvent` delegates to interaction system instead of directly setting blocks. |
| S3.5 | Verify | Add drop rules. | Break behavior now reads `BlockDefinition::defaultDrop` and adds the configured material to player inventory before setting the block to air. Item-entity fallback is still tracked by S5.5. Runtime smoke is still pending. | Breaking grass/stone/wood produces configured drops. |
| S3.6 | Verify | Add first stateful block placeholder. | Added `BlockEntityRecord` and chunk-level block-entity storage hooks. Version 2 chunk files reserve serialized block-entity records, but no container UI or runtime block-entity behavior is implemented yet. Runtime save/load smoke is still pending. | Save format can reserve block-entity data without forcing container UI now. |

## Milestone S4: Event System

Goal: expose stable gameplay events before adding rules, scripts, or adventure logic.

| ID | Status | Task | Notes | Validation |
| -- | ------ | ---- | ----- | ---------- |
| S4.1 | Verify | Add `SandboxEventBus`. | Added synchronous C++ event bus with subscribe/unsubscribe/publish by event type. It is owned by `World` and intentionally has no async queue yet. Runtime subscriber smoke is still pending. | Systems can subscribe/unsubscribe without direct dependencies. |
| S4.2 | Verify | Add block events. | Added `BlockBreakEvent`, `BlockPlaceEvent`, and `BlockChangedEvent`; break/place interactions publish them after successful mutations. `BlockUseEvent` is reserved until use interactions exist. Runtime subscriber smoke is still pending. | Events fire once per successful interaction. |
| S4.3 | Verify | Add chunk events. | Added `ChunkGeneratedEvent`, `ChunkLoadedEvent`, `ChunkUnloadedEvent`, and `ChunkSavedEvent`; `ChunkManager` publishes them from generation/load/save/unload paths. Runtime subscriber/log smoke is still pending. | Debug logs show chunk lifecycle order. |
| S4.4 | Verify | Add entity events. | Added `EntitySpawnEvent`, `EntityDamageEvent`, `EntityDeathEvent`, and `ItemPickupEvent`. `Actor::enterWorld()` publishes spawn, `LivingActor::damage()` publishes damage/death, and `ItemEntity` publishes pickup. Runtime subscriber smoke is still pending. | Events can drive simple logging and future rules. |
| S4.5 | Verify | Add player events. | Added `PlayerSpawnEvent`, `PlayerTeleportEvent`, and `PlayerInventoryChangedEvent`. `WorldManager` publishes spawn/teleport events; block break/place publishes inventory changes after successful stack mutation. Runtime subscriber smoke is still pending. | Player state changes are observable outside `Player` itself. |

## Milestone S5: Entity, Actor, and Player Foundation

Goal: replace the current thin `Entity` with a minimal actor model that can support mobs and adventure.

| ID | Status | Task | Notes | Validation |
| -- | ------ | ---- | ----- | ---------- |
| S5.1 | Verify | Split `Entity` and `Actor`. | Added `Actor` with id, type, alive state, tick hook, enter-world event hook, and save-state helpers while leaving the existing `Entity` transform/collision shape intact. Added `ActorManager` and connected it to `World::tick()` for runtime actor lifecycle. Entity persistence is still pending. | Existing player can be represented without breaking movement. |
| S5.2 | Verify | Add `LivingActor`. | Added `LivingActor` with health, max health, damage, heal, death state, and damage/death event publishing. Invulnerability timing is still pending. | Player can take damage and die in a controlled test. |
| S5.3 | Verify | Add `PlayerActor` and `PlayerController`. | Added `PlayerActor` with sync helpers for the legacy `Player`. Added `PlayerController` to own keyboard/mouse/toggle input state; `Player::handleInput()` now delegates to it while `Player` keeps physics, collision, inventory, and save state. Verified with VS2017 Debug build and existing headless smoke tests. | Input code no longer lives entirely inside player state. |
| S5.4 | Verify | Add `Inventory` and hotbar. | Added `Inventory` with explicit slots, selected hotbar slot, add/remove APIs, and save-state conversion. `Player` now delegates inventory storage and hotbar selection to it while preserving existing controls. Runtime smoke is still pending. | Break/place consumes and adds item stacks consistently. |
| S5.5 | Verify | Add `ItemEntity`. | Added `ItemEntity` under `ActorManager`; block drops go to inventory when possible and spawn an item entity when the inventory is full. Item entities tick, apply light gravity/collision, attempt nearby player pickup, and publish pickup/inventory events. Rendering and entity persistence are still pending. | Breaking a block can spawn a pickup when inventory is full or configured. |
| S5.6 | Verify | Add first `MobActor`. | Added `MobActor` with deterministic wander stepping, configurable drop data, and `World::spawnMob()`. World-aware damage now drops mob loot through `MobActor::dropLoot()` after death, while the headless entity smoke verifies spawn, movement, damage, death, drop configuration, and actor cleanup. Dedicated mob rendering/chase AI remain later gameplay polish. | Mob spawns, moves, can be damaged, and can die/drop an item. |

## Milestone S6: Terrain Generation and Content Structure

Goal: keep procedural generation deterministic and extensible.

| ID | Status | Task | Notes | Validation |
| -- | ------ | ---- | ----- | ---------- |
| S6.1 | Verify | Add world seed ownership. | `WorldSaveData::seed` now drives `ChunkManager::setTerrainSeed()` before chunk generation. `ClassicOverWorldGenerator` uses an instance seed for biome noise, biome height generators, and per-chunk decorator RNG. Existing legacy saves with seed 0 remain deterministic from seed 0; same-seed sampling smoke is still pending. | Same seed generates same sampled heights and blocks. |
| S6.2 | Verify | Split terrain base and decorators. | `ClassicOverWorldGenerator` now runs explicit base terrain, ore, plant, and tree decorator passes. Base terrain fills water/stone/dirt/surface blocks and only records decoration positions; decorators mutate the generated chunk afterward. Runtime terrain smoke is still pending. | Adding ore generation does not modify core height generation heavily. |
| S6.3 | Verify | Formalize biome definitions. | Added `BiomeDefinition` with height-noise parameters, tree/plant frequencies, and default top/underwater/beach/plant blocks. Existing biomes now construct from definition data while retaining their current random block and tree selection behavior. Verified with VS2017 Debug build and the existing headless smoke tests. | Biome affects top block, fill block, plants, and trees predictably. |
| S6.4 | Verify | Add ore decorator. | Added `CoalOre` and `IronOre` block/material ids and block data. Ore generation is a deterministic decorator pass seeded from world seed plus chunk coordinate and only replaces stone. Dedicated ore textures are still pending; both ores currently reuse stone texture coordinates. | Same seed/chunk produces the same ore layout. |
| S6.5 | Verify | Add structure placement boundary rules. | Tree/large-structure decoration points now require a safe margin inside the chunk before placement, preventing current one-chunk structure builders from writing across chunk boundaries. Cross-chunk structure support remains a future extension. | Trees/structures near chunk edges do not disappear or duplicate after reload. |

## Milestone S7: Debugging and Validation

Goal: make sandbox behavior measurable while the foundation is being rewritten.

| ID | Status | Task | Notes | Validation |
| -- | ------ | ---- | ----- | ---------- |
| S7.1 | Verify | Add sandbox debug panel. | Added a `Sandbox` ImGui panel behind the existing debug toggle. It reads `World::collectDebugStats()` under the World main lock and shows seed, world time, actor count, existing/loaded chunks, dirty chunks, queued chunk updates, section mesh states, and cumulative mesh rebuilds. There is still no async save queue; dirty chunk count is the current save visibility metric. Runtime smoke is still pending. | Debug panel updates while moving/editing blocks. |
| S7.2 | Verify | Add focused save/load smoke test. | Added headless `HelloMine3DSaveLoadSmoke` runner over the shared chunk data serialization layer. It writes a temporary chunk save, reloads it, and asserts block id, metadata, and block-entity record roundtrip. Also routed `World` chunk storage through the selected world save directory so metadata and chunk files use the same save root. Verified with `bin\HelloMine3DSaveLoadSmoke.exe`. Full client relaunch validation remains a future/manual runtime check. | Test catches basic persistence regressions. |
| S7.3 | Verify | Add coordinate conversion tests. | Added lightweight `WorldCoordinates` helpers and kept `World` static coordinate APIs delegating to them. Added `HelloMine3DCoordinateTests` runner with table-driven positive/negative floor-div, floor-mod, block/chunk coordinate, and float-to-block cases. Verified with `bin\HelloMine3DCoordinateTests.exe`. | Floor division helpers pass table-driven cases. |
| S7.4 | Verify | Add mesh dirty validation scenario. | Added `ChunkUpdatePlanner` as the shared production planner for block-edit mesh refresh keys, and wired `World::queueChunkUpdate()` through it. Added `HelloMine3DMeshDirtyTests` to cover interior blocks, positive/negative chunk boundaries, section top/bottom boundaries, corner combinations, and ignored negative Y edits. Verified with `bin\HelloMine3DMeshDirtyTests.exe`. Full rendered neighbor-face validation remains a runtime/manual check. | Neighbor faces update without manual reload. |
| S7.5 | Verify | Add entity lifecycle smoke test. | Added `HelloMine3DEntityLifecycleSmoke` and split actor lifecycle entry points so headless tests can use `SandboxEventBus` directly while production keeps `World` adapters. The smoke adds a `MobActor` through `ActorManager`, asserts spawn/damage/death events, applies lethal damage, and verifies dead actor cleanup. Drops and full runtime save/reload remain part of S5.6/entity persistence follow-up. Verified with `bin\HelloMine3DEntityLifecycleSmoke.exe`. | Entity lifecycle can be verified without full adventure mode. |

## First Playable Foundation Target

The sandbox foundation can be considered ready for adventure gameplay when all of the following are true:

1. The player can move across positive and negative chunk coordinates.
2. Chunks load, unload, save, and reload modified block data.
3. Block break/place/use goes through one interaction layer.
4. Mesh updates are local, automatic, and visible on chunk/section boundaries.
5. World metadata stores seed, spawn point, and player position.
6. A basic inventory and hotbar exist.
7. Dropped item entities can spawn and be picked up.
8. At least one simple Mob actor can spawn, tick, take damage, die, and drop an item.
9. Debug UI or logs expose chunk, mesh, save, and entity counters.
10. A smoke test verifies save/load and coordinate conversion.

## Recommended Implementation Order

1. S0.1-S0.6: stabilize coordinates, chunk APIs, loader threading, and mesh dirty flow.
2. S2.1-S2.6: add world/chunk/player persistence before expanding gameplay state.
3. S1.1-S1.5: introduce sandbox runtime and world manager boundary.
4. S3.1-S3.5: upgrade block definitions and interaction.
5. S4.1-S4.5: add event bus and core events.
6. S5.1-S5.6: split player, inventory, actors, mobs, and item entities.
7. S6.1-S6.5: make generation deterministic and extensible.
8. S7.1-S7.5: keep validation and debug visibility current during all phases.

## Notes

- Keep each milestone independently buildable.
- VS2017 Debug requires ProgramDatabase debug info and the local SFML 3 inline-constexpr compatibility patch.
- Prefer simple synchronous save/load first; add async IO only after correctness is proven.
- Prefer C++ events and JSON/TOML-style data first; add Lua only after the native extension points are stable.
- Avoid copying MiniGame subsystem complexity directly. Use it as an architectural reference, not as a code migration source.
