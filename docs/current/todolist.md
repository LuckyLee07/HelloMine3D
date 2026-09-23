# HelloMine3D 当前待办

本文只回答：项目现在做到哪里、当前批准什么、下一候选是什么、什么会阻塞开发。详细历史、合同和
封板证据分别进入 `docs/archive/`、`docs/contracts/` 和 `docs/reports/`。

最后更新：2026-09-23。

## 项目目标

HelloMine3D 是 **以真实可玩的单机体素沙盒为载体的 C++ Architecture Lab**。玩法不是可删除的
包装：架构工作必须由游戏内真实需求触发，并能通过正常菜单、输入、世界状态、保存和重开观察。

当前产品载体已完成 Windows PLAYABILITY-RC 工程范围；Architecture Lab roadmap 是能力候选池，
不是自动获批 backlog。只有本文件“当前批准批次”中的任务才构成开发承诺。

## 文档与状态规则

- `docs/current/`：当前状态、Architecture Lab、验收规范、架构和验证操作。
- `docs/contracts/`：已实现批次的冻结语义，不是当前 backlog。
- `docs/reports/`：检查点、RC、调查和视觉记录，不回写历史结论。
- `docs/archive/`：已结束路线和被取代协议，不得作为当前待办。

任务状态、执行结果和声明范围是三个正交维度：

| 维度 | 值 | 含义 |
| ---- | -- | ---- |
| 工作流 | `Queued / Planned / Todo / Doing / Engineering Done / Done` | 表示是否获批和工程是否完成。 |
| 执行结果 | `PASS / FAIL / BLOCKED / NOT_RUN` | 表示某项自动化或 AI/Computer Use 是否实际执行。 |
| 声明状态 | `CLAIMED / NOT_CLAIMED / OUT_OF_SCOPE / SUPERSEDED` | 表示证据允许声明什么，不把主观体验冒充 PASS。 |

例如 `Engineering Done + AI Playability NOT_RUN + human_fun NOT_CLAIMED` 是合法状态；没有运行记录
不得写成 AI PASS。完整口径见 `docs/current/ai-assisted-gameplay-acceptance-v1.md`。

## 当前基线

| 领域 | 当前状态 |
| ---- | -------- |
| 可玩载体 | 创建世界 → 采集 → 制作 → 工具成长 → 冶炼/食物 → 战斗 → 探索 → 路标胜利 → 胜利后事件 → 保存重开已经贯通。 |
| 玩法与视觉 | Stage 9、Stage 10/VISUAL-RC、Stage 11/P11F 已完成 Windows 自动工程范围；Stage 11 待开发代码批次为 0。 |
| 世界可靠性 | 世界目录、事务保存、有界备份、验证恢复、世界管理和主菜单入口已经完成；当前 world save format 为 v12。 |
| 自动门禁 | VS2017/v141 双配置、991/991 世界、80/80 资源包、126/126 配方、15/15 启动负例和 105 项干净包通过。 |
| 性能与诊断 | 六类正式 Q1、nominal/stress 各 1800 秒 Q3、崩溃 dump、脱敏 sidecar、离线符号和独立符号归档已闭环。 |
| AI/Computer Use | `AI-01=BLOCKED（部分通过）`，其余场景未完整执行；正常输入、隔离和音频限制见 Goal 报告。 |
| 人类体验 | 乐趣、审美、舒适度和物理设备手感统一为 `NOT_CLAIMED`。 |

PLAYABILITY-RC 发行 ZIP SHA-256：
`422F97E87046D4B6D5FC4BB99C37886FF37C4461A152C1162FA66A972B12F459`。

## 当前批准批次

**2026-09-22 用户明确启动：冒险世界体验升级（Doing，完整 Goal）。** 按 [迭代规划](adventure-experience-plan-2026-09-22.md) 与 [完整 Goal](adventure-experience-goal-prompt-2026-09-22.md) 持续实现：B0 基线 → B1 天空 → B2 前期任务 → B3 动物 → B4 目的地与建造 → B5 地图 → B6 界面 → B7 地下 → B8 玩家 → B9 声音 → B10 整合交付。全部方向均为必做范围；B1 云层／区域氛围仍待阶段验收，B2a–c 已本地提交而普通路线待验，B3 动物领域及模型工程已本地提交、普通画面／性能待集中验收，当前实施 B4 地点布局。按用户要求减少重复客户端启动，将完整图形矩阵集中验收；进度、失败、真实缺项和恢复入口集中在[唯一执行记录](../reports/adventure-experience-execution-2026-09-22.md)。本顺序取代下文历史目标的恢复顺序，已交付地形与 HUD 保留为基础，不把首批完成当成总 Goal 完成。

**2026-09-21 用户批准／2026-09-22 交付：HUD 交互入口 Goal（Done，本轮 HUD 功能）。** 快捷栏真实物品详情、任务日志入口、可交互 3D 区域地图及自由指针输入隔离已实现并分批中文提交。双配置完整世界各 1716、资源各 114、输入各 33、地图各 26 通过，末次名称修正 P11A 各 99 通过；12 次性能对照通过，独立中文客户端约 51 MiB。解锁后已补最终任务卡、地图、返回、切世界和普通菜单重启；工具／食物鼠标详情采用明确标注的隔离测试库存，普通采集及持续输入不冒充通过。见[目标](hud-interaction-goal-prompt-2026-09-22.md)、[合同](../contracts/hud-interaction-contract-v1.md)、[最终复核](../reports/hud-interaction-final-input-2026-09-22.md)和[交付记录](../reports/hud-interaction-delivery-2026-09-22.md)。

**2026-09-21 已交付：冒险地图级地形与生态。** 用户认为现有地貌仍过于简单，要求形成区域鲜明、生态丰富且适合持续探索的方块沙盒地图。先做宏观区域、差异化地貌、水系、成套植被／材质和探索路线；机器／菜单及独立天空重做后移。既有 E6、v13–v15 是基础，不代表新目标完成。可执行提示词见[冒险地图 Goal](adventure-terrain-goal-prompt-2026-09-21.md)，当前差距和天空诊断见[现状调查](../reports/terrain-ecology-baseline-2026-09-21.md)。用户已明确启动完整目标；B1–B5 现已完成实现、当前可执行验证及本地交付，新世界默认 terrain v19，旧 v1–v18 输出保持。双配置完整世界各 1710/1710，42 站连续观察、当前绘制复查和海岛补查完成；当前 Retina 分辨率下 72 次配对、364146 帧的 P95/P99 全部通过。正常菜单重开／合成界面／保存返回和再载入通过，持续步行与正常采集按目标约定保留输入能力限制。独立客户端约 51 MiB，清理冗余产物约 1.80 GB，原用户包和存档不变。详见[最终交付](../reports/adventure-terrain-delivery-2026-09-21.md)及[执行记录](../reports/adventure-terrain-execution-2026-09-21.md)。不替用户宣布审美认可；下文旧 Goal 的 paused／active 状态仅为历史，本次不自动启动天空或菜单新任务。

本轮已按用户要求清理历史 `build` 产物，旧报告中的原始附件和历史候选包已不可用；当前用户客户端及存档原地保留，旧工作副本未提交代码另有校验备份。见[清理记录](../reports/build-cleanup-2026-09-21.md)。下文各批次结果为历史结果，不代表原附件仍存在或已完成本次新目标。

2026-09-17 用户授权从产品、用户和美术角度实施综合视觉升级，随后启用七阶段持续 Goal，
当前 `Doing`，用户已释放观察客户端，并于 2026-09-19 明确暂缓持续输入相关验收、继续视觉迭代；完整范围和恢复入口保留。按 HUD/启动 → E6 与材质 → 真实水深/岸线 → 手持/掉落物 → 天空/光照 → 导航/地标/敌人
→ 整合玩法/性能/客户端交付顺序推进，见[完整目标](visual-upgrade-goal-prompt-2026-09-17.md)。
2026-09-20 地形批次后，按用户最新反馈优先改进游戏 UI，见[界面方向](game-ui-direction-2026-09-20.md)。
`Doing` 是项目工作流状态；本轮读取宿主 Goal 为 paused，未修改其状态，当前 UI 请求单独继续完成。
已整合 E6 terrain v12，使用独立 x86_64 检出构建，与并行 E2/E6 证据分开；旧 v10 隔离证据保留但不代表
当前版本。地图三档范围引入 runtime settings v10；当前新世界 terrain v15 沿用 v13 地貌和 v14 地标，增加岩台地表过渡，world save v12 保持。
具体实现、各检查结果、失败和恢复入口见[执行记录](../reports/visual-upgrade-2026-09-17.md)。
七个方向的当前状态如下。历史批次、失败样本、旧分辨率与旧提交号集中保留在执行记录中，不作为当前包身份。

