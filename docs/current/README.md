# 现行文档

本目录是日常开发的唯一入口。阅读时按下列优先级处理冲突：当前任务账本高于长期路线，当前
验收规范高于历史协议，实际代码与冻结证据高于过时描述。

## 决策与状态

| 文档 | 用途 |
| ---- | ---- |
| [todolist.md](todolist.md) | 当前状态、已批准批次、唯一下一候选、阻塞项和 AI 验收状态。 |
| [architecture-lab-roadmap-v1.md](architecture-lab-roadmap-v1.md) | Architecture Lab 长期能力目录和可玩载体硬约束。 |
| [ai-assisted-gameplay-acceptance-v1.md](ai-assisted-gameplay-acceptance-v1.md) | `AI-01..AI-08`、Computer Use 黑盒规则与声明分类。 |

## 实现与验证

| 文档 | 用途 |
| ---- | ---- |
| [architecture.md](architecture.md) | 当前源码模块、所有权和线程边界。 |
| [architecture-lab-tutorial.md](architecture-lab-tutorial.md) | 已实现 Architecture Lab 批次的问题、失败方案、演进、验证与取舍；不预建空章节。 |
| [validation-matrix.md](validation-matrix.md) | 改动触发的最低构建、测试、性能和交互证据。 |
| [runtime-validation.md](runtime-validation.md) | 已有运行时证据及自动化无法覆盖的边界。 |
| [performance-baseline.md](performance-baseline.md) | 性能场景、身份和比较入口。 |
| [render-regression-smoke.md](render-regression-smoke.md) | 渲染回归采集与判定。 |
| [thread-sanitizer-validation.md](thread-sanitizer-validation.md) | 原生 TSan 门禁。 |
| [windows-release-packaging.md](windows-release-packaging.md) | Windows 隔离根发行包流程。 |
| [iteration-report-template.md](iteration-report-template.md) | 新批次回归报告模板。 |

`AL-A0` 已完成基线审计与文档冻结；`AL-A1` 已完成 `World` 责任地图和新增公开 API 门禁；
`AL-A2` 已把既有 Chunk update/mesh/loader 协调迁入 `ChunkRuntime`，没有引入 Residency 状态机。
`AL-A3` 已把现有 20 Hz fixed-tick 顺序集中到具体 `WorldSimulation`，并只增加最近一次 tick 的
8 phase 原始耗时观察，没有引入 A5 scheduler/budget。
`AL-A4` 已把旧 event-as-command 路径改为 typed command FIFO，并冻结不可变事实事件、订阅者
effect/republish、有界递归、诊断隔离和查询非 mutation 边界。
`AL-A5` 已为四个真实 Simulation phase 冻结 last-tick processed/deferred/budget 观察、scope/status
词汇和开发者面板；`AL-A6` 已完成单一 living tutorial 流水线。Track B 的 B1-B4 已依次完成
Chunk 三套状态、Streaming Demand、typed World Job Scheduler 和 generation cancellation；当前下一批为
已批准的 `B5` Streaming Backpressure。后续批次仍须在 `todolist.md` 中写明真实需求、合同、可观察
证据和范围边界，不能由长期路线自动授权。
