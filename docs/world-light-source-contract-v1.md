# 世界光源合同 v1

本文冻结 `P11-0` 的光源数据、方块定义、metadata 驱动发射、局部重光照、兼容边界和验证
范围。`P11-0` 只解决一个玩家问题：**世界里没有玩家可以制造的光**，因此地下、夜晚和
封闭空间无法游玩。本批不加建造材料、不重构或重平衡既有资源经济、不改敌人、不改地形；
新增火把配方造成的最小材料用途变化属于本合同范围。

## 玩家问题

截至 2026-08-28，运行时和资源清单共注册 21 个方块定义，其中只有两个发光；
`media/blocks/` 另有 3 个未注册的孤儿条目，不计入可用方块定义：

| 方块 | `Light` | 说明 |
| ---- | ------- | ---- |
| `Rose` | 14 | L2 光照里程碑遗留的测试值；一朵装饰花接近满亮度，属于缺陷 |
| `WaystoneCore` | 12 | 结构方块，玩家不可制作、不可放置 |

由此产生三个连锁后果：

1. 煤矿石和铁矿石的主要产地在地下，但玩家没有任何照明手段，`CaveGenerator` 生成的洞穴
   实际不可用。
2. 夜晚缺少可持续的探索和建造行为，昼夜循环主要只是变暗。
3. `V10A` 已经实现的方块光合成、四角 AO 和 `SectionMeshInput::getBlockLight()` 缺少内容
   驱动，`V10C`/`V10E` 的暗部层次也缺少对比参照。

## 范围

在范围：

- 新增可制作、可放置的 `Torch` 方块。
- 修正 `Rose` 的错误发射值。
- 把方块发射从"只按 id"扩展为"id + metadata"，并用燃烧中的熔炉作为第一个消费者。
- 发射值变化时的局部重光照触发边界。

非目标（本批明确不做）：

- 灯笼、营火、可燃烧蔓延、火焰伤害、光源熄灭（下雨/入水）和光源燃尽。
- 火把的放置支撑检查、朝向贴附和斜面模型；第一版沿用 `Cross` 十字面片。
- 掉落物、玩家或敌人携带的动态光源。
- 新 shader、新顶点属性、新后处理，以及与 `V10D` 方向阴影的交互。
- 任何 `save`、`terrain`、`settings` 版本变化。

## 数据与资源所有权

### 方块定义

新增 `media/blocks/Torch.block`，沿用现有严格块式格式：

```text
Name
Torch

Id
21

TexAll
6 1

Hardness
0.05

MiningClass
none

WrongToolDrops
1

Opaque
0

Collidable
0

Light
14

MeshType
1

Shape
Cross

ShaderType
2
```

`BlockId` 枚举在 `WaystoneCore = 20` 之后追加 `Torch = 21`，`NUM_TYPES` 变为 22。
**不得在枚举中间插入**，理由见"兼容边界"。

### 图集

`media/materials/Base.terrain-atlas` 增加一条：

```text
torch|6|1|cutout|000000|Torch|火把
```

坐标 `6,1` 当前空闲（第 1 行只占用 `0-5`）。图集布局条目数从 `112` 变为 `113`，必须同步
修改两处常量：`tools/build_fs3_texture_atlas.ps1:90` 与 `tools/validate_terrain_atlas.ps1:94`。
新 tile 必须由生成脚本产出并登记来源/许可证，禁止手改最终 `DefaultPack.png`。

### 材料与配方

`Material::ID` 在末尾追加 `Torch`，`MaterialIds` 追加 `"hellomine:torch"`，图标坐标复用
方块 tile `{6, 1}`，因此本批只新增一张图。

`media/recipes/Base.recipe` 增加：

```text
recipe hellomine:torch shaped
row hellomine:coal_ore
row hellomine:oak_bark
output hellomine:torch 4
end
```

该配方为 1x2 竖向形状，可在玩家 2x2 制作区完成，不需要工作台。当前材料表没有独立煤物品，
所以首版有意直接消耗 `CoalOre` 与 `OakBark` 方块；不得在本批顺带新增煤物品或改写矿石掉落链。
这是玩家取得煤矿石后能立刻使用的第一个成长节点。