| 方向 | 已落地与现有证据 | 剩余范围 |
| --- | --- | --- |
| 1 启动与 HUD | 资源排序/打包与有限正常界面自测；r34 行囊、箱子、合成层级；r35 沙盒任务/生命图形、固定快捷栏、四向罗盘与提示避让已落地，双语字号/尺寸、限定 CUA 及相关性能通过 | 机器与菜单整体设计未完成；完整正常容器和持续输入仍待验，不把限定 HUD 检查等同全部任务/玩法验收。 |
| 2 E6 与材质 | terrain v12 植被及材质统一；r28 terrain v13 沙丘、岩台、湿地和起伏/盆地；r29 湿地草丛与风摆、r30 岩层/沙纹；r32 terrain v15 坡积斑与草/岩/沙过渡，双配置各 1404 项回归、限定画面及 36 次性能通过，正常新建/保存重开通过 | r33 已柔化生态配色硬边；既有材质变体纹样及正常连续行进体验仍可深化，诊断相机不等同玩家移动。 |
| 3 水深与岸线 | 真实驻留水深、湿润岸面、顺岸风驱细纹；r14 水线过渡、r15/r16 水下远景与关闭回退修正，工程/GPU/连续帧及各批性能通过 | 正常编辑岸线和游泳/出入水待验。 |
| 4 玩家与物品 | 工具体积、真实材质掉落物与 r8 侧转修正；r20 手臂/袖口/三种握持、昼夜和窄窗口适配，24 次定向性能通过 | 正常持续开采、完整工具/容器链仍待验；第三人称全身玩家未实现。 |
| 5 天空与光照 | 云间留白、方向雾与接触区域复查；r19 云团受光/边界过渡，36 次定向性能通过；r26 稳定相机、连续柔化与精度提升，限定多帧、24 次性能及候选交付通过 | r26 已补正常三档切换、两种尺寸重开、切世界和退出；持续行进中的阴影观感仍待验，不声称全部场景零闪烁。 |
| 6 导航与生物 | 三档地图、当前区域、已访问地标；r17 正常切世界刷新；r21 肩髋/头口支点与斜向步态修正、12 次性能通过；r22 面部和材质分区；r23 补足颈部连接；r24 动作过渡与真实展示比例；r31 三类地标建筑、地形衔接和树冠避让通过工程、限定画面及 36 次性能 | 正常改块/移动/地标生命周期与完整战斗待验。脚掌贴地未验证；独立动物体系尚未实现。 |
| 7 整合与交付 | r11 全矩阵 72 次及掉落物 24 次审计通过；后续按受影响范围逐批性能与候选交付，最近候选为 `delivery-r35`；r25 完成 11 组整合静态画面，r32 补当前 v15 正常菜单建档/保存重开，r34/r35 补隔离诊断 CUA 物品存取/制作与 HUD 状态/提示 | 正常创建/进入、设置与有限保存重开已补；移动、采集、完整正常制作/工具/容器链及战斗尚未验收，Goal 未完成。 |

最新 r26：优先减轻阴影跳变，中/高档采用 1024/2048 阴影图、连续 3×3 滤波和稳定太阳参考轴。
双配置构建、V10D 各 35/35、资源各 114/114、相机各 11/11、GPU 滤波 40/生物 93/地形 96
通过；旧参考轴与旧滤波负例被检出。三段对照和七段新版各 41 帧，限定地面跳变指标下降；
24 次性能及逐帧核验通过，交付 `delivery-r26`（111 文件/328 源码）。不声称所有场景零闪烁。
首次 CUA 切档因用户操作中断的记录保留；随后正常补验中→高→关闭→中、960×600/1280×720
重开、不同种子世界切换与正常退出通过。原设置通过 UI 完整恢复，原有四个存档元数据未变；
证据为 `normal-r26-followup/verification.json`。这是设置/生命周期自测，不替代连续行进观感。
r27 已完成近镜头投射物修复：封闭分面滴形、速度朝向与前后明暗。双配置客户端、PROJECTILE
各 15/15、资源各 114/114、几何各 51 个断言、生产 actor GPU 100/100 与旧实现负例通过；
10 段诊断对照、24 次相关性能及逐帧核验通过，交付 `delivery-r27`。正常新建/进入、自然投射物
观察、保存返回菜单和退出完成限定自测，未将持续输入或完整战斗计为通过。
详见[本批记录](../reports/projectile-presentation-r27-2026-09-19.md)。

用户最新优先级已执行：投射物于 `2b4cf15` 提交后立即开始 r28 地形多样化。
terrain v13 已接入新世界，包含弯曲沙丘、岩台/切谷、林地盆地、浅水湿地及区域过渡。
双配置完整 WorldRuntime 各 1253/1253，v1–v12 生产输出保持；多 seed 原图及连续诊断已审阅，
36 次帧时间对照及原始数据核验通过，正常菜单新建/保存重开与干净候选交付完成。见
[地貌方案](terrain-diversity-plan-2026-09-19.md) 和[本批记录](../reports/terrain-diversity-v13-2026-09-19.md)。

r29 已深化湿地草丛：既有 TallGrass 已接入细长叶片、错层茎穗及匹配的选中/裂纹风摆，
不改变生成、采集或存档。双配置编译、真实区块/反馈回归、生产 GPU 与三 seed 限定画面已通过；
24 次新旧模型性能及 96913 帧复核通过，最大 P95 比值 1.0911；干净候选 `delivery-r29` 已交付。详见[本批记录](../reports/wetland-grass-presentation-r29-2026-09-19.md)。

r30 已补岩层冷暖灰层理与沙面风纹，远处按像素覆盖衰减；复用同身份客户端，GPU 148/148、
双配置资源各 114/114、三 seed 限定画面、24 次性能及 100045 帧复核通过，最高 P95 比值 1.0238。
干净候选 `delivery-r30` 已交付，详见[本批记录](../reports/geology-materials-r30-2026-09-19.md)。

r31 已重做营地双棚、遗迹石门和路标台座，接入地形基础与树冠避让；默认新世界 terrain v14，
v1–v13 保持。双配置完整 WorldRuntime 各 1350/1350、限定画面、36 次性能与 122327 帧复核通过，
干净候选 `delivery-r31` 已交付。已补当前 v14 正常新建/保存重开；详见[本批记录](../reports/landmark-architecture-r31-2026-09-19.md)。

r32 已完成 terrain v15 岩台坡积斑、沙沟宽窄和草/岩边缘交错，保留旧 v1–v14。
双配置完整回归各 1404/1404、旧版兼容、短 Q3 和 CPU 成本通过；23 张限定静态/连续原图已复查，
正常 v15 新建/保存重开/退出通过。中断后只补林缘两组，与原四组共同完成 36 次、148136 帧
性能审计，最大 P95/P99 比值 1.0414/1.0625；原失败及不完整样本保留。
最新完整候选 `delivery-r32` 已交付，见[本批记录](../reports/terrain-surface-transitions-r32-2026-09-20.md)。

r33 已柔化林缘生态配色硬边：双配置各 1409 项世界回归、资源各 114 项、GPU 134 项及限定画面通过；
24 次、99577 帧性能审计通过，最大 P95/P99 比值 1.0534/1.0279，峰值网格 buffer 约增 0.21%。
干净候选 `delivery-r33` 已交付。见[本批记录](../reports/ecology-colour-transitions-r33-2026-09-20.md)。

r34 已完成行囊与合成首批重塑：客户端双配置、P11A 各 99、C1-CAP 各 17、资源各 114 通过；
24 组合基础布局、最终代表布局及 CUA 实际存取/制作通过限定检查。12 次、69283 帧相关性能审计通过，
最大 P95/P99 中位数比 1.0364/1.0340，干净候选 `delivery-r34` 已交付；不是整套 UI 或完整正常玩法完成。
详见[本批记录](../reports/game-interface-r34-2026-09-20.md)。

