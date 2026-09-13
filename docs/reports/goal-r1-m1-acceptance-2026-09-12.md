# Goal R1：暖野 M1 验收版

起始 commit：`b155f5345950f844e2ccc5aeeb28aa58824b28a7`；进入 R1 时工作区干净。
本轮只处理已批准的暖野 v2 M1 经典树冠候选，不开始 T1 路线或 Actor 外观。
原合同：[warm-wilderness-m1-contract-v2](../contracts/warm-wilderness-m1-contract-v2.md)。
已有失败与正常输入缺项见 [M1/T1 补验](m1-t1-acceptance-continuation-2026-09-12.md)。
本文件保留旧叶与后续叶色候选的原始失败结论；用户已认可的当前 terrain v6
方块橡树版本、最新包和检查点提交授权见
[R1 方块橡树验收记录](r1-voxel-oak-acceptance-2026-09-13.md)。

## 冻结范围与退出条件

- 保留旧立方叶和 16 张旧树叶语义贴图，保留获批的其他高清材质与标准/兼容 profile。
  不改变地形生成、碰撞、掉落、固定 tick、save v12 或 T1 语义。
- 用本轮版本的带哈希 macOS Release 干净包测林地/密林 Off/Medium/High，按合同每组
  5 秒暖机、30 秒采样、三轮中位数。每组 P95/P99 ≤ 冻结 v1 的 1.10；密林地形
  buffer ≤1.35。失败轮次保留，不改变阈值或重选最好的三轮。
- 在当前包实际检查标准/兼容路径、坏资源失败、代表场景原图和动态移动观察。
  正常菜单创建/载入、移动、采集/换物、暂停设置、保存退出与重开另列证据；
  诊断启动和短按受击位移不替代正常玩法。
- 运行适用的资源、shader、设置与 macOS 自动/真实窗口门禁；新的代码或资源改动
  只重复受影响检查，最终版再完成合同要求的完整验收。独立 AI/视觉与实现者自测分开记录。
- 本轮交付验收包、SHA-256、原始图/视频、逐项 PASS/FAIL/BLOCKED、风险与报告。
  由用户明确确认 R1 通过后才暂存本轮文件并本地提交，然后才可开始 R2。

## 起始问题

在上一带哈希包的完整 18 轮中，仅密林 Medium 的 P95 为 11.955 ms，对冻结 v1
10.341 ms 为 1.156 倍，超过 1.10；P99 和密林 mesh 护栏通过。该包不含后来提交的
花草摆速调整，不能直接代表本轮起始 commit。现先用 `b155f53` 打包，并对该失败组
做固定条件三轮复核；原失败记录保留，不用新运行覆盖。

## 当前候选包与验证（未通过，未提交）

R1 候选包为 `build/goal-r1-m1-20260912/HelloMine3D-R1-Baseline.app`，
macOS Release/x86_64，来自起始 commit。109 个发行文件逐项哈希复核通过，
可执行 SHA-256 `f1a3621830709fb0efb22dda92d9f018f77ddc5f9b8ebf37c67ba68939190c3e`，
发行清单 SHA-256 `209f9aeb8f15a63a6cd0ab9d40c99955d1e8c7b8701710388af0409ba98eee76`；
见 `build/goal-r1-m1-20260912/package-verify.json`。

**原冻结矩阵性能 FAIL，原判定保留。** 按预先固定的 `full-performance-plan.md`，本包从独立副本执行林地/密林
× Off/Medium/High × 3，全部 18 次 `CAPTURED`。每次 5 秒暖机、30 秒采样，
settings v9 显式 Standard，日志确认 `standard=1 leaf_geometry=cube`。
逐轮原图、CSV、汇总和日志保留在同名目录；`performance-comparison.json` 逐轮核对
包身份、设置和产物哈希。比值如下，P95/P99 上限均为 1.10：

| 场景 | 画质档 | P95 / 冻结 v1 | P99 / 冻结 v1 | 判定 |
| --- | --- | ---: | ---: | --- |
| 林地 | Off | 1.046 | 0.903 | PASS |
| 林地 | Medium | 1.094 | 0.945 | PASS |
| 林地 | High | 0.635 | 0.636 | PASS |
| 密林 | Off | 1.045 | **1.169** | **FAIL：P99** |
| 密林 | Medium | 0.929 | 0.765 | PASS |
| 密林 | High | 1.007 | 0.963 | PASS |

