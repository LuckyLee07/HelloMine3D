# HelloMine3D Iteration Plan

本文档保留 HelloMine3D 的长期架构方向和历史阶段划分。规划基于项目源码，
以及 `docs/minigame-reference.md` 中对 MiniGame 项目的参考分析。可执行任务、
当前状态和逐项验收证据以 `docs/todolist.md` 为唯一准绳；本文件中的远期候选
只有在被加入该清单后才成为当前迭代承诺。

总体原则：

1. 先修正确性和工程稳定性，再扩玩法。
2. 先把区块、资源、方块数据模型做稳，再做光照、存档、mod 和多人。
3. 保持 Windows/macOS 兼容作为长期约束，不引入会破坏跨平台边界的实现。
4. 从 MiniGame 借鉴架构方向，不直接照搬它的旧引擎、旧构建和重型 SDK。

## 当前项目状态

HelloMine3D 已具备这些基础：

| 能力 | 当前状态 |
| ---- | -------- |
| 构建系统 | Premake 已作为主构建入口，支持 Make/Xcode/Visual Studio 工程生成。 |
| 依赖管理 | 通过 `src/Engine` 与 `src/external` 本地源码构建 Ogre、FreeImage、FreeType、OIS、ImGui 和 GLM，不依赖 vcpkg 安装树。 |
| 运行资源 | `media/` 保存 shader、texture、block、shape、font 和基础 recipe；严格解析器与 `scripts/check_assets.sh` 负责引用和边界校验。 |
| 世界结构 | `World`/`ChunkManager` 已具备负坐标、固定 tick、版本化存档、玩家与演员持久化。 |
| 区块网格 | `ChunkMeshBuilder` 使用有界 dirty 队列、18x18x18 halo、opaque greedy meshing、光照分界和异步快照版本校验。 |
| 方块数据 | `BlockDefinition`、`BlockRenderInfo`、`BlockShape`、`BlockBehavior` 与 `ChunkBlock` metadata 已落地。 |
| 地形生成 | biome、洞穴、矿石、植物和跨区块结构均由确定性阶段生成，且不依赖相邻区块加载顺序。 |
| 渲染 | Ogre GL3Plus 是唯一渲染路径，已有 terrain/water/glass/flora/程序化天空、演员、选择描边、HUD 和材质程序。 |
| 调试 UI | Ogre/OIS + ImGui 已提供 HUD、调试面板、运行时统计、截图和性能采集入口。 |

当前剩余验收风险：

| 风险 | 说明 |
| ---- | ---- |
| 目标 Windows 更新门禁待复跑 | macOS 已验证版本化的 28 项 Xcode 工程图、Debug/Release 构建、测试和真实窗口启动；Windows 上次证据早于 G1/K1/K2 与当前工程图合同，需在目标机复跑更新后的 PowerShell 门禁。 |
| 输入和并发仍有平台型证据缺口 | OIS 到控制器的数据边界已自动验证，但真实键鼠仍需人工验收；后台加载器已有压力测试，但正式 ThreadSanitizer 证据要等待受支持主机。对应 `R3`、`R4`。 |
| 下一产品阶段已选定 | 第 8 阶段以“更像游戏”为主线，同时补齐存档管理、崩溃诊断和扩展性能预算。R3 仍是玩家功能的封板项；R4 继续等待受支持的 ThreadSanitizer 主机。多人、脚本化 mod、热更新和新渲染后端仍未立项。 |

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
| 第 7 阶段 | 实现完成，待人工验收 | D1-D6、R1-R2、R5、X1-X3 已实现并有自动/硬件证据；R3 完成后关闭 D2、D4、D6，R4 按主机条件延期 |
| 第 8 阶段 | 已启动，仍待 R3 封板 | `K1-K2` 世界目录/事务保存合同和 `G1` 配方注册表已完成，`Q1` 已固定六类性能契约；`K3-K4`、`H1-H3`、`Q2-Q3`、`G2-G6` 仍按依赖推进 |

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

开始条件：

1. 仍需由真人在目标 Release 构建上完成 `R3`，同时关闭 `D2`、`D4` 和 `D6`；
   不依赖输入的 K1-K2/Q1 合同与 G1 基础数据层已先行完成，但这不替代阶段封板。
2. `B3` 已于 2026-08-16 在 Apple M1 Pro 上通过完整 Xcode 原生门禁；保留
   `R4` 的延期事实，没有受支持的 TSan 配置时不得伪造数据竞争证据，但它不阻塞
   Windows 第 8 阶段。
3. 新增格式、指标和用户数据操作必须先定义版本、失败语义和兼容边界，再进入 UI。

| 任务线 | 正式任务 | 交付目标 | 阶段关闭条件 |
| ------ | -------- | -------- | ------------ |
| 长期存档 | `K1-K4` | 建立稳定世界目录、事务保存、有界备份、损坏恢复和世界管理 UI。 | 创建、保存、崩溃中断、恢复、重命名和可恢复删除均有自动与真人证据；任何失败不得覆盖最后一份已验证存档。 |
| 崩溃诊断 | `H1-H3` | Windows Release 本地生成 minidump 和脱敏上下文，建立符号化、受控崩溃和发行包验收。 | 受控崩溃可稳定生成并符号化至少一个项目栈帧；默认不联网、不自动上传，不把符号和开发路径混入发行包。 |
| 性能预算 | `Q1-Q3` | 把现有帧时间门禁扩展到冷启动、进入世界、保存/恢复、区块可见延迟和内容规模压力。 | 每个场景都有版本化身份、固定指标、已批准阈值和可比较/不可比较判定；超预算时门禁非零失败。 |
| 玩法成长 | `G1-G6` | 增加数据驱动配方、工作台、工具采集成长、暂停/设置、基础音频和新的综合垂直切片。 | 玩家能从创建世界开始完成采集、制作工具、战斗、保存、恢复和继续游戏；状态守恒、持久化、性能和输入证据全部通过。 |

