# HelloMine3D Current Architecture Baseline

本文以 `AL-A0 — Latest Architecture Baseline` 的完整审计为起点，并随已完成的
AL-A1/AL-A2/AL-A3/AL-A4/AL-A5/AL-A6/B1/B2/B3/B4/B5/B6/B10/C1/C2/C3/D1 更新当前实现；它描述代码事实，而不是未来目标架构。
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
| `World/` | 102 files / 17,666 lines (A0) | 区块、方块、生成、光照、交互、区块网格 CPU 数据、世界模拟、D1 三 workload item-budget admission 和持久化组合根。 | 方块/区块、block entity、世界元数据、World 内 Actor/战斗/进度实例为权威；光照、mesh、scheduler plan/debug snapshot 为可重建或派生。 | 依赖 Actor、Gameplay、Item、Player、Sandbox Events、Diagnostics、Maths、Physics、Util；不得依赖 Ogre。 |
| `Sandbox/` | 17 / 1,183 | 应用状态、固定 tick 编排、世界集合/活动世界、输入到 World action 的协调、类型化事件协议。 | `GameApplicationFlow`、活动 world id 和调度器累积时间为运行时编排状态；事件是已发生事实，不是持久化真值。 | 依赖 World、Player、Core/Camera、Feedback、Item、Diagnostics；不依赖 Ogre。 |
| `Actor/` | 21 / 2,348 | Actor id、生命周期、Living/Mob/Player/Item actor 行为、存档值和不可变渲染快照。 | `ActorManager` 拥有的 Actor 实例为权威；`ActorSnapshot` 与 `ActorSaveState` 是发布/序列化值。 | 由 World 拥有；Actor tick 可回调 World 并发布 Sandbox 事件；依赖 Item、Player、Entity、Maths。 |
| `Feedback/` | 2 / 361 | 从已提交领域事件和开采进度生成有界 recoil、hit-stop、粒子等表现时间线；提供注册模型的表面几何。 | 全部为派生表现状态；不得改变战斗、方块、库存或存档结果。 | 订阅 Sandbox EventBus；由 Sandbox 更新，Ogre 消费 snapshot 和表面几何。 |
| `Gameplay/` | 15 / 2,340 | 目标、Alpha Journey 兼容视图、胜利、Waystone 遭遇、难度、探索奖励和胜利后事件语义。 | 注册表冻结定义和 World 所持运行时实例/保存 payload 为权威；HUD/progress snapshot 为派生。目标 definition 当前为 v3。 | 依赖 Actor、Item、Player、Sandbox Events、Maths/Util；具体实例由 World 组合。 |
| `Audio/` | 12 / 2,626 | cue/music 定义、样本缓存、流式音乐状态、真实/静默后端和音频统计。 | 定义与播放状态只对音频域权威，不是 Gameplay 真值；caption/cue 输出为派生。 | 订阅 Sandbox facts；使用 Maths/Util；由 Ogre shell 组合和逐帧更新。 |
| `Presentation/` | 8 / 812 | 语义文本、locale fallback、caption 生命周期/优先级和布局探针。 | catalogue 是显示语义来源；渲染文本和布局为派生，翻译字符串不得充当玩法 identity。 | 依赖 Item/Util；Ogre UI 消费，不反向修改 Gameplay。 |
| `Ogre/` | 17 / 8,940 | Ogre/GL3Plus/OIS 启动、窗口/焦点/输入、GPU terrain/actor/UI、音频组合、截图和帧序。 | GPU buffer、scene node、UI、方块表面反馈、capture 为派生；绝不拥有 Gameplay truth。 | 向内依赖 Sandbox、World snapshots、Actor/Audio/Presentation/Diagnostics/Item/Gameplay 等；第一方模拟层不得反向依赖 Ogre。 |
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
| root `Config.h`, `GameplayInput.*`, `RuntimeConfig.*` | 5 / 1,373 | 平台无关输入语义、绑定/冲突/hold-mode、内存配置和 settings v9 解析/原子发布。 | 已加载 `Config` 是应用配置真值；磁盘 `settings.txt` 是持久来源，UI draft 是派生/待提交。 | 被 Ogre 输入壳、Sandbox、Core、Audio/Feedback 和 World 创建入口消费。 |

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
  -> WorldSimulation -> D1 concrete phase admission -> existing World/Actor/Gameplay implementations
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
  snapshot 上增加四条历史 processed/deferred/budget 观察。D1 经独立批准后为 Managed Actors、
  Random-Tick Sections、Furnace/Crusher Block Entities 三条真实 workload 加入 64/4/32 item admission
  与稳定集合 service window，并追加 Block Entity metric；它仍不拥有玩法状态，也不是通用 Registry。
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

