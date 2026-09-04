# HelloMine3D Current Architecture Baseline

本文以 `AL-A0 — Latest Architecture Baseline` 的完整审计为起点，并随已完成的
AL-A1/AL-A2/AL-A3/AL-A4/AL-A5/AL-A6/B1/B2/B3/B4/B5/B6/B10/C1/C2/C3 更新当前实现；它描述代码事实，而不是未来目标架构。
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

下表覆盖当前 `src/HelloMine3D/` 的全部 18 个顶层目录，以及 5 个根级配置/输入源文件。既有行的
`Size at A0` 保留审计快照；C3 新增模块标记其引入时规模。“权威/派生”描述运行时所有权，
不表示每个模块只能包含一种数据。

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
| `Item/` | 21 / 3,866 | Material/ItemStack、库存/容器、配方/制作、工具、食物、冶炼、C2 machine process 定义和资源经济校验。 | 冻结注册表与 Inventory/Container 内容为各自域的权威值；预览、process observation 和统计为派生。 | 主要依赖 Util，少数交互边界依赖 World；被 Player/World/Gameplay/UI 消费。 |
| `Mechanical/` | 2 / 305 (C3) | C2 Crusher 的六面相邻节点、连接、component、merge/split 和 copied topology snapshot。 | 当前已加载 Crusher 方块/严格 payload 是权威输入；network id、component、edge 与统计全部可重建且不持久化。 | 只依赖 Maths 值；由 World 在既有锁内同步，Block capability/UI 只消费 copied snapshot；不依赖 Ogre、Storage 或 C4 power。 |
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
  -> ChunkRuntime -> ChunkManager
  -> WorldSimulation -> existing World/Actor/Gameplay implementations
  -> MechanicalTopology (derived loaded-Crusher connectivity)
  -> ActorManager + PlayerActor + Gameplay runtimes
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
- Event handler 当前同步执行；AL-A4 已把请求 mutation 的 typed Command、已发生的 immutable Event
  与不提交 Gameplay 的 Query 分开。订阅者不得假设异步或跨线程投递。
- `World` 仍是宽 facade/组合根；AL-A2 把现有 Chunk 派生工作与 loader 协调移入
  `ChunkRuntime`，B1 又在不改变该边界的前提下加入三套正交生命周期，B2/B3/B4/B5/B6 分别加入
  bounded demand、typed background work、generation cancellation、streaming pressure control 与
  spatial interest；B10 用同一生产路径完成长稳收口并修正被压力场景暴露的卸载饥饿，仍不改变
  World facade。
- AL-A3 已把 `World::tick` 的现有调用顺序集中到具体 `WorldSimulation`；AL-A5 在同一 last-tick
  snapshot 上增加四条真实 processed/deferred/budget 观察。两者都不拥有玩法状态，也不是通用 scheduler。
- C1 把 Chest/Furnace 的能力声明附着到既有 `BlockDefinition`；C2 以可玩的 Crusher 成为第二个
  Processor 后，把真实声明扩展为 Chest/Furnace/Crusher，并从 Furnace/Crusher 提炼 Ogre-free、
  persistence-free 的 `MachineRuntime` 五态与单 tick 转换。`BlockCapabilityAccess` 只复制观察值并把
  命令委托给具体容器；库存、燃料/手摇动力、payload 和完成副作用仍归具体 owner，当前没有第二套
  Capability/recipe Registry。C3 又只为真实 Crusher 声明六面 `MechanicalPort`，并从当前已加载
  block/entity truth 同步派生具体 topology；它不持久化 component id，也不传播动力。

### 3.1 C1/C2/C3 capability, machine and topology path

```text
BlockDatabase -> BlockDefinition.capabilities
                         |
loaded block + matching block-entity record
                         v
               BlockCapabilityAccess
                  /              \
       InventoryProvider     MachineProcessor       MechanicalPort
      Chest/Furnace/Crusher    Furnace/Crusher
                  \              /
                  \              |                 /
                   +------- Ogre container UI -----+

WorldSimulation::BlockEntitySimulation
          |                         |
          v                         v
  FurnaceContainer           CrusherContainer
  fuel/light/event owner     crank/two-slot owner
          \                         /
           +----> MachineRuntime <+
             pure five-state transition

World block/entity mutation + successful Chunk load/unload
                         |
                         v
                MechanicalTopology
          six-face BFS merge/split rebuild
                         |
                         v
            copied node/component snapshot
```

