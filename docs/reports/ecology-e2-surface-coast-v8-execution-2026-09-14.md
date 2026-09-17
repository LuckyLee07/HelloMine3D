# E2 terrain v8 地表、岸线与生态边缘执行报告

状态：`Doing / 候选 23 匹配世界夹具的正序 PASS、反序岸边流送 P99 FAIL，人工玩法延期`。只有用户明确确认“E2 验收通过”且完成版提交核对完成后才能改为 `Done`。2026-09-16 用户已调整开发顺序，允许保留 E2 未完成状态而进入下一批开发；见文末记录。

## 身份与冻结基线

- 用户参考与实际起始 HEAD 均为 `15d6f3a27d1c6af812e323563b39fea0c3bda362`；开工时工作区干净。
- E1 已验收游戏包 SHA-256 `4257be0cdc9ecfc7eb633d92b7b88b14c1620eb935c01867f5cf0350fb7243cf`，独立证据包 SHA-256 `e6df0444aa5117ca9cc5ce32feb05b5ea0e28b5328076e6332dfe0794b42ae82`。起始 Release 可执行文件 SHA-256 `54ff90e1db89defe55842e8f429e841592deb2199be8b718364de6dad4ece7d5`。
- [E2 合同](../contracts/ecology-surface-coast-v8-e2-contract-v1.md) 在生成参数变动前冻结。`build/ecology-e2-20260914/baseline/v7-data/` 保留 8 seed、463056 条 v7 生产采样与 32 个生成区块；`baseline/v7-scenes-final/` 保留干岸、浅水、草地岸线、林缘和草沙边缘的 1093 行逐格剖面、10752 行表层和 42 个生成区块。合同列出原始 SHA-256、固定机位和判定门槛。
- 唯一项目内工作客户端为 `build/ecology-e2-20260914/HelloMine3D-E2-Prototype.app`。首次从 v7 Release 建包；后续只在客户端关闭后原位刷新，不再新建临时 `.app`。

## 实现与边界

v8 增加一个纯 seed/世界 XZ 地表规划：在原 v7 基底之上按距低水位的局部剖面形成不等宽缓坡与干岸，并用低频轮廓安排沙、土、草及浅水底石/沙。区块高度/生态图、公开高度查询、树木/草花、洞口与地点规划通过同一 v8 列结果；地点投影后再清理失去适宜地面的装饰。旧 v1–v7 仍调用原采样；不读取邻区块，不变更存档格式、方块 ID、区块生命周期、网格或渲染，也不引入水 shader、真实水深和河网。候选 17 减少 v8 岸边高度探针的无用生态噪声计算，并在树木规划时复用本区块已算出的列；候选 20 保持近水缓坡幅度与干岸宽度，把离岸缓坡收束范围从 64..96 格缩至 32..64 格，避免远处缓坡产生大量额外可见面。最终二进制重新验证了生产输出。

## 候选 20 历史工程证据（小地图和产品界面追加前）

| 项目 | 当前状态 | 证据与界限 |
| --- | --- | --- |
| v1–v5 冻结生产采样/区块 | `PASS` | 最终 Release 测试二进制重采 `candidate-20-final/legacy/v1..v5/`，`validation.json` 对旧黄金采样、生成区块和统计全项 PASS。 |
| v6、v7 身份 | `PASS` | 最终二进制的 `candidate-20-final/legacy/`：v6 `chunks.csv` 与 E1 fixture 逐字节相同；v7 `samples.csv`、`chunks.csv` 与 E2 开工前基线逐字节相同。汇总见 `candidate-20-final/validation.json`。 |
| v8 多 seed、正负坐标、逆序/独立生成、装饰与保存 | `PASS` | 最终二进制 `candidate-20-final-e2-focus.log` 中 E2 24/24，Debug/Release 完整 WorldRuntime 各 1067/1067；包括全 signed int 高度、林地 E1 区块保持、洞口、8 seed 出生与木/石/煤/干沙/种子草资源、三类地点近端通路、已生成区块和两处玩家改块保存重开。seed 424 的干沙距出生 13 区块仍为资源距离风险。 |
| v8 高度/坡度、岸线形态与表层 | `PASS`（生产统计） | 最终 `candidate-20-final/validation.json` 九项 PASS：463056 条采样高度 37..165、最大单格坡度 3；96 条跨水线剖面中 90 条有连续干岸及浅滩，有效干岸 P10/P90 为 3/16 格，16 格主材质最多 3 次转换；32 个真实岸边区块的干岸沙面 59.1%、69..76 草/土 94.5%。9 个林缘区块与 v7 一致，9 个干岸区块有变化。最终输出与候选 20 工程调查 CSV 除计时外逐字节相同。 |
| Release 完整 WorldRuntime | `PASS` | 最终 `candidate-20-verify-build.log` 与 `candidate-20-verify-xcode-gui-retry.log`：各 1067/1067。 |
| Debug/Release 全目标、资源及存档回归 | `PASS` | `candidate-20-verify-build.log` 与 `candidate-20-verify-xcode-gui-retry.log`：两配置各 13 个自动目标，WorldRuntime 各 1067/1067，ResourcePack 各 105/105，StorageTransaction 各 18/18，WorldBackup 各 19/19；Xcode 两配置客户端窗口探针 PASS。首次 Xcode 普通沙箱窗口探针因无 GL3 上下文 FAIL，原始 `candidate-20-verify-xcode.log` 保留；在图形会话权限下同脚本重跑 PASS。 |
| 固定原图 | `PASS（遮挡明示）` | 最终 SHA-256 `18827b265981ce59a35260bdaa9b3a06cc1253d11c7caf21a79ba2a34a2a5834` 的同一 `.app` 采集 6 组 v7/v8 原图，逐图哈希与初始机位、朝向、时间和画质见 `window-visual-candidate20-final/comparison-manifest.json`，网页见 `comparison.html`。草地岸线原机位被敌人/前景遮挡，补拍保留 v8 地点建筑变化；未手动调整初始相机 Y，重力导致的落点差异单独记录。 |
| 窗口性能 | `最终候选 20 PASS；旧失败保留` | 最终 `window-performance-candidate20-final/comparison.json`：24 次均 CAPTURED、同一二进制、顺序与设置 PASS。三轮中位数 v8/v7 P95/P99：林地常驻 `1.002/0.958`、林地流送 `1.001/1.003`、岸边常驻 `0.991/0.995`、岸边流送 `1.017/1.062`，均 ≤1.10。361 常驻区块；岸边常驻可见面 v7/v8 约 `168806/171310`，驻留地形 buffer `25918336/26237616` 字节，网格重建均 `1339`。每次仍有 2–6 帧超过 33ms，单次最大帧约 667–963ms，原始 `frames.csv` 全保留，长帧原因未独立证实。候选 16 反序和候选 17 正序/反序 FAIL 不删除，见下文。 |
| 中文菜单正常玩法 | `PARTIAL / 动态输入 BLOCKED` | 最终二进制由 Computer Use 经真实中文菜单创建 seed `20260807`、休闲难度的 `E2 Final Coast v8`，进入林地、保存退出并重开成功；`normal-play-candidate20-final/world-meta-after-reopen.txt` 证明版本 `8` 和相同种子。12 次 W 短按未形成持续位移，保存位置仍为出生点；敌人攻击导致最低观察血量 5/20。林地到岸边步行、采集和独立正常玩法视频仍 `BLOCKED`，不以固定诊断替代。 |
| 独立带哈希游戏包与证据包 | `交付验收` | 最终可执行文件 SHA-256 `18827b265981ce59a35260bdaa9b3a06cc1253d11c7caf21a79ba2a34a2a5834`；工作客户端始终为唯一项目内 `.app`，退出后原位刷新。正式干净游戏 ZIP、独立证据 ZIP 和逐文件清单见 `build/ecology-e2-20260914/review-status.json` 及同目录的 `review-delivery.json`；按实际包哈希核对，不沿用旧候选身份。 |
| 用户确认、完成版认定与提交后核对 | `NOT_RUN` | 用户已允许先做本地实现检查点，但这不表示 E2 验收通过；最终候选仍须交付并等待明确确认。 |

