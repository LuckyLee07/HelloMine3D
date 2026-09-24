# HelloMine3D 验证矩阵

本文把改动类型映射到最低必要验证。H2-H3、Q1-Q3、Stage 9/BETA-RC、Stage 10/VISUAL-RC
和 Stage 11/P11F 的 Windows 自动工程范围已经集中封板。项目现采用
`docs/current/ai-assisted-gameplay-acceptance-v1.md`：自动化证明确定性和工程边界，AI/Computer Use
证明真实窗口中的功能可玩性，AI 视觉检查证明可观察缺陷边界；人类乐趣、审美、舒适度和
物理设备手感为 `NOT_CLAIMED`。

R3 v1、Physical Input v2 和人工产品体验 v1 继续作为历史合同保留，但已 `SUPERSEDED` 为
Architecture Lab 的当前退出门槛。它们的模板保持 `NOT_RUN`，不得由 AI 冒充物理操作者填写
PASS。新的 AI 场景尚未执行时只写 `NOT_RUN`，不写成永久 `Deferred`。

历史运行结果和逐项证据保存在 `docs/archive/project-ledger-2026-08-17.md` 与
`docs/current/runtime-validation.md`。

2026-09-05 TODOLIST Goal 经所有者明确调整：本批先执行 macOS 对应构建、自动测试、干净包和
真实窗口场景，Windows 专属验证后置；以下 Windows 路由保留，但不再阻塞本次 macOS 交付。
必须区分 gmake/Apple clang、Xcode、x86_64/arm64 和 Windows 证据，不能互相冒充。
AI 平台对应规则见 [当前验收规范](ai-assisted-gameplay-acceptance-v1.md)。

## 日常开发最低门槛

按实际改动匹配下表，完成对应合同的必需检查。检查通过后，只有新改动、失败或未解决疑点
才重复或扩大验证；不把完整里程碑门禁应用到每次文字提交。执行/恢复方法见
[AI 工作流](agent-workflow.md)，它不替代本矩阵的验收要求。

