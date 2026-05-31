# MiniGame Reference Notes

本文记录对本机 `/Users/lizi/Desktop/Workspace/MiniGame` 项目的结构分析，以及它对
MineCraft3D 后续迭代的可参考点。结论先行：MiniGame 更像一个成熟但历史包袱较重的
Ogre 时代商业体素客户端，适合作为架构和功能拆分参考，不适合直接复制源码、第三方库
或构建方式。

## 分析范围

本次主要查看了这些目录和文件：

| 路径 | 观察重点 |
| ---- | -------- |
| `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld` | 方块、区块、世界数据、地形生成、渲染网格、玩家、网络和游戏逻辑。 |
| `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/OgreMain` | 资源管理、文件包、UI、Lua、图片/压缩/网络等运行时基础设施。 |
| `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/RenderSystem_D3D9` | D3D9 渲染后端，说明其渲染层有后端拆分思路。 |
| `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/RenderSystem_OGL` | OpenGL 渲染后端，说明渲染后端不是完全绑定 D3D。 |
| `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/RakNet` | 网络库和多人通信基础设施。 |
| `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/ObjectEditor` | 对象/资源编辑工具。 |
| `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/UIEditor` | UI 编辑工具。 |
| `/Users/lizi/Desktop/Workspace/MiniGame/bin` | 运行时 exe/dll、配置、资源包、脚本、UI、CSV、模型和工具输出。 |

局部规模参考：

| 项 | 规模 |
| -- | ---- |
| `Miniw-Client` | 约 423 MB。 |
| `bin` | 约 1.6 GB。 |
| `Miniw-Client/iworld` C++ 头/源文件 | 约 1574 个。 |
| `bin/res` 资源文件 | 约 17187 个。 |

这不是一个小型示例项目，而是一个包含客户端、编辑器、资源管线、脚本、平台 SDK、网络
和大量历史依赖的完整工程。因此后续参考时要按模块取思想，避免把复杂度搬进当前项目。

## 总体判断

MiniGame 对 MineCraft3D 的最大价值不是构建脚本或第三方库，而是这些方面：

| 方向 | 参考价值 | 当前 MineCraft3D 状态 |
| ---- | -------- | --------------------- |
| 方块数据模型 | 证明 `block id + metadata + light` 的紧凑表示很有价值。 | 目前 `ChunkBlock` 主要存 block id，适合后续扩展 metadata/light。 |
| 区块/Section 管理 | 有懒分配、dirty flags、邻居关系、光照脏标记、mesh/physics/minimap 分离。 | 当前区块结构更轻量，适合逐步加入 dirty 标记和邻居缓存。 |
| 网格生成 | 使用邻居 halo 缓存和面片合并思路，避免每次重复跨 chunk 查询。 | 当前 `ChunkMeshBuilder` 是直观面剔除，可以先保留，再引入局部优化。 |
| 方块材质/行为 | `BlockMaterial` 把渲染、碰撞、放置、tick、事件等行为集中抽象。 | 当前 `BlockDataHolder` 偏渲染数据，行为仍比较分散。 |
| 资源系统 | 有 package/mount、zip/web/patch/mod 等资源层概念。 | 当前有 `media/` 和 `ResourcePaths`，后续可演进到资源索引和包挂载。 |
| 地形生成 | 用生态/装饰器单元拆分地貌、树、草、矿物、湖、地牢等。 | 当前生成逻辑较直接，适合按 biome/decorator 拆分。 |
| 工具链 | 有 ObjectEditor、UIEditor、资源转换工具、几何定义文件。 | 当前没有编辑器工具，后续可先做轻量资源校验和 block atlas 工具。 |
| 脚本/UI | UI 资源存在 XML + Lua + MVC 分层。 | 当前用 ImGui 调试界面即可，复杂 UI 可参考它的 View/Model/Controller 思路。 |
| 跨平台/后端 | 有 D3D9/OGL、Android、Apple、平台 SDK 目录。 | 当前重点是 Windows/macOS，参考其抽象边界，不参考旧 SDK 细节。 |
| 网络/多人 | RakNet 和大量 net/cs 代码说明多人系统是独立大模块。 | 当前不建议引入，多人化应作为远期独立里程碑。 |

## MiniGame 顶层结构

### `Miniw-Client/iworld`

这是 MiniGame 最值得看的目录，核心内容包括：

