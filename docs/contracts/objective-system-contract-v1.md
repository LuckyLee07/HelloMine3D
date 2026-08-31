# 目标与首轮引导合同 v1

本文冻结 N1 的数据驱动目标边界。目标系统负责回答“玩家下一步做什么、已经完成什么”，
不拥有库存、方块、Actor、制作结果或奖励，也不替代这些系统各自的守恒规则。

## 定义资源

基础定义位于 `media/objectives/Base.objective`，使用严格的块式文本格式：

```text
# HelloMine3D objective registry v1
version 1
objective alpha.gather_wood
type break_block
target hellomine:oak_bark
required 11
prerequisite none
visible 1
optional 0
title "Start with wood"
instruction "Collect 11 Oak Bark for a Workbench and tools."
feedback "Enough Oak Bark collected"
end
```

N1 第一版支持 `obtain_item`、`craft_item`、`place_block`、`break_block`、
`defeat_enemy`、`reach_location`、`pickup_item` 和 `reopen_world` 八种类型。
物品/方块引用必须已经注册；到达位置必须给出有限坐标、正半径；到达/重开目标的完成数
必须为 1；隐藏目标必须是可选目标。
目标 ID 只能使用规范化小写 ASCII，前置目标必须已经在同一冻结视图中声明，因此缺失前置、
前向引用、环、重复 ID、未知字段、重复字段、错误类型和错误版本都会在启动时明确拒绝。

注册表在 Ogre 初始化前从有效资源视图冻结一次。N1 封板时基础清单包含 41 项，其中目标资源 1 项。
资源包 v1 不允许取得 `objective` 类别所有权，避免未版本化覆盖改变已有世界进度语义；无窗口
测试可以从同一基础文件建立只读注册表，不维护第二份硬编码定义。

## 运行时语义

N1 基础链保留 G6 的 10 个可见必做目标，并增加 1 个隐藏可选到达位置目标。事件目标只消费
`SandboxEventBus` 已发生的事实：

- 制作、放置、破坏、敌人死亡和物品拾取分别消费对应领域事件；
- 取得物品读取玩家当前库存，到达位置读取玩家位置；
- 查询 `ObjectiveSnapshot`、绘制 HUD 或保存快照均不能推进进度；
- 前置目标未完成时，后续事件不缓存、不补记；
- `reopen_world` 只在进入世界时前置目标已经完成的会话中生效，同一会话刚完成拾取不能
  直接冒充一次重开；
- 隐藏可选目标不改变必做总数，也不抢占玩家可见完成反馈。

HUD 显示当前目标、整数进度、下一目标标题和 3 秒完成反馈。N1 封板时，全部 10 个必做
目标完成后到达明确的首轮会话终点。N1 不发放任何物品奖励；提示和进度永远不能修改业务状态。

N2 在该终点后追加 `smelt_item` 第九种类型和 5 个必做目标，N2 封板时基础定义共 16 项；旧十步
仍是 `AlphaJourney` 的完整兼容范围。N2 的新增语义和当前证据见
`smelting-progression-contract-v1.md`。

N3 再追加 `consume_item` 第十种类型，以及“制作面包”“食用面包”2 个必做目标，当前基础
定义共 18 项。只有实际恢复生命的 `FoodConsumedEvent` 才推进食用目标；错误物品、恢复量为
0、满血或其他失败结果都不能推进。食用目标的材料必须在严格食物注册表中登记。完整语义
见 `food-recovery-contract-v1.md`。

`AlphaJourney` 现在只是兼容门面，把通用目标快照映射回 G6 的十步枚举和低 10 位标志。
新代码应使用 `ObjectiveSnapshot`；旧接口在 v4 迁移和 G6 回归仍保留。

## 目标状态与当前世界存档

目标状态最初由 `world.meta` 版本 5 在 v4 字段之外增加：

```text
objective_definition_version 1
objective_completed_count 2
objective_completed alpha.gather_wood
objective_completed alpha.craft_workbench
objective_progress_count 1
objective_progress alpha.place_workbench 1
```

完成集合和部分进度最多各 256 项。计数必须先出现并与记录数完全一致；ID 必须规范且唯一；
进度必须在有界整数范围内，已完成目标不得同时保留部分进度。`alpha_journey_flags` 从 v5
继续作为兼容镜像，必须与完成集合中的 10 个旧 ID 精确一致。任何版本、计数、重复项、进度
或镜像不一致都会拒绝候选文件，并由既有事务层保留最后一份有效存档。

迁移规则如下：

