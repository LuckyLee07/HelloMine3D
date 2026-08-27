# HelloMine3D Vertex Lighting Contract v1

本文冻结 Stage 10 `V10A` 的 CPU 顶点平滑光照、环境遮蔽、三角形划分和 greedy merge
边界。该合同只描述可重新生成的 mesh 数据，不进入世界保存、地形生成、玩家设置或资源身份。

状态：`In Progress`。合同、自动夹具、短 Q3 和首轮 Release 候选截图已实现；Q1 已得到明确
回归结论，尚未获性能例外批准，洞口/树冠截图和开发者视觉检查仍待完成。

## 身份与兼容

- `VertexLighting::ContractVersion = 1`，只作为源码合同常量，不持久化。
- save 保持 v11，terrain 保持 v3，settings 保持 v4，Q1 核心 schema 保持 3。
- terrain vertex 仍为 `position3 + atlasUV2 + repeatUV2 + light1`，总计 8 个 `float`、32 字节。
- AO 与平滑光照共同写入既有的单个 `float light`；不新增顶点流、attribute 或 shader 接口。
- 原有整面亮度重载仍保留，resource flora 等任意形状可继续使用统一亮度。

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

## Dirty 传播

顶点 AO 会读取边和角邻居。边界编辑的刷新集合因此是所有受影响轴集合的笛卡尔积：普通内部
编辑只刷新 owner；单轴边界刷新 2 个 section；双轴边界最多 4 个；三轴角点最多 8 个。
未加载 section 仍不为刷新而强制创建。

## 自动验证

`HelloMine3DWorldRuntimeSmoke` 覆盖：

- AO 0/1/3、双侧强制封角、透明邻居、确定性 tie-break 和替代索引；
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

- VS2017/v141 Debug 与 Release 完整世界回归均为 `714/714`，Release 聚焦 V10A 子集为
  `37/37`；MeshDirty 和 38 项资源包回归通过，隐藏真实客户端以 `exit_code=0` 退出且保持
  `terrain_vertex_stride_bytes=32`、`terrain_index_stride_bytes=4`。
- Stage 10 性能比较器的 5 项正反例通过。实际 schema 3 candidate 保持冻结 baseline 不变，
  但旧绝对门禁判为 `REGRESSION`：fast-streaming 的 frame P95/P99 为
  `7.814/12.118 -> 11.656/15.729 ms`，solid vertices 为
  `949,784 -> 1,501,224`；scaled-gameplay 的 frame P95/P99 为
  `6.830/9.238 -> 10.041/12.555 ms`，resident terrain vertices 为
  `805,364 -> 1,141,196`。原因是 AO/光照变化边界按本合同禁止错误大面合并，当前收益与
  约 42%-58% 的关键几何增长尚未形成批准结论，因此不得关闭 V10A。
- nominal/stress Q3 各运行 20 秒并通过；这只是开发期短探针，不替代 VISUAL-RC 的正式双
  1800 秒证据。
- 隐藏 Release RuntimeReadback 已采集森林正午、海岸正午、森林黄昏、森林夜晚、遗迹墙角
  和营地夜景，未见黑缝、破面或未加载边界永久变黑。洞口与树冠下方 candidate、两类新增
  场景的无 AO 对照以及开发者真实窗口主观检查仍待完成。