| 子目录 | 作用 | 对 MineCraft3D 的参考点 |
| ------ | ---- | ---------------------- |
| `worlddata` | `Block`、`BlockLight`、`Chunk`、`Section`、mesh/light/save 等世界数据。 | 后续区块存储、光照、dirty flags、邻居缓存、序列化设计的主要参考。 |
| `blocks` | `BlockMaterial`、`BlockMaterialMgr` 和各种方块行为。 | 方块从“数据表”升级到“数据 + 行为策略”的参考。 |
| `display` | 方块几何、顶点格式、显示相关数据。 | 非标准方块、紧凑顶点格式、模型/几何模板的参考。 |
| `terrgen` | 地形生成、生态系统、树、矿物、湖、地牢等生成单元。 | 把 terrain base 和 decorator 拆开的参考。 |
| `player` | 玩家行为、控制、交互。 | 当前暂时参考价值一般，等交互系统复杂后再看。 |
| `actors` | 实体/生物/动态对象。 | 未来实体系统、chunk 内 actor 管理的参考。 |
| `camera` | 相机系统。 | 当前 MineCraft3D 已有基础相机，参考价值较小。 |
| `ai` | AI 行为。 | 远期实体 AI 可参考，但不应现在引入。 |
| `net` / `cs` | 网络和客户端/服务器交互。 | 多人化前不引入。 |
| `mod` | 模组相关。 | 未来支持自定义方块/模型/资源包时参考。 |
| `utility` | 通用工具。 | 可按需查看，不建议整包迁移。 |

### `Miniw-Client/OgreMain`

这是运行时基础设施和历史第三方依赖集合，包含资源管理、文件系统、UI、Lua、图片格式、
压缩库、网络库等。值得关注的不是它的具体实现，而是模块边界：

| 模块 | 参考点 |
| ---- | ------ |
| `OgreResourceManager.*` | 资源管理器作为统一入口，避免业务代码直接拼路径。 |
| `OgrePackageFile.*` | 抽象 package 文件来源，普通目录、zip、web、patch 都可以是资源来源。 |
| `OgrePackageZipFile.*` | 资源包可压缩分发。 |
| `OgrePackageWebFile.*` | 资源可以来自远端或热更新系统。 |
| `OgreFileSystem.*` | 文件访问封装，便于跨平台和虚拟路径。 |
| `OgreModFileManager.*` | mod/自定义资源管理思路。 |
| `UILib` | UI 运行时的独立层。 |
| `Lua` / `ToLua` | 脚本桥接层，说明游戏逻辑可从 C++ 逐步外置。 |

MineCraft3D 当前不需要引入这种完整资源系统，但可以沿着 `ResourcePaths` 演进：

1. 先保持 `media/` 直读。
2. 增加一个资源索引/manifest，记录 texture、shader、block data。
3. 增加资源加载失败诊断和路径打印。
4. 再考虑资源包挂载、mod 和覆盖顺序。

### `RenderSystem_D3D9` / `RenderSystem_OGL`

这两个目录说明 MiniGame 把渲染后端和游戏逻辑做了拆分。对当前项目的意义是：

| 可参考点 | 说明 |
| -------- | ---- |
| 渲染后端隔离 | 游戏世界不应直接依赖具体 OpenGL 调用。 |
| 平台差异收口 | Windows/macOS 的上下文创建、扩展、路径和动态库处理应放在少数边界里。 |
| 不直接照搬 D3D9 | D3D9 已过时，当前 MineCraft3D 继续保留 SFML + OpenGL 更合适。 |

## 运行时资源模型

MiniGame 的 `/Users/lizi/Desktop/Workspace/MiniGame/bin/iworld.cfg` 里能看到典型资源挂载：

```xml
<Package name="default" path="res\" readonly="true" />
<Package name="root" path="" />
```

这说明它把运行时资源组织成多个 package，其中 `res` 是只读默认资源，`root` 可以覆盖或
放置运行时文件。结合 `OgrePackageFile`、`OgrePackageZipFile`、`OgrePackageWebFile`、
`OgrePackagePatch` 和 `OgreModFileManager`，可以抽象出一套资源层级：

| 层级 | 可能含义 | MineCraft3D 可借鉴方式 |
| ---- | -------- | ---------------------- |
| 默认资源 | 游戏随包资源。 | 继续使用 `media/`，但建立 manifest。 |
| 根目录覆盖 | 本地配置、缓存、用户生成内容。 | 把 `bin/` 中的运行时配置和用户数据分开。 |
| patch | 热更新或修复包。 | 短期不做，远期资源包可覆盖 manifest 项。 |
| zip package | 压缩分发资源。 | 等资源量增长后再做。 |
| web package | 远端下载资源。 | 当前不需要。 |
| mod package | 用户扩展资源。 | 远期自定义 block/model 时有价值。 |