- v1-v3 没有旅程字段，迁移为空目标状态；
- v4 的低 10 位旅程标志逐位迁移为对应目标 ID；
- 合法但当前定义未知的完成 ID 和部分进度原样保留、稳定排序后再保存，但不计入 HUD、
  不推进已知目标，也不产生反馈；
- N7B 把当时的定义版本提升为 2；读取定义版本 1 的 v5-v9 世界时保留完成集合和部分进度，
  再显式归一为版本 2。P11C 又把当前定义版本提升为 3；当前读取 v1/v2 状态后保留集合与
  进度并归一为 v3，保存只发布 v3。未来修改完成语义仍必须增加显式迁移，不能静默复用版本号。

N3 将当前世界格式升级到 v6，只追加玩家生命和食物冷却字段。v5→v6 必须原样保留完成
集合、部分进度和未知规范 ID，并为新字段采用生命 20、冷却 0 的默认值；跟踪夹具
`tools/fixtures/food/world-v5-objectives.meta` 固定该迁移。

跟踪夹具 `tools/fixtures/objectives/world-v4-partial-alpha.meta` 固定了 v4 部分旅程迁移；
G6 的 v3 空旅程夹具继续证明更老世界可读。

## 自动证据

`HelloMine3DWorldRuntimeSmoke` 的 N1 用例覆盖基础定义、八种目标类型、严格拒绝、前置顺序、
查询只读、部分进度保存、未知目标保留、真正重开、无奖励、v5 正反例和 v4 迁移。加入 N1
后完整世界栈为 `429/429`；G6 正常玩家路径仍证明拾取后停在“保存并重开”，重新进入才完成。

`HelloMine3DResourcePackSmoke` 增加目标覆盖拒绝后为 `24/24`；资源清单检查为 41 项。
目录、事务、备份和 soak 分别保持 `45/45`、`16/16`、`19/19` 和通过。正式键鼠观感仍按
既定计划后置到 N6/R3，不改变本合同的数据、迁移和失败语义。

N3 完成后的当前总量为 18 个基础定义、10 种目标类型、世界运行时 `463/463`、资源包
`26/26` 和 44 项资源清单；N1 的历史封板数字继续保留，用于区分每批增量。

N7B 在既有 N4/N5 扩展链之后新增 `activate_waystone` 和 `claim_victory_reward` 两种单事件
类型，并允许 `defeat_enemy` 使用可选的规范 `target_actor` 精确筛选守卫变体。N7B 封板时基础定义
共 28 项、12 种类型、定义版本 2；其中 5 个有顺序的终局目标只读取领域事件，不能发奖、
修改路标或把目标耗尽推断为胜利。版本 1 存档迁移、错误 actor 过滤、激活/死亡/领奖顺序和
只读快照均由 N7B 世界运行时回归覆盖；完整双配置世界栈为 `580/580`（2026-08-25）。

## P11C 并行机会与配方发现补充

P11C 在保留 N7B 历史身份的基础上把当前定义升级为 v3，共 34 项：原有 28 项顺序不变，在隐藏
Alpha 标记之后追加 4 项 `shelter.*` 和 2 项早期 `exploration.*`。完成木材采集后，工作台成长、
木板庇护所和煤矿探索可同时满足前置；系统按注册顺序最多输出 3 项 `opportunities`。事件会推进
所有已满足前置的匹配分支，而不是只推进 HUD 当前项，因此切换、保存和恢复不会丢失其他分支。
旧 `current/next` 字段继续投影前两项，`AlphaJourney` 十步门面和低十位标志不变。

配方发现只在生产运行时目标注册表上启用，独立单元测试注册表不被隐式耦合。正向库存变化和成功
制作触发材料相关配方发现；恢复时扫描现有库存与已完成目标的目标材料。发现集合以
`recipe.<16 位 FNV-1a 十六进制>` 写入既有未知完成 ID 扩展槽，启动时检查当前配方的 token
碰撞；未知 token 继续无损往返。配方书据此过滤显示，但制作会话仍按真实网格匹配，不把知识
状态当作合成权限。

P11C 保持 save v11，v1/v2 目标状态归一到 v3，当前保存只发布 v3。Debug/Release 定向均为
`41/41`，Release 完整世界为 `803/803`，配方/资源双配置为 `121/121`、`80/80`。完整 30 分钟
30 分钟无上下文 AI 盲玩当前为 `NOT_RUN`；结论只作为 AI 可理解性代理，人类首次体验/乐趣
不声明。详细边界见 `first-thirty-minutes-contract-v1.md`（2026-08-31 迁移）。
