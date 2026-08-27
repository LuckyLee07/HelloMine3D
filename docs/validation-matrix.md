# HelloMine3D 验证矩阵

本文把改动类型映射到最低必要验证。当前 Release Candidate 的 H2-H3、Q1-Q3 和干净包
路径及 Stage 9 / BETA-RC 工程封板已集中完成，真实输入与产品体验仍需目标机器上的操作者记录；
现有 R3 v1 只保留为十二项物理输入基线，不能单独关闭当前 D4/D6 或视觉/双语/听感验收。
Stage 10 的 V10A/V10B1 已完成 Windows 工程范围；V10B2 工程与隐藏画面证据已完成，当前等待
项目所有者自行启动前台 Release 关闭视觉 `Verify`。V10B1 的 macOS Release shader/窗口证据
保持 `Verify`。详细退出条件见 `docs/visual-quality-roadmap.md`。

历史运行结果和逐项证据保存在 `docs/project-ledger-2026-08-17.md` 与
`docs/runtime-validation.md`。

## 日常开发最低门槛

| 改动 | 最低必要验证 |
| ---- | ------------ |
| 所有 C++ 改动 | 受影响目标能够编译；运行对应定向自动测试。 |
| 世界、区块、实体或持久化 | 定向自动测试 + `HelloMine3DWorldRuntimeSmoke`。 |
| 物品、容器、制作、工具或食物 | 状态守恒、容量边界、失败原子性、固定 tick 和保存/重载测试。 |
| UI 或输入 | 自动焦点隔离测试；开发者进行一次交互冒烟。正式 Physical Input v2 记录可后移。 |
| 资源、配方、声音或 shader | 资源清单/解析验证；缺失和非法引用必须明确失败。 |
| terrain atlas 或 HUD/手持图标 | `tools\validate_terrain_atlas.ps1`、确定性重建、Alpha/空 tile、block 分面、Material 坐标、资源包与隐藏固定截图。 |
| 顶点格式、网格、光照或 AO | 确定性角落/边界夹具、MeshDirty、隐藏固定截图、既有 schema 3 顶点/索引/构建/驻留字段的补充比较和相关 Q1。仅改每顶点值时不得误报为顶点格式升级。 |
| 雾、天空、云、阴影或后处理 | shader 正反例、关闭回退、固定昼夜截图、窗口缩放/切世界清理、各图形档性能和开发者视觉检查；修改 shader/顶点接口时增加 macOS Release 真实窗口冒烟。 |
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
| 性能采集 | `tools\run_perf_baseline.ps1`；正式六场景使用 `tools\capture_release_candidate_performance.ps1` | 区块加载、网格、更新、渲染提交 |
| 性能比较 | `tools\compare_perf_baselines.ps1` | 可能改变帧时间或世界驻留的改动 |
| 资产检查 | `bash scripts/check_assets.sh` | 资产和数据 |
| V10B2 图集合同 | `tools\validate_terrain_atlas.ps1` | 37 个语义/双语名、Alpha 边界、分面、HUD/手持一致性与确定性输出 |
| R3 自动预检 | `tools\validate_r3_automated_preflight.ps1 -Configuration Release -Build` | 控制器、交互、容器、战斗、D6 和后台窗口焦点的逻辑回归；结果不能替代真人输入 |
| R3 v1 真人输入基线 | `docs\manual-input-acceptance-v1.md` 和 `tools\validate_manual_input_record.ps1 -RequirePass` | 只验证现有十二项键鼠/焦点记录；通过也不自动关闭当前 D4/D6/R3，后续先立 Physical Input v2。 |
| 开发者视觉检查 | `docs\manual-product-experience-acceptance-v1.md` A 节、`tools\validate_developer_visual_record.ps1 -RequirePass` 和视觉路线截图矩阵 | 每个 V10 批次由项目所有者自行启动真实 Release 窗口观察 5-10 分钟，记录身份、场景、PASS/FAIL 和一句理由；自动化不打开可见窗口，不使用 R3 `-RequirePass`。 |
| 正式产品体验 | `docs\manual-product-experience-acceptance-v1.md` B/C 节 | 视觉风格、双语可读性、音效/音乐听感；与 Physical Input v2 共享构建身份但分开判定。 |
| 长时间 soak | `tools\run_world_soak.ps1`；正式双 profile 使用 `tools\run_release_candidate_soak.ps1` | 区块/实体生命周期、存档、后台加载 |
| 资源包 | `tools\validate_resource_packs.ps1` | manifest、资源解析、启动预检 |
| 干净发行包 | `tools\package_windows_release.ps1` | 发行包、manifest、资源解析 |
| 世界目录 | `HelloMine3DWorldCatalogueSmoke` | 世界发现、名称、id、目录 |
| 事务保存/恢复 | `HelloMine3DStorageTransactionSmoke`、`HelloMine3DWorldBackupSmoke` | 保存发布、隔离、备份、恢复、格式迁移 |
| 崩溃产物 | `HelloMine3DCrashDiagnosticsSmoke`、`tools\validate_crash_diagnostics.ps1` | 异常处理、dump、sidecar、符号、崩溃 UX |
| 数据竞争 | `bash scripts/verify_tsan.sh` | 后台加载、区块图同步、工作线程调度 |
| 启动/保存/恢复预算 | Q1 schema + Q2 阶段计时 | 目录、启动、进世界、保存、备份、恢复 |
| 内容规模 | `docs/q3-scale-soak-contract-v1.md` + 正式双 profile soak | 快速移动、人口规模、持久化压力 |
| 完整玩法切片 | G6 fixture + 截图 + 真人输入 | 第 8 阶段最终封板 |