当前项目可以先实现一个小的 `AssetRegistry`，不要直接实现完整 package 系统。建议字段：

| 字段 | 用途 |
| ---- | ---- |
| logical name | 例如 `textures.blocks`, `shader.chunk.vert`。 |
| relative path | 相对 `media/` 的真实路径。 |
| type | texture/shader/font/block-data/model。 |
| required | 缺失时是否直接失败。 |
| hash/version | 远期资源校验和热更新。 |

## 方块数据模型

MiniGame 的 `worlddata/block.h` 很有参考价值。它把一个方块压进 `unsigned short`：

| 数据 | 设计 |
| ---- | ---- |
| block id | 低 12 bit，最多 4096 种方块。 |
| block data | 高 4 bit，用于朝向、状态、水位、开关等 metadata。 |
| block light | 另一个 `unsigned char`，4 bit skylight + 4 bit torch/lava light。 |

对应代码位置：

| 文件 | 可关注内容 |
| ---- | ---------- |
| `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/worlddata/block.h` | `Block`、`BlockLight`、id/data/light packing。 |
| `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/worlddata/section.h` | Section 内 block/light 存储和 lazy allocation。 |
| `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/worlddata/chunk.h` | Chunk 对 section、height、biome、light、neighbor 的组织。 |

MineCraft3D 当前 `src/MineCraft3D/World/Block/ChunkBlock.h` 的模型更简单，主要存 block id。
建议演进路线：

| 阶段 | 建议 |
| ---- | ---- |
| 短期 | 保持 `ChunkBlock` API 稳定，新增 metadata 字段但先不广泛使用。 |
| 中期 | 把朝向、门开关、水位、生长阶段等状态放进 metadata，不再靠方块 id 爆炸。 |
| 中期 | 新增 `BlockLight` 存储，先只支持 sunlight 和 torch light 的查询。 |
| 长期 | 引入区块光照传播、dirty light bitset 和局部 relight。 |

不要一开始就把 4096 种上限、位布局和所有状态照搬。当前项目可以先这样定义：

```cpp
struct ChunkBlock {
    BlockId id;
    std::uint8_t metadata;
};
```

等存档格式和光照系统明确后，再压缩成 16-bit packed block。

## 方块材质和行为系统

MiniGame 的 `blocks/BlockMaterial.h` 把方块行为做成一个较大的虚函数接口，覆盖了：

| 能力 | 例子 |
| ---- | ---- |
| 渲染 | `createBlockMesh`、`getRenderType`、draw type。 |
| 碰撞 | `createCollideData`、`getCollisionBoundingBox`。 |
| 选择/拾取 | `createPickData`。 |
| 物理属性 | `isSolid`、`isLiquid`、`isAir`、`isOpaqueCube`。 |
| 世界事件 | `onBlockAdded`、`onBlockRemoved`、`onNeighborBlockChange`。 |
| tick | `blockTick`、随机 tick、重力、液体流动、植物生长。 |
| 放置规则 | `canPlaceBlockAt`、`canBlockStay`、方向、旋转。 |
| 掉落和硬度 | `dropBlock`、`getBlockHardness`。 |
| 红石/供电 | `canProvidePower`、弱/强 power。 |

`blocks/BlockMaterialMgr.h` 则承担材质注册、模型加载、纹理图集、item icon、方块几何、
材质文件等管理。

对 MineCraft3D 的可参考点：

| 当前问题 | 可借鉴方向 |
| -------- | ---------- |
| `BlockDataHolder` 偏静态渲染数据。 | 增加 `BlockBehavior` 或 `BlockType` 策略层，不把所有行为塞进 enum 判断。 |
| 方块渲染类型有限。 | 引入 render type/draw type：opaque、transparent、water、flora、model、decal。 |
| 碰撞和渲染耦合风险。 | 把 collision shape 和 render mesh shape 分开。 |
| 后续状态方块会增多。 | id 表示类型，metadata 表示状态，behavior 根据两者给出结果。 |

建议不要复制 MiniGame 的超大 `BlockMaterial` 虚接口。更适合当前项目的轻量拆法：