### 3.2 D1 concrete phase-admission path

2026-09-05 Goal 的 D2 进入调查未改变此运行时边界：0/8/32 台远处空闲 Furnace 与近处 Crusher
探针只观察到 D1 已定义的超载轮转，尚未建立进入 Full/Reduced/Dormant 的充分依据。
结果与限制见 [进入调查](../reports/simulation-activation-entry-investigation-2026-09-05.md)。

```text
WorldSimulation::fixedTick
  -> mandatory PlayerActor / cooldown work
  -> planManagedActors(live managed actors, 64)
       -> ActorManager::tickBudgetedRange(firstIndex, admitted)
  -> planRandomTickSections(active FIFO sections, existing 4)
       -> World::runRandomTicks(tick, admitted); World keeps FIFO order
  -> collect loaded Furnace + Crusher work
       -> sort Furnace before Crusher, then X/Y/Z
       -> planBlockEntities(work.size, 32)
       -> concrete Furnace/Crusher tickOne adapters
  -> publish copied plans + five phase metrics
```

`SimulationPhaseScheduler` 只拥有 Actor 与 Block Entity 的 transient next index，并为三条真实
workload 计算 item admission。稳定集合的服务窗口为 `ceil(eligible / budget)`；集合变化时索引先对
新长度取模，不缓存 Actor、Chunk 或 block-entity pointer。PlayerActor 与其余 phase barrier 不进入
可延期集合。服务一次只推进原来的一个 20 Hz step；延期状态不做 wall-clock catch-up，也不写入
save v12。C3 topology 没有 tick 行为，因此不存在伪造的 Network workload。

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

`SectionMeshInput` 默认继续提供完整方块／光照／生态快照；只有两条 mesh 构建入口显式允许
先读层封闭标志，完全封闭时省略未使用的 payload。开洞、缺少邻区段或任一非实心层仍捕获完整数据；
网格状态、revision 拒绝、上传和预算保持原语义。

这些对象是派生数据或工作协调，不拥有 block/light/save truth。AL-A2 至 B10 保持当时的 78 项公开面；
C3 为 copied topology observation 增至 79 项并同步更新 machine-checked hash；
`World::planChunkMeshWork` 仍只转发到纯 `ChunkRuntime::planMeshWork`。

`World` 按值拥有一个 `ChunkManager`；`ChunkManager` 保存 non-owning `World*` 以调用光照协调和发布
事件。其当前职责为：

