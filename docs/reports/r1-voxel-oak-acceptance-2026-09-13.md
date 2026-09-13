# R1 方块橡树树冠：候选与验收记录

状态：**方块树冠视觉获用户认可，获准本地提交一版；R1 整轮仍有 BLOCKED，不进入 R2**。起始 commit：
`b155f5345950f844e2ccc5aeeb28aa58824b28a7`。本次修订在用户明确
拒收斜切低多边形树冠后，只处理橡树叶色与树冠轮廓。保留失败样板于本地
`build/goal-r1-canopy-shape-20260913/rejected-lowpoly/`；它不属于当前候选。

## 当前候选

- 标准路径 16 个橡树叶语义层使用两张原创方像素 RGB 源，经确定性的挖空、
  线性光预乘缩小、128 像素母版和 64 像素独立 mip 烘焙；兼容图集不变。
- 新建世界的 terrain v6 橡树采用完整方块叶，宽层、窄顶与少量边缘缺口；
  不含斜切面、交叉卡片或叶壳。v1–v5 旧世界保留旧树形及未探索区块生成。
- v6 沿用 v5 地貌、遗迹/营地规划及奖励语义；方块身份、碰撞、采集与
  save v12 格式不变。旧版生成函数未改动。
- 第一张 v6 样板单树约 62 个叶格、遮挡过强，已保留原图并收敛；当前
  定向测试树为旧版 36 格／新版 39 格。

## 可比实机画面

图片为独立包固定机位诊断，**不等于正常键鼠玩法验收**。两版均为 seed
`20260807`、时间 `6000`、RD8、FOV90、High 阴影、后处理 On、实际
2560×1440。林地初始高空出生点 `(1024,140,1024)` 的第一组 A/B 因
v6 叶块导致落地高度相差 5 格，已标记不可比并保留。重拍林地实际落地
`(1032,77,1024)`，密林实际落地 `(928,83,928)`，朝向均为 `0 0 0`。

| 场景 | v5 方叶树形 | v6 方块树冠候选 |
| --- | --- | --- |
| 林地同机位 | 本地 `build/goal-r1-canopy-shape-20260913/visual-prototype/voxel-colour-offset-1032/frames/capture_10000ms.png` | 本地 `build/goal-r1-canopy-shape-20260913/visual-final/forest-high/frames/capture_10000ms.png` |
| 密林同机位 | 本地 `build/goal-r1-canopy-shape-20260913/visual-prototype/voxel-colour-dense-high/frames/capture_10000ms.png` | 本地 `build/goal-r1-canopy-shape-20260913/visual-final/dense-high/frames/capture_10000ms.png` |

v5 同机位对照采用同系列方像素初版叶色；v6 还包含后续轻度提亮，故该对照
观察的是**树形与颜色的合并变化**，并非纯树形差分。封面图是静态菜单美术，
与运行时图仅作审美目标比照，不能逐像素比较。用户试玩后反馈“现在的效果是挺不错的，相比之前好不少”，并要求按此版本先提交一版；
此反馈记为当前树冠方向的用户视觉认可，不扩写为全部玩法通过。
最终包还保存林地 `forest-off`、`forest-night`、`forest-compat` 各两张 5/10 秒
原始帧与 `capture.json`（均 `CAPTURED`）；这是固定机位检查，不声称动态移动或
音频已验收。

## 独立包身份

正式候选为本地
`build/goal-r1-canopy-shape-20260913/HelloMine3D-R1-Voxel-Oak-Acceptance.app`。
可交付压缩包为同目录 `HelloMine3D-R1-Voxel-Oak-Acceptance.zip`（20 MiB），
`unzip -tq` PASS，ZIP SHA-256 为
`a7972bce6e27141971c0bd17855865c77f5ad7c30eb68769501949f32849b9e2`。
包内发行清单 `Contents/Resources/distribution-sha256.txt` 的 SHA-256 为
`bf3106d28fa20410faa11425d040172ef6682315a8684ae1c84280ffd32acb21`；
Release x86_64 程序为
`6278b8fbad0eeb2d7ea408264ffc5a3b46973bd079b8f09458c4830343ccbbd8`，
纹理数组为
`e64871ba629d3aa1571e731813466e3ee142fcb3d432765be84593972b9e7f3d`。
包内 109 个发行文件逐项有哈希。`acceptance-package-verify.json` 证实与
正式性能采样包相比，程序/游戏资源逐字节一致，仅包身份元数据和发行清单
不同。正常试玩副本在 `manual-play/`，其三项关键哈希与正式包相同；正式包
自身不写入测试存档。
本地证据索引 `acceptance-evidence-sha256.json` 列出 448 份原始/汇总文件及
各自 SHA-256，索引自身 SHA-256 为
`91b79771c55acfe0205a8da80d8cfbd950eac6bd84bd0983254529cc14b4ff17`。
首次锁屏尝试另记 `manual-play/computer-use-attempt.json`；用户解锁后的正常
窗口补验见 `manual-play/normal-play-20260913.json`（SHA-256
`57464dd7b958835209b044364f19dec84f07e5a842a87d5dead92a274bc74321`）。

## 验证与恢复

固定性能计划在本地 `build/goal-r1-canopy-shape-20260913/performance-plan-voxel.json`，
SHA-256 `6037968c6cf3e412f7e0a725d03b1a73889e57b942fc881b1655538d76d6862f`。
林地因 v6 树冠覆盖原出生点，改用两版实际落地一致的 x1032 机位；密林
保持 x928。计划包含林地／密林 × Off／Medium／High，每档 v1／v6 各三轮、
5 秒预热、30 秒采样，按冻结顺序不替换失败，P95/P99 中位数比值门槛均
≤1.10，密林常驻地形 buffer ≤1.35。

