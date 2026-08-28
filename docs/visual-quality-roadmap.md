# HelloMine3D Stage 10 视觉质量路线

本文把 BETA-RC 之后的画面升级拆成可独立实现、验收和回滚的批次。它只参考 Luanti 等成熟
体素引擎的公开设计与算法边界，不复制 Minecraft、Luanti 或第三方项目的美术资产，也不把
视觉升级扩大成渲染后端重写。

最后更新：2026-08-28。

## 当前结论

现有 FS2/FS3 已完成程序化天空、独立水面、昼夜雾色、统一 16x16 图集、HUD、手持物和基础
交互反馈；区块侧也已有 18x18x18 邻域快照、opaque cube greedy mesh、solid/water/flora
分层、后台网格构建和正式 Q1/Q3 预算。因此 Stage 10 不重做这些基础，集中解决以下观感缺口：

1. 地形当前按顶/侧/底整面使用固定方向亮度，同一大面缺少顶点级接触阴影，墙角、树冠、
   洞穴和结构接缝显得平。
2. V10B1-B3 已把图集/颜色参数、原创分面资产、生态 tint 和确定性变体接入同一严格材质合同。
3. V10C 已让地形、水、actor 与天空共享定向雾，并用独立有界云层提供视差、厚度和云底层次。
4. V10D/V10E 已增加可关闭、可回退的方向阴影和轻量后处理；VISUAL-RC 已完成 Windows
   最终身份、性能、资源、许可证、发行包和工程图封板，macOS 原生与正式产品体验保持 Verify。

## 参考与许可证边界

- Luanti 引擎代码采用 LGPL-2.1-or-later，许可证见
  <https://github.com/luanti-org/luanti/blob/master/LICENSE.txt>。本项目当前没有统一顶层许可证，
  默认只做独立实现；任何源码级移植必须先单独评估许可证、保留声明和源码提供义务。
- 顶点平滑光照与邻域 AO 只参考公开算法职责，入口见
  <https://github.com/luanti-org/luanti/blob/master/src/client/mapblock_mesh.cpp>。
- 天空、云、太阳/月亮参数化只参考职责拆分，入口见
  <https://github.com/luanti-org/luanti/blob/master/src/client/sky.cpp>。
- 图形质量分级只参考开关与预算思路，入口见
  <https://github.com/luanti-org/luanti/blob/master/builtin/settingtypes.txt>。
- 纹理覆盖只参考资源命名和按面覆盖思路，入口见
  <https://github.com/luanti-org/luanti/blob/master/doc/texture_packs.md>。
- 禁止复制 Minecraft/Luanti 的纹理、模型、声音、UI、截图裁片或标志性角色；所有正式素材
  必须为项目原创或具有明确兼容许可证，并登记来源、作者、许可证和 SHA-256。

## 批次总览

| 顺序 | 批次 | 优先级 | 规模 | 状态 | 玩家可见结果 |
| ---- | ---- | ------ | ---- | ---- | ------------ |
| 1 | V10A 顶点平滑光照与 AO | P0 | M/L | Done | 方块接缝、墙角、洞穴、树冠和结构落地位置具有稳定层次，不再只有整面明暗。 |
| 2 | V10B1 材质与图集管线 | P0 | M | Done（macOS Verify） | 图集尺寸、tile 尺寸和颜色管线不再由 shader 魔数隐式决定，错误资源能够明确失败。 |
| 3 | V10B2 原创材质资产 | P0 | L | Done | 世界、HUD 和手持物使用来源明确、像素密度与分面一致的原创材质。 |
| 4 | V10B3 生态着色与确定性变体 | P1 | M | Done | 草、叶、水拥有受控生态 tint，自然地表重复感降低且不改变 terrain 身份。 |
| 5 | V10C 定向大气与立体云 | P1 | M/L | Done（Windows；macOS Verify） | 日出/日落方向、远景雾、云层视差和云底明暗形成更有深度的天空。 |
| 6 | V10D 可选方向阴影 | P1 | L | Done（Windows；macOS Verify） | 玩家、实体、结构和植被在近景获得可配置、可关闭的太阳阴影。 |
| 7 | V10E 轻量后处理 | P2 | M/L | Done（Windows；macOS Verify） | 在不改变像素风格的前提下统一亮部、暗部和色带，按能力安全降级。 |
| 8 | VISUAL-RC 视觉集中封板 | P0 | M | Done（Windows 工程；macOS/产品体验 Verify） | 固定截图、性能、资源、许可证和干净包形成新的可比较视觉基线。 |

