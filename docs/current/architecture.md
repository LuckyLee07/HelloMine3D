# HelloMine3D Current Architecture Baseline

本文冻结 `AL-A0 — Latest Architecture Baseline` 审计到的当前实现，而不是描述未来目标架构。
审计起点为 Git commit `4930023fb2f3022daac9968c10a1a0b76e1ac392`；冻结的
PLAYABILITY-RC 运行时代码身份仍是
`320e293c2f1db7f46aba776ddccdcf94369f2d05`。A0 只更新文档，没有移动源码、改变 Gameplay、
引入 AL-A1 wrapper 或提前实现 Chunk Residency。

性能、发行包和验证结果见
`docs/reports/architecture-lab-baseline-v1.md`；未来候选架构见
`docs/current/architecture-lab-roadmap-v1.md`。两者不得反向覆盖本文记录的当前事实。

## 1. Repository and Build Boundary

| Path | Ownership |
| ---- | --------- |
| `src/HelloMine3D/` | 第一方游戏、应用、音频、诊断和 Ogre 适配代码。 |
| `src/Engine/`、`src/external/` | 构建内的引擎与第三方依赖，不属于第一方模块责任图。 |
| `media/` | shader、方块/形状/物品/配方/敌人/目标/声音/文本等运行时资源。 |
| `bin/` | 可执行输出、运行时模板以及本机生成的保存、日志和证据。 |
| `premake/` | 规范工程图入口；当前 Windows 工具链为 VS2017/v141。 |
| `scripts/`、`tools/` | 构建、验证、性能、打包、诊断和证据采集。 |
| `docs/` | 当前状态、合同、报告和历史归档。 |

Premake 从共享的 `src/HelloMine3D` 与资源边界生成 `build/` 下工程，客户端只产生一个
`HelloMine3D.exe`。运行时代码不得把 `build/` 或仓库文档当成 Gameplay 输入。

## 2. First-party Top-level Module Inventory

下表覆盖审计时 `src/HelloMine3D/` 的全部 17 个顶层目录，以及 5 个根级配置/输入源文件。
“权威/派生”描述运行时所有权，不表示每个模块只能包含一种数据。

