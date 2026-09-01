# 现行文档

本目录是日常开发的唯一入口。阅读时按下列优先级处理冲突：当前任务账本高于长期路线，当前
验收规范高于历史协议，实际代码与冻结证据高于过时描述。

## 决策与状态

| 文档 | 用途 |
| ---- | ---- |
| [todolist.md](todolist.md) | 当前状态、唯一已批准下一批、阻塞项和 AI 验收状态。 |
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

`AL-A0` 已完成基线审计与文档冻结；`AL-A1` 已完成 `World` 责任地图和新增公开 API 门禁，
没有迁移旧调用或开始 AL-A2。开始后续 Sprint 前，仍必须在 `todolist.md` 中写明独立批准、
真实游戏需求、合同、可观察 Demo、自动门禁和对应 AI 场景；当前 `AL-A2` 只是 Queued 候选。