Chest exposes nine general insert/extract slots with automatic insertion.
Furnace exposes input/fuel/output roles; Crusher exposes insert/extract input,
extract-only output and a bounded manual-crank command. Processor views copy
derived status, recipe, progress and power values. Each handle rechecks current
identity and existing payload validation before use, so block replacement, a
mismatched record or a malformed payload fails closed.

`MachineRuntime` matches a concrete copied recipe, checks output capacity and
power, then advances at most one fixed tick or completes atomically. Its states
are `Idle / MissingInput / BlockedOutput / NoPower / Running`; no state or
recipe id is persisted. Furnace owns its existing fuel, lighting, event and v1
payload semantics, while Crusher owns crank admission and Crusher payload v1.
`MechanicalPort` 只由 Crusher 声明。它再次核对 live block/entity identity，再读取 World 锁内的
copied topology snapshot；缺失、错配、损坏或已卸载状态 fail closed。C3 的 network id 是 component
中 X/Y/Z 字典序最小位置，未序列化；每台 Crusher 仍独立手摇，连接不影响 C2 processing。

## 4. World Public API Surface

`World.h` 在 A0 时的公开入口按当前用途分组如下；这是责任审计，不是建议的新接口。

| Group | Current public surface |
| ----- | ---------------------- |
| 生命周期 | constructor/destructor；构造时加载/创建存档、预载、恢复 Actor/进度并可启动 loader。 |
| Block query/mutation | `getBlock`、`getSunlight`、`getBlockLight`、`setBlock`；block entity 的 get/create/update/remove/list；`getMechanicalNodeSnapshot` 返回派生连接快照。 |
| Tick/streaming/mesh | `tick`、`update`、render distance get/set、`resetChunkMeshes`、`updateChunk`、`preloadAround`、`startBackgroundLoader`。 |
| Persistence/observation | `save`、`getWorldTime`、`collectDebugStats`、`collectSectionMeshSnapshot`、mesh upload acknowledgement。 |
| Actor/combat | spawn item/mob、attack/damage/guard、combat budget/query、melee/projectile resolve、Actor/projectile snapshots。 |
| Player/food | food use、health/cooldown/spawn/reward snapshots。 |
| Progression | Alpha journey、objective、recipe discovery、outcome、difficulty、post-victory、Waystone state/action/feedback。 |
| Owned component access | `getChunkManager`、`getActorManager`、`getEventBus`、`getPlayer`。 |
| Pure/static helpers | coordinate conversion、random-tick sampling、natural-mob mapping、mesh work planning。 |
| Command FIFO | `addCommand<T>` accepts an `IWorldCommand` and executes it from the frame-owned FIFO in `World::update`; completed mutations may synchronously publish immutable facts through `SandboxEventBus`. |

### 4.1 AL-A1 machine-checked responsibility map

`AL-A1` 为每个公开方法分配两个正交标签：API concept 描述调用语义，responsibility 描述当前主要
实现领域。重载只列一次；完整声明、重载、公开常量和签名由 public-surface hash 共同保护。

<!-- AL-A1-WORLD-API-HASH sha256=D6D45DAC48E25A0FE19DFF375C8A7E4AAFFC96B06CD23A41E530482FBFB89B54 -->
<!-- AL-A1-WORLD-API-MAP-BEGIN -->
| API | Concept | Responsibility | Current boundary |
| --- | ------- | -------------- | ---------------- |
| `~World` | `Command` | `World Mutation` | 终止 loader 并释放组合根。 |
| `acknowledgeSectionMeshUploads` | `Command` | `Streaming` | 仅确认同 revision 的 GPU upload。 |
| `addCommand` | `Command` | `World Mutation` | 把 typed `IWorldCommand` 加入 frame-owned FIFO；执行后才可发布已发生事实。 |
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
| `getMechanicalNodeSnapshot` | `Query` | `World Query` | 返回指定已加载 Crusher 的派生机械 component 值快照。 |
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
Combat、Actor、World Mutation 与 EventBus 入口协作。AL-A2/AL-A3 都保持当时的 78 项公开面不变；
C3 为正常 capability 观察新增 `getMechanicalNodeSnapshot`，当前为 79 项：Streaming 方法内部转发给
`ChunkRuntime`，20 Hz `World::tick(int)` 内部转发给 `WorldSimulation::fixedTick`。