| Module | Size at A0 | Responsibility | Authoritative state / derived state | Main dependency direction |
| ------ | ---------- | -------------- | --------------------------- | ------------------------- |
| `World/` | 102 files / 17,666 lines | 区块、方块、生成、光照、交互、区块网格 CPU 数据、世界模拟和持久化组合根。 | 方块/区块、block entity、世界元数据、World 内 Actor/战斗/进度实例为权威；光照、mesh、debug snapshot 为可重建或派生。 | 依赖 Actor、Gameplay、Item、Player、Sandbox Events、Diagnostics、Maths、Physics、Util；不得依赖 Ogre。 |
| `Sandbox/` | 17 / 1,183 | 应用状态、固定 tick 编排、世界集合/活动世界、输入到 World action 的协调、类型化事件协议。 | `GameApplicationFlow`、活动 world id 和调度器累积时间为运行时编排状态；事件是已发生事实，不是持久化真值。 | 依赖 World、Player、Core/Camera、Feedback、Item、Diagnostics；不依赖 Ogre。 |
| `Actor/` | 21 / 2,348 | Actor id、生命周期、Living/Mob/Player/Item actor 行为、存档值和不可变渲染快照。 | `ActorManager` 拥有的 Actor 实例为权威；`ActorSnapshot` 与 `ActorSaveState` 是发布/序列化值。 | 由 World 拥有；Actor tick 可回调 World 并发布 Sandbox 事件；依赖 Item、Player、Entity、Maths。 |
| `Feedback/` | 2 / 361 | 从已提交领域事件生成有界 recoil、hit-stop、粒子等表现时间线。 | 全部为派生表现状态；不得改变战斗、方块、库存或存档结果。 | 订阅 Sandbox EventBus；由 Sandbox 更新，Ogre 只消费 snapshot。 |
| `Gameplay/` | 15 / 2,340 | 目标、Alpha Journey 兼容视图、胜利、Waystone 遭遇、难度、探索奖励和胜利后事件语义。 | 注册表冻结定义和 World 所持运行时实例/保存 payload 为权威；HUD/progress snapshot 为派生。目标 definition 当前为 v3。 | 依赖 Actor、Item、Player、Sandbox Events、Maths/Util；具体实例由 World 组合。 |
| `Audio/` | 12 / 2,626 | cue/music 定义、样本缓存、流式音乐状态、真实/静默后端和音频统计。 | 定义与播放状态只对音频域权威，不是 Gameplay 真值；caption/cue 输出为派生。 | 订阅 Sandbox facts；使用 Maths/Util；由 Ogre shell 组合和逐帧更新。 |
| `Presentation/` | 8 / 812 | 语义文本、locale fallback、caption 生命周期/优先级和布局探针。 | catalogue 是显示语义来源；渲染文本和布局为派生，翻译字符串不得充当玩法 identity。 | 依赖 Item/Util；Ogre UI 消费，不反向修改 Gameplay。 |
| `Ogre/` | 17 / 8,940 | Ogre/GL3Plus/OIS 启动、窗口/焦点/输入、GPU terrain/actor/UI、音频组合、截图和帧序。 | GPU buffer、scene node、UI、selection outline、capture 为派生；绝不拥有 Gameplay truth。 | 向内依赖 Sandbox、World snapshots、Actor/Audio/Presentation/Diagnostics/Item/Gameplay 等；第一方模拟层不得反向依赖 Ogre。 |
| `Diagnostics/` | 16 / 2,581 | 性能采集、Q2 操作阶段、Tracy 边界、崩溃 dump/sidecar/inbox 和 terrain buffer metrics。 | 指标和崩溃产物是观察/诊断记录，不驱动 Gameplay。 | 可被 World/Sandbox/Ogre 使用；Windows 异常与 DbgHelp 只留在平台实现。 |
| `Player/` | 4 / 557 | 玩家运动、碰撞、输入应用、库存访问、容器/制作 UI ownership 和保存值。 | `Player` 拥有当前运动、旋转、库存与 UI 打开状态；战斗生命由 World 的 `PlayerActor` 镜像/覆盖后存盘。 | 依赖 Entity、Item、World 查询、Sandbox Events；由 SandboxRuntime 拥有。 |
| `Item/` | 21 / 3,866 | Material/ItemStack、库存/容器、配方/制作、工具、食物、冶炼和资源经济校验。 | 冻结注册表与 Inventory/Container 内容为各自域的权威值；预览和统计为派生。 | 主要依赖 Util，少数交互边界依赖 World；被 Player/World/Gameplay/UI 消费。 |
| `Physics/` | 1 / 45 | AABB 数据和碰撞辅助边界。 | 无独立生命周期所有权；AABB 是 Entity/Player/Actor 的空间值。 | 依赖 Maths；被 Entity/World 使用。 |
| `Entity/` | 1 / 32 | 最低层 position/velocity/rotation/AABB 数据基类。 | 不拥有对象生命周期；派生实例由 Player 或 ActorManager 拥有。 | 依赖 Maths/Physics；被 Player、Actor、Camera 使用。 |
| `Core/` | 2 / 79 | 当前只有逻辑 Camera：跟随目标、矩阵和 frustum。 | Camera 是从玩家/配置推导的视图状态，不是世界真值。 | 依赖 Config、Entity、Maths；被 Sandbox、World streaming priority 和 Ogre 使用。 |
| `Maths/` | 13 / 375 | GLM 边界、矩阵、frustum、ray、坐标与噪声算法。 | 以纯值/纯算法为主，无运行时组合根。 | 被各层使用；个别旧 helper 仍引用 Camera/Entity/World 常量。 |
| `Util/` | 11 / 823 | 文件、路径、资源包解析、随机和通用容器/生命周期 helper。 | effective resource view 从磁盘资源派生；随机单例只用于明确允许的非确定性入口。 | 被多数数据/运行时模块使用，不拥有 Gameplay。 |
| `Tests/` | 16 / 20,868 | 13 个 headless/Smoke/Soak 目标及崩溃符号化工具。 | 仅验证证据；fixture 和注入不构成真实窗口可玩性。 | 可依赖所有受测模块；生产模块不得依赖 Tests。 |
| root `Config.h`, `GameplayInput.*`, `RuntimeConfig.*` | 5 / 1,373 | 平台无关输入语义、绑定/冲突/hold-mode、内存配置和 settings v8 解析/原子发布。 | 已加载 `Config` 是应用配置真值；磁盘 `settings.txt` 是持久来源，UI draft 是派生/待提交。 | 被 Ogre 输入壳、Sandbox、Core、Audio/Feedback 和 World 创建入口消费。 |