r35 已完成常驻 HUD 重塑，并按用户认可的低饱和像素方向接入生成素材；补回草图中的四向罗盘。
客户端双配置、P11A 各 99、导航各 65、资源各 114；33 张分阶段限定原图及 CUA 进食/换槽/菜单检查通过。
12 次、59025 帧性能审计通过，最大 P95/P99 比 1.0120/0.9835；干净候选 `delivery-r35` 已交付。
详见[本批记录](../reports/hud-interface-r35-2026-09-21.md)，机器/菜单与完整正常玩法仍待完成。

自动诊断保持隐藏/noActivate；r35 所有进程已退出，候选、启动器与正常设置恢复，客户端关闭。
十个世界共 31 份元数据在性能结束后保持。实际提交及证据记录于本地 `goal-recovery-r7.json`；历史包保留。
macOS 单调时钟修复和睡眠中断样本分类已有证据，历史 Reduced 停摆根因仍未证实；E2 性能问题独立保留。
正常持续输入按用户要求暂缓，不再重复权限排查；脚掌贴地、独立动物和第三人称全身玩家没有被上述限定结果计为完成。

2026-09-12 方块选中与破坏表现已接入：贴合花草模型的表面高亮替代黄框，十阶段表面裂纹与
材质碎屑接入真实开采/事件路径。工程和定向视觉证据见[实施记录](../reports/block-feedback-2026-09-12.md)，
不改变命中、硬度、掉落或存档；不替代完整正常玩法或人类审美验收。

2026-09-12 花草摆动生硬问题已修复：连续时间、缓慢阵风和根部固定接入普通/阴影路径，
GPU 10/10、资源 105/105、实机动态序列与定向性能通过，见[修复记录](../reports/flora-wind-fix-2026-09-12.md)。
后续按用户反馈将连续摆速提高约两倍、增强弱风摆幅；GPU 对齐检查和 Off/High 动态复查通过，见同记录的后续调整。

2026-09-12 用户截图中的水面区块接缝修复已完成：波浪改用世界坐标，210 对 GPU 边界采样
位置/法线误差归零；岸边与水下实机、资源/相关回归及定向性能对照完成。首组流送性能异常
与全部复测保留，见[修复记录](../reports/water-seam-fix-2026-09-12.md)。这项局部修复不扩展水系或改变存档。

2026-09-12 后续授权：开始暖野 v2 首轮 `WV2-00..03`，交付高清自然材质、单元格内叶簇与
可正常游玩的林地样板 M1。用户再次反馈后已撤回新树冠，当前候选恢复旧立方叶/叶贴图、保留其他材质升级；整体仍 `Doing`。补验后 macOS 双配置工程门禁通过，
旧候选包林地 High 帧 P95 为 v1 的 1.215 倍；当前包完整复测该档通过，但密林 Medium
P95 为 1.156 倍，仍超过 1.10 性能护栏。正常移动/采集仍缺可靠输入证据。
见 [首轮合同](../contracts/warm-wilderness-m1-contract-v2.md)、[实施记录](../reports/warm-wilderness-m1-implementation-2026-09-12.md)
与 [M1/T1 补验](../reports/m1-t1-acceptance-continuation-2026-09-12.md)。
`WV2-04..08` 仍为后续候选；本轮不改生成规则、save v12 或 D2。

2026-09-13 R1 当前旧叶包在用户批准的同机同期口径下性能通过，但用户以封面和真实试玩
截图指出树冠视觉不合格；持续移动/采集与移动画面仍缺证据。用户要求重新制定
[封面观感树冠升级方案](r1-canopy-cover-style-plan-2026-09-13.md)，当时方案已完成、尚未实施；
R1 仍 `Doing`，旧包不作为完成版提交，也不进入 R2。当前逐项状态见
[R1 验收报告](../reports/goal-r1-m1-acceptance-2026-09-12.md)。
用户随后明确要求落实该方案并交付最终实机对比；标准路径树叶外观已获 R1 内修订授权，
旧包性能和视觉结果不继承给新包。
现已做出只改 16 个标准树叶层的新 R1 候选及
[实机前后对比](../reports/r1-canopy-cover-comparison-2026-09-13.md)，并制作 109 文件
独立包。macOS 双配置工程/资源门禁 PASS，正式六组同机成对性能中五组 PASS，
林地 High P95 **1.831× FAIL**；旧 R1／新叶六轮定位为 1.043×，仅作诊断不覆盖
正式失败。正常创建/设置/保存重开 PASS；持续移动/采集与移动视频 BLOCKED。
R1 保持 `Doing`，不作为完成版提交，也不进入 R2。

2026-09-13 用户进一步拒收斜切低多边形树冠，要求完整方叶块轮廓。
R1 当前改为原创方像素叶纹理＋**仅新世界 terrain v6** 的错层橡树；旧存档
v1–v5 树块布局、其他地貌/遗迹规划及玩法语义保持。第一版 v6 62 个叶格过密、
压低视线，已按同机位实机图继续收敛叶块量和叶色，尚未形成正式验收版。
旧新候选包、失败数据与重拍原图均保存于 `build/goal-r1-canopy-shape-20260913/`。
R1 仍 `Doing`；不得沿用上一候选的性能 PASS、提交或进入 R2。
新一轮 v1/v6 林地／密林六档各三轮的 36 次正式采样现全部完整；P95/P99
中位数及密林常驻 buffer 均 PASS，原始长帧与旧版失败分别保留，见
[方块橡树验收记录](../reports/r1-voxel-oak-acceptance-2026-09-13.md)。
Debug 受影响目标、15 个启动资源负例、最终独立包五组固定画面亦 PASS。
用户解锁 Mac 后，最终包隔离副本的正常菜单建档/载入、暂停设置与保存重开
PASS；持续移动/转头、采集和移动视频受当前 Computer Use 缺少按住输入接口
限制而 BLOCKED，原始短按/拖动、窗口图与存档哈希已保存。用户实际试玩后
认可当前树冠效果，并明确要求按此版本本地提交一版；该授权允许提交 R1
**版本检查点**，不把缺失的正常玩法证据改判 PASS。R1 整轮仍 `Doing`，
不宣布完成、不进入 R2、不推送或发布。

2026-09-13 用户指出当前生态生成质量不合格，明确调整顺序为先做生态地形改造，
再以改造后的实际版本收口 R1/M1 和 R2/T1；此前“先 R1、再 R2、再生态”的
执行顺序被这次授权取代。生态批次仍严格逐轮验收、获确认后本地提交；
本批仅 E1 森林生态 v7 已获批准，范围与退出条件见
[E1 合同](../contracts/ecology-forest-v7-e1-contract-v1.md)。R1/R2 已通过的旧版工程证据
保留，受 v7 影响的视觉、性能、正常玩法项待改造后重新验收，不预先宣称通过。
用户已认可当前 E1 视觉效果并明确授权“这版可以提交”；据此做本地检查点提交。
2026-09-14 收口复核：Debug/Release WorldRuntime 均 1043/1043，固定客户端的
v6/v7 森林常驻性能 PASS；快速流送首轮 P95 FAIL（1.645 倍），反序复核 PASS、
六次汇总 PASS，原始失败与波动风险保留。Computer Use 完成中文菜单建档、保存重开，
当时游戏内持续移动/转头/采木未取得可验证输入证据。用户随后亲自试玩完成采木、
合成并摆放工作台；旧客户端保存 FAIL，失败候选保留了位置、转向、进度与工作台。
已修复掉落物次正规速度导致的 macOS 存档读回错误，从原候选救援用户进度，
固定客户端原位刷新后正常菜单保存及同世界重开 PASS。两配置存档相关回归通过，
原始失败、恢复和修复版均有独立哈希包。修复版现已重新取得同包 v6/v7 森林、
密林固定原图；森林常驻三轮 P95/P99 比值为 0.999/1.079，快速流送为
0.984/0.999，均 PASS。旧包首轮快速流送 FAIL 未查明原因，密林固定机位近树干
遮挡及缺少独立连续玩法视频明示为剩余风险。修复版干净审查包 SHA-256 为
`4257be0cdc9ecfc7eb633d92b7b88b14c1620eb935c01867f5cf0350fb7243cf`，
原始证据包 SHA-256 为
`e6df0444aa5117ca9cc5ce32feb05b5ea0e28b5328076e6332dfe0794b42ae82`。
详见 [E1 执行报告](../reports/ecology-e1-forest-v7-execution-2026-09-13.md)。
用户已于 2026-09-14 明确确认 **E1 整轮验收通过**；E1 状态为 `Done`，
完成版仅本地提交。已验收的包和证据哈希不变，旧包流送首轮 FAIL、密林近树
遮挡与无独立连续玩法视频继续作为已披露风险保留。E2 尚未启动，R1/R2 也
不因 E1 完成自动收口。

