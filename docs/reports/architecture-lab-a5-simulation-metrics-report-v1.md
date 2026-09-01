# HelloMine3D AL-A5 Simulation Phase Metrics Report v1

日期：2026-09-01

批次：`AL-A5 — Tick Phase Metrics & Budget Vocabulary`

工作流状态：`Done`

本报告记录四条真实 Simulation phase 的复制诊断词汇及自动验证。它不声明新的 Gameplay、
Q1/Q3 性能、AI 可玩性或人类主观体验结论。

## 1. Implementation Result

AL-A5 保留 AL-A3 的 8 phase 顺序、20 Hz fixed tick、caller-owned pause 和 last-tick snapshot，
只为存在真实工作量语义的四个 phase 增加统一观察字段：

| Phase | Processed | Deferred | Existing budget |
| ----- | --------- | -------- | --------------- |
| `ActorSimulation` | PlayerActor 调用加实际 tick 的 live managed actor 数 | `0` | `Unbudgeted` |
| `Combat` | 本 tick 实际执行的 projectile step | 因既有上限未执行的 projectile step | `32 / tick` |
| `BlockRandomTick` | 本 tick 实际处理的有效 active section | 仍留待后续轮转的 active section | `4 / tick` |
| `Population` | 本周期实际执行的 natural spawn attempt；非周期 tick 为 `0` | `0`，因为当前没有 retained population queue | 当前难度的 attempts / cycle |

统一字段为 `elapsedMilliseconds / processed / deferred / budget / budgetScope`；状态只由字段派生为
`Unbudgeted / WithinBudget / AtBudget / WorkDeferred`。A3 phase timing 与对应 metric 使用同一份
elapsed 样本，开发者 Simulation 面板显示复制值和派生状态，但这些值不反向决定执行。

## 2. Preserved Behavior and Explicit Limits

- `ActorManager::tick` 只返回实际调用计数，actor 循环和清理顺序不变。
- Combat 继续使用既有 32-step hard limit；Random Tick 继续使用既有 4-section hard limit；Population
  继续使用当前难度的周期 attempt 数。
- save v12、资源、配方、输入、渲染结果、phase 顺序和 Gameplay 均未改变。
- 本批没有平均值、百分位、毫秒阈值或新的 Q1/Q3 结论。
- 本批没有 `SimulationScheduler`、`ISandboxSystem`、系统 Registry、priority、job、取消令牌、新队列、
  持久化 metric 或未来 Machine/Network/AI/Transport 空槽。

## 3. Static Boundary Evidence

| Gate | Result |
| ---- | ------ |
| `tools\validate_world_responsibility_map.ps1` | `PASS`；78 methods、45 queries、31 commands、2 runtime ticks；public-surface SHA-256 `8B2CDDF30B70DA91D5EF4944D7E1397BC9434EB0E129B9313DA471143F653EC4`。 |
| `tools\validate_chunk_runtime_boundary.ps1` | `PASS`；AL-A2 queue/worker/mesh protocol 边界未回退。 |
| `tools\validate_world_simulation_boundary.ps1` | `PASS`；AL-A3 的 8 phases、caller-owned pause 和调用顺序未回退。 |
| `tools\validate_event_command_query_boundary.ps1` | `PASS`；AL-A4 typed command / immutable fact / effect 边界未回退。 |
| `tools\validate_simulation_metrics_boundary.ps1` | `PASS`；4 metric phases、3 budget scopes、4 budget statuses、persistence exclusion 和 speculative scheduler absence 均被冻结。 |
| `git diff --check` | `PASS`；仅有 checkout 的 LF/CRLF 提示，无 whitespace error。 |

五个架构 validator 均已接入 `scripts\verify_build.ps1`，后续完整 Windows 门禁会同时阻止 A1-A5
边界回退。

## 4. Behavioral Regression Evidence

定向 `HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A5` 在 VS2017/v141 Debug 与 Release 各通过 `13/13`；
其中 6 项为 AL-A3 phase 回归，新增 7 项为：

