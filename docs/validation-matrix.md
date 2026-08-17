# HelloMine3D 验证矩阵

本文把改动类型映射到最低必要验证。当前采用“玩法闭环优先”：开发期间执行低成本、
高反馈的自动验证；真实硬件记录、正式性能预算、长稳运行和干净发行包可以在 G6 前后
集中完成。

历史运行结果和逐项证据保存在 `docs/project-ledger-2026-08-17.md` 与
`docs/runtime-validation.md`。

## 日常开发最低门槛

| 改动 | 最低必要验证 |
| ---- | ------------ |
| 所有 C++ 改动 | 受影响目标能够编译；运行对应定向自动测试。 |
| 世界、区块、实体或持久化 | 定向自动测试 + `HelloMine3DWorldRuntimeSmoke`。 |
| 物品、容器、制作或工具 | 状态守恒、容量边界、失败原子性和保存/重载测试。 |
| UI 或输入 | 自动焦点隔离测试；开发者进行一次交互冒烟。正式 R3 记录可后移。 |
| 资源、配方、声音或 shader | 资源清单/解析验证；缺失和非法引用必须明确失败。 |
| 构建图或平台代码 | 对应平台工程生成和受影响目标编译。 |

## 完整验证路由

| 验证 | 命令或目标 | 适用改动 |
| ---- | ---------- | -------- |
| Windows 工程生成 | `vs2022.bat` | 构建系统或文件布局 |
| Windows 全量门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_build.ps1` | 里程碑封板、跨目标源码或链接变化 |
| Windows Debug 编译 | `MSBuild build\HelloMine3D.sln /p:Configuration=Debug /p:Platform=x64` | 所有 C++ 改动的主干检查 |
| Windows Release 编译 | 同上，配置改为 `Release` | 里程碑和发行候选 |
| macOS Xcode 门禁 | `bash scripts/verify_xcode.sh` | Xcode 图、macOS 平台或原生封板 |
| 十三个 headless 目标 | `scripts\verify_build.ps1` 中列出的测试/Smoke/Soak | 全量里程碑回归 |
| 世界运行冒烟 | `bin\HelloMine3DWorldRuntimeSmoke.exe` | 区块、存档、交互、事件、实体、地形 |
| 渲染截图 | `tools\run_render_capture.ps1` | renderer、shader、texture、mesh、HUD |
| 性能采集 | `tools\run_perf_baseline.ps1` | 区块加载、网格、更新、渲染提交 |
| 性能比较 | `tools\compare_perf_baselines.ps1` | 可能改变帧时间或世界驻留的改动 |
| 资产检查 | `bash scripts/check_assets.sh` | 资产和数据 |
| 真人输入 | `docs\manual-input-acceptance-v1.md` 和 `tools\validate_manual_input_record.ps1 -RequirePass` | 正式输入、窗口焦点、容器、战斗封板 |
| 长时间 soak | `tools\run_world_soak.ps1` | 区块/实体生命周期、存档、后台加载 |
| 资源包 | `tools\validate_resource_packs.ps1` | manifest、资源解析、启动预检 |
| 干净发行包 | `tools\package_windows_release.ps1` | 发行包、manifest、资源解析 |
| 世界目录 | `HelloMine3DWorldCatalogueSmoke` | 世界发现、名称、id、目录 |
| 事务保存/恢复 | `HelloMine3DStorageTransactionSmoke`、`HelloMine3DWorldBackupSmoke` | 保存发布、隔离、备份、恢复、格式迁移 |
| 崩溃产物 | `HelloMine3DCrashDiagnosticsSmoke`、`tools\validate_crash_diagnostics.ps1` | 异常处理、dump、sidecar、符号、崩溃 UX |
| 数据竞争 | `bash scripts/verify_tsan.sh` | 后台加载、区块图同步、工作线程调度 |
| 启动/保存/恢复预算 | Q1 schema + Q2 阶段计时 | 目录、启动、进世界、保存、备份、恢复 |
| 内容规模 | Q3 + 正式 R2 soak | 快速移动、人口规模、持久化压力 |
| 完整玩法切片 | G6 fixture + 截图 + 真人输入 | 第 8 阶段最终封板 |

## 验收节奏

| 时机 | 要求 |
| ---- | ---- |
| 功能开发中 | 编译、定向自动测试、数据守恒、存档兼容和必要交互冒烟。 |
| 一个玩法任务完成 | Debug/Release 相关目标、世界冒烟和该任务的失败边界。 |
| G6 集成前 | 清理所有主干构建失败，补齐跨系统回归。 |
| 发布候选 | 全平台门禁、R3、H1-H3、Q1-Q3、长稳运行、截图和干净包。 |

## 第 8 阶段批次门禁

| 批次 | 合入前最低证据 | 可后置证据 |
| ---- | -------------- | ---------- |
| `BLD-1` | 重新生成 VS 工程；WorldRuntimeSmoke/Soak 项目同时包含崩溃源和 `dbghelp.lib`；Windows Debug 编译及定向目标通过。 | Release 受控 dump 和干净包。 |
| `K4` | WorldCatalogue、StorageTransaction、WorldBackup 定向测试；创建/重命名/删除恢复的路径与故障边界；一次菜单交互冒烟。 | 正式真人输入记录和发行包恢复演练。 |
| `G2` | RecipeSmoke；库存/制作状态守恒；预览无副作用；提交失败原子性；关闭、满背包、连点和重载。 | UI 截图和完整流程真人记录。 |
| `G3` | 工具表解析；破坏进度状态机；等级/掉落/耐久边界；物品实例存档迁移；固定动作性能采样。 | 正式 Q1/Q3 预算和长稳。 |
| `G4` | 设置解析/版本/原子写入；应用/取消；暂停模拟；UI 焦点隔离；开发者交互冒烟。 | R3 正式窗口和输入记录。 |
| `G5` | 音频资产检查；事件去重；音量/pause/mute；dummy backend；无设备和缺失资源降级。 | 目标设备主观听感、音乐混音和完整发行资产验收。 |
| `G6` | 干净启动垂直切片 fixture、跨系统保存/恢复、Debug/Release 相关目标、截图和可比较性能采样。 | 发布候选的全平台、真人输入、正式预算、soak 和干净包证据。 |
| 封板 | R3、H1-H3、Q1-Q3、全平台构建、存档故障、资源包、截图、soak 和干净包全部通过。 | 无；失败只允许修复或明确重新立项。 |