2026-09-14 用户批准独立 E2 Goal：terrain v8「地表、岸线与生态边缘」，状态 `Doing`。
实际起始 HEAD `15d6f3a27d1c6af812e323563b39fea0c3bda362`、干净工作区及 E1
验收包/证据哈希均已核对；本轮范围、v7 生产基线、固定机位和退出门槛见
[E2 合同](../contracts/ecology-surface-coast-v8-e2-contract-v1.md)。E2 仅在独立交付、
用户明确确认验收通过后本地提交；其前不实施 E3，也不收口 R1/M1、R2/T1。
候选 20 收窄离岸缓坡的作用距离，其 Release 二进制的 v1–v7 生产身份、v8 岸线和
资源/存档统计、Debug/Release 生成工程、Xcode 完整回归及原合同 24 次窗口性能均 PASS；
旧失败和正常玩法动态输入 `BLOCKED` 均已保留。用户于 2026-09-15 查看后要求先增加定位
对比，现进入候选 21：采用像素地图缓存，在右上角显示约 128 米地表、
区块网格和玩家方向。用户复核首版后要求缩小并移除方形底板与文字栏。Debug/Release 增量构建、资源读取、中文固定岸边
实机画面及一次 30 秒快速流送聚焦检查已通过；完整回归、最终 24 次性能、正式原图与新
哈希验收包仍待重做。候选 20 的包和证据继续保留，但已不是最终身份。E2 仍留在本轮，
未经用户明确验收通过不提交。

小地图运行时和中英文“北”标签已按用户明确授权先独立本地提交 `00e9b51`；提交仅含
`OgreUserInterface.cpp`、`en-US.text`、`zh-CN.text`，E2 地形、合同和证据未混入。
该中间提交不表示 E2 验收通过或完成。

随后用户明确允许 E2 内容也先提交。当前 terrain v8 实现、生产回归、验证工具和 E2 文档
可形成独立本地实现检查点；这项授权不等同于“E2 验收通过”。候选 21 的完整回归、最终
24 次性能、正式画面对照、新哈希验收包和用户最终确认仍是退出条件，完成前不进入 E3、
R1/M1 或 R2/T1。
完整结果与原始失败见
[E2 执行报告](../reports/ecology-e2-surface-coast-v8-execution-2026-09-14.md)。

2026-09-15 terrain v8 实现检查点已按用户授权独立本地提交 `2d3acfe`；E2 正式验收
身份和退出条件不变。用户随后要求以产品视角实机检查并打磨界面细节；小地图圆边、
中文按键显示、提示占位、世界目录信息与布局、暂停和初始合成细节作为独立产品界面改动交付。
macOS 双配置增量构建、Release 资源/目录/本地化聚焦回归和 Computer Use 中文菜单、
1.0/1.25 倍界面、normal v8 世界保存重开均通过；同机位地图原图对照及失败已保留。
新世界 Stalker 单色贴脸遮挡仍是产品风险，未实施延后的 R3/R4。E2 正式性能、
独立验收包与用户确认仍待完成，见[产品界面审计](../reports/product-ui-polish-audit-2026-09-15.md)。

2026-09-16 用户要求一次启动完成多组 E2 验收。候选 23 在干净 HEAD `b729184` 起步，
增加仅由诊断环境启用的同进程世界切换与分段性能/画面采集；两段试跑同 PID PASS。
当前最终 Release 二进制 `d41e615a…` 的 gmake/Xcode Debug/Release 全回归、v1–v7
生产指纹和 v8 岸线统计 PASS。正式客户端一次启动完成 24 段性能与 12 段 v7/v8 原图，
36 段同 PID/事件与画面身份 PASS；三轮比较中林地流送 P99 `1.391× FAIL`，其余
七个 P95/P99 指标 PASS。失败原件和被用户停止的旧 20 段逐启动数据全保留；按开工前
冻结的反序另一次启动完整复核 24 段，林地流送 P99 仍 `1.305× FAIL`，其余指标
PASS。六段针对性图形侧录的 Ogre batch/triangle 计数明显无效，临时实现已撤回，
原始数据与失败说明保留。撤回诊断侧录后 Xcode 双配置再次 PASS，Release 二进制
SHA-256 逐字节恢复正式候选 `d41e615a…`。Computer Use 报告 Mac 已锁定，最终版
中文菜单、步行/采集、保存重开暂 `BLOCKED`；当前 FAIL 版的干净游戏包、独立
原始证据包和逐文件 SHA-256 已封存并通过第二道审计，实际哈希见项目内
`build/ecology-e2-20260914/review-delivery-candidate23.json`。正式性能不符合
E2 原门槛，未经修复复验或用户明确调整验收，E2 仍为 `Doing`，
不进入 E3、R1/M1 或 R2/T1。详细证据见
[E2 执行报告](../reports/ecology-e2-surface-coast-v8-execution-2026-09-14.md)。

2026-09-16 用户随后明确调整**开发顺序**：本轮可自动执行的验证完成后，人工操作项先延期；
E2 不必全项验收通过才可启动 E3 或 R1/R2。该新指令取代上文各阶段禁止进入下一轮的
顺序限制，但不改变 E2 原性能门槛或失败结论。候选 23 林地流送 P99 正序 `1.391× FAIL`、
反序 `1.305× FAIL` 原件继续保留；最终版中文菜单步行/采集/保存重开因 Mac 锁屏未执行，
按用户要求列为延期验收。E2 仍 `Doing / 完成版未验收`，可自动化验证已收尾；本地检查点
提交不得标记为 E2 完成版。下一开发批次优先沿此前生态地形方向选 E3，R1/R2 收口保持候选。

E3 已按这次授权启动为 terrain v9「内陆林缘与草甸开窗」首批，范围和冻结的 v8
生产基线见 [E3 合同](../contracts/ecology-inland-meadow-v9-e3-contract-v1.md)。当前状态
`Doing / 自动工程完成`：新世界 v9 已接入真实生成路径，旧版 v1–v8 生产采样/区块
指纹 16 项逐字节不变；v9 内陆草甸转换 27.278%、高度 0 变化、八 seed 自然出生
资源与三类地点 PASS；新增真实区块对照确认 v8 天然高草地稀疏橡树在 v9 保留，
gmake Debug/Release 聚焦各 24/24、世界完整回归各 1091/1091、客户端双配置编译
PASS。额外从冻结的生产 CSV 选定八 seed 正负坐标共 16 个真实草甸区块，
16/16 方块指纹变化、4096 列高度不变、草甸树根为 0，Debug/Release 三表逐字节一致。
原先“最终草甸列全是 Grass”的冻结检查记录 52 格 FAIL；生产地点规划证明全部是两座
v9 RaiderCamp 的合法 Dirt 地基，地点覆盖外非草块为 0，初次失败及判定说明均保留。
首次超量开窗及测试夹具失败也保留。窗口性能、正式画面与动态中文玩法
尚未取得，不声明 E3 验收通过，见 [E3 执行记录](../reports/ecology-e3-inland-meadow-v9-execution-2026-09-16.md)。
E2 仍 `Doing`，其林地流送 P99 FAIL 和最终版人工玩法延期不因 E3 开工消失。
R1/R2 本轮未并行实施。

