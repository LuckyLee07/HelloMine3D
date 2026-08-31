# HelloMine3D Iteration Plan

本文档保留 HelloMine3D 的长期架构方向和历史阶段划分。规划基于项目源码，
以及 `docs/minigame-reference.md` 中对 MiniGame 项目的参考分析。可执行任务和
当前状态以 `docs/todolist.md` 为准；已完成任务的详细历史证据保存在
`docs/project-ledger-2026-08-17.md`。本文件中的远期候选只有在被加入当前清单后
才成为迭代承诺。

总体原则：

1. 保持主干可编译和数据安全，在此基础上优先完成玩家可感知的玩法闭环。
2. 先把区块、资源、方块数据模型做稳，再做光照、存档、mod 和多人。
3. 保持 Windows/macOS 兼容作为长期约束，不引入会破坏跨平台边界的实现。
4. 从 MiniGame 借鉴架构方向，不直接照搬它的旧引擎、旧构建和重型 SDK。

## 当前项目状态

HelloMine3D 已具备这些基础：

| 能力 | 当前状态 |
| ---- | -------- |
| 构建系统 | Premake 已作为主构建入口，支持 Make/Xcode/Visual Studio 工程生成。 |
| 依赖管理 | 通过 `src/Engine` 与 `src/external` 本地源码构建 Ogre、FreeImage、FreeType、OIS、ImGui 和 GLM，不依赖 vcpkg 安装树。 |
| 运行资源 | `media/` 保存 shader、texture、block、shape、font、基础 recipe/tool/audio；严格解析器与 `scripts/check_assets.sh` 负责引用和边界校验。 |
| 世界结构 | `World`/`ChunkManager` 已具备负坐标、固定 tick、版本化存档、玩家与演员持久化。 |
| 区块网格 | `ChunkMeshBuilder` 使用有界 dirty 队列、18x18x18 halo、opaque greedy meshing、光照分界和异步快照版本校验。 |
| 方块数据 | `BlockDefinition`、`BlockRenderInfo`、`BlockShape`、`BlockBehavior` 与 `ChunkBlock` metadata 已落地。 |
| 地形生成 | biome、洞穴、矿石、植物和跨区块结构均由确定性阶段生成，且不依赖相邻区块加载顺序。 |
| 渲染 | Ogre GL3Plus 是唯一渲染路径，已有 terrain/water/glass/flora/程序化天空、演员、选择描边、HUD 和材质程序。 |
| 调试 UI | Ogre/OIS + ImGui 已提供 HUD、调试面板、运行时统计、截图和性能采集入口。 |

当前剩余验收风险：

| 风险 | 说明 |
| ---- | ---- |
| 目标 Windows 更新门禁已恢复 | 2026-08-20 最终 PowerShell 门禁通过 46 项资源、36 个性能夹具、Debug/Release 0 错误全量重建、十三目标、491 项世界回归、隐藏客户端、124,513 字节后台受控 minidump 与 66 文件干净发行包。 |
| 输入仍有平台型证据缺口 | OIS 到控制器的数据边界已自动验证，但真实 Windows 键鼠仍需人工验收；原生 arm64 Apple Clang ThreadSanitizer 已关闭后台加载器的 R4 证据缺口。现有 R3 v1 不足以单独关闭当前 D4/D6，后续需独立建立 Physical Input v2；视觉、双语和听感不并入 R3。 |
| 当前产品阶段已封板 | 第 8 阶段、Stage 9/N7-N12、BETA-RC、Stage 10/VISUAL-RC 与 Stage 11/P11-0-P11F Windows 自动工程均已完成。真人输入/试玩统一后置为 Deferred，正式产品体验和 macOS 原生证据继续保持 Verify；当前没有剩余已批准代码批次，多人、脚本化 mod、热更新和新渲染后端仍未立项。 |

## 迭代阶段

下列表格保留最初的架构拆分和优先级，不再直接表示未完成工作。当前交付映射
如下；其中未进入 `docs/todolist.md` 的 P2/产品化候选不计作当前待办。

| 阶段 | 当前交付状态 | 权威证据 |
| ---- | ------------ | -------- |
| 第 0 阶段 | 已完成 | `S0.1-S0.6`、`V4-V5` |
| 第 1 阶段 | 已完成 | `S0.4`、`A1-A4`、`S7.1` |
| 第 2 阶段 | 已完成当前范围 | `M1-M7`；顶点格式压缩未排期 |
| 第 3 阶段 | 已完成当前范围 | `S3.1-S3.6`、`C1-C3`、`C7` |
| 第 4 阶段 | 已完成 | `S2.1-S2.6`、`S6.1-S6.5`、`L1-L3`、`C4-C5` |
| 第 5 阶段 | 已完成当前范围 | `V3`、`P1-P5`、`L4`、`W1-W2` |
| 第 6 阶段 | 已完成当前范围 | `B1-B5`、`W3`、`R5`、`X1-X3` 已完成；B3 macOS 原生门禁于 2026-08-16 通过 |
| 第 7 阶段 | 实现完成，待人工验收 | D1-D6、R1-R2、R4-R5、X1-X3 已实现并有自动/硬件/TSan 证据；D2/D4/D6 的最终关闭等待扩展后的物理输入协议，不再宣称 R3 v1 单独足够。 |
| 第 8 阶段 | 自动化闭环，待人工验收 | `K1-K4`、`G1-G6`、`N1-N6`、`H1-H3` 和 `Q1-Q3` 已完成；目标、成长、恢复、战斗掉落、生态探索、可访问体验、诊断、正式 Windows 预算和双档长稳均已串联；物理输入、视觉/双语和听感证据独立延期。 |
| 第 9 阶段 | 工程完成，待 R3 | `N7-N12` 与 `BETA-RC` 不计入当前 77 项总账；BETA-RC 自动门禁、正式 Q1/Q3 和发行证据已封板，R3 真人项延期。 |
| 第 10 阶段 | Done（Windows 工程；macOS/产品体验 Verify） | `V10A`、`V10B1-B3`、`V10C-V10E` 与 `VISUAL-RC` 不计入当前 77 项总账；最终 Windows 全门禁、Q1/Q3、视觉矩阵、资源许可和干净包已封板。V10B1/V10C/V10D/V10E 的 macOS shader 子状态及正式产品体验保持 Verify。 |
| 第 11 阶段 | P11-0/P11A/P11B/P11-1/P11C/P11D/P11-2/P11E/P11F Engineering Done | 可玩性与操作体验路线不计入当前 77 项总账；可制造光源、核心输入、动作反馈、最小建造集、并行前 30 分钟、独特探索奖励、terrain v4 山地/洞口、敌人表现/路标共鸣与 PLAYABILITY-RC 自动工程封板均已完成。Physical Input v2、真人试玩、正式产品体验和 macOS 证据保留 Deferred/Verify。 |

### 第 0 阶段：稳定现有基础

目标：修复当前会影响后续迭代的正确性问题，确保区块修改、加载和渲染链路可靠。

| 优先级 | 任务 | 说明 | 验收标准 |
| ------ | ---- | ---- | -------- |
| P0 | 修正 `ChunkSection::Layer` 计数 | `Layer` 应根据旧方块和新方块是否 opaque 调整 solid count。 | 全 solid layer、全 air layer、混合 layer 的 `isAllSolid()` 结果正确。 |
| P0 | 恢复 mesh dirty 更新链路 | 方块修改后标记当前 section，边界方块标记相邻 section。 | 运行时破坏/放置方块后，mesh 能局部刷新。 |
| P0 | 明确负坐标策略 | 要么支持 floor division，要么短期明确禁止负坐标。 | 玩家移动到 chunk 边界不会出现错误 chunk/block 映射。 |
| P0 | 收紧 chunk 加载线程 | 后台线程不直接读 `Camera&`，主线程提交加载目标。 | ThreadSanitizer 或代码审查层面避免 camera 数据竞争。 |
| P1 | 修正 spawn 附近 chunk 预加载 | 当前 spawn 附近加载应使用 chunk 坐标，不应混淆 world block 坐标。 | 出生点周围 chunk 稳定生成并可见。 |
| P1 | 整理 `getChunk()` 副作用 | 明确哪些 API 会创建 chunk，哪些只查询。 | mesh 构建和读方块不会意外创建大量空 chunk。 |

