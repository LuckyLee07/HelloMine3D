# HelloMine3D 验证矩阵

本文把改动类型映射到最低必要验证。H2-H3、Q1-Q3、Stage 9/BETA-RC、Stage 10/VISUAL-RC
和 Stage 11/P11F 的 Windows 自动工程范围已经集中封板。项目现采用
`docs/current/ai-assisted-gameplay-acceptance-v1.md`：自动化证明确定性和工程边界，AI/Computer Use
证明真实窗口中的功能可玩性，AI 视觉检查证明可观察缺陷边界；人类乐趣、审美、舒适度和
物理设备手感为 `NOT_CLAIMED`。

R3 v1、Physical Input v2 和人工产品体验 v1 继续作为历史合同保留，但已 `SUPERSEDED` 为
Architecture Lab 的当前退出门槛。它们的模板保持 `NOT_RUN`，不得由 AI 冒充物理操作者填写
PASS。新的 AI 场景尚未执行时只写 `NOT_RUN`，不写成永久 `Deferred`。

历史运行结果和逐项证据保存在 `docs/archive/project-ledger-2026-08-17.md` 与
`docs/current/runtime-validation.md`。

## 日常开发最低门槛

| 改动 | 最低必要验证 |
| ---- | ------------ |
| 所有 C++ 改动 | 受影响目标能够编译；运行对应定向自动测试。 |
| 世界、区块、实体或持久化 | 定向自动测试 + `HelloMine3DWorldRuntimeSmoke`。 |
| 物品、容器、制作、工具或食物 | 状态守恒、容量边界、失败原子性、固定 tick 和保存/重载测试。 |
| UI 或输入 | 动作仲裁、焦点隔离、映射/冲突、设置迁移自动测试；适用时运行 `AI-01..AI-04`。OS 焦点、Alt+Tab、最小化和窗口关闭只能由 Computer Use 关闭功能范围。 |
| 第一人称动作、命中/受击、粒子或镜头反馈 | 判定时刻与表现解耦测试、数量/持续时间上限、关闭回退、HUD/准星截图和 AI 多帧/视频观察；镜头效果必须可调或可关，人类舒适度不声明。 |
| 目标、配方发现、探索奖励或资源经济 | 主线可达、输入输出守恒、重复奖励/一次性领取、保存重载和全部受影响迁移；脚本化 AI 记录可执行流程，无上下文 AI 盲玩只提供可理解性代理。 |
| 资源、配方、声音或 shader | 资源清单/解析验证；缺失和非法引用必须明确失败。 |
| terrain atlas 或 HUD/手持图标 | `tools\validate_terrain_atlas.ps1`、确定性重建、Alpha/空 tile、block 分面、Material 坐标、资源包与隐藏固定截图。 |
| 顶点格式、网格、光照或 AO | 确定性角落/边界夹具、MeshDirty、隐藏固定截图、既有 schema 3 顶点/索引/构建/驻留字段的补充比较和相关 Q1。仅改每顶点值时不得误报为顶点格式升级。 |
| 雾、天空、云、阴影或后处理 | shader 正反例、关闭回退、固定昼夜截图、窗口缩放/切世界清理、各图形档性能和 AI/开发者视觉检查；动态项必须用多帧、视频或连续窗口。 |
| 构建图或平台代码 | 当前批准平台的工程生成和受影响目标编译；macOS 只有被当批列入范围时才要求新原生证据。 |
| `AL-A0` 纯文档基线 | 逐项对照实际源码冻结模块/API/ownership/tick/snapshot；`git diff --check`、本地 Markdown 引用、World→Ogre 反向依赖检查和 VS2017 完整门禁。运行时代码/身份未变时引用既有正式 Q1/Q3，不重跑 1800 秒；无 OS Computer Use 时 `AI-08=NOT_RUN`。 |
| `AL-A1` World 责任地图 | `tools\validate_world_responsibility_map.ps1` 必须覆盖全部公开方法、匹配 public-surface hash 且无 stale/重复行；随后运行 VS2017 完整门禁。没有运行时行为变化时引用既有正式 Q1/Q3，`AI-01..AI-08` 保持 `NOT_RUN`。 |
| `AL-A2` Chunk Runtime 边界 | `tools\validate_chunk_runtime_boundary.ps1` + AL-A1 公开面门禁；VS2017/v141 Debug/Release 完整门禁；WorldRuntime 的 S0.5/M2/M6/M7/E5/S2.4 与 loader stress 必须通过。禁止新增 Residency 状态、改变 save/unload 转换或修改既有预算；AI 场景未执行时保持 `NOT_RUN`。 |
| `AL-A3` Simulation Runtime 边界 | `tools\validate_world_simulation_boundary.ps1` + AL-A1/A2 边界门禁；`HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A3` 和完整 WorldRuntime；VS2017/v141 Debug/Release 完整门禁。必须保持 8 phase 顺序、20 Hz context、caller-owned pause 和确定性；后续 A5 只能增加观察词汇，仍禁止 Scheduler/Registry 和执行行为变化。 |
| `AL-A4` Event / Command / Query 边界 | `tools\validate_event_command_query_boundary.ps1` + AL-A1/A2/A3 边界门禁；`HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A4` 和完整 WorldRuntime；必须保持 typed command FIFO、immutable fact、订阅者 effect/republish、8 层递归、诊断隔离和查询非 mutation 语义。 |
| `AL-A5` Tick Phase Metrics / Budget 词汇 | `tools\validate_simulation_metrics_boundary.ps1` + AL-A1..A4 边界门禁；`HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A5` 和完整 WorldRuntime；VS2017/v141 Debug/Release 完整门禁。只允许四个真实 metric phase 的 last-tick elapsed/processed/deferred/budget scope/status；不得改变 hard limit、phase 顺序、Gameplay 或引入 Scheduler/Registry/空系统槽。 |
| Track B Core（B1-B6/B10） | 逐批静态 gate 与聚焦 WorldRuntime；B10 额外运行 `HELLOMINE3D_WORLD_SMOKE_FOCUS=B10`、正式 schedule-v3 1800 秒五阶段压力、同 seed 双确定性探针、未放宽 Q1 fast-streaming 和完整 VS2017/v141/隔离包门禁。Lifecycle 资格必须在 8 项 unload budget 前过滤，取消 reservation 不得保留无界 `Absent` 墓碑。 |
| `C1` Block Capability Model | `tools\validate_block_capability_model.ps1` + `HELLOMINE3D_WORLD_SMOKE_FOCUS=C1-CAP` + 完整 WorldRuntime/VS2017 双配置门禁。保留 Chest/Furnace/Crusher 的既有 provider、UI 能力访问和错配/损坏/陈旧句柄失败关闭；C3 只允许 Crusher 增加具体 `MechanicalPort`，仍禁止 Registry 与 Extended 预注册。 |
| `C2` Machine Runtime v0 | `tools\validate_machine_runtime.ps1` + `HELLOMINE3D_WORLD_SMOKE_FOCUS=C2-MACHINE` + Recipe/Resource Pack/terrain atlas + 完整 WorldRuntime/VS2017 双配置门禁。必须保留五态优先级、Furnace 兼容、Crusher 正常 craft/place/Use/crank、槽位/动力/原子完成、break spill、malformed/stale/mismatch、unload/reload、save/reopen 和 economy v2；C3 拓扑不得改变独立手摇或引入动力传播。 |
| `C3` Mechanical Topology Model v0 | `tools\validate_mechanical_topology.ps1` + `HELLOMINE3D_WORLD_SMOKE_FOCUS=C3-TOPOLOGY` + 完整 WorldRuntime/VS2017 双配置门禁。必须覆盖 Crusher-only 六面端口、确定性 component id/canonical edge、merge/split/no-op、正常 place/break、malformed/stale、Chunk unload/reload、save/reopen 派生重建、正常 UI 与 Debug 观察；禁止持久化 topology、C4 power、通用网络、物流和 C4+。 |