## 3. Current Dependency Direction

当前实现不是严格无环分层；A0 冻结实际依赖，而不把目标设计冒充现状。

```text
Ogre shell
  -> SandboxRuntime / WorldManager
  -> World snapshots and commands
  -> Audio / Presentation / Diagnostics

SandboxRuntime
  -> Player + Camera + WorldManager
  -> World public facade
  -> ActionFeedbackTimeline

World (composition root)
  -> ChunkManager + ActorManager + PlayerActor + Gameplay runtimes
  -> SandboxEventBus + WorldSave + WorldBackup

World <-> Actor
World <-> Player
World -> Sandbox event protocol

Core / Entity / Physics / Maths / Util
  -> lower-level values and helpers used above
```

已验证的硬边界：

- `World/` 不包含 Ogre、SFML、OpenGL handle 或 GPU buffer 类型。
- Ogre 可以调用 World facade 并消费 immutable-by-value snapshots；World 不回调 Ogre。
- `World <-> Actor` 是当前真实双向协作：World 拥有 ActorManager，Actor tick 接收 `World&`。
- Sandbox 定义事件协议，但每个 `World` 实例实际拥有自己的 `SandboxEventBus`。
- Event handler 当前同步执行；事件是“已发生事实”，但代码尚未形成 A4 的 Command/Query/Event
  强制分层。订阅者不得假设异步或跨线程投递。
- `World` 仍是 God Object/Facade 混合体。AL-A1 可以基于本基线分类责任，但 A0 不新增 wrapper。

## 4. World Public API Surface

`World.h` 在 A0 时的公开入口按当前用途分组如下；这是责任审计，不是建议的新接口。

| Group | Current public surface |
| ----- | ---------------------- |
| 生命周期 | constructor/destructor；构造时加载/创建存档、预载、恢复 Actor/进度并可启动 loader。 |
| Block query/mutation | `getBlock`、`getSunlight`、`getBlockLight`、`setBlock`；block entity 的 get/create/update/remove/list。 |
| Tick/streaming/mesh | `tick`、`update`、render distance get/set、`resetChunkMeshes`、`updateChunk`、`preloadAround`、`startBackgroundLoader`。 |
| Persistence/observation | `save`、`getWorldTime`、`collectDebugStats`、`collectSectionMeshSnapshot`、mesh upload acknowledgement。 |
| Actor/combat | spawn item/mob、attack/damage/guard、combat budget/query、melee/projectile resolve、Actor/projectile snapshots。 |
| Player/food | food use、health/cooldown/spawn/reward snapshots。 |
| Progression | Alpha journey、objective、recipe discovery、outcome、difficulty、post-victory、Waystone state/action/feedback。 |
| Owned component access | `getChunkManager`、`getActorManager`、`getEventBus`、`getPlayer`。 |
| Pure/static helpers | coordinate conversion、random-tick sampling、natural-mob mapping、mesh work planning。 |
| Legacy queued action | `addEvent<T>` pushes `IWorldEvent` into a frame queue handled by `World::update`; it coexists with immediate `SandboxEventBus::publish`. |

### 4.1 AL-A1 machine-checked responsibility map

`AL-A1` 为每个公开方法分配两个正交标签：API concept 描述调用语义，responsibility 描述当前主要
实现领域。重载只列一次；完整声明、重载、公开常量和签名由 public-surface hash 共同保护。