## 候选 21：右上角定位小地图（2026-09-15）

用户查看候选 20 后指出画面缺少位置对比，明确要求参考 Desktop 下 NMiniGame 先加入右上角
小地图。NMiniGame 旧实现使用俯视第二相机和 RenderTarget；新版 Lua 实现按 PixelMap 单元
缓存地表、独立叠加玩家/方向/坐标。HelloMine3D 采用后者的分层与缓存思路，避免第二套场景
渲染：`OgreUserInterface` 从当前 `WorldDebugStats` 取得 seed 与 terrain version，从玩家快照
取得位置，并通过对应版本的生产地形规划生成 33×33、每格 4 米的北向色块。地图显示区块
网格、玩家朝向和约 128 米范围；世界切换时清缓存，不读取或加载 World
区块，也不改变 Gameplay 与存档。

中文资源增加北向标签。用户查看首版后要求缩小并只保留圆圈，当前版移除 ImGui 方形底板、
XYZ/范围/版本栏，圆形直径从 174 缩至 132 个逻辑像素；原右上角操作提示按圆形底边下移。
Debug/Release 增量客户端构建和 ResourcePackSmoke 105/105 已通过；唯一工作 `.app` 在关闭后
原位刷新。首个带方形底板原型的 Release SHA-256
`d7af72203c52213579751be2e89492a325b2a9ed86267a07fd8d088a2f6e95e3` 及截图保留；按用户反馈
缩小后的当前 Release SHA-256 为
`951bc2efa6be782d867663fa9c6aac151ebbd16a7e0692213b81dc7be8e16071`。固定 seed `20260807`、
草沙边缘 `(112,86,544)`、中文、High 阴影、后处理 On 的新版实机截图已确认只剩圆形地图，
操作提示无重叠。首个原型的一次岸边快速流送 30 秒聚焦检查为 3358 帧、P95 `11.975 ms`、
P99 `14.529 ms`、5 帧超过 33 ms；当前缩小版仍须重新采集性能。该单次结果只说明原型未
发现明显退化，不替代最终 v7/v8 24 次合同对比。

提交前将地图采样收口到既有 `ClassicOverWorldGenerator` 公共高度/生态查询，移除对未提交
v8 `Surface` 内部类型的编译依赖；当前 E2 工作树 Release SHA-256 为
`eb2e04778adcd8449f1e4a09bae2177c585a959e11e423a34d66bf39d5555a0e`。Debug/Release 与资源
105/105 重新通过。用户明确要求先提交小地图后，本地提交 `00e9b51` 只包含 UI 源码和两份
文本资源；E2 地形、合同、报告、测试和证据仍未提交，该中间提交不代表 E2 完成。

候选 20 的两个 ZIP、画面、性能与失败原件不覆盖；因运行时和资源身份已变化，它们降为
追加前历史证据。候选 21 尚需完整回归、正式成对原图、最终 24 次性能和新哈希游戏/证据包。

用户随后明确允许 E2 内容也先提交。提交范围固定为当前 terrain v8 生成实现、生产测试、
验证工具、v7 冻结指纹和 E2 文档；聚焦回归在提交前重跑 `24/24` PASS，候选 20 最终生产
CSV 由当前验证器复核九项 PASS。该本地提交是实现检查点，不是完成版确认；候选 21 的最终
同包证据和用户明确验收仍未执行，不据此进入下一轮。

生产调查的 8 seed 计时仅作工程诊断：v7/v8 逐格查询中位数约为 156.2/188.7 ms，
所抽生成区块中位数约为 11.5/11.7 ms。样本量、缓存和测量环境不足以推断窗口帧时间，
不用于代替上述 24 次同客户端性能门槛。窗口汇总工具已从原始 `frames.csv`
复核 P95/P99、长帧数、执行顺序和同一二进制身份；在正式 E2 采集前，另用 E1
真实 Release 记录验证了解析（3285 帧，P95 9.926 ms、P99 16.338 ms）。
候选 17 的 8 seed 生成区块计时中位数约 10.9 ms，候选 16 为 11.7 ms；
运行环境和样本量不足以把这组差异当作窗口性能结论。

## 候选 23：一次启动完成多段验收（2026-09-16）