| 改动 | 最低必要验证 |
| ---- | ------------ |
| 所有 C++ 改动 | 受影响目标能够编译；运行对应定向自动测试。 |
| 世界、区块、实体或持久化 | 定向自动测试 + `HelloMine3DWorldRuntimeSmoke`。 |
| 冒险地图区域规划 | `bash scripts/verify_adventure_terrain.sh Debug` / `Release`；`HELLOMINE3D_WORLD_SMOKE_FOCUS=ADVENTURE` 生产区块、水柱、植被、正逆加载与默认版本保存重开；v1–v15 T0 生产摘要对照、完整世界回归、双配置客户端及适用视觉/性能。地区覆盖不代替实机效果，参见[区域骨架合同](../contracts/adventure-terrain-v16-contract-v1.md)。 |
| 冒险地图水系 | `bash scripts/verify_adventure_water.sh Debug` / `Release`；`HELLOMINE3D_WORLD_SMOKE_FOCUS=ADVENTURE_WATER` 检查八 seed 河湖实际块、水柱、岸坡树冠、正逆生成及冻结 v17 保存重开；v1–v16 T0 生产摘要、完整世界回归、双配置客户端与冻结干岸实机。最终配对性能随生态整合执行，见[水系合同](../contracts/adventure-water-v17-contract-v1.md)。 |
| 斜向岸坡网格 | `HELLOMINE3D_WORLD_SMOKE_FOCUS=BANK_MESH`：正负坐标阶梯、AO 开关、实际湖岸三角形平面／朝向与暴露面积；结合固定机位图形对照。几何检查不代替岸坡美术判断。 |
| 冒险生态材料与树种数据 | `HELLOMINE3D_WORLD_SMOKE_FOCUS=ADVENTURE_MATERIAL` 检查末尾追加 ID、工具／掉落、真实采集放置、12 个网格材质槽及 metadata 保存重开；资源包与配方回归、图集校验和标准／兼容材质展台。新生成与生态路线另按 [v18 合同](../contracts/adventure-ecology-v18-contract-v1.md) 验收。 |
| 冒险生态实际生成 | `bash scripts/verify_adventure_ecology.sh Debug` / `Release`；`HELLOMINE3D_WORLD_SMOKE_FOCUS=ADVENTURE_ECOLOGY` 检查八 seed × 九区域实际地表、水柱、树型、地被、正逆生成、碎片 metadata 和默认 v18 保存；v1–v17 T0 摘要对照、完整世界回归及双配置客户端。预冻结机位与连续路线分开验收，最终性能和普通探索仍按 [v18 合同](../contracts/adventure-ecology-v18-contract-v1.md) 与目标执行。 |
| 冒险地图探索整合 | `HELLOMINE3D_WORLD_SMOKE_FOCUS=ADVENTURE_EXPLORE`：八 seed 普通出生、实际木石资源、有界陆路连接区域核心／水岸／三类地点、洞口植被避让、雪层保持、自定义 Cross 回退、v19 保存重开；完整世界回归、v1–v18 生产摘要、双配置客户端。陆路查询与正常输入分开，见 [v19 合同](../contracts/adventure-exploration-v19-contract-v1.md)。 |
| 载入/卸载边界方块光优化 | `LIGHT_BOUNDARY` 覆盖光源/遮挡、跨区块编辑、完整邻区块光场在光源卸载后归零、九区块正逆加载及 detached commit 的完整光场、熔炉燃烧状态；完整 WorldRuntime 和双配置客户端。不得减少真实亮度、邻区块范围或世界更新预算。 |
| 封闭网格输入跳过 | `MESH_INPUT` 覆盖完整 halo／编辑 revision、默认完整快照、封闭输入零生态查询、开洞后复用输入与网格重建、同步与异步跳过；完整 WorldRuntime 和双配置客户端。输入省略不能改变任何可见层或状态机。 |
| 冒险地表纯查询缓存 | `ADVENTURE_QUERY`：八 seed／v17–v19 与无缓存规划逐列一致，负坐标／整数极限、超容量替换与交替世界身份、共享生成器并发；完整 WorldRuntime、v17–v19 生产摘要和双配置客户端。不得缓存实际方块或改变生成版本。 |
| 物品、容器、制作、工具或食物 | 状态守恒、容量边界、失败原子性、固定 tick 和保存/重载测试。 |
| UI 或输入 | 动作仲裁、焦点隔离、映射/冲突、设置迁移自动测试；macOS Cocoa 改动在已构建对应配置后运行 `bash scripts/verify_cocoa_input.sh Debug` / `Release`（非可见自动回归）；适用时运行 `AI-01..AI-04`。OS 焦点、Alt+Tab、最小化和窗口关闭只能由 Computer Use 关闭功能范围。 |
| 第一人称动作、命中/受击、粒子或镜头反馈 | 判定时刻与表现解耦测试、数量/持续时间上限、关闭回退、HUD/准星截图和 AI 多帧/视频观察；镜头效果必须可调或可关，人类舒适度不声明。 |
| 第一人称手臂与抓握表现 | `bash scripts/verify_player_hand_presentation.sh Debug` / `Release`；受影响客户端双配置、P11A/P11B/ITEM_VISUAL 定向回归、空手/图标/方块及昼夜/窄窗口/面板隐藏截图、三档反馈连续帧和相关三轮性能。诊断采集夹具不替代正常输入。 |
| 生物关节与步态表现 | `bash scripts/verify_enemy_articulation.sh Debug` / `Release`；方向/传送/固定关节/头口连接、颈部截面体积、30/60/120 fps 姿态过渡、暂停/死亡复位、真实展示比例/时序与故障负例，客户端双配置和 P11E 回归，静态前后图、具有不同时间点的连续行走/战斗姿态、相关三轮性能。新增部件还需检查 GPU 角色编号、昼夜及回退。诊断展示不代替正常战斗。 |
| 生物表面与面部表现 | `tools/validate_actor_shader_macos.cpp` 运行生产普通/阴影 shader，检查原型分区、局部坐标、夜间提示、雾遮挡和关闭回退；旧 shader 负例、双配置客户端/资源检查、昼夜/中远距离连续画面及受影响三轮性能。正常战斗与人类审美单独记录。 |
| 投射物模型与朝向 | `bash scripts/verify_projectile_presentation.sh Debug` / `Release` 检查封闭外向网格、体积边界、速度正交基和非法半径；双配置客户端与 `PROJECTILE` 定向行为回归、生产生物 shader GPU 检查及旧实现负例，近/中/远、昼夜、关闭回退和连续帧，相关三轮性能。诊断快照不替代正常战斗，未改 World 步进时不声明运动插值改善。 |
| 方块选中表面、裂纹与世界碎屑 | P11B 定向与完整 WorldRuntime、模型/metadata/tile 映射、GPU 透明遮罩/阶段/遮挡检查、标准与兼容干净包多帧、取消与 Off 回退、相关性能；诊断夹具与正常输入分开记录。见[补充合同](../contracts/block-feedback-contract-v2.md)。 |
| 湿地草丛区域模型 | `bash scripts/verify_wetland_grass.sh Debug` / `Release`；双配置客户端、`WETLAND_GRASS` / `P11B`、WorldRuntime 与 MeshDirty，检查真实区块和反馈几何/材质/风摆一致、成熟状态、单格边界及自定义资源形状回退。`tools/validate_flora_shader_macos.cpp` 检查生产 GPU 的根部、茎穗接点、普通/阴影一致及故障负例；多 seed 昼夜/兼容画面、可见连续风摆和 Off/High 常驻/流送三轮性能。GPU 编译需 `-Isrc/external/glm`，完整命令见[本批记录](../reports/wetland-grass-presentation-r29-2026-09-19.md)。 |
| 目标、配方发现、探索奖励或资源经济 | 主线可达、输入输出守恒、重复奖励/一次性领取、保存重载和全部受影响迁移；脚本化 AI 记录可执行流程，无上下文 AI 盲玩只提供可理解性代理。 |
| 冒险前期目标定义迁移 | `ADVENTURE_PROGRESS` 双配置：早期食物事件、无重开熔炉、可选历史、Alpha 十位兼容、v1／v2／v3 完成／进度／配方发现迁移；资源解析与 Release 完整 WorldRuntime。B2b 同入口补当前库存／生命／冷却、三项优先级、世界时钟及保存恢复、双语改键／只读查询；双配置客户端编译。普通体验、三字号／紧凑窗口和同机三对性能在 B2 整合检查，不将每个定义用例拆成客户端启动。见[前期引导合同](../contracts/adventure-onboarding-contract-v1.md)。 |
| 资源、配方、声音或 shader | 资源清单/解析验证；缺失和非法引用必须明确失败。水波坐标变化用 `tools/validate_water_shader_macos.cpp` 执行实际 GLSL 的相邻区块位置/法线检查，命令见[水面修复记录](../reports/water-seam-fix-2026-09-12.md)。 |
| terrain atlas 或 HUD/手持图标 | `tools\validate_terrain_atlas.ps1`（暖野 macOS 使用 `python3 tools/validate_warm_texture_atlas.py`，含分面/图标映射）、确定性重建、Alpha/空 tile、block 分面、Material 坐标、资源包与隐藏固定截图。 |
| 岩层与沙面地质着色 | 当前冒险地貌去条带使用 `tools/validate_block_feedback_shader_macos.cpp <root> <新目录> <冻结旧条带资源根> --irregular-geology`，检查旧条带负例、相邻像素对比下降但宽变化保留；其余历史模式保留原资源版本归属。 `tools/validate_block_feedback_shader_macos.cpp` 以可选旧包资源根参数比较实际 GLSL，检查 atlas/array、普通/阴影、昼夜、时间不变、区块原点、负坐标接缝、关闭回退、远处细节衰减及旧版/移除衰减负例；双配置资源检查、多 seed 原图与相机连续帧、相关三轮常驻/流送性能。仅改 fragment 时可复用源码身份一致的客户端，不冒充 C++ 重建或正常行进。命令及范围见[地质材质记录](../reports/geology-materials-r30-2026-09-19.md)。 |
| 生态植被颜色过渡 | `bash scripts/verify_terrain_ecology_colour.sh Debug` / `Release` 检查有界查询、跨区块/负坐标连续和极值；`V10B3` 真实快照及 greedy 内部颜色重建、`WETLAND_GRASS` 反馈/风摆和完整 WorldRuntime；双配置客户端/资源、生产 GPU 四路径及旧 shader/取消滤波/错误合并负例，配对原图、连续帧和相关三轮帧耗/网格规模。见[本批记录](../reports/ecology-colour-transitions-r33-2026-09-20.md)。 |
| 顶点格式、网格、光照或 AO | 确定性角落/边界夹具、MeshDirty、隐藏固定截图、既有 schema 3 顶点/索引/构建/驻留字段的补充比较和相关 Q1。仅改每顶点值时不得误报为顶点格式升级。 |
| 同列 section 合并绘制 | `RENDER_BATCH` 双配置聚焦：全部属性／世界坐标／索引保持、负坐标与整数极值、非法和空数据、替换／移除、透明／多 pass／自定义程序回退；完整 WorldRuntime、双配置客户端、真实改块／水岸／林地／雪山／阴影及兼容路径多帧，三轮静止与流送性能。合并不改变实际几何量，驻留对象数不能当作 draw 次数。 |
| 雾、天空、云、阴影或后处理 | shader 正反例、关闭回退、固定昼夜截图、窗口缩放/切世界清理、各图形档性能和 AI/开发者视觉检查；动态项必须用多帧、视频或连续窗口。 |
| 冒险前期危险与种植 | `ADVENTURE_SURVIVAL` 双配置：夜间／跨日、候选距离、实际光照与树顶拒绝、加载数不变、三种子自然种子与土层、无战斗领域种植；D3／D5／D6／G6、阶段指标、难度与食物，Release 完整世界，双配置客户端／资源。正常输入及同负荷性能在 B2 整合，详见[前期引导合同](../contracts/adventure-onboarding-contract-v1.md)。 |
| 冒险动物生态 | `ADVENTURE_WILDLIFE` 双配置：三生态自然出现、真实移动／停留／觅食／受惊、四角贴地避水与查询预算、原敌人 D3、区块卸载／重开再生、击杀无掉落且不计敌人目标；Release 完整世界、双配置客户端。B3b 同入口检查三类有界模型与状态动作，`tools/validate_actor_shader_macos.cpp` 验证生产普通／阴影 GPU、像素标记、关闭回退、雾和原生物／投射物回归，双配置资源。三生态普通画面、密集流送与配对性能集中实机验收，详见[冒险动物合同](../contracts/adventure-wildlife-contract-v1.md)。 |
| 冒险目的地布局 | `ADVENTURE_LANDMARK` 双配置：terrain v20 六布局在实际种子选中、v19 候选及奖励不变、生产区块逐格与蓝图一致、正反区块顺序、入口与矿物／核心／箱子、箱子 payload、v20 创建编辑重开；同入口保留 E8 v14 回归。Release 完整世界、v1–v19 保存／生成兼容及客户端双配置；三类普通进入、接近线索、区域建造材料和三对性能在 B4 整合。见[目的地合同](../contracts/adventure-landmarks-v20-contract-v1.md)。 |
| 冒险区域建造配方 | `ADVENTURE_MATERIAL` 双配置验证区域地表采集／放置及林地储物、河岸窑炉、高地破碎三套真实工作台制作／放置／保存重开；`HelloMine3DRecipeSmoke` 复核旧配方、经济守恒及新配方，双配置资源包与 `check_assets.sh`。普通地区采集、配方发现、地点建筑示例及带回基地使用在 B4 集中实机验收，见[区域建造配方合同](../contracts/adventure-regional-build-recipes-contract-v1.md)。 |
| 冒险目的地接近线索 | `ADVENTURE_APPROACH` 双配置检查 terrain v21 在冻结种子真实林地／河岸／高地地点的地面、净空、侧边标记、正反区块顺序、v20 建筑与奖励不变、v21 编辑保存重开；`ADVENTURE_LANDMARK` 复核旧 v20 身份，Release 完整世界和双配置客户端编译。普通发现／进入、画面及三对性能在 B4 集中实机验收，见[接近线索合同](../contracts/adventure-landmark-approach-v21-contract-v1.md)。 |
| 冒险区域工位 | `ADVENTURE_WORKSHOP` 双配置检查 terrain v22 三个冻结区域的真实地点、3×3 建筑材料、净空、箱子／熔炉／破碎机空实体及使用、v21 建筑与奖励、正反区块顺序、默认 v22 保存及拆除不重生；旧 v21／v20 聚焦和 Release 完整世界回归、双配置客户端编译。普通探索／建造、画面与同机三对性能仍由 B4 集中验收，见[区域工位合同](../contracts/adventure-regional-workshops-v22-contract-v1.md)。 |
| 持久探索地图 | B5a／B5b 运行 `bash scripts/verify_exploration_atlas.sh` 双配置，覆盖未知、已知空列、坐标极值、刷新、满额保持历史、文件身份／校验、事务故障和损坏回退；B5c 同脚本检查 UTF-8、稳定 ID、单一基地／追踪、修改删除和容量、北向八方位与极值距离、已知任务锚点阶段门槛，以及文件 v1/v2/v3→v4 读取、标记／发现与任务绑定分别往返、错世界隔离和篡改负例。缩放总览另检查已知格汇总、代表样本坐标、页接缝与边界输入。备份／恢复运行 `HelloMine3DWorldBackupSmoke`，世界运行用 `EXPLORATION_MAP` 双配置检查真实采样、远区拒绝、World 标记命令、真实放置／拆除、多界石导航、远区保存重开、损坏与错世界隔离、隔离失败／保存重试／满额状态，并编译双配置客户端、运行 Release 完整世界回归。平面图与标记页工程检查双配置客户端、资源包、`check_assets.sh` 及双语键引用，不凭无窗口编译宣称界面可用；集中窗口核查平面／立体切换、缩放取样与鼠标标记、目的地真值、双语与两窗口、正常返回路线及同机三对性能。存储接线通过不代表 B5 完成，见[探索地图合同](../contracts/adventure-exploration-map-contract-v1.md)。 |
| 冒险天空日月与遮挡 | `tools/validate_sky_shader_macos.cpp` 文件头构建后以 `<候选 shader> <冻结旧 shader> <新输出目录> --celestial` 执行生产 GPU：日月特征、背面、正午连续、云遮挡、旧版及移除遮挡负例、FS2 逐像素保持及既有云动态；双配置资源检查、FOV 90／120 昼夜实景、三档各三对性能。B1a 不关闭后续云形／区域氛围／阴影范围，见[冒险天空合同](../contracts/adventure-sky-contract-v1.md)。 |
| 冒险天空云团 | 同一 GPU 工具以 `--cloud-form` 检查 B1b 云形变化、清晰边缘与留白、正负坐标风移不变性及停止风移负例；保留日月／遮挡、60 Hz 漂移、云层穿越和 FS2 原版回退检查。林地／海岸／雪山、两 FOV 日月、连续诊断和适用三对性能，普通输入另记。B1d 用 `--natural-sky` 另检查圆形日月及错误方牌负例。B1e 用 `--atmospheric-clouds` 验证清空／薄缘／浓密层次、硬剪影与整幕雾负例；云高变化补双配置 `V10C`、资源与客户端编译。先用同一 GPU 进程出低地／雪山／高处探针图，再以一个独立工作客户端会话集中检查实景；历史硬边界指标不作为薄云的美术要求。 |
| 阴影稳定性 | `bash scripts/verify_directional_shadow.sh Debug` / `Release` 检查正午参考轴、光源平面纹素锚定与负坐标；生产滤波 `tools/validate_shadow_filter_macos.cpp` 的 GPU 连续性/遮挡/回退和旧版负例，完整 terrain/actor shader 回归、资源接口陈旧覆盖负例、客户端双配置、昼夜/移动多帧与各受影响图形档三轮性能。有限采样不声明全部场景零闪烁。 |
| 构建图或平台代码 | 当前批准平台的工程生成和受影响目标编译；macOS 只有被当批列入范围时才要求新原生证据。 |
| POSIX 渲染计时 | `bash scripts/verify_posix_timer.sh Debug` / `Release` 以实际 Timer 源码隔离注入墙钟回拨、前跳和暂停；不改系统时间。重建受影响 Ogre 依赖及客户端，再核查隐藏客户端的真实帧增量、模拟 tick 和正常配置恢复；短采样不代替三轮性能门槛。 |
| 隐藏诊断相机 | `bash scripts/verify_visual_camera_sweep.sh Debug` / `Release` 检查开关隔离、有限参数、位移/角度/时长边界与连续性、有界中间高程／端点一致／非法路径拒绝；客户端双配置构建、实际启动负例、开关关闭路径及原始连续帧。相机观察不证明玩家移动、碰撞、小地图位置刷新或正常玩法。 |
| 水线表层过渡 | `tools/validate_water_shader_macos.cpp` 执行生产 GLSL，覆盖浅/深水、细节开启/关闭共 8,004 个跨水线采样、合成颜色连续性与近裁剪覆盖；冻结旧 shader 反例、同机位连续帧及相关三轮性能。只改 fragment 时复用身份匹配的客户端，资源/实际 GPU 检查不冒充 C++ 重建或正常游泳验收。 |
| 相机水介质与远景衔接 | `V10C` 定向检查顶层水块内的深度过渡、表面/块边界连续性及昼夜雾色与地平线一致；双配置客户端构建，同机位水下/出水连续帧、空气及大气关闭回退、相关三轮性能。基础水介质独立于增强大气开关，关闭大气时仍检查昼夜水下与干燥岸边，保留云层/水面细节回退。端点亮带消失不能替代穿越过程检查。 |
| 冒险区域氛围 | `tools/validate_regional_atmosphere.cpp` 双配置纯检查采样预算／缓存／坐标／区域连续／权威与水介质，双配置客户端及 `V10C`；八生态与林地海岸雪山四时段、FOV 90／120、关闭回退、动态和生命周期检查，三档常驻／流送三对性能。见[冒险天空合同](../contracts/adventure-sky-contract-v1.md)。 |
| `AL-A0` 纯文档基线 | 逐项对照实际源码冻结模块/API/ownership/tick/snapshot；`git diff --check`、本地 Markdown 引用、World→Ogre 反向依赖检查和 VS2017 完整门禁。运行时代码/身份未变时引用既有正式 Q1/Q3，不重跑 1800 秒；无 OS Computer Use 时 `AI-08=NOT_RUN`。 |
| `AL-A1` World 责任地图 | `tools\validate_world_responsibility_map.ps1` 必须覆盖全部公开方法、匹配 public-surface hash 且无 stale/重复行；随后运行 VS2017 完整门禁。没有运行时行为变化时引用既有正式 Q1/Q3，`AI-01..AI-08` 保持 `NOT_RUN`。 |
| `AL-A2` Chunk Runtime 边界 | `tools\validate_chunk_runtime_boundary.ps1` + AL-A1 公开面门禁；VS2017/v141 Debug/Release 完整门禁；WorldRuntime 的 S0.5/M2/M6/M7/E5/S2.4 与 loader stress 必须通过。禁止新增 Residency 状态、改变 save/unload 转换或修改既有预算；AI 场景未执行时保持 `NOT_RUN`。 |
| `AL-A3` Simulation Runtime 边界 | `tools\validate_world_simulation_boundary.ps1` + AL-A1/A2 边界门禁；`HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A3` 和完整 WorldRuntime；VS2017/v141 Debug/Release 完整门禁。必须保持 8 phase 顺序、20 Hz context、caller-owned pause 和确定性；后续 A5 只能增加观察词汇，仍禁止 Scheduler/Registry 和执行行为变化。 |
| `AL-A4` Event / Command / Query 边界 | `tools\validate_event_command_query_boundary.ps1` + AL-A1/A2/A3 边界门禁；`HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A4` 和完整 WorldRuntime；必须保持 typed command FIFO、immutable fact、订阅者 effect/republish、8 层递归、诊断隔离和查询非 mutation 语义。 |
| `AL-A5` Tick Phase Metrics / Budget 词汇 | `tools\validate_simulation_metrics_boundary.ps1` + AL-A1..A4 边界门禁；`HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A5` 和完整 WorldRuntime；VS2017/v141 Debug/Release 完整门禁。只允许四个真实 metric phase 的 last-tick elapsed/processed/deferred/budget scope/status；不得改变 hard limit、phase 顺序、Gameplay 或引入 Scheduler/Registry/空系统槽。 |
| Track B Core（B1-B6/B10） | 逐批静态 gate 与聚焦 WorldRuntime；B10 额外运行 `HELLOMINE3D_WORLD_SMOKE_FOCUS=B10`、正式 schedule-v3 1800 秒五阶段压力、同 seed 双确定性探针、未放宽 Q1 fast-streaming 和完整 VS2017/v141/隔离包门禁。Lifecycle 资格必须在 8 项 unload budget 前过滤，取消 reservation 不得保留无界 `Absent` 墓碑。 |
| `C1` Block Capability Model | `tools\validate_block_capability_model.ps1` + `HELLOMINE3D_WORLD_SMOKE_FOCUS=C1-CAP` + 完整 WorldRuntime/VS2017 双配置门禁。保留 Chest/Furnace/Crusher 的既有 provider、UI 能力访问和错配/损坏/陈旧句柄失败关闭；C3 只允许 Crusher 增加具体 `MechanicalPort`，仍禁止 Registry 与 Extended 预注册。 |
| `C2` Machine Runtime v0 | `tools\validate_machine_runtime.ps1` + `HELLOMINE3D_WORLD_SMOKE_FOCUS=C2-MACHINE` + Recipe/Resource Pack/terrain atlas + 完整 WorldRuntime/VS2017 双配置门禁。必须保留五态优先级、Furnace 兼容、Crusher 正常 craft/place/Use/crank、槽位/动力/原子完成、break spill、malformed/stale/mismatch、unload/reload、save/reopen 和 economy v2；C3 拓扑不得改变独立手摇或引入动力传播。 |
| `C3` Mechanical Topology Model v0 | `tools\validate_mechanical_topology.ps1` + `HELLOMINE3D_WORLD_SMOKE_FOCUS=C3-TOPOLOGY` + 完整 WorldRuntime/VS2017 双配置门禁。必须覆盖 Crusher-only 六面端口、确定性 component id/canonical edge、merge/split/no-op、正常 place/break、malformed/stale、Chunk unload/reload、save/reopen 派生重建、正常 UI 与 Debug 观察；禁止持久化 topology、C4 power、通用网络、物流和 C4+。 |
| `D1` Simulation Phase Scheduler v0 | `tools\validate_simulation_phase_scheduler.ps1` + `HELLOMINE3D_WORLD_SMOKE_FOCUS=D1-SCHEDULER` + AL-A5/B6/C2/C3 聚焦回归 + 完整 WorldRuntime/VS2017 双配置门禁。保留三类真实 workload、64/4/32 item budget、稳定集合 round-robin/FIFO、单步无 catch-up、mandatory Player/8 phase barrier、copied diagnostics 和 save v12；D2 进入调查不等于已实现 activation。 |