执行顺序固定为
`V10A -> V10B1 -> V10B2 -> V10B3 -> V10C -> V10D -> V10E -> VISUAL-RC`。
V10D/V10E 是可关闭的高成本项，但在实现前不能通过删除 V10A、V10B1/B2 或 V10C 来换取
排期。每批完成后单独本地提交；默认不 push。

## 验收层级

Stage 10 的主观判断不并入 R3 v1。物理输入路线只负责真实键鼠、窗口焦点和玩家输入链；视觉、双语可读性
与听感属于独立产品体验证据，记录格式见 `docs/manual-product-experience-acceptance-v1.md`。每个
视觉批次按下面三层记录：

1. **自动证据**：确定性夹具、shader/资源负例、固定截图、性能和受影响构建门禁；采集过程不
   自行生成主观结论。
2. **开发者视觉检查**：项目所有者或其明确授权的审阅者检查真实 Release 证据，记录提交、配置、
   GPU、窗口、图形档、场景、`PASS/FAIL` 和一句理由。静态批次可用至少三张原尺寸固定图；涉及
   云运动、闪烁、相机运动或窗口变化的批次必须有多帧/视频或连续窗口观察。它不要求 R3 输入
   签字，是关闭 V10A、V10B1-B3 与 V10C-V10E 的主观依据。
3. **正式产品体验验收**：在 VISUAL-RC/发行封板集中记录视觉风格、双语可读性和听感；它与
   后续 Physical Input v2 共享构建身份但不共享检查表，任一项都不能伪装成自动 `PASS`。

## V10A：顶点平滑光照与 AO

实现合同见 `docs/vertex-lighting-contract-v1.md`。四角光照/AO、greedy 重建保护、边角 dirty
传播、共面顶点复用和自动夹具已经落地；短 Q3、八场景 AO/no-AO 截图和开发者窗口检查已
通过。项目所有者于 2026-08-27 明确批准记录在合同中的 Q1 性能例外，V10A 状态为 `Done`；
该例外只覆盖当前 exact AO 身份，不自动覆盖后续视觉批次；最终 Q1/Q3 已在 VISUAL-RC 重跑通过。

玩家问题：当前 `ChunkMeshBuilder` 为每个面写入一个亮度值，顶、X、Z、底面分别使用固定系数；
即使太阳光传播正确，面内四个角仍完全一致，邻接方块也不会在角落产生接触阴影。

必须完成：

1. 定义版本化但不持久化的 `VertexLighting`/AO 计算合同。每个可见面的四个角分别检查两个
   侧邻居与一个对角邻居，得到确定性的 0-3 遮挡等级；两个侧面同时遮挡时，对角不得漏光。
2. 把邻域天空光、方块光和 AO 合成为四个顶点亮度；透明、flora、水、发光方块和未知邻域
   必须有明确规则，不能把未加载区块当成永久黑墙。
3. 跨 section/chunk 边界使用现有 18x18x18 快照，同一世界顶点从任一相邻 section 计算必须
   得到相同结果，不产生亮缝或暗缝。
4. 复用现有每顶点 `float light` 属性和 32 字节 terrain vertex stride，把每个逻辑面的四个角从
   相同值改为独立值；不得为 AO 新增第二套顶点流或升级顶点格式。position、tile、repeat UV
   和最终 light 逐位相同的共面角允许通过 index 复用，但不得跨不兼容属性或朝向错误合并。