密林 Off P99 本版三轮为 13.999/12.699/14.527 ms，中位数 13.999 ms；
冻结 v1 为 11.974 ms。密林三档常驻地形 buffer 约 1.00 倍，通过 ≤1.35 护栏。
原失败轮次不重选。同一 R1 包的兼容 profile 额外诊断三轮 P99 中位数为
14.594 ms；原 v1 包同机重新运行三轮为 14.216 ms，也高于它冻结时的
11.974 ms。诊断目录和预声明计划均在本轮 `build/goal-r1-m1-20260912/`。
这支持存在运行环境波动的解释，却不证明标准材质是失败原因，也不更改合同基线或
原 FAIL 结果。
`proposed-dense-off-paired-gate.md` 先将同机同期复测写为待批准提案；
用户于 2026-09-13 回复“好的，就听你建议的继续吧”，明确同意按该提案继续。
此批准只改变密林 Off 的 v1 对照分母，不覆盖原 18 轮或放宽其他门槛。
另核对冻结 v1 与 R1 的启动方式：旧采样用 `/usr/bin/open -n -W`，R1 正式
18 轮用直接启动。为测试此方法差异，预先固定的 LaunchServices 诊断在
R1 和原 v1 各启动一次；两者现均于采样前报
`NSOSStatusErrorDomain -10827 kLSNoExecutableErr`，无帧数据，原始
`capture.json` 保留在 `dense-off-open-diagnostic-r1/` 与
`dense-off-v1-open-control-r1/`。按计划停止余下诊断，不将此次 OS 启动错误
归因为 R1 运行时缺陷，也不改判正式性能结果。
无采样的 `open -n -a` 能让 R1 试玩包进入可见主菜单并正常退出；附带完整采样
环境变量的两种 `-a` 诊断仍在启动前报同一错误，失败记录另保留在
`dense-off-open-a-diagnostic-r1/` 和 `dense-off-open-a-noredir-r1/`。
这证明正式包可经 LaunchServices 正常进入菜单，但没有取得同启动方法的性能样本。
`open-launch-error-raw.log` 保留启动器 stderr 原文和退出 1；所有失败目录未覆盖。

**批准后的密林 Off 成对门槛 PASS。** 首次 `approved-dense-off-paired-run.json`
在第一项 v1-1 的启动阶段即失败，原始 stderr 为当前受限执行环境无法建立 OpenGL 3
上下文；该次没有窗口帧或有效性能样本，整组停止。保留其 `capture.json`、stderr
和未完成组记录。之后在图形可用的环境里，R1 菜单预检 `CAPTURED`，按
`paired-dense-off-attempt-b-plan.md` 从第一项开始**整组六轮**重跑，不沿用或
挑选首次结果。固定顺序 v1-1 → R1-1 → R1-2 → v1-2 → v1-3 → R1-3，
全部 `CAPTURED`，每次 5 秒暖机、30 秒采样；两包均为 1280×720 逻辑窗口、
2560×1440 实际 backing store、直接启动，seed/地点/时刻/设置除 v1 隐式与
R1 显式 Standard 外一致。包可执行文件 SHA-256 与提案吻合，逐轮四件原始产物
哈希复核通过，R1 日志确认 `standard=1 leaf_geometry=cube`。

| 密林 Off 指标 | 同机 v1 三轮 / 中位数 | R1 三轮 / 中位数 | 比值 | 门槛 | 判定 |
| --- | --- | --- | ---: | ---: | --- |
| P95 ms | 12.098/13.573/11.703 / **12.098** | 11.035/13.738/11.248 / **11.248** | 0.930 | ≤1.10 | PASS |
| P99 ms | 14.814/15.002/14.532 / **14.814** | 14.194/14.979/13.118 / **14.194** | 0.958 | ≤1.10 | PASS |
| 地形 buffer 字节 | 三轮均 42,478,872 | 三轮均 42,478,872 | 1.000 | ≤1.35 | PASS |

