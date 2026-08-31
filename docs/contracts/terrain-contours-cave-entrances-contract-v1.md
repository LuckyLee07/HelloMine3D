# P11-2 地形轮廓与天然洞口合同 v1

> Architecture Lab 迁移说明（2026-08-31）：工程合同保持冻结；远景轮廓、洞口可见性和实际
> 通行映射到 `docs/current/ai-assisted-gameplay-acceptance-v1.md`。人类风景审美为 `NOT_CLAIMED`。

状态：`Engineering Done`。确定性、版本隔离、保存恢复、双配置构建、自动回归和最终 Q1 已关闭；
AI 远景/洞口交互与视觉场景为 `NOT_RUN`，不登记为自动 PASS。

## 目标与范围

P11-2 只改变新建世界的地形生成身份，使远景拥有可辨识的高地轮廓，并让地下洞穴可从地表被
发现。它不修改 Stage 10 的 shader、材质、后处理或阴影合同，也不引入湖泊、河流、瀑布、天气、
体积光、地形破坏物理或新的渲染后端。

## 版本身份与兼容

| 身份 | 数值 | 行为 |
| ---- | ---- | ---- |
| Legacy terrain | v1 | 保留原始地形输出。 |
| Waystone terrain | v2 | 保留原始路标生成身份。 |
| Exploration-site terrain | v3 | 保留遗迹/营地时代的高度、生态和结构输出。 |
| Mountain terrain | v4 | 新世界默认；增加山地高度域和天然洞口。 |

- terrain 版本只在末尾追加；未知的 v0/v5 及更高版本严格拒绝。
- world save 继续使用 v12，并继续显式保存 `terrain_generation_version`。P11-2 不新增持久字段。
- v1-v3 世界永久使用创建时的生成器；不会迁移为 v4，也不会给未加载区块回填山地或洞口。
- 已保存区块以自身方块数据为准。世界 metadata 只有一个地形身份，因此运行时不会把 v3/v4
  生成器混用到同一世界；手工拼接或外部改写造成的边界不由运行时静默修复。
- 历史 terrain v3 固定样本、结构夹具和保存重开继续走显式 v3，防止当前默认值升级掩盖回归。

## 山地高度域

- `TerrainBiome::Mountain` 追加在既有五种生态之后，旧枚举数值不变。
- v4 以世界有符号坐标、seed 和固定 salt 计算三层确定性 value noise：宽域决定山系范围，ridge
  决定主轮廓，detail 只提供有界局部变化。负坐标使用 floor 语义，不依赖区块加载顺序。
- 山地判定要求强度至少 `0.48` 且地表至少高于水位 16 格；最终高度严格限制在 1..176。
- 高于 `WATER_LEVEL + 36` 的山地顶层与浅层使用 Stone，且山地不生成树木或植物，避免远景轮廓
  被装饰物遮蔽。
- 山地复用 TemperateForest 的受控外观行，不增加图集身份；自然生成压力映射到既有 Brute，
  不在本批增加敌人或资源经济。
- 既有遗迹/营地生态资格不扩展到 Mountain；路标继续服从既有确定性规划器。

## 洞穴与天然入口

- v1-v3 普通洞穴继续只生成到 `min(surface - 5, WATER_LEVEL - 8)`，输出保持冻结。
- v4 普通洞穴可接近到 `min(surface - 5, WATER_LEVEL + 24)`，但仍保留五格地表缓冲。
- 天然入口只在 v4 Mountain 中生成。世界被划分为 96×96 格 cell，每 cell 最多检查 12 个
  确定性候选；候选必须高于水位至少 16 格。
- 入口选择四个基准方向中覆盖最高的方向，并要求 24 格后的目标仍属于 Mountain 且地表至少
  高 2 格。由此得到一条长 24 格、宽 3 格、高 3 格、每四格下降三格的通道。
- 通道末端使用半径 3 的有界终室；雕刻不会写入 Water，不会越过世界底部。
- 每个区块从会影响自身的世界 cell 重新计算同一入口计划。入口所有权由世界坐标决定，因此正序、
  逆序或并行加载区块不会改变输出，也不会同步加载邻区块。

## 自动验收证据

2026-08-30 的 VS2017/v141 验证结果：

- Debug/Release P11-2 焦点各 `9/9`；terrain v3 固定高度/生态样本保持不变。
- seed `20260807` 的采样高度范围为 `22..176`，Mountain 样本 `4829`；相邻 seed 有 `13240`
  个高度样本不同，证明确定性与 seed 敏感性同时成立。
- 固定入口 cell `(-3, 3)` 得到 anchor `(-227, 105, 368)`；实际生成通道抽样 77 个点从地表
  连通终室，逆序区块生成逐块一致。
- save v12 的 terrain v4 与显式 terrain v3 均保存/重开保持原身份，未知 v5 被拒绝。
- Release 完整 `HelloMine3DWorldRuntimeSmoke` 为 `820/820`；Recipe `121/121`、ResourcePack
  `80/80`、WorldCatalogue `59/59` 在 Debug/Release 均通过。
- 主客户端 Debug/Release 均成功使用 VS2017 v141 工具链编译链接；仅存在既有 OGRE/FreeImage
  编码和宏重定义警告。

## AI/Computer Use 验收边界

当前 Windows 会话为断开状态（WTS connect state 4）。正式采集器因此拒绝生成硬件性能结论，
而不是使用无效桌面伪造 fast-streaming/scaled-gameplay PASS。恢复活动桌面后需要：

1. 在相同 terrain v4 身份下各采集 fast-streaming 与 scaled-gameplay baseline/repeat，再按既有
   Stage 10 阈值比较，不继承或新增性能例外。
2. 按 `AI-05` 在多个固定和随机 seed 中实际寻找山地与洞口，记录发现时间、是否可步行进入、
   是否存在卡死点。
3. 观察远景轮廓、雾与方向阴影是否获得清晰层次，并检查水面、结构和旧 v1-v3 世界是否出现
   明显边界异常。

性能复采和 AI 场景当前均为 `NOT_RUN`，不能在执行前写为 PASS；人类风景审美为
`NOT_CLAIMED`。