该表解释了 AL-A1 的真实动机：查询、命令、模拟、流送、持久化、Actor、战斗、进度和诊断目前
都暴露在一个 facade 中。A1 只冻结责任与新增入口规则，不改变旧调用者或兼容性。

## 5. World Member Responsibility Map

| Member group | Members | Current responsibility |
| ------------ | ------- | ---------------------- |
| Core composition | `m_mechanicalTopology`, `m_chunkManager`, `m_chunkRuntime`, `m_worldSimulation`, `m_actorManager`, `m_playerActor`, `m_eventBus`, `m_player` | World 按值拥有 Chunk 权威存储、C3 派生机械连通性、Chunk 派生工作协调、fixed-tick 编排和 Actor/Event 生命周期；SandboxRuntime 拥有 Player，World 保存 non-owning pointer 并维护战斗 Actor 镜像。 |
| Persistence | `m_worldSave`, `m_worldBackup`, `m_worldSaveData`, save counters/timings | 元数据读写、整世界备份、当前保存 payload 与可观察耗时。 |
| Progression | `m_alphaJourney`, `m_victoryFlow`, Waystone anchor/state/guardian ids/cooldown/feedback | 目标兼容视图、结局、遭遇和胜利后状态。 |
| Frame command/event queue | `m_events` | `PlayerDigEvent` 等延迟到 `World::update` 处理；区别于同步 typed event bus。 |
| Random-tick work | random-tick deque/set/counters | World 仍负责 active random-tick section 调度；Chunk mesh update queue 已迁入 `ChunkRuntime`。 |
| Population/difficulty | spawn counters、pending difficulty、application epoch | 自然生物预算和下个 fixed tick 的难度提交。 |
| Chunk runtime coordination | `m_chunkRuntime` 内的 update deque/set、loader thread、demand model、`WorldJobScheduler`、active plan cursor、revisions、pressure/consumer counters、unload scan、frustum priority snapshot | 一个后台 Chunk worker、typed load/mesh work、bounded admission/refill/commit/upload/unload、预载和 Camera 派生需求；这些都是派生/协调状态。 |
| Shared Chunk lock | `m_mainMutex` | 仍由 World 拥有并先于 `ChunkRuntime` 构造；Runtime 只持 non-owning 引用，保护 loaded Chunk/section 权威状态。 |
| Player combat runtime | spawn point、cooldowns、projectile vector/id/counters、guard/feedback/respawn state | 固定 tick 的当前战斗真值；projectile render snapshot 从这里派生。 |
| Simulation orchestration | `m_worldSimulation` | 按冻结的 8 phase 顺序调用现有实现，并保存最近一次 tick 的非持久化原始耗时；不拥有 gameplay truth、pause state 或预算。 |

## 6. ChunkRuntime and ChunkManager Boundary

```text
World (public facade / composition root)
  -> ChunkRuntime (derived work and coordination)
       -> ChunkManager (authoritative Chunk/storage owner)
```

`ChunkRuntime` 持有 `ChunkManager` 和 `m_mainMutex` 的 non-owning 引用，并拥有：

- deduplicated FIFO section update queue；
- 每次 `World::update` 最多 2 个 section 的同步 mesh rebuild；
- 单个 loader worker、完整 mesh target 规划、load center/frustum priority revision；
- B2 四槽 demand model、B3 typed pending/in-flight/completed job scheduler、B4 generation
  token/linearized commit boundary、B5 admission/pressure/window-refill policy，以及 B6 copied
  spatial-interest snapshot；
- 现有 preload、render-distance invalidation、每 update 最多 8 个成功 distant unload 与 truthful backlog；
- 每帧至多 8 个、以 Player demand 原点确定性近优先的 CPU-ready mesh copied snapshot、所有 live
  section revision 与同 revision GPU acknowledgement；
- 每 loader pass 至多 8 个 authoritative commit interval，以及不持久化的 copied pressure diagnostics。