| 项目 | 当前结果 | 证据 |
| --- | --- | --- |
| R1 树形及 v5 兼容定向 | PASS，4 项 | `validation/r1-voxel-reduced-focused-world.log` |
| T1 v5 定向回归 | PASS | `validation/r1-voxel-t1-focused.log` |
| 完整 WorldRuntime | PASS，1036 项 | `validation/r1-voxel-full-world.log` |
| 资源包 | PASS，105 项 | `validation/r1-voxel-resource-pack.log` |
| 纹理数组/旧图集 | PASS | `validation/r1-voxel-final-array.json`、`r1-voxel-atlas.json` |
| 资产清单 | PASS，80 项 | `validation/r1-voxel-asset-check.log` |
| x86_64 Release 客户端与测试目标 | PASS | `validation/r1-voxel-reduced-*-build.log`、`r1-voxel-final-release-client-build.log` |
| 六档正式性能 | PASS，36/36 采集成功 | `performance-run-voxel.json`、两场景 comparison JSON 与各轮原始 CSV |
| x86_64 Debug 客户端与 WorldRuntime | PASS，定向 4 项 | `validation/r1-voxel-debug-*-build.log`、`r1-voxel-debug-focused-world.log` |
| 启动资源缺失/损坏负例 | PASS，15/15 | `validation/startup-errors-voxel/summary.json`；使用与包相同 SHA 的 Release 程序 |
| 最终包固定机位标准/兼容、日/夜、High/Off | PASS，五组两帧，限诊断 | `visual-final/*/capture.json` 与原始 PNG |
| 最终包正常菜单、建档、载入、设置、保存重开 | PASS，限这些可验证步骤 | `manual-play/normal-play-20260913.json`、世界列表/暂停/重开原始 PNG、save v12 `world.meta`、`config.txt` |
| 持续移动/转头、采集与移动视频 | BLOCKED | 12 次 W、30 次 D、视口点击和两次拖动未形成可信移动路径；保存朝向仍 `0 0 0`，当前 Computer Use 没有按住键/鼠标的接口 |
| 用户对当前树冠观感 | PASS，限本版视觉方向 | 用户试玩后明确认可并授权按本版提交一版 |
| R1 整轮正常玩法退出条件 | BLOCKED | 持续移动/转头、采集与移动视频仍缺可信原始证据 |

正常窗口补验在用户解锁后于**正式包的隔离副本**完成：通过可见菜单创建
`R1 Voxel Oak Normal`（seed `20260807`），加载、返回菜单、数次重开；
设置 High 阴影和后处理 On 后 `config.txt` 持久化对应值，暂停菜单切到
Casual 并在重开后的世界列表显示。存档仍为 save v12、terrain v6，关键程序、
叶数组及包清单哈希与正式包一致。四张未经编辑的 CUA 原始窗口截图和最终
测试存档/设置副本均有逐文件 SHA-256。敌人攻击导致血量波动；短按之后的
细小位置变化不能归因于玩家输入。故**只给菜单/设置/保存链 PASS**，
不把截图、短按或敌人击退记作持续移动、转头、采集或移动视觉 PASS。

36 次采样的包/架构/种子/生成版本/实际落地位置、CSV 和摘要逐项校验 PASS，
见本地 `performance-integrity-voxel.json`。逐档中位数比值如下（门槛 P95/P99
≤1.10；密林 buffer ≤1.35）：

| 场景/画质 | P95 | P99 | 常驻地形 buffer | 判定 |
| --- | ---: | ---: | ---: | --- |
| 林地 Off | 1.004× | 1.022× | 1.067× | PASS |
| 林地 Medium | 1.013× | 1.030× | 1.067× | PASS |
| 林地 High | 0.997× | 1.006× | 1.067× | PASS |
| 密林 Off | 1.003× | 0.997× | 1.068× | PASS |
| 密林 Medium | 0.986× | 0.986× | 1.068× | PASS |
| 密林 High | 1.003× | 1.007× | 1.068× | PASS |

尾部风险：每个场景/画质三轮中，v1 与 v6 都各有 6 帧 >100 ms；
v6 的单轮最大帧比 v1 高（例如林地 High 最高 886.948 对 792.192 ms）。
本合同用三轮 P95/P99 与密林常驻 buffer 判定，最大帧和长帧单列，不隐去。
上一候选林地 High 正式 P95 `1.831× FAIL` 仍留在其原报告，不被本结果改写。

恢复：性能、Debug、启动负例、固定画面与正常菜单/设置/保存链已结束，
用户已认可本版树冠并明确授权**本地版本检查点提交**。当前运行客户端通过
设置已切至 `zh-CN`；重新解压的原始 ZIP 仍含初始 `en-US` 配置，中文资源
与切换入口存在，此检查点不改变包内默认语言。
本 Computer Use 接口不支持持续按住键或鼠标，真实移动/转头/采集和移动
画面仍需支持持续输入的正常 OS 操作者补验；锁屏首次失败和解锁后短按/拖动
的原始结果都保留。此前候选的正常玩法结果不继承给本包。
新代码/资源若再改，重做受影响验证和包身份。本次只可按用户最新授权提交
**当前版本检查点**；未获 R1 整轮通过确认前，不把该提交标为完成版、不进入
R2，也不推送、发布或打标签。
