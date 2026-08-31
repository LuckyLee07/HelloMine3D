# Ogre 迁移方案

本文档记录把 HelloMine3D 的底层渲染从 `SFML + 裸 OpenGL` 迁移到 `E:\Workspace\HelloOgre3D\src\Engine`
（Ogre 1.10）的完整方案，并同步记录各阶段的实施状态。

## 背景与决策

### 为什么迁移

| 理由 | 说明 |
| ---- | ---- |
| 与 HelloOgre3D 对齐 | 两个项目共用同一套 premake 布局和引擎，工程定义可直接复用。 |
| 参考项目的熟悉度 | 长期在 `F:\env1_trunk` 上工作，其体素实现（`sandboxCore/worldMesh`）可以直接对照借鉴。对个人项目而言，"看得懂的参考实现"比技术纯度更能决定推进速度。 |
| 体素方案已被验证 | `env1_trunk` 是 Ogre 上的商业体素客户端，证明"Ogre 当渲染底座 + 自写区块 Renderable"这条路走得通。 |
| 顺带获得能力 | 天空盒、粒子、模型/骨骼动画、场景剔除等从零写很贵的东西，Ogre 自带。 |

### 已知代价（决策时已接受）

| 代价 | 说明 |
| ---- | ---- |
| 重写渲染层 | 约 1300–1500 行，占第一方代码 13%。 |
| 构建规模上升 | 引擎侧新增约 1500 个文件，Debug 全量构建时间和产物体积明显增加。 |
| 顶点管线隔一层 | greedy meshing 和顶点压缩要透过 `HardwareVertexBuffer` 做。 |
| 玩法进度暂停 | 迁移期间没有可玩性产出。 |

### 一个必须澄清的事实

`F:\env1_trunk\client\miniEngine` 与 HelloOgre3D 的 Ogre **不是同一个 Ogre 的不同版本**，而是同源的两个分支：

| | env1_trunk `miniEngine/OgreMain` | HelloOgre3D `Engine/ogre3d` |
| --- | --- | --- |
| 文件数 | 270 | 502 |
| 命名空间 | `MINIW` | `Ogre` |
| 版本宏 | 已剥除 | 1.10.0 "Xalafu" |
| 游戏耦合 | FairyGUI、`IWORLD_REALTIME_SHADOW`、`MINI_NEW_UI`、`IWORLD_FUNC` 直接写在 `OgrePrerequisites.h` | 无 |

结论：**概念和架构可以对照参考，代码不能共用**。`MINIW::SceneNode` 与 `Ogre::SceneNode` 在编译器眼里没有关系。
本方案采用 HelloOgre3D 的原版 Ogre 1.10，不采用 env1_trunk 的 fork。

## 事实基线

以下数据于 2026-08-07 实测，作为方案的依据。

### HelloMine3D 当前渲染耦合

| 模块 | 规模 | 迁移后处置 |
| ---- | ---- | ---------- |
| `Renderer/` | 14 文件 / 603 行 | 重写 |
| `Shaders/` | 14 文件 / 316 行 | 删除，转 Ogre material |
| `Texture/` | 6 文件 / 209 行 | 删除，转 `TextureManager` |
| `GL/` | 4 文件 / 185 行 | 删除 |
| `World/Chunk/ChunkMesh.*` | — | 重写为 Ogre Renderable |
| `Main.cpp` 窗口/上下文 | — | 换 Ogre `Root`/`RenderWindow` |
| `Debug/`（imgui-SFML） | 2 文件 | 见风险 R3 |

逻辑层耦合很浅，这是本次迁移可控的关键：

| 位置 | 耦合内容 |
| ---- | -------- |
| `World/` 对 SFML | E0 已清零；输入设备收敛到 `SandboxRuntime` 边界，整数坐标改用 `glm::ivec2/ivec3` |
| GL 类型外泄 | E0 已清零；`ChunkMesh.h`、`ChunkMeshBuilder.h` 的 CPU 数据改用 `float`/`std::uint32_t` |
| 隐式图形依赖 | E0 已解除；`TextureAtlas` 由 `RenderMaster` 创建，数据层无需 GL 上下文 |