| Assertion | Result and meaning |
| --------- | ------------------ |
| `AL-A5/metric-identities-exclude-empty-system-slots` | `PASS`；只存在四个已实现工作合同，顺序稳定。 |
| `AL-A5/budget-status-vocabulary-is-frozen` | `PASS`；四种状态映射完整且互斥。 |
| `AL-A5/metric-elapsed-matches-a3-phase-timing` | `PASS`；metric 与 A3 phase row 共用一份 elapsed 样本。 |
| `AL-A5/actor-work-is-unbudgeted-and-counted` | `PASS`；PlayerActor 与 live managed actor 被真实计数且不伪造上限。 |
| `AL-A5/combat-uses-existing-per-tick-budget` | `PASS`；processed/deferred 复用既有 32-step 限制。 |
| `AL-A5/random-tick-uses-existing-per-tick-budget` | `PASS`；active-section work 复用既有 4-section 限制。 |
| `AL-A5/population-is-per-cycle-and-resets-next-tick` | `PASS`；Normal 周期报告 `16/16`，随后非周期 tick 归零。 |

Debug 与 Release 的完整 `HelloMine3DWorldRuntimeSmoke` 各通过 `853/853`。既有确定性、Actor、
Combat、Random Tick、Population、save/reload、streaming 和完整玩法回归保持绿色；两种配置的 short
soak 均以 `failures=0` 结束。

## 5. Full Windows Gate and Artifact Identity

执行命令：

```powershell
& .\scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
```

| Evidence | Result |
| -------- | ------ |
| Toolchain | Visual Studio 2017 / v141 / x64 Debug + Release |
| Full solution builds | `PASS`；Debug 2043 warnings / 0 errors，Release 2023 warnings / 0 errors；均为既有 Ogre/第三方兼容警告。 |
| Architecture gates | `PASS`；AL-A1 至 AL-A5 五个静态边界门禁通过。 |
| World / resource / recipe | `853/853` twice；`80/80`；`122/122` |
| Startup negative diagnostics | `15/15` |
| Resource identity | 84 manifest entries；278 atlas checks；38 performance fixtures；11 Stage 10 supplement checks |
| Crash/package boundary | `PASS`；validation-only crash diagnostics 和 isolated clean-root package 通过。 |
| Release executable | 8,762,880 bytes；SHA-256 `14416A52E882EBDE15A41FD8A251AF46D74EE0388A297BFD77596C8097296F2A` |
| Verification package | 104 entries；SHA-256 `4DA15FA1799804FB61710FA285DD0E6818BE1B42CE26AFA25E1F777DF3334009` |

该 ZIP 是本地 AL-A5 验证产物，不替换历史 PLAYABILITY-RC 发行身份，不创建 tag，也不 push。
本批没有重跑正式硬件 Q1/Q3，也没有申请性能例外；last-tick metrics 只属于诊断观察。

## 6. Evidence Boundary

为避免抢占项目所有者当前窗口，本次完整门禁显式使用 `-SkipRealWindow`：

| Dimension | Result | Declaration |
| --------- | ------ | ----------- |
| Automated engineering | `PASS` | AL-A5 boundary and preserved automated behavior are claimed. |
| Gate real-window launch | `DEFERRED` | No real-window PASS is claimed by this run. |
| `AI-01..AI-08` | `NOT_RUN` | No AI playability or AI-understandability PASS is claimed. |
| Human fun/aesthetics/comfort/input feel | not executed | `NOT_CLAIMED` |

Hidden/validation-only Ogre startup、headless smoke 和干净包启动边界都不是 Computer Use 验收，不能
替代 `docs/current/ai-assisted-gameplay-acceptance-v1.md`。

## 7. Exit and Next Approval Gate

AL-A5 合同的五项退出条件均已满足，合同状态冻结为 `Frozen after AL-A5 verification`。本报告所在
本地 commit 是 AL-A5 的提交身份，可用
`git log -1 -- docs/reports/architecture-lab-a5-simulation-metrics-report-v1.md` 解析。

下一候选仅为 `AL-A6 — Architecture Lab Documentation Pipeline`，状态仍是 `Queued`。AL-A5 完成
不构成 AL-A6、B1、C1、D1 或任何 Extended 能力的批准；开始下一批前必须再次获得项目所有者
独立批准。
