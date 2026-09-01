# HelloMine3D AL-A4 Event / Command / Query Boundary Report v1

日期：2026-09-01

批次：`AL-A4 — Event / Command / Query Boundary`

工作流状态：`Done`

本报告记录既有玩家交互 FIFO 与 World-local 事件总线的语义分离及自动验证。它不声明新的
Gameplay、Q1/Q3 性能、AI 可玩性或人类主观体验结论。

## 1. Implementation Result

旧 `IWorldEvent / PlayerDigEvent / World::addEvent` 路径实际承载的是“请求改变世界”的操作，现已由
`IWorldCommand / PlayerBlockInteractionCommand / World::addCommand` 单一替代。Break、Use、Place
仍进入原有 frame-owned FIFO，并在 `World::update` 中按提交顺序各执行一次；没有新增 scheduler、
跨帧队列或第二套兼容路径。

`SandboxEventBus` 继续由每个 World 独立拥有并同步分发“已经发生的事实”。事件 type/category
不可变，category 明确分为 `Domain` 与 `Diagnostic`。生产订阅者必须声明 owner、
`ObserveOnly / DomainMutation` effect 和 `Forbidden / Bounded` republish：

| Owner | Effect | Republish | Bounded result |
| ----- | ------ | --------- | -------------- |
| `ObjectiveSystem` | `DomainMutation` | `Forbidden` | objective/recipe-discovery state only |
| `World.WaystoneGuardianDeath` | `DomainMutation` | `Bounded` | encounter transition and actor facts only |
| `ActionFeedbackTimeline` | `ObserveOnly` | `Forbidden` | transient presentation state only |
| `AudioRuntime` | `ObserveOnly` | `Forbidden` | transient audio state only |

Observer 嵌套发布会被拒绝；显式允许的领域反应受 8 层同步 dispatch 上限约束。每次 publish 使用
入口时的订阅 membership 快照，handler 内 subscribe/unsubscribe 只影响后续或嵌套 publish。
`Diagnostic` 不投递给 `DomainMutation` handler，异常传播后 depth/policy 也会由 RAII 恢复。

## 2. Preserved Behavior and Explicit Limits

- 玩家方块交互结果、目标进度、Waystone 波次、反馈和音频路由保持原语义。
- `World::update`、20 Hz fixed tick、ChunkRuntime、save v12、资源、配方和渲染均未改变。
- AL-A1 的 78 项公开方法数量不变；仅把误导性的公开方法名 `addEvent` 复核为 `addCommand`。
- 现有 mutable manager/reference getters 仍只是兼容逃生口，不成为新 Query 模板。
- 本批没有增加 Machine、Network、通用 registry、事件持久化、异步 bus、jobs、A5 metrics/budget
  或新 Gameplay 内容。

## 3. Static Boundary Evidence

| Gate | Result |
| ---- | ------ |
| `tools\validate_world_responsibility_map.ps1` | `PASS`；78 methods、45 queries、31 commands、2 runtime ticks；A4 public-surface SHA-256 `8B2CDDF30B70DA91D5EF4944D7E1397BC9434EB0E129B9313DA471143F653EC4`。 |
| `tools\validate_chunk_runtime_boundary.ps1` | `PASS`；AL-A2 queue/worker/mesh protocol 边界未回退。 |
| `tools\validate_world_simulation_boundary.ps1` | `PASS`；AL-A3 的 8 phases、caller-owned pause 与无 A5 抽象边界未回退。 |
| `tools\validate_event_command_query_boundary.ps1` | `PASS`；typed command FIFO、2 categories、8 层上限、4 个生产 effect owner 和 8 项行为断言均被冻结。 |
| `git diff --check` | `PASS`；仅有 checkout 的 LF/CRLF 提示，无 whitespace error。 |

四个架构 validator 都已接入 `scripts\verify_build.ps1`，后续完整 Windows 门禁会同时阻止 World
公开面、Chunk、Simulation 与 Event/Command/Query 边界回退。

## 4. Behavioral Regression Evidence

定向 `HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A4` 通过 `35/35`，其中新增 8 项断言：

