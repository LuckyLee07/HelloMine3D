# P11-1 最小建造集与工具分化合同 v1

> Architecture Lab 迁移说明（2026-08-31）：工程合同保持冻结；建造、工具选择、放置/开门和
> 可见性由 `docs/current/ai-assisted-gameplay-acceptance-v1.md` 覆盖。人类建造乐趣和主观速度手感为
> `NOT_CLAIMED`。

本文冻结 P11-1 的方块、材料、配方、工具职责、兼容边界和验收口径。本批只补足“建一个可关闭
落脚点”所需的最小内容，并让木质、土质与石质方块拥有不同的工具选择；不扩张为家具、装饰、
多方块门、台阶/楼梯、工具 tier 或建造辅助系统。

## 内容与交互

- 1 个橡木原木可无序制作 4 个橡木木板；6 个木板按 2×3 制作 1 扇橡木门。
- 石头继续要求镐且不允许错误工具掉落，但权威掉落改为圆石；圆石可用 80 tick 冶炼回石头。
  石镐、石剑、熔炉和强化熔炉的石质输入同步改为圆石，使采集与加工形成闭环。
- 橡木门使用单方块资源 shape。关闭态可碰撞，打开态不可碰撞；对任一状态执行 `use` 都切换到
  另一状态，破坏两种状态都只掉落同一个 `hellomine:oak_door` 物品。
- `axe` 负责橡木原木、木板和门；`shovel` 负责草、泥土和沙；`pickaxe` 的既有速度、tier、
  耐久、攻击和触及数值不变。
- 木斧为 tier 1、速度 3、耐久 16；木铲为 tier 1、速度 4、耐久 16。两者只在匹配方块类别时
  获得加速，错误类别与空手保持既有速度语义。

## 身份与兼容

所有持久身份只在末尾追加：

| 类型 | 追加身份 | 结果 |
| ---- | -------- | ---- |
| `BlockId` | `OakPlank=22`、`Cobblestone=23`、`OakDoorClosed=24`、`OakDoorOpen=25` | `NUM_TYPES=26`；0-21 完全不变 |
| `Material::ID` | `OakPlank=35`、`Cobblestone=36`、`OakDoor=37`、`WoodenAxe=38`、`WoodenShovel=39` | `Count=40`；0-34 完全不变 |
| 方块/物品语义 | `hellomine:oak_plank`、`hellomine:cobblestone`、`hellomine:oak_door`、`hellomine:wooden_axe`、`hellomine:wooden_shovel` | 严格双向映射；门的两个方块态映射同一物品 |

world save 保持 v11、terrain 保持 v3、settings 保持 v8。旧存档不迁移、不重生成，也不会把旧
数值解释为新内容；新物品、木板以及打开的门已通过保存、退出、恢复和再次交互回归。

## 数据、图集与孤儿清理

- `Base.terrain-atlas` 从 113 增至 117 个语义 tile：圆石 `(7,1)`、橡木门 `(8,1)`、木斧
  `(15,2)`、木铲 `(15,3)`；既有木板 `(5,1)` 正式接入方块和物品。
- 最终 `DefaultPack.png` 只由 `tools/build_fs3_texture_atlas.ps1` 确定性生成。新增 tile 使用项目
  现有像素颜色和脚本绘制，不引入第三方素材或额外许可证。
- 原孤儿 `OakPlank.block` 已正式注册；无扩展名且格式损坏的 `CobbleStone` 由正式
  `Cobblestone.block` 替代；没有本批用途的 `OakSapling.block` 删除。资源清单只登记有效文件。
- 基础配方为 24 条、冶炼为 4 条、工具为 8 项；资源经济验证把 Stone 的地形来源改为
  `terrain.stone_drop -> Cobblestone`，并覆盖新增材料的可达性、引用完整性和无环约束。

## 失败与非目标

- 方块/材料重复 id、缺失映射、未知 mining class、配方不可达或经济环仍在加载或测试阶段失败，
  不以默认值静默替代。
- 门切换只修改目标方块并发布既有 `BlockChangedEvent`；不创建多方块所有权、方向 metadata、
  邻接更新或新的保存 payload。
- 本批不加入家具、装饰方块、木材变体、台阶、楼梯、双格门、涂色、旋转预览、批量铺设、
  修理、附魔和新工具 tier。

## 自动验证、AI 场景与不声明边界

`HELLOMINE3D_WORLD_SMOKE_FOCUS=P11-1` 覆盖双语/图集、末尾追加身份、门的 shape/碰撞/use、
斧铲职责、Stone→Cobblestone 权威掉落、木板/门放置，以及新物品和打开门的 v11 保存恢复。
`RecipeSmoke` 覆盖 24 条配方、4 条冶炼、8 项工具、旧 id 冻结、新配方和圆石闭环；图集验证覆盖
117 项语义和确定性重建，资源包验证覆盖最终 manifest。

2026-08-30 的 VS2017/v141 Debug/Release 客户端及受影响目标均零错误编译；Debug/Release P11-1
定向均为 39/39，`RecipeSmoke` 均为 121/121，`ResourcePackSmoke` 均为 80/80，Release 完整
`WorldRuntimeSmoke` 为 794/794，图集合同为 274/274，资源清单为 84 项。

AI 场景 `AI-05` 当前为 `NOT_RUN`：从新世界采木→制板→放门并建出可关闭落脚点；分别以
空手/错误工具/木斧或木铲确认功能速度次序；检查门的画面、碰撞和连续开关。人类建造乐趣和
主观速度手感为 `NOT_CLAIMED`，不能由自动化或 AI 冒充 PASS。
