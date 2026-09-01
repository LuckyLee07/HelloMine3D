# HelloMine3D AL-A3 World Simulation Boundary Report v1

日期：2026-09-01

批次：`AL-A3 — Simulation Runtime`

工作流状态：`Done`

本报告记录既有 fixed-tick 编排的责任提取及其自动验证。它不声明新的 Gameplay、Q1/Q3 性能、
AI 可玩性或人类主观体验结论。

## 1. Implementation Result

`World::tick(int)` 继续作为 78 项公开面中的唯一 20 Hz Gameplay 入口，但原有调用序列已迁入
World 按值拥有的具体 `WorldSimulation`。Runtime 持有 non-owning `World&`，只负责编排现有实现；
World、ActorManager、PlayerActor、ChunkManager 与 Gameplay runtime 仍拥有各自权威状态。

冻结的 phase 顺序为：

```text
TickPreparation
  -> ActorSimulation
  -> Combat
  -> Encounter
  -> BlockRandomTick
  -> Population
  -> BlockEntitySimulation
  -> GameplayRuntime
```

`WorldTickContext` 只携带调用者 tick 和既有 `1/20` 秒 delta。每次成功 tick 发布一个复制型
`WorldSimulationSnapshot`：完成计数、tick、delta、整 tick 原始毫秒数和 8 项 phase 原始毫秒数。
`WorldDebugStats` 与开发者面板可观察这些值；它们不持久化，也不进入确定性状态比较。

## 2. Preserved Behavior and Explicit Limits

- Phase 内部与 phase 之间的旧调用顺序保持不变。
- `GameApplicationFlow::acceptsWorldSimulation()` 仍在 Ogre 调用 `SandboxRuntime::update` 前拦截暂停；
  `WorldSimulation` 没有第二份 pause state。
- fixed tick 频率、catch-up cap、random tick、population、combat 与 mesh budget 均未改变。
- save v12、terrain identity、资源、配方、输入、渲染和公开 World facade 均未改变。
- 本批没有迁移玩法状态，也没有增加 `SimulationScheduler`、`ISandboxSystem`、Registry、
  `SimulationBudget`、priority 或 deferred-work slot。

最近一次 tick 的 `steady_clock` 样本只是原始诊断。平均值、percentile、预算、overrun 与统一指标
口径仍属于需独立批准的 A5；AL-A3 不以一份样本做性能决策。

## 3. Static Boundary Evidence

| Gate | Result |
| ---- | ------ |
| `tools\validate_world_responsibility_map.ps1` | `PASS`；78 methods、45 queries、31 commands、2 runtime ticks；public-surface SHA-256 `3C53F56C425F0395354C8A5CE966E96CDA8BC93D836699955E03BA965A664AD8`。 |
| `tools\validate_chunk_runtime_boundary.ps1` | `PASS`；AL-A2 的 queue、worker 与 mesh protocol 边界未回退。 |
| `tools\validate_world_simulation_boundary.ps1` | `PASS`；8 phases、单一 tick delegation、last-tick raw timing、caller-owned pause，且没有 A5 抽象。 |
| `git diff --check` | `PASS`；仅报告仓库 checkout 的 LF/CRLF 提示，无 whitespace error。 |

三个架构 validator 都已接入 `scripts\verify_build.ps1`，后续完整 Windows 门禁会同时阻止 World
公开面、Chunk runtime 与 Simulation runtime 边界回退。

## 4. Behavioral Regression Evidence

定向 `HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A3` 通过 `16/16`，其中 A3 新增 6 项断言：

| Assertion | Result and meaning |
| --------- | ------------------ |
| `initial-snapshot-has-no-completed-tick` | `PASS`；初始观察不会伪装成已完成 tick。 |
| `phase-identity-and-order-are-frozen` | `PASS`；8 项 phase identity 与顺序固定。 |
| `tick-context-is-propagated` | `PASS`；调用者 tick、20 Hz delta 与 world time 一致。 |
| `raw-last-tick-timings-are-observable` | `PASS`；总计与各 phase 都是 finite、non-negative 原始值。 |
| `caller-owned-pause-gate-freezes-simulation` | `PASS`；暂停期间 completed tick 不前进。 |
| `resume-advances-exactly-one-tick` | `PASS`；恢复后只执行调用者提交的一次 tick。 |

