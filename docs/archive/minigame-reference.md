# MiniGame Reference Notes

> 当前定位（2026-08-21 复核）：本文对应的本机参考仓库是 `F:\env1_trunk`，不是
> 早期笔记误写的 `/Users/lizi/Desktop/Workspace/MiniGame`。参考仓库当前可读，但未纳入
> HelloMine3D，也不是它的构建依赖。本文仍然只是架构考察，不是 backlog、需求来源或
> 优先级依据。使用前应先检查 HelloMine3D 当前代码和 `docs/current/todolist.md`，只借鉴模块
> 边界，不复制参考仓库的源码、资源、第三方库、账号/网络逻辑或构建方式。

## 当前用途

本文最适合在一个明确功能已经进入开发时回答“成熟体素项目通常如何拆这个系统”，
而不适合回答“下一步应该做什么”。当前任务顺序只由 `docs/current/todolist.md` 决定。

| 用法 | 结论 |
| ---- | ---- |
| 体素核心设计复核 | 可参考 block/metadata/light、section、halo、greedy mesh、行为与形状分层。 |
| K4/G2-G4 UI 设计 | 只借鉴 View/Model/Controller 和状态隔离，不引入旧 XML/Lua 体系。 |
| G5 音频边界 | 可参考 2D/3D、listener、音量、pause/mute 和 dummy backend，不引入 FMOD/直播 DSP。 |
| G3 工具与采集 | 可借鉴方块硬度、行为、掉落和碰撞职责分离，不复制大型 `BlockMaterial` 接口。 |
| 内容扩展 | 可参考 biome/decorator、资源索引和轻量工具链的模块边界。 |
| Stage 9 / N7-N12 | 只作为胜利、AI、投射物、结构、经济、本地化和音频的边界/反模式库；详细映射见本文“Stage 9 定向参考映射”。 |
| 当前不采用 | RakNet、多平台 SDK、D3D9、完整编辑器、旧 OgreMain、Lua/ToLua、在线更新和大规模 mod。 |

## 对当前玩法主线的直接价值

重新读取 `F:\env1_trunk` 后，已经完成的 K4/G2-G6 可以使用的历史参考边界如下。参考强度只表示
“值得阅读”，不表示应该移植代码。

| 当前任务 | 参考位置 | 价值与边界 |
| -------- | -------- | ---------- |
| K4 世界管理 | `client\miniModule\MiniPlatform\GameWorld\WorldListMgr.*`、`Account\OWorldList\*` | 可参考世界描述、最近打开和列表更新语义；实现深度耦合账号、云世界、网络和 Lua，不能作为本地世界管理实现模板。HelloMine3D 应继续以 `WorldCatalogue`/`WorldBackup` 为核心。 |
| G2 工作台制作 | `client\miniSandbox\sandboxPlay\gameplay\mgr\CraftMgr.*`、`sandboxCore\blocks\container*` | 可参考可制作数量、材料组替换、产物容量、烹饪与普通制作分离；应避免其 Singleton、Player、BackPack 和 UI 混合方式，保留 HelloMine3D 的纯预览与原子提交。 |
| G3 工具成长 | `client\miniModule\CsvLoader\ToolDefCsv.*`、`sandboxCore\blocks\BlockMaterial*` | 可参考数据驱动工具表、方块硬度/掉落/能力边界；不复制超大的 `BlockMaterial` 虚接口。 |
| G4 设置与暂停 | `bin_externel\iworld.cfg`、`sandboxPlay\gameplay\GameSettings.*` | 配置覆盖视距、音量、灵敏度、反转 Y 等产品字段；但 `GameSettings::loadSettings/saveSettings` 当前为空实现，只适合作为字段清单，不是持久化范例。 |
| G5 基础音频 | `client\miniEngine\OgreMain\sound\OgreSoundSystem.*` | 2D/3D、listener、全局音乐/音效音量、pause/mute 和 dummy backend 的接口边界很有价值；FMOD、直播 DSP、资源和平台实现都过重，不应移植。 |
| G6 综合流程 | `bin_externel\res\ui\mobile` 和既有新手引导配置 | 只能用于检查完整产品会覆盖哪些入口和反馈；其在线活动、账号、商城和脚本复杂度不属于当前单机切片。 |

## 与当前实现的关系

本文最初列出的许多“短期/中期建议”现已完成或被更合适的本项目方案替代：