## 完整验证路由

| 验证 | 命令或目标 | 适用改动 |
| ---- | ---------- | -------- |
| Windows 工程生成 | `tools\premake\premake5.exe --os=windows --file=premake/premake.lua vs2017` | 构建系统或文件布局；当前正式工具链是 VS2017/v141 |
| Windows 全量门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_build.ps1 -VisualStudioVersion 2017` | 里程碑封板、跨目标源码或链接变化；无活动桌面时显式加 `-SkipRealWindow`，相关真实窗口结果记 `NOT_RUN` |
| World 公开 API 责任门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_world_responsibility_map.ps1` | `World.h` 公开声明或 `docs/current/architecture.md` 责任地图变化；完整 Windows 门禁也会自动运行 |
| Chunk Runtime 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_chunk_runtime_boundary.ps1` | `World` / `ChunkRuntime` / `ChunkManager` 的队列、worker、预算、mesh commit 或 unload 协调变化；完整 Windows 门禁也会自动运行 |
| Simulation Runtime 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_world_simulation_boundary.ps1` | `World::tick`、`WorldSimulation`、phase/context/raw timing、暂停入口或相关 debug snapshot 变化；完整 Windows 门禁也会自动运行 |
| Event / Command / Query 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_event_command_query_boundary.ps1` | command FIFO、EventBus、生产订阅者、查询或未来 Machine/Network 依赖变化；完整 Windows 门禁也会自动运行 |
| Simulation Metrics 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_simulation_metrics_boundary.ps1` | phase metric identity、processed/deferred/budget scope/status、Actor 计数、开发者 Simulation 面板或相关 snapshot 变化；完整 Windows 门禁也会自动运行 |
| Block Capability 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_block_capability_model.ps1` | BlockDefinition capability 声明、Chest/Furnace 访问适配、容器 UI 分派或未来 C2/C3/Extended 边界；完整 Windows 门禁也会自动运行 |
| Machine Runtime 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_machine_runtime.ps1` | MachineRuntime、Furnace/Crusher adapter、processor capability、Crusher payload/recipe/crank、资源经济 schema 或 C3/网络越界；完整 Windows 门禁也会自动运行 |
| Mechanical Topology 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_mechanical_topology.ps1` | C3 Crusher 节点/端口、确定性连通分量、World/Chunk 同步、能力/UI 观察、save 非持久化或 C4/通用网络越界；完整 Windows 门禁也会自动运行 |
| Simulation Phase Scheduler 门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_simulation_phase_scheduler.ps1` | D1 admission、Actor adapter、Furnace/Crusher 单项执行、调度 snapshot 或 UI；完整 Windows 门禁自动运行 |
| Windows Debug 编译 | `MSBuild build\HelloMine3D.sln /p:Configuration=Debug /p:Platform=x64` | 所有 C++ 改动的主干检查 |
| Windows Release 编译 | 同上，配置改为 `Release` | 里程碑和发行候选 |
| macOS Xcode 门禁 | `bash scripts/verify_xcode.sh` | Xcode 图、macOS 平台或原生封板 |
| macOS gmake 双配置门禁 | `MAKEFLAGS='-j2 -B' bash scripts/verify_build.sh` | 从头编译客户端/依赖和 13 个测试目标，运行 Debug/Release。两配置共用 `bin/` 输出，正式干净门禁保留强制重编译，避免旧配置产物混用；不为普通文字提交触发此门禁。生成 x86_64 macOS 证据，不冒充 Xcode/arm64/Windows。 |
| macOS 启动负例 | `python3 tools/validate_startup_errors_macos.py --output <new-output-dir>` | 与 Windows 共用 10 个缺失和 5 个非法 fixture 定义，要求非零退出、准确资源诊断和 `ui=stderr-only` 报告；Windows MessageBoxW 范围后置 |
| macOS 干净包 | `python3 tools/package_macos_release.py --output <new-app-path>` | 调用者先构建 Release；复制 manifest 资源和 notices、检查动态依赖、记录构建/可执行文件/资源哈希；不会覆盖已有输出，正常窗口验收需另行执行 |
| 十三个 headless 目标 | `scripts\verify_build.ps1` 中列出的测试/Smoke/Soak | 全量里程碑回归 |
| 世界运行冒烟 | `bin\HelloMine3DWorldRuntimeSmoke.exe` | 区块、存档、交互、事件、实体、地形 |
| 渲染截图 | `tools\run_render_capture.ps1` | renderer、shader、texture、mesh、HUD |
| 性能采集 | `tools\run_perf_baseline.ps1`；正式六场景使用 `tools\capture_release_candidate_performance.ps1` | 区块加载、网格、更新、渲染提交 |
| 性能比较 | `tools\compare_perf_baselines.ps1` | 可能改变帧时间或世界驻留的改动 |
| 资产检查 | `bash scripts/check_assets.sh` | 资产和数据 |
| V10B2 图集合同 | `tools\validate_terrain_atlas.ps1` | 37 个语义/双语名、Alpha 边界、分面、HUD/手持一致性与确定性输出 |
| R3 自动预检 | `tools\validate_r3_automated_preflight.ps1 -Configuration Release -Build` | 控制器、交互、容器、战斗、D6 和后台窗口焦点的逻辑回归；只是 AI 交互前置条件。 |
| AI/Computer Use 功能验收 | `docs\current\ai-assisted-gameplay-acceptance-v1.md` 的 `AI-01..AI-08` | 从带哈希的干净 Release 包用正常 OS 输入执行；禁止 fixture、注入、传送、存档编辑和直接 Gameplay API。严格 `AI-06` 还要求仓库不可访问的 package-only 新任务。Windows 首份记录为 `NOT_RUN`；本次 macOS AI-01 为部分通过、总项 `BLOCKED`，见 Goal 记录。 |
| AI 视觉/可读性验收 | 同规范的 `AI-07`，配合原尺寸截图、多帧/视频、连续窗口观察和可访问音频证据 | 可关闭截断、重叠、缺字、破面、闪烁、状态/轮廓可见性和 cue/字幕生命周期；正式证据来自带哈希干净包。`run_render_capture.ps1 -CaptureMs ...` 已支持多帧，但在直接证明发行包可执行文件身份前只作开发预检；不声明人类审美、听感或舒适度。 |
| R3 v1 / Physical Input v2（历史） | `docs\archive\manual-input-acceptance-v1.md`、`docs\archive\physical-input-acceptance-v2.md` 及原校验器 | 历史物理合同 `SUPERSEDED` 为当前门槛，模板保持 `NOT_RUN`；未来自愿运行也必须遵守原物理语义。 |
| 开发者视觉检查（历史/补充） | `docs\archive\manual-product-experience-acceptance-v1.md` A 节与既有 PASS 记录 | 已完成记录继续有效；后续可作为 `DEVELOPER_SELF_TEST` 补充 AI 视觉证据。 |
| 长时间 soak | `tools\run_world_soak.ps1`；正式双 profile 使用 `tools\run_release_candidate_soak.ps1` | 区块/实体生命周期、存档、后台加载 |
| B10 Track B Core 长稳 | `tools\run_large_world_stress_acceptance.ps1 -Formal` + `tools\validate_large_world_stress_acceptance.ps1 -Evidence` | B1-B6 的 demand/job/cancellation/backpressure/spatial-interest 组合变化；精确 36,000 fixed ticks、LW1-LW5、进程/世界界限与确定性摘要 |
| 资源包 | `tools\validate_resource_packs.ps1` | manifest、资源解析、启动预检 |
| 干净发行包 | `tools\package_windows_release.ps1` | 发行包、manifest、资源解析 |
| 世界目录 | `HelloMine3DWorldCatalogueSmoke` | 世界发现、名称、id、目录 |
| 事务保存/恢复 | `HelloMine3DStorageTransactionSmoke`、`HelloMine3DWorldBackupSmoke` | 保存发布、隔离、备份、恢复、格式迁移 |
| 崩溃产物 | `HelloMine3DCrashDiagnosticsSmoke`、`tools\validate_crash_diagnostics.ps1` | 异常处理、dump、sidecar、符号、崩溃 UX |
| 数据竞争 | `bash scripts/verify_tsan.sh` | 后台加载、区块图同步、工作线程调度 |
| 启动/保存/恢复预算 | Q1 schema + Q2 阶段计时 | 目录、启动、进世界、保存、备份、恢复 |
| 内容规模 | `docs/contracts/q3-scale-soak-contract-v1.md` + 正式双 profile soak | 快速移动、人口规模、持久化压力 |
| 完整玩法切片 | G6 自动 fixture + 干净包 `AI-08` | 自动 fixture 证明状态边界，Computer Use 证明正常界面和输入可走通 |