| 层 | 职责 |
| -- | ---- |
| `BlockDefinition` | id、名称、纹理、透明度、硬度、是否可碰撞等静态数据。 |
| `BlockBehavior` | 放置、破坏、tick、邻居变化、掉落等行为。 |
| `BlockShape` | cube、cross、fluid、model、自定义几何。 |
| `BlockRenderInfo` | atlas 坐标、shader/render pass、透明排序需求。 |

这样既能吸收 MiniGame 的扩展点，又不会把当前代码一次性重构成大型继承树。

## 区块和 Section 管理

MiniGame 的 `Section` 是 16x16x16 体素块组织单位，功能远多于当前 MineCraft3D 的
`ChunkSection`：

| MiniGame Section 能力 | 价值 |
| -------------------- | ---- |
| lazy `allocBlocks` / `clearBlocks` | 空 section 不分配完整 block 数组，节省内存。 |
| `m_EmptyBlock` fallback | 读取空 section 时不必特殊处理空指针。 |
| block light 存储 | 每个 section 可以独立维护光照。 |
| dirty flags bitset | 精确标记哪些 block/light 需要更新。 |
| face connectivity | 可用于可见性、寻路、连通性或渲染裁剪。 |
| non-empty count | 快速判断 section 是否为空。 |
| random tick count | 快速判断是否需要随机 tick。 |
| actor list | section 可以挂动态对象。 |
| render/physics/minimap mesh 分离 | 不同用途的 mesh 可分别失效和重建。 |

MiniGame 的 `Chunk` 还维护：

| Chunk 能力 | 价值 |
| ---------- | ---- |
| section 数组 | Y 方向拆 section，而不是一个大数组。 |
| height map | 快速查询每个 x/z 列最高方块。 |
| biome data | 每个 x/z 列有生态/温湿度/地貌信息。 |
| neighbor chunk 指针 | mesh、光照、流体和寻路减少重复查询。 |
| chunk load/save | 世界持久化基础。 |
| light propagation | 区块级光照更新。 |
| block index search | 快速找某类方块，如容器、光源、作物。 |

对 MineCraft3D 的建议演进：

| 优先级 | 建议 |
| ------ | ---- |
| 高 | 给 `ChunkSection` 加 dirty 标记，至少区分 `meshDirty` 和 `blocksDirty`。 |
| 高 | 区块修改时只重建受影响 section 和相邻边界 section。 |
| 中 | 增加 non-empty block count，空 section 可以跳过 mesh 构建。 |
| 中 | 增加 3x3 neighbor cache，mesh 构建时减少 `World` 查询。 |
| 中 | 增加 per-column height map，用于生成、光照和天气。 |
| 低 | actor/container/search index 等到实体和存档系统成型后再做。 |

## 网格生成和顶点格式

MiniGame 在 `display/BlockMeshVert.h` 中使用更紧凑的顶点格式：

| 数据 | 类型/策略 |
| ---- | --------- |
| position | short/int16 级别的局部坐标。 |
| uv | short/int16，使用 `BLOCKUV_SCALE = 4096`。 |
| color/light | packed color。 |
| block position/face | 可压缩或复用。 |

它在 `display/BlockGeom.h` 中支持几何模板：

| 能力 | 意义 |
| ---- | ---- |
| XML/JSON 加载 | 非标准方块不写死在 C++。 |
| cube/morph cube/model face | 支持栅栏、台阶、墙、植物、模型方块等。 |
| clipping | 方块可以按状态裁剪几何。 |
| bounding box | 渲染和碰撞都能拿到几何边界。 |

`bin/res/blockgeom.xml` 是非常有价值的资源格式参考。它把 cube、fence、wall 等几何用
顶点和 face 定义出来，说明“方块形状”可以独立于 C++ 方块类。

MiniGame 的 `worlddata/section_mesh.cpp` 还有两个关键优化方向：

| 优化 | 说明 |
| ---- | ---- |
| 18x18x18 halo 缓存 | 为 16x16x16 section 额外缓存一圈邻居方块，构建 mesh 时不用频繁跨 chunk 查询。 |
| pane/greedy merge | 把同材质、同方向、同光照条件的相邻面片合并，减少顶点和 draw 压力。 |

MineCraft3D 当前 `src/MineCraft3D/World/Chunk/ChunkMeshBuilder.cpp` 的做法适合早期维护：
判断相邻方块是否遮挡，然后逐面输出。后续建议按这个顺序优化：