5. greedy mesh 只有在合并后的四角值与确定性三角形划分能够重建被覆盖区域全部原始 AO/光照
   采样时才允许合并；无法重建就在变化边界切分。禁止仅因每个单元的四元组相同而把重复梯度
   拉伸到大面，也不要求所有可合并面的四角必须完全等值。
6. 对角线按四角 AO/光照插值误差确定性选择；误差相等时使用固定 tie-break。相同 seed、加载
   与重建顺序必须得到相同索引，不能继续依赖永远固定的 `0,1,2 / 2,3,0`。
7. 不升级 save、terrain、设置、资源或性能 comparison schema；不修改世界生成、碰撞、方块
   透明性和游戏逻辑。

最低自动验证：

- 独立、边贴边、L 形、三面包角、对角缺口、洞口、透明邻居、发光邻居和 section 边界夹具；
- 加载顺序、重建顺序和相机方向变化后顶点结果字节一致；
- 破坏/放置边界方块只使必要 section dirty，邻区块光照与网格及时刷新；
- Debug/Release 世界运行时、MeshDirty、资源包和隐藏真实客户端通过；
- 快速流送与规模玩法 Q1 candidate 对现有 BETA-RC schema 3 冻结基线比较，不升级 schema、
  不重采 before，也不放宽现有绝对预算。冻结摘要已经包含 `last_mesh_build_avg/max_ms`、各 pass
  顶点、驻留顶点/索引和 `last_resident_terrain_buffer_bytes`；Stage 10 补充比较器直接消费这些
  既有字段。
- P95/P99、顶点数、索引数、网格构建耗时、驻留 terrain buffer 和可见延迟均进入补充报告。
  退化不超过 10% 且旧绝对预算通过可自动接受；超过 10% 必须记录原因、玩家收益和批准结论，
  未获批准不得关闭。另立 Stage 10 绝对上限时必须在 V10A candidate 前冻结，不能实现后倒推。
  比较入口固定为 `tools/compare_stage10_visual_performance.ps1`：它先调用既有 schema 3 核心
  比较器验证身份和绝对预算，再读取同一份 summary 的 mesh/geometry/residency 字段；不复制、
  改写或重新采集 BETA-RC baseline。
- V10A 完成时运行短 nominal/stress Q3 探针；正式双 1800 秒 Q3 已在 VISUAL-RC 与最终代码身份
  一次执行通过。Q3 证明后台 CPU 网格/驻留生命周期，不代替真实 Ogre 客户端的 GPU buffer/Q1。

截图矩阵：沿用 FS2 固定森林正午、海岸正午、森林黄昏、森林夜晚，新增同 seed 洞穴入口、
树冠下方、遗迹墙角和营地夜景。截图必须来自隐藏 Release Ogre RuntimeReadback，并保存
无 AO 基线与 AO candidate；自动截图证明渲染确定性，不替代真人主观评价。

退出条件：所有角落夹具和跨区块接缝通过；四组原基线不出现闪烁、破面或过黑；新增四组中
墙地接触、洞穴边缘和植被层次清晰可见；相关 Q1 为 `PASS`，开发者视觉检查为 `PASS`，工作区
无临时 shader/截图产物。

## V10B1：材质与图集管线

实现合同见 `docs/terrain-material-profile-contract-v1.md`。严格 v1 profile、CPU/GPU 半像素
中心、三类 terrain pass uniform 同步、资源包覆盖和启动预检已经落地；默认 256/16/16 与
V10A 静态前景 216,000 像素完全一致。VS2017/v141 双配置资源包为 54/54、V10B1 世界聚焦为
4/4，Release 完整世界为 718/718；12 类真实客户端启动失败、65 项 manifest、真实 GL3Plus
启动和 5 分钟隐藏 Release 观察通过。Windows 工程状态为 `Done`，macOS Release 编译/窗口
因当前无目标机器保持 `Verify`；该原生证据需在 macOS 机器补齐，不由 Windows VISUAL-RC 替代。

玩家问题：固定图集和全局颜色修正让草、叶、土、木、石与建筑材料缺少统一但可辨的色彩层级，
连续地表又重复同一 tile，远看容易形成明显棋盘纹理。

