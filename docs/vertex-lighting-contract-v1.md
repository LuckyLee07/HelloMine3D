# HelloMine3D Vertex Lighting Contract v1

> Architecture Lab 迁移说明（2026-08-31）：本文冻结 V10A 工程、性能例外和开发者视觉证据；
> 旧的真人产品体验边界现由 `AI-07` 与 `NOT_CLAIMED` 分类承接，详见
> `docs/ai-assisted-gameplay-acceptance-v1.md`。

本文冻结 Stage 10 `V10A` 的 CPU 顶点平滑光照、环境遮蔽、三角形划分和 greedy merge
边界。该合同只描述可重新生成的 mesh 数据，不进入世界保存、地形生成、玩家设置或资源身份。

状态：`Done`。合同、自动夹具、短 Q3、八场景 AO/no-AO Release 截图和开发者窗口
检查已完成；项目所有者于 2026-08-27 明确批准下述 Q1 性能例外，后续批次不得静默改变
本合同的 AO 强度、光照曲线或合成顺序。

## 身份与兼容

- `VertexLighting::ContractVersion = 1`，只作为源码合同常量，不持久化。
- save 保持 v11，terrain 保持 v3，settings 保持 v4，Q1 核心 schema 保持 3。
- terrain vertex 仍为 `position3 + atlasUV2 + repeatUV2 + light1`，总计 8 个 `float`、32 字节。
- AO 与平滑光照共同写入既有的单个 `float light`；不新增顶点流、attribute 或 shader 接口。
- 原有整面亮度重载仍保留，resource flora 等任意形状可继续使用统一亮度。
- `HELLOMINE3D_DISABLE_VERTEX_AO=1` 只用于同构截图和开发期诊断；它在进程内只读取一次，
  仅关闭 AO 乘数而保留四角平滑光照，不是玩家设置、保存字段或正式画质档位。
- `HELLOMINE3D_VERTEX_LIGHTING_FIXTURE=cave|canopy` 只构造确定性洞口/树冠截图夹具，不改变
  正常世界生成身份；非法值在 Ogre 启动时直接拒绝。

## 四角与采样

每个 cube face 使用规范化顺序：左下、右下、右上、左上。每个角从可见面外侧的 2×2
邻域读取四个样本：

1. `centre`：紧邻可见面的空气/透明体素；
2. `sideU`：沿第一条面切线移动一格；
3. `sideV`：沿第二条面切线移动一格；
4. `diagonal`：同时沿两条面切线移动一格。

所有坐标都落在现有 `SectionMeshInput` 的 `[-1, 16]` 18³ halo 内。未加载区块沿用世界读取
规则：方块为空气、天空光为最大值、方块光为最小值，因此未知邻域不形成永久黑墙。

## 遮挡与光照合成

- 只有 `BlockDefinition::transparent == false` 的方块遮挡 AO。
- 空气、水、玻璃、flora/resource shape 和其他透明方块不遮挡 AO。
- resource flora 不具备 cube face 邻域，保持方块中心统一亮度，也不会作为其他面的 AO 墙。
- 天空光与方块光在每个样本先取较大值；发光方块通过现有方块光传播自然进入顶点采样。
- 被判为 AO 遮挡物的侧边/对角样本不再把其内部零光重复计入平滑平均，避免光照与 AO 双重
  压暗；其余样本以 `lightLevelToBrightness` 转换后等权平均。

AO 等级使用下式：

```text
if sideU && sideV: ao = 3
else:              ao = sideU + sideV + diagonal
```

最终写入值为：

```text
finalLight = clamp(cardinalLight * smoothLight * (1 - 0.12 * ao), 0, 1)
```

方向系数保持既有值：顶面 1.0、X 面 0.8、Z 面 0.6、底面 0.4。本批不修改片元 shader 的
`shapedLight` 曲线，V10B1-B3 也不得静默重定该曲线所有权。

## 对角线与 greedy merge

- 每个 quad 比较 `0-2` 与 `1-3` 两条对角线端点的平滑光差、AO 差和最终光差，选择误差较小
  的一条；误差相等固定保留 `0-2`。