- 拥有 `unordered_map<VectorXZ, Chunk>`、`TerrainGenerator` 与 `ChunkStorage`；
- 按 seed / terrain generation version / exploration reward version 冻结生成身份；
- terrain v5 的纯 `TerrainFoundation::sample` 以 floor lattice 和 uint64 hash 覆盖 signed world
  坐标；v6 橡树、v7 森林覆盖层、v8 `sampleV8` 地表/岸线、v9 `sampleV9` 内陆草甸开窗、
  v10 `sampleV10` 内陆起伏、v11 `sampleV11` 内陆浅切水系及 v12
  `TerrainEcologyPlanner` 林地层次依次版本化接入。v12 纯规划只消费 seed、带符号世界坐标和
  v11 规划列，为现有橡树选择 Standard/Tall/Broad 三种有界连通轮廓，并以低频场规划树木疏密
  与地被斑块；最大树冠水平半径为 3，总高小于 12，且保留至少 3 格可见树干。v8 的近岸探针、
  v9/v10 的低频内陆轮廓、v11 的连续河谷和 v12 的生态规划都只查询纯规划世界坐标，不加载
  邻 Chunk。v13 `sampleV13` 连续混合弯曲沙丘、岩台切谷、林地盆地和浅水湿地；岩台位移
  有界，湿地复用既有水位，Wetland/RockPlateau 只追加生态值。区块高度/生态、公开查询
  和放置规则消费同一列结果，湿地树木和草丛只落在干燥地表。完整区块生成在装饰 halo
  运算前拒绝越界。v14 沿用 v13 的高度/生态列，仅版本化三类地标建筑和树冠避让。
  `LandmarkArchitecture::blockAt` 是有界、逐格唯一的蓝图；纯结构规划校验 footprint 与前向入口，
  查询 halo 最多 9 格（树木来源 6 格加树冠 3 格），不加载邻 Chunk。同一批计划供树木避让和
  地标投影使用，箱子仍走既有 block entity 初始化与保存路径。v1–v13 旧路径及存档身份保留，
  已保存区块不重生成。v15 在同一地貌算式后加入有界干地岩台表层分支：
  连续材料场控制坡积土、沙沟宽窄及边缘裸岩，保留高度/生态、湿地、近水和高山路径；
  旧 `sampleV13` 不启用该分支。见[地表过渡合同](../contracts/surface-transitions-v15-contract-v1.md)；
  v16 使用独立的 `AdventureTerrainPlanner`：固定 3×3 区域锚点、连续归一化权重、
  温湿度与地貌参数共同规划草甸、林地、高地、沙丘、峡谷、湿地和高山轮廓；陆海场独立控制
  海岸及海床。三向边界也混合全部有效锚点，不只选最近两个；坐标偏移以 double/int64
  计算，查询有界且无区块依赖。`sampleFoundationForVersion` 统一向实际方块、公开查询、
  洞口、出生、植被与地标提供同一列。规划区域身份不写入存档，首批复用既有 biome/block ID；
  后续 v18 接入专用树型及地表材料。v1–v15 原路径与保存身份保持，见
  [区域骨架合同](../contracts/adventure-terrain-v16-contract-v1.md)。
  v17 新世界在 v16 列上叠加 `AdventureWaterPlanner`：512 米扰动节点先规划低地湖盆水头，
  再向较低势能节点连接弯曲河谷；河床汇合处共享端点，低段与湖盆使用真实水位 64。
  每次读取固定 5×5 节点，派生 3×3 出边及湖盆；线程本地缓存固定 64 项、上限 256 KiB，
  按 seed/网格完整校验，不持有世界或区块。River/Lake 只追加生态 ID；树冠投影在 v17
  开始保护实体地表和水体，防止陡岸被叶块覆盖。旧 v1–v16 生成输出与存档身份保持，见
  [水系合同](../contracts/adventure-water-v17-contract-v1.md)。
  v18 生态资源先独立接入：BlockId 27..32 / Material ID 43..48 追加雪、砾石、黏土、林床、
  苔石及淤泥，不改变既有编号。木材／叶块 metadata 0 保持橡树，1/2 表示针叶／浅色阔叶，
  真实区块保存该字节；区块材质按树种选择树皮、断面和叶片，原木掉落及配方继续共用原资源。
  v18 引入生态生成，`AdventureEcologyPlanner` 复用 v17 高度／水系及已有区域权重，
  通过 96/28 米树群斑块、37 米过渡斑块、海拔／水岸组合选择实际材料与植物。
  7 米格内固定扰动树锚点先拒绝密度，再做四次坡度查询；树型为橡树、浅色阔叶、
  分层锥形针叶和低垂湿地冠，沙地保留仙人掌／棕榈。metadata 与真实方块一起投影、
  保存，并传到选中网格及破坏／挖掘碎片。既有草 metadata 0/1 不变，2/3 为可采的蕨草／芦苇外观，
  相应几何在普通网格与反馈轮廓中共用；资源经济仍按成熟草处理。
  林床、泥岸、砾滩与波动雪线进入同一表面列，树冠不覆盖地形和水体；v1–v17 原输出保留。
  参见[v18 合同](../contracts/adventure-ecology-v18-contract-v1.md)，连续探索／最终性能验收单独记录。
  v19 为默认新世界版本：洞口规划额外返回纯几何入口清单，植被在通道投影周围留空，
  树冠来源 halo 同步纳入有界规划；不加载邻区块。营地允许使用满足既有坡差和入口条件的
  疏林／森林空地，旧版本仍保持原候选语义。v18 高度、水位、雪层及材料查询保持，
  蕨类外观只替换标准 Cross，自定义资源形状回退原定义。见[v19 合同](../contracts/adventure-exploration-v19-contract-v1.md)。
  v17+ 纯地表列查询使用每线程固定 8192 项、最多 256 KiB 的派生缓存，完整校验 seed、
  生成版本和有符号世界坐标；不缓存实际方块、存档或 World 指针，替换仅影响计算成本。
  不同高度 section 可复用同列结果，世界切换和并发线程不共享可变缓存。
  v5 基线见 [v5 合同](../contracts/terrain-foundation-v5-contract-v1.md)，当前 E2 门槛见
  [v8 合同](../contracts/ecology-surface-coast-v8-e2-contract-v1.md)，v9 首批范围见
  [E3 合同](../contracts/ecology-inland-meadow-v9-e3-contract-v1.md)，v10/v11 范围分别见
  [E4 合同](../contracts/ecology-inland-relief-v10-e4-contract-v1.md)和
  [E5 合同](../contracts/ecology-inland-water-v11-e5-contract-v1.md)，v12 范围见
  [E6 合同](../contracts/ecology-vegetation-mosaic-v12-e6-contract-v1.md)，v13 范围见
  [地貌多样化合同](../contracts/terrain-diversity-v13-contract-v1.md)，v14 范围见
  [地标建筑合同](../contracts/landmark-architecture-v14-contract-v1.md)；
