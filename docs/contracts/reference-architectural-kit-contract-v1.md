# 参考建筑套件合同 v1

范围：用户已启动参考画质完整 Goal 的 B 批次，接续首版 `55234567`。本合同约束正式建筑套件与正常获取路径；工程检查、普通输入、视觉审查和整个 Goal 完成分别记账。

## 身份与预算

保留旧 BlockId 0–34、Material 0–50 及既有 metadata。首版 StoneStep 33／49、StoneWindowFrame 34／50 属于正式套件。追加：

| 部件 | BlockId | Material | stable ID |
| --- | --- | --- | --- |
| 浅石砖 | 35 | 51 | hellomine:stone_brick |
| 浅石半砖 | 36 | 52 | hellomine:stone_slab |
| 浅石檐口 | 37 | 53 | hellomine:stone_cornice |
| 陶瓦台阶 | 38 | 54 | hellomine:clay_tile_step |
| 陶瓦檐口 | 39 | 55 | hellomine:clay_tile_eave |
| 木梁 | 40 | 56 | hellomine:timber_beam |
| 木栏杆 | 41 | 57 | hellomine:timber_railing |
| 石窗台 | 42 | 58 | hellomine:stone_window_sill |
| 石花槽 | 43 | 59 | hellomine:stone_planter |
| 提灯 | 44 | 60 | hellomine:lantern |

十二类均为单格 shape v2，metadata 精确 0–3 表示四向，不使用未定义高位。每格最多 8 盒／384 四边形，每盒界限按 1/8 m，碰撞和选取独立，逐面沿用 0／1／2 材质角色。各朝向几何、UV 和面覆盖为载入时有界缓存。世界仍保存格级 ID＋metadata，save v12 和设置格式不变。

`media/sources/architectural-kit-v1.json` 是可编辑整数格作者源；`tools/export_architectural_kit.py` 确定性导出十二套 `.block`／`.shape`，`--check` 拒绝漂移，`--preview` 可输出同尺度 SVG 作者预览。预览不计游戏画面证据。

## 几何、材质与光

使用 C 批次固定语义：pale_stone tile (0,9)／layer144，terracotta (2,9)／146，timber (3,9)／147，metal (4,9)／148，lamp_glow (5,9)／149；花槽土面使用既有 forest_floor (9,8)／137。旧 Stone／Cobblestone 不改身份、贴图或 metadata。逐盒各面的角色选择这些正式纹理；UV 按真实米尺度连续，不把窄面拉成完整一米纹理。

只有浅石砖可整格遮挡、封层和 AO；部分形状不能借 Opaque 隐藏邻格整面。格边界采用四向 8×8 覆盖缓存，只删除被真实不透明邻面完整覆盖的四边形；部分覆盖保守保留，不引入无界切片。窗框、栏杆的空隙保留真实后方选取。

完整覆盖水平投影的台阶、半砖、屋檐、窗台和花槽底按格阻光，使屋面不会全面漏光；窗框、梁、栏杆和提灯不封整格光。阻光和整格几何遮挡独立。提灯由现有 light=14 传播机制提供真实源；中间灯芯选发光语义，金属部选 metal。C 批次消费只读光源派生索引，不新增灯源持久状态。

世界、碰撞、ray、反馈、手持和掉落消费同一缓存形状。石半砖、石台阶和瓦台阶支持至多 0.5 m 自动登阶，先验证抬升后的完整身体空间。其他部件继续真实碰撞，不强制扩大为整格。旧方块规则保持。World revision、边界 MeshDirty、快照和上传确认保持原有所有权；Ogre 不写世界。

## 正常获取

使用正式工作台原子预览／提交，保留既有配方和发现语义，无拆解逆配方：

- 1 Stone → 1 StoneBrick；3 StoneBrick → 6 StoneSlab；六块阶梯排布 StoneBrick → 8 StoneStep。
- 三块 L 形 StoneBrick → 4 StoneCornice；八块环形 StoneBrick → 4 StoneWindowFrame；1 StoneSlab → 2 StoneWindowSill。
- Furnace：1 Clay → 1 ClayTileStep，80 ticks，消耗既有合法燃料；2 ClayTileStep → 4 ClayTileEave。
- 3 OakPlank → 4 TimberBeam；3 OakPlank＋2 TimberBeam → 4 TimberRailing。
- 2 StoneSlab＋1 ForestFloor → 1 StonePlanter；2 IronIngot＋1 Glass＋1 Torch → 1 Lantern。

原材料分别沿用采集、Stone 冶炼、木板制作、煤铁和玻璃冶炼来源。输出无逆向净增链，制作容量／缺料／陈旧预览拒绝原子，拆除只回收自身一件。学习的 stable recipe IDs 和新增材料沿现有 world save v12 持久化。十二类提供双语语义名称，地图使用正式 ID 和高度概览。

## 验证

必要工程覆盖：作者源导出一致、v1 与旧 metadata、v2 坏输入／预算、十二类四向／UV／真实面覆盖、完整遮挡与部分遮挡负例、拾取/库存身份；正式制作和 Furnace 燃料／材料／产物守恒、正常发现／满库存／缺料、经济图无环与可达；真实 World 放置／ray／身体／灯光／MeshDirty／边界及保存重开。按实际改动执行双配置纯几何与生产源码检查，主线程整合相关客户端／资源／Recipe／World 构建。

普通菜单、工作台制作、四向放置、登阶、空隙选取、拆建、灯编辑、地图和保存重开由整个 Goal 的唯一正常会话验证。工程 fixture 和赠送样板库存不冒充普通获取。C 材质、样板第二栋、当前游戏尺寸原图和复用成本由主线程整合，不由本合同宣称完成。
