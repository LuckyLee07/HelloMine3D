# HelloMine3D 当前待办

本文只回答：项目现在做到哪里、当前批准什么、下一候选是什么、什么会阻塞开发。详细历史、合同和
封板证据分别进入 `docs/archive/`、`docs/contracts/` 和 `docs/reports/`。

最后更新：2026-09-12。

## 项目目标

HelloMine3D 是 **以真实可玩的单机体素沙盒为载体的 C++ Architecture Lab**。玩法不是可删除的
包装：架构工作必须由游戏内真实需求触发，并能通过正常菜单、输入、世界状态、保存和重开观察。

当前产品载体已完成 Windows PLAYABILITY-RC 工程范围；Architecture Lab roadmap 是能力候选池，
不是自动获批 backlog。只有本文件“当前批准批次”中的任务才构成开发承诺。

## 文档与状态规则

- `docs/current/`：当前状态、Architecture Lab、验收规范、架构和验证操作。
- `docs/contracts/`：已实现批次的冻结语义，不是当前 backlog。
- `docs/reports/`：检查点、RC、调查和视觉记录，不回写历史结论。
- `docs/archive/`：已结束路线和被取代协议，不得作为当前待办。

任务状态、执行结果和声明范围是三个正交维度：

| 维度 | 值 | 含义 |
| ---- | -- | ---- |
| 工作流 | `Queued / Planned / Todo / Doing / Engineering Done / Done` | 表示是否获批和工程是否完成。 |
| 执行结果 | `PASS / FAIL / BLOCKED / NOT_RUN` | 表示某项自动化或 AI/Computer Use 是否实际执行。 |
| 声明状态 | `CLAIMED / NOT_CLAIMED / OUT_OF_SCOPE / SUPERSEDED` | 表示证据允许声明什么，不把主观体验冒充 PASS。 |

例如 `Engineering Done + AI Playability NOT_RUN + human_fun NOT_CLAIMED` 是合法状态；没有运行记录
不得写成 AI PASS。完整口径见 `docs/current/ai-assisted-gameplay-acceptance-v1.md`。

## 当前基线

| 领域 | 当前状态 |
| ---- | -------- |
| 可玩载体 | 创建世界 → 采集 → 制作 → 工具成长 → 冶炼/食物 → 战斗 → 探索 → 路标胜利 → 胜利后事件 → 保存重开已经贯通。 |
| 玩法与视觉 | Stage 9、Stage 10/VISUAL-RC、Stage 11/P11F 已完成 Windows 自动工程范围；Stage 11 待开发代码批次为 0。 |
| 世界可靠性 | 世界目录、事务保存、有界备份、验证恢复、世界管理和主菜单入口已经完成；当前 world save format 为 v12。 |
| 自动门禁 | VS2017/v141 双配置、991/991 世界、80/80 资源包、126/126 配方、15/15 启动负例和 105 项干净包通过。 |
| 性能与诊断 | 六类正式 Q1、nominal/stress 各 1800 秒 Q3、崩溃 dump、脱敏 sidecar、离线符号和独立符号归档已闭环。 |
| AI/Computer Use | `AI-01=BLOCKED（部分通过）`，其余场景未完整执行；正常输入、隔离和音频限制见 Goal 报告。 |
| 人类体验 | 乐趣、审美、舒适度和物理设备手感统一为 `NOT_CLAIMED`。 |

PLAYABILITY-RC 发行 ZIP SHA-256：
`422F97E87046D4B6D5FC4BB99C37886FF37C4461A152C1162FA66A972B12F459`。

## 当前批准批次

2026-09-12 方块选中与破坏表现已接入：贴合花草模型的表面高亮替代黄框，十阶段表面裂纹与
材质碎屑接入真实开采/事件路径。工程和定向视觉证据见[实施记录](../reports/block-feedback-2026-09-12.md)，
不改变命中、硬度、掉落或存档；不替代完整正常玩法或人类审美验收。

2026-09-12 花草摆动生硬问题已修复：连续时间、缓慢阵风和根部固定接入普通/阴影路径，
GPU 10/10、资源 105/105、实机动态序列与定向性能通过，见[修复记录](../reports/flora-wind-fix-2026-09-12.md)。
后续按用户反馈将连续摆速提高约两倍、增强弱风摆幅；GPU 对齐检查和 Off/High 动态复查通过，见同记录的后续调整。