- 查询、创建、加载、生成、保存和卸载 Chunk；
- 在卸载前同步保存 dirty Chunk，失败时保留 resident Chunk；
- 发布 generated/loaded/saved/unloaded 事实；
- 载入后在既有世界锁内衔接方块光：区块内部重建已完成，仅读取新边界及四个已驻留邻区块的光值，
  将亮度大于一的边界格送入原传播器；不为黑暗边界构造哈希集合，不加载缺失邻区块。
  卸载同样直接读取四个驻留边界，保留全部非零移除根；熔炉归一化、传播规则和 dirty 通知保持；
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
- world save format 当前为 v12；新世界 terrain generation 为独立 v19，旧 v1–v18 身份保留；settings 当前为独立 v10（含三档小地图范围，保留 v9 标准/兼容画面选择与旧偏好迁移）。
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
                                     -> mandatory PlayerActor/cooldowns
                                     -> up to 64 managed actors, rotating cursor
                                  3. Combat
                                  4. Encounter
                                  5. BlockRandomTick
                                     -> up to existing 4 FIFO sections
                                  6. Population
                                  7. BlockEntitySimulation
                                     -> up to 32 Furnace/Crusher items, rotating cursor
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
       -> sync render camera / section meshes / actor visuals / block surface feedback
            -> offer at most 8 nearest CpuReady sections; defer the remainder
  -> update AudioRuntime and MusicRuntime
  -> collect debug stats / UI frame
