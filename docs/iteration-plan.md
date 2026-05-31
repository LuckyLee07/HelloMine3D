# HelloMine3D Iteration Plan

本文档记录 HelloMine3D 后续迭代规划。规划基于当前项目源码状态，以及
`docs/minigame-reference.md` 中对 MiniGame 项目的参考分析。

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
| 依赖管理 | 支持 `src/external` 本地依赖，也预留 vcpkg/系统依赖路径。 |
| 运行资源 | `media/` 保存 shader、texture、block、font，`ResourcePaths` 负责路径解析。 |
| 世界结构 | 已有 `World`、`ChunkManager`、`Chunk`、`ChunkSection`。 |
| 区块网格 | 已有 `ChunkMeshBuilder`，按 solid/water/flora 拆 mesh。 |
| 方块数据 | 已有 `.block` 文件、`BlockDatabase`、`BlockDataHolder`。 |
| 地形生成 | 已有 biome、tree、structure 和 classic overworld generator。 |
| 渲染 | 已有 chunk/water/flora/skybox renderer 和 shader 层。 |
| 调试 UI | 已有 ImGui 集成基础。 |

当前主要风险：

| 风险 | 说明 |
| ---- | ---- |
| 区块 dirty 更新不完整 | 方块修改后的 mesh 更新链路尚未可靠启用。 |
| Section solid layer 计数不可信 | `Layer::update()` 没有基于旧方块和新方块做增量修正。 |
| 负坐标支持不明确 | 当前 chunk/block 映射对负坐标不安全，加载逻辑也 clamp 到 0。 |
| 后台加载线程边界较粗 | 线程直接读 camera 引用，后续容易出现同步和生命周期问题。 |
| 方块数据校验较弱 | `.block` 缺字段、坐标越界、id 不匹配等问题没有集中诊断。 |
| 资源系统仍是直读路径 | 适合当前规模，但不利于后续资源检查、资源包和 mod。 |
| 地形生成职责开始变重 | terrain、biome、植物、树等逻辑仍有混合趋势。 |

## 迭代阶段

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
| P2 | random tick 支持 | 植物生长、流体、火等只扫描需要 tick 的 section。 | tick 逻辑不会全世界暴力扫描。 |

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
| P0 | Windows/macOS 构建检查 | 明确 SFML/ImGui/GLM 布局、DLL/dylib/rpath。 | 新机器按 README 能构建运行。 |
| P1 | smoke build 脚本 | Make/Xcode/VS 至少有清晰构建命令。 | 每次结构改动后能快速验证构建。 |
| P1 | 资源打包规划 | 先 manifest，再资源包。 | 不影响当前 `media/` 直读流程。 |
| P2 | mod/资源包 | 支持覆盖资源和自定义 block/model。 | 等 block 数据模型稳定后再做。 |
| P2 | 多人网络 | 单独设计 server authority、chunk streaming、实体同步。 | 不在单机架构未稳定前启动。 |

## 不建议近期投入的方向

| 方向 | 原因 |
| ---- | ---- |
| 多人网络 | 会改变世界权威、事件同步、实体同步、存档和输入模型。 |
| Lua/脚本系统 | 当前方块和 UI 规模还不需要脚本化，会增加调试成本。 |
| 完整资源包热更新 | 当前资源规模不大，先做 manifest 和校验更实际。 |
| D3D/Vulkan 后端 | 当前目标是 Windows/macOS 兼容，SFML + OpenGL 足够支撑近期开发。 |
| 完整编辑器 | 先做资源校验和 block atlas 工具，避免工具链复杂度过早膨胀。 |
| 大规模引入 MiniGame 代码 | MiniGame 历史依赖重，适合参考架构，不适合迁移源码。 |

## 推荐执行顺序

建议按小闭环推进，每个闭环都应该能构建、运行、验证。

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
| Make debug build | `sh scripts/build.sh` |
| Make release build | `sh scripts/build.sh release` |
| macOS Xcode project | `sh scripts/premake.sh xcode4` 后用 Xcode 或 `xcodebuild` 编译。 |
| Windows VS project | `vs2022.bat` 生成工程，并在 Windows 上编译。 |
| 资源检查 | 后续新增 `scripts/check_assets.sh` 后纳入常规检查。 |
| 手动运行 | `sh scripts/run.sh release`，观察 chunk 加载、放置/破坏、退出是否正常。 |

## 里程碑定义

| 里程碑 | 标准 |
| ------ | ---- |
| M1 稳定 demo | 区块加载和方块修改可靠，资源错误可诊断，Windows/macOS 可构建运行。 |
| M2 可扩展体素核心 | dirty queue、halo cache、基础 greedy meshing、方块 metadata 完成。 |
| M3 可保存沙盒 | world seed、chunk 存档、基础光照、decorator 地形生成完成。 |
| M4 内容扩展阶段 | 方块 shape 资源化、BlockBehavior、更多方块/生物群系/结构完成。 |
| M5 产品化探索 | 资源包、mod、工具链、实体系统、多人网络按独立设计推进。 |

## 与 MiniGame 参考文档的关系

`docs/minigame-reference.md` 负责回答“MiniGame 有哪些架构点值得参考”。本文档负责回答
“HelloMine3D 接下来按什么顺序做”。两者的关系如下：

| MiniGame 参考点 | 本项目落地阶段 |
| --------------- | -------------- |
| package/mount 资源层 | 第 1 阶段先做 manifest，第 6 阶段再考虑资源包。 |
| block id + metadata | 第 3 阶段落地。 |
| BlockLight 4+4 bit | 第 4 阶段落地。 |
| lazy section/dirty flags | 第 0-2 阶段逐步落地。 |
| 18x18x18 halo cache | 第 2 阶段落地。 |
| greedy/pane meshing | 第 2 阶段落地。 |
| ecosystem/decorator | 第 4 阶段落地。 |
| UI XML/Lua MVC | 暂不落地，只参考调试 UI 分层。 |
| ObjectEditor/UIEditor | 暂不做完整编辑器，先做资源校验工具。 |
| RakNet/多人 | 第 6 阶段以后单独设计。 |