2026-09-12 用户截图中的水面区块接缝修复已完成：波浪改用世界坐标，210 对 GPU 边界采样
位置/法线误差归零；岸边与水下实机、资源/相关回归及定向性能对照完成。首组流送性能异常
与全部复测保留，见[修复记录](../reports/water-seam-fix-2026-09-12.md)。这项局部修复不扩展水系或改变存档。

2026-09-12 后续授权：开始暖野 v2 首轮 `WV2-00..03`，交付高清自然材质、单元格内叶簇与
可正常游玩的林地样板 M1。用户再次反馈后已撤回新树冠，当前候选恢复旧立方叶/叶贴图、保留其他材质升级；整体仍 `Doing`。补验后 macOS 双配置工程门禁通过，
旧候选包林地 High 帧 P95 为 v1 的 1.215 倍；当前包完整复测该档通过，但密林 Medium
P95 为 1.156 倍，仍超过 1.10 性能护栏。正常移动/采集仍缺可靠输入证据。
见 [首轮合同](../contracts/warm-wilderness-m1-contract-v2.md)、[实施记录](../reports/warm-wilderness-m1-implementation-2026-09-12.md)
与 [M1/T1 补验](../reports/m1-t1-acceptance-continuation-2026-09-12.md)。
`WV2-04..08` 仍为后续候选；本轮不改生成规则、save v12 或 D2。

2026-09-13 R1 当前旧叶包在用户批准的同机同期口径下性能通过，但用户以封面和真实试玩
截图指出树冠视觉不合格；持续移动/采集与移动画面仍缺证据。用户要求重新制定
[封面观感树冠升级方案](r1-canopy-cover-style-plan-2026-09-13.md)，当时方案已完成、尚未实施；
R1 仍 `Doing`，旧包不作为完成版提交，也不进入 R2。当前逐项状态见
[R1 验收报告](../reports/goal-r1-m1-acceptance-2026-09-12.md)。
用户随后明确要求落实该方案并交付最终实机对比；标准路径树叶外观已获 R1 内修订授权，
旧包性能和视觉结果不继承给新包。
现已做出只改 16 个标准树叶层的新 R1 候选及
[实机前后对比](../reports/r1-canopy-cover-comparison-2026-09-13.md)，并制作 109 文件
独立包。macOS 双配置工程/资源门禁 PASS，正式六组同机成对性能中五组 PASS，
林地 High P95 **1.831× FAIL**；旧 R1／新叶六轮定位为 1.043×，仅作诊断不覆盖
正式失败。正常创建/设置/保存重开 PASS；持续移动/采集与移动视频 BLOCKED。
R1 保持 `Doing`，不作为完成版提交，也不进入 R2。

2026-09-13 用户进一步拒收斜切低多边形树冠，要求完整方叶块轮廓。
R1 当前改为原创方像素叶纹理＋**仅新世界 terrain v6** 的错层橡树；旧存档
v1–v5 树块布局、其他地貌/遗迹规划及玩法语义保持。第一版 v6 62 个叶格过密、
压低视线，已按同机位实机图继续收敛叶块量和叶色，尚未形成正式验收版。
旧新候选包、失败数据与重拍原图均保存于 `build/goal-r1-canopy-shape-20260913/`。
R1 仍 `Doing`；不得沿用上一候选的性能 PASS、提交或进入 R2。
新一轮 v1/v6 林地／密林六档各三轮的 36 次正式采样现全部完整；P95/P99
中位数及密林常驻 buffer 均 PASS，原始长帧与旧版失败分别保留，见
[方块橡树验收记录](../reports/r1-voxel-oak-acceptance-2026-09-13.md)。
Debug 受影响目标、15 个启动资源负例、最终独立包五组固定画面亦 PASS。
用户解锁 Mac 后，最终包隔离副本的正常菜单建档/载入、暂停设置与保存重开
PASS；持续移动/转头、采集和移动视频受当前 Computer Use 缺少按住输入接口
限制而 BLOCKED，原始短按/拖动、窗口图与存档哈希已保存。用户实际试玩后
认可当前树冠效果，并明确要求按此版本本地提交一版；该授权允许提交 R1
**版本检查点**，不把缺失的正常玩法证据改判 PASS。R1 整轮仍 `Doing`，
不宣布完成、不进入 R2、不推送或发布。