## 验收节奏

| 时机 | 要求 |
| ---- | ---- |
| 功能开发中 | 编译、定向自动测试、数据守恒、存档兼容和必要交互冒烟。 |
| 一个玩法任务完成 | Debug/Release 相关目标、世界冒烟和该任务的失败边界。 |
| G6 集成前 | 清理所有主干构建失败，补齐跨系统回归。 |
| 发布/展示候选 | 当前批准平台门禁、适用 AI 场景、H1-H3、Q1-Q3、长稳、视觉证据和干净包。 |
| Stage 9 单批完成 | 批次合同、定向测试、全部受影响旧版迁移、相关 Q1、必要的 60-120 秒 nominal/stress 和隐藏客户端。 |
| BETA-RC | 历史工程封板保持原证据；当前分类由 AI 验收规范承接，不补写物理/真人 PASS。 |
| Stage 10 单批完成 | 批次合同、VS2017/v141 双配置受影响目标、shader/资源负例、固定 before/after RuntimeReadback、开发者视觉检查和相关 Q1；图形功能必须可关闭。修改 shader/顶点接口时当批补 macOS Release 真实窗口冒烟，否则跨平台状态保持 `Verify`。 |
| Stage 11 单批工程完成 | 历史工程证据保持冻结；适用 AI 场景未执行时写 `Engineering Done / AI NOT_RUN`，主观范围写 `NOT_CLAIMED`。 |
| Architecture Lab Sprint | 受影响自动门禁、数据/迁移、性能、可解释 Debug、游戏内 Demo 和适用 AI 场景定义。 |
| Architecture Lab Track | 完整自动门禁、干净包、新系统压力证据和 `AI-08`；既有主菜单→胜利→保存重开流程必须可完成。 |

