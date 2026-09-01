# HelloMine3D AL-A6 Architecture Lab Documentation Pipeline Report v1

日期：2026-09-01

批次：`AL-A6 — Architecture Lab Documentation Pipeline`

工作流状态：`Done`

本报告记录 living tutorial 的结构收敛、manifest/证据边界和自动验证。它不声明新的 Gameplay、
Q1/Q3 性能、AI 可玩性或人类主观体验结论。

## 1. Implementation Result

`docs/current/architecture-lab-tutorial.md` 保持为唯一现行教程文件。原先按 A2-A5 分配的多个物理
Part 已收敛到 Track A 的 `Part 00`，既有问题、失败方案、设计、实现、验证和取舍内容全部保留。
新增 manifest 将 AL-A0 至 AL-A6 映射到所属 Part、真实 Section 和冻结证据路径。

每个已实现 Section 统一包含七个非空逻辑标题：Problem、Naive Solution、Failure、Design
Evolution、Implementation、Validation、Trade-offs。Data Structure、Runtime Flow、Debug、Benchmark
和 Exercises 只在存在真实材料时嵌入，不创建空模板。Track B/C/D/Integrated Part 均未提前创建。

## 2. Validator and Negative Fixtures

新增 `tools/validate_architecture_lab_documentation.ps1`，检查：

- 只有一份 canonical living tutorial；
- manifest marker、row、batch 和 evidence 唯一且合法；
- manifest 与当前任务账本的完成状态一致；
- evidence 使用仓库内相对路径并真实存在；
- 每个 Part/Section 与七段非空结构存在；
- 不存在没有已实现 batch 的 placeholder Part；
- roadmap 仍保留单文件、Core/Extended 和未实现能力不进正文的规则。

`-SelfTest` 使用四个隔离字符串变体，分别证明 malformed manifest、missing evidence、empty section
和 placeholder Part 必须失败。当前 focused 结果为：

```text
[ARCHITECTURE_LAB_DOCUMENTATION] status=PASS batches=7 parts=1 sections=6 negative_fixtures=4
```

该 validator 已接入 `scripts/verify_build.ps1`，位于 AL-A1 至 AL-A5 架构门禁之后、资源和编译门禁
之前。

## 3. Preserved Boundaries

- 没有修改 `src/`、`media/`、Premake build graph 或任何运行时代码。
- save v12、terrain v4、settings v8、资源、配方、玩法主线和渲染结果均不变。
- 没有创建 B1 Residency、B2 Demand、B3 Jobs、B4 Cancellation、B5 Backpressure、B6 Spatial
  Interest 或 B7-B9 Extended 内容。
- tutorial manifest 只描述已实现 AL-A0 至 AL-A6，不把路线图候选冒充当前事实。

## 4. Full Windows Gate and Artifact Identity

关闭命令：

```powershell
& .\scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
```

| Evidence | Current result |
| -------- | -------------- |
| Focused documentation validator + four negatives | `PASS` |
| VS2017/v141 Debug/Release complete gate | `PASS`：两轮 WorldRuntime `853/853`、resource-pack `80/80`、recipe `122/122`、startup negatives `15/15`、两轮 short soak 零失败 |
| Isolated clean-root package | `PASS`：104 entries，SHA-256 `0E387185600E957F8BC6ED4B46333817B0C12F0725322F9F939337455C245990` |
| Release executable | `PASS`：8,762,880 bytes，SHA-256 `30F441A59FBF55AAEB6964BA8D43B08E008BA869EE4AB7392FD49CAC5D6741D2` |
| Runtime/source identity change | `NO` |

Debug/Release 全量重建分别以 `0 errors` 完成；现有第三方库和 Ogre 警告保留为已知编译噪声，不是
本批新增回归。`scripts/verify_build.ps1` 最终返回
`[BUILD_VERIFY] status=PASS real_window=DEFERRED`。

## 5. Evidence Boundary

| Dimension | Result | Declaration |
| --------- | ------ | ----------- |
| Documentation engineering | `PASS` | A6 structure, manifest and negative-fixture boundary are claimed. |
| Complete automated engineering | `PASS` | The VS2017/v141 Debug/Release, headless runtime and isolated-package gate passed. |
| Gate real-window launch | `DEFERRED` | The approved command uses `-SkipRealWindow`. |
| `AI-01..AI-08` | `NOT_RUN` | No AI playability or understandability PASS is claimed. |
| Human fun/aesthetics/comfort/input feel | not executed | `NOT_CLAIMED` |

## 6. Exit and Next Approval Gate

AL-A6 的实现、focused contract、完整 VS2017/v141 门禁和 104 项隔离包均已完成，工作流状态为
`Done`。B1 已由当前 Goal 独立批准，可在 A6 独立本地提交后开始；A6 不批准 B7-B9、Track C/D
或任何 Extended 能力。
