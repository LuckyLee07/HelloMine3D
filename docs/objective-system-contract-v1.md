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

N2 在该终点后追加 `smelt_item` 第九种类型和 5 个必做目标，当前基础定义共 16 项；旧十步
仍是 `AlphaJourney` 的完整兼容范围。N2 的新增语义和当前证据见
`smelting-progression-contract-v1.md`。

`AlphaJourney` 现在只是兼容门面，把通用目标快照映射回 G6 的十步枚举和低 10 位标志。
新代码应使用 `ObjectiveSnapshot`；旧接口在 v4 迁移和 G6 回归仍保留。

## 世界存档版本 5

当前 `world.meta` 版本 5 在 v4 字段之外增加：

```text
objective_definition_version 1
objective_completed_count 2
objective_completed alpha.gather_wood
objective_completed alpha.craft_workbench
objective_progress_count 1
objective_progress alpha.place_workbench 1
```

完成集合和部分进度最多各 256 项。计数必须先出现并与记录数完全一致；ID 必须规范且唯一；
进度必须在有界整数范围内，已完成目标不得同时保留部分进度。`alpha_journey_flags` 在 v5
继续作为兼容镜像，必须与完成集合中的 10 个旧 ID 精确一致。任何版本、计数、重复项、进度
或镜像不一致都会拒绝候选文件，并由既有事务层保留最后一份有效存档。

迁移规则如下：

- v1-v3 没有旅程字段，迁移为空目标状态；
- v4 的低 10 位旅程标志逐位迁移为对应目标 ID；
- 合法但当前定义未知的完成 ID 和部分进度原样保留、稳定排序后再保存，但不计入 HUD、
  不推进已知目标，也不产生反馈；
- 当前只接受定义版本 1；未来修改完成语义时必须先增加显式迁移，不能静默复用版本号。

跟踪夹具 `tools/fixtures/objectives/world-v4-partial-alpha.meta` 固定了 v4 部分旅程迁移；
G6 的 v3 空旅程夹具继续证明更老世界可读。

## 自动证据

`HelloMine3DWorldRuntimeSmoke` 的 N1 用例覆盖基础定义、八种目标类型、严格拒绝、前置顺序、
查询只读、部分进度保存、未知目标保留、真正重开、无奖励、v5 正反例和 v4 迁移。加入 N1
后完整世界栈为 `429/429`；G6 正常玩家路径仍证明拾取后停在“保存并重开”，重新进入才完成。

`HelloMine3DResourcePackSmoke` 增加目标覆盖拒绝后为 `24/24`；资源清单检查为 41 项。
目录、事务、备份和 soak 分别保持 `45/45`、`16/16`、`19/19` 和通过。正式键鼠观感仍按
既定计划后置到 N6/R3，不改变本合同的数据、迁移和失败语义。