## 第 8 阶段批次门禁

| 批次 | 合入前最低证据 | 可后置证据 |
| ---- | -------------- | ---------- |
| `BLD-1` | 重新生成 VS 工程；WorldRuntimeSmoke/Soak 项目同时包含崩溃源和 `dbghelp.lib`；Windows Debug/Release 全量编译及定向目标通过。 | 已于 2026-08-17 通过后台 Release 受控 dump 和干净包。 |
| `K4` | WorldCatalogue、StorageTransaction、WorldBackup 定向测试；创建/重命名/删除恢复的路径与故障边界；一次菜单交互冒烟。 | `AI-01` 正常主菜单与恢复操作。 |
| `G2` | RecipeSmoke；库存/制作状态守恒；预览无副作用；提交失败原子性；关闭、满背包、连点和重载。 | `AI-02/AI-05` UI 与完整流程。 |
| `G3` | 工具表解析；破坏进度状态机；等级/掉落/耐久边界；物品实例存档迁移；固定动作性能采样。 | Q1/Q3 和长稳已完成；功能差异进入 `AI-05`，人类手感不声明。 |
| `G4` | 设置解析/版本/原子写入；应用/取消；暂停模拟；UI 焦点隔离；开发者交互冒烟。 | `AI-01` 窗口、焦点、暂停和设置重启。 |
| `G5` | 音频资产检查；事件去重；音量/pause/mute；dummy backend；无设备和缺失资源降级。 | `AI-07` cue/字幕/生命周期；人类听感 `NOT_CLAIMED`。 |
| `G6` | 干净启动垂直切片 fixture、跨系统保存/恢复、Debug/Release 相关目标、截图和可比较性能采样。 | 干净包 `AI-08` 正常主线。 |
| Alpha 检查点 | 跟踪 v3 迁移 fixture；Debug/Release 世界与崩溃 smoke；双模式隐藏客户端；Alpha 基线/复测；匹配及错误 PDB。 | 后续工程证据已在 RC 关闭；真实窗口功能由 AI 场景承接。 |
| `N3` | 严格食物解析；面包配方；成功/满血/暂停/死亡/冷却/UI 占用的原子语义；生命/冷却保存重载；v5→v6 迁移；目标事件；资源包与启动缺失检查。 | `AI-03/AI-04/AI-08` 证明战斗、恢复和正常旅程；是否引入饥饿仍需独立立项。 |
| 封板 | H1-H3、Q1-Q3、当前批准平台构建、存档故障、资源包、视觉证据、soak、干净包和适用 AI 场景全部通过。 | 人类主观与物理体验保持 `NOT_CLAIMED`，不作为隐藏待办。 |

## Stage 9 / Beta 预排门禁

本节只定义未来批次启动后的最低证据，不表示这些工作已经完成或已经加入当前 77 项总账。

| 批次 | 合入前最低证据 | 阶段结束补充证据 |
| ---- | -------------- | ---------------- |
| `RC0` | `Superseded`：bundle、文档一致性和自动工程基线已由 BETA-RC 覆盖，不再作为活跃批次。 | 历史物理输入合同不再关闭当前功能范围；D2/D4/D6 由 AI 场景承接。 |
| `N7A` | 独立结局状态/奖励 epoch；不以目标耗尽推导胜利；预计 v8→v9 及全部旧版迁移；状态非法值、事务失败、备份恢复；中英文 key/fallback。 | 胜利前中后保存重开，世界列表只从持久状态标记；重复重入不重复发奖。 |
| `N7B` | 激活/守卫状态机；复用现有敌人；actor/事件/驻留上限；死亡、暂停、卸载和遭遇中恢复；相关 Q1 和短时 soak。 | 干净新世界无 debug 注入完成胜利并继续 Playing；进入 `AI-08`。 |
| `N8A` | 前摇/命中/击退/格挡；显式状态转换、目标失效、固定 tick 射线/寻路预算和调试快照。 | 多敌人压力、AI 战斗可读性、内容规模 Q1 和阶段 Q3；人类战斗感不声明。 |
| `N8B` | 战斗档案严格解析；投射物遮挡、寿命/距离/上限，死亡/卸载/保存重载清理；世界冒烟和短时 soak。 | 远程/防御差异清晰，无幽灵投射物、容量突破或重复命中；最终 Q3。 |
| `N9A` | 结构类型/cell/seed/terrain 版本/加载顺序确定性；footprint/间距/覆盖；禁止 `std::rand` 和同步邻区块加载。 | 结构计划快照、跨区块投影、快速流送 Q1 和旧 terrain v1/v2 身份。 |
| `N9B` | 初始战利品快照；箱子库存持久后不再初始化；玩家修改不被覆盖；资源/actor/结构上限。 | 两类地点综合旅程和正式 Q3；若启用 terrain v3，补全新旧世界矩阵。 |
| `N10` | 食物/精确配方/三槽冶炼严格解析；库存守恒；容量/连点/暂停/卸载/保存；主线可达、来源/消耗点和净增益循环检测。 | 综合经济旅程、平衡数据、库存/保存规模比较；饥饿仍需独立评估。 |
| `N11A` | 三档版本化参数；旧世界 Normal；世界保存/metadata 迁移；暂停菜单事务修改并在下个固定 tick 生效；同 seed/难度确定性。 | 三档核心旅程、内容规模/短时 soak、世界列表/备份恢复和 AI 交互确认。 |
| `N11B` | 有界事件状态和奖励去重；胜利前后/重载/备份恢复；不破坏主线结局。 | P2 伸缩项；纳入首个 Beta 才进入 BETA-RC 必过矩阵，否则明确后置。 |
| `N12A` | `en-US`/`zh-CN` key 对齐；fallback、非法 UTF-8、字体、长文本、极端缩放、字幕、Credits 和许可证。自动开发门禁已于 2026-08-26 通过。 | 两种语言核心界面进入 `AI-07`；人类阅读偏好 `NOT_CLAIMED`。 |
| `N12B` | v3 采样定义、9 个 WAV、61 项 manifest、许可证/Credits、缓存/并发上限、缺文件/损坏/设备降级、停止与退出清理均已于 2026-08-26 通过；VS2017/v141 双配置世界 681/681、资源包 34/34，隐藏真实三帧后端无降级。 | cue/字幕/生命周期进入 `AI-07`；人类听感 `NOT_CLAIMED`。 |
| `N12C` | v1 单通道流式定义、20 秒原创 WAV、settings v4、64 项 manifest、许可证/Credits、严格 RIFF/路径/时长、延迟/淡入淡出/低密度间隔、暂停/挂起/静音/音量归零、缺资源/设备、线程和退出清理已于 2026-08-26 通过；VS2017/v141 双配置世界 699/699、资源包 38/38，隐藏真实后端无降级，同身份 Q1 比较通过。 | 暂停/切世界/退出过渡功能进入 `AI-07`；人类音乐偏好 `NOT_CLAIMED`。 |
| `BETA-RC` | 2026-08-26 工程通过：Windows 适用全门禁、v1→v11 迁移、结局/奖励、战斗/投射物、结构/战利品、经济、语言/音频清理、六类 Q1、Q3 双档各 1800 秒、崩溃/符号/84 文件干净包均通过。 | 历史 R3/产品体验状态保持在原报告；当前功能与可观察范围由 AI 规范承接。 |

