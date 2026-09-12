# 报告与证据索引

本目录保存一次性检查点、Release Candidate、调查结论和开发者视觉记录。报告描述生成时的
事实，不因后续项目定位改变而回写为新的 PASS。

## 里程碑与封板

- [architecture-lab-baseline-v1.md](architecture-lab-baseline-v1.md)
- [architecture-lab-a2-chunk-runtime-report-v1.md](architecture-lab-a2-chunk-runtime-report-v1.md)
- [architecture-lab-a3-world-simulation-report-v1.md](architecture-lab-a3-world-simulation-report-v1.md)
- [architecture-lab-a4-event-command-query-report-v1.md](architecture-lab-a4-event-command-query-report-v1.md)
- [architecture-lab-a5-simulation-metrics-report-v1.md](architecture-lab-a5-simulation-metrics-report-v1.md)
- [architecture-lab-a6-documentation-pipeline-report-v1.md](architecture-lab-a6-documentation-pipeline-report-v1.md)
- [architecture-lab-b1-chunk-residency-report-v1.md](architecture-lab-b1-chunk-residency-report-v1.md)
- [architecture-lab-b2-streaming-demand-report-v1.md](architecture-lab-b2-streaming-demand-report-v1.md)
- [architecture-lab-b3-world-job-scheduler-report-v1.md](architecture-lab-b3-world-job-scheduler-report-v1.md)
- [architecture-lab-b4-world-job-cancellation-report-v1.md](architecture-lab-b4-world-job-cancellation-report-v1.md)
- [architecture-lab-b5-streaming-backpressure-report-v1.md](architecture-lab-b5-streaming-backpressure-report-v1.md)
- [architecture-lab-b6-spatial-activation-report-v1.md](architecture-lab-b6-spatial-activation-report-v1.md)
- [architecture-lab-b10-large-world-stress-report-v1.md](architecture-lab-b10-large-world-stress-report-v1.md)
- [architecture-lab-c1-block-capability-report-v1.md](architecture-lab-c1-block-capability-report-v1.md)
- [architecture-lab-c2-machine-runtime-report-v1.md](architecture-lab-c2-machine-runtime-report-v1.md)
- [architecture-lab-c3-mechanical-topology-report-v1.md](architecture-lab-c3-mechanical-topology-report-v1.md)
- [architecture-lab-d1-simulation-phase-scheduler-report-v1.md](architecture-lab-d1-simulation-phase-scheduler-report-v1.md)
- [alpha-development-checkpoint-v1.md](alpha-development-checkpoint-v1.md)
- [beta-release-candidate-report-2026-08-26.md](beta-release-candidate-report-2026-08-26.md)
- [visual-release-candidate-report-2026-08-28.md](visual-release-candidate-report-2026-08-28.md)
- [playability-release-candidate-report-2026-08-31.md](playability-release-candidate-report-2026-08-31.md)

## 调查与来源

- [minigame-visual-feasibility-2026-09-12.md](minigame-visual-feasibility-2026-09-12.md)：Miniw-Client 实机参照、可行性与观察限制；后续路线见[暖野 v2 方案](../current/visual-upgrade-plan-v2.md)。
- [visual-direction-warm-wilderness-2026-09-10.md](visual-direction-warm-wilderness-2026-09-10.md)：暖野美术方向与概念图。
- [todolist-goal-execution-2026-09-05.md](todolist-goal-execution-2026-09-05.md)
- [simulation-activation-entry-investigation-2026-09-05.md](simulation-activation-entry-investigation-2026-09-05.md)
- [chunk-streaming-regression.md](chunk-streaming-regression.md)
- [art-asset-sources.md](art-asset-sources.md)

## 开发者视觉记录

- [flora-wind-fix-2026-09-12.md](flora-wind-fix-2026-09-12.md)：花草每秒动画重置、根部漂移修复，连续风场与动态视频/GPU/性能证据。
- [water-seam-fix-2026-09-12.md](water-seam-fix-2026-09-12.md)：水波局部坐标导致的区块接缝、GPU 边界回归及岸边/水下实机对照。
- [warm-wilderness-implementation-2026-09-12.md](warm-wilderness-implementation-2026-09-12.md)：暖野 v1 实现、macOS 包、自动检查、性能与正常界面自测。
- [warm-wilderness-m1-implementation-2026-09-12.md](warm-wilderness-m1-implementation-2026-09-12.md)：M1 高清材质、经典树冠恢复、分批提交与尚未完成的验收。
- `developer-visual-record-v10b2.txt`
- `developer-visual-record-v10b3.txt`
- `developer-visual-record-v10c.txt`
- `developer-visual-record-v10d.txt`
- `developer-visual-record-v10e.txt`
- `developer-visual-record-p11-0.txt`

结构化性能原始结果位于 `docs/baselines/`，图片位于 `docs/screenshots/`，图像来源位于
`docs/art-sources/`。

- [TODOLIST Goal completion audit — 2026-09-05](todolist-goal-completion-audit-2026-09-05.md): current proof and unresolved acceptance conditions; not a completion claim.