除 E0 的边界解耦外，`World/`、`Actor/`、`Item/`、`Player/`、`Sandbox/`
约 6500 行不需要随渲染后端重写。

### 当前顶点格式

```
location 0: vec3  inVertexPosition
location 1: vec2  inTextureCoord
location 2: float inCardinalLight
```

`inCardinalLight` 目前填的是按面方向硬编码的常量（`ChunkMeshBuilder.cpp`）：
顶面 1.0、X 侧 0.8、Z 侧 0.6、底面 0.4。后续光照工作要替换的正是这个值。

### 当前运行时规模

来自 `bin/perf_baseline_20260807190255313-41064`（VS2022 x64 Debug，视距 8）：

| 指标 | 值 |
| ---- | -- |
| 已加载区块 | 289 |
| section 总数 | 2013 |
| GPU 已缓冲 section | 447 |
| 累计 mesh 重建 | 449 |
| `frame_p95_ms` | 16.430 |

**迁移后必须用同一条命令复测并对比**，这是判断 Ogre 场景图开销是否可接受的唯一依据。

### HelloOgre3D 工程结构

与 HelloMine3D 布局完全一致（`premake/premake.lua` + `vs2022.bat` + `bin/` + `media/` + `tools/premake`），
工程定义可整段搬用。

| 分组 | 内容 |
| ---- | ---- |
| `Engine` | `ogre3d`、`ogre3d_glsupport`、`ogre3d_opengl`、`ogre3d_gl3plus`、`ogre3d_direct3d9`、`ogre3d_particlefx`、`ogre3d_procedural` |
| `Engine/ThirdParty` | `freeimage`、`libjpeg`、`libopenjpeg`、`libpng`、`libraw`、`libtiff4`、`openexr`、`freetype`、`zlib`、`zzip` |
| `External` | `ois` 1.5、`lua`/`tolua`/`luasocket`、`bullet_*` ×3、`ogre3d_gorilla`、`opensteer`、`recast`/`detour`、`tracy`、`fairygui`（可选） |

引导链路：`client/main.cpp` → `DemoHelloWorld::Run()` → `ClientManager`（`Fancy::Singleton`），
后者持有 `Ogre::Root` / `RenderWindow` / `SceneManager` / `Camera` / `FrameListener` / `InputManager`。
资源配置为 `bin/Sandbox.cfg`、`bin/Sandbox_d.cfg`、`bin/SandboxResources.cfg`、`bin/SandboxResources_d.cfg`。

关键工程设置差异（必须统一，否则链接失败）：

| 设置 | HelloOgre3D | HelloMine3D 当前 |
| ---- | ----------- | ---------------- |
| `staticruntime` | `On` | 默认（动态） |
| `characterset` | `MBCS` | 默认 |
| `platforms` | `x86` + `x64` | 仅 `x64` |
| `cppdialect` | 未设置（走 MSVC 默认） | `C++17` |

## 目标与非目标

### 目标

1. 渲染底座换成 Ogre 1.10，游戏逻辑层（`World`/`Actor`/`Item`/`Player`/`Sandbox`）保持不变。
2. 渲染表现不回退：`tools/run_render_capture.ps1` 截图仍显示正确地形、水面、植被。
3. 性能不明显回退：`frame_p95_ms` 与迁移前基线可比。
4. 现有 5 个测试目标继续通过。
5. 概念结构与 `env1_trunk` 对齐，便于对照参考。

### 非目标