Ogre::frameRenderingQueued
Ogre::frameEnded -> capture + performance record + exit/crash gates
```

暂停或非 Playing 状态由 `GameApplicationFlow` 在进入 `SandboxRuntime::update` 之前阻止 simulation；
`WorldSimulation` 不复制 pause 状态，渲染、UI 和必要的应用处理仍可继续。每个完成 tick 记录 8 项
phase 与整 tick 的 `steady_clock` 原始毫秒值，并为 Actor、Combat、Block Random Tick、Population、
Block Entity 发布五条 last-tick work metrics。D1 另复制恰好三条 workload plan：eligible、admitted、
deferred、item budget、first index 和稳定集合 service window。`WorldDebugStats` 只把这些值复制到
开发者面板；它们不持久化、不进入确定性比较，也不以 wall-clock 反向决定 Gameplay。

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

BlockSelection + MiningProgressSnapshot + ActionFeedbackSnapshot
  -> OgreBlockFeedback: registered model surface highlight / cracks / world fragments
```

Snapshots are copied values and Ogre owns only their visual mirrors and Render state. A removed live section destroys its Ogre visual；
stale CPU upload acknowledgement cannot promote a newer revision，且上传后会在进入下一帧前被销毁。Renderer reset/rebuild therefore does not mutate
block、Actor、inventory、objective or persistence truth。
Ogre 保留上一份 live section 数值坐标清单，仅在清单改变时重建字符串驻留索引并清理卸载；
退出世界／销毁渲染器时同步清空该派生清单。无上传时不取第二份确认快照；有上传时仍
确认真实当前 revision 和 CpuReady 状态，但仅对至多八个上传位置做数值查找，不构建全量字符串索引。
内置单 pass、写深度的 Terrain／Flora 可将同一 x/z 列内最多四个连续高度 section 合并成 GPU 对象；
`TerrainRenderBatch` 保留全部顶点、三角形、UV、重复坐标与光照，只重基到共同原点。
Ogre 从复制快照保留驻留 section 的 solid/flora CPU 数据，修改、卸载及陈旧确认均失效所属组，
本帧上传结束后统一重建；确认、八 section 预算及 World 权威仍按原 section。
水／玻璃保持逐 section 排序；非内置程序、混合、禁写深度或多 pass 材质保持原对象路径。
两处世界退出均释放分组 GPU／CPU 数据。统计同时累计独立对象和分组对象的真实缓冲，
新增 resident renderables 仅指驻留 GPU 对象数量，不冒充相机／阴影实际 draw 次数。

方向阴影的太阳投影设置由 Ogre 灯持有，第一方纯数学 helper 负责正午稳定参考轴与纹素锚定；
terrain/actor 接收端共享连续比较滤波语义。切档、切世界及退出随灯清理，不向 World 写回状态，
不改第三方默认相机；当前参数与范围见[阴影合同](../contracts/directional-shadow-contract-v1.md)。

POSIX Ogre `Timer` 的经过时间使用 `std::chrono::steady_clock`，毫秒/微秒 API 和各实例 reset 语义不变；
避免系统墙钟校正经 Root 的无符号差值形成巨大帧增量。CPU 时间 API 保持原行为，世界仍按既有 fixed tick
推进，存档时间戳不受此修改影响。真实长帧由诊断原样记录，不以截断帧耗时掩盖渲染停顿。

方块表面反馈复用实际模型、metadata 高度、生态 tile、透明遮罩与 Flora 风摆，高亮和十阶段裂纹
由 Ogre 持有；目标改变或取消时更新派生表现。开采碎屑观察真实进度，破坏/放置碎屑只订阅
已提交事实，使用世界坐标和解析重力轨迹；拾取图标仍在 HUD。两者共用 48 粒子上限与
0.55 秒生命周期，不进入世界 fixed tick 或存档。详见[表现补充合同](../contracts/block-feedback-contract-v2.md)。

湿地的既有 `TallGrass` 通过纯值 `WetlandGrassGeometry` 派生细长叶片、错层茎秆与成熟穗头；
仅匹配 Wetland 和未改造的标准 Cross，资源包自定义形状保留原路径。区块网格和表面反馈共用
同一几何及逐顶点风摆高度，复用已注册 Grass/OakBark tile；幼株六面、成熟八面，仍进入既有
Flora 批次。世界生成、方块 ID、成熟状态、命中、掉落与存档不承载此表现变体。