<!-- AL-A1-WORLD-API-HASH sha256=3C53F56C425F0395354C8A5CE966E96CDA8BC93D836699955E03BA965A664AD8 -->
<!-- AL-A1-WORLD-API-MAP-BEGIN -->
| API | Concept | Responsibility | Current boundary |
| --- | ------- | -------------- | ---------------- |
| `~World` | `Command` | `World Mutation` | 终止 loader 并释放组合根。 |
| `acknowledgeSectionMeshUploads` | `Command` | `Streaming` | 仅确认同 revision 的 GPU upload。 |
| `addEvent` | `Command` | `World Mutation` | 把旧式 `IWorldEvent` 加入帧队列。 |
| `attackActor` | `Command` | `Combat` | 两个旧重载共享同一责任。 |
| `canOccupyCombatPosition` | `Query` | `Combat` | 战斗移动占位查询。 |
| `canPlayerGuard` | `Query` | `Combat` | 防御资格查询。 |
| `claimWaystoneReward` | `Command` | `Progression` | 提交一次性路标奖励。 |
| `collectActorSnapshots` | `Query` | `Actor` | 发布不可变 Actor render 值。 |
| `collectCombatProjectileSnapshots` | `Query` | `Combat` | 发布不可变 projectile render 值。 |
| `collectDebugStats` | `Query` | `Diagnostics` | 聚合只读运行时指标。 |
| `collectLoadedBlockEntityPositions` | `Query` | `World Query` | 查询已驻留 block entity。 |
| `collectSectionMeshSnapshot` | `Query` | `Streaming` | 发布 CPU-ready mesh snapshot。 |
| `consumeWaystoneFeedbackKey` | `Command` | `Progression` | 读取并清除一次性反馈。 |
| `createBlockEntity` | `Command` | `World Mutation` | 创建权威 block entity。 |
| `damagePlayer` | `Command` | `Combat` | 提交玩家伤害。 |
| `floorDiv` | `Query` | `World Query` | 纯坐标 helper。 |
| `floorMod` | `Query` | `World Query` | 纯坐标 helper。 |
| `getActorManager` | `Query` | `Actor` | 旧 mutable escape hatch；不新增同类入口。 |
| `getAlphaJourneySnapshot` | `Query` | `Progression` | 旧旅程兼容视图。 |
| `getAttackCooldownTicksRemaining` | `Query` | `Combat` | 战斗冷却查询。 |
| `getBlock` | `Query` | `World Query` | 方块查询；可能触发既有驻留读取路径。 |
| `getBlockEntity` | `Query` | `World Query` | block entity 值查询。 |
| `getBlockLight` | `Query` | `World Query` | 方块光查询。 |
| `getBlockXZ` | `Query` | `World Query` | 纯坐标 helper。 |
| `getChunkManager` | `Query` | `Streaming` | 旧 mutable escape hatch；A2 的迁移风险。 |
| `getChunkXZ` | `Query` | `World Query` | 纯坐标 helper。 |
| `getDifficultySnapshot` | `Query` | `Progression` | 难度权威状态快照。 |
| `getEventBus` | `Query` | `World Query` | 旧 mutable subscription/publish escape hatch。 |
| `getExplorationRewardSnapshot` | `Query` | `Progression` | 探索奖励能力快照。 |
| `getFoodCooldownTicksRemaining` | `Query` | `Progression` | 食物恢复冷却查询。 |
| `getObjectiveSnapshot` | `Query` | `Progression` | 当前目标只读快照。 |
| `getPlayer` | `Query` | `Actor` | 旧 non-owning mutable Player escape hatch。 |
| `getPlayerGuardRecoverDurationTicks` | `Query` | `Combat` | 防御恢复时长查询。 |
| `getPlayerHealth` | `Query` | `Combat` | PlayerActor 生命查询。 |
| `getPlayerMaxHealth` | `Query` | `Combat` | PlayerActor 最大生命查询。 |
| `getPlayerSpawnPoint` | `Query` | `Actor` | 玩家世界内出生点查询。 |
| `getPostVictoryEventSnapshot` | `Query` | `Progression` | 胜利后事件快照。 |
| `getRecipeDiscoverySnapshot` | `Query` | `Progression` | 配方发现快照。 |
| `getRenderDistance` | `Query` | `Streaming` | 当前流送/渲染半径查询。 |
| `getSunlight` | `Query` | `World Query` | 天光查询。 |
| `getWaystoneEncounterSnapshot` | `Query` | `Progression` | 路标遭遇快照。 |
| `getWorldOutcomeSnapshot` | `Query` | `Progression` | 结局权威状态快照。 |
| `getWorldTime` | `Query` | `World Query` | 当前世界时间查询。 |
| `initializeWaystone` | `Command` | `Progression` | 初始化路标持久状态。 |
| `isCombatTargetAvailable` | `Query` | `Combat` | 目标存活/可用性查询。 |
| `isNaturalMobType` | `Query` | `Actor` | 纯敌人类型 helper。 |
| `isPlayerGuarding` | `Query` | `Combat` | 当前防御状态查询。 |
| `isRecipeDiscovered` | `Query` | `Progression` | 配方发现查询。 |
| `launchMobProjectile` | `Command` | `Combat` | 创建有界 transient projectile。 |
| `naturalMobSpawnOffset` | `Query` | `Actor` | 确定性自然生物采样 helper。 |
| `naturalMobTypeForBiome` | `Query` | `Actor` | biome 到敌人类型的纯映射。 |
| `onWaystoneBroken` | `Command` | `Progression` | 提交路标破坏后状态转换。 |
| `planChunkMeshWork` | `Query` | `Streaming` | 纯 mesh work 排序 helper。 |
| `preloadAround` | `Command` | `Streaming` | 同步预载指定位置周围区块。 |
| `publishCombatWindup` | `Command` | `Combat` | 发布已提交的攻击前摇事实。 |
| `randomTickBlockIndex` | `Query` | `Simulation` | 确定性 random-tick 采样 helper。 |
| `removeBlockEntity` | `Command` | `World Mutation` | 移除并返回权威 block entity。 |
| `requestDifficulty` | `Command` | `Progression` | 排队下个 fixed tick 的难度提交。 |
| `resetChunkMeshes` | `Command` | `Streaming` | 使派生 mesh 失效并重建。 |
| `resolveMobMeleeAttack` | `Command` | `Combat` | 提交近战命中/防御结果。 |
| `save` | `Command` | `Persistence` | 发布 chunk、metadata 与备份事务。 |
| `scaleDifficultyLootAmount` | `Query` | `Progression` | 纯难度掉落比例查询。 |
| `setBlock` | `Command` | `World Mutation` | 提交方块、光照、mesh dirty 与事件。 |
| `setPlayerGuarding` | `Command` | `Combat` | 提交当前防御请求。 |
| `setRenderDistance` | `Command` | `Streaming` | 更新 bounded streaming demand 半径。 |
| `spawnItemEntity` | `Command` | `Actor` | 创建权威掉落 Actor。 |
| `spawnMob` | `Command` | `Actor` | 创建权威 Mob Actor。 |
| `startBackgroundLoader` | `Command` | `Streaming` | 启动当前单 loader worker。 |
| `tick` | `Runtime Tick` | `Simulation` | 20 Hz 权威 Gameplay tick。 |
| `toBlockCoord` | `Query` | `World Query` | 纯坐标 helper。 |
| `tryAttackActor` | `Command` | `Combat` | 带完整拒绝原因的玩家攻击提交。 |
| `tryConsumeCombatChaseStep` | `Command` | `Combat` | 消耗当 tick 的有界 chase budget。 |
| `update` | `Runtime Tick` | `Simulation` | 每帧命令、卸载与同步 mesh budget 编排。 |
| `updateBlockEntity` | `Command` | `World Mutation` | 原子替换 block entity payload。 |
| `updateChunk` | `Command` | `Streaming` | 把受影响 section 加入现有 mesh queue。 |
| `useHeldFood` | `Command` | `Progression` | 提交库存消耗与生命恢复。 |
| `useWaystone` | `Command` | `Progression` | 提交路标状态机动作。 |
| `World` | `Command` | `World Mutation` | 创建/恢复 World 组合根并可启动 loader。 |
<!-- AL-A1-WORLD-API-MAP-END -->