| 不做 | 原因 |
| ---- | ---- |
| 引入 Lua / tolua | 本项目不做脚本化扩展。 |
| 引入 Bullet | 已有自研 AABB 碰撞，够用。 |
| 引入 Recast/Detour、OpenSteer | 寻路与群体 AI 不在近期规划。 |
| 引入 FairyGUI | UI 方案未定，不提前绑定。 |
| 使用 D3D9 后端 | 已过时，且与 macOS 目标冲突。 |
| 复用 env1_trunk 的引擎代码 | 命名空间与 API 已分叉，无法共用。 |

## 最小依赖子集

初期只引入以下工程，其余按需再加：

| 必需 | 用途 |
| ---- | ---- |
| `ogre3d` | 引擎核心 |
| `ogre3d_glsupport` | GL 平台支持层 |
| `ogre3d_gl3plus` | GL3+ 渲染后端（跨平台，对应现有 GL 3.3/4.1） |
| `freeimage` + `libjpeg` + `libpng` + `libopenjpeg` + `libraw` + `libtiff4` + `openexr` | 纹理加载（freeimage 的必需依赖链） |
| `freetype` | 字体 |
| `zlib`、`zzip` | 压缩与资源包读取 |
| `ois` | 输入，替代 SFML 键鼠 |

| 延后 | 触发条件 |
| ---- | -------- |
| `ogre3d_particlefx` | 做粒子特效时 |
| `ogre3d_opengl` | 需要兼容老 GL 设备时 |
| `ogre3d_gorilla` | 确定用它做游戏 UI 时 |
| `tracy` | 做性能优化时（`tools/tracy-viewer` 已就位） |

2026-08-16：该触发条件已满足。Tracy 0.13.1 现以可选静态库接入；默认构建关闭，
`--with-tracy` 或 `HELLOMINE3D_ENABLE_TRACY=1` 才启用按需采集，既有 CSV 性能门禁仍是
可比较回归的正式证据。

## 关键设计决策

### D1 区块如何进入 Ogre

采用 **自定义 `Ogre::MovableObject` + `Ogre::Renderable`，自持 `HardwareVertexBuffer`**，
即 `ChunkSectionRenderable`。

理由：`Ogre::ManualObject` 每次重建开销大，而本项目 13 秒内有 449 次 mesh 重建；
`StaticGeometry` 不适用于动态几何。env1_trunk 的 `sandboxCore/worldMesh` 正是这个做法，可直接对照。

### D2 剔除交给 Ogre

每个 `ChunkSectionRenderable` 设置自身 AABB，由 Ogre 场景图完成视锥剔除，
删除现有 `Maths/Frustum` 在渲染路径上的调用。

顺带解决现存问题：`World/WorldRender.cpp` 目前在渲染循环内持有主锁遍历整个 chunk map 做卸载，
迁移时把卸载判定移出渲染路径。

### D3 着色器转 material

现有 5 组 GLSL（Chunk / Water / Flora / Skybox / Basic）转为 Ogre `.material` 脚本 + GLSL program。
矩阵 uniform 改用 `param_named_auto`（如 `viewproj_matrix`）。
图集材质必须设 `filtering none`，否则体素像素风会被插值糊掉。

### D4 天空盒改用内置

Ogre 自带 `SceneManager::setSkyBox`，可删除 `Renderer/SkyboxRenderer.*` 和 `Texture/CubeTexture.*`。

### D5 数据层与图形解耦

把纹理图集的创建从 `BlockDatabase` 构造函数中剥离，`BlockDefinition` 只保留图集坐标。

这一步的价值超出迁移本身。该项已于 2026-08-09 提前完成：纹理对象归 `RenderMaster`
所有，网格侧只使用纯数据的图集坐标计算，`HelloMine3DWorldRuntimeSmoke` 已移除离屏
`sf::Context`，headless 测试完全不需要图形环境。

### D6 整数向量类型

`sf::Vector2i/3i` 替换为 `glm::ivec2/ivec3`（glm 已 vendored）。
Ogre 1.10 没有整数向量类型，不要为此自造。

### D7 迁移期共存策略

E1–E4 期间新旧两套渲染并存，用编译开关切换，保证任何一个阶段结束时项目都能构建运行。
E5 才删除旧代码。