这些对象是派生数据或工作协调，不拥有 block/light/save truth。AL-A2 至 B10 保持当时的 78 项公开面；
C3 为 copied topology observation 增至 79 项并同步更新 machine-checked hash；
`World::planChunkMeshWork` 仍只转发到纯 `ChunkRuntime::planMeshWork`。

`World` 按值拥有一个 `ChunkManager`；`ChunkManager` 保存 non-owning `World*` 以调用光照协调和发布
事件。其当前职责为：

- 拥有 `unordered_map<VectorXZ, Chunk>`、`TerrainGenerator` 与 `ChunkStorage`；
- 按 seed / terrain v4 / exploration reward v1 冻结生成身份；
- 查询、创建、加载、生成、保存和卸载 Chunk；
- 在卸载前同步保存 dirty Chunk，失败时保留 resident Chunk；
- 发布 generated/loaded/saved/unloaded 事实；
- 提供 `beginMeshJob -> off-lock build -> finishMeshJob` 和 B4 detached
  `begin/prepare/finish/cancel ChunkLoadJob` 边界；
- cancelled detached reservation 转成语义 `Absent` 后立即从 manager map 擦除，不保留无界坐标墓碑；
- 在 `finishMeshJob` 用 section block revision 拒绝 stale CPU mesh；
- 汇总 Chunk、保存事务、mesh build、face/vertex 等 debug metrics。

B1 在 `ChunkLifecycle.*` 冻结三套独立词汇和合法转换：

- `Chunk` 拥有 Data Residency：`Absent -> Requested -> Loading ->
  Generating/Resident`，以及 `Resident/EvictRequested/Saving` 的保存与驱逐闭环；B4 只增加
  cancelled reservation 使用的 `Loading -> Absent`；
- `ChunkSection` 拥有 CPU Mesh：`Clean/Dirty/Queued/Building/CpuReady`；编辑可把
  任一有效派生状态重新置为 `Dirty`，stale off-lock result 不能离开 `Dirty`；
- Ogre 以 section key 拥有 Render Residency：`NotResident/UploadPending/GpuResident/Stale`，
  World 只发布 copied snapshot 和 revision acknowledgement，不拥有 GPU 状态。

`ChunkManager` 在 load/generate/save/unload 真实调用周围推进 Data 状态；dirty eviction 的保存失败
返回 `Resident` 且保留 dirty。`ChunkRuntime` 保持 AL-A2 的单 worker；B3 只替换 worker 内部的隐式
坐标 deque，不影响主线程 deduplicated section update FIFO。
开发者面板分别显示 7/5/4 状态计数；`Absent` 是转换语义而非常驻 manager 对象，不虚构或保留
无限坐标集合。

B2 在不改变上述状态所有权的前提下，用 `ChunkDemandModel` 取代单个隐式 load center。模型由
`ChunkRuntime` 拥有，并以每个 reason 一个槽位保存 `Player / Camera / TeleportDestination /
Preload` 的坐标、priority、epoch、expiry 和 radius。每次 `World::update` 推进一次 demand epoch 并
刷新 Player/Camera；既有同步 preload 同时发布 Preload，成功的同世界 teleport 才通过私有 bridge
发布 TeleportDestination。Demand 是不持久化的派生 runtime input，不进入 `ChunkManager` 或 B1 状态机。

Loader 在每轮规划时展开四个有界半径，合并重复坐标及 reason bits，并按 reason priority、frustum、
最近 Player Chunk 移动方向、newest epoch、distance 和稳定坐标排序。Demand semantic revision 会在
B4 推进 generation；仅 camera/frustum priority 重排仍只替换 pending plan，不使 in-flight 工作失效。
这些变化没有增加 worker。开发者快照公开 epoch/revision、active/reason/expired 数和最近
de-duplicated plan size。

B3 将该 plan 转换为两个且仅两个真实 job type：`ChunkLoadOrGenerate` 和 `ChunkMeshBuild`。B4 未拆分
这两个身份，只把前者实现为 `beginChunkNeighborhoodLoadJob -> off-lock storage/generation ->
finish/cancelChunkLoadJob`，并继续让后者执行 `beginMeshJob -> off-lock ChunkMeshBuilder ->
finish/cancelMeshJob`。每个执行仍最多准备一个 Chunk，Chunk/storage 所有权不进入 scheduler。

