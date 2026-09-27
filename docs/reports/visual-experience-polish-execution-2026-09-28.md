# 场景与视觉体验精修执行记录

## 恢复区

- 范围：[规划 V01–V10](../current/visual-experience-polish-plan-2026-09-25.md)与
  [执行提示词](../current/visual-experience-polish-goal-prompt-2026-09-28.md)，另记录 V00 基线和 V11 整合。
- 用户已于 2026-09-28 要求直接创建并逐步执行 Goal；宿主创建成功，状态 `active`，未设置 token 预算。
  人工核验、人工审美确认、独立试玩和严格配对性能不作为前置；自动验证与代理自测保留。
- 起始 HEAD `9ea80fb`，最近运行时 `84b3879`；起始未提交内容为上轮三份文档修改与新 Goal 提示词。
  save v12／terrain v23／map v4／settings v11。本轮首先提交执行范围与账本，再处理材质。
- 保护对象：`build/adventure-experience-20260922/HelloMine3D-Adventure-84b3879.app`、其他冻结候选和
  全部用户世界；不原位刷新、不清空。旧报告证据不改写，后续只复用有效版本和对应覆盖。
- 运行状态：启动核对时沙箱内 `pgrep` 无法访问进程列表，宿主批准的同一只读查询无匹配进程。
  当时未启动游戏；不将沙箱进程查询失败记作游戏故障。后续实际 handle 按批更新。
- 当前方向：V01a 草地／林床素材已接入，工程与固定画面小批通过；V01 整项仍 `Doing`。
  范围／启动记录已提交为 `06df80d`，本节与 V01a 实现一并提交，可由文件历史追溯。
- 当前工作包：`build/visual-experience-polish-20260928/VisualPolishWorkbench.app`，独立 bundle
  `local.hellomine3d.visual-polish`；二进制仍为已验证 `84b3879` Release，未改 C++／shader／生成规则。
  包刷新只涉及本工作包；源提交与最终清单身份见本地 `v01a/package-verification.json`。
- 下一步：V01b 收整标准草侧、土石沙木叶的像素层次，优先处理同屏明显偏细的树皮与树叶，
  再补其余种子、光照与近中远覆盖。V00 普通主路线仍待调查；不因首批通过关闭 V01 或总 Goal。

## 方向状态

| 编号 | 范围 | 状态与证据 |
| --- | --- | --- |
| V00 | 版本、保护、代表路线、对照条件 | Doing：版本与保护已核对；普通主路线待生产世界调查，当前固定机位只作诊断 |
| V01 | 材质密度与同物身份 | Doing：V01a 草地顶面／林床接入；自动工程与两处固定实景通过，完整材料／昼夜／种子／动态覆盖待补 |
| V02 | 植被轮廓与空间层次 | Todo |
| V03 | 局部地形和过渡 | Todo |
| V04 | 天空、远景、昼夜 | Todo |
| V05 | 阴影稳定与接触感 | Todo |
| V06 | 河湖海岸湿地 | Todo |
| V07 | 动物、玩家、工具动作 | Todo |
| V08 | 地标与建筑周边 | Todo |
| V09 | 地下空间和照明 | Todo |
| V10 | UI 和地图连续性 | Todo |
| V11 | 整合、必要验证和客户端交付 | Todo |

## V00：已复用的调查与边界

已读材质参数、暖野 M1 合同、架构渲染快照链及验证矩阵；旧合同的人工确认和三对性能条款
按本 Goal 最新范围调整，不新加确认关卡。素材打包沿用既有 132 个语义槽和有界独立 mip。

本地 B10 林地源图
`build/adventure-experience-20260922/b10-sky-current/forest-sun-medium/frames/capture_10000ms.png`
属于历史包诊断相机，源 framebuffer 2560×1440；用于识别后续调查点，不作为本轮材质结果。
草地源为 `docs/art-sources/warm-wilderness-v2/pixel-revision/grass-top-a.png`，
林床源为 `docs/art-sources/adventure-ecology-20260921/forest-floor-v3.png`。
新素材采用版本目录保留旧来源，不覆盖历史原图；自然像素色块和生产显示尺度分别检查。

## 验证边界与授权

中文小批次本地提交已随执行提示词获准；不推送、不发布、不打标签。实际检查按变更风险执行，
不把历史数量当本轮 PASS；普通操作、诊断机位、静态画面、连续动态与声音分开记账。
人工和独立试玩 `DEFERRED_BY_USER`，人类体验 `NOT_CLAIMED`；本轮未验项仍为 `NOT_RUN`。

## V01a：草地与林床像素层次（2026-09-28）