### 本地化

`en-US` 与 `zh-CN` 同批新增方块名、物品名和配方书条目，保持严格 key 对齐；缺 key 必须在
启动前失败，不允许回退到英文占位。

## 发射语义

### 当前实现

`World.cpp` 的 `blockEmission(ChunkBlock block)` 已经接收完整 `ChunkBlock`，但只读取
`block.id` 查表：

```cpp
LightLevel blockEmission(ChunkBlock block)
{
    const int value = BlockDatabase::get()
        .getDefinition(static_cast<BlockId>(block.id)).light;
    return clampLightLevel(static_cast<LightLevel>(value));
}
```

`Chunk.cpp` 在区块加载与重建时按同一规则播种 block light。因此发射目前与 metadata 无关。

### 本批扩展

`blockEmission` 改为：先取定义中的 `Light` 作为基础值，再交由方块行为按 metadata 决定
实际发射值。默认行为原样返回基础值，因此**除本批明确修改的 `Rose` 与 `Furnace` 外，所有
现有方块在全部合法 metadata 下的光照输出必须逐字节不变**，这一点由定向断言保证。

熔炉增加唯一的已定义 metadata 位：

```cpp
namespace BlockMetadata {
namespace Furnace {
constexpr BlockMetadata_t LitBit = 0x01;
}
}
```

`FurnaceState` block entity 是冶炼过程的权威状态；`ChunkBlock.metadata` 只是持久化到区块中的
光照/表现投影。发射函数只读取 `LitBit`，其他位保持原值并忽略，以免抢占后续朝向等用途。
不得从 metadata 反向改写燃料、输入、输出、进度或剩余燃烧时间。

熔炉的规则：

| 固定 tick 结束后的熔炉状态 | `LitBit` | 发射值 |
| -------------------------- | -------- | ------ |
| 输入存在有效配方、输出可接收，且 `burnTicksRemaining > 0` | 1 | 13 |
| 其他状态 | 0 | 0 |

“有燃料”以已经消耗并记录在 `burnTicksRemaining` 中的燃烧时间为准，不能要求燃料槽仍有物品。
首次点火的固定 tick 可以消费一件燃料并立即进入亮起状态；输入取空或输出变满后，在下一固定
tick 投影为熄灭，但保留现有实现冻结的剩余燃烧时间。重新具备加工条件后，若剩余燃烧时间大于
0，则在下一固定 tick 重新亮起，不重复消耗燃料。

熔炉不新增贴图。本批交付的是"燃烧时房间会亮"这一反馈本身；发光正面贴图属于 `P11B` 的
动作反馈范围。

### 重光照触发

熔炉的 `LitBit` 在固定 tick 上翻转时，必须触发一次局部重光照，复用 L3 已有的边界
（`caseLocalRelightAfterEdits`）。约束：

- 先持久化本 tick 的 `FurnaceState`，再把按上表计算出的 `LitBit` 投影到同位置 `ChunkBlock`；
  只有 bit **真正翻转**才发起重光照，持续燃烧期间不得每 tick 重光照。
- 燃料耗尽、输入取空或输出槽满导致的熄灭，以及恢复加工条件后的重新亮起，均按固定 tick
  产生一次且仅一次重光照。
- 世界暂停冻结 simulation、block entity 和 metadata；暂停/恢复本身不改变亮度，也不发起
  重光照。暂停界面仍显示暂停前的世界照明。
- 区块卸载不改 `FurnaceState` 或 metadata，也不计作熔炉状态翻转；光照移除与重新接边由现有
  `reconcileBlockLightAfterChunkUnload/Load` 边界负责。
- 旧 v11 区块中的熔炉 metadata 默认为 0，但 block entity 可能保存了剩余燃烧时间。首次加载时
  必须在光照接边前按权威 `FurnaceState` 规范化 `LitBit`，或执行一次等价的有界规范化重光照；
  不修改 block entity payload 版本。