## Stage 9 回归触发器

| 改动类型 | 必须触发 |
| -------- | -------- |
| 世界、玩家、目标、容器或胜利状态 | 版本/非法值/全部旧版迁移，结局权威，奖励 epoch，遭遇中恢复，保存退出重开，事务失败，备份恢复，幂等和守恒。 |
| 地形、生态、结构或战利品 | terrain 身份，结构类型/cell/seed 与加载顺序确定性，禁止同步邻区块加载，跨区块投影，初始战利品快照，快速流送 Q1，结构规模和 Q3。 |
| actor、AI、攻击或投射物 | 战斗档案解析，固定 tick，状态/目标失效，actor/投射物/事件/射线/寻路容量，遮挡，死亡/卸载/重载清理，掉落去重，内容规模 Q1/Q3。 |
| 食物、配方、冶炼或资源 | 严格解析，输入输出守恒，容量/连点/暂停/卸载，主线可达，来源/消耗点和净增益循环，库存与保存规模。 |
| UI、输入、键位或设置 | 焦点隔离，配置迁移，键位冲突，隐藏客户端，截图和 R3。 |
| 音效、音乐、字体或本地化 | manifest，资源包，语言 key/fallback，字体/长文本，许可证，采样缓存/并发，句柄/线程生命周期，隐藏客户端和听感。 |
| 构建、崩溃或打包 | H2/H3 受控崩溃、符号、隐私、可执行清单、干净包和新 SHA-256。 |

Windows 自动化 EXE 一律隐藏或后台运行。只有用户明确安排的 AI/Computer Use、开发者自测或
历史物理合同运行才可以启动前台窗口；`ShowWindowNoActivate` 只是 best-effort，不能作为不会
抢焦点的保证。Computer Use 记录必须明确标成 `AI_INTERACTIVE`，不得伪装真人/物理证据，也
不得在用户未安排的时段反复启动、抢占当前焦点。

## Stage 10 视觉门禁

| 批次 | 合入前最低证据 | 阶段结束补充证据 |
| ---- | -------------- | ---------------- |
| `V10A` | 顶点 AO 0-3、侧边/对角规则、天空光/方块光、透明/未知邻域、跨 section 一致性；只有合并结果可重建全部内部采样才 greedy 合并，对角线按误差和固定 tie-break 选择；保持 32 字节 stride；Debug/Release 受影响目标。 | FS2 四图加洞穴/树冠/遗迹/营地 before/after；schema 3 既有 geometry/mesh/residency 字段补充比较、快速流送/规模 Q1、短 Q3 和开发者检查。超过 10% 未获批准不得关闭。 |
| `V10B1` | 图集尺寸/分格/坐标和颜色参数严格解析；旧图集像素兼容；不得静默改变 V10A AO/光照曲线；缺失/越界负例。 | shader 负例、Windows 双配置、开发者检查和 macOS Release 真实窗口冒烟。 |
| `V10B2` | 原创 top/side/bottom 资产、生成脚本、来源/许可、manifest、透明边界和世界/HUD/手持一致性。 | 近景/中景截图、资源包、相关 Q1、干净包和开发者检查。 |
| `V10B3`（Done） | 生态 tint 范围、坐标/seed/加载顺序确定性 tile 变体、greedy merge key 和 terrain v3 不变。 | 261 项图集、732 项 Release 世界、五张固定截图、相关 Q1、短 Q3 和 `developer-visual-record-v10b3.txt` 均通过。 |
| `V10C`（Done；Windows） | 定向雾共享参数、云层高度/厚度/速度/颜色边界、帧率无关移动、进入云层与关闭回退。 | 双配置聚焦 21/21、Release 世界 741/741、资源包 65/65、启动负例 14/14、十图多帧开发者视觉 PASS；相关 Q1 保留已批准的快速流送单样本延迟例外。新 macOS 运行 `NOT_RUN`。 |
| `V10D`（Done；Windows） | settings v5、v0-v4→Off、双语 key、Stage 10 shadow 补充身份；Off/Medium/High、距离/纹理/bias/PCF 上限、能力回退和资源清理。 | 双配置聚焦 21/21、资源包 75/75、Release 世界 742/742、74 项 manifest、14 类启动负例、9 项档位合同和强制回退通过；正午/黄昏六图开发者检查 PASS，最终同场景各档 frame P95/P99 无需例外。新 macOS 运行 `NOT_RUN`。 |
| `V10E`（Done；Windows） | settings v6、v0-v5→Off、双语 key、Stage 10 post 补充身份；Off/On tone curve/确定性抖动/八采样极轻 bloom、HUD 排除、缩放、回退和清理。 | 双配置聚焦 22/22、资源包 80/80、Release 世界 743/743、77 项 manifest、15 类启动负例、11 项性能合同和真实支持/强制回退通过；六图开发者检查 PASS，同档性能无需例外。新 macOS 运行 `NOT_RUN`。 |
| `VISUAL-RC`（Done：Windows 工程） | 适用全门禁、完整截图/开发者检查矩阵、资源/许可/Credits、正式相关 Q1、干净包和 bundle。 | Windows 77 项 manifest、743/743 世界、80/80 资源包、六类 Q1、nominal/stress 各 1800 秒 Q3、97 文件包和 239 项 Xcode 工程图静态检查通过；AI 产品表现进入 `AI-07`，主观体验 `NOT_CLAIMED`。 |

Stage 10 的地形 shader、顶点数据、图集、云、阴影或后处理变化都必须保留一个明确的低画质
回退路径。固定截图证明确定性，AI/开发者视觉检查负责可观察缺陷；动态项必须使用多帧、视频或
连续窗口。人类审美和舒适度 `NOT_CLAIMED`，视觉证据也不替代 AI 交互场景。

## Stage 11 可玩性门禁

Stage 11 的工程证据已经冻结。下表不重跑或改写这些 PASS，只定义 Architecture Lab 当前如何
补充真实窗口证据。本次 macOS `AI-01=BLOCKED（部分通过）`、
`AI-06=BLOCKED（隔离环境）`；其余正式场景尚未完成，逐项状态见当前 TODOLIST 和 Goal 记录。