- `ChunkMesh` 的替代索引固定为 `0,1,3 / 1,2,3`，顶点顺序和 winding 不变。
- greedy 候选必须先保持方块 id/metadata 和 tile 相同，再使用候选大面的四个外角及所选对角线，
  对覆盖区域每个原始单元的四角重建 `smoothLight`、AO 和 `finalLight`。
- 任一原始角与分片线性插值不一致时立即停止扩张；允许真正平面梯度合并，不允许把重复或阶跃
  梯度错误拉伸到大面。

## 顶点复用与纹理重复

- AO 合同约束逻辑 quad 和三角形，不要求每个 quad 独占四份顶点。相同朝向的一组 solid faces
  可复用位置、tile、repeat UV 和最终 light 均逐位相同的顶点；faces 与六索引拓扑保持不变。
- solid greedy 面使用 section 内全局整数 repeat UV。terrain shader 仍以 `fract(repeatUV)` 重复
  tile，因此跨分片公共边得到相同属性；atlas UV 固定为同一 tile 内的规范坐标，只承担 tile
  选择，不改变实际采样像素。
- 复用缓存按六个面朝向分别开始，使用固定 17³ section-corner 索引和 generation stamp；它不在
  不同朝向、不同 tile、不同光照或不同 repeat UV 间合并，也不用于 water、glass 或 flora。
- 四角邻域样本另由每次 builder 私有的 18³ lazy cache 去重；uniform-light 矩形绕过完整重建扫描。
  两项缓存均为派生 CPU 临时数据，不进入上传顶点、保存格式或运行时身份。

## Dirty 传播

顶点 AO 会读取边和角邻居。边界编辑的刷新集合因此是所有受影响轴集合的笛卡尔积：普通内部
编辑只刷新 owner；单轴边界刷新 2 个 section；双轴边界最多 4 个；三轴角点最多 8 个。
未加载 section 仍不为刷新而强制创建。

## 自动验证

`HelloMine3DWorldRuntimeSmoke` 覆盖：

- AO 0/1/3、双侧强制封角、透明邻居、确定性 tie-break 和替代索引；
- 关闭 AO 时保留四角平滑光照，builder 显式覆盖会移除接触压暗；
- 四个独立 light 值仍使用 32 字节 vertex stride；
- 独立方块、L 形包角、跨 section 共享顶点与相反重建顺序字节一致；
- sunlight/block-light 旧夹具按顶点梯度判定，不再假设整面四值相同；
- greedy slab 只在重建兼容时合并，透明、水和 flora 继续保留原拓扑。

`HelloMine3DMeshDirtyTests` 另行覆盖水平边角和三轴角点的 4/8-section 刷新集合。

开发期可设置 `HELLOMINE3D_WORLD_SMOKE_FOCUS=V10A`，只运行 greedy、V10A、sunlight 和
block-light 相关的聚焦子集；未设置时仍运行完整世界回归。Stage 10 补充性能比较使用
`tools/compare_stage10_visual_performance.ps1`，其五项正反例由
`tools/validate_stage10_visual_performance.ps1` 固定，并已接入完整构建门禁。

V10A 只有在 VS2017/v141 Debug/Release、完整相关门禁、快速流送与规模玩法 Q1、短
nominal/stress Q3、八图矩阵和开发者真实窗口视觉检查全部记录后才能改为 `Done`。

## 2026-08-27 候选证据

- 共享顶点版本的 VS2017/v141 Debug 与 Release 完整世界回归均为 `714/714`，两种配置聚焦
  V10A 子集均为 `37/37`；最终代码增加两项 AO 关闭断言后，双配置聚焦子集为 `39/39`，
  Release 完整世界回归为 `716/716`。共享顶点夹具把同一 14-face slab 从未复用的 56 顶点
  压到 36 顶点，同时保持
  84 索引、纹理 tile 和 8 格 repeat span。MeshDirty 和 38 项资源包回归通过，隐藏真实客户端
  以 `exit_code=0` 退出且保持
  `terrain_vertex_stride_bytes=32`、`terrain_index_stride_bytes=4`。