`approved-dense-off-paired-comparison.json` 含顺序、哈希、逐轮指标和判断；
`approved-dense-off-paired-run-attempt-b.json` 含实际宿主、供电与逐轮命令。
纹理数组加保留图集为 5,854,208 字节，即使将旧版按零计也低于 16 MiB
增量预算；资源检查此前已 PASS。其余五组沿用原 18 轮的 PASS，
故**按获批的组合口径，R1 性能项 PASS**。原冻结矩阵的
密林 Off P99 1.169× FAIL 及其全部样本不变。每包三轮均有 2 帧 >100 ms；
R1 最大帧约 333–353 ms，v1 约 247–258 ms，虽非合同 P95/P99 门槛，
仍列为偶发长帧风险，不隐去。

**自动检查：** 暖野图集/分面/图标与 5 个坏输入 PASS；64px 数组、7 级 mip、
120 有效/136 空层、纹理预算 PASS；启动缺资源/坏 shader 15/15 PASS；
当前花草 shader 真实 macOS GL 检查 10/10 PASS。原始结果在本轮目录的
`warm-atlas.log`、`warm-array.log`、`startup-negatives.log` 和 `flora-shader.log`。
第一次 Xcode Debug/Release 完整门禁在首条日志前因仓库目录访问长时间停滞而中断，
该次记 `BLOCKED`，未冒充测试结果。目录访问恢复后，本版完整门禁重新执行并
**PASS**：`xcode-gate-r2.log` 退出 0；工程图 31 项通过；Debug/Release
WorldRuntime 各 1032/1032，其余 13 个目标两配置、真实窗口与 120 帧探针均通过。
完整原始日志在 `build/xcode-validation-20260912231103/`。门禁结束后的 Release
可执行文件哈希与本轮发行包哈希相同。Windows 新验证为 `NOT_RUN`。

**视觉：** 本包生成林地/密林六档，以及草地、岩地、已核实干岸、夜间、
强制大气回退、兼容 profile 和简中 0.85 / 英文 1.25 菜单原图。
`visual-flora-motion/` 保存 80 张连续原始 BMP 和本包身份；
8 秒 10 fps 视频为 `visual-flora-motion-current.mp4`，SHA-256
`735cd61494be7f8ef2cd68b85e9102842ce4d30112f6a4cd1222a00a252f4fe6`。
旧编码器首次拒绝 BMP 的失败日志保留，当前编码器成功。林地 High、密林、
草地、岩地、干岸、夜间、大气回退、兼容 profile 与双语菜单原图已逐组复查，
所看帧未见明显截断、缺字或破面；动态序列抽帧可见花草摆动。
静态可观察缺陷检查为 `PASS`，但固定机位视频不能证明玩家移动时的闪烁/剔除，
该项仍 `BLOCKED`。主菜单背景是 `WarmWildernessMenu.png` 静态美术图，
不能算实时游戏画质证据。2026-09-13 用户提供封面与真实试玩截图，明确指出
当前树冠/树叶难看、未达到封面效果；该主观视觉项按 **FAIL（需改进）** 记录。
原图与可行性分析保存在本轮 `visual-feedback/`；其 `manifest.json` SHA-256 为
`f680cd3f0a312b15a6fa6876eb7ce1bee601429a8acee1b21f0fdeda1efe6360`，
并引用此前完整证据索引。既有静态技术检查 PASS
不覆盖此次用户审美反馈。
新的树冠修订边界和实机退出条件已整理为
[R1 树冠封面观感方案](../current/r1-canopy-cover-style-plan-2026-09-13.md)，仅为方案，未改当前包。
用户随后授权落实该方案并交付最终效果对比。以上 PASS/FAIL/BLOCKED 仍只归属
旧叶包；新候选完成后另列身份与结果，不覆盖旧证据。

**正常玩法部分 PASS，仍有 BLOCKED。** 首次 R1 隔离副本经主菜单创建 seed
20260807、terrain v5 休闲世界并进入，暂停及设置入口可用；20 次短按 W 同时遭敌攻击，
生命 20→10，位移不能归因为受控步行。首次应用英语/1.25 倍时工作区 I/O 停滞，
原窗口失联，`config.txt.pending` 完整写出新值而 `config.txt` 仍是旧值。
该失败现场保留在 `normal-play/`，不抹去。