| 原参考点 | 当前 HelloMine3D 结果 |
| -------- | -------------------- |
| section dirty、边界失效 | 已由 S0/M 系列完成并有定向自动测试。 |
| block metadata、定义/渲染/行为/形状分层 | 已由 S3 系列完成。 |
| 18x18x18 halo、opaque greedy meshing | 已由 M 系列完成。 |
| height map、sunlight/block light | 已有缓存、更新和运行时验证。 |
| biome、decorator、稳定随机和跨区块结构 | 已由 S6/C 系列完成。 |
| chunk 级版本化保存 | 已扩展到 K1-K3 世界身份、事务发布和验证恢复。 |
| 资源检查、manifest、只读资源包 | 已由 W3/X1-X3 完成；无需照搬 MiniGame package 系统。 |
| actor 生命周期和持久化 | 已由 P/D 系列完成。 |
| 紧凑顶点 | W4 已用实际数据判定当前收益不足，因此暂不实施。 |

因此，本文现在最有价值的剩余部分是“架构经验库”，不是一张尚未完成的功能清单。
尤其是多人、脚本、mod、编辑器和多渲染后端都不应因为出现在本文中而自动进入规划。

本文记录对本机 `F:\env1_trunk` 的结构分析，以及它对 HelloMine3D 后续迭代的可参考点。
结论先行：它是一个成熟但历史包袱较重的商业体素客户端/工具/平台仓库，适合作为架构
和功能拆分参考，不适合直接复制源码、第三方库或构建方式。

## 分析范围

本次主要查看了这些目录和文件：

| 路径 | 观察重点 |
| ---- | -------- |
| `F:\env1_trunk\client\miniSandbox\sandboxCore` 与 `miniSandboxSDK\sandboxCore\include` | 方块、区块、世界数据、地形生成、网格、光照、实体和存档核心。 |
| `F:\env1_trunk\client\miniSandbox\sandboxPlay` | 玩家、制作、玩法管理器、AI、输入状态与游戏模式。 |
| `F:\env1_trunk\client\miniGame\iworld` | 客户端运行时、平台、UI 控制和业务接入层；体素核心已经不主要位于此处。 |
| `F:\env1_trunk\client\miniEngine\OgreMain` 与 `client\miniShared` | 渲染/声音基础设施，以及资源、文件和 package 抽象。 |
| `F:\env1_trunk\client\miniEngine\RenderSystemD3D9`、`RenderSystemOGL` | D3D9/OpenGL 渲染后端。 |
| `F:\env1_trunk\client\miniModule\MiniSandbox\RakNet` | 网络库和多人通信基础设施。 |
| `F:\env1_trunk\client\miniModule\MiniDeveloper\uieditor`、`SceneEditor` | UI、场景、资源和开发者工具；旧笔记中的独立 ObjectEditor 路径并不存在。 |
| `F:\env1_trunk\bin_externel` | 运行时配置、资源包、脚本、UI、CSV、模型、工具输出和历史运行数据。 |
| `F:\env1_trunk\client\miniSandbox\docs` | 仓库内已有的总架构和 17 篇系统分解文档，适合优先阅读。 |

局部规模参考：

| 项 | 规模 |
| -- | ---- |
| `client` | 37,731 个文件，约 17.82 GiB。 |
| `bin_externel` | 87,540 个文件，约 6.45 GiB。 |
| `miniSandbox/sandboxCore` | 1,698 个文件，其中约 1,691 个原生源码文件。 |
| `miniSandboxSDK/sandboxCore` | 796 个公开/预构建头文件。 |
| `miniGame/iworld` | 317 个文件，其中约 273 个原生源码文件；它不再等同于整个体素核心。 |
| `miniSandbox/sandboxPlay` | 731 个文件，其中约 730 个原生源码文件。 |
| `miniModule` | 2,740 个文件，其中约 2,288 个原生源码文件。 |
| `bin_externel/res` | 61,922 个资源文件。 |

这不是一个小型示例项目，而是一个包含客户端、编辑器、资源管线、脚本、平台 SDK、网络
和大量历史依赖的完整工程。因此后续参考时要按模块取思想，避免把复杂度搬进当前项目。

## 总体判断

MiniGame 对 HelloMine3D 的最大价值不是构建脚本或第三方库，而是这些方面：