建议执行批次：

| 批次 | 任务 | 原因和出口 |
| ---- | ---- | ---------- |
| 0：封板 | `R3` | 关闭上一阶段唯一的人机输入缺口，冻结新的输入/UI 基线。 |
| 1：合同先行 | `K1`、`H1`、`Q1` | K1 与 Q1 的本机可完成合同已落地；继续固定崩溃产物/隐私边界和目标 Windows 正式预算。 |
| 2：数据安全 | `K2-K3`、`Q2` | K2 事务保存及故障注入已完成；继续交付有界备份恢复和保存/启动分阶段耗时门禁。 |
| 3：玩家入口 | `K4`、`G1-G2` | G1 数据层已完成；继续用世界列表和工作台把“选择世界—采集—制作”接入正常玩家流程。 |
| 4：诊断与成长 | `H2-H3`、`G3` | 完成符号化发行链路，并加入工具等级、采集速度、耐久和掉落限制。 |
| 5：体验与规模 | `G4-G5`、`Q3` | 加入暂停/设置和基础音频，以快速移动和内容扩容场景验证性能预算。 |
| 6：综合验收 | `G6` | 从干净发行包走完整新玩家流程，并增加保存中断、恢复和受控崩溃后的继续游玩证据。 |

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
| Windows 双配置完整门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_build.ps1` |
| Linux/macOS Make 双配置门禁 | `bash scripts/verify_build.sh` |
| macOS Xcode project | `bash scripts/verify_xcode.sh` 先验证 9 个合同/工程图夹具及实际 28 项工程清单，再构建 Debug/Release、执行十项测试和客户端启动探针；陈旧/缺失工程、重复或多分组引用、手工目标顺序及第一方编译告警均会使门禁失败。 |
| 资源检查 | `bash scripts/check_assets.sh`，资产引用缺失时返回非零。 |
| 手动运行 | `bash scripts/run.sh release`，观察 chunk 加载、放置/破坏、退出是否正常。 |
| 性能回归比较 | `tools/compare_perf_baselines.ps1`；D/R5/X 中改变场景、资源或驻留规模的任务关闭前必须运行。 |
| 长时间稳定性 | `tools/run_world_soak.ps1`；区块、演员、存档或后台线程变更按固定种子动作表运行。 |
| 物理输入验收 | `docs/manual-input-acceptance-v1.md`；输入、窗口系统、容器 UI 或战斗交互变更后人工记录。 |
| 干净目录发布验收 | `tools/package_windows_release.ps1`；从隔离根目录完成 manifest 预检、validation-only、真实窗口和负向校验。 |
| 存档故障与恢复（第 8 阶段） | K2 故障注入已证明发布失败不会覆盖最后有效存档；K3 继续交付备份容量上限、损坏隔离和完整恢复 harness。 |
| 本地崩溃诊断（第 8 阶段） | H1-H3 交付受控崩溃、sidecar、符号化和干净包探针；默认禁止上传并检查隐私/符号泄漏。 |
| 扩展性能预算（第 8 阶段） | Q1-Q3 扩展采集和比较器；覆盖启动、世界进入、保存/恢复、区块可见延迟和内容规模。 |
| 新玩法综合验收（第 8 阶段） | G6 从干净包运行创建世界、制作工具、等级采集、战斗、保存失败恢复和继续游玩。 |

## 里程碑定义

| 里程碑 | 当前状态 | 标准 |
| ------ | -------- | ---- |
| M1 稳定 demo | 已完成 | 区块加载、方块修改和资源诊断已完成；Windows 已验收，macOS Debug/Release 原生门禁已通过。 |
| M2 可扩展体素核心 | 已完成 | dirty queue、halo cache、基础 greedy meshing、方块 metadata 完成。 |
| M3 可保存沙盒 | 已完成 | world seed、chunk 存档、基础光照、decorator 地形生成完成。 |
| M4 内容扩展阶段 | 已完成当前范围 | 方块 shape 资源化、BlockBehavior、演员、洞穴和跨区块结构完成。 |
| M5 可玩与发布探索 | 实现完成，待 R3 | D/R/X 当前范围均已实现；R3 人工记录后关闭余下 `Verify`，脚本化 mod、完整编辑器和多人网络仍未立项。 |
| M6 可持续游玩与可靠发布 | 已启动，待 R3 与后续任务 | K1-K2 与 G1 已完成，Q1 的可移植合同已落地；继续交付 K3-K4、H1-H3、Q2-Q3、G2-G6 及目标 Windows/真人证据。 |

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
| ObjectEditor/UIEditor | 暂不做完整编辑器，先做资源校验工具。 |
| RakNet/多人 | 第 6 阶段以后单独设计。 |