## 分阶段计划

阶段编号用 `E`（Engine），避免与 `docs/current/todolist.md` 中已有的 `M`（Mesh Pipeline）混淆。
按每周 1–2 天估算，总计约 7–11 周。

### E0 解耦准备（1 周）

**不碰渲染，独立有价值，且与是否迁移无关。**

| 任务 | 验收 |
| ---- | ---- |
| `sf::Vector2i/3i` → `glm::ivec2/ivec3` | `World/` 内不再出现 `sf::Vector` |
| `ChunkMesh.h` / `ChunkMeshBuilder.h` 的 `GLfloat`/`GLuint` → `float`/`std::uint32_t` | 非渲染层无 GL 类型 |
| 纹理图集创建移出 `BlockDatabase`（见 D5，已完成） | `HelloMine3DWorldRuntimeSmoke` 去掉 `sf::Context` 后 114/114 通过；`E0/texture-coordinates-*` 验证 UV 不变 |
| `World/` 中 `sf::Mouse`/`sf::Clock`/`sf::Keyboard` 共 8 处收敛到输入/计时边界 | `World/` 不再直接 include SFML |

**阶段目标：`World/` 对 SFML 和 OpenGL 零依赖。**

状态：2026-08-09 已完成。静态扫描无图形 API 直接引用，Debug/Release
全量构建与五个测试目标全部通过，运行时 smoke 为 114/114。

可回退性：完全独立，即使最终放弃迁移也应保留。

### E1 Ogre 进构建（1–2 周）

| 任务 | 验收 |
| ---- | ---- |
| 复制 `src/Engine`（最小子集）到本项目 | 目录就位 |
| 移植 premake 的 `Engine` / `Engine/ThirdParty` 分组 | `vs2022.bat` 生成成功 |
| 统一 `staticruntime`、`characterset`、`platforms`、`cppdialect` | 见风险 R1/R4 |
| 处理 freetype 重复（现有 SFML 构建自带一份） | 见风险 R2 |
| 引入 `ois` | 静态库编过 |

**阶段目标：Ogre 静态库编译通过，游戏仍是 SFML 版本且行为不变。**

验证：全套 5 个测试 + render capture + perf baseline 全部照常通过。

状态：2026-08-12 已完成源码、构建与硬件图形验收。项目只导入 Ogre 核心、
GLSupport、GL3Plus、FreeImage 完整依赖链、独立的 Ogre FreeType、zlib/zzip 和 OIS 的
头文件/源码，合计 1868 个文件、约 29.57 MiB；OIS demo 与生成文档未导入。构建统一为
`x64 + C++17 + staticruntime On + MBCS`，Ogre 的 FreeType 工程命名为 `ogre_freetype`，
避免与迁移期 SFML 内置 FreeType 混淆。

VS2022 Debug/Release 全量构建与 5 个测试目标全部通过，运行时 smoke 为 239/239。
为兼容 C++17，仅局部迁移了 LibRaw 的 `auto_ptr` 所有权、Ogre 的现代 MSVC 哈希分支和
已移除的 STL 函数对象适配器；FreeImage 因历史源码编码保持原文件，使用 MSVC 兼容宏
`_HAS_AUTO_PTR_ETC=1`。GTX 1050 Ti / OpenGL 4.6 已完成地形、透明层、UI、演员、矿石和
天空盒截图；L4 的 10 秒基线记录 60.10 FPS、17.54 ms frame P95，且无超过 33 ms 的帧。

### E2 Ogre 引导与空场景（1–2 周）

| 任务 | 验收 |
| ---- | ---- |
| 仿 `ClientManager` 建立 `Root`/`RenderWindow`/`SceneManager`/`Camera`/`FrameListener` | 能开窗 |
| `bin/Mine.cfg` + `bin/MineResources.cfg`，把 `media/` 挂入资源组 | 资源可加载 |
| OIS 接管键鼠，`PlayerController` 改用 OIS | WASD + 鼠标视角可用 |
| 启用 Ogre 内置天空盒（D4） | 天空正常 |