涉及主要文件：

| 文件 | 关注点 |
| ---- | ------ |
| `src/HelloMine3D/World/Chunk/ChunkSection.h` | `Layer` 计数和 section mesh 状态。 |
| `src/HelloMine3D/World/Chunk/ChunkSection.cpp` | set/get block 越界访问和 mesh dirty。 |
| `src/HelloMine3D/World/Chunk/Chunk.cpp` | `setBlock()`、height map、dirty 通知。 |
| `src/HelloMine3D/World/World.cpp` | chunk/block 坐标映射、加载线程、更新队列。 |
| `src/HelloMine3D/World/Chunk/ChunkManager.cpp` | chunk 创建、加载、卸载 API 边界。 |

### 第 1 阶段：工程化和数据化

目标：让项目更适合持续扩展，降低资源、配置和方块数据出错成本。

| 优先级 | 任务 | 说明 | 验收标准 |
| ------ | ---- | ---- | -------- |
| P0 | 定义 chunk/section 状态机 | 统一 `Loaded`、`MeshDirty`、`CpuMeshReady`、`GpuBuffered` 等状态。 | 状态转换清晰，删除 mesh、重建 mesh、buffer mesh 不互相踩。 |
| P0 | 加强 `.block` 文件校验 | 缺字段、id 越界、texture coord 越界、非法 enum 都要报出具体文件。 | 资源错误能在启动时明确定位。 |
| P1 | 增加资源检查脚本 | 检查 shader、texture、font、block 文件是否存在和可读。 | `scripts/check_assets.sh` 能独立运行并返回错误码。 |
| P1 | 增加轻量 `AssetRegistry` | 在 `ResourcePaths` 上层维护逻辑资源名到真实路径的映射。 | 业务代码尽量不再拼散落的资源路径。 |
| P1 | 外置运行配置 | 从 `bin/config.txt` 或简单配置文件读取窗口、FOV、render distance、seed。 | 无需改代码即可调整常用运行参数。 |
| P2 | 拆分 ImGui 调试面板 | world、chunk、block、render、player 分开。 | 调试信息能支持后续区块和资源问题定位。 |

涉及主要文件：

| 文件 | 关注点 |
| ---- | ------ |
| `src/HelloMine3D/World/Block/BlockData.cpp` | `.block` 解析和校验。 |
| `src/HelloMine3D/World/Block/BlockDatabase.cpp` | 方块注册和数据一致性检查。 |
| `src/HelloMine3D/Util/ResourcePaths.h` | 项目根目录和资源路径解析。 |
| `src/HelloMine3D/Config.h` | 运行配置来源。 |
| `src/HelloMine3D/GUI.cpp` | 调试 UI 模块化。 |

### 第 2 阶段：体素核心性能

目标：支持更远视距、更大世界和更频繁的方块编辑。

| 优先级 | 任务 | 说明 | 验收标准 |
| ------ | ---- | ---- | -------- |
| P0 | Section mesh dirty queue | 只重建 dirty section，不全量扫描。 | 单个方块修改只触发相关 section mesh 重建。 |
| P0 | 邻居 halo cache | mesh 构建前缓存 16x16x16 外一圈邻居方块。 | `ChunkMeshBuilder` 中跨 section/world 查询明显减少。 |
| P1 | opaque greedy meshing | 先只对普通实体方块合并面片。 | 同等地形下 solid mesh face/vertex 数下降。 |
| P1 | mesh 构建剖析指标 | 输出每帧构建数量、耗时、face 数、buffer 次数。 | ImGui 或日志能看出 mesh 性能瓶颈。 |
| P2 | 顶点格式压缩 | 位置/uv/light 使用更紧凑格式。 | 在性能瓶颈明确后再做，避免提前复杂化。 |

涉及主要文件：

| 文件 | 关注点 |
| ---- | ------ |
| `src/HelloMine3D/World/Chunk/ChunkMeshBuilder.cpp` | halo cache、face 判断、greedy meshing。 |
| `src/HelloMine3D/World/Chunk/ChunkMesh.*` | CPU mesh 数据结构和 GPU buffer。 |
| `src/HelloMine3D/Renderer/ChunkRenderer.*` | solid mesh 渲染路径。 |
| `src/HelloMine3D/Renderer/WaterRenderer.*` | water pass 保持独立。 |
| `src/HelloMine3D/Renderer/FloraRenderer.*` | flora pass 保持独立。 |

### 第 3 阶段：方块和玩法数据模型

目标：让方块系统能承载更多状态、行为和内容，而不是继续靠硬编码扩张。

| 优先级 | 任务 | 说明 | 验收标准 |
| ------ | ---- | ---- | -------- |
| P0 | `ChunkBlock` 增加 metadata | 用于朝向、水位、生长阶段、开关等状态。 | 存储模型支持同一 block id 的不同状态。 |
| P0 | 拆分方块定义层 | 引入 `BlockDefinition`、`BlockRenderInfo`、`BlockShape` 等概念。 | 渲染、碰撞、基础属性不再全部挤在 `BlockDataHolder`。 |
| P1 | 引入轻量 `BlockBehavior` | 放置、破坏、tick、邻居变化、掉落等行为集中扩展。 | 新增一种特殊方块不需要到处写 `switch`。 |
| P1 | 方块形状资源化 | 从 MiniGame 的 `blockgeom.xml` 借鉴，但使用适合本项目的轻量格式。 | 非 cube 方块不再必须写死在 mesh builder 中。 |
| P2 | random tick 支持（已完成，C7） | 植物生长、流体、火等只处理需要 tick 的 section。 | 去重轮转队列每次最多处理四个活跃 section，每个 section 均匀采样三个体素，不扫描整张世界。 |

建议拆分后的模型：

| 类型 | 职责 |
| ---- | ---- |
| `BlockDefinition` | id、名称、硬度、是否碰撞、是否透明等静态属性。 |
| `BlockRenderInfo` | texture atlas 坐标、shader/render pass、透明排序需求。 |
| `BlockShape` | cube、cross、fluid、model、自定义几何。 |
| `BlockBehavior` | 放置、破坏、tick、邻居变化、掉落。 |
| `ChunkBlock` | 具体世界中的 id + metadata + light 引用或字段。 |

### 第 4 阶段：地形、世界持久化和光照

目标：把世界从“启动时生成的临时地图”推进到可保存、可扩展、可复现的沙盒世界。

| 优先级 | 任务 | 说明 | 验收标准 |
| ------ | ---- | ---- | -------- |
| P0 | world seed 配置化 | seed 不再是启动时随机静态值。 | 同一个 seed 能复现同一个世界。 |
| P0 | 地形 decorator 拆分 | tree、flora、ore、lake 等从主 terrain loop 拆出。 | 新增装饰物不需要改主 terrain 生成流程。 |
| P1 | chunk-level 存档格式 | 每个 chunk 独立保存，带 magic/version/chunk coords。 | 修改过的 chunk 重启后能恢复。 |
| P1 | per-column height map 整理 | 生成、光照、天气、碰撞都能复用高度数据。 | height map 更新和 block 修改一致。 |
| P1 | 初版 sunlight | 每个 block 保存 4 bit sunlight。 | 地表和洞穴有基础亮度差异。 |
| P2 | block light | 火把、岩浆、发光方块等 4 bit block light。 | 发光方块能局部影响 mesh 顶点光照。 |
| P2 | 局部 relight | 方块修改后只更新受影响区域光照。 | 不因一个方块变化重算整张地图。 |

涉及主要文件：

| 文件 | 关注点 |
| ---- | ------ |
| `src/HelloMine3D/World/Generation/Terrain/ClassicOverWorldGenerator.cpp` | 地形生成入口和 decorator 拆分。 |
| `src/HelloMine3D/World/Generation/Biome/*` | biome 数据和 top/fill/plant/tree 规则。 |
| `src/HelloMine3D/World/Generation/Structures/*` | 树和结构生成。 |
| `src/HelloMine3D/World/Chunk/Chunk.*` | height map、存档、section 管理。 |
| `src/HelloMine3D/World/Block/ChunkBlock.*` | metadata/light 数据承载。 |

