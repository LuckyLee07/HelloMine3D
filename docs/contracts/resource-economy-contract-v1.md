# N10 资源经济合同 v1

> Architecture Lab 迁移说明（2026-08-31）：本合同冻结历史工程证据；文中 R3、真人和延期
> 描述保留当时语境，当前退出模型见 `docs/current/ai-assisted-gameplay-acceptance-v1.md`。

本文冻结 N10 的食物、冶炼、精确配方、资源来源/消耗、经济校验和容量边界。它在 N2/N3 已有
原子制作、三槽熔炉和主动食用语义上扩展内容，不改变这些模型的所有权。

最后更新：2026-08-25。

## 版本和持久化

- 世界/玩家保存保持 v9，熔炉 payload 保持 v1，terrain 保持世界创建时的 v1/v2/v3 身份。
- 新材料只追加在 `WaystoneCore` 后：`RawMeat`、`CookedMeat`、`CactusSalad`、
  `TrailRation`、`PlantFiber`。旧材料数值不移动，旧存档不需要迁移。
- 食物冷却继续使用玩家的单一 `foodCooldownTicks` 持久字段；不同食物只从冻结定义读取恢复量和
  冷却，不新增离线时间戳。暂停、未执行固定 tick 和关闭游戏都不推进冷却。
- 熔炉继续保存单输入、单燃料、单输出、当前进度和剩余燃烧 tick；区块卸载不推进，也不在
  重载后追赶离线时间。

## 冻结内容

### 食物

| 材料 | 来源/成本角色 | 恢复 | 冷却 tick | 选择理由 |
| ---- | ------------- | ---- | --------- | -------- |
| Bread | 3 Wheat 精确制作 | 6 | 20 | 稳定农作物基础恢复。 |
| CookedMeat | 敌人掉落 RawMeat，熔炼 60 tick | 9 | 28 | 战斗路线取得更高单次恢复。 |
| CactusSalad | 2 Cactus + 1 Wheat | 4 | 12 | 跨生态、低恢复但最快再次使用。 |
| TrailRation | Bread + CookedMeat + CactusSalad + PlantFiber | 14 | 40 | 高取得成本换取远行时的单槽密度。 |

第一轮仍不增加饥饿、被动掉血或强制进食。食用在满生命、死亡、暂停、冷却、UI 占用和库存失败
时不得消耗；成功后只消耗一件、发布一次事件，并把生命钳制在上限。

### 冶炼和燃料

| 输入 | 输出 | 时间 tick |
| ---- | ---- | --------- |
| IronOre | IronIngot ×1 | 100 |
| RawMeat | CookedMeat ×1 | 60 |
| Sand | Glass ×1 | 80 |

`CoalOre` 燃烧 160 tick，`PlantFiber` 燃烧 40 tick。一次燃料开始燃烧后沿用已有剩余 tick；输出
满、输入缺失或无燃料时不推进。批量操作仍由既有逐次原子转移和固定 tick 完成，不增加多输入、
多输出、温度、品质或十二槽炉体。

### 精确配方

基础配方总数从 11 增至 19。新增八个配方：TallGrass/DeadShrub/Rose 各 2 件精确转换为
PlantFiber；CactusSalad；TrailRation；消耗 4 OakBark + 2 PlantFiber 的 FieldChest；由
OakBark/PlantFiber 对角排列的 FieldWorkbench；由 6 Stone + 2 Glass 构成的
ReinforcedFurnace。它们不会替代原有配方，也不引入材料组模糊匹配。

## 来源、消耗和守恒

| N10 材料 | 正常来源 | 有效消耗点 |
| -------- | -------- | ---------- |
| RawMeat | Stalker 1，Brute 1-2，一次死亡只掉落一次 | 60 tick 熔炼为 CookedMeat |
| CookedMeat | RawMeat 冶炼 | 主动食用；TrailRation 配方 |
| CactusSalad | Cactus + Wheat 精确制作 | 主动食用；TrailRation 配方 |
| TrailRation | 四种明确材料精确制作 | 主动食用 |
| PlantFiber | TallGrass、DeadShrub 或 Rose 精确制作 | 燃料；TrailRation、FieldChest、FieldWorkbench |

制作预览必须纯读；单次/批量提交必须在同一库存 revision 上验证全部输入和产物容量后原子交换。
熔炉输入、燃料、输出和破坏溢出继续逐槽守恒。失败、满容量、陈旧预览、连点、关闭、保存重载
不得消耗或复制物品。

## 经济校验器 schema

`ResourceEconomyContract` v1 固定四类输入：

1. `acquisitionSources`：来源 id、材料、一次取得量、估算取得 tick；
2. `requiredMaterials`：干净世界必须可达的主线材料；
3. `trackedNewMaterials`：本批必须同时具有来源和消耗点的材料；
4. `goalRequirements`：目标 id、材料和所需数量。

