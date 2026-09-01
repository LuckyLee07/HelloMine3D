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
| 自动门禁 | VS2017/v141 双配置、832/832 世界、80/80 资源包、122/122 配方、15/15 启动负例和 104 项干净包通过。 |
| 性能与诊断 | 六类正式 Q1、nominal/stress 各 1800 秒 Q3、崩溃 dump、脱敏 sidecar、离线符号和独立符号归档已闭环。 |
| AI/Computer Use | `AI-01..AI-08=NOT_RUN`；没有外部玩家依赖，具备 OS 级 Computer Use 时按当前验收规范执行。 |
| 人类体验 | 乐趣、审美、舒适度和物理设备手感统一为 `NOT_CLAIMED`。 |

PLAYABILITY-RC 发行 ZIP SHA-256：
`422F97E87046D4B6D5FC4BB99C37886FF37C4461A152C1162FA66A972B12F459`。

## 当前批准批次

| 批次 | 状态 | 当前结论 |
| ---- | ---- | -------- |
| `AL-A0` Latest Architecture Baseline | `Done` | 架构、依赖、性能、验证与 AI 证据身份已冻结；VS2017/v141 Debug/Release 完整门禁和 real window 通过，没有受跟踪 Gameplay/runtime/resource/build input 改动。详见 `docs/reports/architecture-lab-baseline-v1.md`。 |

## 下一候选

| 批次 | 状态 | 目标 | 进入条件 | 退出边界 |
| ---- | ---- | ---- | -------- | -------- |
| `AL-A1` World Responsibility Map | `Queued` | 在 A0 冻结的事实基线上分类 World API，建立 Query / Command / Runtime Tick 责任地图。 | 获得项目所有者独立批准；不得由 A0 完成自动启动。 | 当前只是未批准候选，不构成开发承诺。 |

`AL-A1 World Responsibility Map` 是当前唯一 Queued 候选，不是已批准任务；A0 已在此处停止并
回到所有者批准门。后续 A2 只迁移现有 Chunk 调度行为，B1 才允许引入新的 Residency 状态机。

## 当前阻塞

- 工程开发没有已知主干阻塞。
- 当前任务环境若没有 OS 级 Computer Use，AI 场景保持 `NOT_RUN`；这不阻塞独立 Sprint 的
  `Engineering Done`，但 Track 不得标记 `AI Playability PASS`。
- 严格 `AI-06` 还要求 package-only 文件系统访问；仅切换工作目录但仓库仍可读取时记录
  `BLOCKED`，不能声明 blind PASS。
- AL-A0 已完成；AL-A1 尚未批准，当前没有开发中的 Architecture Lab 批次。

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