## 完整验证路由

| 验证 | 命令或目标 | 适用改动 |
| ---- | ---------- | -------- |
| Windows 工程生成 | `tools\premake\premake5.exe --os=windows --file=premake/premake.lua vs2017` | 构建系统或文件布局；当前正式工具链是 VS2017/v141 |
| Windows 全量门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_build.ps1 -VisualStudioVersion 2017` | 里程碑封板、跨目标源码或链接变化；无活动桌面时显式加 `-SkipRealWindow`，相关真实窗口结果记 `NOT_RUN` |
| World 公开 API 责任门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_world_responsibility_map.ps1` | `World.h` 公开声明或 `docs/current/architecture.md` 责任地图变化；完整 Windows 门禁也会自动运行 |
| Chunk Runtime 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_chunk_runtime_boundary.ps1` | `World` / `ChunkRuntime` / `ChunkManager` 的队列、worker、预算、mesh commit 或 unload 协调变化；完整 Windows 门禁也会自动运行 |
| Simulation Runtime 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_world_simulation_boundary.ps1` | `World::tick`、`WorldSimulation`、phase/context/raw timing、暂停入口或相关 debug snapshot 变化；完整 Windows 门禁也会自动运行 |
| Event / Command / Query 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_event_command_query_boundary.ps1` | command FIFO、EventBus、生产订阅者、查询或未来 Machine/Network 依赖变化；完整 Windows 门禁也会自动运行 |
| Simulation Metrics 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_simulation_metrics_boundary.ps1` | phase metric identity、processed/deferred/budget scope/status、Actor 计数、开发者 Simulation 面板或相关 snapshot 变化；完整 Windows 门禁也会自动运行 |
| Block Capability 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_block_capability_model.ps1` | BlockDefinition capability 声明、Chest/Furnace 访问适配、容器 UI 分派或未来 C2/C3/Extended 边界；完整 Windows 门禁也会自动运行 |
| Machine Runtime 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_machine_runtime.ps1` | MachineRuntime、Furnace/Crusher adapter、processor capability、Crusher payload/recipe/crank、资源经济 schema 或 C3/网络越界；完整 Windows 门禁也会自动运行 |
| Mechanical Topology 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_mechanical_topology.ps1` | C3 Crusher 节点/端口、确定性连通分量、World/Chunk 同步、能力/UI 观察、save 非持久化或 C4/通用网络越界；完整 Windows 门禁也会自动运行 |
| Windows Debug 编译 | `MSBuild build\HelloMine3D.sln /p:Configuration=Debug /p:Platform=x64` | 所有 C++ 改动的主干检查 |
| Windows Release 编译 | 同上，配置改为 `Release` | 里程碑和发行候选 |
| macOS Xcode 门禁 | `bash scripts/verify_xcode.sh` | Xcode 图、macOS 平台或原生封板 |
| 十三个 headless 目标 | `scripts\verify_build.ps1` 中列出的测试/Smoke/Soak | 全量里程碑回归 |
| 世界运行冒烟 | `bin\HelloMine3DWorldRuntimeSmoke.exe` | 区块、存档、交互、事件、实体、地形 |
| 渲染截图 | `tools\run_render_capture.ps1` | renderer、shader、texture、mesh、HUD |
| 性能采集 | `tools\run_perf_baseline.ps1`；正式六场景使用 `tools\capture_release_candidate_performance.ps1` | 区块加载、网格、更新、渲染提交 |
| 性能比较 | `tools\compare_perf_baselines.ps1` | 可能改变帧时间或世界驻留的改动 |
| 资产检查 | `bash scripts/check_assets.sh` | 资产和数据 |
| V10B2 图集合同 | `tools\validate_terrain_atlas.ps1` | 37 个语义/双语名、Alpha 边界、分面、HUD/手持一致性与确定性输出 |
| R3 自动预检 | `tools\validate_r3_automated_preflight.ps1 -Configuration Release -Build` | 控制器、交互、容器、战斗、D6 和后台窗口焦点的逻辑回归；只是 AI 交互前置条件。 |
| AI/Computer Use 功能验收 | `docs\current\ai-assisted-gameplay-acceptance-v1.md` 的 `AI-01..AI-08` | 从带哈希的干净 Release 包用正常 OS 输入执行；禁止 fixture、注入、传送、存档编辑和直接 Gameplay API。严格 `AI-06` 还要求仓库不可访问的 package-only 新任务。当前首份记录为 `NOT_RUN`。 |
| AI 视觉/可读性验收 | 同规范的 `AI-07`，配合原尺寸截图、多帧/视频、连续窗口观察和可访问音频证据 | 可关闭截断、重叠、缺字、破面、闪烁、状态/轮廓可见性和 cue/字幕生命周期；正式证据来自带哈希干净包。`run_render_capture.ps1 -CaptureMs ...` 已支持多帧，但在直接证明发行包可执行文件身份前只作开发预检；不声明人类审美、听感或舒适度。 |
| R3 v1 / Physical Input v2（历史） | `docs\archive\manual-input-acceptance-v1.md`、`docs\archive\physical-input-acceptance-v2.md` 及原校验器 | 历史物理合同 `SUPERSEDED` 为当前门槛，模板保持 `NOT_RUN`；未来自愿运行也必须遵守原物理语义。 |
| 开发者视觉检查（历史/补充） | `docs\archive\manual-product-experience-acceptance-v1.md` A 节与既有 PASS 记录 | 已完成记录继续有效；后续可作为 `DEVELOPER_SELF_TEST` 补充 AI 视觉证据。 |
| 长时间 soak | `tools\run_world_soak.ps1`；正式双 profile 使用 `tools\run_release_candidate_soak.ps1` | 区块/实体生命周期、存档、后台加载 |
| B10 Track B Core 长稳 | `tools\run_large_world_stress_acceptance.ps1 -Formal` + `tools\validate_large_world_stress_acceptance.ps1 -Evidence` | B1-B6 的 demand/job/cancellation/backpressure/spatial-interest 组合变化；精确 36,000 fixed ticks、LW1-LW5、进程/世界界限与确定性摘要 |
| 资源包 | `tools\validate_resource_packs.ps1` | manifest、资源解析、启动预检 |
| 干净发行包 | `tools\package_windows_release.ps1` | 发行包、manifest、资源解析 |
| 世界目录 | `HelloMine3DWorldCatalogueSmoke` | 世界发现、名称、id、目录 |
| 事务保存/恢复 | `HelloMine3DStorageTransactionSmoke`、`HelloMine3DWorldBackupSmoke` | 保存发布、隔离、备份、恢复、格式迁移 |
| 崩溃产物 | `HelloMine3DCrashDiagnosticsSmoke`、`tools\validate_crash_diagnostics.ps1` | 异常处理、dump、sidecar、符号、崩溃 UX |
| 数据竞争 | `bash scripts/verify_tsan.sh` | 后台加载、区块图同步、工作线程调度 |
| 启动/保存/恢复预算 | Q1 schema + Q2 阶段计时 | 目录、启动、进世界、保存、备份、恢复 |
| 内容规模 | `docs/contracts/q3-scale-soak-contract-v1.md` + 正式双 profile soak | 快速移动、人口规模、持久化压力 |
| 完整玩法切片 | G6 自动 fixture + 干净包 `AI-08` | 自动 fixture 证明状态边界，Computer Use 证明正常界面和输入可走通 |

## 验收节奏

| 时机 | 要求 |
| ---- | ---- |
| 功能开发中 | 编译、定向自动测试、数据守恒、存档兼容和必要交互冒烟。 |
| 一个玩法任务完成 | Debug/Release 相关目标、世界冒烟和该任务的失败边界。 |
| G6 集成前 | 清理所有主干构建失败，补齐跨系统回归。 |
| 发布/展示候选 | 当前批准平台门禁、适用 AI 场景、H1-H3、Q1-Q3、长稳、视觉证据和干净包。 |
| Stage 9 单批完成 | 批次合同、定向测试、全部受影响旧版迁移、相关 Q1、必要的 60-120 秒 nominal/stress 和隐藏客户端。 |
| BETA-RC | 历史工程封板保持原证据；当前分类由 AI 验收规范承接，不补写物理/真人 PASS。 |
| Stage 10 单批完成 | 批次合同、VS2017/v141 双配置受影响目标、shader/资源负例、固定 before/after RuntimeReadback、开发者视觉检查和相关 Q1；图形功能必须可关闭。修改 shader/顶点接口时当批补 macOS Release 真实窗口冒烟，否则跨平台状态保持 `Verify`。 |
| Stage 11 单批工程完成 | 历史工程证据保持冻结；适用 AI 场景未执行时写 `Engineering Done / AI NOT_RUN`，主观范围写 `NOT_CLAIMED`。 |
| Architecture Lab Sprint | 受影响自动门禁、数据/迁移、性能、可解释 Debug、游戏内 Demo 和适用 AI 场景定义。 |
| Architecture Lab Track | 完整自动门禁、干净包、新系统压力证据和 `AI-08`；既有主菜单→胜利→保存重开流程必须可完成。 |

## 第 8 阶段批次门禁

| 批次 | 合入前最低证据 | 可后置证据 |
| ---- | -------------- | ---------- |
| `BLD-1` | 重新生成 VS 工程；WorldRuntimeSmoke/Soak 项目同时包含崩溃源和 `dbghelp.lib`；Windows Debug/Release 全量编译及定向目标通过。 | 已于 2026-08-17 通过后台 Release 受控 dump 和干净包。 |
| `K4` | WorldCatalogue、StorageTransaction、WorldBackup 定向测试；创建/重命名/删除恢复的路径与故障边界；一次菜单交互冒烟。 | `AI-01` 正常主菜单与恢复操作。 |
| `G2` | RecipeSmoke；库存/制作状态守恒；预览无副作用；提交失败原子性；关闭、满背包、连点和重载。 | `AI-02/AI-05` UI 与完整流程。 |
| `G3` | 工具表解析；破坏进度状态机；等级/掉落/耐久边界；物品实例存档迁移；固定动作性能采样。 | Q1/Q3 和长稳已完成；功能差异进入 `AI-05`，人类手感不声明。 |
| `G4` | 设置解析/版本/原子写入；应用/取消；暂停模拟；UI 焦点隔离；开发者交互冒烟。 | `AI-01` 窗口、焦点、暂停和设置重启。 |
| `G5` | 音频资产检查；事件去重；音量/pause/mute；dummy backend；无设备和缺失资源降级。 | `AI-07` cue/字幕/生命周期；人类听感 `NOT_CLAIMED`。 |
| `G6` | 干净启动垂直切片 fixture、跨系统保存/恢复、Debug/Release 相关目标、截图和可比较性能采样。 | 干净包 `AI-08` 正常主线。 |
| Alpha 检查点 | 跟踪 v3 迁移 fixture；Debug/Release 世界与崩溃 smoke；双模式隐藏客户端；Alpha 基线/复测；匹配及错误 PDB。 | 后续工程证据已在 RC 关闭；真实窗口功能由 AI 场景承接。 |
| `N3` | 严格食物解析；面包配方；成功/满血/暂停/死亡/冷却/UI 占用的原子语义；生命/冷却保存重载；v5→v6 迁移；目标事件；资源包与启动缺失检查。 | `AI-03/AI-04/AI-08` 证明战斗、恢复和正常旅程；是否引入饥饿仍需独立立项。 |
| 封板 | H1-H3、Q1-Q3、当前批准平台构建、存档故障、资源包、视觉证据、soak、干净包和适用 AI 场景全部通过。 | 人类主观与物理体验保持 `NOT_CLAIMED`，不作为隐藏待办。 |

## Stage 9 / Beta 预排门禁

本节只定义未来批次启动后的最低证据，不表示这些工作已经完成或已经加入当前 77 项总账。

| 批次 | 合入前最低证据 | 阶段结束补充证据 |
| ---- | -------------- | ---------------- |
| `RC0` | `Superseded`：bundle、文档一致性和自动工程基线已由 BETA-RC 覆盖，不再作为活跃批次。 | 历史物理输入合同不再关闭当前功能范围；D2/D4/D6 由 AI 场景承接。 |
| `N7A` | 独立结局状态/奖励 epoch；不以目标耗尽推导胜利；预计 v8→v9 及全部旧版迁移；状态非法值、事务失败、备份恢复；中英文 key/fallback。 | 胜利前中后保存重开，世界列表只从持久状态标记；重复重入不重复发奖。 |
| `N7B` | 激活/守卫状态机；复用现有敌人；actor/事件/驻留上限；死亡、暂停、卸载和遭遇中恢复；相关 Q1 和短时 soak。 | 干净新世界无 debug 注入完成胜利并继续 Playing；进入 `AI-08`。 |
| `N8A` | 前摇/命中/击退/格挡；显式状态转换、目标失效、固定 tick 射线/寻路预算和调试快照。 | 多敌人压力、AI 战斗可读性、内容规模 Q1 和阶段 Q3；人类战斗感不声明。 |
| `N8B` | 战斗档案严格解析；投射物遮挡、寿命/距离/上限，死亡/卸载/保存重载清理；世界冒烟和短时 soak。 | 远程/防御差异清晰，无幽灵投射物、容量突破或重复命中；最终 Q3。 |
| `N9A` | 结构类型/cell/seed/terrain 版本/加载顺序确定性；footprint/间距/覆盖；禁止 `std::rand` 和同步邻区块加载。 | 结构计划快照、跨区块投影、快速流送 Q1 和旧 terrain v1/v2 身份。 |
| `N9B` | 初始战利品快照；箱子库存持久后不再初始化；玩家修改不被覆盖；资源/actor/结构上限。 | 两类地点综合旅程和正式 Q3；若启用 terrain v3，补全新旧世界矩阵。 |
| `N10` | 食物/精确配方/三槽冶炼严格解析；库存守恒；容量/连点/暂停/卸载/保存；主线可达、来源/消耗点和净增益循环检测。 | 综合经济旅程、平衡数据、库存/保存规模比较；饥饿仍需独立评估。 |
| `N11A` | 三档版本化参数；旧世界 Normal；世界保存/metadata 迁移；暂停菜单事务修改并在下个固定 tick 生效；同 seed/难度确定性。 | 三档核心旅程、内容规模/短时 soak、世界列表/备份恢复和 AI 交互确认。 |
| `N11B` | 有界事件状态和奖励去重；胜利前后/重载/备份恢复；不破坏主线结局。 | P2 伸缩项；纳入首个 Beta 才进入 BETA-RC 必过矩阵，否则明确后置。 |
| `N12A` | `en-US`/`zh-CN` key 对齐；fallback、非法 UTF-8、字体、长文本、极端缩放、字幕、Credits 和许可证。自动开发门禁已于 2026-08-26 通过。 | 两种语言核心界面进入 `AI-07`；人类阅读偏好 `NOT_CLAIMED`。 |
| `N12B` | v3 采样定义、9 个 WAV、61 项 manifest、许可证/Credits、缓存/并发上限、缺文件/损坏/设备降级、停止与退出清理均已于 2026-08-26 通过；VS2017/v141 双配置世界 681/681、资源包 34/34，隐藏真实三帧后端无降级。 | cue/字幕/生命周期进入 `AI-07`；人类听感 `NOT_CLAIMED`。 |
| `N12C` | v1 单通道流式定义、20 秒原创 WAV、settings v4、64 项 manifest、许可证/Credits、严格 RIFF/路径/时长、延迟/淡入淡出/低密度间隔、暂停/挂起/静音/音量归零、缺资源/设备、线程和退出清理已于 2026-08-26 通过；VS2017/v141 双配置世界 699/699、资源包 38/38，隐藏真实后端无降级，同身份 Q1 比较通过。 | 暂停/切世界/退出过渡功能进入 `AI-07`；人类音乐偏好 `NOT_CLAIMED`。 |
| `BETA-RC` | 2026-08-26 工程通过：Windows 适用全门禁、v1→v11 迁移、结局/奖励、战斗/投射物、结构/战利品、经济、语言/音频清理、六类 Q1、Q3 双档各 1800 秒、崩溃/符号/84 文件干净包均通过。 | 历史 R3/产品体验状态保持在原报告；当前功能与可观察范围由 AI 规范承接。 |

## Stage 9 回归触发器

| 改动类型 | 必须触发 |
| -------- | -------- |
| 世界、玩家、目标、容器或胜利状态 | 版本/非法值/全部旧版迁移，结局权威，奖励 epoch，遭遇中恢复，保存退出重开，事务失败，备份恢复，幂等和守恒。 |
| 地形、生态、结构或战利品 | terrain 身份，结构类型/cell/seed 与加载顺序确定性，禁止同步邻区块加载，跨区块投影，初始战利品快照，快速流送 Q1，结构规模和 Q3。 |
| actor、AI、攻击或投射物 | 战斗档案解析，固定 tick，状态/目标失效，actor/投射物/事件/射线/寻路容量，遮挡，死亡/卸载/重载清理，掉落去重，内容规模 Q1/Q3。 |
| 食物、配方、冶炼或资源 | 严格解析，输入输出守恒，容量/连点/暂停/卸载，主线可达，来源/消耗点和净增益循环，库存与保存规模。 |
| UI、输入、键位或设置 | 焦点隔离，配置迁移，键位冲突，隐藏客户端，截图和 R3。 |
| 音效、音乐、字体或本地化 | manifest，资源包，语言 key/fallback，字体/长文本，许可证，采样缓存/并发，句柄/线程生命周期，隐藏客户端和听感。 |
| 构建、崩溃或打包 | H2/H3 受控崩溃、符号、隐私、可执行清单、干净包和新 SHA-256。 |

Windows 自动化 EXE 一律隐藏或后台运行。只有用户明确安排的 AI/Computer Use、开发者自测或
历史物理合同运行才可以启动前台窗口；`ShowWindowNoActivate` 只是 best-effort，不能作为不会
抢焦点的保证。Computer Use 记录必须明确标成 `AI_INTERACTIVE`，不得伪装真人/物理证据，也
不得在用户未安排的时段反复启动、抢占当前焦点。

## Stage 10 视觉门禁

| 批次 | 合入前最低证据 | 阶段结束补充证据 |
| ---- | -------------- | ---------------- |
| `V10A` | 顶点 AO 0-3、侧边/对角规则、天空光/方块光、透明/未知邻域、跨 section 一致性；只有合并结果可重建全部内部采样才 greedy 合并，对角线按误差和固定 tie-break 选择；保持 32 字节 stride；Debug/Release 受影响目标。 | FS2 四图加洞穴/树冠/遗迹/营地 before/after；schema 3 既有 geometry/mesh/residency 字段补充比较、快速流送/规模 Q1、短 Q3 和开发者检查。超过 10% 未获批准不得关闭。 |
| `V10B1` | 图集尺寸/分格/坐标和颜色参数严格解析；旧图集像素兼容；不得静默改变 V10A AO/光照曲线；缺失/越界负例。 | shader 负例、Windows 双配置、开发者检查和 macOS Release 真实窗口冒烟。 |
| `V10B2` | 原创 top/side/bottom 资产、生成脚本、来源/许可、manifest、透明边界和世界/HUD/手持一致性。 | 近景/中景截图、资源包、相关 Q1、干净包和开发者检查。 |
| `V10B3`（Done） | 生态 tint 范围、坐标/seed/加载顺序确定性 tile 变体、greedy merge key 和 terrain v3 不变。 | 261 项图集、732 项 Release 世界、五张固定截图、相关 Q1、短 Q3 和 `developer-visual-record-v10b3.txt` 均通过。 |
| `V10C`（Done；Windows） | 定向雾共享参数、云层高度/厚度/速度/颜色边界、帧率无关移动、进入云层与关闭回退。 | 双配置聚焦 21/21、Release 世界 741/741、资源包 65/65、启动负例 14/14、十图多帧开发者视觉 PASS；相关 Q1 保留已批准的快速流送单样本延迟例外。新 macOS 运行 `NOT_RUN`。 |
| `V10D`（Done；Windows） | settings v5、v0-v4→Off、双语 key、Stage 10 shadow 补充身份；Off/Medium/High、距离/纹理/bias/PCF 上限、能力回退和资源清理。 | 双配置聚焦 21/21、资源包 75/75、Release 世界 742/742、74 项 manifest、14 类启动负例、9 项档位合同和强制回退通过；正午/黄昏六图开发者检查 PASS，最终同场景各档 frame P95/P99 无需例外。新 macOS 运行 `NOT_RUN`。 |
| `V10E`（Done；Windows） | settings v6、v0-v5→Off、双语 key、Stage 10 post 补充身份；Off/On tone curve/确定性抖动/八采样极轻 bloom、HUD 排除、缩放、回退和清理。 | 双配置聚焦 22/22、资源包 80/80、Release 世界 743/743、77 项 manifest、15 类启动负例、11 项性能合同和真实支持/强制回退通过；六图开发者检查 PASS，同档性能无需例外。新 macOS 运行 `NOT_RUN`。 |
| `VISUAL-RC`（Done：Windows 工程） | 适用全门禁、完整截图/开发者检查矩阵、资源/许可/Credits、正式相关 Q1、干净包和 bundle。 | Windows 77 项 manifest、743/743 世界、80/80 资源包、六类 Q1、nominal/stress 各 1800 秒 Q3、97 文件包和 239 项 Xcode 工程图静态检查通过；AI 产品表现进入 `AI-07`，主观体验 `NOT_CLAIMED`。 |

Stage 10 的地形 shader、顶点数据、图集、云、阴影或后处理变化都必须保留一个明确的低画质
回退路径。固定截图证明确定性，AI/开发者视觉检查负责可观察缺陷；动态项必须使用多帧、视频或
连续窗口。人类审美和舒适度 `NOT_CLAIMED`，视觉证据也不替代 AI 交互场景。

## Stage 11 可玩性门禁

Stage 11 的工程证据已经冻结。下表不重跑或改写这些 PASS，只定义 Architecture Lab 当前如何
补充真实窗口证据。全部 AI 场景当前为 `NOT_RUN`。

| 批次 | 已有最低工程证据 | 当前 AI 场景 | 不声明范围 |
| ---- | ---------------- | ----------- | ---------- |
| `P11-0` | 火把/材料末尾追加、配方守恒、Rose 修正、metadata 发射、局部重光照、双配置和正式 Q1。 | `AI-05` 制作/放置/洞穴照明；`AI-07` 夜晚/熔炉多帧。 | 人类审美。 |
| `P11A` | 单一动作仲裁、settings v7 迁移、线性相对增量、焦点门、hold/toggle、定向 88/88。 | `AI-01..AI-04` 移动、世界/UI、暂停、Alt+Tab、最小化、回焦和重启。 | 物理鼠标距离、键鼠舒适度。 |
| `P11B` | 判定/表现解耦、数量/持续时间上限、关闭回退、音频并发和 HUD/准星隔离。 | `AI-05` 动态交互；`AI-07` 多帧/视频辨识。 | 人类打击感、眩晕和长期舒适度。 |
| `P11-1` | 身份末尾追加、门保存、木板/圆石守恒、axe/shovel 矩阵、资源和旧存档回归。 | `AI-05` 采集、工具比较、建可关闭落脚点。 | 人类建造乐趣、主观速度感。 |
| `P11C` | 目标 v3、三项并行机会、分支进度、配方发现、迁移、双语与完整世界回归。 | `AI-06` 在仓库不可访问的 package-only 新任务中完成 30 分钟盲玩；只有新工作目录但仍可读仓库时为 `BLOCKED`。 | 人类首次体验、留存和乐趣。 |
| `P11D` | 奖励 v1/v0、save v12、三类能力、持久化和全部迁移/资源门禁。 | `AI-05` 逐结构取得并实际使用能力，记录后续可执行操作变化。 | 奖励吸引力和人类价值感。 |
| `P11-2` | terrain v4、Mountain/洞口、v1-v3 冻结、保存身份、完整回归与正式 Q1。 | `AI-05` 寻找/通行；`AI-07` 轮廓/洞口可见性。 | 人类风景审美。 |
| `P11E` | 多部件轮廓、关键姿态、死亡表现隔离、身份掉落、Waystone 共鸣和完整门禁。 | `AI-05` 战斗/共鸣；`AI-07` 无名称轮廓与状态可辨识。 | 人类危险感、掉落价值感和战斗乐趣。 |
| `P11F` | VS2017/v141 双配置、832/832 世界、六类 Q1、双档 Q3、dump/符号和 104 项干净包。 | `AI-08` 从主菜单经胜利到保存重开；历史报告不回写。 | 外部玩家签字和未批准的新 macOS 运行。 |

AI 记录至少包含 commit、包哈希、执行器/Computer Use 环境、OS/GPU、窗口/图形/语言、seed、
存档身份、重试/超时/意外弹窗、证据路径和声明边界。截图或日志只能支持窗口内的实际步骤；
物理设备手感和人类可玩性不得由 AI 记录推导。