| 方向 | 参考价值 | 当前 HelloMine3D 状态 |
| ---- | -------- | --------------------- |
| 方块数据模型 | 证明 `block id + metadata + light` 的紧凑表示很有价值。 | metadata 与 block light 已完成；当前只在扩展状态方块时回看兼容边界。 |
| 区块/Section 管理 | 有懒分配、dirty flags、邻居关系、光照脏标记和多用途 mesh。 | dirty、邻居边界、height/light 等关键能力已落地，不再是当前主线。 |
| 网格生成 | 使用邻居 halo 缓存和面片合并思路，避免重复跨 chunk 查询。 | halo 与 opaque greedy meshing 已完成；后续只在性能证据需要时继续优化。 |
| 方块材质/行为 | `BlockMaterial` 集中渲染、碰撞、放置、tick、事件等行为。 | HelloMine3D 已拆成 Definition/Behavior/Shape/RenderInfo，边界更适合当前规模。 |
| 资源系统 | 有 package/mount、zip/web/patch/mod 等资源层概念。 | manifest 与有界只读资源包已完成，不引入 web/patch/mod 复杂度。 |
| 地形生成 | 用生态/装饰器单元拆分地貌、树、草、矿物、湖、地牢等。 | biome/decorator/结构已经落地，不再是当前主线。 |
| 工具链 | 有 UI/Scene 编辑器、资源转换工具和几何定义文件。 | 资产检查和 manifest 已完成；完整编辑器仍不值得近期投入。 |
| 脚本/UI | UI 资源存在 XML + Lua + MVC 分层。 | 当前用 ImGui 调试界面即可，复杂 UI 可参考它的 View/Model/Controller 思路。 |
| 跨平台/后端 | 有 D3D9/OGL、Android、Apple、平台 SDK 目录。 | 当前重点是 Windows/macOS，参考其抽象边界，不参考旧 SDK 细节。 |
| 网络/多人 | RakNet 和大量 net/cs 代码说明多人系统是独立大模块。 | 当前不建议引入，多人化应作为远期独立里程碑。 |

## MiniGame 顶层结构

### `client/miniSandbox/sandboxCore` 与 `miniSandboxSDK/sandboxCore/include`

这是 MiniGame 最值得看的目录，核心内容包括：

| 子目录 | 作用 | 对 HelloMine3D 的参考点 |
| ------ | ---- | ---------------------- |
| `worldData` | `Block`、`BlockLight`、`Chunk`、`Section`、mesh/light/save 等世界数据。 | 区块存储、光照、dirty flags、邻居缓存、序列化设计的主要参考。 |
| `blocks` | `BlockMaterial`、`BlockMaterialMgr` 和各种方块行为。 | 方块从“数据表”升级到“数据 + 行为策略”的参考。 |
| `worldMesh` / `display` | 方块几何、顶点格式、显示相关数据。 | 非标准方块、紧凑顶点格式、模型/几何模板的参考。 |
| `terrgen` | 地形生成、生态系统、树、矿物、湖、地牢等生成单元。 | 把 terrain base 和 decorator 拆开的参考。 |
| `sandboxPlay/player` | 玩家行为、控制、交互。 | G2-G4 只参考状态和职责拆分。 |
| `actors` | 实体/生物/动态对象。 | 未来实体系统、chunk 内 actor 管理的参考。 |
| `camera` | 相机系统。 | 当前 HelloMine3D 已有基础相机，参考价值较小。 |
| `sandboxPlay/ai` | AI 行为。 | 远期实体 AI 可参考，但不应现在引入。 |
| `sandboxPlay/gamenet` / `miniModule` | 网络和客户端/服务器交互。 | 多人化前不引入。 |
| `miniModule/MiniDeveloper/mod` | 模组相关。 | 当前不引入可执行 mod。 |
| `utility` | 通用工具。 | 可按需查看，不建议整包迁移。 |

### `client/miniEngine/OgreMain` 与 `client/miniShared`

OgreMain 主要提供渲染、场景、输入和声音边界；资源管理、文件系统和 package 实现主要
位于 `miniShared`，Lua/UI/网络又分散在其他模块。值得关注的是边界，不是具体实现：

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

HelloMine3D 已经用严格 manifest、启动冻结的有效资源视图、路径约束和 X1-X3 只读资源包
吸收了其中适合当前规模的部分。zip/web/patch/mod 和运行时热更新不属于当前路线；G5
如果增加声音资源，应通过新的版本化资源类别扩展现有合同，而不是另建 package 系统。

### `client/miniEngine/RenderSystemD3D9` / `RenderSystemOGL`

这两个目录说明 MiniGame 把渲染后端和游戏逻辑做了拆分。对当前项目的意义是：

