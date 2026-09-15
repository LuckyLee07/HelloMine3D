# E2 terrain v8 地表、岸线与生态边缘执行报告

状态：`Doing / 候选 21 小地图追加验证中 / 允许实现检查点提交`。本报告随 E2 验证继续更新；只有用户明确确认“E2 验收通过”且最终包核对完成后才能改为 `Done`。本轮不实施 E3，也不收口 R1/M1、R2/T1。

## 身份与冻结基线

- 用户参考与实际起始 HEAD 均为 `15d6f3a27d1c6af812e323563b39fea0c3bda362`；开工时工作区干净。
- E1 已验收游戏包 SHA-256 `4257be0cdc9ecfc7eb633d92b7b88b14c1620eb935c01867f5cf0350fb7243cf`，独立证据包 SHA-256 `e6df0444aa5117ca9cc5ce32feb05b5ea0e28b5328076e6332dfe0794b42ae82`。起始 Release 可执行文件 SHA-256 `54ff90e1db89defe55842e8f429e841592deb2199be8b718364de6dad4ece7d5`。
- [E2 合同](../contracts/ecology-surface-coast-v8-e2-contract-v1.md) 在生成参数变动前冻结。`build/ecology-e2-20260914/baseline/v7-data/` 保留 8 seed、463056 条 v7 生产采样与 32 个生成区块；`baseline/v7-scenes-final/` 保留干岸、浅水、草地岸线、林缘和草沙边缘的 1093 行逐格剖面、10752 行表层和 42 个生成区块。合同列出原始 SHA-256、固定机位和判定门槛。
- 唯一项目内工作客户端为 `build/ecology-e2-20260914/HelloMine3D-E2-Prototype.app`。首次从 v7 Release 建包；后续只在客户端关闭后原位刷新，不再新建临时 `.app`。

## 实现与边界

v8 增加一个纯 seed/世界 XZ 地表规划：在原 v7 基底之上按距低水位的局部剖面形成不等宽缓坡与干岸，并用低频轮廓安排沙、土、草及浅水底石/沙。区块高度/生态图、公开高度查询、树木/草花、洞口与地点规划通过同一 v8 列结果；地点投影后再清理失去适宜地面的装饰。旧 v1–v7 仍调用原采样；不读取邻区块，不变更存档格式、方块 ID、区块生命周期、网格或渲染，也不引入水 shader、真实水深和河网。候选 17 减少 v8 岸边高度探针的无用生态噪声计算，并在树木规划时复用本区块已算出的列；候选 20 保持近水缓坡幅度与干岸宽度，把离岸缓坡收束范围从 64..96 格缩至 32..64 格，避免远处缓坡产生大量额外可见面。最终二进制重新验证了生产输出。

## 当前工程证据

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