概念规则：

- `Query` 返回观察值，不允许提交新的 Gameplay 结果；既有 lazy residency/cache side effect 必须在
  表中说明，不能借“查询”隐藏新的权威 mutation。
- `Command` 可以提交状态，但必须保留现有拒绝、原子性、事件和保存语义。
- `Runtime Tick` 只用于时间推进与有界工作编排；新增子系统不得再给 `World` 增加平行 tick 入口。
- `getChunkManager/getActorManager/getEventBus/getPlayer` 是兼容性 escape hatch，不是新 API 的范例。
- 任何 `World.h` public surface 变化必须同步更新本表及 hash，并通过
  `tools/validate_world_responsibility_map.ps1`；该检查已进入完整 Windows 门禁。

当前调用关系把边界进一步钉死：`SandboxRuntime/WorldManager` 驱动 `tick/update` 和玩家命令，
`OgreBootstrap` 消费 mesh/Actor/diagnostic snapshot 并确认 upload，Actor/Block/Interaction 代码通过
Combat、Actor、World Mutation 与 EventBus 入口协作。旧调用在 A1 不迁移。

该表解释了 AL-A1 的真实动机：查询、命令、模拟、流送、持久化、Actor、战斗、进度和诊断目前
都暴露在一个 facade 中。A1 只冻结责任与新增入口规则，不改变旧调用者或兼容性。