用户指出验收期间反复启动客户端，应在一次启动中做多次检查。该要求已固定在
[E2 合同补充](../contracts/ecology-surface-coast-v8-e2-contract-v1.md)。批量诊断仅由
`HELLOMINE3D_E2_BATCH_MANIFEST` 启用：同一 Release `.app`/PID 逐段载入独立的
v7/v8 测量存档，每段保存世界、5 秒预热、30 秒逐帧 CSV、两张原图和起止事件。
正常中文菜单玩法仍另验。改动起点为干净 HEAD
`b729184ededaecb096348bdec474111deaa9a8ce`；旧逐启动脚本在用户要求后停止，
已完成的 20 段和第 21 段中断原件保存在
`build/ecology-e2-20260914/window-performance-candidate22-final/`，其中林地流送
P95 初算 FAIL，不拼接到新采集。两段试跑在
`window-batch-candidate23-pilot/` 使用一个 PID 完成 v7/v8 林地常驻、独立原图、
逐帧和 `world.meta`，仅验证方法，不作为最终性能。

最终 Xcode Release 可执行文件 SHA-256 为
`d41e615aeea8250c9790ffa7585925b807a109ce18a0e1be62a693a50b9714e6`。
`candidate-23-batch-verify-build-gmake.log` 与
`candidate-23-batch-verify-xcode.log` 的 Debug/Release 全目标、资源、存档及客户端
窗口探针 PASS。最终生产调查 `candidate-23-final/validation.json` 全项 PASS：v1–v7
冻结采样/区块指纹一致；v8 共 463056 条生产采样，高度 37..165、最大相邻坡度 3；
96 条岸线剖面中 90 条连续干岸/浅滩，干岸 P10/P90 为 3/16 格，32 个岸区块干沙
比例 58.97%，9 个林缘区块保持 v7，9 个干岸区块发生变化。

正式 `window-batch-candidate23-final/` 用**一次客户端启动**完成 24 段性能和 12 段
画面，`batch-status.json` 为 `CAPTURED`，72 条起止事件均为 PID `12512`，无相位
缺失。六组未编辑 v7/v8 原图及逐图 SHA-256 见
`visual/comparison-manifest.json` 和 `visual/comparison.html`。逐帧比较
`performance/comparison.json` 的身份、顺序和批量 PID 门禁 PASS；四组的 P95/P99
三轮中位数 v8/v7 分别为：林地常驻 `0.988/0.990 PASS`，林地快速流送
`1.024 PASS / 1.391 FAIL`，岸边常驻 `1.000/1.020 PASS`，岸边快速流送
`0.992/1.052 PASS`。林地流送超标轮次的渲染 P99 约 20 ms，低波动轮次约 13 ms；
更新耗时相近，v8 可见面较少，原因尚未证实。首轮 FAIL 和所有原始 `frames.csv`
保持不变；按开工前冻结的反序进行 `window-batch-candidate23-recheck/` 24 段独立复核，
`batch-status.json` 为 `CAPTURED`，48 条事件均为同一 PID `13353`，无相位缺失。
反序 `performance/comparison.json` 身份/顺序门禁 PASS；林地常驻 P95/P99
`1.001/0.988 PASS`、林地快速流送 `0.991 PASS / 1.305 FAIL`、岸边常驻
`0.997/1.005 PASS`、岸边快速流送 `1.014/0.994 PASS`。两套顺序均未满足
林地流送 P99 原门槛；不删除首轮失败、不拼成一次 PASS。E2 仍为 `Doing`，
未经用户明确调整验收或完成修复复验，不认定完成版。

批量模式中进程只启动一次，因此后续相位的 `summary.txt` 可没有
`startup_success`；每段必须有 `entry_success=1`、正确 seed/terrain version、完整帧数
和存档元数据，第一段仍必须有 `startup_success=1`。比较工具保留旧逐启动模式的每段
startup 门槛，同时新增同 PID 和 0..23 相位顺序核对；候选 20 旧式记录复核 PASS，
故意篡改一段 PID 的负例被判 `batch_status=FAIL`。原始摘要未补写或编辑。

因两轮林地流送均 FAIL，候选 24 曾一次启动采六段针对性图形侧录
`window-batch-candidate24-graphics-diagnostic/`，每段保留原始 `graphics.csv` 与
`frames.csv`、12 条同 PID `14238` 事件。Ogre 当前窗口统计却在真实世界有约 77 万
可见面时始终只报 `1 batch / 2 triangles`，v7/v8 全部如此，接口数据明显无效；
`candidate-24-graphics-diagnostic-note.json` 及临时源码快照保留，侧录实现已撤回，
其 gmake Release 二进制 SHA-256 `dd6ebbdb9a03362e1bab95bb9b71b9c2b77fd528ba954af54cad4d67a481234d`
不替代候选 23 的 Xcode 正式证据。候选 24 六段 P99 波动方向也变化，只作定位失败
与窗口波动风险，不当作新正式 PASS。撤回侧录后重新运行 Xcode Debug/Release
完整门禁 PASS，Release SHA-256 逐字节恢复为上述 `d41e615a…`，同一项目工作
`.app` 在关闭后原位刷新；正式与反序失败证据保持同二进制身份。

候选 23 当前正常玩法 `BLOCKED`：Computer Use 在读取 app inventory 时报告 Mac
已锁定且无法自动解锁，见本地 `normal-play-candidate23-final/cua-observations.md`；
已请求用户解锁。候选 20 与此前产品界面候选的中文菜单建档/保存重开观察保留为历史，
不替代候选 23 的正常步行、采集、保存重开或独立窗口视频。当前 FAIL 版的干净
Release 游戏包、独立证据包、逐文件 SHA-256 清单及第二道独立审计已保存在
`build/ecology-e2-20260914/review-status-candidate23.json` 与
`review-delivery-candidate23.json`；实际包哈希以本地这两份收据为准，包内没有
诊断存档或游戏工作客户端。旧封装尝试和全部失败原件保留。没有用户明确验收确认，
不做 E2 完成版提交。

## 候选 23 验证后开发顺序调整（2026-09-16）

用户明确要求本轮验证完后跳过需要人工操作的验收，并允许 E2 尚未全部通过时开展
E3 或 R1/R2。当前自动证据已结束：双配置 gmake/Xcode、资源/存档、旧版生产指纹、
v8 生产统计、同进程 24 段正式性能＋24 段反序复核及 12 段固定画面均已取得。
林地流送 P99 `1.391×` 与 `1.305×` 是两次正式 `FAIL`，其余窗口指标 `PASS`。
正常玩法记录中的锁屏 `BLOCKED` 是历史尝试结果；按用户最新顺序，该最终版步行、
采集、保存重开验收现在列为**延期 / NOT_RUN**，不换算为 `PASS`。