**阶段目标：Ogre 窗口 + 天空盒 + 自由飞行相机，世界尚未渲染。**

状态：2026-08-12 已完成代码和硬件图形验收。引导程序现由唯一的
`HelloMine3D.exe` 客户端承载，负责
`Root`、GL3Plus 静态插件、`RenderWindow`、`SceneManager`、`Camera`、`FrameListener`、
OIS 键鼠输入和 Ogre 内置天空盒。`Mine.cfg` 与 `MineResources.cfg` 已纳入版本控制，
资源组挂载 `media/ogre` 和 `media/textures`。自由相机支持 WASD、空格/Ctrl 垂直移动、
鼠标视角和 Escape 退出，并提供按帧自动退出的验证入口。

VS2022 Debug/Release 全量构建与五个测试目标全部通过，运行时 smoke 为 239/239；
无窗口校验模式在两个配置下均成功注册 GL3Plus、两个资源目录和 OIS。硬件运行时创建并
捕获 OIS 键鼠设备，V2 对设备采集后的 `PlayerInputState` 控制器边界做确定性验证。
`docs/screenshots/validation-skybox-panel-outline.png` 证明六面云层天空盒正常渲染；资源日志
报告 `sky.png` 从六张图片加载为 cube texture。

### E3 区块渲染迁移（2–3 周）— 核心阶段

| 任务 | 验收 |
| ---- | ---- |
| 实现 `ChunkSectionRenderable`（D1） | 地形可见 |
| `ChunkMeshBuilder` 输出对接 `HardwareVertexBuffer`（E0 已解耦，无需改生成逻辑） | mesh 数据正确 |
| Chunk 着色器转 material（D3） | 纹理与光照系数正确 |
| 图集走 `TextureManager`，`filtering none` | 无插值糊化 |
| 每 section 设 AABB，剔除交给 Ogre（D2） | 视锥外不绘制 |
| 卸载判定移出渲染路径（D2） | 主锁不再在渲染循环内长时间持有 |

**参考实现：`F:\env1_trunk\client\miniSandbox\sandboxCore\worldMesh`。**

验证：render capture 显示正确地形；perf baseline 与迁移前基线对比，重点看
`frame_p95_ms`、`render_p95_ms`、`last_gpu_buffered_sections`。

状态：2026-08-12 已完成代码、硬件图形与性能验收。新增
`ChunkSectionRenderable`，基于 Ogre `SimpleRenderable` 实现自定义
`MovableObject + Renderable`，每个实体 section 自持交错的位置/图集 UV/重复 UV/组合光照顶点缓冲和
32 位索引缓冲。CPU 顶点转换为 section 局部坐标，场景节点承担世界平移，每个 section
使用 `0..CHUNK_SIZE` 的局部 AABB，由 Ogre 场景图负责剔除。原 `ChunkMeshBuilder` 未改写，
只新增只读 CPU 数据出口；地形 GLSL 已转为 Ogre program/material，图集
`DefaultPack.png` 由资源组加载并显式设置 `filtering none`。E5 已删除旧 SFML 客户端。

无窗口校验使用固定种子和固定玩家位置生成真实世界，并验证位置、图集/重复 UV、组合光照、
索引范围以及 section 局部边界。两套配置的全量构建、五个测试目标和 239/239 运行时 smoke
全部通过。硬件截图确认地形、最近邻图集和块级重复纹理，L4 性能基线同时验证逐帧 section
上传和场景图路径。

### E4 其余渲染路径（1–2 周）

| 任务 | 验收 |
| ---- | ---- |
| water / flora 各自 material + render queue group | 水面与植被正确 |
| 玩家 HUD 接回 | — |
| ImGui 调试面板接回（见风险 R3） | 调试数据可见 |
| `Diagnostics/RuntimeRenderCapture` 改用 `RenderWindow::writeContentsToFile` | 截图 smoke 通过 |
| `Diagnostics/RuntimePerformanceCapture` 接 Ogre `FrameListener` | perf CSV 正常 |

