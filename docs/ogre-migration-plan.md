# Ogre 迁移方案

本文档记录把 HelloMine3D 的底层渲染从 `SFML + 裸 OpenGL` 迁移到 `E:\Workspace\HelloOgre3D\src\Engine`
（Ogre 1.10）的完整方案。本文只做规划，不含实施。

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
| `World/` 对 SFML | `sf::Vector` 43 处（纯值类型）、`sf::Mouse` 5 处、`sf::Clock` 2 处、`sf::Keyboard` 1 处 |
| GL 类型外泄 | 仅 `ChunkMesh.h`、`ChunkMeshBuilder.h`（`GLfloat` 即 `float`，`GLuint` 即 `unsigned int`） |
| 隐式图形依赖 | `BlockDatabase` 构造时创建 `TextureAtlas`，导致数据层依赖 GL 上下文 |

`World/`、`Actor/`、`Item/`、`Player/`、`Sandbox/` 约 6500 行**不需要改动**。

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

这一步的价值超出迁移本身：当前 `HelloMine3DWorldRuntimeSmoke` 必须创建离屏 `sf::Context`
才能跑，正是因为这条隐式依赖。解耦后 headless 测试完全不需要图形环境。

### D6 整数向量类型

`sf::Vector2i/3i` 替换为 `glm::ivec2/ivec3`（glm 已 vendored）。
Ogre 1.10 没有整数向量类型，不要为此自造。

### D7 迁移期共存策略

E1–E4 期间新旧两套渲染并存，用编译开关切换，保证任何一个阶段结束时项目都能构建运行。
E5 才删除旧代码。

## 分阶段计划

阶段编号用 `E`（Engine），避免与 `docs/todolist.md` 中已有的 `M`（Mesh Pipeline）混淆。
按每周 1–2 天估算，总计约 7–11 周。

### E0 解耦准备（1 周）

**不碰渲染，独立有价值，且与是否迁移无关。**

| 任务 | 验收 |
| ---- | ---- |
| `sf::Vector2i/3i` → `glm::ivec2/ivec3` | `World/` 内不再出现 `sf::Vector` |
| `ChunkMesh.h` / `ChunkMeshBuilder.h` 的 `GLfloat`/`GLuint` → `float`/`std::uint32_t` | 非渲染层无 GL 类型 |
| 纹理图集创建移出 `BlockDatabase`（见 D5） | `HelloMine3DWorldRuntimeSmoke` 去掉 `sf::Context` 后仍通过 |
| `World/` 中 `sf::Mouse`/`sf::Clock`/`sf::Keyboard` 共 8 处收敛到输入/计时边界 | `World/` 不再直接 include SFML |

**阶段目标：`World/` 对 SFML 和 OpenGL 零依赖。**

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

### E2 Ogre 引导与空场景（1–2 周）

| 任务 | 验收 |
| ---- | ---- |
| 仿 `ClientManager` 建立 `Root`/`RenderWindow`/`SceneManager`/`Camera`/`FrameListener` | 能开窗 |
| `bin/Mine.cfg` + `bin/MineResources.cfg`，把 `media/` 挂入资源组 | 资源可加载 |
| OIS 接管键鼠，`PlayerController` 改用 OIS | WASD + 鼠标视角可用 |
| 启用 Ogre 内置天空盒（D4） | 天空正常 |

**阶段目标：Ogre 窗口 + 天空盒 + 自由飞行相机，世界尚未渲染。**

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

### E4 其余渲染路径（1–2 周）

| 任务 | 验收 |
| ---- | ---- |
| water / flora 各自 material + render queue group | 水面与植被正确 |
| 玩家 HUD 接回 | — |
| ImGui 调试面板接回（见风险 R3） | 调试数据可见 |
| `Diagnostics/RuntimeRenderCapture` 改用 `RenderWindow::writeContentsToFile` | 截图 smoke 通过 |
| `Diagnostics/RuntimePerformanceCapture` 接 Ogre `FrameListener` | perf CSV 正常 |

### E5 清理（1 周）

| 任务 | 验收 |
| ---- | ---- |
| 删除 SFML、glad、`Shaders/`、`Texture/`、`GL/`、旧 `Renderer/` | 无残留引用 |
| 删除编译开关与共存代码（D7） | 单一渲染路径 |
| 更新 `README.md`、`docs/runtime-validation.md`、构建与验证命令 | 文档与实际一致 |
| 全套验证 + 新性能基线归档 | 5 个测试 + 2 个 smoke 全绿 |

## 风险登记

| 编号 | 风险 | 影响 | 应对 |
| ---- | ---- | ---- | ---- |
| R1 | `staticruntime` 不一致（HelloOgre3D 为 `On`，本项目为默认动态） | 直接链接失败 | E1 阶段第一件事就统一，不要拖到 E3 |
| R2 | freetype 重复：现有 SFML 构建自带一份，Ogre ThirdParty 也有一份 | 符号冲突 | E1 阶段决定保留哪一份；SFML 移除后自然消解 |
| R3 | ImGui 在 Ogre 下的接入方式未验证 | 调试面板可能暂时不可用 | 首选 `imgui_impl_opengl3` 挂进 GL3Plus 上下文；退路是暂时只保留日志与 perf CSV，S7.1 顺延 |
| R4 | `characterset` MBCS 与本项目默认不一致 | 字符串 API 行为差异 | E1 统一 |
| R5 | Ogre 1.10 有 1 个文件使用 C++17 已移除的 `bind1st`/`mem_fun` 系列 | 编译失败 | 单文件降标准或小改。已确认无 `auto_ptr`、`random_shuffle`、`register`，整体 C++17 兼容性良好 |
| R6 | Ogre 场景图在 2000+ Renderable 量级的每帧开销 | 帧时间回退 | E3 结束必须做 perf baseline 对比；若回退明显，考虑合并 section 或降低 Renderable 粒度 |
| R7 | 构建时间与产物体积上升 | 迭代变慢 | 接受；必要时用 `/MP` 与分组构建缓解 |
| R8 | macOS 路径未验证 | 跨平台目标受阻 | HelloOgre3D 已有 `xcode.sh` 与 macOS filter 可抄；仍建议 E5 后单独验证 |

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