### 第 5 阶段：视觉、交互和调试体验

目标：在核心稳定后提升体验，但避免视觉系统提前绑死玩法模型。

| 优先级 | 任务 | 说明 | 验收标准 |
| ------ | ---- | ---- | -------- |
| P0 | 调试信息面板 | chunk 数、mesh 数、face 数、加载队列、玩家 chunk 坐标。 | 出问题时能从 UI 快速定位。 |
| P1 | 水和透明方块规则 | 明确透明方块面剔除、排序和渲染 pass。 | water/glass/leaves 视觉稳定。 |
| P1 | block selection 和交互反馈 | 方块描边、当前 block 信息、放置预览。 | 编辑方块时反馈明确。 |
| P2 | 简单昼夜或雾效 | 建立时间/天空/光照参数通道。 | 不破坏现有 chunk shader 和光照规划。 |
| P2 | 资源加载失败 UI | shader/texture/block 文件错误能在窗口或日志里明确显示。 | 用户不需要看崩溃堆栈才能定位资源问题。 |

### 第 6 阶段：跨平台发布和长期扩展

目标：让 Windows/macOS 体验稳定，并为大系统保留清晰边界。

| 优先级 | 任务 | 说明 | 验收标准 |
| ------ | ---- | ---- | -------- |
| P0 | 固化 Premake 入口 | `premake/premake.lua` 是唯一工程生成入口。 | Windows/macOS 都从 Premake 生成工程。 |
| P0 | Windows/macOS 构建检查 | 明确 Ogre/OIS/ImGui/GLM 布局、静态库和 rpath。 | 新机器按 README 能构建运行。 |
| P1 | smoke build 脚本 | Make/Xcode/VS 至少有清晰构建命令。 | 每次结构改动后能快速验证构建。 |
| P1 | 资源打包 | `R5` 已生成自包含 Windows 包并从隔离根目录完成正/负向验收。 | 61 文件清单和确定性 ZIP 可复现，不携带开发期状态。 |
| P2 | 有界资源包 | `X1-X3` 已交付冻结的只读覆盖和回退，不承诺热更新、脚本或新增原生行为。 | 18 项 resolver 测试、有效 manifest、硬件截图、性能比较和打包覆盖通过。 |
| P2 | 多人网络 | 单独设计 server authority、chunk streaming、实体同步。 | 不在单机架构未稳定前启动。 |

### 第 7 阶段：可玩闭环、回归和发布硬化

目标：把已经分别验证的交互、存档、演员、方块行为和 UI 连接成一个玩家可完成、
可保存、可回归的垂直切片，再以稳定的发布包和资源边界承接后续内容扩展。
任务状态和逐项验收仍以 `docs/todolist.md` 为准。

| 顺序 | 任务组 | 交付目标 | 关闭条件 |
| ---- | ------ | -------- | -------- |
| 1 | `R1` 性能比较门禁 | 把现有信息型基线升级为可比较、会失败的回归门禁。 | 场景、配置、垂直同步和世界驻留可比后，P95/P99/长帧阈值自动判定。 |
| 2 | `D1-D2` 状态方块和容器 | 用箱子打通 BlockEntity、use、物品栏、UI 和区块存档。 | 创建、交互、转移、破坏和重载都有自动与硬件证据。 |
| 3 | `D3-D4` 实际 Mob 和战斗 | 把验证用 actor 能力接入正常世界生成、攻击、死亡、掉落和重生。 | 不依赖 debug spawn，固定种子下可复现且受容量/频率约束。 |
| 4 | `D5` 种植循环 | 用 metadata、行为和随机刻完成种植、生长、收获和再种植。 | 卸载不推进、重载保留阶段，资产和 manifest 全部通过。 |
| 5 | `R2-R3` 长稳与输入 | 增加至少 30 分钟的确定性 soak，并固化真实键鼠人工验收协议。 | 队列、内存、句柄、存档和物理输入有可留档的结果。 |
| 6 | `D6` 垂直切片验收 | 在一次正常玩家流程中组合容器、作物、Mob、战斗、掉落和重载。 | 双配置门禁、硬件截图、交互记录和性能比较同时通过。 |
| 7 | `R5` Windows 发布包 | 从 Release 输出生成隔离、可启动、清单完整的分发目录和归档。 | 干净根目录可完成预检与真实窗口启动，缺失/陈旧资源会失败。 |
| 8 | `X1-X3` 资源包 v1 | 增加只读目录包、确定覆盖顺序和有效资源视图。 | 无包保持兼容，有包可覆盖现有资源且不绕过启动校验。 |

执行约束：

1. `R1` 必须先于会改变场景规模的 D 任务完成。
2. `D1` 先定义 BlockEntity 生命周期，再由 `D2` 引入 UI，避免把状态所有权塞进渲染层。
3. `D3` 先解决生成、容量和恢复，再由 `D4` 增加战斗，避免验证 Actor 无限制增长。
4. `D6` 是集成验收，不得用 debug-only 注入代替正常玩法路径。
5. `X` 只处理进程启动时冻结的有效资源视图；热更新、脚本和下载信任不搭车进入。

### 第 8 阶段：可持续游玩与可靠发布

目标：在现有种植、容器、战斗、掉落和持久化闭环上增加可持续的成长目标，
同时保证玩家可以安全管理长期存档，开发者可以定位崩溃，内容增长不会绕过性能
预算。阶段投入以玩法和玩家体验约 60%、可靠性和产品化约 40% 为指导，不按任务
数量机械分配工时。

执行前提：

1. `R3` 仍需由真人在目标 Release 构建上完成，并最终关闭 `D2`、`D4` 和 `D6`，
   但它不再作为 K4/G2-G6 开发的开始条件；真实输入和发布证据在综合封板阶段集中完成。
2. `B3` 已于 2026-08-16 在 Apple M1 Pro 上通过完整 Xcode 原生门禁；`R4`
   又于 2026-08-17 用原生 arm64 Apple Clang TSan 运行真实 346 项世界栈和 V5
   并关闭。Windows 压力测试仍不得伪装成 sanitizer 证据。
3. 新增格式、指标和用户数据操作必须先定义版本、失败语义和兼容边界，再进入 UI。

| 任务线 | 正式任务 | 交付目标 | 阶段关闭条件 |
| ------ | -------- | -------- | ------------ |
| 长期存档 | `K1-K4` | 建立稳定世界目录、事务保存、有界备份、损坏恢复和世界管理 UI。 | 创建、保存、崩溃中断、恢复、重命名和可恢复删除均有自动与真人证据；任何失败不得覆盖最后一份已验证存档。 |
| 崩溃诊断 | `H1-H3` | Windows Release 本地生成 minidump 和脱敏上下文，建立符号化、受控崩溃和发行包验收。 | 受控崩溃可稳定生成并符号化至少一个项目栈帧；默认不联网、不自动上传，不把符号和开发路径混入发行包。 |
| 性能预算 | `Q1-Q3` | 把现有帧时间门禁扩展到冷启动、进入世界、保存/恢复、区块可见延迟和内容规模压力。 | 每个场景都有版本化身份、固定指标、已批准阈值和可比较/不可比较判定；超预算时门禁非零失败。 |
| 玩法成长 | `G1-G6` | 增加数据驱动配方、工作台、工具采集成长、暂停/设置、基础音频和新的综合垂直切片。 | 玩家能从创建世界开始完成采集、制作工具、战斗、保存、恢复和继续游戏；状态守恒、持久化、性能和输入证据全部通过。 |

### 第 8 阶段执行路线

批次顺序由玩家价值和依赖决定，不以 MiniGame 的目录或系统规模决定。MiniGame 只在
对应批次已经开始后提供设计复核，不能自动带入在线、脚本、商城、FMOD 或完整编辑器。