| 可参考点 | 说明 |
| -------- | ---- |
| 渲染后端隔离 | 游戏世界不应直接依赖具体 OpenGL 调用。 |
| 平台差异收口 | Windows/macOS 的上下文创建、扩展、路径和动态库处理应放在少数边界里。 |
| 不直接照搬 D3D9 | D3D9 已过时，当前 HelloMine3D 使用 Ogre GL3Plus 更合适。 |

## 运行时资源模型

`F:\env1_trunk\bin_externel\iworld.cfg` 里能看到典型资源挂载：

```xml
<Package name="default" path="res\" readonly="true" />
<Package name="root" path="" />
```

这说明它把运行时资源组织成多个 package，其中 `res` 是只读默认资源，`root` 可以覆盖或
放置运行时文件。结合 `OgrePackageFile`、`OgrePackageZipFile`、`OgrePackageWebFile`、
`OgrePackagePatch` 和 `OgreModFileManager`，可以抽象出一套资源层级：

| 层级 | 可能含义 | HelloMine3D 可借鉴方式 |
| ---- | -------- | ---------------------- |
| 默认资源 | 游戏随包资源。 | 继续使用 `media/`，但建立 manifest。 |
| 根目录覆盖 | 本地配置、缓存、用户生成内容。 | 把 `bin/` 中的运行时配置和用户数据分开。 |
| patch | 热更新或修复包。 | 短期不做，远期资源包可覆盖 manifest 项。 |
| zip package | 压缩分发资源。 | 等资源量增长后再做。 |
| web package | 远端下载资源。 | 当前不需要。 |
| mod package | 用户扩展资源。 | 远期自定义 block/model 时有价值。 |

这条早期建议已经由当前的 manifest、严格启动资源视图和 X1-X3 有界资源包实现覆盖。
除非 G5 声音或后续模型资源需要新增版本化类别，不应再平行增加一个 `AssetRegistry`。
如果未来扩展资源合同，仍可沿用这些字段：

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
| `F:\env1_trunk\client\miniSandboxSDK\sandboxCore\include\worldData\block.h` | `Block`、`BlockLight`、id/data/light packing。 |
| `F:\env1_trunk\client\miniSandboxSDK\sandboxCore\include\worldData\section.h` | Section 内 block/light 存储和 lazy allocation。 |
| `F:\env1_trunk\client\miniSandboxSDK\sandboxCore\include\worldData\chunk.h` | Chunk 对 section、height、biome、light、neighbor 的组织。 |

这条演进路线已经大体完成：HelloMine3D 的 `ChunkBlock` 已有 metadata，光照存储、查询、
传播与局部更新也已进入世界运行栈。下面保留的是原始设计顺序，不再是当前待办：

| 阶段 | 建议 |
| ---- | ---- |
| 短期 | 保持 `ChunkBlock` API 稳定，新增 metadata 字段但先不广泛使用。 |
| 中期 | 把朝向、门开关、水位、生长阶段等状态放进 metadata，不再靠方块 id 爆炸。 |
| 中期 | 新增 `BlockLight` 存储，先只支持 sunlight 和 torch light 的查询。 |
| 长期 | 引入区块光照传播、dirty light bitset 和局部 relight。 |

项目仍不需要照搬 4096 种上限和完全相同的位布局。当前显式表示比过早压缩更容易维护：

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

对 HelloMine3D 的可参考点：

| 原问题 | 当前结果 |
| ------ | -------- |
| 静态渲染数据与行为混杂风险 | 已拆出 `BlockDefinition`、`BlockBehavior`、`BlockShape`、`BlockRenderInfo`。 |
| 方块渲染类型有限 | opaque、transparent、water、flora 等渲染通道已经显式分离。 |
| 碰撞和渲染耦合风险 | 当前已有独立 shape/碰撞边界，G3 只需扩展硬度、工具和掉落策略。 |
| 状态方块增多 | id 表示类型、metadata 表示状态的方向已由作物等功能验证。 |

建议不要复制 MiniGame 的超大 `BlockMaterial` 虚接口。更适合当前项目的轻量拆法：

| 层 | 职责 |
| -- | ---- |
| `BlockDefinition` | id、名称、纹理、透明度、硬度、是否可碰撞等静态数据。 |
| `BlockBehavior` | 放置、破坏、tick、邻居变化、掉落等行为。 |
| `BlockShape` | cube、cross、fluid、model、自定义几何。 |
| `BlockRenderInfo` | atlas 坐标、shader/render pass、透明排序需求。 |