2026-09-13 用户指出当前生态生成质量不合格，明确调整顺序为先做生态地形改造，
再以改造后的实际版本收口 R1/M1 和 R2/T1；此前“先 R1、再 R2、再生态”的
执行顺序被这次授权取代。生态批次仍严格逐轮验收、获确认后本地提交；
本批仅 E1 森林生态 v7 已获批准，范围与退出条件见
[E1 合同](../contracts/ecology-forest-v7-e1-contract-v1.md)。R1/R2 已通过的旧版工程证据
保留，受 v7 影响的视觉、性能、正常玩法项待改造后重新验收，不预先宣称通过。
用户已认可当前 E1 视觉效果并明确授权“这版可以提交”；据此做本地检查点提交。
2026-09-14 收口复核：Debug/Release WorldRuntime 均 1043/1043，固定客户端的
v6/v7 森林常驻性能 PASS；快速流送首轮 P95 FAIL（1.645 倍），反序复核 PASS、
六次汇总 PASS，原始失败与波动风险保留。Computer Use 完成中文菜单建档、保存重开，
当时游戏内持续移动/转头/采木未取得可验证输入证据。用户随后亲自试玩完成采木、
合成并摆放工作台；旧客户端保存 FAIL，失败候选保留了位置、转向、进度与工作台。
已修复掉落物次正规速度导致的 macOS 存档读回错误，从原候选救援用户进度，
固定客户端原位刷新后正常菜单保存及同世界重开 PASS。两配置存档相关回归通过，
原始失败、恢复和修复版均有独立哈希包。修复版现已重新取得同包 v6/v7 森林、
密林固定原图；森林常驻三轮 P95/P99 比值为 0.999/1.079，快速流送为
0.984/0.999，均 PASS。旧包首轮快速流送 FAIL 未查明原因，密林固定机位近树干
遮挡及缺少独立连续玩法视频明示为剩余风险。修复版干净审查包 SHA-256 为
`4257be0cdc9ecfc7eb633d92b7b88b14c1620eb935c01867f5cf0350fb7243cf`，
原始证据包 SHA-256 为
`e6df0444aa5117ca9cc5ce32feb05b5ea0e28b5328076e6332dfe0794b42ae82`。
详见 [E1 执行报告](../reports/ecology-e1-forest-v7-execution-2026-09-13.md)。
用户已于 2026-09-14 明确确认 **E1 整轮验收通过**；E1 状态为 `Done`，
完成版仅本地提交。已验收的包和证据哈希不变，旧包流送首轮 FAIL、密林近树
遮挡与无独立连续玩法视频继续作为已披露风险保留。E2 尚未启动，R1/R2 也
不因 E1 完成自动收口。

2026-09-12 新增授权：实施“暖野”第一版视觉改造，包括自然材质、HUD/菜单、地形光色及 macOS
实机与必要工程验证。保持地形和存档语义；不扩展树形、水系或 D2。见
[视觉合同](../contracts/warm-wilderness-visual-contract-v1.md) 与
[实施记录](../reports/warm-wilderness-implementation-2026-09-12.md)，当前 `Engineering Done`：材质/光色/UI、macOS 双配置回归、三档性能和正常界面自测完成；不替代独立玩法与人类审美验收。

2026-09-06 新增所有者授权：`T0/T1` 地形基线与基础地貌改善 Goal。以 macOS 完成实现、
必要回归、干净包、固定条件画面对照和正常步行验收，并本地提交；Windows 专属验证后置。
范围和退出条件见 [T0/T1 合同](../contracts/terrain-foundation-v5-contract-v1.md)，
进度见 [执行报告](../reports/terrain-t0-t1-execution-2026-09-06.md)。不扩展到 D2/T2+ 或新渲染系统。