| 批次 | 主任务 | 交付范围 | 跟随护栏 | 退出条件 |
| ---- | ------ | -------- | -------- | -------- |
| 0：恢复基线（Done） | `BLD-1` | 给实际编译 `WindowsCrashDiagnostics.cpp` 的 Windows 游戏逻辑目标补齐 DbgHelp 链接，重新生成工程并恢复 Debug/Release 主干。 | H1 便携探针和后台受控 dump 通过；已采集不批准阈值的 Q1/Q2 Release 参考快照。 | WorldRuntimeSmoke/Soak 正确链接，完整 Windows 门禁通过。 |
| 1：世界入口（Done） | `K4` | 世界管理服务、列表模型、最近游玩、创建/打开/重命名、备份选择、恢复和可恢复删除，再接入主菜单。 | 44 项 K1/K4 断言、Debug 主程序编译、隐藏主菜单和直进世界冒烟通过；当时后置的真人输入现归 Physical Input v2。 | 玩家路径和失败边界已落地；id/目录稳定，显示名可变，删除可恢复。 |
| 2：制作闭环（Done） | `G2` | 制作会话、纯预览、最大可制作数量、原子提交、玩家制作区、工作台 3x3 和制作 UI。 | 54 项配方/制作与 351 项世界断言通过；资源清单增长为 38 项。 | G1 数据已通过正常玩家路径产生产物；连点、满背包、关闭和重载均不复制或吞物品。 |
| 3：成长闭环（Done） | `G3` | 工具定义表、破坏进度、采集等级、速度倍率、掉落限制、耐久和工具状态持久化。 | 64 项配方/工具、365 项世界、20 项资源包断言和 Debug/Release 隐藏客户端通过。 | 手、木、石形成三级采集能力；旧库存可迁移，木镐/石镐状态可恢复并解锁石头/铁矿掉落。 |
| 4：产品外壳（Done） | `G4` | 暂停状态机、设置草稿与快照、应用/取消/恢复默认值、版本化用户设置。 | 386 项世界回归覆盖模拟门、范围、迁移、事务失败、即时 FOV/视距和 seed 隔离；双配置隐藏客户端通过。 | 暂停冻结本地模拟；视距、FOV、灵敏度、反转 Y、音量等设置可预测地保存和恢复。 |
| 5：反馈层（Done） | `G5` | 业务音频事件、2D/3D/listener、分类音量、pause/mute、Windows `waveOut` 和 dummy 后端。 | 40 项资产清单、23 项资源包、403 项世界回归、事件去重、静默降级和并发上限均在 Debug/Release 通过。 | 七类基础提示已接线；失败动作不误报，缺定义/设备不阻止加载、保存或退出。 |
| 6：可玩 Alpha（Done） | `G6` | 整合主菜单、世界、制作、工具、战斗、暂停、音频、保存和继续游戏；补最低限度目标提示与平衡。 | 十步全流程 fixture、v1-v3 迁移、v4 状态、Debug/Release 自动矩阵；截图与性能参考移交下一检查点批准。 | 从干净启动无需 debug 注入即可完成完整单机流程，并在重启后继续。 |
| 7：Alpha 开发检查点（Done） | `H2`、`Q1/Q2` 跟随 | 已冻结 G6 旅程和存档迁移样本，批准 Alpha 性能基线，建立脱敏 sidecar 与离线符号工具骨架。 | Debug/Release 自动矩阵、隐藏客户端、性能复测及匹配/错误符号验证通过；证据见 `alpha-development-checkpoint-v1.md`。 | N1 可在稳定旅程、存档和性能参照上开始；R3、H3、Q3 与其余正式预算仍开放。 |
| 8：目标与首轮引导（Done） | `N1` | 严格目标定义、八类进度、当前/下一目标 HUD、G6 兼容门面和版本 5 存档。 | 429 项世界、24 项资源包、41 项清单及 v1-v4 迁移通过；合同见 `objective-system-contract-v1.md`。 | N2 可直接复用目标事件、完成集合和保存边界。 |
| 9：熔炉与铁级成长（Done） | `N2` | 冶炼定义、熔炉三槽、固定燃料/时间、负载 v1、铁锭、铁镐、铁剑和五步目标链。 | 447 项世界、65 项配方/工具、25 项资源包、43 项清单和双配置全量门禁；合同见 `smelting-progression-contract-v1.md`。 | N3 可直接复用库存守恒、固定 tick、玩家生命和目标事件边界。 |
| 10：食物与恢复（Done） | `N3` | 严格食物定义、主动食用命令、恢复上限/冷却、面包配方、保存与目标接入。 | 463 项世界、76 项配方/食物、26 项资源包、44 项清单和 v5→v6 迁移通过；合同见 `food-recovery-contract-v1.md`。 | N4 可复用恢复资源、生命存档和食用事件；饥饿压力另行评估。 |
| 11：战斗、敌人与掉落（Done） | `N4` | 两类敌人、木/石/铁剑、近战距离与固定 tick 冷却、一次性有界掉落、三条目标和 v7 存档。 | 477 项世界、92 项配方/敌人/工具、27 项资源包、45 项清单和 v6→v7 迁移通过；完整双配置门禁及隐藏客户端通过。 | 已交付；合同见 `combat-depth-contract-v1.md`。 |
| 12：生态与探索（Done） | `N5` | 五类生态身份、沙漠敌人压力、terrain v2 路标、发光核心、两步探索目标和 v8 存档。 | 487 项世界、92 项配方/材料/敌人、27 项资源包、46 项清单、23 项目标和 36 个性能比较夹具通过。 | 已交付；合同见 `ecology-exploration-contract-v1.md`。 |
| 13：产品体验（Done） | `N6` | 已交付 UI 层级、配方书、目标历史、九项可重映射键位、UI 缩放、操作提示、声音字幕和食用结果提示。低密度音乐因当前短提示后端缺少流式生命周期和许可证据而明确后置。 | 491 项世界、95 项配方、27 项资源包、46 项清单、36 个性能夹具和 Debug/Release 完整门禁通过；R3 真人记录未伪装为自动完成。 | 已交付；合同见 `product-experience-contract-v1.md`。 |
| 14：Release Candidate（工程 Done，人工项拆分延后） | Physical Input v2、产品体验 | H2-H3、Q1-Q3、R5 干净包和最终双配置自动门禁已完成；R3 v1 仅保留为历史十二项基线。 | 扩展物理输入与视觉/双语/听感证据分别按各自合同完成；不把自动预检当成人工 PASS。 | N1-N6 已冻结，只修缺陷。 |

#### 批次 1：K4 世界入口拆分

状态：`Done`。实现与失败语义固定在 `world-management-contract-v1.md`；普通启动进入
主菜单，带测试存档目录的自动化仍可直进世界。正式物理输入记录现归 Physical Input v2。

1. 先增加独立于 Ogre/ImGui 的世界管理命令层。查询使用 `WorldCatalogue`，保存和
   重命名元数据使用 `StorageTransaction`，恢复使用 `WorldBackup`；UI 不直接操作目录。
2. 世界目录使用稳定 id，重命名只修改显示名；创建、重命名、删除和恢复都返回可展示
   的结构化结果，避免 UI 从异常文本猜状态。
3. 增加 `MainMenu -> WorldList -> Loading -> Playing -> Paused` 应用状态；最近游玩只作
   默认选择，不绕过验证直接打开。
4. 删除进入项目自己的有界恢复区，并提供恢复/永久清理的明确入口；本批不做云世界、
   账号同步或联机列表。
5. 自动覆盖空目录、旧世界、损坏元数据、同名、路径逃逸、删除恢复和恢复失败回滚，
   再做一次开发者菜单交互冒烟。

#### 批次 2：G2 制作闭环拆分

状态：`Done`。实现、原子失败和 UI 焦点语义固定在 `crafting-contract-v1.md`；正式
物理输入记录现归 Physical Input v2。

1. 增加纯 `CraftingSession`/`CraftingPreview` 边界：输入网格和配方视图决定匹配结果、
   输出和最大可制作数量；查询不得修改玩家或容器。
2. 增加单次原子提交：先验证材料、输出容量和会话版本，再一次性消耗并发放；任何条件
   变化都整体失败。批量制作重复使用同一提交规则。
3. 玩家自带制作区只匹配 2x2，工作台匹配 3x3；新增材料 id 只能尾部追加，避免破坏
   已有存档。关闭界面时所有暂存材料必须确定地退回或落地。
4. UI 只展示会话快照并提交命令，不能直接增删库存。重点验证连点、满背包、关闭、
   方块被破坏、保存中断和重载。