Debug 与 Release 的真实 `HelloMine3DWorldRuntimeSmoke` 各通过 `838/838`。既有 random-tick、Actor、
combat、population、furnace、progression、determinism、save/reload 与完整 G6 可玩切片均保持绿色；
两种配置的 short soak 也都以 `failures=0` 结束。

## 5. Full Windows Gate and Artifact Identity

执行命令：

```powershell
& .\scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
```

| Evidence | Result |
| -------- | ------ |
| Toolchain | Visual Studio 2017 / v141 / x64 Debug + Release |
| Full solution builds | `PASS`；Debug/Release 均 0 errors；既有 Ogre/第三方兼容警告不改变门禁结果。 |
| Headless suite | `PASS`；所有 gate targets 在 Debug/Release 通过。 |
| World / resource / recipe | `838/838` twice；`80/80`；`122/122` |
| Startup negative diagnostics | `15/15` |
| Resource identity | 84 manifest entries；278 atlas checks；38 performance fixtures；11 Stage 10 supplement checks |
| Crash/package boundary | `PASS`；validation-only crash diagnostics 和 isolated clean-root package 通过。 |
| Release executable | 8,757,248 bytes；SHA-256 `44C74920486AB7AE83CC30D2CFAC37E9565FB902D2FC95956D5D6A932BFB570F` |
| Verification package | 104 entries；SHA-256 `A948E9021E20FD6F0FB18736E847417A2E9E4F9B8F2CDAD85D19A5CEE1F1D341` |

第一次完整门禁在编译前的 `q1-cold-start-v1/invalid` 性能比较夹具遇到一次 PowerShell
`OutOfMemoryException`，原失败没有删除或改写。随后单独复跑 38/38 性能比较夹具通过，第二次从头
执行上述完整门禁全部通过，因此该事件分类为本机进程内存压力的瞬态重试，而不是 A3 行为失败。

该 ZIP 是本地 AL-A3 验证产物，不替换历史 PLAYABILITY-RC 发行身份，不创建 tag，也不 push。
本批没有重跑正式硬件 Q1/Q3，也没有申请性能例外；新增 raw timer 的正式性能结论留给未来获批批次。

## 6. Evidence Boundary

为避免抢占项目所有者当前窗口，本次完整门禁显式使用 `-SkipRealWindow`：

| Dimension | Result | Declaration |
| --------- | ------ | ----------- |
| Automated engineering | `PASS` | AL-A3 orchestration boundary and preserved automated behavior are claimed. |
| Gate real-window launch | `DEFERRED` | No real-window PASS is claimed by this run. |
| `AI-01..AI-08` | `NOT_RUN` | No AI playability or AI-understandability PASS is claimed. |
| Human fun/aesthetics/comfort/input feel | not executed | `NOT_CLAIMED` |

Hidden/validation-only Ogre startup、headless smoke 和干净包启动边界都不是 Computer Use 验收，不能
替代 `docs/current/ai-assisted-gameplay-acceptance-v1.md`。

## 7. Exit and Next Approval Gate

AL-A3 合同的五项退出条件均已满足，合同状态冻结为 `Frozen after AL-A3 verification`。本报告所在
本地 commit 是 AL-A3 的提交身份，可用
`git log -1 -- docs/reports/architecture-lab-a3-world-simulation-report-v1.md` 解析。

下一候选仅为 `AL-A4 — Event / Command / Query Boundary`，状态仍是 `Queued`。AL-A3 完成不构成
AL-A4、AL-A5、B1、D1 或任何 Extended 能力的批准；开始下一批前必须再次获得项目所有者独立批准。
