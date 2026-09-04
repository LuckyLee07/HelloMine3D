# HelloMine3D 文档入口

本目录按“现行决策、冻结合同、证据报告、历史资料”分层。文件总数并未通过删除证据来减少；
日常开发只需要从 `current/` 开始，其余目录按需查阅。

## 日常阅读顺序

1. [当前状态与下一批](current/todolist.md)：唯一的当前任务账本。
2. [Architecture Lab 路线](current/architecture-lab-roadmap-v1.md)：长期能力目录；没有进入当前
   任务账本的 Sprint 不构成开发承诺。
3. [AI 辅助玩法验收](current/ai-assisted-gameplay-acceptance-v1.md)：自动化、Computer Use、
   AI 视觉和 `NOT_CLAIMED` 的权威边界。
4. [验证矩阵](current/validation-matrix.md)：改动类型到构建、测试、性能和交互证据的路由。
5. [当前代码架构](current/architecture.md)、[Architecture Lab 教程](current/architecture-lab-tutorial.md)
   与[运行时证据](current/runtime-validation.md)：需要进入实现、学习或审计时阅读。

当前摘要：Stage 11 Windows 工程范围已经完成；`AI-01..AI-08` 尚未运行，状态为
`NOT_RUN`；人类乐趣、审美、舒适度和物理设备手感为 `NOT_CLAIMED`。`AL-A0` 已完成当前
架构/依赖/性能/验证身份冻结；`AL-A1` 已完成 `World` 责任地图和新增 API 门禁；`AL-A2` 已完成
既有 Chunk runtime 协调提取；`AL-A3` 已完成 Simulation Runtime 编排与原始 phase timing。
`AL-A4` 已完成 Event / Command / Query 边界与事实分发门禁；`AL-A5` 已完成四条真实 phase
metrics、预算词汇和开发者面板观察；`AL-A6` 已完成单一 living tutorial 流水线；`B1` 已冻结
Chunk Data/Mesh/Render 三态，`B2` Streaming Demand、`B3` World Job Scheduler、`B4` Generation
Cancellation、`B5` Streaming Backpressure 与 `B6` Spatial Activation 已通过完整门禁；`B10`
Large World Stress & Acceptance 已通过正式 Core 压力/确定性验收、双配置 `920/920` 组成式门禁
与最终 Q1。
其余 Core/Extended 能力目录都不是自动批准的 backlog。

## 目录分工

| 目录 | 内容 | 是否现行 |
| ---- | ---- | -------- |
| [`current/`](current/) | 当前状态、Architecture Lab、验收规范、架构和验证操作。 | 是 |
| [`contracts/`](contracts/) | 每个已实现批次冻结的数据、失败、迁移和退出合同。 | 合同语义有效；通常不主动改写 |
| [`reports/`](reports/) | 检查点、RC、视觉记录、性能/缺陷调查和素材来源证据。 | 事实有效；不回写历史结论 |
| [`archive/`](archive/) | 已结束路线、旧总账、参考研究和被取代的真人协议。 | 否；不得作为当前待办 |
| `baselines/` | 版本化性能基线和正式比较结果。 | 证据 |
| `screenshots/` | 固定身份渲染与玩法截图。 | 证据 |
| `art-sources/` | 图像生成和原创素材来源。 | 证据 |

各层的完整索引见对应目录的 `README.md`。

## 权威性规则

- 当前任务、状态和优先级只写入 `current/todolist.md`。
- 长期路线只描述候选能力；只有被当前任务账本批准的下一批才能进入开发。
- 新功能在编码前新增或更新 `contracts/` 中的一份合同，完成后冻结实际证据。
- 一次性封板、人工/AI 记录和调查结论进入 `reports/`，不得把后来的口径回写成当时的 PASS。
- 被取代的路线和验收协议移入 `archive/`，并保留历史身份与引用。
- 文档中的仓库路径统一从仓库根开始写成 `docs/...`，避免移动后产生相对路径歧义。

## 新文档放置规则

| 新文档类型 | 目标位置 |
| ---------- | -------- |
| 当前状态或长期方向 | `docs/current/`；优先更新已有文件 |
| 批次合同 | `docs/contracts/<batch>-contract-vN.md` |
| 封板、检查或调查报告 | `docs/reports/` |
| 已结束路线或被取代协议 | `docs/archive/` |
| 性能/视觉/素材原始证据 | 现有 `baselines/`、`screenshots/`、`art-sources/` 或 `reports/` |

不要为同一状态再新增一份平行路线图；先更新现行文档，只有需要冻结不可变历史时才新建报告。