- 重光照失败不得阻止冶炼推进、保存、备份或退出；失败按现有诊断路径记录，不升级为致命错误。
- 触发必须是幂等的：同一 tick 内多次状态查询只产生一次重光照请求。

## 兼容边界

**不升任何持久化版本。** `save` 保持 v11，`terrain` 保持 v3，`settings` 保持 v6。

两条硬约束：

1. **`Material::ID` 只能在末尾追加。** 库存以数值枚举值持久化并按
   `Material::ID::Count` 校验（`WorldSave.cpp:482-492`、`WorldSave.cpp:663-667`）。在中间
   插入会静默改写所有旧存档的物品身份。
2. **`BlockId` 只能在末尾追加。** 区块以数值 id 存储；旧存档中不存在 id 21，加载后自然
   不含火把，无需字段迁移，但仍必须保留旧 v11 兼容 fixture。

`Rose` 的发射修正不需要迁移：block light 不是持久化数据，`Chunk.cpp` 在加载时按定义重建。
旧世界重开后玫瑰周围自然变暗，必须有一条断言证明重建后 `Rose` 邻域 block light 为 0。

## 失败与边界语义

- 方块定义中 `Light` 超出 `[0, 15]` 在解析期严格失败并给出可读诊断，不静默 clamp。
- 火把可以放置在任何现有放置规则允许的位置；第一版不引入支撑面检查，也不因支撑方块被
  破坏而掉落。此限制是显式取舍，不是遗漏。
- 火把可以放置在水中且不熄灭。熄灭机制属于非目标。
- 火把不可作为燃料，不参与冶炼，不进入资源经济守恒的新回路；`ResourceEconomyVerifier`
  只需确认它可达且不产生净增益环。
- 破坏火把必定掉落自身一件，与工具等级无关。

## 自动验证

合入前最低证据：

| 类别 | 必须覆盖 |
| ---- | -------- |
| 光照 | 放置后发射为 14 并逐格衰减；破坏后邻域回到原值；跨 section 与跨 chunk 传播一致；不透明方块阻断；与天空光合成后取较强者 |
| metadata 发射 | 熔炉点火、加工受阻/恢复和燃料耗尽各按 `LitBit` 真正翻转触发一次且仅一次重光照；持续燃烧不重复触发；暂停保持亮度且无重光照；卸载不改状态、重载正确接边；旧 v11 活跃熔炉规范化正确；除 `Rose`/`Furnace` 外所有方块发射输出与本批之前逐字节一致 |
| `Rose` 修正 | 新世界与旧存档重开后 `Rose` 邻域 block light 均为 0 |
| 配方 | 2x2 可制作、产出 4、材料守恒、满背包原子拒绝、连点版本一致 |
| 存档 | 放置火把后保存/重开/备份恢复，位置与库存守恒；旧 v11 存档加载不出现未知 id |
| 资源 | manifest 条目 +1、图集布局 113、资源包覆盖与缺失负例、双语 key 对齐 |
| 构建 | VS2017/v141 Debug/Release 受影响目标零错误，`WorldRuntimeSmoke`、`RecipeSmoke`、`ResourcePackSmoke` 全通过 |
| 性能 | 快速流送与规模玩法 Q1 同身份比较。本批不改顶点布局（stride 保持 32 字节），预期无退化；洞穴场景光照传播开销必须记录 |

开发者视觉记录（`tools/validate_developer_visual_record.ps1`，`-RequirePass`）至少四张
Release 原图：洞穴内放置火把前、放置后、夜晚地表火把照明、熔炉燃烧时的房间亮度。

## 退出条件

1. 玩家能在取得煤矿石与原木后，不查阅文档、只通过 2x2 制作区做出火把。
2. 放置火把后洞穴可正常游玩，破坏后照明按预期消失。
3. 熔炉燃烧时房间变亮、熄灭时恢复，且不产生每 tick 重光照。
4. `Rose` 不再发光，旧存档重开后表现一致。
5. 上表全部自动证据通过，四张开发者视觉记录为 `PASS`。
6. 未新增任何持久化版本；`Material::ID` 与 `BlockId` 均为末尾追加。
