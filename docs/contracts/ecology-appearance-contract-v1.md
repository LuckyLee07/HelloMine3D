# V10B3 生态外观合同 v1

## 状态与范围

V10B3 的 Windows 工程、自动回归、性能探针和静态视觉检查已经完成，状态为 `Done`。工程身份为
`706acafab3849100b86334e2914295d9ebcdbe64`；开发者视觉记录为
`docs/reports/developer-visual-record-v10b3.txt`，并以 `-RequirePass` 校验通过。

本批只改变世界网格选择的草顶、草侧、橡树叶、水和高草图块。矿物、木材、交互方块、草底、
HUD、容器图标和手持物继续使用 V10B2 基础图块。方块 id、掉落、世界生成、save v11、
terrain v3、settings v4、32 字节 terrain vertex 和 4 字节 index 均不变；不新增 shader、
持久化设置或资源包覆盖语义。

## 选择与确定性

权威入口为 `TerrainAppearance`，合同版本为 1：

- 每个生态/表面组固定 3 个变体，世界坐标按 `4 x 4 x 4` 小块选择变体；这样同一小块内部仍可
  greedy 合并，又不会形成逐方块棋盘噪声。
- 选择哈希以 terrain seed、向下取整后的三维小块坐标和 block id 为输入，采用固定宽度
  FNV-1a 混合与最终 avalanche；负坐标显式执行 floor division。
- 同 seed/坐标/block 的结果不受 section 创建顺序、邻块加载顺序、重开或线程调度影响。
- 选中的 tile 坐标进入 `appearanceKey`。只有几何、材质、光照/AO 与外观 key 都相同的面才可
  greedy 合并；跨变体边界不得错误共享纹理。
- `SectionMeshInput` 快照 `18 x 18` biome halo 和 terrain seed，greedy、非 greedy cube 与资源
  flora 使用同一选择函数，网格构建过程不回读可变世界状态。

## 图集布局与颜色边界

`Base.terrain-atlas` 保留 V10B2 的前 37 个基础条目，并增加 75 个只供世界网格使用的条目，
总数为 112。生态行和列固定如下：

| 行 | 生态 | 草顶 | 草侧 | 橡树叶 | 水 | 高草 |
| --- | --- | --- | --- | --- | --- | --- |
| 3 | Desert | 0-2 | 3-5 | 6-8 | 9-11 | 12-14 |
| 4 | Grassland | 0-2 | 3-5 | 6-8 | 9-11 | 12-14 |
| 5 | LightForest | 0-2 | 3-5 | 6-8 | 9-11 | 12-14 |
| 6 | TemperateForest | 0-2 | 3-5 | 6-8 | 9-11 | 12-14 |
| 7 | Ocean | 0-2 | 3-5 | 6-8 | 9-11 | 12-14 |

颜色变化只作用于原 tile 的可见 RGB，Alpha 原样保留；草侧的泥土主体保持中性，不随生态
整体染色。草顶、叶和水允许四分之一转向及 `1.00/1.02/0.98` 的克制亮度差；草侧和高草只
允许水平镜像，避免根部、切边或生长方向被翻到错误位置。最终 `DefaultPack.png` SHA-256 为
`6EA21822989D7AF95E5A2DBE816365CC9E4BDA9573AD53591A5925FAF9050B52`。

## 自动证据

- `tools/validate_terrain_atlas.ps1`：261 项通过、0 失败；覆盖基础条目冻结、75 个生态条目、
  Alpha/中性泥土边界、变体变换、图集重建哈希和世界/UI 路径分离。
- VS2017/v141：受影响客户端与 WorldRuntimeSmoke 的 Debug/Release 均成功；Release
  WorldRuntimeSmoke 为 732/732，V10B3 聚焦夹具双配置均为 20/20。
- `tools/validate_resource_packs.ps1`：ResourcePackSmoke 56/56、resolver 28、startup 2、
  manifest entries 67；`tools/validate_startup_errors.ps1` 为 13/13。
- Stage 10 性能补充验证 7/7；资源 manifest 变化只能通过显式
  `-AllowResourceManifestChange` 桥接，且只归一化 core comparator 的 manifest 字段，其余身份、
  绝对预算和几何/网格/驻留指标继续严格比较。