## 5. World Member Responsibility Map

| Member group | Members | Current responsibility |
| ------------ | ------- | ---------------------- |
| Core composition | `m_chunkManager`, `m_actorManager`, `m_playerActor`, `m_eventBus`, `m_player` | 世界拥有 Chunk/Actor/Event 生命周期；SandboxRuntime 拥有 Player，World 保存 non-owning pointer 并维护战斗 Actor 镜像。 |
| Persistence | `m_worldSave`, `m_worldBackup`, `m_worldSaveData`, save counters/timings | 元数据读写、整世界备份、当前保存 payload 与可观察耗时。 |
| Progression | `m_alphaJourney`, `m_victoryFlow`, Waystone anchor/state/guardian ids/cooldown/feedback | 目标兼容视图、结局、遭遇和胜利后状态。 |
| Frame command/event queue | `m_events` | `PlayerDigEvent` 等延迟到 `World::update` 处理；区别于同步 typed event bus。 |
| Mesh/random-tick work | chunk update deque/set、random-tick deque/set/counters | 主线程同步 mesh 重建预算与 active random-tick section 调度。 |
| Population/difficulty | spawn counters、pending difficulty、application epoch | 自然生物预算和下个 fixed tick 的难度提交。 |
| Loader ownership | `m_isRunning`, `m_chunkLoadThreads`, main/gen/priority mutexes | 一个后台 chunk loader 的生命周期和共享状态保护；`m_genMutex` 仍是当前成员。 |
| Streaming demand snapshot | load center/revisions/distances、unload scan、frustum priority snapshot | 从 Camera 派生的当前加载中心、优先级 epoch 和有界卸载扫描状态。 |
| Player combat runtime | spawn point、cooldowns、projectile vector/id/counters、guard/feedback/respawn state | 固定 tick 的当前战斗真值；projectile render snapshot 从这里派生。 |

## 6. ChunkManager Boundary

`World` 按值拥有一个 `ChunkManager`；`ChunkManager` 保存 non-owning `World*` 以调用光照协调和发布
事件。其当前职责为：