### 实现与可见结果

内置 ImageGen 生成[四格地表源](../art-sources/visual-polish-20260928/README.md)，保留完整提示词与哈希。
三个草地顶面变体和林床共用 16×16 创作网格，分别接入标准数组与兼容／UI 图集。
兼容草侧上缘使用同组草色，标准草侧仍沿用旧源，作为 V01b 的明确剩余项。
未修改世界、方块 ID、采集／碰撞、生成、存档或相机数值，没有为本批重建客户端。

同条件生产画面可见：草地的细碎高亮缩减为较大的连通色块；林床的密集碎纹减少，保留棕土和苔色区别。
近景仍有像素结构，中景没有发现材质串色或透明边框，兼容路径可读。
树叶、树皮和土侧仍偏细；本轮不宣称整套材质统一或高频闪烁已解决。

### 当前证据

本地目录：`build/visual-experience-polish-20260928/v01a/`；总索引 `review.json`。
全部使用同一个独立工作包，后台隐藏、不激活；6 次客户端启动均已退出 0。
两次修改前基线、两次修改后对照、一次兼容、一次有界诊断相机采样，共 16 张 1280×720 原图。

| 检查 | 条件／证据 | 结果与边界 |
| --- | --- | --- |
| 草甸前后对照 | seed 42，`1032 76 640`，旋转 `30 70 0`，time 6000，FOV 90，Medium／Off，standard；`before-meadow`／`after-meadow` | PASS：可见色块增大、碎亮点减少；固定诊断机位 |
| 林床前后对照 | seed 42，`52 86 -1152`，其余同上；`before-forest`／`after-forest` | PASS：密纹减少、地表身份保持；固定诊断机位 |
| 兼容路径 | 同草甸机位，compatibility；`compatibility-meadow` | PASS：草地与草侧可读；不是全材料回退验收 |
| 变化视角 | `standard-meadow-sweep`，2 m 横移、25° 转向、-10° 俯仰，12 s，六帧 | 分项观察：已查看起／中／末帧，未见材质消失或串色；抽帧不证明高频闪烁或普通碰撞行走 |
| 图集与语义 | `python3 tools/validate_warm_texture_atlas.py` | PASS：132 语义、124 空槽、32 方块分面、49 物品映射，独立图标保持，确定性重建、5 个负例 |
| 数组与 mip | `python3 tools/validate_warm_texture_array.py`；`array-validation.json` | PASS：7 mip、34 cutout 层、16 橡树叶层保持；17 个地表层与 UI 同源像素最大舍入差 ≤1 |
| 变更边界与重建 | `packing-verification.json` | PASS：数组逐字节重建；只改 17 层，其余 239 层在各 mip 保持；图集只改草顶／草侧／林床共 33 语义 |
| 校验器负例 | 对草地像素造错，再重算合法 FNV 与 SHA，执行新增同物检查 | PASS：仍以 `World/UI ground identity differs: grass_top` 拒绝，不能只靠文件哈希漏过语义漂移 |
| 资源解析 | macOS Xcode x86_64 Debug／Release，ResourcePackSmoke，日志 `resource-*.log` | 各 128／128 PASS；只构建受影响资源检查目标 |
| 资产清单 | `bash scripts/check_assets.sh` | 89／89 PASS；正式运行资源路径不变 |

首个 Xcode 尝试因默认用户 DerivedData 不可写而退出 65，日志 `resource-Debug-build.log` 保留；
将 DerivedData 定位到本轮工作目录后，双配置构建和资源检查通过。不是自动审批拒绝或游戏错误。
既有 Pillow `Image.fromarray(..., 'F')` 弃用警告保留，本轮不为无关警告改写图像过滤。

数组仍为 **5,592,100 B**（像素 payload 5,592,064 B），兼容图集仍为 **262,488 B**；
无新增运行纹理层、离屏目标或缓存。标准数组 SHA-256
`ee4d048ec01672903818e960b12c89b6d435725a4858afb06b94d55019367601`，图集 SHA-256
`24273081931affd89f25c679a3a823d70b93661c93ddeccddacd09b888ef8012`。
程序 SHA-256 `5acd2923f1017e561d4632af26584c490ef7ba0d5cfe4364dbb38ce03daac33e` 未变。

本批工程与静态检查通过，不等于 V01 全部完成。其他两冻结种子、背光／夜间、全部材料、
手持／掉落物实景及普通移动动态待后续 V01 整合；V00 连贯普通路线和 V02–V11 仍未完成。
严格配对性能和人工环节按范围延期，未进行人工核验、性能采样或独立试玩。