- Stage 10 性能比较器的 5 项正反例通过。实际 schema 3 candidate 保持冻结 baseline 不变，
  仍不能自动接受。系统负载稳定后的 fast-streaming 核心旧预算为 `PASS`，frame P95/P99
  为 `7.814/12.118 -> 8.924/13.769 ms`（+14.2%/+13.6%），chunk-visible P95/P99
  反而从 `40.285/40.285` 降到 `37.276/37.276 ms`；但 Stage 10 的 10% 补充门槛仍判
  `REVIEW_REQUIRED`，mesh build avg 为 `0.541 -> 0.682 ms`（+26.1%）。同一 mesh 代码的
  近等 rebuild 快照为 `2,410 -> 2,414`，累计 solid vertices 为
  `949,784 -> 1,138,562`（+19.9%）；该快照 GPU-buffered sections 不相等，因此不把其
  resident 数解释为归一化改善。最终 scaled-gameplay 在相同 `361 chunks / 1,833 sections`
  下 frame P95/P99 为 `6.830/9.238 -> 7.899/10.396 ms`，P95 比旧核心上限 `7.854 ms`
  高 `0.045 ms`，故核心比较仍为 `REGRESSION`；mesh build avg 为
  `0.531 -> 0.654 ms`（+23.2%），resident vertices 为 `805,364 -> 892,013`（+10.8%），
  indices 为 `1,208,046 -> 1,712,448`（+41.8%），terrain buffer 为
  `30,603,832 -> 35,394,208 bytes`（+15.7%）。顶点复用已显著收窄首轮候选的 42%-58%
  顶点增长，但 exact AO 仍需要更多逻辑 quad/indices。关闭 AO、保留平滑光照的同场诊断把
  resident vertices/indices/buffer 降至 `764,805 / 1,210,812 / 29,317,008`，其中索引仅比
  冻结基线高约 `0.23%`；这证明主要索引增长来自 exact AO 必须保留的内部明暗边界，而不是
  顶点布局或无关的 greedy 退化。两种额外寻址优化分别实测为 `0.663 ms` 和
  `0.678/0.667 ms`，均不优于稳定实现的 `0.654 ms`，因此已回退，未把无收益复杂度并入主干。
- nominal/stress Q3 各运行 20 秒并通过；这只是开发期短探针，不替代 VISUAL-RC 的正式双
  1800 秒证据。
- `docs/screenshots/validation-v10a-*.png` 已归档森林正午、海岸正午、森林黄昏、森林夜晚、
  洞穴入口、树冠下方、遗迹墙角和营地夜景共八组 AO/no-AO Release RuntimeReadback（16 张）。
  海岸镜头从 FS2 原始存档恢复为 `(285,67,40)` / `(12,270,0)`，不再误用森林朝向。逐对检查
  可见角部与墙地接触层次，未见黑缝、破面、永久黑边或透明树叶误遮蔽；另一次非隐藏真实
  窗口洞口读回通过，开发者视觉检查记为 `PASS`。该检查不替代 VISUAL-RC 的真人产品体验。
- 一次构建后高负载 fast-streaming 探针曾因只到 `298 chunks / 1,514 sections` 而被正确判为
  `INCOMPARABLE`；系统稳定后复测恢复到 `318 / 1,618`，落在冻结基线 `320 / 1,624` 的可比
  容差内并得到上方结论。项目所有者于 2026-08-27 接受墙地接触、洞穴边缘和植被层次的
  玩家可见收益，明确回复“我批准 V10A 性能例外，并同意将 V10A 标记为 Done，继续 V10B1”。
  fast-streaming 旧核心预算仍通过，scaled P95 只超旧上限 `0.045 ms`，但上述
  mesh/geometry/residency 增幅必须继续作为 VISUAL-RC 最终身份的正式 Q1/Q3 观察项。
  V10A 据此关闭为 `Done`，不重采 baseline、不放宽 schema 3 绝对预算，也不把例外自动
  继承给后续视觉批次。