校验器从冻结的 `RecipeRegistry`、`SmeltingRegistry` 和 `FoodRegistry` 读取权威变换，执行固定点
可达性/最低成本计算，并把每条配方输入、冶炼输入和燃料依赖加入有向图。任何材料环都按比
“只拒绝净正环”更严格的规则拒绝，因此无成本环、燃料自举和复制回路不能通过。报告同时验证
每个新增材料的生产与消费集合，并输出 CSV：取得 tick/件、恢复、冷却、恢复/取得 tick、最大堆叠、
目标总量和目标库存槽。

当前正式报告由 `HelloMine3DRecipeSmoke` 写入隔离的
`bin/validation_runs/resource_economy/resource-economy.csv`，不进入源码提交或发行包。关键快照：

| 材料 | 取得 tick/件 | 恢复/取得 tick | 目标量/槽 |
| ---- | -----------: | -------------: | --------- |
| IronIngot | 170 | 0 | 7 / 1 |
| Bread | 481 | 0.0125 | 2 / 1 |
| CookedMeat | 230 | 0.0391 | 2 / 1 |
| CactusSalad | 201 | 0.0199 | 0 / 0 |
| TrailRation | 934 | 0.0150 | 1 / 1 |
| PlantFiber | 21 | 0 | 0 / 0 |

取得成本是可比较的平衡观测值，不是墙钟承诺；世界来源成本、1 tick 制作提交和燃料按燃烧 tick
摊销共同参与计算。改变来源、配方、燃烧时间或目标需求必须更新合同快照并重跑负例。

## 容量和预算

- 玩家库存仍为 5 槽；新增五种材料均最多堆叠 99。最坏的五类新增材料各占满一槽时是 495 件。
- Release 定向样本中，1 槽基线 `world.meta` 为 629 字节，5 槽 N10 样本为 722 字节；同次事务
  写入分别为 28,496 / 16,530 微秒。自动门禁使用更稳定的上界：增量不超过 256 字节、单次写入
  小于 2 秒，不把这次墙钟值当作性能预算。
- 图集仍为 256×256、16×16 单元；N10 只占用第三行 x=10..14。既有单元由原 FS3 生成源构建，
  新单元由独立透明生成源确定性裁切，资源包路径和清单条目数不变。

## 自动验收

- `HelloMine3DRecipeSmoke`：117/117。覆盖 19/3/2/4 内容计数、精确配方原子提交、主线可达、
  新材料来源/消耗、无环、CSV 导出，以及缺来源、缺消耗和净正材料环负例。
- `HelloMine3DWorldRuntimeSmoke`：650/650。N10 新增 5 项运行时断言，覆盖四种食物、两条新冶炼、
  PlantFiber 燃料、v9 新材料往返、5 槽/495 件容量和保存尺寸/耗时；FS3 同时证明 15 个物品图标
  全部可见且互不相同。
- VS2017/v141 Debug/Release 的 `HelloMine3DRecipeSmoke` 均为 117/117，
  `HelloMine3DWorldRuntimeSmoke` 均为 650/650；完整 Windows 门禁另以生成的 VS2022/v143 工程
  通过 Debug/Release 零错误全量重建、49 项资源、36 个性能夹具、十三个测试目标、隐藏客户端、
  8 类启动负例、131,009 字节受控 dump 和 69 文件隔离发行包。符号 ZIP SHA-256 为
  `34ebbe24d74572b471237243f32b5e3cdd85e943a3106a5d5eab75b3e6a947c0`，发行 ZIP SHA-256 为
  `A39BFAE92E25775A461DD7251A9D48DF15067812287650CCE65E72EA448050AD`。
- N10 规模场景的同身份 30 秒基线/复测均通过 Q1：帧 P95 为 2.631/2.707 ms、P99 为
  7.631/7.717 ms，两个样本都只有 1 帧超过 33 ms、1 帧超过 50 ms，容量事件为 0；24 个 actor、
  16 个 item、64 株 crop 和 8 个 chest 的夹具身份一致。固定 seed 20260825 的 nominal/stress
  各运行 60 秒、1200 fixed ticks 且零失败；stress 峰值为 72 个已有区块、52 个加载区块、
  42 个队列项、25 个 actor、27,668,480 字节私有内存和 233 个句柄。
- R3 只记录为部分真人自测完成，其余按用户决定延后；未生成十二项正式记录，不标为 `PASS`，
  也不阻塞 N11-N12。

## 明确不做

本批不做饥饿、材料组替换、品质/温度、多通道/多输出炉体、工具修理、自动化设备树、离线追赶
或通用动态材料 ID。任何需要新增持久字段或改变槽位结构的后续需求必须另立版本合同。