此变更只解除“未通过 E2 不能开工下一批”的流程约束；不放宽 1.10 性能门槛、
不删除原始逐帧、也不宣布 E2 完成。原游戏包和独立证据包仍为调整前已封存的
候选 23 快照，`review-delivery-candidate23.json` 的包哈希及文件清单原样保留。
E2 性能问题和最终版正常玩法在后续收口账本中继续跟踪。

## 候选 23 林地流送尾帧复查（2026-09-16 后续）

基于同一 Release 二进制已封存的正序与反序 `forest-streaming` 共 12 段
原始 `frames.csv`，只读工具 `tools/analyze_ecology_e2_tail.py` 保存逐段 CSV/Summary
SHA-256、超 20 ms 帧按五秒分布和渲染侧高帧分类。新分析原件位于
`build/ecology-e2-20260914/candidate23-tail-analysis/analysis.json`，SHA-256
`e03da1cccb51176b6d93ad1ee510ddafffcd04e155cbcecdf018a416ead73526`；原正式与
反序比较 JSON 分别为 `1e6c6edd986dfe21ad4c3a8268379c64ae80ffa24716ab8d3bd8e804cff9b27f`
和 `ebb3945578245e744da926dacef6ded33c00f500c96416497b7b8f0cf88aabcd`。

12 秒以后，正序有 217 帧、反序有 99 帧的 `render_ms≥15`，这些帧相对前一帧
均无区块加载数、GPU-buffered section 数、驻留地形缓冲字节、可见面分类、网格构建
累计量变化。高帧的更新耗时中位数约 2.36..2.61 ms，渲染耗时中位数约
20.38..20.94 ms；高尾帧并不随同步区块生成、网格上传或驻留增长发生。
v7 也出现这类稳定场景尖峰：正序 r1 有 91 帧，反序 r3 有 31 帧；v8 在两组
比较的更多轮次出现，仍使原 P99 规则两次 `FAIL`。这缩小了下一次定位范围，
但 CPU CSV 无法把 Ogre 绘制、GPU 等待、present 或 OS 调度分开；不能据此
把失败归咎于系统、把 v8 认定无责或改判 `PASS`。

后续若继续修复 E2 性能，应在同一个项目客户端补图形帧阶段计时或有效的 GPU
侧采样，针对这些稳定场景孤立尖峰定位，然后以原冻结顺序重新跑正式三轮与
反序复核。此前 Ogre batch/triangle 接口的明显无效数据不能作为原因证据。
Mac 锁屏与用户延期人工玩法的决定仍保持；没有新验收通过确认，不做 E2 完成版提交。

同一轮还从已提交的 `699eacf` 建立隔离源码工作树
`codex/e2-present-diagnostics`，保护主工作区未提交的 E3。项目自带 Ogre 源码确认：
`frameRenderingQueued` 位于 render target 更新与最终缓冲交换之间；据此准备仅在
显式 E2 批量诊断下启用的 `render_draw_ms`、`render_post_draw_ms`、
`render_ended_ms` 逐帧侧录，后绘制段包含缓冲交换及 LOD 事件，不冒称纯 GPU 时间。
macOS Debug/Release 两个受影响源文件对象编译 `PASS`，批量采集器参数/CSV
一致性核对就绪；先前**没有链接、创建或启动第二个游戏客户端**，实际图形侧录
`NOT_RUN`。最初隔离代码补丁保存在
`build/ecology-e2-20260914/candidate23-tail-analysis/e2-render-phase-diagnostics.patch`，
SHA-256 `d8a3288dd95ff18bf22ee90b8781686c8bc4dd1cd0c93f32ca7bff2b25368478`。
候选 23 原包、两轮性能 FAIL 与人工延期状态均不变；只有图形会话可用且原项目
E2 工作客户端关闭后，才能原位刷新并用一次启动的 pilot 核对这项新侧录。

隔离分支随后完整链接 E2 Release **可执行文件**：首次因缺少第三方静态库
`FAIL`，原始 `build/e2-render-phase-client-release-build.log` 保留；从未改的主工程
同 commit 第三方源码引用 17 个既有 Release 静态库，逐项 SHA-256 冻结并在链接后
17/17 重核 `PASS`。第二次完整链接 `PASS`，隔离工作树
`bin/HelloMine3D` SHA-256
`843eb603e1c4b100ab30c9c153d705b4488704e81f8269aec2962ae593ea4f60`。
批量工具五个合成 CSV 自检 `PASS`，包括拒绝 `NaN`、负值、三段求和错误和不足
100 帧；自检 JSON SHA-256
`6a1689fe637baaf722651c9a1f948275e5a394912c7d212c3da8c7da35857ed9`。
修订源码补丁见
`build/ecology-e2-20260914/candidate23-tail-analysis/e2-render-phase-diagnostics-r2.patch`，
SHA-256 `5fc9a6d9b47806e496f7238aec6017e10735446dc61ee3cc01cdea8079a18879`。
工作 `.app`、封存验收包均未刷新或启动；合成 CSV 不作为真实性能证据，
E2 林地 P99 仍 `FAIL`，动态图形 Pilot 仍 `NOT_RUN`。
隔离诊断构建原件已另存项目内
`build/ecology-e2-20260914/candidate23-tail-analysis/diagnostic-release-20260916/`：
二进制、初次失败及重试日志、17 库哈希清单/重核、五份合成 CSV 与工具自检共
11 个文件逐项 SHA-256 复核 `PASS`，`receipt.json` SHA-256 为
`0c3c20c671978792ad18169612ddac8b5bed3b5706a985b0a1b1bc292671d5c0`。
原 E2 工作 `.app` 可执行文件重核仍为封存的
`d41e615aeea8250c9790ffa7585925b807a109ce18a0e1be62a693a50b9714e6`。
此目录是**未做动态图形复测的诊断构建**，不是新 E2 游戏验收包。