工作区恢复后从同一已核对发行包新建 `normal-play-save/Runtime.app` 隔离副本。
真实窗口经主菜单创建 `R1 Save Reopen`（seed 20260807、Casual）、进入世界、
暂停并单独应用英语 1.0，再应用 1.25 倍，界面立即变更且配置持久化为
`locale en-US`、`uiscale 1.25`。经“Save and Main Menu”回到世界列表，
同一进程重进成功；“Save and Quit”正常退出后重启隔离包，世界仍在且重进成功。
随后在设置中应用简中 0.85 倍并保存退出，配置和窗口原图留在同目录。
再次重启后简中 0.85 倍主菜单、世界列表及已有世界仍可操作；
配置实际保存为 `locale zh-CN`、`uiscale 0.850000024`（浮点序列化）。
这些路径为 `PASS`，原始窗口截图 `menu.png`、`world-created.png`、
`world-entered.png`、`paused-en-100.png`、`paused-en-125.png`、
`world-list-after-save.png`、`world-reopened-same-process.png`、
`menu-after-restart.png`、`world-list-after-restart.png`、
`world-reopened-after-restart.png`、`paused-zh-085.png`、`settings-zh-085.png`。
游戏采用 OIS 持续按键；Computer Use 的短按 40 次 W 后机位仍未改变，
鼠标拖动也未产生可归因的视角变化；`look-before.png` 与
`look-after-drag.png` 保存了该尝试。不能证明持续步行、转头或采集/换物，
故该正常玩法链仍 `BLOCKED`。
独立 AI、AI-06 package-only、声音与用户视觉确认没有取得本版证据。

## 交付清单

本轮目录中的 `evidence-index.json` 逐项记录非应用包原始证据的相对路径、字节数和
SHA-256，`evidence-index.sha256` 校验索引自身；正式应用包由其
`Contents/Resources/distribution-sha256.txt` 逐文件校验。Xcode 门禁原始日志及
窗口帧数据已复制到 `xcode-raw/`；原始生成目录仍保留。当前索引涵盖 666 个
证据文件，索引 SHA-256 为
`765f2a2f63c3807021cc3e4dbeaa2da9d42e74a4bb6b701321ae6eb174dbfe24`；
全部索引项已重算校验通过。候选应用包不含验收世界。

获批复测后新增 `evidence-index-v2.json` 和 `evidence-index-v2.sha256`，
将首次无帧失败、图形预检、整组六轮原始帧/CSV/日志/截图、比较脚本与结果，
连同原索引一起纳入 **747** 个非应用包文件；索引 SHA-256 为
`acc1ec887e2d56efb3a8dfe4f006764bbeca65f0bc91a4698f290193369a99ea`。
新版索引再次核对正式候选包的 109 项发行清单；旧版索引及其 666 项均保留且复核通过。

另备 `manual-play/HelloMine3D-R1-Playtest.app` 供正常键鼠试玩；它从正式候选包
逐文件复制，109 项发行清单复核无失败，起始无配置或存档，运行时只污染该副本。
同目录 `README-验收步骤.md` 固定创建/移动/采集/设置/保存重开的观察步骤及失败保留方式。
此交接不把尚未取得的正常输入或独立 AI 结果预填为 PASS。

| 验收项 | 当前状态 | 证据/缺项 |
| --- | --- | --- |
| 包身份与资源完整性 | PASS | 109 文件发行清单及 `package-verify.json` |
| macOS 双配置自动、真实窗口与资源负例 | PASS | `xcode-gate-r2.log`、`xcode-raw/`、聚焦日志 |
| 按批准口径的六场景性能及密林 mesh | PASS | 原 18/18 完整；原密林 Off P99 1.169× FAIL 保留，批准后的六次成对复测 P95 0.930×、P99 0.958×、mesh 1.000× PASS；其余五组原判 PASS |
| 固定场景与界面静态可观察缺陷 | PASS | 原图、兼容/回退、双语缩放画面 |
| 花草动态固定机位 | PASS | 80 帧原图及 8 秒视频；仅证明该机位 |
| 树冠与封面观感的用户视觉验收 | FAIL | 2026-09-13 封面/真实试玩截图；用户反馈当前树冠树叶难看，需改进 |
| 正常创建、设置、保存退出与重开 | PASS | `normal-play-save/` 真实窗口截图及保留存档 |
| 正常移动、转头、采集/换物 | BLOCKED | 短按/拖动不形成可归因输入证据 |
| 移动时闪烁/剔除观察及独立黑盒 | BLOCKED | 尚无正常移动与独立 package-only 操作者证据 |
| Windows 新一轮验证 | NOT_RUN | 非本轮必需平台；不以 macOS 推断 |