| 批次 | 状态 | 当前结论 |
| ---- | ---- | -------- |
| `T0` Terrain Baseline | `Done` | 8 seed、1852224 点、128 区块兼容快照、三场景原图、三轮静态/流送性能及实现前冻结门槛已本地提交 `6dbf7eb`；详见执行报告。 |
| `T1` Terrain Foundation v5 | `Doing` | 实现已提交 `850dd85`；v1–v4 指纹兼容、v5 统计与 23 项聚焦、当批 macOS 双配置 1014 项世界回归、干净包固定画面及三轮性能通过。独立正常创建/保存重开通过；当前包再测世界与设置仍可操作，但持续步行、采集与结构路线因 CUA 输入能力受阻，T1/Goal 未完成，详见报告及 [补验](../reports/m1-t1-acceptance-continuation-2026-09-12.md)。 |
| `AL-A0` Latest Architecture Baseline | `Done` | 架构、依赖、性能、验证与 AI 证据身份已冻结；VS2017/v141 Debug/Release 完整门禁和 real window 通过，没有受跟踪 Gameplay/runtime/resource/build input 改动。详见 `docs/reports/architecture-lab-baseline-v1.md`。 |
| `AL-A1` World Responsibility Map | `Done` | 78 个公开方法已按 3 个 API concept / 9 个 responsibility 分类；public-surface hash 和集合一致性门禁已接入完整 Windows 验证，VS2017/v141 Debug/Release、832/832 世界、80/80 资源、122/122 配方、15/15 启动负例、104 项干净包与 real window 全部 PASS。没有迁移旧调用、增加 Facade wrapper 或开始 AL-A2。 |
| `AL-A2` Chunk Runtime Boundary | `Done` | 既有 Chunk Update Queue、Mesh Work Planner、单 loader 及 preload/unload 协调已迁入 `ChunkRuntime`；World 公开面、共享锁、预算、save v12 与 unload 语义保持不变，没有引入 B1 Residency 状态机。VS2017/v141 Debug/Release 完整门禁、两轮 832/832 世界和 104 项干净包通过。详见 `docs/reports/architecture-lab-a2-chunk-runtime-report-v1.md`。 |
| `AL-A3` Simulation Runtime | `Done` | 具体 `WorldSimulation` 已承接既有 20 Hz fixed-tick 编排；8 phase 顺序、context、最近一次 tick 原始耗时和 caller-owned pause 均有自动门禁，玩法所有权与 78 项 World 公开面不变，没有引入 A5 Scheduler/Metrics/Budget。VS2017/v141 Debug/Release、两轮 838/838 世界和 104 项隔离包通过。详见 `docs/reports/architecture-lab-a3-world-simulation-report-v1.md`。 |
| `AL-A4` Event / Command / Query Boundary | `Done` | `IWorldCommand / addCommand` 已取代旧 event-as-command 路径；事实事件不可变并区分 Domain/Diagnostic，生产订阅者显式声明 effect/republish，有界递归、诊断隔离和订阅快照语义均有自动门禁。VS2017/v141 Debug/Release、846/846 世界与 104 项隔离包通过；没有进入 A5。详见 `docs/reports/architecture-lab-a4-event-command-query-report-v1.md`。 |
| `AL-A5` Tick Phase Metrics & Budget Vocabulary | `Done` | Actor、Combat、Block Random Tick、Population 的 last-tick processed/deferred/budget scope/status 词汇和开发者面板已经冻结；VS2017/v141 Debug/Release、两轮 853/853 世界与 104 项隔离包通过，没有改变 phase 顺序、hard limit 或 Gameplay，也没有进入 A6/B1/D1。详见 `docs/reports/architecture-lab-a5-simulation-metrics-report-v1.md`。 |
| `AL-A6` Architecture Lab Documentation Pipeline | `Done` | 单一 living tutorial 已按 Track/真实 Section 重组；manifest、七段非空结构、证据路径和无占位 Part 由新 validator 及四个负例保护。VS2017/v141 Debug/Release、两轮 853/853 世界与 104 项隔离包通过；运行时代码、Gameplay、save v12 与 AI/人类声明均未改变。详见 `docs/reports/architecture-lab-a6-documentation-pipeline-report-v1.md`。 |
| `B1` Chunk Residency State Machine | `Done` | Data Residency、CPU Mesh 与 Ogre Render 已成为三套正交且有断言保护的状态机；38/38 聚焦用例覆盖光照失效、保存失败回滚、dirty eviction、stale build/upload、卸载持久化和直接结构生成/重载。VS2017/v141 Debug/Release 完整门禁均为 863/863 WorldRuntime，104 项干净包 SHA-256 为 `F76F5A1429C869FDA94FDD37BF34F8266866B5F665D1E550CA04F15CCD7ECF88`。没有进入 B2-B9，Gameplay、预算和 save v12 保持不变。详见 `docs/reports/architecture-lab-b1-chunk-residency-report-v1.md`。 |
| `B2` Streaming Demand Model | `Done` | Player、Camera、TeleportDestination、Preload 已统一为四槽、可合并、可过期的需求模型；26/26 聚焦用例与 AL-A1..B1 静态门禁通过。VS2017/v141 Debug/Release 完整门禁均为 875/875 WorldRuntime，104 项干净包 SHA-256 为 `9955E51AD94C7E5600325329031432E16C1859F936FA880F68F65F3B80369503`；未进入 B3-B9。详见 `docs/reports/architecture-lab-b2-streaming-demand-report-v1.md`。 |
| `B3` Generic World Job Scheduler | `Done` | 两类真实后台工作已进入 typed scheduler；9/9 聚焦用例、B2 26/26 与 B1 38/38 回归通过。VS2017/v141 Debug/Release 完整门禁均为 884/884 WorldRuntime，104 项干净包 SHA-256 为 `B983889B5553FF0DBFEAF6C14D2E349CC81AD1205C314CC39C0894C2D1CD9459`。未进入 B4-B9，Gameplay、预算与 save v12 保持不变。详见 `docs/reports/architecture-lab-b3-world-job-scheduler-report-v1.md`。 |
| `B4` Cancellation & Generation Token | `Done` | uint64 generation、`Cancelled` outcome、六类失效入口、detached load candidate、线性化提交和 mesh 回滚已实现；B4 10/10，B3/B2/B1 回归 9/9、26/26、38/38。VS2017/v141 Debug/Release 完整门禁均为 894/894 WorldRuntime，104 项干净包 SHA-256 为 `A9A7CC9AF528F3C725ACC13A62A69FB6D10718AD25F7F4796F5E47B70453C33A`。未进入 B5-B9，Gameplay、预算与 save v12 保持不变。详见 `docs/reports/architecture-lab-b4-world-job-cancellation-report-v1.md`。 |
| `B5` Streaming Backpressure | `Done` | 两类真实 job 已受 128 hard cap、96/48 watermarks、显式 admission、确定性 shedding 和 plan-window refill 约束；loader commit、CPU-ready upload、unload 分别受 8/8/8 边界保护。B5 12/12 及 B4/B3/B2/B1 10/10、9/9、26/26、38/38 已通过；完整 VS2017/v141 Debug/Release 门禁均为 906/906 WorldRuntime，104 项干净包 SHA-256 为 `7D126B31B78F3A4E8F8C90A5D769028EC686C0D4F708D1F6B2E2979BD164050B`。详见 `docs/reports/architecture-lab-b5-streaming-backpressure-report-v1.md`。 |
| `B6` Spatial Activation | `Done` | Resident Data、Near Representation、Simulation Requested 三层兴趣已接入真实 plan/load→mesh→render/unload 路径，simulation request 只发布不消费。B6 12/12、B5/B4/B3/B2/B1 回归 12/12、10/10、9/9、26/26、38/38；完整 VS2017/v141 Debug/Release 门禁均为 918/918，104 项隔离包 SHA-256 为 `C8E260E00CF76C952150EBC3DC851A7EDE5E13FE63A58F98B77DC103723EFA3C`。详见 `docs/reports/architecture-lab-b6-spatial-activation-report-v1.md`。 |
| `B10` Large World Stress & Acceptance | `Done` | 最终 schedule v3 Release Core 已完成 1800 秒/36000 ticks、五阶段、10 次保存重开和双确定性探针：最大区块 216、最大 pending 95、消费者 8/8/8、最大/最终 `Absent=0`，峰值 private 137551872 bytes，wall 1824.453 秒且未超时。两次失败均保留并修复，未放宽阈值或使用性能例外。组成式 VS2017/v141 Debug/Release 门禁均为 920/920；最终 Q1 六组比较 PASS；104 项隔离包 SHA-256 为 `E13203F8E18382A4D13ABA22DFB975DDB189B2858F74DAAE340CE5A4A8F34B14`。详见 `docs/reports/architecture-lab-b10-large-world-stress-report-v1.md`。 |
| `C1` Block Capability Model | `Done` | 现有 Chest/Furnace 已通过 `BlockDefinition` 声明 `InventoryProvider`，Furnace 另声明 `MachineProcessor`；Ogre 容器 UI 改为能力发现/访问，17/17 聚焦用例与静态门禁通过。VS2017/v141 Debug/Release 完整门禁均为 937/937 WorldRuntime，104 项隔离包 SHA-256 为 `1618ACD7995FE5181169B0B46A5D4F479F63FA1CCB8B533B358ED694A3846EB6`。未预建 Registry、MechanicalPort、C2 Machine Runtime 或 C3 网络。详见 `docs/reports/architecture-lab-c1-block-capability-report-v1.md`。 |
| `C2` Machine Runtime v0 | `Done` | 可制作、放置、Use、保存重开的手摇 Crusher 已成为第二个真实 Processor；共享 Runtime 只提炼 Furnace/Crusher 已共同证明的五态、配方匹配、输出容量、动力、单 tick 与原子完成语义。C2 聚焦 51/51、完整 VS2017/v141 Debug/Release WorldRuntime 963/963、Recipe 126/126、Resource Pack 80/80 和 15/15 启动负例均通过；105 项隔离包 SHA-256 为 `B4D73704A93B4377EB26336592448B4E31439387BA6198B49C59704517775739`。save v12、terrain v4、settings v8、8 工具和 34 目标不变；未进入 C3+、MechanicalPort、网络或自动物流。详见 `docs/reports/architecture-lab-c2-machine-runtime-report-v1.md`。 |
| `C3` Mechanical Topology Model v0 | `Done` | C2 Crusher 是唯一真实节点；六面相邻、确定性最小位置 component id、canonical edge、同步 BFS merge/split、Chunk unload/reload 和 save/reopen 派生重建已完成，正常容器 UI 与 Debug 面板均可观察。聚焦 Debug 68/68；完整 VS2017/v141 Debug/Release 均为 WorldRuntime 980/980、Recipe 126/126、Resource Pack 80/80 和启动负例 15/15；105 项隔离包 SHA-256 为 `8CC3ED1FC37A0F115D57C3C56349BE3278AA4B35D9DAD33975D9B45B3D46776F`。save v12 不变，未进入 C4 动力传播或通用网络。详见 `docs/reports/architecture-lab-c3-mechanical-topology-report-v1.md`。 |
| `D1` Simulation Phase Scheduler v0 | `Done` | 已证明 Managed Actors、Random-Tick Sections、Furnace/Crusher Block Entities 三条真实 workload 的共同 admission 问题，并实现确定性 64/4/32 item budget、稳定集合 round-robin/FIFO service window 和 copied diagnostics。VS2017 Debug 聚焦 24/24，AL-A5 14/14、B6 12/12、C2 51/51、C3 68/68 回归通过；完整 Debug/Release 门禁均为 WorldRuntime 991/991、Recipe 126/126、Resource Pack 80/80、启动负例 15/15，105 项隔离包 SHA-256 为 `0B34CD34265ED1A4F88FD5833975FD328FB026FCD6B13A0FAFE9710859F1B2F6`。save v12、20 Hz、8 phase barrier 不变；未进入 D2+。详见 `docs/reports/architecture-lab-d1-simulation-phase-scheduler-report-v1.md`。 |