再次对 12 段候选 23 原始林地流送 `frames.csv` 做只读变量复查，
`tools/analyze_ecology_e2_tail_signals.py` 逐段核对源 SHA-256 后生成
`build/ecology-e2-20260914/candidate23-tail-analysis/signals.json`，SHA-256
`98bfd8d9458682f346b537daa295d0f2abb6a487b89757c74b795a83ec8d284e`。
12 秒后、场景驻留指标不变的高渲染帧正序 217、反序 99；这些帧没有
演员数、天空亮度、雾、GUI 或截图计时变化；独立 `display_ms` 栏恒为 0，
不能用它排除缓冲交换／present。固定 tick 是否推进的
比例与对照帧接近，不能把尖峰归为某个 tick。六对林地流送的稳定段中，
v8 实心面数和地形驻留字节均**低于** v7，常态 `render_ms` 中位数约
5.9..6.2 ms；v8 常驻演员 4，v7 为 3，仍是不能排除的场景差异。
这些 CPU/世界统计缩小下一次图形侧录的关注点，但不证明 GPU 或系统根因，
更不改变 P99 `FAIL` 与原 1.10 门槛。

## 唯一工作客户端原位刷新为 E2 侧录版（2026-09-16）

图形会话仍锁定，未启动游戏。系统权限下的进程检查只返回 `PID` 表头，
没有 HelloMine3D 进程；现有工作 `.app` 的 109 项分发清单全部通过哈希验证，
`acceptance=NOT_RUN` 且未签名。刷新前把该客户端保存为**非 `.app` 工作副本**的
`candidate23-tail-analysis/pre-render-phase-work-client-20260916.tar`，
348 个 tar 成员，SHA-256
`a539ae856f1d0b8c6dfc3ca3d016e702fda7f5a77a4df11dfad0e6eff380b2ad`。
随后从隔离的 v8 源码工作树调用现有 `package_macos_release.py --refresh-existing`，
只原位刷新项目内唯一的 `HelloMine3D-E2-Prototype.app`；刷新后的 109 项清单
仍 PASS，二进制 SHA-256 与 metadata 的 `executable_sha256` 字段均为
`843eb603e1c4b100ab30c9c153d705b4488704e81f8269aec2962ae593ea4f60`，
源码起点 `699eacf`、terrain 当前版本仍为 8。主工程批量采集器已同步
`--render-phase-diagnostics`，与隔离版工具 SHA-256 同为
`7537104f5f83f37fc5b1c503db6843532d74fd862977641a5dbaf460d717dd56`。
刷新与回滚身份见
`candidate23-tail-analysis/work-client-refresh-20260916.json`，SHA-256
`e73ba235e97040fc27989ab236bc321a1e1cd633fc8d564fdcd4b971fda44116`。
主工程采集器的静态集成预检 `PASS`：v7 存档模板存在、两段 Pilot 调度存在、
工作包 metadata/二进制匹配、有效 CSV 接受且 `NaN` CSV 拒绝；
`candidate23-tail-analysis/main-capture-tool-preflight-20260916.json` SHA-256
`4797d5d16d18d10cf40407761c467762841b363a3ef8a8baa4b724df6ce3bff8`。
静态预检不声称图形 Pilot 已运行。

刷新的是**工作客户端**，不是候选 23 的正式验收包。正式游戏 ZIP 与独立证据
ZIP 在刷新后重核 SHA-256 仍分别为
`ff2ba216060064ca0332bd89efda90ead913837fecab0f0ed388d65687cebb22`、
`293b1406286920fe3cff5031fbc9aa752b0e20dfa67a1949c23a8e089e0b3a1f`。
动态图形 Pilot `NOT_RUN`、原林地 P99 `FAIL`、人工正常玩法依用户要求延期；诊断版工作客户端
不得当作新验收游戏包，也不能用刷新动作改判 P99。

E2 阶段侧录源码随后也同步到主工作区的 `RuntimePerformanceCapture` 与
`OgreBootstrap`，与主工程批量采集器保持一致；本机 gmake Debug/Release 客户端
增量构建均 `PASS`，原始日志留在
`build/ecology-e2-20260914/checkpoint-validation-20260916/`，SHA-256 分别为
`8d5f13e3014d7a5b9fb59b1c32d958c114cf0c414b673c2620850f02f6654f26` 与
`0c41c67f6e9dfe7dd4548afbb1738117c4429a8e41187ba15970f20dc405b9d4`。
这是诊断实现检查点，不能代替锁屏下未运行的图形 Pilot、正式 P99 复核或用户验收。

## 解锁后的 E2 绘制阶段诊断（2026-09-16）

工作客户端实际游戏二进制在 `Contents/Resources/bin/HelloMine3D`；
`Contents/MacOS/HelloMine3D` 是 176 字节启动脚本。先按正确路径核对诊断版
SHA-256 `843eb603e1c4b100ab30c9c153d705b4488704e81f8269aec2962ae593ea4f60`、
metadata 和 109 项分发清单均 `PASS`，只读进程检查无游戏进程。为直接观察已失败的
场景，短 Pilot 改为林地快速流送 r1 v7/v8，两段同 PID `32913` 一次启动：
3114/3154 帧的三段计时全有效，各两张原图；两段 P99 为 23.416/23.448 ms。
12 秒后驻留/面数/网格累计量不变的 `render_ms≥15` 帧 v7 为 101、v8 为 56，
全部以 `render_draw_ms` 为主要耗时，后绘制和 frameEnded 段中位数约
0.01/0.002 ms。Pilot 原始分析 SHA-256
`fe2c16aabd5f29e685d1fbebb6378d8c288c30336ac5b086c5cacfb2e8c6daa9`。

随后同一诊断工作 `.app` 再一次启动采集冻结正序四组、各三轮的 24 段：同 PID
`33035`，24 份原始 CSV、48 张原图，全部 `CAPTURED`；比较器从逐帧重新核对
P95/P99、每段身份和顺序，诊断版四组的 P95/P99 比值依次为林地常驻
`1.016/0.964`、林地流送 `0.992/0.975`、岸边常驻 `1.000/0.993`、岸边流送
`1.020/1.004`，均 `PASS`。比较 JSON SHA-256
`6d4dfb36f2a1f9f029e1862694112bb07914201b5b13f1f5bafe595af1f93a49`。
24 段中稳定场景高渲染帧的 v7/v8 数分别为林地常驻 `74/76`、林地流送
`24/13`、岸边常驻 `0/1`、岸边流送 `3/6`；这些帧的主要耗时均落在绘制段。
生产逐帧阶段分析 JSON SHA-256
`f2ef20e75864a40c64fdcf8f28c127a4fb3bb617eb09e2569869fe5a61b303a4`。
诊断原件与分析共 221 文件二次 SHA-256 复核 `PASS`，收据 SHA-256
`69a7a4a6ebe7e430c19e219b8338fe14939597d401c3cd6d2dd3bc20b4106c7a`。