这样既能吸收 MiniGame 的扩展点，又不会把当前代码一次性重构成大型继承树。

## 区块和 Section 管理

MiniGame 的 `Section` 是 16x16x16 体素块组织单位，功能远多于当前 HelloMine3D 的
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

对 HelloMine3D 的建议演进：

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

HelloMine3D 当前 `src/HelloMine3D/World/Chunk/ChunkMeshBuilder.cpp` 的做法适合早期维护：
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
等接口。HelloMine3D 后续如果要做昼夜、火把、水下亮度、透明方块影响，就需要类似模型。

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

对 HelloMine3D 的建议：

| 当前/近期需求 | 参考实现方向 |
| ------------- | ------------ |
| 基础地形 | 保持当前 terrain generator，但抽出 noise 和 height map 生成。 |
| 树/花/草 | 做成 `Structure` 或 `Decorator`，不要塞回主 terrain loop。 |
| biome | 每个 x/z column 存 biome id 或 climate 值。 |
| 稳定随机 | 使用 world seed + chunk coord 派生随机数。 |
| 异步生成 | 等 chunk streaming 做起来后，再加生成队列/线程。 |
| 生成依赖邻区 | 大型结构要有边界策略，避免跨 chunk 写入丢失。 |

一个适合 HelloMine3D 的中期结构：

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

HelloMine3D 当前用 ImGui 做调试界面是合理的。后续可以参考 MiniGame 的分层，但不要急着
引入 Lua。更现实的路线：

1. 先把 ImGui 调试面板按模块拆分，如 world、player、blocks、render。
2. UI 状态不要散落在渲染代码里，集中到 debug/controller 对象。
3. 如果未来做真正游戏 UI，再决定是否引入数据驱动布局。
4. 如果要脚本化，先脚本化资源/方块定义，不要先脚本化核心循环。

## 工具链

MiniGame 包含多个工具和资源管线痕迹：

| 工具/资源 | 参考价值 |
| --------- | -------- |
| `SceneEditor` / `BlockModelEditor` | 编辑场景、模型、方块和资源的工具思路。 |
| `UIEditor` | UI 可视化编辑工具思路。 |
| `Tool_UIEditor` | 工具和运行时共用部分 UI 代码。 |
| `resbuild.bat` / `ConvertFile` | 资源预处理和打包。 |
| `blockgeom.xml` | 方块几何可以变成资源，而不是 C++ 常量。 |
| `csvdef` | 表格数据定义方块、道具、UI 文案等。 |

这份早期工具建议中的 asset check、manifest、数据输出和截图冒烟已经落地；完整编辑器仍
不值得近期投入。保留清单用于新增资源类型时复核：

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

对当前 HelloMine3D 的结论：

| 判断 | 原因 |
| ---- | ---- |
| 不建议现在引入网络层 | 会改变世界权威、事件同步、存档和输入模型。 |
| 可提前保留事件边界 | `World/Event` 这类模块后续可以变成同步命令。 |
| 多人化应单独设计 | 需要 server authoritative、chunk streaming、实体同步、预测/回滚等。 |
| RakNet 不作为首选 | 库较旧，后续若做多人应重新评估 ENet、Steam Networking、asio 等。 |

## 持久化和存档

MiniGame 的 `Chunk` 中能看到 load/save buffer、data save、chunk binary/protobuf 风格接口。
这说明它把世界数据作为 chunk 级单位持久化，而不是保存整张地图。

HelloMine3D 可以参考：

| 能力 | 建议 |
| ---- | ---- |
| chunk-level save | 每个 chunk 独立存储，便于增量保存和无限世界。 |
| versioned format | 文件头带版本，避免后续格式修改无法兼容。 |
| compressed payload | 大量 air block 或重复 block 可以压缩。 |
| block palette | 一个 chunk 内用局部 palette 压缩 block id。 |
| delayed save | block 修改后标记 dirty，后台或定期写盘。 |

HelloMine3D 已经有版本化世界/区块格式和 K1-K3 的事务发布、备份恢复合同，不需要改用
FlatBuffers/Protobuf 或 MiniGame 的格式。下面的简单布局只保留为历史设计起点：

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

MiniGame 里有 Windows、Android、Apple、D3D9、OpenGL、平台 SDK 等痕迹。对 HelloMine3D 的
Windows/macOS 兼容工作，参考点主要是边界意识：