进展：2026-08-12 已完成 water / flora 材质、渲染队列和硬件外观验收。两者复用 E3 的
`ChunkSectionRenderable` 缓冲和 section AABB；water 使用透明混合、关闭深度写入并进入
队列 80，flora 保持深度写入并进入队列 60。两套顶点程序保留旧路径的正弦水面起伏与
植被摆动，时间由 Ogre `time_0_x` 自动常量提供；图集继续使用 `filtering none`。

固定种子 `20260809`、位置 `264 96 8` 的无窗口验证在 Debug/Release 下覆盖实体、玻璃、
水体和植被 section，四类网格的 UV/光照/索引校验全部通过。全量构建和 239/239 回归通过；
L4 硬件截图确认透明墙、叶片和交叉植被的材质与队列关系。

截图与性能诊断现已接入 Ogre：截图按既有环境变量调度，并通过
`RenderWindow::writeContentsToFile` 写出；固定步进与逐帧耗时由 `FrameListener` 送入
`RuntimePerformanceCapture`；两条验证脚本现直接使用唯一的 `HelloMine3D.exe` 客户端。无窗口模式
已在 Debug/Release 验证截图目标的配置解析，全量构建和 239/239 回归继续通过。
GTX 1050 Ti / OpenGL 4.6 已生成并独立解码实际 PNG；10 秒性能 CSV 同时通过帧率、帧预算和
20 Hz 固定步进门槛。

HUD 与 ImGui 也已迁移：`OgreUserInterface` 将 OIS 键鼠事件映射到 ImGui，常驻绘制准星和
5 格快捷栏，F1 控制玩家/世界调试面板，并在场景渲染队列结束后通过 OpenGL3 后端提交。
输入采集已移到 `frameStarted`，UI 捕获输入时会抑制相机操作；按帧退出则在 `frameEnded`
收尾，保证最后一帧仍进入截图与性能采集。Debug/Release 无窗口校验分别验证调试面板开启
和关闭配置，HUD 均报告 5 个槽位及合法选中项；全量构建和 239/239 回归通过。
`docs/screenshots/validation-skybox-panel-outline.png` 进一步确认准星、快捷栏、黄色选框和实时
调试面板均在硬件渲染路径中可见。

### E5 清理（1 周）

| 任务 | 验收 |
| ---- | ---- |
| 删除 SFML、glad、`Shaders/`、`Texture/`、`GL/`、旧 `Renderer/` | 无残留引用 |
| 删除编译开关与共存代码（D7） | 单一渲染路径 |
| 更新 `README.md`、`docs/current/runtime-validation.md`、构建与验证命令 | 文档与实际一致 |
| 全套验证 + 新性能基线归档 | 5 个测试 + 2 个 smoke 全绿 |

进展：2026-08-09 已完成单渲染路径切换。`HelloMine3D.exe` 直接运行 Ogre 客户端，旧
SFML/glad/Renderer/Shaders/Texture/GL/Input 外壳和并行引导目标均已移除；游戏逻辑收敛到
平台无关的 `SandboxRuntime`。Ogre 通过带方块版本号的 CPU 网格快照接收后台重建结果，
只有成功上传的当前版本才会标记为 GPU ready，挖掘、放置、卸载后的场景节点可逐帧同步。
方块选择反馈也迁移为 `OgreBlockOutline`。Premake、README、截图和性能脚本均只保留一个
客户端入口，Debug/Release 全量构建、五个测试和 239/239 运行时回归通过。新的 Ogre
PNG/CSV 硬件基线已在 GTX 1050 Ti / OpenGL 4.6 上归档，E5 完成。

## 风险登记