上述 `PASS` 属于**新增诊断二进制**，不能覆盖候选 23 正式二进制先前正序
`1.391× FAIL`、反序 `1.305× FAIL`。绘制段包含渲染目标更新及其中可能的驱动等待，
不是纯 GPU 时间；v7/v8 都出现约 20 ms 的稳定场景绘制尖峰，频率随轮次波动，
现有数据没有证明独立根因。动态诊断结束且无游戏进程后，把诊断工作客户端另存
非 `.app` tar，SHA-256
`c82b08e494d2ac8a4512be06757ead05607744990853e98533cd318ba1c4e446`，
再从事先封存的 pre-diagnostic tar **原位恢复唯一工作 `.app`** 为正式候选 23
二进制 SHA-256 `d41e615aeea8250c9790ffa7585925b807a109ce18a0e1be62a693a50b9714e6`；
109 项清单及 metadata `PASS`，恢复收据 SHA-256
`2238795c8451a04e296c76d66173b9fdf4d51372cc47b488a62a5ed896a0a8c1`。
正式同包 24 段复核另开全新原始目录，不回写诊断或候选 23 已封存快照。

恢复后的正式候选 23 首次同包复核目录为
`candidate23-tail-analysis/candidate23-official-performance-20260916-r1/`。
客户端进程已经启动，但在写入首个相位事件前图形会话再次锁定；Computer Use 返回
`The Mac is locked`。为避免 24 段工具空等，终止该无相位进程并让采集器正常封存
`FAIL`：事件数 0、PID 列表空、26 项缺失相位/身份错误，`batch-status.json`
SHA-256 `12f7ba9e01157d65c0222ed8a559cb5e7657f3b785fbc374d6e2c294262f4e6d`。
没有 CSV 或截图，不能用于性能比较；失败原件不删除，也不改写候选 23 的既有结果。
后续正式重试前已启动最长两小时的本轮 `caffeinate -dimsu` 会话，用于阻止验证期间
因闲置触发显示器/系统睡眠；它不解锁已锁定的登录会话，也不作为性能豁免。
为使之后的正式重试不依赖独立常驻进程，批量采集器随后把 `/usr/bin/open -W`
直接包在 `/usr/bin/caffeinate -dimsu` 中；防闲置与这一次游戏进程同生命周期，
并在批次/逐段收据里分别记录 wrapper 与 application command。该改动不启动游戏、
不改变包身份或计时门槛，也仍不能解锁已锁定的登录会话。

## 正式复核的世界夹具身份纠正（2026-09-16）

解锁后，正式候选 23 二进制 `d41e615a…` 先完成一轮正序和一轮反序同包采集。正序目录
`candidate23-official-performance-20260916-r2/` 有 24 份 CSV、48 张原图，批次收据
SHA-256 `9b914c95a7bf2763a96945c019be581a6081f2f654a59b494229cf43e8af61b8`；
原比较中林地常驻 P95 `2.436×`、岸边流送 P99 `1.180×` 为 `FAIL`。反序目录
`candidate23-official-performance-reverse-20260916-r1/` 同样完整，批次收据 SHA-256
`aa90330a38a43fdccd694c3e48d8a32a9f297fafcd06b022c119f59cb036d66c`，原比较四组均
未超过 1.10。两轮表面上互相矛盾，不能择取其中一轮作为结论。

继续核对存档身份后定位到协议缺陷：冻结 v7 模板是 `difficulty_id=0`、`actor_count=0`；旧工具
只给 v7 复制模板，v8 建空目录并由游戏按 `difficulty_id=1` 初始化。实际 summary 因此始终是
v7 休闲、v8 普通，结束时 Actor 数也常见 v7 为 3、v8 为 4 或 5。它混入了难度和自然生成
Actor 差异，既不能证明 terrain v8 退化，也不能证明 terrain v8 通过。上述正序、反序、诊断和
更早候选原件都不删除，但从正式 terrain-only 性能门槛降为“世界夹具身份不一致”的协议失败证据。

采集器现改为每个 v7/v8 相位都复制同一冻结模板，只改唯一 `world_id` 与
`terrain_generation_version`，并统一 `difficulty_id=1`、初始 `actor_count=0`；模板 SHA、初始
元数据 SHA 和字段写入批次/相位收据。比较器同时要求 summary 为普通难度并核对夹具收据，旧
正序数据已被新规则明确拒绝。临时夹具自测确认两版元数据只有世界 ID 与 terrain version 不同，
24 段正反序计划均未改变。正式游戏包和二进制未刷新。

匹配夹具的正式正序目录为 `candidate23-official-matched-performance-20260916-r1/`：同一 PID
`43096` 完成 48 个事件、24 份 CSV 和 48 张原图，批次、身份、夹具与顺序均 `PASS`；四组
P95/P99 比值分别为林地常驻 `0.868/0.996`、林地流送 `0.999/0.998`、岸边常驻
`1.003/1.002`、岸边流送 `1.015/1.079`，总结果 `PASS`。批次/比较 SHA-256 分别为
`f03703bd94ebd5df63731509700c0a56aaff15654631f698dc1d0b978f0072a3`、
`c4f3bf35b79d7f6710829887e1b32a126fcdce72d074caf07af68da73572c686`。

按预先冻结的反序完整复核保存在
`candidate23-official-matched-performance-reverse-20260916-r1/`：同一 PID `43437` 的 24 段
全部 `CAPTURED`，身份/夹具/顺序仍 `PASS`。林地常驻 `0.724/0.995`、林地流送
`1.000/0.998`、岸边常驻 `0.998/0.999` 均通过；岸边流送 P95 `1.006 PASS`，P99
`1.134 FAIL`。其 v8 三轮 P99 为 `17.234/21.079/18.962 ms`，v7 为
`14.719/16.717/22.974 ms`；不是删除单个坏轮次可解决的证据。两版在每个对应场景的最终
Actor 数一致（常驻 4、岸边流送 5），均为 361 驻留区块，世界夹具差异已排除。反序批次/比较
SHA-256 分别为 `b16b1fe92356d4cb6e4561f18e40156478f98c4db8690ccd21fb661bc226ee35`、
`7399d34d44877744da3d322b5c3a6ce9e9e95c580166e207a9d629e9cd3d74fe`。两轮 396 个文件
完整性收据 SHA-256 为
`0afc100054f993dd5950d4d22bdfad9e5dbf6880a2ead3f174b30656571c6463`。正序 PASS
不能覆盖反序 FAIL；E2 性能仍未收口。