风险：原冻结 v1 旧采样性能护栏仍为 FAIL，获批的同机同期口径已 PASS；
成对复测 R1 偶发最大帧仍高于 v1。用户已指出实时树冠未达到封面观感，
主菜单静态艺术图不能替代游戏内视觉验收；正常交互和移动中的稳定性仍待独立可操作输入验证。

## 旧叶候选恢复点（历史记录）

R1 当前为 **按批准口径性能 PASS + 树冠用户视觉 FAIL + 持续移动/采集与动态视觉 BLOCKED**；
自动门禁已 PASS，正常创建/设置/保存/重开已 PASS。需补正常持续输入、采集/换物、
移动时画面与独立黑盒证据，并由用户明确确认 R1 整轮验收通过。
在此之前不提交为完成版，不进入 R2；
不推送、发布或打标签。

## 2026-09-13 树冠新叶候选：同机位效果与本版验收

用户授权在 R1 内实施[树冠封面观感方案](../current/r1-canopy-cover-style-plan-2026-09-13.md)。
本段只描述**新叶包**；上文旧叶包的原始失败、获批复测及结论均保留，不把旧版 PASS
转给新版。R1 起始 commit 仍为 `b155f5345950f844e2ccc5aeeb28aa58824b28a7`，
本版没有提交、推送、发布或进入 R2。

### 冻结范围与候选身份

本版只替换标准地形纹理数组中 16 个橡树叶语义层；两张新绘 RGBA 源图及第三个
确定性镜像变化烘焙为 64 像素、7 级 mip。其余 240 个数组层、兼容图集、可执行
文件和发行资源不变，树木生成、碰撞、采集、存档语义不变。未采用的四次美术生成
及失败原因保留在 `build/goal-r1-canopy-20260913/rejected-art/`。原方案期望
旧版对比两种可用新材质；实际仅取得一个由两张有效 Alpha 源图组成的新候选，
没有把无透明通道的失败图充作第二候选。
范围冻结见同目录 `scope-freeze.json`，SHA-256
`24741d3923cfc838624137d8d0cd1410eeb751e782500efa128a3a6c1278ae94`；
资源冻结见 `asset-freeze.json`，SHA-256
`d9fadc1ad459e7c1820898ba2f2dc009e41690cc2c41a17f6d76e024fa6b51e8`。

独立 Release 包：
`build/goal-r1-canopy-20260913/HelloMine3D-R1-Canopy-Final.app`。
109 个发行文件逐项 SHA-256 核对通过；发行清单
`Contents/Resources/distribution-sha256.txt` SHA-256 为
`c068ba74d96dbb5dbe161c5f9c14b3319891010a7c7cd466698cfb41fa12cc43`。
新纹理数组 SHA-256 为
`cced1d43ea6f7813eaad889c190bd9db739f3f06e1461c391e208ab62a45c107`。
`package-verify.json` 与 `validation/leaf-only-diff.json` 给出旧包/新包、
所有层及 mip 的字节对照。验收目录的 `ACCEPTANCE-README.md` 是独立入口；
原始证据另由 `evidence-index.json` 和自身的 SHA-256 清单核对。
当前索引含 794 个非应用包文件、373,212,953 字节，SHA-256
`303768ee17343d2055ee434fb9417878b3e62ab013eed936115b1f582a33f4d1`；
全部条目已重算校验。四个运行/试玩副本不计入索引，正式包由其 109 项发行
清单独立校验。

### 同机位实际画面

[新旧叶实机对比报告](r1-canopy-cover-comparison-2026-09-13.md)列出林中、
密林与草地原始 PNG，附包身份、seed 20260807、terrain v5、时间 6000、朝向、
FOV、分辨率和画质条件。林中与密林 High 对比均从正式独立包采集，
同一机位、同一世界、同一时刻。新叶呈现更细的像素叶簇及亮暗层次；
Off、夜间和兼容画质也有原图。固定机位观察未见明显黑边、棋盘洞或整冠消失。
但冠层仍为原来的两层方块平台，与封面的高低、冠幅和松散边缘**仍有差距**；
“是否够像封面”尚待用户判断。封面 `WarmWildernessMenu.png` 是静态图，
不能当作实机效果或同机位 A/B。没有正常移动中视频，动态闪烁项仍 BLOCKED。