- Q3 短探针：nominal/stress 各 20 秒、400 tick、0 失败；稳定段 private bytes 增长分别为
  737,280 / 2,244,608，handle 增长均为 0。正式双 1800 秒留到 VISUAL-RC 最终身份执行。

第一次普通 Release `Build` 因 Debug 可执行文件时间戳更新而复用了共享 `bin` 输出；该次摘要
明确显示 `build_configuration=Debug`，已废弃且未进入证据。随后以 VS2017/v141 强制 Release
重建客户端、WorldRuntimeSmoke 和 Soak，并重新取得本合同全部最终运行证据。

## 性能判断

V10B3 改变图集 manifest 与网格输出，不能把 BETA-RC 的补充几何差异直接解释为本批退化。
因此保留两层对照：

1. 冻结 BETA-RC 仍是 core 身份和绝对预算权威。显式 manifest 桥接后 core 通过；补充报告保留
   V10A 已批准的网格/驻留例外，不把它重新包装成 V10B3 例外。
2. `stage10-v10a-approved-reference-v1` 是 V10A 已批准候选的增量参考，不替代冻结基线。
   scaled 场景两次采样均达到 361 loaded chunks、1,833 sections、1,292 GPU-buffered sections，
   因而可直接比较本批新增成本。

scaled 增量结果全部在 10% 内：frame P95/P99 为 -6.229%/-4.502%，mesh avg/max 为
+8.410%/-4.158%，solid/transparent/water/flora vertices 为
+0.182%/-14.286%/-1.078%/-5.357%，resident vertices/indices/buffer 为
+0.148%/+0.069%/+0.132%。V10B3 不新增性能例外。

fast 场景的帧时间与可见延迟在阈值内，但 V10A 参考仅有 653 个 GPU sections、候选有 825 个，
其进度相关几何/驻留值不可直接归因；报告保留 `REVIEW_REQUIRED` 原样，不用于掩盖或关闭指标。

## 固定画面与视觉结论

五张图片均来自同一真实隐藏 Release `RuntimeReadback`，1280x720，seed 20260807，time 6000；
自动窗口保持隐藏，未抢占用户焦点。流送尚未稳定的早期 RD8 图片已拒绝且未纳入仓库。

| 画面 | 机位与范围 | SHA-256 |
| --- | --- | --- |
| `validation-v10b3-desert-noon.png` | pos `96 87 0`，rot `35 225 0`，RD2，20s | `6992279D289995A930E804E1816169954A99F333C45F7701F7224C8D97D3C9B5` |
| `validation-v10b3-grassland-noon.png` | pos `928 105 0`，rot `25 225 0`，RD3，30s | `1CD2189335235E7C8E35FE604C3EAE917F95318316271AA4D9FE5379F30C2E84` |
| `validation-v10b3-light-forest-noon.png` | pos `1312 152 0`，rot `35 225 0`，RD2，20s | `1A07DBFC49E34DF685630C1F1D52D98ED020F6E01D48A5581970985413EC5B48` |
| `validation-v10b3-temperate-forest-noon.png` | pos `1248 112 0`，rot `35 225 0`，RD2，20s | `D0FAAE6586EA637873F3C2D6CF24F3EA690C9DD778DDC20FC7528CA65276FCBD` |
| `validation-v10b3-ocean-noon.png` | pos `1792 80 -144`，rot `20 45 0`，RD2，20s | `1211C3C03E8ADE01241BAB62179CA1E21D1D721A02AE31E48917DC916092DC97` |

审阅者按原始分辨率逐图检查，结论为 `PASS`：同一 4 格小块内纹理连续、跨小块方向有变化，
没有逐 tile 棋盘；草地、疏林与温带林的生态差异可辨；叶片 Alpha 正常，水面连续，无接缝、
黑块或明显重复带；草侧泥土仍保持中性；HUD/手持身份没有漂移。

疏林图左侧的小型青色对象与同帧敌人警告、受伤效果和 5 点生命状态一致，是飞行敌人的投射物，
不是图集泄漏。隐藏离屏截图上的瞬时 FPS 不作为玩家窗口性能结论，Q1 摘要才是权威性能证据。
V10B3 只包含静态 tile 选择，不声明动画、相机运动或时间稳定性已由单帧覆盖。
