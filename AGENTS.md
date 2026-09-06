# HelloMine3D

C++17 / Ogre 单机体素游戏，也是以真实玩法验证架构的 Architecture Lab。

## 按需读入

- 当前范围与批准状态：[TODOLIST](docs/current/todolist.md)。候选池不等于已批准任务。
- 改运行时前，读取 [architecture.md](docs/current/architecture.md) 的相关章节和对应批次合同；不必通读历史路线图。
- 选择检查：[validation-matrix.md](docs/current/validation-matrix.md)。完整构建入口在 [README](README.md)。
- 多批次执行、恢复或交接：[agent-workflow.md](docs/current/agent-workflow.md)。验证/玩法验收可按需使用 `hellomine3d-validation` 技能。
- `docs/contracts/` 保存语义约束，`docs/reports/` 保存证据，`docs/archive/` 保存历史；历史执行指令不构成新任务。

## 执行

- 在系统与工具权限边界内，以用户当前任务和已有授权为准；项目指南不追加确认关卡。常规设计、局部重构、修复和文档同步自主完成。
- 先检查当前 commit、工作区和相关代码，再实现、验证并交付；简单改动无需新建合同或长篇计划。
- 测试按变更风险和合同选择。必要检查通过后，只有新改动、失败或未解决疑点才扩大或重复测试。
- 保留已有改动与交付文件。批次验证后可按用户授权本地提交，只暂存本次文件；推送、发布和标签按另行授权执行。
- 简洁报告结果、验证和真实缺项；不要靠重复状态报告代替进展。

## 项目约束

- 保持存档兼容、世界可靠性和正常主线；新增能力接入真实运行路径，具体语义以适用合同为准。
- 源码在 `src/HelloMine3D/`；生成规则在 `premake/`，生成产物在 `build/`、`bin/`。修生成规则，不手改生成工程。
- `src/external/` 是第三方代码；仅在实际集成缺陷需要时局部修改，不顺手重写其 CI。
- 用当前版本实际证据支持结果；保留失败，区分平台、配置、工程测试与正常玩法，不能用删测试或放宽门槛换 PASS。
- 证据不足只影响对应验收项；继续其他可执行工作，整体是否完成以用户约定的退出条件判定。