| 方向 | HelloMine3D 建议 |
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
| D3D9 渲染后端 | 已过时，且当前目标是 Ogre GL3Plus 的 Windows/macOS 兼容。 |
| 平台 SDK 目录 | 4399、QQ、Steam、WeTest、GVoice 等和当前目标无关。 |
| 大型 Lua/ToLua 桥接 | 当前没有复杂 UI/任务/活动系统，提前引入收益低。 |
| RakNet 网络层 | 多人不是当前核心目标，且库选择需要重新评估。 |
| 旧 Visual Studio/Android.mk 构建 | HelloMine3D 已使用 Premake，更干净。 |
| bin 下完整资源和 exe/dll | 版权、体量和依赖都不适合迁移。 |
| 超大 `BlockMaterial` 虚函数接口 | 思路可借鉴，接口应按当前项目规模重做。 |

## Stage 9 定向参考映射

本节只服务 `docs/archive/beta-gameplay-roadmap.md` 的 N7-N12。参考仓库展示的是系统扩大后的真实边界
和耦合代价，不是目标体量；每批先看 HelloMine3D 当前实现，再用下表核对生命周期、失败语义
和明确不带入的复杂度。

| 批次 | 定向入口 | 只借鉴什么 | 明确不带入什么 | 对当前计划的结论 |
| ---- | -------- | ---------- | -------------- | ---------------- |
| N7A/N7B 结局与路标胜利 | `sandboxPlay\player\PlayerTaskManager.*`、`sandboxPlay\gamemode\GameMode.*`、`docs\systems\10-gamestage-lifecycle.md`、`12-gameplay-gamemode-cloud.md` | 任务事件、结果阶段、一次性奖励、保存恢复和胜利后继续游玩的边界问题。 | 把任务、背包、UI、脚本、团队、网络和胜负塞进一个 TaskManager/GameMode；新增独立 Victory 应用阶段。 | 目标只做引导；另建持久化结局状态作为世界列表、奖励和恢复的唯一事实来源。首轮守卫战复用现有敌人。 |
| N8A/N8B 战斗与投射物 | `docs\systems\14-ai-system.md`、`sandboxPlay\ai\AIProjectileAttack.*`、`sandboxCore\actors\ClientActorProjectile.*`、`ProjectileFactory.*`、`docs\systems\06-actor-attribute-locomotion.md` | 显式攻击阶段、前摇/冷却、目标失效、投射物所有者、碰撞、寿命、容量和调试快照。 | 多套 AI 后端、Lua/行为树、Boss 巨类、三千行投射物层级、同步/保存投射物、巨型 ActorManager。 | 使用小型版本化战斗档案和显式 FSM；投射物瞬态、有上限，并受每 tick 射线/寻路预算约束。 |
| N9A/N9B 结构与战利品 | `docs\systems\07-terrain-mesh-physics-nav.md`、`sandboxCore\terrgen\EcosysUnit_Dungeons.*`、`EcosysUnit_BonusChest.*`、`EcosysUnit_ShipWrecks.*`、`EcosysUnit_VoxelModel.*` | 结构 footprint、跨区块投影、固定 seed 测试、初始箱子和生成耗时指标。 | `std::rand`、生成期间同步加载相邻区块、专用结构 manager、通用 VOX/模型导入器和大型资源生命周期。 | 继续复用现有 cell hash 与 `StructureBuilder`；结构跟随 terrain 身份，不另设版本；箱子初始化后只认持久库存。 |
| N10 资源经济 | `sandboxPlay\gameplay\mgr\CraftMgr.*`、`sandboxCore\blocks\FurnaceContainer.*`、`docs\systems\05-block-container-material.md` | 制作预览/提交、燃料/输入/输出、暂停和容器生命周期需要覆盖的边界。 | 材料组模糊替换、硬编码背包槽、十二槽/多通道熔炉、温度品质和 UI/背包/规则混合。 | 保持精确配方与现有三槽熔炉，补资源可达性、守恒、净增益循环、库存和保存规模审计。 |
| N11A/N11B 难度与重玩 | `sandboxPlay\gamemode\GameMode.*` 及零散 difficulty 调用仅作反例 | 难度需要统一身份、默认值、固定 tick 生效、世界 metadata 和回归维度。 | GameMode 分支和散落倍率；把无限任务、在线活动或 Boss 系统当作“重玩性”。 | 建立集中、版本化档案；旧世界 Normal；不改变 terrain seed。胜利后重复事件是 P2 伸缩项。 |
| N12A 本地化 | `StringDefCsv` 调用链、`IMiniGameProxy` 语言 JSON 入口、UI View/Model/Ctrl 边界 | 文本资源加载、fallback、视图与状态分离，以及晚加本地化会渗透全代码的教训。 | 数字字符串 ID、Lua 文案层、在线语言资源和整套 XML UI。 | N7A 先建立语义化 key；N12A 完成中英文、字体、长文本、字幕和 Credits。 |
| N12B/N12C 音频 | `miniEngine\OgreMain\sound\OgreSoundSystem.*`、`OgreSoundSystemFMod.*` | 可控制声音句柄、2D/3D、listener、分类音量、暂停/停止、音乐淡入淡出、dummy backend 和设备失败。 | FMOD 迁移、四音乐通道、实时 DSP、平台资源层、直播/在线音频。 | 正式采样音效与单通道流式音乐分开交付；先改资源 schema/后端，再接资产和许可。 |