世界中的 Stone/Sand 在普通与阴影 terrain fragment 中按连续世界坐标派生矿物色层和沙面风纹；
岩层沿高度缓慢弯曲，沙纹具有局部曲率和强弱变化，两者按像素覆盖衰减细节。沿用既有 tile、
纹理、顶点与绘制批次，不修改生成或碰撞。该表现也适用于玩家放置的块及旧世界，不区分块的
来源；矿石、圆石、机器保持各自路径。兼容图集仍可叠加地质着色，关闭 surface lighting 才
返回原始着色，不能把两种回退混为一谈。

生态植被颜色由 `SectionMeshInput` 捕获的纯值 `TerrainEcologyColour` 场驱动：世界对齐 4 米网格、
8 米半径过滤，每段 81 次纯查询、25 个气候值，不加载邻块。uv0 整数 tile 保持，小数携带
干暖/林湿参数；uv1、32 字节顶点、光照及存档不变。greedy 检查内部颜色可重建，普通/阴影
材质使用同变体参考层再着色；关闭表面光照保留旧生态行。见
[颜色过渡合同](../contracts/ecology-colour-transition-contract-v1.md)。

圆形小地图属于 Ogre 派生表现。2026-09-17 视觉升级将旧的生成器预测色块改为
`ChunkManager::collectSurfaceMapSamples` 的真实驻留区块表面快照：World 锁忙时延后，
每次最多 256 列，UI 以 30 Hz、每次 195 列轮转刷新 65×65 个采样点；三档范围对应 1/2/4 米步长，不加载或生成区块。
快照只复制材料与高度，不携带 Chunk 指针；细小花草被略去，树冠、建筑、玩家改块、水岸和
坡差参与显示。未知区用暗色，缓存按世界身份和实际坐标重映射，换世界清空。
`MinimapNavigation::Memory` 只记实际使用的容器、工作台和激活路标，单次世界访问最多 32 个，
最多画 8 个不重叠图标；换世界清空，改变缩放时清空旧尺度样本。区域名称来自玩家当前坐标的既有 biome。
设置 v10 持久化 64/128/256 m 范围，v1–v9 默认迁移到 128 m，不增加输入快捷键或 restart 要求。
这不是探索存档或 Actor 雷达，地图数据不写回世界或存档。详细范围见
[视觉升级方案](visual-upgrade-plan-2026-09-17.md)。

`HELLOMINE3D_VISUAL_CAMERA_SWEEP` 是仅供隐藏截图的渲染相机观察入口，值为
`dx dy dz yaw pitch duration`；在 4 秒预热后平滑移动最多 48 m、转向最多 180°/45°，持续 1–30 秒。
必须有初始世界且开启隐藏截图，不允许与性能/批量场景混用。它只覆写 Ogre 相机，物品朝向使用该观察机位；
不写玩家、逻辑相机、输入、区块驻留或存档，离开世界即停用。普通启动不启用此路径。
可选 `HELLOMINE3D_VISUAL_CAMERA_PATH` 提供 2–13 个相对 xyz 中间点，首点为零、末点匹配 sweep；
每点距原点不超过 48 米、折线路程不超过 64 米。它复用冻结调查路线的高程，避免端点直线穿山，
仍不提供碰撞或步行，普通启动／混合性能场景继续拒绝。
因此这类连续帧仍属于诊断，不是移动输入、地图位置变化或正常碰撞验收。