E2 封存后对 12 段原始林地流送逐帧复查：正序 217 帧、反序 99 帧在稳定场景中
出现 `render_ms≥15`，相邻帧没有加载、网格累计量、可见面或驻留缓冲变化；v7 也有
同类尖峰。CPU 记录只能把问题收窄到渲染/呈现侧，不能区分 GPU 与 OS 原因；两轮
P99 `FAIL` 不改判，复查 SHA-256 和逐项结论见 [E2 执行报告](../reports/ecology-e2-surface-coast-v8-execution-2026-09-14.md)。
E2 的绘制／缓冲交换分段侧录已在隔离源码工作树准备并通过 macOS 双配置对象编译；
隔离 Release 可执行文件现已完整链接，初次缺库失败和 17 个既有库哈希均保留；
采集器有效/损坏 CSV 五例自检 `PASS`。尚无实际图形会话数据，也未创建或启动
第二个 `.app`，原验收包身份保持。下一次
可用图形会话需要先从同一个项目工作客户端做一次启动 pilot，不能用准备状态改判性能。
隔离诊断构建的二进制、失败/重试日志和工具自检已用 11 文件 SHA-256 收据另存项目内，
见 [E2 执行报告](../reports/ecology-e2-surface-coast-v8-execution-2026-09-14.md)；
这仍不是 E2 的新验收包。
候选 23 的 12 段逐帧数据再经只读源哈希复查：316 个稳定场景高渲染帧
没有天空/雾/GUI/截图/演员数变化；六对林地流送稳定段的 v8 实心面和地形驻留
均小于 v7，但 v8 常驻演员多 1。该变量复查只缩小后续侧录范围，P99 `FAIL`
与 Pilot `NOT_RUN` 仍如实保留，见 [E2 执行报告](../reports/ecology-e2-surface-coast-v8-execution-2026-09-14.md)。
唯一 E2 工作客户端在确认无游戏进程、109 项原清单 PASS 后已从隔离 v8 源码
**原位刷新**为绘制／交换阶段诊断二进制；刷新前 40 MiB tar 回滚快照、刷新日志与
SHA-256 收据保留。主工程批量采集器同步诊断参数，正式候选 23 游戏/证据 ZIP
哈希未变。Mac 锁定下客户端没有启动，Pilot 仍 `NOT_RUN`，P99 仍 `FAIL`；
当前工作 `.app` 是未验收诊断版，不是新的 E2 正式游戏包。详见
[E2 执行报告](../reports/ecology-e2-surface-coast-v8-execution-2026-09-14.md)。

2026-09-16 解锁后的后续结果取代上段“仍锁定/诊断版”的当前状态描述，但保留其历史过程：
绘制阶段 Pilot 与一次 24 段诊断已在同一工作客户端完成，原件封存后客户端已从回滚快照
**原位恢复**为正式候选 23，游戏二进制 SHA-256
`d41e615aeea8250c9790ffa7585925b807a109ce18a0e1be62a693a50b9714e6`，109 项清单和 metadata
均 `PASS`。进一步发现旧性能协议只给 v7 复制冻结存档，导致 v7/v8 难度与 Actor 夹具不同；
旧结果降级为协议失败证据。修正工具后，正式候选以完全匹配的普通难度、初始零 Actor 夹具
各执行一次正序和反序 24 段：正序四组 `PASS`；反序只有岸边流送 P99 `1.134× FAIL`，
其余七项 `PASS`。396 文件完整性收据 SHA-256
`0afc100054f993dd5950d4d22bdfad9e5dbf6880a2ead3f174b30656571c6463`。
随后三轮八 seed 的三种生成器微优化 A/B 均保持生产输出身份，但没有稳定收益，正式 seed
甚至分别为 `1.031×`、`1.045×`、`1.009×`；三项源码均撤回，当前工作区和正式客户端未混入
这些实验。A/B 汇总 SHA-256
`70c32908177ed603cc2fd3959b0a689cd44ea802ee1125260d65f84ed52e0f2f`。E2 仍为 `Doing`，
正常玩法继续按用户指令延期；性能失败保留给后续专门收口，不阻塞已获授权的 E3 开发。

E3 后续窗口批次现为 `Engineering Done / 自动与窗口证据 PASS，正常玩法延期`。单次启动
验收框架已提交 `e8a686f`；唯一项目工作客户端从该干净提交原位刷新，最终 Release 二进制
SHA-256 `7c306b27eeec8ca6b7ed245860fa671fef4a740d3e71675184b4f4eea202062c`。正式批次在同一
PID 内完成 12 段性能和 4 段画面，16/16 `CAPTURED`：草甸常驻 P95/P99
`1.006×/1.014×`，快速流送 `0.995×/0.979×`，均 `PASS`；正负坐标 v9 原图分别证明开阔
草甸和沿坡林缘可读，没有高度接缝、等宽直带或逐格噪点。136 文件收据 SHA-256
`937b28c682b9d74cba1d3929ff1cdb4ea491de1fba08fa0a3cb109bf83f23b43`。首次窗口批次因共享
输出残留 Debug 二进制而整批身份 `FAIL`，失败原件和二进制以 SHA-256
`775f6c092c1ed52426087d12b5cc29e1918b880f3604f8ba24ce891c16aa6e8f` 保留，未覆盖。中文菜单
步行、采集和重开继续按用户指令延期；不声明用户已验收 E3。详见
[E3 执行记录](../reports/ecology-e3-inland-meadow-v9-execution-2026-09-16.md)。

2026-09-16 用户继续授权后续开发，E4 已冻结为 terrain v10「内陆起伏与山麓过渡」。
本批只改善新世界离岸内陆的大尺度高度轮廓，让缓丘、浅盆地、鞍部和山麓肩部在飞行与
步行视角下可读；v1–v9、v9 生态/地表标签、低地岸线、高山脊、河网、水体、存档格式和
渲染系统均有明确边界。实际起始 commit、v9 生产基线哈希和量化退出条件见
[E4 合同](../contracts/ecology-inland-relief-v10-e4-contract-v1.md)。当前状态
`Engineering Done / 自动与窗口证据 PASS，正常玩法延期`：v1–v9 的 18 项生产采样/区块文件
逐字节不变；v10 的 463,056 列标签漂移 0、冻结高度带变化 0、最大坡差 3，宏观内陆
71.048% 改变、平均绝对差 2.0095。gmake Debug/Release 聚焦各 8/8、完整 WorldRuntime
各 1099/1099、目录 60/60、存档事务 18/18、保存载入及客户端链接均 PASS。五轮参数
失败和两轮 E3 夹具失败全部保留；详情见
[E4 执行记录](../reports/ecology-e4-inland-relief-v10-execution-2026-09-17.md)。唯一项目
Release `.app` 已从干净提交 `846e4f5` 原位刷新，在同一 PID 内完成 12 段性能与 4 段
画面，16/16 `CAPTURED`：常驻 P95/P99 `0.994×/1.012×`，快速流送
`0.915×/0.995×`，均 `PASS`；正负机位原图证明宽尺度升降与连续山麓可读，没有接缝、
等宽直带、棋盘噪点或突然陡坎。136 文件收据 SHA-256
`cf6bf74150be95abb71d050fc2a7abe38aa7d576f7bc3040d309cdd2e5a7d984`。中文正常玩法按用户
指令延期；E2 的性能 FAIL 与 E3 延期玩法继续独立记录，不声明用户已验收 E4。

2026-09-17 用户继续授权后续开发，E5 已冻结为 terrain v11「内陆河谷与溪流水系」。本批
复用现有水方块和全局水面，只在 v10 原干地高度 `64..95` 内生成稀疏连续的低地溪流、缓岸
和干河谷；不加入新方块、River 生态、水流模拟、动态水位、完整河网拓扑、侵蚀、桥梁或
渲染改造。实际起始 commit、v10 生产基线和修改前量化门槛见
[E5 合同](../contracts/ecology-inland-water-v11-e5-contract-v1.md)与
[冻结清单](../reports/ecology-e5-v10-production-baseline-2026-09-17.json)。当前状态为
`Engineering Done / 自动验证 PASS，窗口与性能因锁屏延期`：terrain v11 实现提交
`db22168`；v1–v10 的 20 份生产样本/区块文件逐字节不变，v11 在 80,298 个合格低地宏观
样本中改变 19.233%，新增水列占全部宏观样本 0.4049%，最大落差与相邻坡差均为 3。
八 seed 正负坐标均有 48 格连续河谷，八 seed 均生成 2–3 格实际水深；出生、六类基础资源、
洞口、三类地点、反序生成和保存重开均 PASS。Debug/Release 完整 WorldRuntime 各
`1136/1136 PASS`，相关存档/资源回归通过。唯一视觉工作客户端已在原路径合入 E5，干净源码
提交 `fcb7e2d` 对应的 Release 二进制 SHA-256 为
`d78fbb8bf417aa27ad81d9df7f9d61d3e3301d4ee354746cff4e7e67e9a816e5`。Computer Use 仍报告
Mac 锁定，因此 v10/v11 固定画面、三轮窗口性能及中文正常玩法为 `NOT_RUN`，不声明 E5
整体验收完成。详见 [E5 执行记录](../reports/ecology-e5-inland-water-v11-execution-2026-09-17.md)。