`WorldJobScheduler` 由 `ChunkRuntime` 按值拥有，保存 deterministic pending vector、一个 optional
in-flight job、completion deque、monotonic id、从 1 开始的 uint64 generation 和 copied diagnostics。
唯一状态流仍为 `Pending -> InFlight -> Completed`；排序仍为 B2 priority、plan order、newest demand
epoch、type、id。Semantic plan 失效清除旧 pending，in-flight 保留到以 `Cancelled` 或已先完成的有效
结果结束。Completion 记录 `DidWork/NoWork/CommitRejected/Cancelled` 与 queue/worker/commit 时间。

B4 的 generation/commit mutex 在线程持有共享 World mutex 时把最终 token 检查与 authoritative commit
线性化。Detached candidate 关闭 live random-tick index 通知；成功提交后才一次性注册 active section、
协调光照和发布 Chunk 事实。共享 terrain generator 的生成入口被串行化。Generation rejection 与 B1
revision rejection 分别记为 `Cancelled` 和 `CommitRejected`。

B5 为 pending vector 冻结 shared/type `128/128/128` hard cap、`96/48` high/low watermarks 和
`Normal/Elevated/Saturated` hysteresis。`Accepted/AcceptedAfterShedding/Duplicate/StaleGeneration/
RejectedAtCapacity` 使 admission 结果显式；hard cap 处只允许新 job 按既有 B3 order 严格领先时替换
一个确定性的最差 pending，永不淘汰 in-flight。`ChunkRuntime` 在 loader 栈中保留当前 generation 的
immutable B2-derived request vector 与 monotonic cursor，先发布最高 96 项，pending 降到 48 后补到
96；这份 vector 不携带 id/state，不是第二条 job queue，generation 失效即清空。

Scheduler 和 active plan 都不拥有 Gameplay、Chunk、mesh 或 persistence truth。

B6 在 `World/Streaming/SpatialInterest.*` 中把 immutable B2 snapshot 纯派生为按 x/z 排序的 cell：
`requestsSimulation => requiresNearRepresentation => requiresResidentData`。Player/Camera 在各自 B2
半径请求 Resident + Near，Player 的 Chebyshev 2 Chunk 邻域额外请求 Simulation；TeleportDestination/
Preload 只请求 Resident。重叠来源按 OR 合并并保留 reason mask，缺席坐标为 `Outside`。snapshot
只在 demand semantic revision 变化时重建并由 `ChunkRuntime` 在既有 demand mutex 下拥有，不持久化。

B3 plan 只把 Resident cell 转成 job；resident-only target 不进入 mesh follow-up。CPU mesh copied
snapshot 与 upload acknowledgement 复核当前 Near interest，已离开 Near 的 Ogre section 由既有 live
reconciliation 移除。Distant unload 先保护 Resident interest，再执行 B5 的八项上限。Simulation
Requested 只进入 copied diagnostics，当前 Actor/combat/crop/furnace/random tick/population/Gameplay
路径均不消费。B6 没有 Far 字段、额外 worker、未来系统空 job type、D2 fidelity 或通用 Simulation
Scheduler。

B10 不增加新的运行时层。它用 `HelloMine3DSoak` schedule v3 在同一 Release 进程和存档根上依次
驱动直线长途、远距传送、折返、视距变化、编辑离开与保存重开，并持续消费真实 `World::update`、
loader、mesh snapshot/acknowledgement 和 persistence 路径。首次 30 分钟运行暴露一个组合缺陷：B4
取消将 reservation 留作 `Absent` map tombstone，而 B5 unload 在验证 Data Residency 之前先截取八个
坐标；稳定的哈希迭代顺序可让不可卸载 tombstone/Loading entry 永久占满预算，使 Resident Chunk
饥饿。当前边界在取消后擦除 tombstone，并在预算选择前过滤 `Resident`；`lastUnloads` 只计成功移除，
失败的 eligible removal 保持 backlog。修复仍复用 B1 状态机、B5 八项预算和同步 dirty save 语义。
同一长跑还让 `ChunkSection` 的同步 mesh-neighbour Query 改为 `findAdjacent`：缺席邻区按非全实心
边界读取，而不再通过 `getOrCreateChunk` 产生 `Absent` 条目。读取派生 mesh input 因此不扩大权威
Chunk map。第二次正式尝试证明 `Absent=0` 且最大仅 223 个 Chunk，但又暴露 LW5 将 300 秒短测的
十次保存重开按五秒 cadence 放大为六十次，导致备份复制主导墙钟。schedule v3 因此把 LW5
持久化覆盖冻结为十次，不改变 180 秒 shutdown grace 或任何 runtime/Q3 阈值。最终 Release Core
通过 1800 秒/36000 ticks：最大 216 个 Chunk、最大 pending 95、consumer 8/8/8、最大/最终
`Absent=0`，峰值 private 137551872 bytes；双确定性探针一致。