## 下一候选

2026-09-12 所有者要求的视觉升级方案已完成，见
[暖野视觉升级实施方案 v2](visual-upgrade-plan-v2.md)，依据
[暖野 v1 实施结果](../reports/warm-wilderness-implementation-2026-09-12.md) 与本项目现有画面问题分析。
方案列出 `WV2-00..08` 的资源规格、文件触点、兼容策略、性能预算和验收；建议首先实施
`WV2-00..03` 的高清材质/叶簇林地样板，现已由后续用户指令授权并纳入上方当前批准批次。
`WV2-04..08` 仍为 `Planned / NOT_RUN`；不改变 T1 未完成状态、D2 候选条件或 T2+ 范围。

2026-09-06 所有者要求的地形/游戏画面文档分析已完成，形成负坐标基础地貌、
水系、植被、洞口、材质与三维山体的候选建议及验收方式。
其中 T0/T1 已由后续 Goal 明确授权并纳入上表；T2+ 和渲染候选仍未批准，不改变下表 D2 状态。

| 批次 | 状态 | 目标 | 进入条件 | 退出边界 |
| ---- | ---- | ---- | -------- | -------- |
| `D2` Simulation Activation | `Candidate / not approved` | 在已 Resident 空间内区分 Full / Reduced / Dormant 模拟保真度。 | D1 完整门禁通过，且至少一个真实远距离 Actor/Machine workload 证明仅靠 item budget 仍不足。 | 不由 B6 spatial interest 或 D1 自动授权；不得提前开始 D3-D8。 |