| Assertion | Result and meaning |
| --------- | ------------------ |
| `current-events-are-immutable-domain-facts` | `PASS`；当前事实 identity 不可在发布后改写。 |
| `observer-delivery-is-synchronous-and-declared` | `PASS`；观察者仍同步接收且 effect 明示。 |
| `observer-cannot-hide-nested-publication` | `PASS`；观察者不能把通知变成隐式 mutation chain。 |
| `declared-domain-reaction-may-publish-bounded-fact` | `PASS`；明确声明的领域反应可发布有界嵌套事实。 |
| `recursive-publication-has-hard-depth-limit` | `PASS`；第 9 层 dispatch 被拒绝并可观察。 |
| `diagnostic-event-cannot-drive-domain-mutation` | `PASS`；诊断事实不能触发权威状态修改 handler。 |
| `subscription-membership-is-snapshotted-per-publication` | `PASS`；当前迭代不受 handler 内订阅修改影响。 |
| `handler-exception-restores-dispatch-boundary` | `PASS`；异常不会污染后续 dispatch depth/policy。 |

Debug 与 Release 的真实 `HelloMine3DWorldRuntimeSmoke` 各通过 `846/846`。既有交互、目标、
Waystone、Actor、反馈、音频、save/reload、Simulation 与 Chunk 回归保持绿色；两种配置的 short soak
也都以 `failures=0` 结束。

## 5. Full Windows Gate and Artifact Identity

执行命令：

```powershell
& .\scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
```

| Evidence | Result |
| -------- | ------ |
| Toolchain | Visual Studio 2017 / v141 / x64 Debug + Release |
| Full solution builds | `PASS`；Debug 2044 warnings / 0 errors，Release 2023 warnings / 0 errors；均为既有 Ogre/第三方兼容警告。 |
| Headless suite | `PASS`；所有 gate targets 在 Debug/Release 通过。 |
| World / resource / recipe | `846/846` twice；`80/80`；`122/122` |
| Startup negative diagnostics | `15/15` |
| Resource identity | 84 manifest entries；278 atlas checks；38 performance fixtures；11 Stage 10 supplement checks |
| Crash/package boundary | `PASS`；validation-only crash diagnostics 和 isolated clean-root package 通过。 |
| Release executable | 8,760,832 bytes；SHA-256 `AE184322BD709A0801C0602BFBBDB3A690D35113F4004BD5D450D6732F076E5F` |
| Verification package | 104 entries；SHA-256 `0E33793CD2F504662712C79184AACECF544C5548C39E496431D2ACACCE07FA77` |

该 ZIP 是本地 AL-A4 验证产物，不替换历史 PLAYABILITY-RC 发行身份，不创建 tag，也不 push。
本批没有重跑正式硬件 Q1/Q3，也没有申请性能例外；EventBus 统计只属于诊断观察。

## 6. Evidence Boundary

为避免抢占项目所有者当前窗口，本次完整门禁显式使用 `-SkipRealWindow`：

| Dimension | Result | Declaration |
| --------- | ------ | ----------- |
| Automated engineering | `PASS` | AL-A4 boundary and preserved automated behavior are claimed. |
| Gate real-window launch | `DEFERRED` | No real-window PASS is claimed by this run. |
| `AI-01..AI-08` | `NOT_RUN` | No AI playability or AI-understandability PASS is claimed. |
| Human fun/aesthetics/comfort/input feel | not executed | `NOT_CLAIMED` |

Hidden/validation-only Ogre startup、headless smoke 和干净包启动边界都不是 Computer Use 验收，不能
替代 `docs/current/ai-assisted-gameplay-acceptance-v1.md`。

## 7. Exit and Next Approval Gate

AL-A4 合同的五项退出条件均已满足，合同状态冻结为 `Frozen after AL-A4 verification`。本报告所在
本地 commit 是 AL-A4 的提交身份，可用
`git log -1 -- docs/reports/architecture-lab-a4-event-command-query-report-v1.md` 解析。

下一候选仅为 `AL-A5 — Tick Phase Metrics & Budget Vocabulary`，状态仍是 `Queued`。AL-A4 完成
不构成 AL-A5、B1、C1、D1 或任何 Extended 能力的批准；开始下一批前必须再次获得项目所有者
独立批准。