## 7. Thread Ownership

| Thread | Created/owned by | May do | Commit/stop rule |
| ------ | ---------------- | ------ | ---------------- |
| Main/Ogre thread | `OgreBootstrap` frame loop | OS input、application flow、Sandbox fixed ticks、World mutation、同步 event handler、每 update 至多 8 个卸载、同步 mesh budget、每帧至多 8 个 CPU-ready upload、UI/audio update。 | Gameplay truth 默认只在这里变更；未选择的 `CpuReady` 和 unload backlog 留到后续帧，frame end 记录性能。 |
| Chunk loader worker | `World::startBackgroundLoader` delegates to `ChunkRuntime` | 消费 B3 typed job；B5 在 96/48 窗口内 refill；B6 只让 Resident cell 入 plan，并让 resident-only target 停在数据层；锁内预留/snapshot，锁外 storage/generation/mesh build，再在线性化 generation boundary 内提交或取消。 | 每 pass 至多 8 个 authoritative commit interval；`World` 析构先使 generation 失效、停止并 join 后才保存。旧 generation 为 `Cancelled`，同代 revision 不匹配为 `CommitRejected`。当前只有一个 worker。 |
| Music stream worker | Windows `MusicRuntime` backend | 从已验证 WAV 分段读取、增益缩放、维护最多 3 个 WaveOut buffer。 | 主线程用 atomic/mutex/condition variable 控制；stop/reset 后 join；不读写 World。 |
| Crash writer thread | `WindowsCrashDiagnostics` install | 预创建后等待 crash event，在异常路径写 dump/sidecar。 | 不参与普通 Gameplay；进程退出终止，普通路径无上传。 |

除上述线程与测试专用线程外，当前没有额外 worker pool。B3/B4/B5 scheduler 只协调既有 Chunk
load/generate 与 CPU mesh work、协作取消及其 pressure admission；B6 spatial snapshot 只过滤已有
工作和表示消费者。它不是通用 Simulation Scheduler、主线程 completion dispatcher、Far renderer
或 simulation-fidelity controller。

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
- Chunk block-entity v2 已能保存经过具体 owner 验证的 type/payload；C2 因此直接保存 Crusher payload v1
  的输入、输出、进度和剩余手摇动力，不改变 world save v12，也没有 offline catch-up。
- `WorldBackup` 在 metadata/chunk 发布之后创建有界且可验证的整世界快照。
- 可稳定重建的 sunlight、block light、mesh、render nodes、storage/diagnostic caches 不作为独立
  Gameplay truth 保存。

## 9. Event Ownership

AL-A4 把原有两条路径明确分开：

1. `World::addCommand<IWorldCommand>` 是请求改变权威状态的帧内 FIFO，目前承载 Break/Use/Place；
   `World::update` 在主线程依次执行并清空。
2. `SandboxEventBus::publish` 是同步事实分发。事件在 handler 眼中为 const，并显式携带
   `Domain` 或 `Diagnostic` category；每个 World 仍拥有自己的 bus。
3. World 的 45 项 Query 继续由 AL-A1 map 管理，不得提交 Gameplay、排队 Command 或发布领域事实。