| 批次 | 已有最低工程证据 | 当前 AI 场景 | 不声明范围 |
| ---- | ---------------- | ----------- | ---------- |
| `P11-0` | 火把/材料末尾追加、配方守恒、Rose 修正、metadata 发射、局部重光照、双配置和正式 Q1。 | `AI-05` 制作/放置/洞穴照明；`AI-07` 夜晚/熔炉多帧。 | 人类审美。 |
| `P11A` | 单一动作仲裁、settings v7 迁移、线性相对增量、焦点门、hold/toggle、定向 88/88。 | `AI-01..AI-04` 移动、世界/UI、暂停、Alt+Tab、最小化、回焦和重启。 | 物理鼠标距离、键鼠舒适度。 |
| `P11B` | 判定/表现解耦、数量/持续时间上限、关闭回退、音频并发和 HUD/准星隔离。 | `AI-05` 动态交互；`AI-07` 多帧/视频辨识。 | 人类打击感、眩晕和长期舒适度。 |
| `P11-1` | 身份末尾追加、门保存、木板/圆石守恒、axe/shovel 矩阵、资源和旧存档回归。 | `AI-05` 采集、工具比较、建可关闭落脚点。 | 人类建造乐趣、主观速度感。 |
| `P11C` | 目标 v3、三项并行机会、分支进度、配方发现、迁移、双语与完整世界回归。 | `AI-06` 在仓库不可访问的 package-only 新任务中完成 30 分钟盲玩；只有新工作目录但仍可读仓库时为 `BLOCKED`。 | 人类首次体验、留存和乐趣。 |
| `P11D` | 奖励 v1/v0、save v12、三类能力、持久化和全部迁移/资源门禁。 | `AI-05` 逐结构取得并实际使用能力，记录后续可执行操作变化。 | 奖励吸引力和人类价值感。 |
| `P11-2` | terrain v4、Mountain/洞口、v1-v3 冻结、保存身份、完整回归与正式 Q1。 | `AI-05` 寻找/通行；`AI-07` 轮廓/洞口可见性。 | 人类风景审美。 |
| `P11E` | 多部件轮廓、关键姿态、死亡表现隔离、身份掉落、Waystone 共鸣和完整门禁。 | `AI-05` 战斗/共鸣；`AI-07` 无名称轮廓与状态可辨识。 | 人类危险感、掉落价值感和战斗乐趣。 |
| `P11F` | VS2017/v141 双配置、832/832 世界、六类 Q1、双档 Q3、dump/符号和 104 项干净包。 | `AI-08` 从主菜单经胜利到保存重开；历史报告不回写。 | 外部玩家签字和未批准的新 macOS 运行。 |

AI 记录至少包含 commit、包哈希、执行器/Computer Use 环境、OS/GPU、窗口/图形/语言、seed、
存档身份、重试/超时/意外弹窗、证据路径和声明边界。截图或日志只能支持窗口内的实际步骤；
物理设备手感和人类可玩性不得由 AI 记录推导。

## T0/T1 地形基础（2026-09-06）

本次以 macOS 为必需平台，Windows 专属验证后置。`T0-SURVEY` 生产采样与
`tools/validate_terrain_foundation.py` 检查冻结的统计及 v1–v4 输出；`T1` 聚焦覆盖全 signed int
采样、8 seed 加载顺序/独立并发、出生支撑/资源距离/结构候选及保存修改重开。
完整 Xcode Debug/Release 门禁、干净包三场景固定画面、steady/streaming 三轮成本和正常输入
步行另列，不能用 headless 或固定诊断替代。具体阈值见 [v5 合同](../contracts/terrain-foundation-v5-contract-v1.md)。

E2 terrain v8 按 [E2 合同](../contracts/ecology-surface-coast-v8-e2-contract-v1.md)
单独验收：v1–v7 生产采样与区块指纹、`E2_SURFACE` 聚焦和完整 WorldRuntime、
`E2-SCENES`/`E2-COASTS` 生产原始 CSV 与 `tools/validate_ecology_e2.py` 汇总、
Debug/Release 双配置与资源/存档回归。窗口性能必须在同一 E2 Release `.app` 用
v7/v8 林地和岸边常驻/快速流送各三轮，预热 5 秒记录 30 秒，以
`tools/compare_ecology_e2_performance.py` 汇总原始逐帧记录，P95/P99
三轮中位数比值均不超过 1.10；保留原始逐帧和失败。固定原图及中文菜单正常玩法
分别交付；桌面锁定或输入不可用时按项标 `BLOCKED`，不得以自动化替代。

2026-09-15 用户为 E2 验收追加右上角小地图。最低检查增加：v1–v8 使用当前世界身份的
生产地形规划且不加载邻区块；切换 seed/版本清缓存；中文 1280×720 下纯圆形地图的北向、
玩家方向和约 128 米范围可辨，且无方形底板、坐标栏、版本栏，操作提示不重叠。小地图加入后须重新生成最终二进制、
固定原图、24 次同包性能和两个带哈希验收包；候选 20 只作为追加前历史证据。

2026-09-16 用户要求减少客户端重复启动：允许显式环境变量启用的同进程 E2 批量诊断，
一次启动顺序执行 24 段性能及固定画面，各段仍分别载入测量世界、留 5 秒预热和 30 秒
原始逐帧 CSV。批量证据必须证明同一 `.app`/PID、0..23 相位顺序、每段正确的 seed/terrain
version、存档元数据与世界进入成功；仅首相位需要进程启动成功字段。旧逐启动记录仍要求
每段启动成功。若正式三轮出现 FAIL 或明显波动，保留原件，按合同预定反序另做一次启动的
24 段完整复核，门槛不变；正常中文菜单输入与批量诊断分开。

E2 v7/v8 性能相位必须从同一冻结存档模板复制，除唯一 `world_id` 和
`terrain_generation_version` 外初始元数据一致，并同时固定普通难度与零初始 Actor。批次和逐相位
收据须记录模板/初始元数据哈希；比较器拒绝缺少夹具身份、难度不同或初始 Actor 非零的数据。
此前空 v8 目录与休闲 v7 模板混用的数据保留为协议失败原件，不作为 terrain-only 门槛证据。

2026-09-16 用户允许 E2 自动证据结束后先延期人工项目并启动 E3/R1/R2；该调整仅改变
开发顺序，E2 林地流送 P99 两轮 `FAIL` 不改判。E3 terrain v9 按
[E3 合同](../contracts/ecology-inland-meadow-v9-e3-contract-v1.md) 验证：
`E3_MEADOW` 聚焦及完整 WorldRuntime、由生产测试进程导出的 v1–v9 `T0-SURVEY`
样本/区块 CSV、`tools/validate_ecology_e3.py` 的 v1–v8 原始指纹与 v9 高度/生态/草甸
比例核对、Debug/Release 构建和必要的存档/资源回归。

为弥补常规 32 区块样本不覆盖开窗内域，`E3-MEADOW-SURVEY` 在冻结的 16 个
正/负坐标生产区块导出 Debug/Release 逐列、区块指纹和地点计划，并由
`tools/validate_ecology_e3_meadow_chunks.py` 核对源哈希、完整 XZ、方块与地点覆盖。
地点投影后的 Dirt 地基须保留在原始统计中；只有地点覆盖外的非草甸地表是 E3
地表失败，原误判及修正说明分别存证。该离线调查不替代窗口林缘视觉验收。

窗口性能及可见画面对照必须另记
真实同客户端数据；锁屏下的中文菜单步行/采集/重开按用户顺序延期为 `NOT_RUN`，不能写
`PASS`。E3 开工不关闭 E2，也不放宽 E2 的 1.10 门槛。

E4 terrain v10 按 [E4 合同](../contracts/ecology-inland-relief-v10-e4-contract-v1.md)
验证：`E4_RELIEF` 聚焦、完整 WorldRuntime、由生产进程导出的 v1–v10 `T0-SURVEY`、
`tools/validate_ecology_e4.py` 汇总、Debug/Release 构建和相关存档/资源回归。窗口检查复用
同一个工作 `.app`，在一次启动中完成固定山麓画面和常驻/快速流送采样；版本夹具必须显式
创建 v10 世界，不能用“当前版本”代替冻结身份。

E5 terrain v11 按 [E5 合同](../contracts/ecology-inland-water-v11-e5-contract-v1.md)
验证：`E5_WATER` 聚焦、完整 WorldRuntime、v1–v11 生产 `T0-SURVEY` 与
`tools/validate_ecology_e5.py` 汇总、Debug/Release 构建及 WorldCatalogue、StorageTransaction、
WorldBackup、SaveLoad、ResourcePack 回归。聚焦范围至少覆盖八 seed 正负坐标的连续河谷、
实际水深、生成方块、反序加载、出生资源、洞口、三类地点及 v11 保存重开；通用固定区块不
要求必然命中稀疏水道。正式窗口仍使用唯一工作 `.app`，固定 v10/v11 同 seed/XZ/朝向/时间
画面并各做三轮常驻和快速流送。图形会话锁定时如实记为 `NOT_RUN`，自动证据只能关闭工程
范围，不能代替正常中文菜单步行、采集和保存重开。