### 自动、性能及玩法

暖野图集与五种坏输入、数组 16 叶层/Alpha/7 级 mip、确定性字节重建、
15 个启动资源负例均 PASS。当前资源通过
`bash scripts/verify_xcode.sh` 的 macOS Debug/Release 完整门禁，
含真实窗口/世界探针；原始门禁在 `validation/xcode-gate.log`，
72 项原始产物复制在 `validation/xcode-raw/`。首次误用启动负例命令缺
`--output` 参数；控制台原记录在任务执行日志，另将同命令的退出 2
复现输出保存在 `validation/startup-invocation-missing-output.log`，
修正调用后 15/15 PASS。

正式性能计划 `performance-plan.json` 在采样前冻结，SHA-256
`d686ac47226e6ceb97639151af67e988783b022bdf7a1cccb5e649e72cc12137`。
v1/新包 × 林地/密林 × Off/Medium/High × 各三轮共 36 次全部 CAPTURED；
每次暖机 5 秒、采样 30 秒，原始 CSV、日志、原图与逐轮指标在
`performance/`、`performance-run.json` 和 `performance-comparison.json`。
所有 P95/P99 中位数比值门槛均为 ≤1.10：

| 场景/画质 | P95 新版/v1 | P99 新版/v1 | 正式判定 |
| --- | ---: | ---: | --- |
| 林地 Off | 1.003 | 1.003 | PASS |
| 林地 Medium | 0.982 | 1.022 | PASS |
| 林地 High | **1.831** | 1.024 | **FAIL：P95** |
| 密林 Off | 1.005 | 0.999 | PASS |
| 密林 Medium | 0.974 | 0.984 | PASS |
| 密林 High | 1.000 | 1.002 | PASS |

地形常驻 buffer 比值约 1.000、纹理增量低于 16 MiB，均 PASS。正式林地
High P95 的 v1 三次为 8.953/23.705/12.947 ms，新包为
23.823/23.703/10.187 ms；两包该档均出现帧时双峰。为了定位，另冻结
六轮**旧 R1 叶包／新叶包**同机林地 High 诊断，P95/P99 比值
1.043/1.013，buffer 1.000。它提示宿主/阴影档波动可能参与正式结果，
但只是诊断，**不改写正式 FAIL，不改变阈值或挑选好样本**。原始最大帧、
>33/50/100 ms 计数与全部失败记录继续保留。

新包的隔离正常试玩副本经真实菜单创建 seed 20260807、Normal 世界，进入、
暂停、打开 Standard 设置、保存回列表、同进程重进、重启后重进，均 PASS；
证据在 `manual-play/record.json`、原始窗口 PNG/JSON、世界元信息与配置。
首次窗口采集因同时有两个应用窗口而失败的 JSON 也保留；后续明确选主窗口
重新采集。Computer Use 只能短按、拖动后存档朝向仍为 0 0 0，不能取得可归因
的持续移动/转头/采集/换物或移动镜头视频，这些项目 BLOCKED。
独立 package-only 操作者、用户主观视觉签收和 Windows 新验证未取得。
供用户正常键鼠试玩的干净副本为
`manual-play/HelloMine3D-R1-Canopy-For-Review.app`；正式候选包没有测试世界。

| 验收项 | 本版状态 |
| --- | --- |
| 发行包身份、叶层边界与资源完整性 | PASS |
| 自动检查、macOS 双配置与真实窗口 | PASS |
| 正式六场景性能矩阵 | **FAIL：林地 High P95 1.831× > 1.10×** |
| 固定机位旧/新视觉与 Off/夜间/兼容检查 | PASS（仅可观察缺陷） |
| 封面相似度与整轮用户视觉确认 | 待用户判断；主轮廓仍偏方 |
| 正常创建、设置、保存、重开 | PASS |
| 正常移动、转头、采集、动态视觉视频 | BLOCKED |
| 独立 package-only 黑盒 | BLOCKED |

**本版未达到 R1 退出条件。** 林地 High 正式性能 FAIL、持续输入/动态画面
及用户视觉确认仍缺证据；因此不提交为完成版、不进入 R2。后续若修资源、
改变画质实现或经用户明确批准更改性能复测口径，必须为新候选重新冻结身份与
适用验证，并保留本版失败。