订阅者必须声明 owner、`ObserveOnly / DomainMutation` effect 和 `Forbidden / Bounded` republish。
Observer 的嵌套发布会被拒绝；显式允许的领域反应最多同时进入 8 层。每次 publish 固定入口时的
订阅 membership，handler 内 subscribe/unsubscribe 只影响后续或嵌套 publish。`Diagnostic` 不会
投递给 `DomainMutation` handler，因此诊断不能成为隐式 Gameplay command。EventBus 仍不拥有事件
历史或 Gameplay state，也不把拒绝的嵌套发布转成隐藏队列。

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
                             -> WorldSimulation::fixedTick(context)
                                  1. TickPreparation
                                  2. ActorSimulation
                                  3. Combat
                                  4. Encounter
                                  5. BlockRandomTick
                                  6. Population
                                  7. BlockEntitySimulation
                                     -> Furnace/Crusher concrete owner
                                     -> MachineRuntime one fixed transition
                                  8. GameplayRuntime
            -> update Camera from interpolated Player
            -> selection + one resolved GameplayWorldAction
            -> queue dig/use/place or perform food/combat path
            -> World::update(Camera)
                 -> ChunkRuntime publishes Player/Camera demand + frustum snapshot
                 -> execute queued IWorldCommand requests
                 -> ChunkRuntime bounded distant unload
                 -> ChunkRuntime bounded synchronous dirty-section mesh rebuild
       [single ChunkRuntime worker]
            -> B2 demand plan -> B5 96/48 retained-plan window -> B3 typed pending jobs + B4 generation token
            -> ChunkLoadOrGenerate reserve under mutex / prepare detached / token commit-or-cancel
            -> ChunkMeshBuild snapshot under mutex / build off-lock / token + revision commit-or-cancel
            -> at most 8 authoritative commit intervals per loader pass
       -> sync render camera / section meshes / actor visuals / outline
            -> offer at most 8 nearest CpuReady sections; defer the remainder
  -> update AudioRuntime and MusicRuntime
  -> collect debug stats / UI frame
Ogre::frameRenderingQueued
Ogre::frameEnded -> capture + performance record + exit/crash gates
```

暂停或非 Playing 状态由 `GameApplicationFlow` 在进入 `SandboxRuntime::update` 之前阻止 simulation；
`WorldSimulation` 不复制 pause 状态，渲染、UI 和必要的应用处理仍可继续。每个完成 tick 记录 8 项
phase 与整 tick 的 `steady_clock` 原始毫秒值，并为 Actor、Combat、Block Random Tick、Population
发布四条 last-tick work metrics。`WorldDebugStats` 把 elapsed、processed、deferred、budget scope/status
复制到开发者面板；这些值不持久化、不进入确定性比较，也不定义平均值、百分位或毫秒预算。

## 11. Render Snapshot Chain

```text
Authoritative World state
  -> ChunkRuntime worker builds CPU mesh from SectionMeshInput snapshot
  -> ChunkSection CpuReady + blockRevision
  -> World::collectSectionMeshSnapshot()
       -> ChunkRuntime copies live revisions and at most 8 deterministically selected CpuReady meshes
       -> records total/deferred ready counts; unselected sections remain CpuReady
  -> OgreBootstrap::syncSectionMeshes()
       -> NotResident/Stale -> UploadPending
       -> ChunkSectionRenderable GPU buffers
  -> acknowledgeSectionMeshUploads(location, revision)
       -> ChunkSection CpuReady -> Clean only when revision is current
  -> collect current live revisions/CpuReady snapshot
       -> accepted visual becomes GpuResident
       -> stale/unloaded visual is destroyed and becomes NotResident

ActorManager / World projectile state
  -> collectActorSnapshots() / collectCombatProjectileSnapshots()
  -> OgreActorRenderer::sync / syncProjectiles
  -> Ogre scene nodes

WorldDebugStats + Gameplay/Feedback snapshots
  -> OgreUserInterface / RuntimePerformanceCapture