必须完成：

1. 把 atlas pixels、tile pixels 和 tiles-per-row 从 GLSL 常量移到经过资源校验的材质参数；
   非法尺寸、越界 tile、非整数分格和缺失图集必须在启动预检中明确失败。
2. 把统一降饱和、压绿色与 tone/gamma 常量改成有名称、有范围、有默认值的材质参数；本批只
   建立管线和兼容默认值，不制作正式新图集。
3. 固定 AO/光照曲线所有权：V10B1-B3 默认不得改变 V10A 的 AO 强度、`shapedLight` 最暗值或
   合成顺序；确需改变时必须重新执行 V10A 夹具、八图矩阵、开发者检查和性能比较。
4. 保持当前 256x256、16x16 图集完全兼容，资源包与缺失/非法参数必须有确定失败语义。

退出条件：参数正反例、旧图集像素兼容、shader 启动负例、Windows 双配置、真实窗口与开发者
视觉检查通过；修改 shader 后必须在真实 macOS Release 上完成编译和窗口启动，否则该批跨平台
状态保持 `Verify`。

## V10B2：原创材质资产

1. 建立材质表现合同：统一像素密度、明度阶梯、噪声尺度、轮廓强度和 top/side/bottom 分面。
2. 至少覆盖草、土、石、沙、木、叶、水、矿物、箱子、工作台、熔炉、路标、遗迹和营地；
   世界、HUD 和手持物必须保持同一材质身份和近景/中景可读性。
3. 所有新图进入生成脚本、manifest、资源包负例、来源记录和许可证清单；禁止手改最终 atlas，
   禁止复制 Minecraft、Luanti 或纹理包作者资产。

退出条件：资源 manifest、图集坐标/透明边界、双语名称、HUD/手持一致性、固定截图、相关 Q1、
干净包与开发者视觉检查通过。资产变化不自动触发 settings、save 或 terrain 升版。

工程结果（2026-08-27）：`Base.terrain-atlas` 固定 37 个语义 tile、四类 Alpha、填充色与双语名；
内置 imagegen 三步母版经确定性脚本生成 `DefaultPack.png`，独立验证为 106/106。HUD/手持 UV
已接入 V10B1 冻结 profile。VS2017/v141 双配置受影响目标、ResourcePackSmoke 56/56、Release
WorldRuntimeSmoke 718/718、13 类启动负例、67 项 manifest、资源包与 85 文件干净包通过；森林、
遗迹、营地三张隐藏 Release 图未见接缝、黑材质或透明漏色。项目所有者于 2026-08-27 明确授权
Codex 承担视觉结论；三张 1280x720 原尺寸图经逐张检查，材质分面、HUD/手持一致性、透明边界、
曝光和身份均通过，记录校验为 `PASS`。规则重复感留给 V10B3；本批状态为 `Done`。完整实现、
图像哈希和结论见 `docs/material-visual-contract-v1.md`。

## V10B3：生态着色与确定性变体

1. 以受控生态 tint 替代对所有材质统一压色；草、叶和水可着色，矿物、木材、交互方块和 UI
   图标必须保持稳定身份。
2. 对指定自然方块提供少量、坐标确定性的 tile 变体；相同 seed/坐标、加载顺序和重开结果
   一致，不升级 terrain 身份，不改变掉落或方块 id。
3. tile 变体进入 greedy merge key；不得为了保留大面合并而让相邻世界坐标错误共享变体。

退出条件：生态/坐标/加载顺序夹具、近景/中景矩阵、相关 Q1、开发者视觉检查和短
nominal/stress Q3 探针通过；正式 Q3 已在 VISUAL-RC 通过。

工程结果（2026-08-27）：世界专用的五生态、五表面组、每组三变体已经接入 4x4x4 坐标小块，
选中 tile 进入 greedy identity；save v11、terrain v3、settings v4 与 32 字节顶点保持不变。
图集合同 261/261、Release 世界 732/732、聚焦双配置 20/20、资源包/启动负例、相关 Q1 和短
Q3 均通过。五张 1280x720 隐藏真实 Release 原图由授权审阅者逐图检查，生态差异、局部连续性、
透明边界、水面与 HUD 身份均通过，记录为 `PASS`。完整合同见
`docs/ecology-appearance-contract-v1.md`；该批关闭后进入 V10C。