5. MiniGame 的可制作数量和产物容量可作为复核；材料组替换、烹饪和自动化制作延期，
   不扩张 G1 当前精确材料合同。

#### 批次 3：G3 工具成长拆分

状态：`Done`。实现、失败语义和库存子格式固定在
`tool-progression-contract-v1.md`；正式持续按键与暂停焦点归 Physical Input v2，HUD 观感归独立产品体验合同。

1. 先定义轻量 `ToolDefinition` 和方块采集属性，只保留工具类别、等级、速度、耐久、
   方块硬度、最低等级和掉落规则；不复制 MiniGame 的大型 `BlockMaterial` 接口。
2. 将立即破坏改为可取消的破坏进度；目标变化、距离失效、打开 UI 或暂停都会取消，
   只有完成事件能产出掉落并消耗一次耐久。
3. 工具作为不可堆叠的物品实例保存耐久。先设计旧存档迁移和非法值拒绝，再升级库存、
   容器和玩家保存格式，避免只改内存模型。
4. 第一版只要求手、木、石三级采集能力，以及工作台和两级工具配方；熔炉、冶炼、
   铁级工具、附魔和材料修复留到可玩 Alpha 之后。

#### 批次 4：G4 暂停与设置拆分

状态：`Done`。模拟推进、用户设置/世界创建参数边界、版本 1 文件格式、事务失败和
应用语义固定在 `runtime-settings-contract-v1.md`；正式物理焦点归 Physical Input v2，观感记录归独立产品体验合同。

1. 暂停是应用状态，不是一个 ImGui 窗口：暂停时停止固定 tick、AI、作物、掉落和战斗，
   仍处理窗口、菜单与必要加载完成回调。
2. 把现有 `RuntimeConfig` 拆成版本化用户设置与世界创建参数。世界 seed 不跟随“恢复
   默认设置”，也不允许设置页改变已存在世界的身份。
3. 设置页编辑草稿；应用时按能力立即更新或标记重启，取消恢复快照。视距、FOV、输入、
   窗口和音量都必须有范围、默认值、未知版本策略与原子写入。
4. MiniGame 配置只用于核对字段覆盖率；其空的 `loadSettings/saveSettings` 不能作为
   持久化范例。

#### 批次 5：G5 音频反馈拆分

状态：`Done`。数据格式、事件所有权、播放/暂停/音量、静默降级和自动门禁固定在
`audio-feedback-contract-v1.md`；R3 v1 只保留历史十二项输入基线，当前缺口归 Physical Input v2，人工听感改由独立产品体验合同记录。

1. 业务层发布“发生了什么”的音频事件，后端决定资源、空间位置、并发、衰减和播放；
   现有 `SandboxEventBus` 可作为事件入口，世界逻辑不持有播放设备。
2. 接口至少覆盖 2D/3D、listener、UI/效果/环境分类音量、pause/mute 和 dummy backend。
   先完成一次依赖体积、许可证、Windows/macOS 和无设备行为的后端决策检查。
3. 每个动作只允许一个成功事件；输入、库存变化和世界变化不能各自重复播放。声音缺失
   由资产检查报告，设备不可用则切到 dummy，不能阻止世界加载或保存。
4. 可参考 MiniGame `OgreSoundSystem` 的接口边界，不迁移 FMOD、直播 DSP、平台资源层
   或其全局 Singleton 生命周期。

#### 批次 6-7：集成与 Alpha 检查点顺序

1. G6 先用一张固定种子世界和一条正常玩家路径打通垂直切片，再补随机世界冒烟；最低
   目标提示仅说明下一步行动，不引入 MiniGame 的 Lua 新手引导、活动或商城系统。
2. G6 冻结后执行 Alpha 开发检查点：固定旅程和迁移样本，批准一份 Q1/Q2 Alpha 性能
   基线，并建立 H2 脱敏 sidecar 与离线符号工具骨架。此时不反复录制仍会被后续 UI
   改动推翻的 R3 证据。
3. 再按 `game-development-roadmap.md` 逐项确认并推进 N1-N6；每项仍执行编译、定向测试、
   数据守恒和兼容门禁，不能以“最终验收后置”为由取消开发护栏。
4. N6 使主要 UI 和输入稳定后，H2-H3 已验证 dump、脱敏 sidecar、离线符号、下次启动
   提示和干净包；Q1-Q3 预算与双档长稳也已关闭。R3 v1 仍保留为历史真人输入基线。
5. 最终 Windows 双配置构建、存档故障、资源包、崩溃、性能、soak 和干净包自动门禁已
   通过；后续 Physical Input v2 与产品体验分开验收，不能再用 R3 v1 自动关闭当前 D4/D6。
   封板批只修缺陷，不继续增加内容。

关键设计约束：

1. `K2` 初期保持同步保存语义；只有 `Q2` 证明真实 IO 阻塞后，才另行设计异步
   命令队列。不得为了隐藏延迟牺牲顺序、原子性或退出保存。
2. 删除世界必须二次确认并进入可恢复区域；自动备份有容量/数量上限，恢复动作
   不得直接覆盖唯一副本。
3. `H` 默认只写本地；任何上传、用户标识、服务器存储或隐私协议都属于后续独立
   里程碑。崩溃上下文不得记录完整个人路径或未声明的用户内容。
4. `Q1` 必须先采集当前基线并批准阈值。新增玩法不得通过放宽旧阈值关闭回归；
   场景身份或驻留规模不兼容时应返回 `INCOMPARABLE`，而不是假装通过。
5. `G1` 的配方只引用已注册材料，沿用严格资源和 manifest 边界；配方和声音先作为
   base 资源，不能暗中给 resource-pack v1 增加新类别。未来覆盖能力必须另起版本化
   合同。本阶段不引入 Lua、可执行 mod 或运行时热更新。
6. Ogre/ImGui 只负责展示和输入转换；世界目录、保存事务、配方、工具和崩溃上下文
   保持独立于渲染层，继续保留未来 macOS 路径。

阶段级回归场景：

| 场景 | 必须记录的证据 |
| ---- | -------------- |
| 世界目录 | 空目录、旧版世界、多个世界、非法 metadata、重命名冲突和路径逃逸均有确定结果。 |
| 保存故障 | 在临时写、刷盘、校验和替换边界注入失败；最后一份有效世界仍可加载，失败产物被隔离。 |
| 备份恢复 | 自动备份数量有界；损坏主存档后可选择备份、先复制再恢复，并验证世界语义和版本。 |
| 崩溃诊断 | Release 受控崩溃生成 dump 和脱敏 sidecar，符号工具解析项目栈帧，发行包无符号泄漏且无网络上传。 |
| 性能预算 | 冷启动、进入世界、保存、恢复、快速移动、内容扩容和长稳场景输出统一 schema 并执行阈值。 |
| 新玩法闭环 | 创建世界、采集原料、制作工作台/工具、按等级采集、战斗、保存退出、恢复和继续游戏。 |

### 可玩 Alpha 之后已完成的内容阶段

G6 代表“完整游戏骨架可玩”，并不等于内容完成。其后的 `N1-N6` 六个工作包已经按依赖
全部交付，但没有回写拆分前 77 项正式总账。详细历史批次、退出条件、验证分层以及
`F:\env1_trunk` 的参考边界见 `docs/game-development-roadmap.md`；新的 Stage 9 预排见
`docs/beta-gameplay-roadmap.md`。

| 预排工作包 | 目标 | 进入条件 |
| ---------- | ---- | -------- |
| N1 目标与首轮引导（Done） | 事件驱动的轻量目标、阶段进度和一次可达成的会话终点。 | 已交付；合同见 `objective-system-contract-v1.md`。 |
| N2 熔炉与铁级成长（Done） | 冶炼、燃料、铁锭和铁级工具形成第二段资源曲线。 | 已交付；合同见 `smelting-progression-contract-v1.md`。 |
| N3 食物与恢复（Done） | 让小麦和战斗损伤进入真实资源循环，先恢复、后评估饥饿压力。 | 已交付；合同见 `food-recovery-contract-v1.md`。 |
| N4 战斗与掉落（Done） | 武器、敌人差异、攻击节奏和有界掉落奖励。 | 已交付；合同与门禁证据见 `combat-depth-contract-v1.md`。 |
| N5 生态与探索（Done） | 增加少量有用途的生态差异、资源和轻量地标。 | 已交付；合同见 `ecology-exploration-contract-v1.md`。 |
| N6 产品体验（Done） | 键位、辅助选项、UI 统一、配方/目标可读性、基础程序化音效反馈与声音字幕；正式采样音效和低密度音乐经评估后独立后置。 | 已交付；合同与取舍见 `product-experience-contract-v1.md`，输入缺陷由 Physical Input v2 发现，视觉/双语/听感缺陷由独立产品体验合同发现。 |

