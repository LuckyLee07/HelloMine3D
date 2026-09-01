# HelloMine3D 当前待办

本文只回答：项目现在做到哪里、当前批准什么、下一候选是什么、什么会阻塞开发。详细历史、合同和
封板证据分别进入 `docs/archive/`、`docs/contracts/` 和 `docs/reports/`。

最后更新：2026-09-01。

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
| 自动门禁 | VS2017/v141 双配置、884/884 世界、80/80 资源包、122/122 配方、15/15 启动负例和 104 项干净包通过。 |
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

## 下一候选

| 批次 | 状态 | 目标 | 进入条件 | 退出边界 |
| ---- | ---- | ---- | -------- | -------- |
| `B4` Cancellation & Generation Token | `Queued` | 让需求变化可以使正在执行的旧工作失效，并以 generation token 阻止过期结果提交。 | B3 已完成；B3 独立提交后自动开始。 | 不提前实现 B5 Backpressure、B6 Spatial Interest、B7-B9 或通用 Simulation Scheduler。 |

`B3` 已完成实现、聚焦验证和完整门禁，建立独立提交后关闭。B4 是提交后的下一批；
B4-B6/B10 仍须严格按依赖逐批执行，B7-B9 和 Track C/D 未获批。

## 当前阻塞

- 工程开发没有已知主干阻塞。
- 当前任务环境若没有 OS 级 Computer Use，AI 场景保持 `NOT_RUN`；这不阻塞独立 Sprint 的
  `Engineering Done`，但 Track 不得标记 `AI Playability PASS`。
- 严格 `AI-06` 还要求 package-only 文件系统访问；仅切换工作目录但仓库仍可读取时记录
  `BLOCKED`，不能声明 blind PASS。
- 当前没有已知实现阻塞；B3 完整关闭门禁已通过，独立提交后自动进入 B4。

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
