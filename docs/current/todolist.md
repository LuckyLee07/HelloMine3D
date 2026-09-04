# HelloMine3D 当前待办

本文只回答：项目现在做到哪里、当前批准什么、下一候选是什么、什么会阻塞开发。详细历史、合同和
封板证据分别进入 `docs/archive/`、`docs/contracts/` 和 `docs/reports/`。

最后更新：2026-09-04。

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
| AI/Computer Use | `AI-01..AI-08=NOT_RUN`；没有外部玩家依赖，具备 OS 级 Computer Use 时按当前验收规范执行。 |
| 人类体验 | 乐趣、审美、舒适度和物理设备手感统一为 `NOT_CLAIMED`。 |

PLAYABILITY-RC 发行 ZIP SHA-256：
`422F97E87046D4B6D5FC4BB99C37886FF37C4461A152C1162FA66A972B12F459`。

## 当前批准批次

| 批次 | 状态 | 当前结论 |
| ---- | ---- | -------- |
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

| 批次 | 状态 | 目标 | 进入条件 | 退出边界 |
| ---- | ---- | ---- | -------- | -------- |
| `D2` Simulation Activation | `Candidate / not approved` | 在已 Resident 空间内区分 Full / Reduced / Dormant 模拟保真度。 | D1 完整门禁通过，且至少一个真实远距离 Actor/Machine workload 证明仅靠 item budget 仍不足。 | 不由 B6 spatial interest 或 D1 自动授权；不得提前开始 D3-D8。 |

`B10` 已完成正式压力/确定性、组成式完整门禁和 Q1 收口；`C1-C3` 也已依次通过完整门禁。
项目所有者已单独批准并完成 D1；当前没有已批准但未完成的开发批次。B7-B9、C4-C11、D2-D8
与 Extended 仍未获批，也不因 D1 完成自动进入实现。

## 当前阻塞

- 工程开发没有已知主干阻塞。
- 当前任务环境若没有 OS 级 Computer Use，AI 场景保持 `NOT_RUN`；这不阻塞独立 Sprint 的
  `Engineering Done`，但 Track 不得标记 `AI Playability PASS`。
- 严格 `AI-06` 还要求 package-only 文件系统访问；仅切换工作目录但仓库仍可读取时记录
  `BLOCKED`，不能声明 blind PASS。
- 当前没有正在执行的开发批次；D2 只是候选，需另行批准。

## 待执行验收

| 范围 | 状态 | 关闭方式 |
| ---- | ---- | -------- |
| `AI-01..AI-04` 基础窗口功能 | `NOT_RUN` | 在 Windows Release 干净包中以正常 OS 输入执行菜单、容器、战斗、保存和重启。 |
| `AI-05` Stage 11 scripted | `NOT_RUN` | 制作/放置火把、建造、工具职责、探索奖励、洞口、战斗和 Waystone 共鸣。 |
| `AI-06` AI 盲玩 30 分钟 | `NOT_RUN` | 在仓库不可访问的 package-only 新任务中执行；只声明 AI 可理解性，不外推人类留存或乐趣。 |
| `AI-07` 视觉/本地化/音频 | `NOT_RUN` | 从带哈希干净包使用真实窗口、多帧/连续观察和可访问录音检查；现有 render capture 多帧能力只作开发预检。 |
| `AI-08` 完整可玩载体 | `NOT_RUN` | 每个实际完成 Track 结束时，从主菜单运行到胜利、保存重开和该 Track 的正常玩法 Demo。 |
| 新 macOS 视觉/玩法运行 | `NOT_RUN` | 只有后续批次明确纳入退出范围时执行。 |

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