### 第 9 阶段：Beta 内容与首个胜利闭环

第 9 阶段不是扩大体素引擎底座，而是利用已经可靠的存档、目标、战斗、生态、诊断和性能
边界，先关闭真实首屏缺陷并建立视觉基线，再补齐一条有准备、有高潮、有结果、可以重玩的
单机旅程。`FS1-N12C` 和 BETA-RC 工程封板均已完成；Stage 9 自身没有后续批次，延期的
RC0 已被 BETA-RC 取代；R3 v1 保留为历史基线，D2/D4/D6/R3 继续为 `Verify`，当前产品开发
转入下文 Stage 10。

| 顺序 | 批次 | 核心结果 | 关键护栏 |
| ---- | ---- | -------- | -------- |
| 0 | FS1 首次进入正确性（Done） | 安全陆地出生、附近首轮木材、随机建议种子和受限占位世界救援。 | 双配置 532/532、目录 45/45 和隐藏 Release 客户端通过；不移动已有进度玩家，不升级保存或 terrain。 |
| 1 | FS2 天空、水面与颜色（Done） | 独立水面表现、风格化云层、雾色和整体色彩基线。 | 固定截图矩阵；不引入 PBR、实时反射或重型后处理。 |
| 2 | FS3 体素美术与 HUD（Done） | 一致图集、图标化快捷栏、手持物与基础交互反馈。 | 539 项双配置世界回归、27 项资源包、47 项清单、来源记录、隐藏截图与焦点守卫通过；人工输入与体验独立后置。 |
| 3 | RC0 基线收口（Superseded） | bundle、文档一致性和自动基线已由 BETA-RC 覆盖。 | 不再作为活跃批次或标签入口；Physical Input v2 和产品体验独立记录。 |
| 4 | N7A 结局状态与文本键（Done） | 独立世界结局事实、奖励 epoch、世界列表标记和语义化中英文 key 骨架。 | 不以目标耗尽推导胜利；保存迁移、恢复、fallback 和幂等。 |
| 5 | N7B 路标激活与胜利（Done） | 激活路标，以现有敌人组成有界守卫战并显示胜利覆盖层。 | 保持 Playing、胜利后继续沙盒；不提前建立 Boss/GameMode。 |
| 6 | N8A 战斗可读性（Done） | 前摇/恢复、方向性受击、击退和反馈。 | 小型显式 FSM、目标失效恢复、固定 tick 预算和调试快照。 |
| 7 | N8B 远程敌人与投射物（Done） | 版本化战斗档案、远程威胁、瞬态投射物和最低限度格挡。 | 遮挡、寿命/距离/容量、死亡/卸载/重载清理；投射物不保存。 |
| 8 | N9A 确定性结构框架（Done） | 复用 cell hash/`StructureBuilder` 固定结构归属、投影和覆盖规则。 | 不用 `std::rand`、不同步加载邻区块、不另设结构版本或通用 Manager。 |
| 9 | N9B 遗迹、营地与战利品（Done） | 两类 seed 稳定地点带来探索风险与奖励。 | 初始战利品快照、持久箱子库存、terrain 身份和旧世界流送预算均通过。 |
| 10 | N10 食物、冶炼与资源经济（Done） | 已交付 4 种食物、3 类冶炼、2 类燃料、19 个精确配方和经济可达性校验。 | 117 项配方、650 项世界回归、完整门禁、同身份 Q1 与两组 60 秒 soak 通过；合同见 `resource-economy-contract-v1.md`。 |
| 11 | N11A 难度档案（Done） | 已交付三档世界难度，创建时选择、暂停菜单修改并在下个固定 tick 生效。 | 保存 v10、旧世界 Normal、集中版本化参数和 metadata，不改变 terrain seed；658/658 世界与 53/53 目录回归通过。 |
| 12 | N11B 胜利后事件（Done） | 三次有界、可选、显式启动的两波回响试炼。 | 保存 v11、路标 payload v2、三次一次性奖励、死亡/重开/替换核心边界、666 项世界与 59 项目录回归、完整门禁、Q1 和两组 soak 通过。 |
| 13 | N12A 本地化完成（Done） | 已完成中英文、字体、长文本、语义字幕、Credits 和许可。 | 341 个双语 key、675 项世界、32 项资源包、51 项清单、隐藏客户端和完整门禁通过；真人截图/可读性改由独立产品体验合同延期。 |
| 14 | N12B 正式采样音效（Done） | 已建立采样音效 schema/后端并替换程序化占位声音。 | 9 个 WAV、312,230 字节缓存、双配置 681 项世界/34 项资源包、隐藏真实后端和完整门禁通过；真人听感改由独立产品体验合同延期。 |
| 15 | N12C 低密度音乐（Done） | 已交付单通道流式环境音乐及暂停、淡入淡出、降级和清理。 | settings v4、原创 20 秒 WAV、64 项清单、699 项世界、38 项资源包、真实 WaveOut、完整门禁和同身份 Q1 通过；真人听感独立延期。 |
| 16 | BETA-RC（工程 Done） | 已重建迁移、性能、长稳、崩溃、自动输入预检和发行包基线。 | 六类 Q1 与两档 1800 秒 Q3 通过；bundle 已验证；Physical Input v2 与产品体验未完成，故不建标签、不 push。 |

N7A 已把保存升级到 v9，N11A 升到 v10，N11B 升到 v11；N9 只在生成身份变化时引入 terrain v3，
结构不另设独立版本；N12 只在载荷真实变化时升级设置或音频定义。N12C 原为可后置伸缩项，
当前已明确纳入并完成；其余 P0/P1 基础批次未因伸缩边界删减。每批开始前分别建立合同，详细范围、
非目标、最低回归和退出条件统一见 `docs/beta-gameplay-roadmap.md`；FS1-FS3 的细化合同见
`docs/first-session-visual-baseline-contract-v1.md`。

### 第 10 阶段：视觉质量升级

第 10 阶段在不改玩法、save v11 和 terrain v3 的前提下，提高已有世界的空间层次、材质
一致性和大气深度。Luanti 只作为公开算法与职责参考；实现保持独立，正式素材必须原创或具有
明确兼容许可证。权威合同见 `docs/visual-quality-roadmap.md`。

