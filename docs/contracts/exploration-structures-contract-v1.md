# 探索结构规划合同 v1

> Architecture Lab 迁移说明（2026-08-31）：本合同冻结历史工程证据；文中 R3、真人和延期
> 描述保留当时语境，当前退出模型见 `docs/current/ai-assisted-gameplay-acceptance-v1.md`。

## 范围与兼容边界

本合同冻结 `N9A` 的确定性结构规划层。它复用既有
`StructureBuilder` 和 terrain v2 路标算法，只负责“某个结构实例应在哪里、占多大范围、由哪些
区块投影”的纯计算；遗迹、营地、箱子战利品和新的生态内容属于 `N9B`。

- `terrain_generation_version 1` 不生成探索结构，旧世界未探索区块保持原样。
- `terrain_generation_version 2` 的 Waystone 候选流、锚点和最终方块输出保持不变。固定种子
  `20260807` 的 cell `(0, 5)` 仍为 `(18, 70, 328)`。
- `N9A` 不修改世界存档格式，也不引入 terrain v3。只有 `N9B` 的新规则确实会改变未探索区块
  时，才允许在独立迁移矩阵中评估 terrain v3；不得回写 v1/v2 身份。
- 结构没有独立于 terrain 的第二套持久版本号，也没有通用 Manager 或 Singleton。

## 实例身份与随机输入

结构实例的唯一身份是：

`(structure type, terrain generation version, cell X, cell Z)`

所有候选选择只允许读取世界 seed、terrain 版本、结构类型和 cell 坐标。terrain v2 的
Waystone 分支保留既有兼容 hash；类型和版本先选择确定的算法分支，因此相同身份始终得到相同
计划。规划不得读取区块加载顺序、线程调度、全局 `std::rand` 状态或可变运行时对象。

未知类型和不支持的 terrain 版本返回带完整身份的无效计划，不得静默投影。

## Waystone v2 冻结参数

| 项目 | 冻结值 |
| --- | --- |
| cell 尺寸 | `4 x 4` chunks，即 `64 x 64` blocks |
| 每 cell 候选数 | `8` |
| 锚点边缘留白 | `3` blocks |
| 最小相邻 cell 锚点间距 | `7` blocks |
| footprint | `5 x 6 x 5` blocks；锚点水平半径 `2`，垂直为 `y+1..y+6` |
| 投影优先级 | `100` |
| 原始计划写入数 | `195` 次；重叠坐标仍按既有 `StructureBuilder` 顺序覆盖 |
| 单目标区块计划上限 | `4` |
| 可生成地面 | 最高候选地表不低于 `WATER_LEVEL + 4` |

## 归属、冲突与投影

1. 每个计划由 cell 身份拥有；负坐标使用数学 floor 除法，不使用向零截断。
2. `plansForChunk` 只枚举 footprint 可能覆盖目标区块的有限 cell，并返回规范排序的快照。
3. footprint 的冲突按水平 `X/Z` 占地裁决，防止不同高度的世界结构叠放。较高投影优先级胜出；
   优先级相同时按 `(type, terrain version, cell X, cell Z, anchor)` 的稳定顺序胜出。
4. 结构生成阶段固定在基础地形、洞穴、矿物、植物和树木之后。
5. 一个跨区块计划会被每个相交区块独立发现，但 `StructureBuilder` 每次只把落在当前目标区块
   内的方块投影进去。规划和投影均不得同步加载相邻区块。
6. 计划查询是只读纯计算，不持有 `World`、`ChunkManager` 或邻区块指针；调试面读取同一份
   `StructurePlanSnapshot`，不另算一套结果。

## 快照与预算

`StructurePlanSnapshot` 至少暴露身份、有效性、锚点、footprint、优先级、入选候选序号、计划
写入数和选择 hash。所有字段都能被自动化逐项比较，用于定位 seed、版本、负坐标、加载顺序或
边界投影漂移。

本层每个目标区块只检查常量规模 cell，最多返回四个计划；查询不创建或加载区块。快速流送
Q1 必须在相同保存、资源与场景身份下使用已批准的 `release-candidate-windows-hidden-v1` 预算和
比较器，不允许用扩大流送半径或同步邻区块生成来换取结构完整性。

## N9A 验收

自动验收必须覆盖：