## 候选 24 生成器微优化反证（2026-09-16）

为判断岸边流送 P99 是否可由 terrain v8 的列规划计算直接收口，在不启动窗口客户端的
同一台机器上，以当前 `ec82ed3` 为基线分别试验三种局部实现：固定大小直接映射列缓存、
按需计算弯曲/细节噪声并改用轴向距离、复用生态噪声值。每个候选与基线都执行三轮、八个
冻结 seed 的真实 C++ 生产采样和区块生成；所有候选的 `samples.csv`、`chunks.csv` 均与
基线逐字节相同，SHA-256 分别为
`71b46545c2aec8027279df9e615edbd403c73ceadd4e8488d919c99d4133b97a`、
`69054fd975693228067efa4ce7994e43be427a0f9e6c987f20b57131a004d70d`，E2 聚焦
`24/24 PASS`。

三种方案都没有得到可重复的性能收益：固定缓存的全样本生成中位数比值为 `0.987`，但正式
seed `20260807` 为 `1.031`；按需噪声方案总体为 `1.010`、该 seed 为 `1.045`；生态值复用
总体为 `1.005`、该 seed 为 `1.009`。这些幅度不足以解释或可靠修复窗口反序 `1.134×`
尾帧，而且后两种方案整体略退化。因此三项均判为 `REJECTED_NO_MEASURED_BENEFIT`，源码
全部恢复到 `ec82ed3`，没有刷新正式工作客户端，也没有用一次更快的偶然轮次覆盖失败。
汇总 JSON 位于
`candidate23-tail-analysis/candidate24-generator-ab-summary.json`，SHA-256
`70c32908177ed603cc2fd3959b0a689cd44ea802ee1125260d65f84ed52e0f2f`；十二组 A/B 原始目录
和被否决的源码快照继续保存在同一分析目录。该反证说明当前失败更适合在后续专门的渲染/呈现
尾帧批次中处理，不应把未见收益的生成器改动带入 E2 或 E3。

## 岸边流送尾帧负载判定（2026-09-17）

对匹配夹具正序、反序共 12 段岸边流送原始 `frames.csv` 和 `summary.txt` 做逐文件
复核。冻结的逐顺序三轮中位数判定保持不变：正序岸边流送 P95/P99 为
`1.015×/1.079× PASS`，反序为 `1.006×/1.134× FAIL`，因此 E2 正式结果仍是
`FAIL`。

只作定位的六轮合并统计显示，v8/v7 平均整帧为 `1.0009×`、P95 为 `1.0110×`、
P99 为 `1.0999×`；更新阶段 P99 为 `1.0673×`，渲染阶段 P99 为 `1.0075×`。
最终场景中 v8 的实体面、水面、植被面、网格重建数和网格构建总时间中位数分别为 v7 的
`0.9621×`、`0.9524×`、`0.8478×`、`0.9419×`、`0.9490×`，常驻地形缓冲为
`1.0132×`。这说明没有出现足以解释 `1.134×` 的 terrain v8 面数或网格构建负载增长；
六轮合并统计也不能替代合同冻结的逐顺序门槛，故不据此改判。

诊断 JSON 为
`candidate23-tail-analysis/candidate23-shore-streaming-tail-adjudication-20260917.json`，
SHA-256 `398d8f3b4d927ed97d52b640a528147b0dbb110d26ca213c72fcfc0def6c6ac6`。
文件记录 12 份原始 CSV/summary 的源哈希、正式比较器结果、合并诊断和负载中位数；
正式二进制、门槛与失败原件均未修改。

## 候选 25 噪声格点缓存反证（2026-09-17）

在隔离工作树 `d1fcd97` 试验只用于 v8 单区块生成的固定大小噪声格点缓存。缓存项以
格点坐标和 salt 完整比对，碰撞只会成为 miss；三轮八 seed 的 `samples.csv` 与
`chunks.csv` 均和基线逐字节相同，SHA-256 仍分别为
`71b46545c2aec8027279df9e615edbd403c73ceadd4e8488d919c99d4133b97a`、
`69054fd975693228067efa4ce7994e43be427a0f9e6c987f20b57131a004d70d`。

候选没有得到性能收益：八 seed 全部运行的生成时间中位数比值为 `1.0599×`，逐 seed
比值中位数为 `1.0552×`；正式 seed `20260807` 为 `1.0556×`。生产调查时间也分别退化
`1.0364×` 和 `1.0507×`。因此判为 `REJECTED_MEASURED_REGRESSION`，补丁已撤回，
隔离工作树恢复干净并重新链接原始源码。证据位于
`candidate23-tail-analysis/candidate25-lattice-cache-ab/`；汇总 JSON SHA-256
`29b66a269bfe75151ae594c93519b82f35032c338840ceb6ded8b709a9d081af`，27 文件完整性清单
SHA-256 `6fd10394d740646d6971de313df85de4382fb22ff0c1b4a38b507c394de5f84b`。
首次误预建输出目录导致的拒绝日志也保留。该反证继续支持不在 E2 生成层引入未测得收益的
优化，正式岸边流送 `1.134× FAIL` 不改判。

## 候选 23 后续证据增量包（2026-09-17）

候选 23 原证据 ZIP 封存在匹配世界夹具复核之前，不能单独代表后续性能结论。为避免重打
约 1 GB 已逐文件核验的基础包，新增独立后续证据包
`HelloMine3D-E2-terrain-v8-candidate23-late-review-evidence.zip`，SHA-256
`fedbcab270d2d987fd1c9fffb61152968893d25a786919ad6d0b5c2953317981`。它完整包含匹配
夹具正序/反序 396 个文件、岸边流送负载判定、候选 24 汇总与时序、候选 25 三轮 A/B、
合同和最新执行/审计报告，并在 `reference.json` 中绑定：