```

Snapshots are copied values and Ogre owns only their visual mirrors and Render state. A removed live section destroys its Ogre visual；
stale CPU upload acknowledgement cannot promote a newer revision，且上传后会在进入下一帧前被销毁。Renderer reset/rebuild therefore does not mutate
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
C2 的 `ResourceEconomyContract` schema v2 只是 code-owned 验证输入，用来把 machine process 加入
可达性、守恒和无环证明；它不是磁盘存档格式，也没有改变上述持久化 identity。

## 13. Architecture Documentation Ownership

Architecture Lab 的当前事实仍分别由不同文档承担：`todolist.md` 是唯一任务账本，roadmap 是能力
候选池，contract 冻结批次语义，report 冻结执行证据，tutorial 解释问题、失败方案、演进与取舍。
这些身份不能互相替代。

AL-A6 为唯一 living tutorial 增加 machine-readable manifest：每个已完成批次映射到所属 Part、
真实 Section 和一个已存在的冻结证据路径。每个 Section 统一包含 Problem、Naive Solution、Failure、
Design Evolution、Implementation、Validation 和 Trade-offs 七个非空逻辑标题。只有首个已验证批次
出现后才创建对应 Track Part；未批准候选不获得占位正文。

`tools/validate_architecture_lab_documentation.ps1` 检查 manifest 与当前账本的一致性、路径边界、
Section/Part 结构和单文件规则，并在 `scripts/verify_build.ps1` 中先于编译执行。该验证保护文档身份，
不证明文字质量，也不把 roadmap proposal 提升成实现事实。

## 14. Current Conclusions Through C3

- 当前可玩的系统已经有清晰的 Renderer-to-Snapshot 边界和可验证持久化边界。
- `World` 仍承担 facade、组合根和多套 Simulation 玩法状态；AL-A2/AL-A3/AL-A4/AL-A5 只关闭了四条由真实工作
  验证的内部边界，没有试图一次性拆完 God Object。
- Chunk pipeline 的 snapshot/off-lock/revision-commit、FIFO、预算和 unload/save 语义集中在
  `ChunkRuntime` / `ChunkManager` 边界；B1 已加入 Data/Mesh/Render 三套正交状态机；B2 已加入
  四槽、可合并、可过期的 Player/Camera/Teleport/Preload Demand；B3 已用两种真实 typed job 和
  Pending/InFlight/Completed 生命周期替换 worker 私有坐标 deque；B4 又以 generation token、detached
  candidate 和线性化提交阻止旧 plan 发布权威结果；B5 以 128 hard cap、96/48 hysteresis、显式
  admission/shedding、plan window refill 和 commit/upload/unload 8/8/8 预算关闭无界压力入口；B6
  又把 Resident、Near Representation 与 Simulation Requested 从同一 demand 中正交派生，并让真实
  plan/mesh/render/unload consumer 遵守它；B10 以五阶段长稳验证关闭 Track B Core，并修复取消墓碑抢占
  unload budget 的饥饿问题。当前仍没有 Far representation、额外 worker、D2 fidelity 或未来系统空槽。
- fixed-tick 的 8 phase 顺序、context、last-tick 原始耗时及四条真实 work metrics 现在集中在
  `WorldSimulation`；玩法状态与旧实现仍由 World/Actor/Gameplay 所有，没有 `SimulationScheduler`、
  系统 Registry、时间预算或执行优先级。
- 玩家 Break/Use/Place 请求现在只走 `IWorldCommand` FIFO；World-local EventBus 只同步分发不可变事实，
  生产订阅者 effect/republish、8 层递归上限、per-publication membership 与 Diagnostic 隔离均已冻结。
- Architecture Lab 教程现在按 Track/真实 Section 维护，并由 manifest、冻结证据和完整门禁阻止空 Part、
  丢失批次或未实现能力提前进入教程。
- C1 的能力访问已由 Chest/Furnace/Crusher 三个真实 provider 使用；值句柄不缓存权威状态，具体
  序列化、槽位传输和副作用仍由容器拥有。C2 只在 Furnace/Crusher 两个真实 Processor 之间共享
  recipe/output/power/progress 的纯状态转换，没有 Capability/recipe Registry 或继承层次。
- Crusher 是正常可制作、放置、Use、卸载与保存重开的玩法块；其 20-tick pulse、40-tick cap 和
  `Cobblestone -> Sand` 单一 process 为 Machine Runtime 提供真实压力，同时不进入 34 目标和胜利链。
- C3 只把当前已加载且通过严格 payload 校验的 Crusher 投影成六面相邻图。component id 取 X/Y/Z
  字典序最小节点；canonical edge、同步 BFS merge/split、Chunk replace/remove 与 copied snapshot
  都是派生状态，不写入 save v12，也不改变每台 Crusher 独立手摇的 C2 行为。
- C3 完成不构成 B7-B9、C4-C11、Track D、动力传播、通用网络或 Extended 的自动启动权限；后续
  只有进入任务账本并单独获批的具名批次才可实施。