| 顺序 | 批次 | 规模 | 核心结果 | 关键护栏 |
| ---- | ---- | ---- | -------- | -------- |
| 1 | V10A 顶点平滑光照与 AO（Done） | M/L | 四角邻域光照与接触遮蔽替代整面单亮度；共面 byte-identical solid 顶点通过索引复用。 | 保持 32 字节顶点布局；所有者已批准 exact AO 性能例外，最终身份已在 VISUAL-RC 重跑并通过 Q1/Q3。 |
| 2 | V10B1 材质与图集管线（Done；macOS Verify） | M | 参数化图集/tile/颜色管线。 | 旧图集静态前景像素兼容、shader/资源启动负例和 Windows 双配置通过；macOS Release 窗口待补。 |
| 3 | V10B2 原创材质资产（Done） | L | 37 个原创分面材质与世界/HUD/手持一致性已实现。 | 106 项资产合同、来源/许可、67 项 manifest、资源包、Q1、固定图和干净包通过；授权审阅者逐图静态视觉记录为 PASS。 |
| 4 | V10B3 生态着色与确定性变体（Done） | M | 受控 tint 和坐标稳定的自然 tile 变体。 | terrain v3 不变；变体进入 greedy key；261 项图集、Q1、短 Q3 与五图开发者检查通过。 |
| 5 | V10C 定向大气与立体云（Done；macOS Verify） | M/L | terrain/water/actor/sky 共享定向雾，有界云层提供视差、绝对时间移动和上下表面。 | 双配置聚焦 21/21、Release 世界 741/741、十图多帧视觉 PASS；所有者批准快速流送单样本可见延迟例外，macOS Release 窗口待补。 |
| 6 | V10D 可选方向阴影（Done；macOS Verify） | L | 单近景太阳 shadow map 与 Off/Medium/High 档位已经落地。 | settings v5、v0-v4→Off、双语 key、图形性能身份、能力回退、双配置回归、同场景各档性能和六图视觉 PASS；最终无需性能例外，macOS 窗口待补。 |
| 7 | V10E 轻量后处理（Done；macOS Verify） | M/L | 可关闭 tone curve、确定性抖动和八采样极轻 bloom 已落地。 | settings v6、v0-v5→Off、HUD 排除、能力回退、双配置/资源/失败负例、同档性能与六图视觉 PASS；macOS 窗口待补。 |
| 8 | VISUAL-RC（Done：Windows 工程；macOS/产品体验 Verify） | M | before/after、开发者检查、渲染身份、性能、许可证、干净包和 bundle 封板。 | Windows 全门禁、正式 Q1/Q3、资源/许可、97 文件包和 Xcode 静态工程图通过；macOS 原生、正式产品体验与 Physical Input v2 独立保持真实状态；不 push。 |

Stage 10 的历史执行顺序为
`V10A -> V10B1 -> V10B2 -> V10B3 -> V10C -> V10D -> V10E -> VISUAL-RC`。每批单独
实现、验收和本地提交。该路线已经完成，VISUAL-RC 后产品方向评审选择下文 Stage 11；不再
追加渲染批次来掩盖操作与玩法问题。

### 第 11 阶段：可玩性与操作体验

第 11 阶段先补可制造光源这一项最小内容依赖，再解决操作可预测性和动作可读性，随后重构
前 30 分钟的选择、探索奖励、地形轮廓和战斗表现。工程批次仍按固定顺序逐个开放；真人/主观
验收按项目所有者决定统一后置为 Deferred，不阻塞可自动验证的开发。权威诊断、数据版本预案、Physical Input v2、PLAYABILITY-RC
和非目标见 `docs/playability-experience-roadmap.md`。

| 顺序 | 批次 | 规模 | 核心结果 | 关键护栏 |
| ---- | ---- | ---- | -------- | -------- |
| 1 | P11-0 世界光源（Engineering Done；真人 Deferred） | M | 可制造火把、Rose 修正、metadata 熔炉发射和真正翻转时的局部重光照。 | 工程/视觉证据与 PLAYABILITY-RC 正式 Q1 已完成；真人动态记录后置。 |
| 2 | P11A 核心操作手感（Engineering Done；Physical Input v2 Deferred） | M/L | 分离使用/放置/格挡，补鼠标重绑定和冲刺/潜行模式，统一灵敏度与焦点/捕获恢复。 | settings v7、v0-v6 迁移、双配置与自动回归完成；真实输入不冒充 PASS。 |
| 3 | P11B 动作反馈（Engineering Done；真人 Deferred） | M | 手/工具、裂纹、粒子、命中/受击/冷却、拾取和音频微变体已实现。 | 自动时序、上限和关闭回退通过；真实动态舒适度后置。 |
| 4 | P11-1 建造材料与工具（Engineering Done；真人 Deferred） | M/L | 木板、圆石、单方块门和 axe/shovel 的最小建造与工具选择已实现。 | 双配置与自动门禁通过；工具差异和建造动态观感后置。 |
| 5 | P11C 前 30 分钟（Engineering Done；真人 30 分钟 Deferred） | M/L | 成长、建造、探索最多三项并行机会、独立进度和随材料展开的配方书已实现。 | 双配置、P11C 41/41、Release 世界 803/803、配方/资源回归通过；完整新世界 30 分钟试玩后置。 |
| 6 | P11D 探索奖励（Engineering Done；真人 Deferred） | L | 遗迹罗盘、营地护符和既有路标核心形成独特能力结果。 | save v12/奖励 v1 与旧世界 v0 已冻结；双配置、定向和完整回归通过，真人逐结构体验后置。合同见 `exploration-reward-contract-v1.md`。 |
| 7 | P11-2 地形轮廓与洞口（Engineering Done；真人 Deferred） | L | terrain v4 已提供 Mountain 高度域、Stone 高峰和可发现天然洞口，v1-v3 输出冻结。 | 双配置、定向 9/9、Release 世界 820/820、次级回归与 PLAYABILITY-RC 正式 Q1 通过；真人远景/通行性后置，湖泊/河流/瀑布未进入本批。合同见 `terrain-contours-cave-entrances-contract-v1.md`。 |
| 8 | P11E 敌人表现（Engineering Done；真人 Deferred） | L | 6-7 部件轮廓、玩法状态关键姿态、身份化掉落、死亡表现隔离和 Waystone 共鸣已实现。 | 双配置、定向各 35/35、Release 世界 832/832、配方 122/122、资源包 80/80、84 项 manifest 和客户端编译通过；真人动态辨识与守护战体验后置。合同见 `enemy-presentation-waystone-resonance-contract-v1.md`。 |
| 9 | P11F PLAYABILITY-RC（Engineering Done；真人/产品体验/macOS Deferred） | M | 已汇总自动门禁、迁移、性能、恢复、包和延期证据。 | VS2017/v141 双配置、832/832 世界、六类正式 Q1、双档 1800 秒 Q3、崩溃/符号链路与 104 项干净包通过；真人里程碑、误操作、死亡与困惑记录保持 Deferred；未 push 或打标签。报告见 `playability-release-candidate-report-2026-08-31.md`。 |

执行顺序固定为 `P11-0 -> P11A -> P11B -> P11-1 -> P11C -> P11D -> P11-2 -> P11E -> P11F`。每批需要独立合同、
定向自动回归均已完成；真人试玩统一登记 Deferred，恢复时 P11C 至少覆盖完整前 30 分钟。P11F
已汇总工程证据与延期清单，未把未执行的真人体验写成 PASS。当前没有下一项已批准代码批次。

## 不建议近期投入的方向

| 方向 | 原因 |
| ---- | ---- |
| 多人网络 | 会改变世界权威、事件同步、实体同步、存档和输入模型。 |
| Lua/脚本系统 | 先通过 `D1-D5` 验证 C++ 扩展点；脚本所有权、沙箱和调试能力必须另行设计。 |
| 资源热更新、在线下载包和可执行 mod | `X1-X3` 只做冻结的只读覆盖层；运行时失效、网络信任和执行权限是独立系统。 |
| D3D/Vulkan 后端 | 当前目标是 Windows/macOS 兼容，Ogre GL3Plus 足够支撑近期开发。 |
| 完整编辑器 | 先做资源校验和 block atlas 工具，避免工具链复杂度过早膨胀。 |
| 大规模引入 MiniGame 代码 | MiniGame 历史依赖重，适合参考架构，不适合迁移源码。 |

## 历史推荐执行顺序

以下顺序记录规划时采用的小闭环。闭环 1、3、4、5 已完成；闭环 2 曾在
Windows-first 阶段延期，macOS 原生 `B3` 已于 2026-08-16 补齐并通过。

### 闭环 1：区块修改可靠

1. 修正 `Layer` solid count。
2. 恢复 `setBlock()` 后 mesh dirty 标记。
3. 方块修改后重建当前 section 和边界邻居。
4. 加 ImGui/debug log 显示 dirty section 数。
5. 手动验证破坏/放置方块后 mesh 正确刷新。

### 闭环 2：资源和配置可靠

1. 给 `.block` 解析增加默认值和错误诊断。
2. 增加 `scripts/check_assets.sh`。
3. 启动时打印资源根目录和缺失资源。
4. 把 render distance、FOV、seed 放到配置文件。
5. Windows/macOS 分别跑一次构建和启动。

### 闭环 3：区块性能提升

1. 引入 section mesh dirty queue。
2. 增加 mesh 构建统计。
3. 增加 halo cache。
4. 对 opaque cube 做 greedy meshing。
5. 对比优化前后的 face 数和构建耗时。