跨批约束：

1. MiniGame 的 Manager/Singleton、脚本和网络生命周期只能用于暴露风险，不能成为新架构模板。
2. HelloMine3D 已有固定 tick、确定性结构、事务保存、注册表和无渲染测试优先于参考仓库做法。
3. 任何源文件、资源、第三方库、配置和生成产物都不得从 `F:\env1_trunk` 复制进当前仓库。
4. 每批只阅读上表直接相关入口；发现参考实现体量明显超出合同，应记录为后置项而不是扩大批次。

## 可参考点清单（历史映射）

下面保留最初的 35 个参考点。很多项目已经完成；真实落地状态以本文顶部的当前对照和
`docs/current/todolist.md` 为准，不能把本表直接转换成 backlog。

| 编号 | 可参考点 | MiniGame 位置 | HelloMine3D 落地建议 |
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
| 29 | resource/editor tools | `SceneEditor`、`uieditor`、`BlockModelEditor` | 资源校验工具已完成，不做完整编辑器。 |
| 30 | localization/table data | `bin/res/csvdef` | UI 文案、block/item 数据增加后可表格化。 |
| 31 | renderer backend boundary | `RenderSystem_D3D9`、`RenderSystem_OGL` | 保持游戏逻辑不直接依赖平台后端。 |
| 32 | platform-specific SDK isolation | `lib4399MGSDK`、`qqrailsdk`、`steamsdk` | 平台相关代码必须独立边界，不污染核心。 |
| 33 | mod/custom resource path | `mod`、`OgreModFileManager` | 远期支持自定义资源包。 |
| 34 | runtime config | `iworld.cfg` | 把窗口、资源、渲染配置外置。 |
| 35 | asset preprocess | `resbuild.bat`、`ConvertFile` | 增加 atlas/manifest 生成和校验脚本。 |

## 历史建议迭代路线

以下路线形成于 HelloMine3D 早期，资源检查、dirty/metadata、halo/greedy、光照、存档、
实体和资源包等大部分内容已经完成。它用于解释历史决策，不覆盖当前 Stage 9/N7-N12 顺序。

### 短期

适合在当前 HelloMine3D 上尽快做，风险低、收益明确：

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

优先阅读参考仓库自己维护的总览与系统分解：

1. `F:\env1_trunk\client\miniSandbox\docs\current\architecture.md`
2. `F:\env1_trunk\client\miniSandbox\docs\systems\04-worlddata-chunk-save.md`
3. `F:\env1_trunk\client\miniSandbox\docs\systems\05-block-container-material.md`
4. `F:\env1_trunk\client\miniSandbox\docs\systems\12-gameplay-gamemode-cloud.md`
5. `F:\env1_trunk\client\miniSandbox\docs\systems\13-player-control-state.md`

已经完成的 K4/G2-G5 历史定向入口：

1. K4：`client\miniModule\MiniPlatform\GameWorld\WorldListMgr.h`，只看列表模型。
2. G2：`client\miniSandbox\sandboxPlay\gameplay\mgr\CraftMgr.h` 和
   `client\miniSandboxSDK\sandboxCore\include\blocks\container.h`。
3. G3：`client\miniModule\CsvLoader\ToolDefCsv.h` 和 `BlockMaterial.h` 的硬度/掉落边界。
4. G4：`bin_externel\iworld.cfg` 的字段集合；不要把空的 `GameSettings` 持久化函数当范例。
5. G5：`client\miniEngine\OgreMain\sound\OgreSoundSystem.h` 的接口与 dummy backend。