2026-09-17 用户在 E5 分批提交后继续授权后续开发，E6 已冻结为 terrain v12「林地层次与
植被斑块」。本批只重组新世界的树木轮廓、林地疏密和地被斑块，不改变 v11 高度、生态、
地表材料、水系、洞口、矿物、地点、方块 ID、存档格式或渲染。状态为
`Engineering Done / 自动验证与窗口性能 PASS / 正常玩法 NOT_RUN`：起始 commit `3e732a1`，
实现提交 `7be7d2c`，小地图远端夹具稳定性修复提交 `cf521b8`，单进程窗口验收支持提交
`2865334`。16 个固定正负坐标林区的 144 个
真实区块中生成 527 个树根、24,706 个叶块和 1,044 个地被；三种树形在八 seed 均出现，
15/16 林区呈现疏密树群，12/16 林区呈现地被斑块。v1–v11 的 22 份生产样本/区块文件逐字节
不变；Debug/Release 完整 WorldRuntime 各 `1149/1149 PASS`，相关存档、备份、目录和资源回归
通过。唯一工作客户端以 PID `82431` 一次运行完成 24 个性能段和 4 个画面段；四组 P95/P99
三轮中位数比均不超过 `1.10`，最差为正坐标林地常驻 P99 `1.059×`。范围与修改前退出条件见
[E6 合同](../contracts/ecology-vegetation-mosaic-v12-e6-contract-v1.md)，生产结果见
[执行记录](../reports/ecology-e6-vegetation-v12-execution-2026-09-17.md)及
[冻结清单](../reports/ecology-e6-v11-vegetation-baseline-2026-09-17.json)。固定 v11/v12 林地和林缘
原图已保留；Computer Use 在启动后再次报告会话锁定，因此中文菜单正常玩法仍为 `NOT_RUN`，
不以诊断采集代替。

2026-09-12 新增授权：实施“暖野”第一版视觉改造，包括自然材质、HUD/菜单、地形光色及 macOS
实机与必要工程验证。保持地形和存档语义；不扩展树形、水系或 D2。见
[视觉合同](../contracts/warm-wilderness-visual-contract-v1.md) 与
[实施记录](../reports/warm-wilderness-implementation-2026-09-12.md)，当前 `Engineering Done`：材质/光色/UI、macOS 双配置回归、三档性能和正常界面自测完成；不替代独立玩法与人类审美验收。

2026-09-06 新增所有者授权：`T0/T1` 地形基线与基础地貌改善 Goal。以 macOS 完成实现、
必要回归、干净包、固定条件画面对照和正常步行验收，并本地提交；Windows 专属验证后置。
范围和退出条件见 [T0/T1 合同](../contracts/terrain-foundation-v5-contract-v1.md)，
进度见 [执行报告](../reports/terrain-t0-t1-execution-2026-09-06.md)。不扩展到 D2/T2+ 或新渲染系统。