- 拥有 `unordered_map<VectorXZ, Chunk>`、`TerrainGenerator` 与 `ChunkStorage`；
- 按 seed / terrain v4 / exploration reward v1 冻结生成身份；
- 查询、创建、加载、生成、保存和卸载 Chunk；
- 在卸载前同步保存 dirty Chunk，失败时保留 resident Chunk；
- 发布 generated/loaded/saved/unloaded 事实；
- 提供 `beginMeshJob -> off-lock build -> finishMeshJob` 边界；
- 在 `finishMeshJob` 用 section block revision 拒绝 stale CPU mesh；
- 汇总 Chunk、保存事务、mesh build、face/vertex 等 debug metrics。

当前 Chunk 的 data residency、mesh state 和 Ogre render residency 尚未被 B1 拆成三套正式状态机。
`ChunkSectionMeshState` 与 `hasLoaded/needsSave` 是现有局部状态，不能在 A0 报告成 B1 已实现。

## 7. Thread Ownership

| Thread | Created/owned by | May do | Commit/stop rule |
| ------ | ---------------- | ------ | ---------------- |
| Main/Ogre thread | `OgreBootstrap` frame loop | OS input、application flow、Sandbox fixed ticks、World mutation、同步 event handler、卸载/同步 mesh budget、snapshot 采集、GPU upload、UI/audio update。 | Gameplay truth 默认只在这里变更；frame end 记录性能。 |
| Chunk loader worker | `World::startBackgroundLoader` / `m_chunkLoadThreads` | 本地规划队列；在 `m_mainMutex` 下 begin/load/snapshot，锁外构建 `SectionMeshInput`，再在锁下 finish。 | 由 `m_isRunning=false` 停止，World 析构 join；revision 不匹配的 mesh 不提交。当前只有一个 worker。 |
| Music stream worker | Windows `MusicRuntime` backend | 从已验证 WAV 分段读取、增益缩放、维护最多 3 个 WaveOut buffer。 | 主线程用 atomic/mutex/condition variable 控制；stop/reset 后 join；不读写 World。 |
| Crash writer thread | `WindowsCrashDiagnostics` install | 预创建后等待 crash event，在异常路径写 dump/sidecar。 | 不参与普通 Gameplay；进程退出终止，普通路径无上传。 |

除上述线程与测试专用线程外，当前没有通用 World Job Scheduler。后台 loader 不能被描述成 B3 已完成。

## 8. Persistence Ownership

```text
WorldManager
  owns live World objects and invokes World::save()
        |
        +-> ChunkManager::saveDirtyChunks()
        |      +-> ChunkStorage
        |             +-> StorageTransaction
        |
        +-> World::saveWorldState()
        |      +-> WorldSave
        |             +-> StorageTransaction
        |
        +-> WorldBackup::createBackup()
```

- `WorldSaveData` 是内存中的当前 metadata payload，写出前由 World 收集 Player、Actor、目标、结局、
  难度、terrain identity 和其他版本化状态。
- world save format 当前为 v12；terrain generation 为独立 v4；settings 是独立 v8。
- `StorageTransaction` 负责同目录 candidate、flush、真实 reader 校验和原子替换；失败 candidate 不
  成为权威。
- Chunk 只有成功发布后才清 save-dirty；unload 保存失败则取消卸载。
- `WorldBackup` 在 metadata/chunk 发布之后创建有界且可验证的整世界快照。
- 可稳定重建的 sunlight、block light、mesh、render nodes、storage/diagnostic caches 不作为独立
  Gameplay truth 保存。

## 9. Event Ownership

存在两条不同路径：

1. `World::addEvent<IWorldEvent>` 是帧内命令队列，目前主要承载 dig/use/place；
   `World::update` 在主线程依次执行并清空。
2. `SandboxEventBus::publish` 是同步领域事实分发。每个 World 拥有 bus；World、ChunkManager、
   Actor、Item/interaction 发布事实，Objective/AlphaJourney、Feedback、Audio 和 Waystone handler
   订阅。

EventBus 不拥有事件历史或 Gameplay 状态，handler 在发布调用栈内执行。当前代码允许 handler 触发
进一步操作，因此递归/隐式 mutation 仍需由 AL-A4 单独冻结规则；A0 只记录现状。