## 验收节奏

| 时机 | 要求 |
| ---- | ---- |
| 功能开发中 | 编译、定向自动测试、数据守恒、存档兼容和必要交互冒烟。 |
| 一个玩法任务完成 | Debug/Release 相关目标、世界冒烟和该任务的失败边界。 |
| G6 集成前 | 清理所有主干构建失败，补齐跨系统回归。 |
| 发布候选 | 全平台门禁、Physical Input v2、适用产品体验、H1-H3、Q1-Q3、长稳运行、截图和干净包。 |
| Stage 9 单批完成 | 批次合同、定向测试、全部受影响旧版迁移、相关 Q1、必要的 60-120 秒 nominal/stress 和隐藏客户端。 |
| BETA-RC | 全平台门禁、全迁移、物理输入/产品体验真实状态、H1-H3、Q1-Q3 正式双档长稳、许可证、截图、bundle 和干净包新哈希。 |
| Stage 10 单批完成 | 批次合同、VS2017/v141 双配置受影响目标、shader/资源负例、固定 before/after RuntimeReadback、开发者视觉检查和相关 Q1；图形功能必须可关闭。修改 shader/顶点接口时当批补 macOS Release 真实窗口冒烟，否则跨平台状态保持 `Verify`。 |

## 第 8 阶段批次门禁

| 批次 | 合入前最低证据 | 可后置证据 |
| ---- | -------------- | ---------- |
| `BLD-1` | 重新生成 VS 工程；WorldRuntimeSmoke/Soak 项目同时包含崩溃源和 `dbghelp.lib`；Windows Debug/Release 全量编译及定向目标通过。 | 已于 2026-08-17 通过后台 Release 受控 dump 和干净包。 |
| `K4` | WorldCatalogue、StorageTransaction、WorldBackup 定向测试；创建/重命名/删除恢复的路径与故障边界；一次菜单交互冒烟。 | 正式真人输入记录和发行包恢复演练。 |
| `G2` | RecipeSmoke；库存/制作状态守恒；预览无副作用；提交失败原子性；关闭、满背包、连点和重载。 | UI 截图和完整流程真人记录。 |
| `G3` | 工具表解析；破坏进度状态机；等级/掉落/耐久边界；物品实例存档迁移；固定动作性能采样。 | 正式 Q1/Q3 预算和长稳已完成；输入手感随 R3 记录。 |
| `G4` | 设置解析/版本/原子写入；应用/取消；暂停模拟；UI 焦点隔离；开发者交互冒烟。 | R3 正式窗口和输入记录。 |
| `G5` | 音频资产检查；事件去重；音量/pause/mute；dummy backend；无设备和缺失资源降级。 | 目标设备主观听感、音乐混音和完整发行资产验收。 |
| `G6` | 干净启动垂直切片 fixture、跨系统保存/恢复、Debug/Release 相关目标、截图和可比较性能采样。 | 发布候选的全平台、真人输入、正式预算、soak 和干净包证据。 |
| Alpha 检查点 | 跟踪 v3 迁移 fixture；Debug/Release 世界与崩溃 smoke；双模式隐藏客户端；Alpha 基线/复测；匹配及错误 PDB。 | H3、混合 dump 栈、符号归档、其余六个 Q1 场景预算和 Q3 正式长稳已在 RC 关闭；剩余 Physical Input v2 与产品体验人工证据。 |
| `N3` | 严格食物解析；面包配方；成功/满血/暂停/死亡/冷却/UI 占用的原子语义；生命/冷却保存重载；v5→v6 迁移；目标事件；资源包与启动缺失检查。 | R3 真实按键手感、N6 HUD 统一和是否引入饥饿压力的产品评估。 |
| 封板 | R3、H1-H3、Q1-Q3、全平台构建、存档故障、资源包、截图、soak 和干净包全部通过。 | 无；失败只允许修复或明确重新立项。 |