Stage 9 按批次定向入口：

1. N7：`PlayerTaskManager.*`、`GameMode.*`、`10-gamestage-lifecycle.md`，重点看耦合风险和恢复边界。
2. N8：`14-ai-system.md`、`AIProjectileAttack.*`、`ClientActorProjectile.*`，只提取显式状态、
   生命周期、容量和调试信息。
3. N9：`07-terrain-mesh-physics-nav.md`、`EcosysUnit_Dungeons.*`、`EcosysUnit_ShipWrecks.*`，
   同时记录确定性做法和同步邻区块加载等反模式。
4. N10：`CraftMgr.*`、`FurnaceContainer.*`，只核对守恒与容器生命周期，不扩大设备复杂度。
5. N11：不把 `GameMode` 当难度模板；只用其分支扩张证明集中版本化档案的必要性。
6. N12：`StringDefCsv` 调用链和 `OgreSoundSystem.*`，分别参考文本/音频边界及晚期耦合代价。

要继续深入体素底座时，再按这些文件顺序查看：

### 方块和区块

1. `F:\env1_trunk\client\miniSandboxSDK\sandboxCore\include\worldData\block.h`
2. `F:\env1_trunk\client\miniSandboxSDK\sandboxCore\include\worldData\section.h`
3. `F:\env1_trunk\client\miniSandboxSDK\sandboxCore\include\worldData\chunk.h`
4. `F:\env1_trunk\client\miniSandbox\sandboxCore\worldData\section_mesh.cpp`

### 方块材质和形状

1. `F:\env1_trunk\client\miniSandboxSDK\sandboxCore\include\blocks\BlockMaterial.h`
2. `F:\env1_trunk\client\miniSandboxSDK\sandboxCore\include\blocks\BlockMaterialMgr.h`
3. `F:\env1_trunk\client\miniSandboxSDK\sandboxCore\include\worldMesh\BlockGeom.h`
4. `F:\env1_trunk\client\miniSandboxSDK\sandboxCore\include\worldMesh\BlockMeshVert.h`
5. `F:\env1_trunk\bin_externel\res\blockgeom.xml`

### 地形生成

1. `F:\env1_trunk\client\miniSandboxSDK\sandboxCore\include\terrgen\ChunkGenerator.h`
2. `F:\env1_trunk\client\miniSandboxSDK\sandboxCore\include\terrgen\Ecosystem.h`
3. `F:\env1_trunk\client\miniSandboxSDK\sandboxCore\include\terrgen\EcosysUnit_*.h`
4. `F:\env1_trunk\client\miniSandbox\sandboxCore\terrgen\EcosysUnit_*.cpp`

### 资源系统

1. `F:\env1_trunk\bin_externel\iworld.cfg`
2. `F:\env1_trunk\client\miniShared\res\OgreResourceManager.h`
3. `F:\env1_trunk\client\miniShared\filesystem\OgrePackageFile.h`
4. `F:\env1_trunk\client\miniShared\filesystem\OgrePackageZipFile.h`
5. `F:\env1_trunk\client\miniModule\MiniDeveloper\mod\OgreModFileManager.h`

### UI 和工具

1. `F:\env1_trunk\bin_externel\res\ui\mobile`
2. `F:\env1_trunk\client\miniModule\MiniDeveloper\uieditor`
3. `F:\env1_trunk\client\miniModule\MiniDeveloper\SceneEditor`

## 对 HelloMine3D 的最终建议

`F:\env1_trunk` 展示了体素游戏从 demo 走向商业产品时会自然长出的系统，也展示了账号、
云世界、多人、Lua、平台 SDK、编辑器和多份生成代码叠加后的复杂度代价。HelloMine3D
已经吸收了其中大部分体素底座经验，当前不应继续按旧路线扩基础设施。

接下来的正确用法是按 N7-N12 的具体合同定向查阅：N7 把 Task/GameMode 当作防耦合案例，
N8 提取显式 AI 状态和瞬态投射物边界，N9 对照确定性结构与反模式，N10 约束制作/熔炉体量，
N11 坚持集中难度档案，N12 只吸收语义化文本资源和可控制音频接口概念。所有实现继续采用
HelloMine3D 自己的小型、renderer-independent、可测试边界。多人、脚本、在线资源、完整
编辑器、通用 Boss/AI 框架和旧引擎代码仍不进入当前范围。