1. 引入 section 边界 halo cache，不改变最终网格结构。
2. 给每个 face 记录 material/render pass/light，保证后续可以合并。
3. 对 opaque cube 做 greedy meshing。
4. flora/water/model 方块继续走专用 mesh，不强行合并。
5. 需要大量方块后，再考虑压缩顶点格式。

## 光照系统

MiniGame 的 `BlockLight` 设计是很经典的体素方案：

| 光照类型 | bit 数 | 用途 |
| -------- | ------ | ---- |
| skylight | 4 bit | 天光，从上向下传播。 |
| block light | 4 bit | 火把、岩浆、发光方块等。 |

它还在 `Section` 和 `Chunk` 中维护 light dirty flags、relight、spread light、update light
等接口。MineCraft3D 后续如果要做昼夜、火把、水下亮度、透明方块影响，就需要类似模型。

建议路线：

| 阶段 | 实现范围 |
| ---- | -------- |
| 1 | 渲染顶点颜色里加入简单高度/方向亮度，保持当前效果。 |
| 2 | 增加 per-block `BlockLight` 数组，但先只计算 sunlight。 |
| 3 | 方块修改时局部重新传播 sunlight。 |
| 4 | 支持 block-emissive light，如 torch/lava。 |
| 5 | 把 light dirty bitset 和 mesh dirty 关联，避免全 chunk 重建。 |

## 地形生成

MiniGame 的 `terrgen` 很适合作为地形模块拆分参考。它不是把所有生成逻辑写在一个
generator 里，而是拆成：

| 模块 | 职责 |
| ---- | ---- |
| `ChunkGenerator` | 管理生成线程、请求、seed、chunk 范围和生成入口。 |
| `Ecosystem` | biome/生态，决定 top block、fill block、温湿度、树/草/生物等。 |
| `EcosysUnit_*` | 装饰器单元：树、草、矿物、湖、水体、地牢、仙人掌、芦苇、火山、珊瑚等。 |
| `ChunkRandGen` | chunk 级随机数，保证生成稳定可复现。 |

对 MineCraft3D 的建议：

| 当前/近期需求 | 参考实现方向 |
| ------------- | ------------ |
| 基础地形 | 保持当前 terrain generator，但抽出 noise 和 height map 生成。 |
| 树/花/草 | 做成 `Structure` 或 `Decorator`，不要塞回主 terrain loop。 |
| biome | 每个 x/z column 存 biome id 或 climate 值。 |
| 稳定随机 | 使用 world seed + chunk coord 派生随机数。 |
| 异步生成 | 等 chunk streaming 做起来后，再加生成队列/线程。 |
| 生成依赖邻区 | 大型结构要有边界策略，避免跨 chunk 写入丢失。 |

一个适合 MineCraft3D 的中期结构：

```text
World/Generation/
  Terrain/
    TerrainGenerator
    NoiseSettings
    HeightMap
  Biome/
    Biome
    BiomeRegistry
  Decorator/
    TreeDecorator
    OreDecorator
    FloraDecorator
  Structures/
    StructureBuilder
```

当前目录已经有 `World/Generation/Biome`、`Structures`、`Terrain`，所以可以沿这个方向
自然扩展，不需要大搬迁。

## UI 和脚本

MiniGame 的 `bin/res/ui/mobile` 里大量 UI 是 XML + Lua 组合，且能看到 MVC 命名：

| 类型 | 例子 | 参考点 |
| ---- | ---- | ------ |
| XML | UI 布局资源。 | 布局和代码分离。 |
| Lua View | `*View.lua` | 只关心界面展示。 |
| Lua Model | `*Model.lua` | 管理界面状态数据。 |
| Lua Ctrl | `*Ctrl.lua` | 处理交互和事件。 |

MineCraft3D 当前用 ImGui 做调试界面是合理的。后续可以参考 MiniGame 的分层，但不要急着
引入 Lua。更现实的路线：

1. 先把 ImGui 调试面板按模块拆分，如 world、player、blocks、render。
2. UI 状态不要散落在渲染代码里，集中到 debug/controller 对象。
3. 如果未来做真正游戏 UI，再决定是否引入数据驱动布局。
4. 如果要脚本化，先脚本化资源/方块定义，不要先脚本化核心循环。

## 工具链

MiniGame 包含多个工具和资源管线痕迹：