## Stage 9 / Beta 预排门禁

本节只定义未来批次启动后的最低证据，不表示这些工作已经完成或已经加入当前 77 项总账。

| 批次 | 合入前最低证据 | 阶段结束补充证据 |
| ---- | -------------- | ---------------- |
| `RC0` | `Superseded`：bundle、文档一致性和自动工程基线已由 BETA-RC 覆盖，不再作为活跃批次。 | 真人输入归未来 Physical Input v2；R3 v1 十二项通过也不能自动关闭当前 D4/D6。 |
| `N7A` | 独立结局状态/奖励 epoch；不以目标耗尽推导胜利；预计 v8→v9 及全部旧版迁移；状态非法值、事务失败、备份恢复；中英文 key/fallback。 | 胜利前中后保存重开，世界列表只从持久状态标记；重复重入不重复发奖。 |
| `N7B` | 激活/守卫状态机；复用现有敌人；actor/事件/驻留上限；死亡、暂停、卸载和遭遇中恢复；相关 Q1 和短时 soak。 | 干净新世界无 debug 注入完成胜利并继续 Playing；R3 终局路径。 |
| `N8A` | 前摇/命中/击退/格挡；显式状态转换、目标失效、固定 tick 射线/寻路预算和调试快照。 | 多敌人压力、人工战斗可读性、内容规模 Q1 和阶段 Q3。 |
| `N8B` | 战斗档案严格解析；投射物遮挡、寿命/距离/上限，死亡/卸载/保存重载清理；世界冒烟和短时 soak。 | 远程/防御差异清晰，无幽灵投射物、容量突破或重复命中；最终 Q3。 |
| `N9A` | 结构类型/cell/seed/terrain 版本/加载顺序确定性；footprint/间距/覆盖；禁止 `std::rand` 和同步邻区块加载。 | 结构计划快照、跨区块投影、快速流送 Q1 和旧 terrain v1/v2 身份。 |
| `N9B` | 初始战利品快照；箱子库存持久后不再初始化；玩家修改不被覆盖；资源/actor/结构上限。 | 两类地点综合旅程和正式 Q3；若启用 terrain v3，补全新旧世界矩阵。 |
| `N10` | 食物/精确配方/三槽冶炼严格解析；库存守恒；容量/连点/暂停/卸载/保存；主线可达、来源/消耗点和净增益循环检测。 | 综合经济旅程、平衡数据、库存/保存规模比较；饥饿仍需独立评估。 |
| `N11A` | 三档版本化参数；旧世界 Normal；世界保存/metadata 迁移；暂停菜单事务修改并在下个固定 tick 生效；同 seed/难度确定性。 | 三档核心旅程、内容规模/短时 soak、世界列表/备份恢复和人工确认。 |
| `N11B` | 有界事件状态和奖励去重；胜利前后/重载/备份恢复；不破坏主线结局。 | P2 伸缩项；纳入首个 Beta 才进入 BETA-RC 必过矩阵，否则明确后置。 |
| `N12A` | `en-US`/`zh-CN` key 对齐；fallback、非法 UTF-8、字体、长文本、极端缩放、字幕、Credits 和许可证。自动开发门禁已于 2026-08-26 通过。 | 隐藏客户端已通过；两种语言核心旅程真人截图和 UI 可读性按独立产品体验合同延期，不标 PASS。 |
| `N12B` | v3 采样定义、9 个 WAV、61 项 manifest、许可证/Credits、缓存/并发上限、缺文件/损坏/设备降级、停止与退出清理均已于 2026-08-26 通过；VS2017/v141 双配置世界 681/681、资源包 34/34，隐藏真实三帧后端无降级。 | 正式听感与真实无设备机器体验按独立产品体验合同延期，不标 PASS；自动 dummy/缺文件、资源包和生命周期证据已通过。 |
| `N12C` | v1 单通道流式定义、20 秒原创 WAV、settings v4、64 项 manifest、许可证/Credits、严格 RIFF/路径/时长、延迟/淡入淡出/低密度间隔、暂停/挂起/静音/音量归零、缺资源/设备、线程和退出清理已于 2026-08-26 通过；VS2017/v141 双配置世界 699/699、资源包 38/38，隐藏真实后端无降级，同身份 Q1 比较通过。 | 正式听感、实际暂停/切世界听觉过渡和真实无设备机器体验按独立产品体验合同延期，不标 PASS；自动生命周期与性能证据已通过。 |
| `BETA-RC` | 2026-08-26 工程通过：Windows 适用全门禁、v1→v11 迁移、结局/奖励、战斗/投射物、结构/战利品、经济、语言/音频清理、六类 Q1、Q3 双档各 1800 秒、崩溃/符号/84 文件干净包均通过。 | R3 自动预检通过，bundle 已验证；R3 v1 未正式签署，扩展物理输入与产品体验分别延期，不建本地 Beta 标签、不 push。 |

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