| 批次 | 状态 | 当前结论 |
| ---- | ---- | -------- |
| `T0` Terrain Baseline | `Done` | 8 seed、1852224 点、128 区块兼容快照、三场景原图、三轮静态/流送性能及实现前冻结门槛已本地提交 `6dbf7eb`；详见执行报告。 |
| `T1` Terrain Foundation v5 | `Doing` | 实现已提交 `850dd85`；v1–v4 指纹兼容、v5 统计与 23 项聚焦、当批 macOS 双配置 1014 项世界回归、干净包固定画面及三轮性能通过。独立正常创建/保存重开通过；当前包再测世界与设置仍可操作，但持续步行、采集与结构路线因 CUA 输入能力受阻，T1/Goal 未完成，详见报告及 [补验](../reports/m1-t1-acceptance-continuation-2026-09-12.md)。 |
| `AL-A0` Latest Architecture Baseline | `Done` | 架构、依赖、性能、验证与 AI 证据身份已冻结；VS2017/v141 Debug/Release 完整门禁和 real window 通过，没有受跟踪 Gameplay/runtime/resource/build input 改动。详见 `docs/reports/architecture-lab-baseline-v1.md`。 |
| `AL-A1` World Responsibility Map | `Done` | 78 个公开方法已按 3 个 API concept / 9 个 responsibility 分类；public-surface hash 和集合一致性门禁已接入完整 Windows 验证，VS2017/v141 Debug/Release、832/832 世界、80/80 资源、122/122 配方、15/15 启动负例、104 项干净包与 real window 全部 PASS。没有迁移旧调用、增加 Facade wrapper 或开始 AL-A2。 |
| `AL-A2` Chunk Runtime Boundary | `Done` | 既有 Chunk Update Queue、Mesh Work Planner、单 loader 及 preload/unload 协调已迁入 `ChunkRuntime`；World 公开面、共享锁、预算、save v12 与 unload 语义保持不变，没有引入 B1 Residency 状态机。VS2017/v141 Debug/Release 完整门禁、两轮 832/832 世界和 104 项干净包通过。详见 `docs/reports/architecture-lab-a2-chunk-runtime-report-v1.md`。 |
| `AL-A3` Simulation Runtime | `Done` | 具体 `WorldSimulation` 已承接既有 20 Hz fixed-tick 编排；8 phase 顺序、context、最近一次 tick 原始耗时和 caller-owned pause 均有自动门禁，玩法所有权与 78 项 World 公开面不变，没有引入 A5 Scheduler/Metrics/Budget。VS2017/v141 Debug/Release、两轮 838/838 世界和 104 项隔离包通过。详见 `docs/reports/architecture-lab-a3-world-simulation-report-v1.md`。 |
| `AL-A4` Event / Command / Query Boundary | `Done` | `IWorldCommand / addCommand` 已取代旧 event-as-command 路径；事实事件不可变并区分 Domain/Diagnostic，生产订阅者显式声明 effect/republish，有界递归、诊断隔离和订阅快照语义均有自动门禁。VS2017/v141 Debug/Release、846/846 世界与 104 项隔离包通过；没有进入 A5。详见 `docs/reports/architecture-lab-a4-event-command-query-report-v1.md`。 |
| `AL-A5` Tick Phase Metrics & Budget Vocabulary | `Done` | Actor、Combat、Block Random Tick、Population 的 last-tick processed/deferred/budget scope/status 词汇和开发者面板已经冻结；VS2017/v141 Debug/Release、两轮 853/853 世界与 104 项隔离包通过，没有改变 phase 顺序、hard limit 或 Gameplay，也没有进入 A6/B1/D1。详见 `docs/reports/architecture-lab-a5-simulation-metrics-report-v1.md`。 |
| `AL-A6` Architecture Lab Documentation Pipeline | `Done` | 单一 living tutorial 已按 Track/真实 Section 重组；manifest、七段非空结构、证据路径和无占位 Part 由新 validator 及四个负例保护。VS2017/v141 Debug/Release、两轮 853/853 世界与 104 项隔离包通过；运行时代码、Gameplay、save v12 与 AI/人类声明均未改变。详见 `docs/reports/architecture-lab-a6-documentation-pipeline-report-v1.md`。 |
| `B1` Chunk Residency State Machine | `Done` | Data Residency、CPU Mesh 与 Ogre Render 已成为三套正交且有断言保护的状态机；38/38 聚焦用例覆盖光照失效、保存失败回滚、dirty eviction、stale build/upload、卸载持久化和直接结构生成/重载。VS2017/v141 Debug/Release 完整门禁均为 863/863 WorldRuntime，104 项干净包 SHA-256 为 `F76F5A1429C869FDA94FDD37BF34F8266866B5F665D1E550CA04F15CCD7ECF88`。没有进入 B2-B9，Gameplay、预算和 save v12 保持不变。详见 `docs/reports/architecture-lab-b1-chunk-residency-report-v1.md`。 |
| `B2` Streaming Demand Model | `Done` | Player、Camera、TeleportDestination、Preload 已统一为四槽、可合并、可过期的需求模型；26/26 聚焦用例与 AL-A1..B1 静态门禁通过。VS2017/v141 Debug/Release 完整门禁均为 875/875 WorldRuntime，104 项干净包 SHA-256 为 `9955E51AD94C7E5600325329031432E16C1859F936FA880F68F65F3B80369503`；未进入 B3-B9。详见 `docs/reports/architecture-lab-b2-streaming-demand-report-v1.md`。 |
| `B3` Generic World Job Scheduler | `Done` | 两类真实后台工作已进入 typed scheduler；9/9 聚焦用例、B2 26/26 与 B1 38/38 回归通过。VS2017/v141 Debug/Release 完整门禁均为 884/884 WorldRuntime，104 项干净包 SHA-256 为 `B983889B5553FF0DBFEAF6C14D2E349CC81AD1205C314CC39C0894C2D1CD9459`。未进入 B4-B9，Gameplay、预算与 save v12 保持不变。详见 `docs/reports/architecture-lab-b3-world-job-scheduler-report-v1.md`。 |
| `B4` Cancellation & Generation Token | `Done` | uint64 generation、`Cancelled` outcome、六类失效入口、detached load candidate、线性化提交和 mesh 回滚已实现；B4 10/10，B3/B2/B1 回归 9/9、26/26、38/38。VS2017/v141 Debug/Release 完整门禁均为 894/894 WorldRuntime，104 项干净包 SHA-256 为 `A9A7CC9AF528F3C725ACC13A62A69FB6D10718AD25F7F4796F5E47B70453C33A`。未进入 B5-B9，Gameplay、预算与 save v12 保持不变。详见 `docs/reports/architecture-lab-b4-world-job-cancellation-report-v1.md`。 |
| `B5` Streaming Backpressure | `Done` | 两类真实 job 已受 128 hard cap、96/48 watermarks、显式 admission、确定性 shedding 和 plan-window refill 约束；loader commit、CPU-ready upload、unload 分别受 8/8/8 边界保护。B5 12/12 及 B4/B3/B2/B1 10/10、9/9、26/26、38/38 已通过；完整 VS2017/v141 Debug/Release 门禁均为 906/906 WorldRuntime，104 项干净包 SHA-256 为 `7D126B31B78F3A4E8F8C90A5D769028EC686C0D4F708D1F6B2E2979BD164050B`。详见 `docs/reports/architecture-lab-b5-streaming-backpressure-report-v1.md`。 |
| `B6` Spatial Activation | `Done` | Resident Data、Near Representation、Simulation Requested 三层兴趣已接入真实 plan/load→mesh→render/unload 路径，simulation request 只发布不消费。B6 12/12、B5/B4/B3/B2/B1 回归 12/12、10/10、9/9、26/26、38/38；完整 VS2017/v141 Debug/Release 门禁均为 918/918，104 项隔离包 SHA-256 为 `C8E260E00CF76C952150EBC3DC851A7EDE5E13FE63A58F98B77DC103723EFA3C`。详见 `docs/reports/architecture-lab-b6-spatial-activation-report-v1.md`。 |
| `B10` Large World Stress & Acceptance | `Done` | 最终 schedule v3 Release Core 已完成 1800 秒/36000 ticks、五阶段、10 次保存重开和双确定性探针：最大区块 216、最大 pending 95、消费者 8/8/8、最大/最终 `Absent=0`，峰值 private 137551872 bytes，wall 1824.453 秒且未超时。两次失败均保留并修复，未放宽阈值或使用性能例外。组成式 VS2017/v141 Debug/Release 门禁均为 920/920；最终 Q1 六组比较 PASS；104 项隔离包 SHA-256 为 `E13203F8E18382A4D13ABA22DFB975DDB189B2858F74DAAE340CE5A4A8F34B14`。详见 `docs/reports/architecture-lab-b10-large-world-stress-report-v1.md`。 |
| `C1` Block Capability Model | `Done` | 现有 Chest/Furnace 已通过 `BlockDefinition` 声明 `InventoryProvider`，Furnace 另声明 `MachineProcessor`；Ogre 容器 UI 改为能力发现/访问，17/17 聚焦用例与静态门禁通过。VS2017/v141 Debug/Release 完整门禁均为 937/937 WorldRuntime，104 项隔离包 SHA-256 为 `1618ACD7995FE5181169B0B46A5D4F479F63FA1CCB8B533B358ED694A3846EB6`。未预建 Registry、MechanicalPort、C2 Machine Runtime 或 C3 网络。详见 `docs/reports/architecture-lab-c1-block-capability-report-v1.md`。 |
| `C2` Machine Runtime v0 | `Done` | 可制作、放置、Use、保存重开的手摇 Crusher 已成为第二个真实 Processor；共享 Runtime 只提炼 Furnace/Crusher 已共同证明的五态、配方匹配、输出容量、动力、单 tick 与原子完成语义。C2 聚焦 51/51、完整 VS2017/v141 Debug/Release WorldRuntime 963/963、Recipe 126/126、Resource Pack 80/80 和 15/15 启动负例均通过；105 项隔离包 SHA-256 为 `B4D73704A93B4377EB26336592448B4E31439387BA6198B49C59704517775739`。save v12、terrain v4、settings v8、8 工具和 34 目标不变；未进入 C3+、MechanicalPort、网络或自动物流。详见 `docs/reports/architecture-lab-c2-machine-runtime-report-v1.md`。 |
| `C3` Mechanical Topology Model v0 | `Done` | C2 Crusher 是唯一真实节点；六面相邻、确定性最小位置 component id、canonical edge、同步 BFS merge/split、Chunk unload/reload 和 save/reopen 派生重建已完成，正常容器 UI 与 Debug 面板均可观察。聚焦 Debug 68/68；完整 VS2017/v141 Debug/Release 均为 WorldRuntime 980/980、Recipe 126/126、Resource Pack 80/80 和启动负例 15/15；105 项隔离包 SHA-256 为 `8CC3ED1FC37A0F115D57C3C56349BE3278AA4B35D9DAD33975D9B45B3D46776F`。save v12 不变，未进入 C4 动力传播或通用网络。详见 `docs/reports/architecture-lab-c3-mechanical-topology-report-v1.md`。 |
| `D1` Simulation Phase Scheduler v0 | `Done` | 已证明 Managed Actors、Random-Tick Sections、Furnace/Crusher Block Entities 三条真实 workload 的共同 admission 问题，并实现确定性 64/4/32 item budget、稳定集合 round-robin/FIFO service window 和 copied diagnostics。VS2017 Debug 聚焦 24/24，AL-A5 14/14、B6 12/12、C2 51/51、C3 68/68 回归通过；完整 Debug/Release 门禁均为 WorldRuntime 991/991、Recipe 126/126、Resource Pack 80/80、启动负例 15/15，105 项隔离包 SHA-256 为 `0B34CD34265ED1A4F88FD5833975FD328FB026FCD6B13A0FAFE9710859F1B2F6`。save v12、20 Hz、8 phase barrier 不变；未进入 D2+。详见 `docs/reports/architecture-lab-d1-simulation-phase-scheduler-report-v1.md`。 |

## 下一候选

2026-09-12 所有者要求的视觉升级方案已完成，见
[暖野视觉升级实施方案 v2](visual-upgrade-plan-v2.md)，依据
[暖野 v1 实施结果](../reports/warm-wilderness-implementation-2026-09-12.md) 与本项目现有画面问题分析。
方案列出 `WV2-00..08` 的资源规格、文件触点、兼容策略、性能预算和验收；建议首先实施
`WV2-00..03` 的高清材质/叶簇林地样板，现已由后续用户指令授权并纳入上方当前批准批次。
`WV2-04..08` 仍为 `Planned / NOT_RUN`；不改变 T1 未完成状态、D2 候选条件或 T2+ 范围。