- 类型、cell、seed、terrain 版本和线程/重复调用确定性；
- v2 固定路标身份、v1 无结构和未知类型拒绝；
- 负坐标归属、footprint、间距、计划数和每区块上限；
- `std::rand` 状态隔离、查询前后区块计数不变；
- 高优先级和同优先级冲突裁决；
- 跨区块计划可发现、正反加载顺序输出一致且只有一个 Waystone core；
- VS2017/v141 Debug 与 Release、完整 Windows 门禁和快速流送 Q1。

R3 当前只记录部分真人自测，其余按用户决定延后。本批不把它标为 `PASS`，也不以它阻塞
`N9A-N12` 开发。

## 验收结果（2026-08-25）

- 新增 16 项 N9A 断言，Debug/Release 世界回归均为 `627/627`：覆盖固定 v2 路标身份、seed/
  版本/线程确定性、负坐标归属、footprint、间距、查询上限、全局随机隔离、冲突裁决、跨区块
  发现、加载顺序不变和单一 core。
- VS2017/v141 Debug/Release 客户端及相关定向目标通过；完整 Windows 门禁通过 49 项资源、36 个
  性能夹具、十三个测试目标、双配置隐藏客户端和 8 类启动错误诊断。
- 当前可比较身份下的 30 秒快速流送 Q1 对比为 `PASS`：基线/复测帧 P95 为
  `11.080/12.466 ms`，P99 为 `15.139/16.568 ms`；复测区块可见 P95/P99 为
  `35.040/35.040 ms`，队列峰值 `49`，最终驻留均为 `361 chunks / 1845 sections`。
- 固定种子 `20260825` 的 nominal/stress 隐藏 soak 各运行 60 秒、1200 fixed ticks，零失败；
  stress 峰值为 71 个区块、52 个 section、队列 49、25 个 actor。
- 后台受控 dump 为 `131,929` 字节；独立符号归档 SHA-256 为
  `61ae2e57536a63934074dde976f5dc6b79989f61e5f6982ea995c2e8e11dcf46`；69 文件发行包 ZIP
  SHA-256 为 `36FF6C813463046EB0FA980DA1C0657E4C4BFBC98D16208DBD442E51CAF341E8`。
- 旧 N8B 跟踪快照与当前候选因保存/资源身份不同而被比较器正确判定为 `INCOMPARABLE`，不作为
  通过证据；上述 Q1 结果来自当前身份的独立基线/复测。R3 仍为部分自测，其余延期。

## N9A 后续扩展边界

`N9B` 已增加遗迹和敌人营地类型，并为每种类型冻结 salt、cell/footprint、优先级、
数量上限和战利品初始化身份。持久箱子一旦存在，只认存档库存，绝不能因结构重算覆盖玩家修改。

## N9B terrain v3 与单元选择

`N9B` 会改变新世界未生成区块的结构内容，因此新世界从 terrain v2 升级为 terrain v3；世界
保存格式仍为 v9。已有 terrain v1/v2 世界永久保留其创建身份，加载、保存、备份恢复或客户端
升级都不得改写版本，也不得在未探索区块回填遗迹、营地或对应箱子。

terrain v3 继续使用 `4 x 4 chunks` 的 cell。每个 cell 由 world seed、terrain 版本和 cell
坐标先确定唯一结构类型：Waystone、Ruin 或 RaiderCamp 三选一；选中的类型仍需通过地面、起伏和
生态资格检查。未选中的类型返回带完整实例键的无效快照。这样每个 cell 最多一个有效计划，
跨边界目标区块仍最多发现四个计划，不扩大 N9A 的查询预算。

| 类型 | cell 权重 | 候选/边缘留白 | footprint | 生态 | 优先级 | 计划写入上限 |
| --- | --- | --- | --- | --- | --- | --- |
| Waystone | `1/3` | 保持 v2 的 `8 / 3` | `5 x 6 x 5` | 非低岸地面 | `100` | `195` |
| Ruin | `1/3` | `8 / 7` | `9 x 9 x 9` | LightForest / TemperateForest | `80` | `790` |
| RaiderCamp | `1/3` | `8 / 7` | `11 x 8 x 9` | Desert / Grassland | `60` | `905` |

Ruin 和 RaiderCamp 候选会扫描完整水平 footprint，只接受最低地表不低于
`WATER_LEVEL + 4` 且最大起伏不超过 2 blocks 的位置；锚点 Y 取 footprint 最高地表。相邻 cell
通过 7 blocks 留白和最大半径 5 保证不重叠，规划仍按 N9A 规则执行显式冲突裁决。

## N9B 箱子与生态战利品