| 工具/资源 | 参考价值 |
| --------- | -------- |
| `ObjectEditor` | 编辑模型/对象/资源的独立工具思路。 |
| `UIEditor` | UI 可视化编辑工具思路。 |
| `Tool_UIEditor` | 工具和运行时共用部分 UI 代码。 |
| `resbuild.bat` / `ConvertFile` | 资源预处理和打包。 |
| `blockgeom.xml` | 方块几何可以变成资源，而不是 C++ 常量。 |
| `csvdef` | 表格数据定义方块、道具、UI 文案等。 |

MineCraft3D 短期最值得做的不是编辑器，而是轻量工具：

| 工具 | 用途 |
| ---- | ---- |
| asset check | 检查 `media/` 中 shader、texture、font 是否缺失。 |
| block atlas validator | 检查方块纹理坐标是否越界、是否引用不存在贴图。 |
| block data dumper | 打印当前 `BlockDatabase` 注册结果。 |
| screenshot smoke test | 启动游戏后截一张图，检查窗口/资源是否加载。 |
| resource manifest generator | 生成或校验资源索引。 |

## 网络和多人

MiniGame 带有 RakNet、`net`、`cs`、room/server 相关 exe 和大量在线 SDK。这些说明它支持
或曾经支持多人、账号、房间、语音、平台服务等。

对当前 MineCraft3D 的结论：

| 判断 | 原因 |
| ---- | ---- |
| 不建议现在引入网络层 | 会改变世界权威、事件同步、存档和输入模型。 |
| 可提前保留事件边界 | `World/Event` 这类模块后续可以变成同步命令。 |
| 多人化应单独设计 | 需要 server authoritative、chunk streaming、实体同步、预测/回滚等。 |
| RakNet 不作为首选 | 库较旧，后续若做多人应重新评估 ENet、Steam Networking、asio 等。 |

## 持久化和存档

MiniGame 的 `Chunk` 中能看到 load/save buffer、data save、chunk binary/protobuf 风格接口。
这说明它把世界数据作为 chunk 级单位持久化，而不是保存整张地图。

MineCraft3D 可以参考：

| 能力 | 建议 |
| ---- | ---- |
| chunk-level save | 每个 chunk 独立存储，便于增量保存和无限世界。 |
| versioned format | 文件头带版本，避免后续格式修改无法兼容。 |
| compressed payload | 大量 air block 或重复 block 可以压缩。 |
| block palette | 一个 chunk 内用局部 palette 压缩 block id。 |
| delayed save | block 修改后标记 dirty，后台或定期写盘。 |

建议不要直接选 protobuf 或 MiniGame 的格式。当前可以先做简单二进制版本：

```text
magic
version
chunk_x, chunk_z
section_count
section records
optional metadata/light
```

等 block metadata/light/biome 定型后再优化。

## 平台兼容参考

MiniGame 里有 Windows、Android、Apple、D3D9、OpenGL、平台 SDK 等痕迹。对 MineCraft3D 的
Windows/macOS 兼容工作，参考点主要是边界意识：