E6 terrain v12 按 [E6 合同](../contracts/ecology-vegetation-mosaic-v12-e6-contract-v1.md)
验证：`E6-VEGETATION-SURVEY` 从 16 个固定林区导出 144 个生产区块与逐列原始 CSV，修改前
独立导出两次并逐字节核对；实现后用 `E6_VEGETATION` 聚焦覆盖纯规划、三种树形、树冠边界、
地被地面、反序加载、v9 草甸、v11 水系、资源地点和 v12 保存重开。另须运行 v1–v12
`T0-SURVEY`、Debug/Release 完整 WorldRuntime 及受影响的存档/资源回归。窗口与性能复用唯一
工作 `.app` 并一次启动采集；图形会话不可用时按用户授权延期，不改写为 PASS。

2026-09-17 自动阶段结果：v12 独立生产调查两次逐字节相同，固定样本为 527 个树根、
24,706 个叶块和 1,044 个地被；树群疏密 15/16 林区、地被斑块 12/16 林区满足门槛，三种
树形在八 seed 均出现。v1–v11 共 22 份 `samples.csv`/`chunks.csv` 与 E5 冻结输出逐字节相同；
Debug/Release 聚焦各 `13/13 PASS`，完整 WorldRuntime 各 `1149/1149 PASS`，相关目录、事务、
备份、保存载入和资源包回归通过。Release 完整回归首轮暴露两个依赖出生加载范围的小地图
夹具随机失败，失败日志保留；夹具改用确定的远端区块后双配置通过。完整哈希和待做窗口项见
[E6 执行记录](../reports/ecology-e6-vegetation-v12-execution-2026-09-17.md)。

E6 窗口阶段已用唯一工作 `.app` 的同一 PID `82431` 完成 28/28 个阶段：24 个 v11/v12
三轮性能段和 4 个固定画面段。四组 P95/P99 中位数比分别为正坐标常驻 `1.042×/1.059×`、
正坐标流送 `1.017×/1.029×`、负坐标林缘常驻 `0.986×/1.018×`、负坐标林缘流送
`1.001×/1.015×`，均 `PASS`。首次启动因已跟踪资源清单的字体项排序错误在窗口创建前失败，
失败证据保留，排序修复提交 `cd6304a` 后有效重试一次。Computer Use 在批次运行时再次报告
图形会话锁定，中文菜单正常玩法为 `NOT_RUN`。

### E7 地貌多样化 terrain v13

按 [v13 合同](../contracts/terrain-diversity-v13-contract-v1.md) 和
[执行记录](../reports/terrain-diversity-v13-2026-09-19.md) 检查：

- `bash scripts/verify_terrain_landforms.sh Debug <新目录>` 与 Release：八 seed 覆盖、
  高度/坡差、变化量、极值坐标与纯规划确定性。旧 v1–v12 使用同一最终 WorldRuntime 的
  `T0-SURVEY`，与修改前 `samples.csv` / `chunks.csv` 逐字节核对。
- `HELLOMINE3D_WORLD_SMOKE_FOCUS=E7_LANDFORMS`：48 个选定地点实际生成及反序指纹、
  公开查询、干燥植物、八 seed 资源/出生/洞口/结构接近方向、显式 v13 与改块保存重开。
- `HELLOMINE3D_WORLD_SMOKE_FOCUS=E7_REGIONS` 配合
  `HELLOMINE3D_TERRAIN_SURVEY_DIR=<新目录>`：导出三个 seed 的 18 片 128×128 实际区域，
  明确洞口/结构/矿物例外，检查沙丘/岩台跨度和湿地实际水、干草地。连片选点工具为
  `python3 tools/select_terrain_landform_regions.py <samples.csv> <新输出.json>`。
- 双配置客户端、完整 WorldRuntime、资源、世界目录、事务与备份回归。测试使用隔离根，
  完整套件需要现有 `tools/fixtures`，不读取用户世界；构建和运行命令沿 README。
- 同一工作包固定原图、多 seed / 多视角和连续相机；新地貌 Off / High 都观察。受影响
  常驻/快速流送 v12/v13 各三轮，P95/P99 中位数不超过 1.10，记录网格、驻留和生成成本。
  最后通过正常菜单新建/进入/保存重开；持续输入按用户决定暂缓，不由诊断代替。

### E8 地标建筑 terrain v14

按 [v14 合同](../contracts/landmark-architecture-v14-contract-v1.md) 检查：

- `HELLOMINE3D_WORLD_SMOKE_FOCUS=E8`：八 seed 三类实际地标的基础/连通、两格净空入口、
  核心/矿物/玻璃数量、箱子奖励、跨区块正反生成与树冠避让；默认 v14 改块保存重开，
  真实容器取空和门柱破坏后重开保持。查询 halo 超过 9 格必须拒绝。
  三类陡坡候选一旦已不合格，应提前结束高度查询；R4 同时保留旧规划器触发该工作量断言的负例。
- `HELLOMINE3D_WORLD_SMOKE_FOCUS=LANDMARK-SURVEY` 配合
  `HELLOMINE3D_TERRAIN_SURVEY_VERSION=2..14` 和 `HELLOMINE3D_TERRAIN_SURVEY_DIR=<新目录>`，
  导出 `plans.csv` / `chunks.csv`；v2–v13 与修改前生产基线逐字节比较，v1 由既有完整回归覆盖。
  八 seed 每类至多取两个地点；旧 v2 只有路标。扫描半径上限 32 cells，不能无限搜索。
- 双配置客户端、WorldRuntime、Soak 构建及聚焦/完整 WorldRuntime；相关 nominal/stress 短 Q3。
- 三 seed 地标原图、近入口与夜景，连续诊断保留真实时间点；相同地点与机位进行常驻/快速
  流送各三对性能，P95/P99 中位数比不超过 1.10。记录网格、驻留和生成成本。
  诊断截图不等同正常移动，正常持续输入按用户决定暂缓。最新证据见
  [r31 记录](../reports/landmark-architecture-r31-2026-09-19.md)。

### E9 岩台地表过渡 terrain v15

- `bash scripts/verify_terrain_surface_transitions.sh Debug <新目录>` / Release：八 seed 的高度/生态不变、保护地表、连片性、signed 极值和原土带变化；旧 Foundation 实际负例与旧样本哈希对照。
- `HELLOMINE3D_WORLD_SMOKE_FOCUS=E9`：48 处实际表层与公开规划一致、植被支持、四区块反序指纹和默认 v15 改块保存重开。保留显式 v13/v14 生命周期用例。
- v1/v14 生产 T0 及 v2–v14 地标/区块与冻结基线逐字节一致；v14/v15 只忽略预期版本列比较高度/生态和地标规划。双配置完整 WorldRuntime、短 Q3，生成成本单独记录。
- 三 seed 原图、实际草/沙/林地边缘、有效连续相机、夜间/兼容材质及受影响常驻/流送三轮帧耗；正常 v15 新建/保存重开。倍率/图形能力变化和穿入地形的诊断镜头不得计为同协议通过。
- 本批真实进度与失败保留见[记录](../reports/terrain-surface-transitions-r32-2026-09-20.md)，不以 CPU 成本替代图形门槛。

### HUD 交互

按 [HUD 交互合同](../contracts/hud-interaction-contract-v1.md) 分批检查。`bash scripts/verify_hud_interaction.sh Release|Debug <新输出目录>` 覆盖页面转换、Esc 消耗、输入释放门与失焦；物品详情同时需要真实窗口检查。任务／地图增加对应真值和有界采样检查，完整交付采用双配置客户端、世界与资源回归。

HUD 另覆盖标记跨画布／列表的名称缓冲切换、地图异常信息优先级；`verify_exploration_atlas.sh` 覆盖指针朝向与北向导航一致性。HUD 地图纯检查与输入检查共用 `verify_hud_interaction.sh`，分别覆盖正交方向、真实高差、可见墙面、未知列空白、深度拾取、最大网格、快速拖动释放与平移边界。`capture_visual_macos.py --panel map|journal|pointer` 是显式后台诊断；物品内容图加 `--hud-fixture --inspect-slot 0..4`，不作为普通 hover 的证据。双语小窗口和浮层遮挡应查看原图；关闭 HUD 的性能与暂停页面成本分开比较。