## V10C：定向大气与立体云

玩家问题：当前昼夜颜色连续但雾只按距离变化，云没有独立空间层，因此转动相机、登高或观察
地平线时缺少方向层次和移动视差。

必须完成：

1. 在现有 `WorldEnvironmentState` 上增加与太阳方向相关的地平线散射参数；朝日出/日落方向
   适度偏暖，背光方向保持冷色，地形、水、actor 与天空共享同一雾语义。
2. 把云从无限远 skybox 噪声升级为有界独立云层或等价的可产生视差方案；支持覆盖率、速度、
   高度、厚度、亮面和云底色，并在相机进入云层时稳定处理。
3. 保持无天气、无体积光、无云体碰撞；GPU 能力不足或显式验证回退开关启用时退回当前 FS2
   路径。V10C 不新增持久化用户设置，正式图形设置版本从 V10D 开始。

最低验证：森林/海岸在正午、黄昏、夜晚及高处观察的固定截图；地形/水/actor 雾色一致；
云层移动不依赖帧率；隐藏客户端、相关 shader 负例、快速流送/规模玩法 Q1、开发者视觉检查
和真实 macOS Release 编译/窗口启动通过；无目标 Mac 时跨平台状态保持 `Verify`。

工程结果（2026-08-28）：定向雾已由 terrain/water/actor/sky 共享，有界云层固定高度、厚度、
绝对时间速度和下方/层内/上方边界；显式 FS2 回退与启动前 shader 接口负例均通过。VS2017/v141
双配置聚焦为 21/21，Release 完整世界为 741/741，资源包 65/65、启动负例 14/14、manifest
67 项。十张 1280x720 隐藏 Release 原图由 Codex 按原尺寸及正午多帧序列检查，记录为 `PASS`。
规模 Q1 与 V10B3 的几何/驻留身份完全一致；快速流送 frame P95/P99 只增加
4.645%/4.340%，但单样本区块可见延迟增加 22.304%。项目所有者批准该 V10C 例外：绝对值
49.636 ms 远低于 1000 ms 预算，复测与强制 FS2 A/B 不支持归因于新云 shader。例外不由后续
批次继承。完整证据见 `docs/directional-atmosphere-contract-v1.md` 和
`docs/developer-visual-record-v10c.txt`；随后完成的 V10D 不继承该例外，macOS 子状态保持
`Verify`。

## V10D：可选方向阴影

玩家问题：没有太阳投影时，玩家、actor、植被和结构与地面联系较弱，尤其在正午和斜阳场景。

必须完成：

1. 只建立一个近景方向光 shadow map，第一版不做级联阴影、点光源阴影、彩色透射阴影或光追。
2. 设置提供 `Off/Medium/High`；v0-v4 迁移固定为 `Off`，新安装默认值以 GTX 1050 Ti 的
   Release 实测决定。距离、纹理尺寸、bias 和 PCF 档位均有界，切换失败必须退回 `Off`
   而不是阻断启动。
3. solid/actor 为必需投射和接收者；flora 的 alpha cutout 和水面只在定向验证证明稳定后纳入。
4. 相机、昼夜、暂停、切世界、设备丢失和退出时完整清理 shadow 资源。

最低验证：悬浮/穿模/阴影痤疮/peter-panning 边界夹具，正午/黄昏截图，Off 与当前基线像素
兼容，Medium/High 性能分别记录；开发者视觉检查和真实 macOS Release 编译/窗口启动通过。
任何档位都不得修改世界或保存身份。