## 10. Main Tick and Frame Chain

```text
Ogre::frameStarted
  -> pump OS messages / capture OIS input / process UI action
  -> OgreBootstrap::updateSandbox(dt)
       -> application-flow pause/loading gate
       -> build platform-independent SandboxInputState
       -> SandboxRuntime::update(input, dt)
            -> update ActionFeedbackTimeline
            -> Player::applyInput
            -> FixedTickScheduler::advance (20 Hz, bounded catch-up)
                 for each emitted tick:
                   -> Player::update(1/20, World)
                   -> WorldManager::tick
                        -> ++worldTime
                        -> World::tick(worldTime)
                             -> apply pending difficulty
                             -> player/actor tick
                             -> projectiles + Waystone reconciliation
                             -> random ticks + natural population
                             -> loaded furnace tick
                             -> AlphaJourney update + respawn
            -> update Camera from interpolated Player
            -> selection + one resolved GameplayWorldAction
            -> queue dig/use/place or perform food/combat path
            -> World::update(Camera)
                 -> publish load center/frustum snapshot
                 -> handle queued IWorldEvent commands
                 -> bounded distant unload
                 -> bounded synchronous dirty-section mesh rebuild
       -> sync render camera / section meshes / actor visuals / outline
  -> update AudioRuntime and MusicRuntime
  -> collect debug stats / UI frame
Ogre::frameRenderingQueued
Ogre::frameEnded -> capture + performance record + exit/crash gates
```

暂停或非 Playing 状态由 `GameApplicationFlow` 阻止 Sandbox simulation；渲染、UI 和必要的应用处理
仍可继续。fixed tick 顺序是当前 Gameplay contract，但尚未被 D1 拆成可预算 phase scheduler。

## 11. Render Snapshot Chain

```text
Authoritative World state
  -> chunk worker builds CPU mesh from SectionMeshInput snapshot
  -> ChunkSection CpuReady + blockRevision
  -> World::collectSectionMeshSnapshot() under m_mainMutex
  -> OgreBootstrap::syncSectionMeshes()
  -> ChunkSectionRenderable GPU buffers
  -> acknowledgeSectionMeshUploads(location, revision)
  -> markGpuBuffered only when section is still CpuReady at same revision

ActorManager / World projectile state
  -> collectActorSnapshots() / collectCombatProjectileSnapshots()
  -> OgreActorRenderer::sync / syncProjectiles
  -> Ogre scene nodes

WorldDebugStats + Gameplay/Feedback snapshots
  -> OgreUserInterface / RuntimePerformanceCapture
```

Snapshots are copied values and Ogre owns only their visual mirrors. A removed live section destroys its Ogre visual；
stale CPU upload acknowledgement cannot promote a newer revision。Renderer reset/rebuild therefore does not mutate
block、Actor、inventory、objective or persistence truth。

## 12. Frozen Version and Boundary Facts

| Identity | A0 value |
| -------- | -------- |
| world save format | v12 |
| terrain generation | v4 |
| runtime settings | v8 |
| objective definitions | v3 |
| enemy definitions | v3 |
| exploration reward | v1 |
| difficulty profile | v1 |
| post-victory event | v1 |
| audio definitions | v3 |
| music definitions | v1 |

这些版本属于不同兼容性域，不能用 world save v12 推断其他定义已迁移，也不能因重建派生数据而
静默改写 terrain identity。任何后续 Architecture Lab 批次都必须在自己的合同中列出受影响域。

## 13. A0 Conclusions

- 当前可玩的系统已经有清晰的 Renderer-to-Snapshot 边界和可验证持久化边界。
- 最大架构债是 `World` 同时承担 facade、组合根和多系统实现，而不是“缺少更多抽象类”。
- 当前后台 Chunk pipeline 已具备 snapshot/off-lock/revision-commit 模式，但没有通用取消、背压或
  三态 Residency 合同。
- AL-A1 只有在单独批准后才能基于本表做 World Responsibility Map；AL-A2/B1 仍在更后的独立门。