Windows 自动化 EXE 一律隐藏或后台运行。只有必须由真人操作并观察的 R3，以及事先取得用户
明确同意的 Stage 10 开发者视觉检查，才可以在安排好的验收窗口前台运行；`ShowWindowNoActivate`
只是 best-effort，不能作为不会抢焦点的保证。不得用自动输入伪装真人证据。

## Stage 10 视觉门禁

| 批次 | 合入前最低证据 | 阶段结束补充证据 |
| ---- | -------------- | ---------------- |
| `V10A` | 顶点 AO 0-3、侧边/对角规则、天空光/方块光、透明/未知邻域、跨 section 一致性；只有合并结果可重建全部内部采样才 greedy 合并，对角线按误差和固定 tie-break 选择；保持 32 字节 stride；Debug/Release 受影响目标。 | FS2 四图加洞穴/树冠/遗迹/营地 before/after；schema 3 既有 geometry/mesh/residency 字段补充比较、快速流送/规模 Q1、短 Q3 和开发者检查。超过 10% 未获批准不得关闭。 |
| `V10B1` | 图集尺寸/分格/坐标和颜色参数严格解析；旧图集像素兼容；不得静默改变 V10A AO/光照曲线；缺失/越界负例。 | shader 负例、Windows 双配置、开发者检查和 macOS Release 真实窗口冒烟。 |
| `V10B2` | 原创 top/side/bottom 资产、生成脚本、来源/许可、manifest、透明边界和世界/HUD/手持一致性。 | 近景/中景截图、资源包、相关 Q1、干净包和开发者检查。 |
| `V10B3` | 生态 tint 范围、坐标/seed/加载顺序确定性 tile 变体、greedy merge key 和 terrain v3 不变。 | 固定截图、相关 Q1、短 Q3 和开发者检查。 |
| `V10C` | 定向雾共享参数、云层高度/厚度/速度/颜色边界、帧率无关移动、进入云层与关闭回退。 | 森林/海岸正午/黄昏/夜晚/高处截图；地形/水/actor 雾一致；相关 Q1、开发者检查和 macOS Release 冒烟。 |
| `V10D` | settings v5、v0-v4→Off 迁移、双语 key、Stage 10 shadow 补充身份；Off/Medium/High、距离/纹理/bias/PCF 上限、能力不足回退和资源清理。 | 正午/黄昏各档截图和同档性能；Off 与 BETA/V10C 兼容；开发者检查和 macOS Release 冒烟。 |
| `V10E` | settings 预计 v6、v0-v5→Off 迁移、双语 key、Stage 10 post 补充身份；tone curve/抖动/轻 bloom 开关、HUD 排除、缩放和清理。 | 明暗阶梯、白天/夜晚/菜单截图、同档 Q1、开发者检查和 macOS Release 冒烟；未批准自动曝光、运动模糊、景深、SSAO、体积光。 |
| `VISUAL-RC` | 适用全门禁、完整截图/开发者检查矩阵、资源/许可/Credits、正式相关 Q1、干净包和 bundle。 | 因 V10A/V10B3 改变网格输出/驻留，以最终身份正式重跑 Q3 双 1800 秒；完整 macOS Xcode 门禁；产品体验和 Physical Input v2 独立记录，不 push、不自动打标签。 |

Stage 10 的地形 shader、顶点数据、图集、云、阴影或后处理变化都必须保留一个明确的低画质
回退路径。固定截图证明确定性和明显渲染缺陷；开发者视觉检查负责单批主观退出，正式产品体验
负责封板判断，二者都不替代 Physical Input v2。