第二轮水面使用 `SectionMeshInput` 的有界驻留水深快照（最多 8 格），以现有 repeat UV 携带角点深度/岸线，
以水材质闲置 atlas UV 携带顺岸风驱漂移；四个共享角点样本给出水覆盖和岸线切向，不追加查询或顶点 stride。
双相位细纹仅改变水面颜色，不代表 E5 等高水体的水文流向。岸床编辑通过既有 dirty planner 失效上方关联段。相机每帧最多观察两个驻留方块，
在水面下 0.05–1.0 m 带内连续增加水下雾和轻微曝光衰减，地平线背景使用相同介质色。
`WorldEnvironment::cameraWaterImmersion` 在顶层水块底部达到 1，与更深驻留水块连续，
不需要时间累积或额外方块查询；空气和未知块保持原环境。基础水介质与增强大气能力独立，
关闭大气时仍保留水下雾和地平线衔接，云层、表面光照和水面细节继续使用各自回退，
见[水面合同](../contracts/visual-water-depth-v1.md)。
水面 fragment 对穿越表面的颜色和覆盖连续混合，±0.10 m 内淡出被近裁剪面切开的表层，
水上 0.35 m / 水下 1.0 m 外恢复原覆盖；仍使用共享世界表面高度，不增加查询或改变水深数据。
陆地岸面复用同一快照：含水时按共享角点的相邻水覆盖，给不透明立方体派生网格增加最多 18% 的静态明暗衰减，
以一格插值过渡回干燥外观；每个 builder 的 17³ 字节缓存最多读取每角点八格快照，无额外 World 查询。
无水快照跳过，greedy 合并保留最终明暗梯度，不修改真实光照、顶点格式、透明/植物/水材质或存档。
`ActorSnapshot` 复制物品材质、数量、年龄；Ogre/UI 共用有界纯几何缓存，工具按 Alpha 轮廓挤出厚度、
方块使用实际材质分面。掉落方块保留旋转，薄图标模型通过纯 `ItemVisualPose` 朝向观察者并有界摆动，
避免转到侧面丢失轮廓；不改变 Actor、拾取或保存数据，见[物品表现合同](../contracts/visual-item-volume-v1.md)。敌人 shader 接收局部坐标与
每部件 custom 参数（角色/守卫/蓄力比例/原型），派生局部坐标材质分区与面部；细颗粒按屏幕覆盖渐隐，
眼睛和 crest 符记之外不新增发光。普通和阴影路径保持相同语义，平色回退保留；不增加 AI、判定、掉落或持久化状态。

敌人关节通过纯 `EnemyPresentation` 派生偏移，让四肢围绕肩髋顶端、头与口鼻围绕共同颈部运动。
Spitter 用固定颈部体积桥接抬头时的间隙，占满既有 8 部件预算；原部件编号、根部姿态和逻辑 AABB 保持。
每个 `OgreActorRenderer::ActorVisual` 持有一份 `GaitPhase`，只从 Chase 快照累计水平距离，
相位保持在一周内；超过 2 m 的相邻位移只重设参考位置，死亡不累计。生命周期与已有 visual 一致，
不另建全局缓存，不改 Actor、攻击 tick 或存档。已有诊断展示增加 `walk`/`cycle` 及可选 6/12/24 m 距离，
只提供渲染快照，普通入口拒绝诊断距离。
同一 visual 的 `PoseBlend` 用帧时间平滑关节角度、缩放、前倾和步态起伏，再重算连接偏移；
受击/死亡与蓄力眼色保持即时，暂停、传送和生命周期清理有明确边界。诊断展示通过
`EnemyPresentationGallery` 读取冻结 registry 的真实体型和战斗 tick，不使用统一放大的身体尺寸。

行囊、储物与合成面板及快捷栏共用 `Ogre/GameInterfaceWidgets.h` 的纯绘制槽位／面板装饰。
物品预览复用 `itemVisualGeometry` 与实际图集，局部投影、排序和明暗均为派生表现；
箱子仍通过 capability provider 转移，合成仍通过 `CraftingSession` 预览／提交，不写入新玩法或存档状态。
窄窗口中材料列表单独滚动，结果区和主要操作固定；具体视觉范围见
[游戏 UI 方向](game-ui-direction-2026-09-20.md)。
常驻 HUD 的底板与叶片／心形使用两份 Ogre 所持纹理；底板九宫格拉伸、图形 Alpha／nearest 采样，
文字与数值继续即时绘制。任务标题、五格状态底座、按键提示和紧凑窗口两侧通知仅派生布局；
小地图真实样本／缓存预算不变，区域名宽度独立于圆盘；四向徽标使用双语文本，
居中比例尺端点跨 32 个采样间隔，距离随 1／2／4 米步长变化。素材初始化加载／关闭释放，不追加世界查询，
不持有物品、不改变字幕优先级／寿命或保存格式；见 [r35 记录](../reports/hud-interface-r35-2026-09-21.md)。