| 方向 | MineCraft3D 建议 |
| ---- | ---------------- |
| 路径 | 所有运行时路径继续通过 `Util/ResourcePaths` 或后续资源系统访问。 |
| 动态库 | Windows DLL 拷贝、macOS dylib rpath 都放在 Premake 构建层处理。 |
| OpenGL 版本 | macOS 固定兼容 4.1 core，Windows 可用更高版本但代码避免依赖过新特性。 |
| 字体/输入 | 未来中文 UI、输入法、键盘布局要集中处理。 |
| 文件大小写 | macOS/Windows 默认大小写不敏感，但资源命名仍应保持严格一致。 |
| 路径分隔符 | 代码中不要写死 `\`，配置里也尽量使用标准相对路径。 |

## 不建议照搬的内容

| 内容 | 原因 |
| ---- | ---- |
| OgreMain 整套引擎 | 体量大，历史依赖重，会压垮当前项目复杂度。 |
| D3D9 渲染后端 | 已过时，且当前目标是 SFML + OpenGL 的 Windows/macOS 兼容。 |
| 平台 SDK 目录 | 4399、QQ、Steam、WeTest、GVoice 等和当前目标无关。 |
| 大型 Lua/ToLua 桥接 | 当前没有复杂 UI/任务/活动系统，提前引入收益低。 |
| RakNet 网络层 | 多人不是当前核心目标，且库选择需要重新评估。 |
| 旧 Visual Studio/Android.mk 构建 | MineCraft3D 已使用 Premake，更干净。 |
| bin 下完整资源和 exe/dll | 版权、体量和依赖都不适合迁移。 |
| 超大 `BlockMaterial` 虚函数接口 | 思路可借鉴，接口应按当前项目规模重做。 |

## 可参考点清单

下面是后续迭代时可以逐项回看的清单。

| 编号 | 可参考点 | MiniGame 位置 | MineCraft3D 落地建议 |
| ---- | -------- | ------------- | -------------------- |
| 1 | package/mount 资源层 | `bin/iworld.cfg`、`OgrePackageFile.*` | 先做 `media/` manifest，再考虑资源包。 |
| 2 | 统一资源管理入口 | `OgreResourceManager.*` | 在 `ResourcePaths` 上层增加 `AssetRegistry`。 |
| 3 | zip/web/patch/mod 资源来源 | `OgrePackageZipFile.*`、`OgrePackageWebFile.*`、`OgreModFileManager.*` | 远期支持资源包和 mod 时参考。 |
| 4 | block id + metadata | `worlddata/block.h` | 扩展 `ChunkBlock`，先用显式字段，后续再压缩。 |
| 5 | 4+4 bit 光照 | `BlockLight` | 新增 `BlockLight` 数组，先 sunlight 后 block light。 |
| 6 | lazy section storage | `worlddata/section.h` | 空 section 不分配完整 block/light。 |
| 7 | non-empty count | `Section` | 快速跳过空 section mesh 构建。 |
| 8 | dirty flags bitset | `Section` | 区分 block dirty、mesh dirty、light dirty。 |
| 9 | neighbor chunk cache | `worlddata/chunk.h` | mesh/light/fluid 更新减少跨 world 查询。 |
| 10 | height map | `Chunk` | terrain、光照、天气和碰撞查询都可复用。 |
| 11 | biome per column | `Chunk`、`terrgen` | 每个 x/z 列保存 biome/climate。 |
| 12 | chunk-level save/load | `Chunk` | 设计版本化 chunk 存档格式。 |
| 13 | section mesh 分类 | `Section::createMesh*` | 渲染、物理、minimap mesh 独立失效。 |
| 14 | 18x18x18 halo cache | `section_mesh.cpp` | mesh 构建前缓存邻居边界。 |
| 15 | greedy/pane meshing | `section_mesh.cpp` | 先用于 opaque cube。 |
| 16 | packed vertex | `display/BlockMeshVert.h` | 顶点量变大后再压缩。 |
| 17 | block geometry template | `display/BlockGeom.h`、`bin/res/blockgeom.xml` | 非 cube 方块形状资源化。 |
| 18 | render/draw type | `blocks/BlockMaterial.h` | 明确 opaque/water/flora/model pass。 |
| 19 | collision shape 分离 | `BlockMaterial::createCollideData` | 渲染几何和碰撞盒分离。 |
| 20 | block behavior hooks | `BlockMaterial` | 用轻量 `BlockBehavior` 替代硬编码分支。 |
| 21 | material manager | `BlockMaterialMgr.h` | 让 block registry 统一管理数据、shape、render info。 |
| 22 | random tick count | `Section` | 植物生长、液体、火等只扫描需要 tick 的 section。 |
| 23 | block search index | `Chunk` | 光源、容器、作物等后续可快速定位。 |
| 24 | actor in chunk/section | `Section`、`Chunk` | 未来实体系统按 chunk 管理生命周期。 |
| 25 | async chunk generation | `terrgen/ChunkGenerator.h` | chunk streaming 后再加生成线程。 |
| 26 | ecosystem/decorator | `terrgen/Ecosystem.h`、`EcosysUnit_*` | 把树、矿、草、湖等从主生成逻辑拆开。 |
| 27 | stable chunk random | `ChunkRandGen` | world seed + chunk coord 派生随机。 |
| 28 | UI XML + Lua MVC | `bin/res/ui/mobile` | 复杂 UI 时参考 View/Model/Controller 分离。 |
| 29 | resource/editor tools | `ObjectEditor`、`UIEditor` | 先做资源校验工具，不做完整编辑器。 |
| 30 | localization/table data | `bin/res/csvdef` | UI 文案、block/item 数据增加后可表格化。 |
| 31 | renderer backend boundary | `RenderSystem_D3D9`、`RenderSystem_OGL` | 保持游戏逻辑不直接依赖平台后端。 |
| 32 | platform-specific SDK isolation | `lib4399MGSDK`、`qqrailsdk`、`steamsdk` | 平台相关代码必须独立边界，不污染核心。 |
| 33 | mod/custom resource path | `mod`、`OgreModFileManager` | 远期支持自定义资源包。 |
| 34 | runtime config | `iworld.cfg` | 把窗口、资源、渲染配置外置。 |
| 35 | asset preprocess | `resbuild.bat`、`ConvertFile` | 增加 atlas/manifest 生成和校验脚本。 |

## 建议迭代路线

### 短期

适合在当前 MineCraft3D 上尽快做，风险低、收益明确：

| 任务 | 说明 |
| ---- | ---- |
| 增加资源检查脚本 | 检查 shader、texture、font、block texture coords 是否缺失。 |
| 给 chunk/section 加 dirty 状态 | 方块修改后只重建必要 mesh。 |
| 拆分 block 定义 | 把静态数据、渲染信息、行为入口拆清楚。 |
| 引入 metadata 字段 | 为朝向、水位、植物阶段预留空间。 |
| 区分 render pass | opaque、water、flora 逻辑显式化。 |

### 中期

适合在区块数量、方块类型和生成内容增加后做：

| 任务 | 说明 |
| ---- | ---- |
| section halo cache | 减少 mesh 构建时的跨 chunk 查询。 |
| opaque greedy meshing | 大幅减少普通地形顶点数。 |
| height map/biome column | 支撑更丰富地形、光照和天气。 |
| block light 初版 | 支持 sunlight 和简单发光方块。 |
| chunk 存档格式 | 支持世界持久化和无限世界。 |
| decorator 地形生成 | 树、矿、草、湖等从 terrain 主循环拆出。 |

### 长期

适合项目从技术 demo 走向完整沙盒后再做：

| 任务 | 说明 |
| ---- | ---- |
| 资源包和 mod | package mount、覆盖顺序、自定义 block/model。 |
| 脚本化 UI/方块 | 先资源定义脚本化，再考虑逻辑脚本化。 |
| 工具编辑器 | block shape/atlas/UI/world object 编辑器。 |
| 多人网络 | 单独设计 server authority 和同步协议。 |
| 实体系统 | actor 按 chunk/section 管理，支持保存、AI 和碰撞。 |
| 完整光照传播 | dirty light bitset、透明方块、火把/岩浆动态更新。 |

## 推荐阅读路径

后续要深入某个方向时，可以按这些文件顺序看 MiniGame：

### 方块和区块

1. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/worlddata/block.h`
2. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/worlddata/section.h`
3. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/worlddata/chunk.h`
4. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/worlddata/section_mesh.cpp`

### 方块材质和形状

1. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/blocks/BlockMaterial.h`
2. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/blocks/BlockMaterialMgr.h`
3. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/display/BlockGeom.h`
4. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/display/BlockMeshVert.h`
5. `/Users/lizi/Desktop/Workspace/MiniGame/bin/res/blockgeom.xml`

### 地形生成

1. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/terrgen/ChunkGenerator.h`
2. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/terrgen/Ecosystem.h`
3. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/terrgen/EcosysUnit_*.h`
4. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/iworld/terrgen/EcosysUnit_*.cpp`

### 资源系统

1. `/Users/lizi/Desktop/Workspace/MiniGame/bin/iworld.cfg`
2. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/OgreMain/OgreResourceManager.h`
3. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/OgreMain/OgrePackageFile.h`
4. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/OgreMain/OgrePackageZipFile.h`
5. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/OgreMain/OgreModFileManager.h`

### UI 和工具

1. `/Users/lizi/Desktop/Workspace/MiniGame/bin/res/ui/mobile`
2. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/UIEditor`
3. `/Users/lizi/Desktop/Workspace/MiniGame/Miniw-Client/ObjectEditor`

## 对 MineCraft3D 的最终建议

MiniGame 的参考价值可以概括为一句话：它展示了一个体素游戏从 demo 走向完整产品时，
哪些系统会自然长出来。MineCraft3D 当前不应该追求一次性复刻这些系统，而应该按以下
顺序吸收：

1. 先补齐当前项目最缺的工程化能力：资源检查、dirty mesh、block metadata。
2. 再优化体素核心：halo cache、greedy meshing、light storage、height map。
3. 然后扩展内容生产能力：block shape 资源化、terrain decorator、简单工具脚本。
4. 最后再考虑大系统：存档、mod、脚本 UI、实体、多人与资源包。

这样做能保留当前 MineCraft3D 的轻量和可维护性，同时把 MiniGame 里经过验证的体素项目
经验逐步吸收进来。