`B10` 已完成正式压力/确定性、组成式完整门禁和 Q1 收口；`C1-C3` 也已依次通过完整门禁。
项目所有者已单独批准并完成 D1。2026-09-05 又批准本次 TODOLIST Goal：调查 D2 进入条件、
补充待执行验收并修正文档一致性；若真实 workload 证明 D1 budget 不足，可直接实施 D2。
当前调查观察到 32 台远处空闲 Furnace 会使近处 Crusher 在 20 ticks 内推进 19 步，符合 D1
已有超载合同；尚未建立正常玩法或正式性能不可接受的证据，故本次保留 D2 Candidate，未开始
运行时实现。详见 [D2 进入调查](../reports/simulation-activation-entry-investigation-2026-09-05.md)。
B7-B9、C4-C11、D3-D8 与 Extended 不在此次 Goal 范围。

本次 Goal 仍在执行。所有者随后明确授权以 macOS 完成可对应的测试、干净包与窗口验收，
Windows 专属验证后置，不再作为本次 Goal 的必需退出项；已有 Windows 历史证据不改写。
当前布局修复已通过 macOS Xcode Debug/Release 的 13 套回归及客户端探针
（`build/xcode-validation-20260905143633`）。此前修订的 gmake 双配置回归、
gmake/Xcode Release 启动负例各 15 类通过，保留原版本证据，不视作本轮重新执行。
Xcode Release 已去除会破坏 infinity/isfinite 校验的 fast-math。主菜单鼠标误捕获已修复，
并通过菜单、合成及暂停界面的实际点击复查。独立 AI 验收尚未关闭；进度、命令和环境限制见
[执行记录](../reports/todolist-goal-execution-2026-09-05.md)。候选池不会随本次更新自动扩张。