2026-09-06 所有者要求的地形/游戏画面文档分析已完成，形成负坐标基础地貌、
水系、植被、洞口、材质与三维山体的候选建议及验收方式。
其中 T0/T1 已由后续 Goal 明确授权并纳入上表；T2+ 和渲染候选仍未批准，不改变下表 D2 状态。

| 批次 | 状态 | 目标 | 进入条件 | 退出边界 |
| ---- | ---- | ---- | -------- | -------- |
| `D2` Simulation Activation | `Candidate / not approved` | 在已 Resident 空间内区分 Full / Reduced / Dormant 模拟保真度。 | D1 完整门禁通过，且至少一个真实远距离 Actor/Machine workload 证明仅靠 item budget 仍不足。 | 不由 B6 spatial interest 或 D1 自动授权；不得提前开始 D3-D8。 |

`B10` 已完成正式压力/确定性、组成式完整门禁和 Q1 收口；`C1-C3` 也已依次通过完整门禁。
项目所有者已单独批准并完成 D1。2026-09-05 又批准本次 TODOLIST Goal：调查 D2 进入条件、
补充待执行验收并修正文档一致性；若真实 workload 证明 D1 budget 不足，可直接实施 D2。
当前调查观察到 32 台远处空闲 Furnace 会使近处 Crusher 在 20 ticks 内推进 19 步，符合 D1
已有超载合同；尚未建立正常玩法或正式性能不可接受的证据，故本次保留 D2 Candidate，未开始
运行时实现。详见 [D2 进入调查](../reports/simulation-activation-entry-investigation-2026-09-05.md)。
B7-B9、C4-C11、D3-D8 与 Extended 不在此次 Goal 范围。

本次 Goal 仍在执行。所有者随后明确授权以 macOS 完成可对应的测试、干净包与窗口验收，
Windows 专属验证后置，不再作为本次 Goal 的必需退出项；已有 Windows 历史证据不改写。
当前布局修复已通过 macOS Xcode Debug/Release 的 13 套回归及客户端探针
（`build/xcode-validation-20260905143633`）。此前修订的 gmake 双配置回归、
gmake/Xcode Release 启动负例各 15 类通过，保留原版本证据，不视作本轮重新执行。
Xcode Release 已去除会破坏 infinity/isfinite 校验的 fast-math。主菜单鼠标误捕获已修复，
并通过菜单、合成及暂停界面的实际点击复查。独立 AI 验收尚未关闭；进度、命令和环境限制见
[执行记录](../reports/todolist-goal-execution-2026-09-05.md)。候选池不会随本次更新自动扩张。

## 当前阻塞

- 工程开发没有已知主干阻塞。
- 独立窗口绑定和截图保存已恢复，不再作为当前阻塞。工具脚本及真实截图证据见
  [脚本窗口报告](../reports/macos-script-window-20260906-evidence/report.md)。独立新六组合验收
  曾受子任务授权上下文拒绝影响，所有者现已明确批准新独立验收任务，实际执行结果待回收。历史原因见 [拒绝记录](../reports/ai07-ui-matrix-20260906-evidence/independent-report.md)；
  不把该拒绝泛化为整个桌面权限不可用，也不通过其他工具重试被拒绝的创建操作。
- 当前任务环境若没有 OS 级 Computer Use，AI 场景保持 `NOT_RUN`；这不阻塞独立 Sprint 的
  `Engineering Done`，但 Track 不得标记 `AI Playability PASS`。
- 严格 `AI-06` 还要求 package-only 文件系统访问；仅切换工作目录但仓库仍可读取时记录
  `BLOCKED`，不能声明 blind PASS。
- 本次 Goal 以 macOS 为当前验收平台；Windows 11 虚拟机正在执行用户自己的编译，按所有者
  后续指示后置 Windows 验证，不干扰编译。macOS PASS 不替代历史或新的 VS2017/v141 PASS。
- D2 进入调查已给出保留候选的决定；后续若出现新证据，再按本次条件式授权评估，不能从
  `Candidate` 直接推导 `Done`。

## 待执行验收

本次 Goal 的所有者授权 macOS 平台对应执行下列功能场景。表内原 Windows 场景定义保留作为
后续 Windows 路由；新增证据使用 `platform=macOS` 独立记录，不合并为跨平台 PASS。
AI-06 的独立 package-only 访问要求及音频实际证据要求继续有效。

| 范围 | 状态 | 关闭方式 |
| ---- | ---- | -------- |
| `AI-01` 基础窗口功能 | `BLOCKED（macOS 部分通过）` | 独立验收已验证菜单、建档/读档、暂停及语言持久化；移动/视角/按住键受工具限制。快捷键缺陷已修复并通过独立复测，见 `../reports/ai01-macos-6916867-evidence/report.md`。 |
| `AI-02..AI-04` 容器/战斗/旅程 | `NOT_RUN` | 本次在 macOS Release 干净包中以正常 OS 输入执行菜单、容器、战斗、保存和重启；Windows 路由后置。 |
| `AI-05` Stage 11 scripted | `NOT_RUN` | 制作/放置火把、建造、工具职责、探索奖励、洞口、战斗和 Waystone 共鸣。 |
| `AI-06` AI 盲玩 30 分钟 | `BLOCKED（隔离环境）` | 当前工具仍允许读取仓库，尚不满足 package-only；须在仓库不可访问的新任务中执行，只声明 AI 可理解性，不外推人类留存或乐趣。 |
| `AI-07` 视觉/本地化/音频 | `BLOCKED（部分视觉通过）` | 创建表单/目标HUD双语三档复验通过，十位seed列双语大字号及低档复验通过；已验证真实截图无损落盘路径；2026-09-06 独立图片补采因窗口绑定长时间无响应未执行，脚本启动与父上下文菜单诊断已记录；随后新独立上下文已完成主菜单/Worlds往返且原始截图已保存；直接脚本截图也已验证，替代编辑器转存。新六组合验收在创建测试世界时被自动审批拒绝，尚未执行；完整动态和音频证据仍缺失。见本次执行记录，不声明整体AI-07 PASS。 |
| `AI-08` 完整可玩载体 | `NOT_RUN` | 每个实际完成 Track 结束时，从主菜单运行到胜利、保存重开和该 Track 的正常玩法 Demo。 |
| 新 macOS 视觉/玩法运行 | `BLOCKED（部分通过）` | 已由本次 Goal 后续指示纳入范围；从当前源码构建的带哈希干净 Release 包按对应 AI 场景执行。 |

历史 R3 v1 / Physical Input v2 门槛保持 `SUPERSEDED`；开发者既有部分自测继续作为历史证据，
不改写成 AI 或真人 PASS。

## 历史摘要

- 拆分前正式总账：73 个 `Done`、4 个历史 `Verify`，详见
  `docs/archive/project-ledger-2026-08-17.md`。
- Stage 9/BETA-RC 工程封板已完成，详见
  `docs/reports/beta-release-candidate-report-2026-08-26.md`。
- Stage 10/VISUAL-RC Windows 工程封板已完成，详见
  `docs/reports/visual-release-candidate-report-2026-08-28.md`。
- Stage 11/P11F PLAYABILITY-RC Windows 工程封板已完成，详见
  `docs/reports/playability-release-candidate-report-2026-08-31.md`。
- 已结束的视觉与可玩性路线分别冻结在
  `docs/archive/visual-quality-roadmap.md` 和
  `docs/archive/playability-experience-roadmap.md`。

## Architecture Lab 约束摘要

- Capability Map ≠ Backlog。
- Core ≠ 自动批准；Extended 未触发时不进入任务账本，不形成延期欠账。
- A2 = refactor existing behavior；B1 = introduce new runtime lifecycle。
- A5 = phase metrics vocabulary；D1 = scheduler/runtime behavior change。
- B6 = Streaming / representation interest；D2 = simulation fidelity。
- Concrete Mechanical Network → 第二个已批准具体网络 → 观察重复 → 可选 Shared Network Core。
- 教程先维护一份 `docs/current/architecture-lab-tutorial.md`，内部五个 Part；不按 Sprint 新建文件。