工程结果（2026-08-28）：settings v5、v0-v4→Off、双语设置/回退、单张 float32 shadow map、
独立 Off shader、2×2 PCF、距离淡出、能力回退和完整资源清理已经落地。Medium 冻结为
512²/64 m/bias 0.008，High 为 1024²/96 m/bias 0.004；solid/actor 投射，terrain、glass/flora
与 actor 接收，水保持 V10C 路径。双配置聚焦 21/21、资源包 75/75、Release 完整世界
742/742、manifest 74 项、启动负例 14/14、Stage 10 档位合同 9/9 和强制回退均通过。
六张最终 Release 原图由开发者逐图审阅为 `PASS`；Off/Medium/High 同场景各档性能保持 361 chunks、
1833 sections，最终 frame P95/P99 均未超过 Off 参考，所有者的性能例外授权未实际消耗。
合同与视觉记录见 `docs/directional-shadow-contract-v1.md`、
`docs/developer-visual-record-v10d.txt`。Windows 状态为 `Done`，macOS 保持 `Verify`；最终
Windows 身份已由 VISUAL-RC 复核。

## V10E：轻量后处理

V10E 只在 V10A、V10B1-B3 与 V10C-V10D 稳定后评估。第一版允许有界 tone curve、色带抖动和
非常轻的可选 bloom；
不引入自动曝光、运动模糊、景深、SSAO、体积光或完整 HDR/PBR 重写。后处理必须独立开关，
关闭时回到 V10D 输出；UI/HUD 不得被错误曝光或模糊。需要固定明暗阶梯图、白天/夜晚截图、
窗口缩放、暂停菜单、性能比较、开发者视觉检查和真实 macOS Release 编译/窗口启动。

工程结果（2026-08-28）：settings v6 与 v0-v5→Off、双语设置/回退、单 pass Ogre compositor、
有界 tone curve、确定性抖动和八采样极轻 bloom 已落地；Off 不安装 compositor，UI/HUD 改在
post viewport 阶段绘制并排除在后处理外。双配置聚焦 22/22、资源包 80/80、Release 完整世界
743/743、manifest 77 项、启动负例 15/15、Stage 10 性能合同 11/11、真实支持与强制回退客户端
均通过。Off/On 三次中位数的 frame P95/P99 分别为 14.28/17.13 ms 与 12.99/16.45 ms，无需
性能例外。明暗阶梯、正午、夜晚、1024x768 设置页及 Off 控制共六张最终 Release 原图由 Codex
按原尺寸检查，记录为 `PASS`。完整证据见 `docs/post-processing-contract-v1.md`。Windows 状态
为 `Done`，macOS 保持 `Verify`；最终 Windows 身份已由 VISUAL-RC 复核。

## Stage 10 版本预案

| 数据身份 | 当前基线 | 预排变化 | 兼容与失败边界 |
| -------- | -------- | -------- | ---------------- |
| 玩家/世界保存 | save v11 | 不变 | Stage 10 不新增世界或玩家持久字段；v1-v11 迁移夹具保持不变。 |
| 地形生成 | terrain v3 | 不变 | 材质、tint 与坐标 tile 变体不改变方块、结构或旧世界生成身份。 |
| 设置 | settings v6 | 不变 | v5 已持久化 `Off/Medium/High` 阴影档，v0-v4 固定迁移为 `Off`；v6 持久化后处理 `Off/On`，v0-v5 固定迁移为 `Off`。未知版本/档位严格拒绝。 |
| 本地化 | 双语各 360 key | 不变 | 后处理设置、失败回退和帮助文本已在 `en-US`/`zh-CN` 同批增加并严格 key 对齐。 |
| Q1 场景身份 | schema 3 | 核心 schema 不变，Stage 10 补充身份 | 不把新档位直接加入 schema 3 的必填 identity keys。补充报告记录 shadow/post 状态：`Off` 路径可对 BETA-RC 比较，Medium/High/On 只与同档新 baseline/repeat 比较；不同档位不可直接比较。若未来升 schema 4，必须另立旧基线桥接/重采计划。 |
| 顶点/资源 | 32 字节 terrain vertex、当前 atlas/manifest | V10A stride 不变；V10B1/B2 只升级资源合同/清单 | 只有真实顶点布局变化才另立格式身份；资源缺失、越界、非法分格必须在 Ogre 构造前失败。 |