## 当前阻塞

- 工程开发没有已知主干阻塞。
- 独立窗口绑定和截图保存已恢复，不再作为当前阻塞。工具脚本及真实截图证据见
  [脚本窗口报告](../reports/macos-script-window-20260906-evidence/report.md)。独立新六组合验收
  曾受子任务授权上下文拒绝影响，所有者现已明确批准新独立验收任务，实际执行结果待回收。历史原因见 [拒绝记录](../reports/ai07-ui-matrix-20260906-evidence/independent-report.md)；
  不把该拒绝泛化为整个桌面权限不可用，也不通过其他工具重试被拒绝的创建操作。
- 当前任务环境若没有 OS 级 Computer Use，AI 场景保持 `NOT_RUN`；这不阻塞独立 Sprint 的
  `Engineering Done`，但 Track 不得标记 `AI Playability PASS`。
- 严格 `AI-06` 还要求 package-only 文件系统访问；仅切换工作目录但仓库仍可读取时记录
  `BLOCKED`，不能声明 blind PASS。
- 本次 Goal 以 macOS 为当前验收平台；Windows 11 虚拟机正在执行用户自己的编译，按所有者
  后续指示后置 Windows 验证，不干扰编译。macOS PASS 不替代历史或新的 VS2017/v141 PASS。
- D2 进入调查已给出保留候选的决定；后续若出现新证据，再按本次条件式授权评估，不能从
  `Candidate` 直接推导 `Done`。