### 闭环 4：方块系统扩展

1. `ChunkBlock` 增加 metadata。
2. 拆 `BlockDefinition` 和 `BlockRenderInfo`。
3. 新增一个需要 metadata 的方块验证设计，例如朝向方块或生长阶段植物。
4. 拆 `BlockBehavior`。
5. 为后续存档锁定 block id + metadata 表示。

### 闭环 5：可保存世界

1. 固定 world seed。
2. 设计 chunk 存档文件头和版本。
3. 保存 dirty chunk。
4. 启动时优先加载存档，没有存档再生成。
5. 验证修改方块后重启仍保留。

## 验证基线

每轮迭代完成后至少做这些检查：

| 检查 | 命令/方式 |
| ---- | --------- |
| Windows 双配置完整门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_build.ps1 -VisualStudioVersion 2017`；无活动桌面时显式加 `-SkipRealWindow`，真实窗口相关结果记为 Deferred。 |
| Linux/macOS Make 双配置门禁 | `bash scripts/verify_build.sh` |
| macOS Xcode project | `bash scripts/verify_xcode.sh` 先验证 9 个合同/工程图夹具及实际 29 项工程清单，再构建 Debug/Release、执行十一项测试和客户端启动探针；陈旧/缺失工程、重复或多分组引用、手工目标顺序及第一方编译告警均会使门禁失败。 |
| 资源检查 | `bash scripts/check_assets.sh`，资产引用缺失时返回非零。 |
| 手动运行 | `bash scripts/run.sh release`，观察 chunk 加载、放置/破坏、退出是否正常。 |
| 性能回归比较 | `tools/compare_perf_baselines.ps1`；D/R5/X 中改变场景、资源或驻留规模的任务关闭前必须运行。 |
| 长时间稳定性 | `tools/run_world_soak.ps1`；区块、演员、存档或后台线程变更按固定种子动作表运行。 |
| 物理输入验收 | `docs/manual-input-acceptance-v1.md` 只保留十二项历史基线；`docs/physical-input-acceptance-v2.md` 已定义十三项当前合同，真实运行统一 Deferred。 |
| 产品体验验收 | `docs/manual-product-experience-acceptance-v1.md`；视觉、双语可读性和听感独立记录，不并入物理输入。 |
| 干净目录发布验收 | `tools/package_windows_release.ps1`；从隔离根目录完成 manifest 预检、validation-only、真实窗口和负向校验；`-SkipRealWindow` 只关闭需要 OpenGL 窗口的路径并在摘要中保留 Deferred。 |
| 存档故障与恢复（第 8 阶段） | K2 故障注入已证明发布失败不会覆盖最后有效存档；K3 已交付备份容量上限、损坏隔离、发布中断回滚和完整状态恢复 harness。 |
| 本地崩溃诊断（第 8 阶段） | H1-H3 交付受控崩溃、sidecar、符号化和干净包探针；默认禁止上传并检查隐私/符号泄漏。 |
| 扩展性能预算（第 8 阶段） | Q1-Q3 扩展采集和比较器；覆盖启动、世界进入、保存/恢复、区块可见延迟和内容规模。 |
| 新玩法综合验收（第 8 阶段） | G6 从干净包运行创建世界、制作工具、等级采集、战斗、保存失败恢复和继续游玩。 |
| Beta 批次回归（第 9 阶段） | 按 `docs/beta-gameplay-roadmap.md` 的触发矩阵选择迁移、确定性、内容规模、短时 soak、R3 和发行证据；自动化 EXE 保持隐藏运行。 |
| 视觉批次回归（第 10 阶段） | 固定 seed/坐标/时间/分辨率的 before/after RuntimeReadback、shader/资源负例、VS2017/v141 双配置受影响目标，以及快速流送/规模玩法 Q1；只有后台或持久化变化才触发 Q3。 |

## 里程碑定义

| 里程碑 | 当前状态 | 标准 |
| ------ | -------- | ---- |
| M1 稳定 demo | 已完成 | 区块加载、方块修改和资源诊断已完成；Windows 已验收，macOS Debug/Release 原生门禁已通过。 |
| M2 可扩展体素核心 | 已完成 | dirty queue、halo cache、基础 greedy meshing、方块 metadata 完成。 |
| M3 可保存沙盒 | 已完成 | world seed、chunk 存档、基础光照、decorator 地形生成完成。 |
| M4 内容扩展阶段 | 已完成当前范围 | 方块 shape 资源化、BlockBehavior、演员、洞穴和跨区块结构完成。 |
| M5 可玩与发布探索 | 实现完成，待人工验收 | D/R/X 当前范围均已实现；Physical Input v2 后才能按实际覆盖关闭余下 `Verify`，脚本化 mod、完整编辑器和多人网络仍未立项。 |
| M6 可持续游玩与可靠发布 | 自动化闭环，待人工验收 | K1-K4、G1-G6、N1-N6、H1-H3、Q1-Q3 已完成；nominal/stress 正式长稳双 PASS，剩余输入与产品体验分开记录。 |
| M7 Beta 单机游戏闭环 | 工程完成，待人工验收 | 首次进入、视觉/HUD、胜利、战斗、探索结构、资源经济、难度/胜利后事件、双语、本地化音频与 BETA-RC 工程证据均已关闭；RC0 已被 BETA-RC 取代，Physical Input v2 与产品体验验收独立延期。 |
| M8 视觉质量升级 | Windows 工程完成，macOS/产品体验 Verify | V10A 顶点光照/AO、V10B1-B3 材质/生态表现、V10C 大气云层、V10D 可选阴影、V10E 轻量后处理与 VISUAL-RC Windows 工程封板已完成；后续产品方向评审也已完成。 |
| M9 可玩性与操作体验 | P11-0/P11A/P11B/P11-1/P11C/P11D/P11-2/P11E/P11F Engineering Done | 可制造光源、输入动作仲裁、动作反馈、最小建造集、并行机会、配方发现、独特探索奖励、terrain v4 山地/洞口、敌人表现/路标共鸣与 PLAYABILITY-RC 自动工程封板已完成；真人/Physical Input v2/产品体验/macOS 保留 Deferred/Verify 清单。 |

## 与 MiniGame 参考文档的关系

`docs/minigame-reference.md` 负责回答“MiniGame 有哪些架构点值得参考”。本文档负责回答
“HelloMine3D 接下来按什么顺序做”。两者的关系如下：

| MiniGame 参考点 | 本项目落地阶段 |
| --------------- | -------------- |
| package/mount 资源层 | `W3` 已完成 manifest；第 7 阶段由 `X1-X3` 落地只读目录包和冻结的有效资源视图。 |
| block id + metadata | 第 3 阶段落地。 |
| BlockLight 4+4 bit | 第 4 阶段落地。 |
| lazy section/dirty flags | 第 0-2 阶段逐步落地。 |
| 18x18x18 halo cache | 第 2 阶段落地。 |
| greedy/pane meshing | 第 2 阶段落地。 |
| ecosystem/decorator | 第 4 阶段落地。 |
| UI XML/Lua MVC | `D2` 继续使用本项目的 Ogre/ImGui 边界；暂不引入 Lua，只参考状态和视图分离。 |
| Task/GameMode/胜负阶段 | N7 只用作防耦合参考；结局状态独立于目标耗尽、UI、脚本和应用阶段。 |
| AI/投射物 | N8 只提取显式状态、生命周期、容量和调试信息；不引入行为树、Lua、Boss 巨类或可保存投射物。 |
| EcosysUnit 结构 | N9 继续使用本项目 cell hash/`StructureBuilder`；拒绝 `std::rand`、同步邻区块加载和结构 Manager。 |
| Craft/Furnace | N10 保持精确配方和三槽熔炉；新增经济可达性/循环校验，不复制材料组与复杂炉体。 |
| StringDefCsv/OgreSound | N7A/N12 使用语义化文本 key、采样音效和单通道音乐；不复制数字 ID、Lua、FMOD 或多通道 DSP。 |
| ObjectEditor/UIEditor | 暂不做完整编辑器，先做资源校验工具。 |
| RakNet/多人 | 第 6 阶段以后单独设计。 |