| 编号 | 风险 | 影响 | 应对 |
| ---- | ---- | ---- | ---- |
| R1 | `staticruntime` 不一致（HelloOgre3D 为 `On`，本项目为默认动态） | 直接链接失败 | E1 阶段第一件事就统一，不要拖到 E3 |
| R2 | freetype 重复：现有 SFML 构建自带一份，Ogre ThirdParty 也有一份 | 符号冲突 | E1 阶段决定保留哪一份；SFML 移除后自然消解 |
| R3 | ImGui 在 Ogre 下的接入方式未验证 | 调试面板可能暂时不可用 | 已使用 `imgui_impl_opengl3` 接入 GL3Plus，并由硬件截图关闭风险。 |
| R4 | `characterset` MBCS 与本项目默认不一致 | 字符串 API 行为差异 | E1 统一 |
| R5 | Ogre 1.10 有 1 个文件使用 C++17 已移除的 `bind1st`/`mem_fun` 系列 | 编译失败 | 单文件降标准或小改。已确认无 `auto_ptr`、`random_shuffle`、`register`，整体 C++17 兼容性良好 |
| R6 | Ogre 场景图在 2000+ Renderable 量级的每帧开销 | 帧时间回退 | E3 结束必须做 perf baseline 对比；若回退明显，考虑合并 section 或降低 Renderable 粒度 |
| R7 | 构建时间与产物体积上升 | 迭代变慢 | 接受；必要时用 `/MP` 与分组构建缓解 |
| R8 | 后续工程变更破坏 macOS Xcode 图 | 跨平台回归 | 风险已由原生门禁关闭：版本化合同固定当前 29 项工程清单，9 个正/负夹具及 PBX 分组/`ProjectRef` 检查拒绝陈旧、缺失、重复或多分组引用；`scripts/verify_xcode.sh` 随后完成 Debug/Release 构建、全部测试和真实窗口启动。 |

## 与玩法路线的先后关系

**建议在光照工作之前完成迁移。**

理由：光照需要改写 mesh 生成、顶点属性和着色器，而迁移改的是同一批代码。
两件事撞在一起做一次，比先做光照再迁移（等于把光照工作重做一遍）省得多。

推荐顺序：

```
E0 解耦准备
  └─ 阶段 0 让已有系统看得见（实体渲染 / 方块描边 / 矿物贴图）  ← 可插在迁移前后任意位置
E1–E5 Ogre 迁移
  └─ 光照与昼夜
      └─ 无限世界与性能（greedy meshing / halo cache）
          └─ 生存循环
```

注意"阶段 0 让已有系统看得见"中的实体渲染，如果在迁移前做，迁移时要重做一次；
如果在 E3 之后做，可以直接用 Ogre 的 `Entity`/`SceneNode`，成本更低。
**建议实体渲染推迟到 E3 之后**，迁移前只做方块描边和矿物贴图这类不涉及新渲染管线的项。

## 回退策略

| 阶段 | 回退方式 |
| ---- | -------- |
| E0 | 无需回退，独立有价值 |
| E1 | 移除 Engine 分组即可，游戏逻辑未变 |
| E2–E4 | 编译开关切回旧渲染路径（D7） |
| E5 | 已删除旧代码，回退需 revert；因此 E5 必须在 E4 全部验证通过后才做 |

每个阶段结束时项目都应可构建、可运行、可通过全套验证。任何阶段无法达成此条件时，
应先修复再进入下一阶段。

## 决策记录

| 日期 | 决策 | 备注 |
| ---- | ---- | ---- |
| 2026-08-07 | 采用 HelloOgre3D 的 Ogre 1.10 作为渲染底座 | 已评估替代方案（保留 SFML+GL 并只对齐概念边界），因参考项目熟悉度优势选择迁移 |
| 2026-08-07 | 不采用 env1_trunk 的 `MINIW` fork | 命名空间与 API 已分叉，无法共用代码 |
| 2026-08-07 | 迁移排在光照工作之前 | 避免同一批代码改两次 |