## 待执行验收

本次 Goal 的所有者授权 macOS 平台对应执行下列功能场景。表内原 Windows 场景定义保留作为
后续 Windows 路由；新增证据使用 `platform=macOS` 独立记录，不合并为跨平台 PASS。
AI-06 的独立 package-only 访问要求及音频实际证据要求继续有效。

| 范围 | 状态 | 关闭方式 |
| ---- | ---- | -------- |
| `AI-01` 基础窗口功能 | `BLOCKED（macOS 部分通过）` | 独立验收已验证菜单、建档/读档、暂停及语言持久化；移动/视角/按住键受工具限制。快捷键缺陷已修复并通过独立复测，见 `../reports/ai01-macos-6916867-evidence/report.md`。 |
| `AI-02..AI-04` 容器/战斗/旅程 | `NOT_RUN` | 本次在 macOS Release 干净包中以正常 OS 输入执行菜单、容器、战斗、保存和重启；Windows 路由后置。 |
| `AI-05` Stage 11 scripted | `NOT_RUN` | 制作/放置火把、建造、工具职责、探索奖励、洞口、战斗和 Waystone 共鸣。 |
| `AI-06` AI 盲玩 30 分钟 | `BLOCKED（隔离环境）` | 当前工具仍允许读取仓库，尚不满足 package-only；须在仓库不可访问的新任务中执行，只声明 AI 可理解性，不外推人类留存或乐趣。 |
| `AI-07` 视觉/本地化/音频 | `BLOCKED（部分视觉通过）` | 创建表单/目标HUD双语三档复验通过，十位seed列双语大字号及低档复验通过；已验证真实截图无损落盘路径；2026-09-06 独立图片补采因窗口绑定长时间无响应未执行，脚本启动与父上下文菜单诊断已记录；随后新独立上下文已完成主菜单/Worlds往返且原始截图已保存；直接脚本截图也已验证，替代编辑器转存。新六组合验收在创建测试世界时被自动审批拒绝，尚未执行；完整动态和音频证据仍缺失。见本次执行记录，不声明整体AI-07 PASS。 |
| `AI-08` 完整可玩载体 | `NOT_RUN` | 每个实际完成 Track 结束时，从主菜单运行到胜利、保存重开和该 Track 的正常玩法 Demo。 |
| 新 macOS 视觉/玩法运行 | `BLOCKED（部分通过）` | 已由本次 Goal 后续指示纳入范围；从当前源码构建的带哈希干净 Release 包按对应 AI 场景执行。 |

历史 R3 v1 / Physical Input v2 门槛保持 `SUPERSEDED`；开发者既有部分自测继续作为历史证据，
不改写成 AI 或真人 PASS。

## 历史摘要

- 拆分前正式总账：73 个 `Done`、4 个历史 `Verify`，详见
  `docs/archive/project-ledger-2026-08-17.md`。
- Stage 9/BETA-RC 工程封板已完成，详见
  `docs/reports/beta-release-candidate-report-2026-08-26.md`。
- Stage 10/VISUAL-RC Windows 工程封板已完成，详见
  `docs/reports/visual-release-candidate-report-2026-08-28.md`。
- Stage 11/P11F PLAYABILITY-RC Windows 工程封板已完成，详见
  `docs/reports/playability-release-candidate-report-2026-08-31.md`。
- 已结束的视觉与可玩性路线分别冻结在
  `docs/archive/visual-quality-roadmap.md` 和
  `docs/archive/playability-experience-roadmap.md`。

## Architecture Lab 约束摘要

- Capability Map ≠ Backlog。
- Core ≠ 自动批准；Extended 未触发时不进入任务账本，不形成延期欠账。
- A2 = refactor existing behavior；B1 = introduce new runtime lifecycle。
- A5 = phase metrics vocabulary；D1 = scheduler/runtime behavior change。
- B6 = Streaming / representation interest；D2 = simulation fidelity。
- Concrete Mechanical Network → 第二个已批准具体网络 → 观察重复 → 可选 Shared Network Core。
- 教程先维护一份 `docs/current/architecture-lab-tutorial.md`，内部五个 Part；不按 Sprint 新建文件。