## VISUAL-RC：集中封板

1. 重跑 VS2017/v141 Debug/Release 受影响目标、完整 Windows 门禁、资源包、隐藏客户端和 shader
   启动负例；所有 shader/顶点接口批次的 macOS Release 窗口冒烟必须已经关闭，并在此同步执行
   完整 Debug/Release Xcode 门禁。
2. 固化 V10A、V10B1-B3、V10C-V10E 最终截图矩阵与渲染身份，保留 BETA-RC before 和
   VISUAL-RC after；图片必须
   标注 seed、位置、世界时间、分辨率、窗口模式、图形档位、提交和 GPU。
3. 重跑冷启动、进世界、快速流送和规模玩法 Q1；保存/恢复只在资源或设置格式变化时重跑。
   V10A 与 V10B3 已触及网格输出/驻留，VISUAL-RC 必须以最终身份重跑 nominal/stress 各
   1800 秒正式 Q3；Q3 不替代真实 Ogre 客户端的 GPU、画面或帧时间验收。
4. 校验所有原创/第三方素材来源、许可证、Credits、manifest、干净发行包与新 SHA-256。
5. 汇总每批开发者视觉检查，并独立安排正式产品体验验收与 Physical Input v2；二者未完成
   时保持 `Verify`，不得由固定截图或 R3 v1 自动关闭。
6. 生成并验证覆盖全部未 push 提交的 bundle；不因人工项延期创建 1.0 标签，也不 push。

2026-08-28 封板结果：Windows 全门禁、最终六类 Q1、nominal/stress 各 1800 秒 Q3、资源/许可/
Credits、97 文件干净包、逐批开发者视觉矩阵与 239 项 Xcode 工程图静态检查均通过，报告见
`docs/visual-release-candidate-report-2026-08-28.md`。VISUAL-RC 标记为 `Done（Windows 工程）`；
真实 macOS `xcodebuild`/窗口冒烟、正式产品体验和 Physical Input v2 保持 `Verify`。

## VISUAL-RC 后产品决策

VISUAL-RC 不是默认的 1.0 定版，也不自动启动下一批玩法。视觉封板后必须建立一次独立产品
方向评审，至少复核实际试玩记录、资源经济决策价值、敌人/结构/生态重复度和成就/统计需求：

- 若当前主流程、人工体验和发行证据足以支持 1.0，则另立 `1.0-RC` 合同并只做缺陷修复；
- 若试玩暴露明确玩家问题，再从饥饿压力、更多结构/生态、敌人档案或成就/统计中选择一个
  最小批次立项；没有证据时保持 `Unscheduled`，不得把候选方向直接写成已承诺功能。

2026-08-28 评审结论：实际试玩已经暴露出输入动作冲突、反馈不足、前 30 分钟选择密度低、探索
奖励不能改变后续计划和敌人表现单薄等相互关联的问题，因此暂不进入 `1.0-RC`。项目建立
Stage 11 可玩性与操作体验路线；当前只立项 P11A 核心操作手感，P11B-P11F 按证据门依次排队，
权威范围见 `docs/playability-experience-roadmap.md`。强制饥饿、批量新增浅层内容和成就/统计仍不
进入当前开发范围。

## 共用非目标

- 不切换 Ogre GL3Plus，不新增 D3D/Vulkan/Metal 渲染后端。
- 不改世界生成、save v11、terrain v3、目标、经济、战斗或胜利状态。
- V10A-V10C 不改设置格式；V10D 已升到 settings v5，V10E 已按上表升到 v6，后续不得顺带修改其他用户配置。
- 不直接迁入 Luanti 的 Lua、MapBlock、IrrlichtMt、ContentDB 或资源包运行时。
- 不复制 Minecraft、Luanti、纹理包作者或其他游戏的源码与资产。
- 不以 bloom、滤镜或高饱和度掩盖基础光照、材质和构图问题。
- 不取消低画质回退；所有新增 GPU 功能必须可关闭并有明确资源上限。