第一人称 UI 通过纯 `PlayerHandPresentation` 缓存空手、握图标、托方块三种手臂几何，
每种最多 54 个分面；和已有持物共用动作姿态及深度排序。daylight 只调整表现曝光，
窄窗口按 HUD 右侧余量缩放，面板/暂停隐藏；不新增 Player 字段、动作事件或世界查询。

暖野 M1 在用户再次评价树冠后撤回叶簇原型，网格生成、剔除、上传与阴影回到既有立方叶路径，
树叶沿用暖野 v1 贴图；不保留额外叶网格层。标准材质 profile v2 在方块注册前解析冻结，
GL 能力与用户设置在创建世界前只选择一次有效表现。
标准路径使用 64×64×256、7 级独立 mip 的数组纹理，CPU 载荷校验不依赖 Ogre；
Ogre 持有 GPU 纹理和重载用载荷，在释放渲染根后释放 loader。
UI/兼容路径沿用独立旧图集；旧资源包覆盖、能力不足与用户兼容选择均保持完整旧路径。
坏数组与坏 shader 明确失败，不伪装能力回退。最新范围见
[M1 实施记录](../reports/warm-wilderness-m1-implementation-2026-09-12.md)。

## 12. Frozen Version and Boundary Facts

| Identity | A0 value |
| -------- | -------- |
| world save format | v12 |
| terrain generation | v5 |
| runtime settings | v9 |
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

## 14. Current Conclusions Through D1

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
- fixed-tick 的 8 phase 顺序与 mandatory barrier 不变；D1 在 `WorldSimulation` 内组合一个具体
  `SimulationPhaseScheduler`，只对 Managed Actors、Random-Tick Sections、Furnace/Crusher Block
  Entities 做 64/4/32 item admission。Actor/Block Entity 使用轮转索引，Random Tick 保留 World FIFO；
  稳定集合在 `ceil(N/B)` tick 内获得服务。没有墙钟预算、通用系统 Registry、持久化 cursor 或 D2 LOD。
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
- C3 本身没有授权 Track D；D1 是之后单独获批的具名批次。D1 又不构成 B7-B9、C4-C11、D2-D8、
  动力传播、通用网络或 Extended 的自动启动权限。

### HUD 指针所有权

`Presentation/HudInteraction` 只保存临时页面；Ogre 输入壳负责捕获切换，进入或离开时清理输入并等待旧鼠标按钮释放。Tab 查看时暂停世界模拟，Esc 先关闭 HUD 再处理普通暂停。物品浮层读取玩家槽位和冻结注册表，点击通过既有 PlayerInputState 选槽，不修改库存数量。详见 [HUD 交互合同](../contracts/hud-interaction-contract-v1.md)。

任务日志通过 `World::getObjectiveSnapshot(true)` 按需取得值快照，AlphaJourney 转发至 ObjectiveSystem；任务可见性、前置、进度仍由冻结定义和保存状态决定。UI 的选择／过滤／追踪均为当前世界临时状态，不进入保存协议。

区域立体地图使用 `Presentation/TerrainMapView` 对小地图的 `SurfaceMapSample` 值快照进行正交投影；只绘制已知表面和相邻已知列之间的高差墙面，按深度排序并从前到后拾取。共享 65×65 样本、每次 195 列、30 Hz 预算，按样本 revision／视角重建缓存；缩放和平移无需重新采样或生成区块。手势保留按下原点，并应用最终释放位置，兼容渲染帧之间完成的快速拖动。几何、选择、发现地点与页面均为临时表现状态，切世界清理，不影响地形 v19 和存档。