每个有效 Ruin/RaiderCamp 恰有一个箱子。初始库存由结构计划的 selection hash、结构类型、
terrain 版本和 cell 身份派生，只包含三个互不重复、可堆叠且符合材料上限的条目：

| 类型 | 固定条目范围 |
| --- | --- |
| Ruin | IronIngot `1..2`、Glass `2..5`、WheatSeeds `2..6` |
| RaiderCamp | Bread `1..3`、CoalOre `4..8`、IronOre `1..4` |

固定验证 seed `20260807` 的精确快照为：Ruin cell `(3, 6)`、anchor `(222, 70, 400)`、
chest `(222, 72, 400)`、三个条目数量 `1/4/4`；RaiderCamp cell `(6, 0)`、anchor
`(436, 102, 37)`、chest `(436, 104, 38)`、三个条目数量 `3/6/2`。任何有意改变规划 salt、
候选顺序或战利品算法的后续批次都必须显式升级 terrain 身份，不能悄悄改写这个快照。

箱子方块和 version-1 容器载荷在目标区块首次地形生成时一起建立；跨区块投影只有包含箱子坐标的
区块可以创建该 block entity。生成器不读取或写入其他区块。区块从存档加载时不再运行地形生成，
因此取空的库存、玩家存入物品、破坏箱子或损坏结构都会永久保留；不得用“当前库存为空”判断
是否重新初始化。

营地只出现在现有 Desert/Grassland 压力生态，遗迹只出现在两类 forest 生态，复用既有
Brute/Stalker/Spitter 自然种群和世界/局部 actor 上限；本批不新增永久守卫、刷怪器或第二套 actor
持久状态。两类地点用不同生态权重和战利品形成风险/奖励差异。

## N9B 验收

- terrain v3 新世界、v1/v2 全部旧身份、非法 v4 和世界保存 v9 边界；
- 三种 cell 选择、两类地点均可发现、生态/起伏资格、负坐标、seed/线程/加载顺序确定性；
- 每 cell 一个计划、每区块最多四个计划、跨区块完整投影、每类恰有一个箱子和一个 block entity；
- 初始战利品精确快照、不同 seed 敏感、载荷合法且不超过三槽；
- 取空库存、玩家修改、结构损坏、保存重开和反向加载后不重置、不重复；
- v2 固定 Waystone 输出不变，v1/v2 不出现 Ruin/RaiderCamp；
- VS2017/v141 Debug/Release、完整 Windows 门禁、当前身份快速流送 Q1 与 nominal/stress soak。

R3 继续保持部分真人自测、其余延期；N9B 不把它自动标为 `PASS`，也不以它阻塞 N10-N12。

## N9B 验收结果（2026-08-25）

- 18 项新增 N9B 断言使 VS2017/v141 与完整门禁的 Debug/Release 世界回归达到 `645/645`；
  terrain v3、三类型 cell 选择、生态/起伏、seed/线程/加载顺序、跨区块单箱投影和精确快照通过。
- 取空库存、玩家存入 Dirt、损坏遗迹、保存重开均保持玩家状态；terrain v1/v2 世界永不升级，
  未探索区块不回填遗迹、营地或箱子，固定 terrain-v2 Waystone 仍在 `(18,70,328)`。
- 当前 terrain-v3 两轮 30 秒快速流送 Q1 为 `PASS`：帧 P95 `9.526/10.488 ms`、P99
  `13.691/15.380 ms`，区块可见 P95/P99 `40.073/42.039 ms`，队列峰值 `48/46`，最终驻留均为
  `361 chunks / 1846 sections`。
- 固定 seed `20260825` 的 nominal/stress 隐藏 soak 各完成 60 秒、1200 fixed ticks、零失败；
  stress 峰值为 72 个既有区块、52 个加载区块、队列 33、25 个 actor。
- 完整 Windows 门禁通过 49 项资源、36 个性能夹具、十三个测试目标、双配置隐藏客户端和 8 类
  启动错误；受控 dump 为 127,129 字节，独立符号归档 SHA-256 为
  `77122f2e47312643f3988c7fb6df2a93e3b70c90c100abffe9c986956b9993f4`，69 文件发行包 SHA-256 为
  `D3E51E95559333D2E412521522BA2A2052659EB766572F1028A47B02BAE7A380`。
- R3 只记录部分真人自测，其余延期；本结果不把 R3 伪装成 `PASS`，下一开发批次为 N10。