- 游戏包 SHA-256 `ff2ba216060064ca0332bd89efda90ead913837fecab0f0ed388d65687cebb22`；
- 基础证据包 SHA-256 `293b1406286920fe3cff5031fbc9aa752b0e20dfa67a1949c23a8e089e0b3a1f`；
- 正式客户端二进制 SHA-256 `d41e615aeea8250c9790ffa7585925b807a109ce18a0e1be62a693a50b9714e6`。

ZIP 解压校验通过，438 个清单文件逐项复算为 `438/438 PASS`；包内清单 SHA-256
`caa5b946ab83bff770393f171f80791aeaf334e48ea4ad1de07a018134f2f45c`，外部收据为
`build/ecology-e2-20260914/review-delivery-candidate23-late-evidence.json`。包状态明确为
`FAIL_UNCHANGED_NORMAL_PLAY_BLOCKED / NOT_ACCEPTED`，不冒充完成版。最终结论冻结后仍需
合并基础包、增量包和新增玩法证据，或在最终交付收据中将基础包与增量包定义为一组完整证据。

## 保留的失败及修复记录

- v7 窗口首启因锁屏超时，原始失败见 `baseline/visual-v7-dry-shore/`。未以无窗口截图替代。
- 解锁后在普通沙箱内以 `/usr/bin/open` 启动出现 LaunchServices `-10827`，直接执行又因无 GL3 图形上下文失败；原始 `window-visual/dry_shore-v7-r1/`、`dry_shore-v7-direct-r1/` 和日志保留。以系统审核的图形会话权限复用同一 `.app` 后正常捕获；未创建第二个工作客户端。
- 第一轮 `T0-SURVEY` 输出目录已存在，按工具要求转入全新目录；最初手动 `make` 因旧生成工程缺 `TerrainFoundation.cpp` 链接失败，运行 `scripts/premake.sh gmake` 后修复并保留原始构建输出。
- 初始 E2 岸线扫描在固定搜索窗口仅找到 95 条，原始 `baseline/v7-coasts/` 与日志保留；扩大地理搜索范围后固定为 96 条，未改合同数量。
- v8 中间候选的岸宽、陡坎和局部沙土细条纹均有 `candidate-*` 原始 CSV/日志；保留不合格候选，逐次修正规划而非放宽门槛。
- v8 首次装饰回归发现 seed 0 `(1028,1026)` 的 TallGrass 留在地点投影后的 Dirt 上，失败 `candidate-11-e2-focus-3.log` 保留。增加 v8 末端地面适宜性清理后聚焦回归通过。
- 初始资源测试 seed 424 在出生 8 区块内未见沙，失败 `candidate-12-e2-focus-resources.log` 保留；将未在合同指定的搜索半径扩至 16，并仅计干燥地表可采沙面，实测第 13 区块找到。该距离风险不隐去。
- 草沙边缘候选 15 的生产材质图显示土带仍偏直，原始 CSV 与图保留；最终候选 16 加大低频坐标扰动后带宽沿岸变化，全部原门槛重新 PASS。候选 14 的调查目录预先存在导致一次工具报错，其日志与目录也保留。
- 草地岸线固定画面两版均有近处敌人遮挡，v8 尤其严重；原始画面保留并在对照页标注，不当作清晰岸线 PASS。
- 首轮窗口性能四组通过，但因林地流送 v7 P99 波动按合同反序完整复测；第二轮林地流送、岸边常驻均超 1.10，原始 FAIL 与 48 次逐帧记录全部保留。高占用的 macOS 媒体分析进程只是待复核的外部干扰线索，未作为豁免。
- 候选 17 只优化重复生成计算，E2 聚焦 24/24、生产 CSV 与候选 16 逐字节一致；`candidate-17-verify-build-gmake.log` PASS。首次 `candidate-17-verify-build-full.log` 中 Xcode Debug StorageTransaction 实际 18/18 PASS，但验证脚本旧预期 16 导致流程 FAIL；修正必要脚本后 `candidate-17-verify-xcode-retry.log` 两配置完整 PASS，原始误报保留。
- 候选 17 窗口采集在岸边常驻首轮 v8 遇系统睡眠，原始 `window-performance-candidate17/shore-steady-r1-v8/` 有 942391 ms 单帧、0 张截图和 FAIL `capture.json`；`power-log-candidate17-sleep.txt` 记录同步的 945 秒睡眠。已完成的前 13 次仍保留，未把失败目录重新运行成 PASS。解锁后首个防睡眠脚本因 macOS Bash 3.2 空数组展开失败，仅留系统快照及日志于 `window-performance-candidate17-awake/`；修正脚本后换新目录完整重跑。
- 候选 17 完整正序 `window-performance-candidate17-awake-retry/` 中岸边常驻 P99 `1.148× FAIL`；依合同预定反序完整重跑 `window-performance-candidate17-reverse/`，同项 `1.143× FAIL`。两套 48 次逐帧数据和全部可见面/驻留/加载记录保留。岸边常驻可见面 v8 约 223800，v7 约 168800；可重复超标，不能归因于单次系统波动。
- 候选 18 将浅滩抬高幅度减半，生产统计仍 PASS，但六次同包岸边常驻诊断 P99 `1.182× FAIL`，可见面仅减少约 400，失败数据在 `window-performance-candidate18-shore-diagnostic/`。候选 19 改小缓坡幅度，干岸 P90 宽度变为 24 格，超过合同 18 格上限，`candidate-19/validation-exploratory.json` 为 FAIL；未拿静态失败候选做窗口采集。
- 候选 20 只将离岸缓坡收束范围从 64..96 缩至 32..64 格，近水形态与坡度门槛保持。其六次针对性岸边诊断 P95/P99 `0.993/0.996`，可见面降至约 171300；最终 Xcode Release 二进制随后按全合同 24 次重采，四组均 PASS。候选 18/19 与旧候选的失败不会从证据包中删除。
- 候选 20 首次 Xcode 完整脚本在普通沙箱通过工程/自动测试，但客户端窗口探针因无法取得 GL3 图形上下文 FAIL；原始 `candidate-20-verify-xcode.log` 保留。图形会话权限下同脚本 Debug/Release 完整重跑 PASS，`candidate-20-verify-xcode-gui-retry.log` 保留。最后一次正常玩法通过 Computer Use 完成中文菜单建档、保存重开；持续 W 短按未形成步行且遭敌人攻击，该动态验收如实 BLOCKED。
