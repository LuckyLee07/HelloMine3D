# HelloMine3D 当前待办

本文只回答三件事：项目现在做到哪里、接下来做什么、什么会阻塞开发。
已完成里程碑的逐项实现和验收证据已经冻结到
`docs/project-ledger-2026-08-17.md`，不再挤占当前任务视图。

最后更新：2026-08-29。

## 产品目标

把 HelloMine3D 从技术可靠的体素沙盒推进成一个更接近完整单机游戏的产品：玩家能够
创建和管理世界，采集资源，制作工作台与工具，获得成长反馈，暂停和调整设置，听到
基础声音，并在保存、退出、恢复后继续游玩。BETA-RC 与 Stage 10/VISUAL-RC Windows 工程
闭环后的产品方向评审已经决定暂不进入 1.0 缺陷封板，转入 Stage 11 可玩性与操作体验路线。
P11-0、P11A 与 P11B 已完成工程范围，当前计划开发批次为 P11-1 建造材料与工具分化；其余批次按固定顺序排队。

当前采用“先完成可开发工程、人工项后置”策略。编译、定向自动测试、数据守恒、存档兼容和当前
环境可执行的自主验收必须当批执行；真人输入、主观短试玩、活动桌面 GPU、macOS 原生及正式产品
体验统一保持 `Verify/Deferred`，不阻塞下一工程批，也不冒充 PASS。

## 文档分工

| 文档 | 用途 |
| ---- | ---- |
| `docs/todolist.md` | 当前状态、活跃任务和开发顺序。 |
| `docs/project-ledger-2026-08-17.md` | 2026-08-17 拆分前的完整历史总账和逐项验收证据。 |
| `docs/iteration-plan.md` | 长期阶段、依赖关系和产品方向。 |
| `docs/game-development-roadmap.md` | G5/G6、Alpha 后内容批次与最终封板的详细开发计划。 |
| `docs/beta-gameplay-roadmap.md` | Release Candidate 后 Stage 9 / Beta 的 N7-N12 预排批次、版本边界和回归合同。 |
| `docs/visual-quality-roadmap.md` | BETA-RC 后 Stage 10 的 V10A、V10B1-B3、V10C-V10E、VISUAL-RC 视觉升级批次、版本预案与性能护栏。 |
| `docs/playability-experience-roadmap.md` | Stage 11 的 P11-0/P11A-P11F/P11-1/P11-2 顺序、版本预案、Physical Input v2 和 PLAYABILITY-RC 门禁。 |
| `docs/world-light-source-contract-v1.md` | P11-0 火把定义、图集与配方、metadata 驱动发射、局部重光照和末尾追加的身份约束。 |
| `docs/core-input-feel-contract-v1.md` | P11A 动作仲裁、settings v7 鼠标绑定、hold/toggle、灵敏度、焦点门和延期边界。 |
| `docs/action-feedback-contract-v1.md` | P11B 裂纹、粒子、动作/战斗 HUD、掉落物上限、settings v8 强度回退和延期边界。 |
| `docs/vertex-lighting-contract-v1.md` | V10A 四角采样、AO/光照合成、对角线、greedy 重建和 mesh-dirty 边界。 |
| `docs/terrain-material-profile-contract-v1.md` | V10B1 图集/tile/颜色参数、资源包覆盖、CPU/GPU 坐标兼容和启动失败边界。 |
| `docs/directional-shadow-contract-v1.md` | V10D settings v5、方向阴影档位、回退、性能和开发者视觉证据。 |
| `docs/post-processing-contract-v1.md` | V10E settings v6、轻量后处理、能力回退、性能和开发者视觉证据。 |
| `docs/visual-release-candidate-report-2026-08-28.md` | VISUAL-RC 最终 Windows 门禁、视觉矩阵、Q1/Q3、资源许可、发行包和跨平台边界。 |
| `docs/validation-matrix.md` | 代码改动类型到验证命令的映射。 |
| `docs/manual-product-experience-acceptance-v1.md` | 与 R3 分离的开发者视觉检查、正式视觉/双语可读性和听感验收合同。 |
| `docs/iteration-report-template.md` | 新迭代的统一回归记录模板。 |
| `docs/runtime-validation.md` | 运行时覆盖范围、已有证据和限制。 |
| `docs/minigame-reference.md` | 外部 MiniGame 的架构/反模式参考及 Stage 9 定向映射，不是当前任务来源。 |
| `docs/alpha-development-checkpoint-v1.md` | G6 后冻结的旅程、迁移、性能和 H2 开发基线。 |

状态含义：`Queued` 已预排但前序证据门尚未开放；`Planned` 是下一批准开发批次；`Todo`
尚未开始；`Doing` 已有实现但未闭环；`Verify` 功能已实现、只差最终验收；`Done` 已实现并有
足够证据。

## 当前基线

| 领域 | 当前状态 |
| ---- | -------- |
| 现有玩法 | 已有移动、方块放置/破坏、箱子、作物、自然生物、战斗、死亡重生、物品拾取和保存恢复。 |
| 世界可靠性 | K1 世界目录、K2 事务保存、K3 有界备份与验证恢复、K4 世界管理和主菜单入口已经完成。 |
| 玩法数据层 | G1-G6、N1-N6、N7A-N12C、BETA-RC 与 Stage 10/VISUAL-RC 均已完成 Windows 工程范围；Stage 11 的 P11-0/P11A/P11B 工程已完成，P11-1 是下一开发批次。 |
| 性能观测 | Q1 七类场景和 Q2 有界阶段计时已闭环；VISUAL-RC 六类 Windows 基线/复测和 Q3 双档正式长稳全部通过。 |
| 崩溃诊断 | H1-H3 已闭环：本地 dump、脱敏 sidecar、混合栈离线符号、独立符号归档和下次启动提示均通过，默认不上传。 |
| 跨平台 | macOS Debug/Release、真实窗口、31 项 Xcode 工程图和原生 TSan 门禁已有证据。 |
| 当前 Windows 门禁 | 2026-08-28 VISUAL-RC 工程封板通过：77 项资源、261 项图集、38 个性能夹具、11 项 Stage 10 补充合同、VS2017/v141 双配置、743/743 世界、80/80 资源包、117/117 配方、15/15 启动负例、145,169 字节 dump 和 97 文件干净包均通过；六类正式 Q1 与 nominal/stress 各 1800 秒 Q3 全部 `PASS`。发行 ZIP SHA-256 为 `ACCB99B3F984D5FB8EF11279280406F8477ED625719F4801479DB6F7FE7A6005`；macOS 原生、正式产品体验和 Physical Input v2 保持 Verify。 |

## 进度摘要

拆分前总账共有 77 个正式任务；历史总账仍为 73 个 `Done`、4 个 `Verify`。新增的 Stage 9
批次不回写该统计：`FS1-FS3`、`N7A/N7B`、`N8A/N8B`、`N9A/N9B`、`N10`、`N11A/N11B`
与 `N12A/N12B/N12C`、`BETA-RC` 工程封板当前为 `Done`。新增 Stage 10 不回写历史统计，
当前建立的 8 个视觉批次均已完成 Windows 工程范围，VISUAL-RC 为 `Done（Windows 工程）`；
V10B1/V10C/V10D/V10E 的 macOS shader 子状态因当前无目标机器保持 `Verify`。
R3 已做部分非正式真人自测；现有 v1 十二项记录尚未完成，而且其范围不足以单独关闭当前
D4/D6 和 Stage 9/10 的全部人工体验。Stage 11 新增九个不回写历史统计的批次：P11-0/P11A/P11B 为
`Engineering Done`，P11-1 为 `Planned`，其余五批为 `Queued`。2026-08-28 的内容与视觉复核在原有六批之间插入了 P11-0、
P11-1 和 P11-2 三个内容密度批次，理由是原有批次的退出条件依赖世界里并不存在的内容。
P11A 的 Physical Input v2 已定义但真实执行保持 Deferred，并与视觉/双语/听感验收分开；不能因
工程完成而自动关闭 D2/D4/D6/R3。

| 类型 | 数量 | 任务 |
| ---- | ---- | ---- |
| 已完成的第 8 阶段正式任务 | 16 | K1-K4、G1-G6、H1-H3、Q1-Q3 |
| Stage 9 已完成 | 16 | FS1 首次进入正确性、FS2 天空/水面/颜色、FS3 体素美术/HUD、N7A 结局状态与文本键基础、N7B 路标胜利闭环、N8A 战斗可读性、N8B 远程敌人与投射物、N9A 确定性结构框架、N9B 遗迹/营地与生态战利品、N10 食物/冶炼/资源经济、N11A 难度档案、N11B 胜利后事件、N12A 本地化完成、N12B 正式采样音效、N12C 低密度音乐、BETA-RC 工程封板 |
| Stage 10 已完成 | 8 | V10A 顶点平滑光照/AO、V10B1 材质/图集管线、V10B2 原创材质资产、V10B3 生态着色/确定性变体、V10C 定向大气/立体云、V10D 可选方向阴影、V10E 轻量后处理、VISUAL-RC Windows 工程封板 |
| Stage 10 待开发 | 0 | 无；VISUAL-RC 后产品方向评审已完成 |
| Stage 11 工程已完成 | 3 | P11-0 世界光源与洞穴照明、P11A 核心操作手感、P11B 动作反馈（人工/Q1/动态观感子项 Deferred） |
| Stage 11 下一批 | 1 | P11-1 建造材料与工具分化（Planned） |
| Stage 11 已排队 | 5 | P11C 前 30 分钟、P11D 探索奖励、P11-2 地形轮廓与洞口、P11E 敌人表现、P11F PLAYABILITY-RC |
| 等待最终验收 | 4 | D2、D4、D6、R3 |

## Stage 10 视觉质量待办

Stage 10 只提升已有世界的表现，不改玩法、世界生成、save v11 或 terrain v3。Luanti 只作为
公开职责和算法参考；默认独立实现，不复制其 LGPL 源码或任何第三方素材。权威范围、截图矩阵、
许可证和逐批退出条件见 `docs/visual-quality-roadmap.md`。

| 批次 | 状态 | 优先级 | 计划交付 | 退出边界 |
| ---- | ---- | ------ | -------- | -------- |
| V10A 顶点平滑光照与 AO | Done | P0 | 四角采样、透明规则、确定性对角线、greedy 重建保护和边角 dirty 传播已实现；solid 共面顶点复用、全局 repeat UV、18³ lazy 光照样本缓存、uniform-light 与非共享 pass 快路把 14-face 夹具从 56 压到 36 顶点。最终 VS2017/v141 双配置聚焦为 39/39、Release 完整世界为 716/716，保持 32 字节顶点布局；八场景 AO/no-AO 共 16 张截图和开发者窗口检查已通过。 | 项目所有者于 2026-08-27 批准 exact AO 性能例外：fast 旧核心预算通过；scaled P95 只超旧上限 0.045 ms。AO-disabled 诊断证明 +41.8% 索引主要来自 exact AO 内部明暗边界；后续批次未继承例外，最终身份已在 VISUAL-RC 重跑并通过 Q1/Q3。合同见 `docs/vertex-lighting-contract-v1.md`。 |
| V10B1 材质与图集管线 | Done（macOS Verify） | P0 | v1 profile 参数化 atlas/tile/颜色，CPU/GPU 共用像素中心，三类 terrain pass 从冻结资源视图同步 uniform；保持 V10A AO、`shapedLight=0.24` 和合成顺序。 | 双配置资源包 54/54、V10B1 世界 4/4、Release 完整世界 718/718、12 类启动负例、65 项 manifest、默认静态前景 216,000 像素一致和 5 分钟 Release 检查通过；macOS Release shader 窗口仍为 `Verify`，合同见 `docs/terrain-material-profile-contract-v1.md`。 |
| V10B2 原创材质资产 | Done | P0 | 37 个原创语义 tile、top/side/bottom、四类 Alpha、双语 layout、确定性生成与许可已落地；HUD/手持改用冻结 atlas profile。 | 自动合同 106/106、资源包 56/56、Release 世界 718/718、13 类启动负例、67 项 manifest、3 张隐藏固定图和 85 文件干净包通过；项目所有者授权 Codex 逐图完成静态视觉检查，记录为 PASS，合同见 `docs/material-visual-contract-v1.md`。 |
| V10B3 生态着色与确定性变体 | Done | P1 | 五生态、五表面组、每组三变体以 4x4x4 坐标小块接入世界网格，变体进入 greedy key；UI/手持与非着色材质保持基础身份。 | terrain v3/save v11/32 字节顶点不变；261 项图集、732 项 Release 世界、相关 Q1、短 Q3 和五图开发者静态检查通过，合同见 `docs/ecology-appearance-contract-v1.md`。 |
| V10C 定向大气与立体云 | Done（Windows；macOS Verify） | P1 | terrain/water/actor/sky 已共享定向雾；有界空间云层具备高度、厚度、绝对时间速度、云底/云顶和稳定层内处理。 | 双配置聚焦 21/21、Release 世界 741/741、资源包 65/65、14 类启动负例、相关 Q1 和十图多帧开发者检查通过；所有者批准单样本可见延迟例外，macOS 待补。合同见 `docs/directional-atmosphere-contract-v1.md`。 |
| V10D 可选方向阴影 | Done（Windows；macOS Verify） | P1 | 单近景方向 shadow map 已提供 Off/Medium/High；settings v5、v0-v4→Off、双语 key、独立 Off shader、2×2 PCF、距离淡出、能力回退和资源清理已落地。 | 双配置聚焦 21/21、资源包 75/75、Release 世界 742/742、74 项 manifest、14 类启动负例、9 项档位合同、强制回退、同场景各档性能和六图开发者视觉 PASS；最终 frame P95/P99 无需性能例外，macOS 冒烟待补。合同见 `docs/directional-shadow-contract-v1.md`。 |
| V10E 轻量后处理 | Done（Windows；macOS Verify） | P2 | settings v6、v0-v5→Off、可关闭 tone curve/确定性抖动/八采样极轻 bloom、HUD 排除、能力回退和 Stage 10 补充身份已落地。 | 双配置聚焦 22/22、资源包 80/80、Release 世界 743/743、77 项 manifest、15 类启动负例、11 项性能合同、真实支持/强制回退、同档三次中位数和六图开发者视觉检查通过；无需性能例外，macOS 冒烟待补。合同见 `docs/post-processing-contract-v1.md`。 |
| VISUAL-RC 视觉集中封板 | Done（Windows 工程；macOS/产品体验 Verify） | P0 | 已固化逐批开发者检查、最终渲染身份、性能、许可证、干净包和 bundle 边界。 | Windows 全门禁、正式 Q1、nominal/stress 各 1800 秒 Q3、239 项 Xcode 工程图静态检查、资源/许可/Credits 和 97 文件干净包通过；macOS 原生及正式人工体验保持 Verify，不 push、不自动打标签。报告见 `docs/visual-release-candidate-report-2026-08-28.md`。 |

Stage 10 的历史开发顺序为
`V10A -> V10B1 -> V10B2 -> V10B3 -> V10C -> V10D -> V10E -> VISUAL-RC`。每批实现、
验收并单独本地提交后再进入下一批。该路线已经完成，产品方向评审已选择下文 Stage 11；
Stage 10 不再追加渲染批次来掩盖玩法和操作问题。

## Stage 11 可玩性与操作体验待办

Stage 11 的固定顺序为
`P11-0 -> P11A -> P11B -> P11-1 -> P11C -> P11D -> P11-2 -> P11E -> P11F`。工程批次在前一批
代码、自动回归、双配置和可执行自主验收关闭后逐个开放；真人/主观项保留 Deferred 清单。
权威范围、版本预案、试玩记录字段和非目标见 `docs/playability-experience-roadmap.md`。

| 批次 | 状态 | 优先级 | 计划交付 | 退出边界 |
| ---- | ---- | ------ | -------- | -------- |
| P11-0 世界光源与洞穴照明 | Engineering Done（Q1/真人 Deferred） | P0 | 火把、Rose 修正、metadata 熔炉发射和局部重光照已落地。 | 双配置、自动/视觉证据完成；活动桌面 Q1 与真人动态试玩后置。合同见 `docs/world-light-source-contract-v1.md`。 |
| P11A 核心操作手感 | Engineering Done（Physical Input v2 Deferred） | P0 | 单一世界动作仲裁、鼠标重绑定、线性相对增量、焦点门、hold/toggle 与 settings v7 已落地。 | VS2017/v141 双配置、定向 88/88、资源包 80/80 和完整世界回归通过；真实键鼠/Alt+Tab/重启后置。合同见 `docs/core-input-feel-contract-v1.md`。 |
| P11B 动作反馈与游戏汁水 | Engineering Done（真人动态观察 Deferred） | P0 | 十级裂纹、材质粒子、第一人称动作、命中/受击/冷却、拾取、音量微变体和 settings v8 强度回退已落地。 | VS2017/v141 双配置、定向 84/84、Release 完整世界 786/786 和资源包 80/80 通过；真人动态观察后置。合同见 `docs/action-feedback-contract-v1.md`。 |
| P11-1 建造材料与工具分化 | Planned | P0 | 木板、圆石、门/活板门的最小建造集，`axe`/`shovel` 工具 class，清理孤儿方块资产。 | 玩家能造出可关闭的落脚点；工具选择产生可感知速度差；`BlockId`/`Material::ID` 末尾追加、旧存档零迁移；资源经济可达性与无环校验通过。 |
| P11C 前 30 分钟重构 | Queued | P0 | 建造、成长、探索并行循环，至少两个可见可选目标和自然配方发现。 | 新世界前 30 分钟真人试玩没有长时间目标空白，并出现至少两项有意义的并行选择；首版不强推饥饿。 |
| P11D 改变玩法的探索奖励 | Queued | P1 | 结构专属资源/蓝图、实用能力解锁和生态差异经济。 | 主要奖励改变玩家下一步计划或能力，不再以普通矿物和重复铁奖励为主。 |
| P11-2 地形轮廓与洞口 | Queued | P1 | 山地生物群系与高度域重映射、天然洞口，形成 `terrain v4`；水文生成另行立项。 | 远景出现真实垂直落差；洞穴可从地表发现；旧世界保留创建时生成身份、不回填；按新地形身份重采 Q1 基线后比较，不放宽 Stage 10 已批准的护栏。 |
| P11E 敌人表现与战斗层次 | Queued | P1 | 多部件体素轮廓、完整关键动作、敌人身份化掉落与独特 Waystone 守护阶段。 | 不依赖名称即可辨识主要敌人和攻击窗口；敌人掉落通过经济/存档边界；新增敌人必须带来新决策与新奖励。 |
| P11F PLAYABILITY-RC | Queued | P0 | 汇总逐批试玩、迁移、性能、保存恢复、干净包和延期证据。 | 冻结构建/硬件、里程碑耗时、误操作、死亡和困惑时段；全门禁与适用 Q1/Q3 通过，不用自动化伪装真人结论。 |

## 当前阻塞修复

| ID | 优先级 | 状态 | 内容 | 完成条件 |
| -- | ------ | ---- | ---- | -------- |
| BLD-1 | P0 | Done | Windows 游戏逻辑目标统一继承 `dbghelp.lib`，资源包与崩溃验证脚本同步修复过期假设。 | WorldRuntimeSmoke/Soak 正确链接；Debug/Release 全量重建、346 项运行时检查、受控 dump 和干净包均通过。 |

BLD-1 是保持主干可开发的构建修复，不代表要立即执行所有发布级验收。

## 玩法主线

这些任务直接决定项目是否更像完整游戏，优先级高于集中补齐发布证据。

| ID | 状态 | 优先级 | 玩家价值 | 近期完成条件 | 依赖 |
| -- | ---- | ------ | -------- | ------------ | ---- |
| K4 | Done | P0 | 已交付主菜单世界列表，以及创建、打开、重命名、备份恢复和可恢复删除。 | 44 项目录/命令断言全通过；主菜单与直进世界均完成隐藏后台冒烟；稳定 id/目录、失败回滚和删除恢复合同见 `world-management-contract-v1.md`。 | K1-K3 |
| G2 | Done | P0 | 已交付玩家 2×2 制作区、工作台 3×3、纯预览、原子领取、批量制作和制作 UI。 | 54 项配方/制作断言与 351 项世界回归通过；满背包、连点版本、关闭、方块生命周期和重载守恒见 `crafting-contract-v1.md`。 | G1 |
| G3 | Done | P0 | 已交付手/木镐/石镐三级采集能力、数据驱动等级/速度/掉落、渐进破坏、耐久和工具状态保存。 | 64 项配方/工具、365 项世界和 20 项资源包断言在 Debug/Release 全通过；旧库存迁移、死亡保留、容器拒收和 UI 进度见 `tool-progression-contract-v1.md`。 | G2 |
| G4 | Done | P1 | 已交付真正冻结模拟的暂停状态，以及显示、视距、FOV、输入灵敏度、反转 Y 和分类音量设置。 | 386 项世界回归覆盖暂停门、草稿/取消/默认值、范围、版本迁移、原子失败、即时 FOV/视距与 seed 隔离；Debug/Release 隐藏客户端退出 0，合同见 `runtime-settings-contract-v1.md`。 | K4 |
| G5 | Done | P1 | 已交付 UI、方块、拾取、制作、战斗和环境基础音频，以及 Windows `waveOut`/dummy 双后端。 | 资源清单 40 项、资源包 23/23、世界运行时 403/403；Debug/Release 隐藏校验和三帧启动退出 0，缺定义/设备静默降级，合同见 `audio-feedback-contract-v1.md`。 | G2-G4 |
| G6 | Done | P0 | 已交付从创建世界到收集木材、制作工作台与两级工具、取得铁矿、战斗拾取、保存和重开的十步 Alpha 旅程。 | 45 项目录、16 项事务、19 项备份和 420 项世界运行时断言在 Debug/Release 通过；版本 1-3 迁移、版本 4 旅程标志、非法位拒绝和随机种子起步见 `playable-alpha-contract-v1.md`。 | K4、G2-G5 |
| N1 | Done | P0 | 已交付严格数据驱动目标、事件消费进度、当前/下一目标 HUD、明确首轮终点和 G6 兼容门面。 | 版本 5 保存完成集合与部分进度；v1-v3 空迁移、v4 标志迁移、未知 ID 保留、429 项世界回归、24 项资源包回归和 41 项清单见 `objective-system-contract-v1.md`。 | G6 |
| N2 | Done | P0 | 已让煤和铁矿进入真实用途，形成熔炉、燃料、铁锭、铁镐和铁剑的第二段成长。 | 三槽守恒、固定 tick、暂停/卸载无离线追赶、负载 v1、5 项目标和真实攻击均通过；447 项世界、65 项配方/工具、25 项资源包与 43 项清单见 `smelting-progression-contract-v1.md`。 | N1、G2、G3 |
| N3 | Done | P0 | 已让小麦和战斗损伤进入真实恢复循环，面包可主动恢复生命，第一版不引入持续饥饿压力。 | 严格食物定义、3 小麦面包配方、固定 tick 冷却、原子食用、死亡次 tick 重生、v6 生命/冷却存档及 v5 迁移、2 项目标均通过；463 项世界、76 项配方、26 项资源包与 44 项清单见 `food-recovery-contract-v1.md`。 | N2、D4、D5 |
| N4 | Done | P1 | 已交付 Stalker/Brute、木/石/铁剑、固定 tick 近战、一次性有界掉落和三步战斗目标。 | v7 存档、成功命中才损耗、AABB 距离、接触伤害差异和掉落去重通过；477 项世界、92 项配方/敌人、27 项资源包和 45 项清单见 `combat-depth-contract-v1.md`。 | N3、D3-D4 |
| N5 | Done | P1 | 已增加五类生态身份、沙漠敌人压力、seed 稳定的跨区块路标和两步探索目标。 | terrain v1/v2、v7→v8 迁移、加载顺序、单核心和性能身份通过；487 项世界、92 项配方/材料/敌人、27 项资源包和 46 项清单见 `ecology-exploration-contract-v1.md`。 | N4、Q1-Q2 |
| N6 | Done | P1 | 已统一 UI 信息层级、配方/目标可读性、九项可重映射键位、UI 缩放、操作提示和声音字幕，并补齐食用失败提示。 | 设置 v2 迁移与冲突拒绝、音频字幕、目标历史和配方书装填通过；491 项世界、95 项配方、27 项资源包、46 项清单及 Debug/Release 完整门禁通过。物理输入和产品体验分别后置。 | N5、G4-G5 |
| FS1 | Done | P0 | 让真实新世界首次打开时确定性落在安全陆地，并能在附近取得首轮木材；世界创建默认提供随机建议种子。 | 双配置世界回归 532/532、目录 45/45 和隐藏 Release 客户端通过；seed 0 重开稳定，救援保留身份/掉落物且不移动已有进度玩家。 | K4、N5 |

## 可靠性和性能跟随项

这些能力继续保留，但不应阻塞 G6 和后续内容批次的功能开发。

| ID | 状态 | 优先级 | 内容 | 后续闭环条件 |
| -- | ---- | ------ | ---- | ------------ |
| H1 | Done | P1 | Windows Release 本地 minidump，默认不上传。 | 2026-08-17 后台受控崩溃生成 117,741 字节 dump；保存重开通过且无 `.pending` 残留。 |
| H2 | Done | P1 | 已交付版本化脱敏 sidecar、实际 dump 混合栈离线符号和独立确定性符号归档。 | 21/21 检查、匹配/错误 PDB、受控触发项目帧和七项符号归档通过；回退栈扫描明确标注，不输出绝对路径。 |
| H3 | Done | P2 | 已交付干净包和下次启动的本地崩溃提示。 | 最终 66 文件包不含符号/旧崩溃；受控包崩溃后恰好发现一份报告，可打开、复制或忽略，不联网。 |
| Q1 | Done | P1 | 冷启动、进入世界、保存、恢复、快速移动、规模玩法和 Alpha 综合流程的版本化性能场景。 | 六类 RC baseline/repeat 加 Alpha 场景全部 PASS；批准 profile 和摘要跟踪在 `docs/baselines/`。 |
| Q2 | Done | P1 | 启动/目录/世界进入/保存/备份/恢复的有界阶段计时。 | 实际保存 `195.821/160.612 ms`、恢复 `53.861/58.516 ms`，两轮阶段/计数/预算全部通过。 |
| Q3 | Done | P2 | 已完成快速移动、内容规模、队列、内存和长时间 soak。 | schedule v2 的 nominal/stress 各串行运行 1800 秒并双 PASS；峰值私有内存分别为 18,878,464/28,123,136 字节，稳定段增长 4,558,848/7,602,176 字节，峰值句柄均为 260 且稳定增长均为 0。 |

## 等待最终验收

以下功能已经实现，不需要现在中断玩法开发去集中补证据。

| ID | 状态 | 剩余事项 |
| -- | ---- | -------- |
| D2 | Verify | 用真实键鼠确认箱子 UI 的打开、转移、Escape/关闭和焦点隔离。 |
| D4 | Verify | 用真实鼠标确认目标选择、攻击、受伤、死亡和重生输入链路。 |
| D6 | Verify | 用真实输入完成作物、箱子、战斗、保存和重启流程。 |
| R3 | Verify（部分自测，延后） | 已非正式尝试第 1、8 项以及方块破坏/拾取路径；v1 十二项正式记录未完成。P11A 已建立可机器校验的 Physical Input v2 十三项合同，补齐 D4 的受伤/死亡/重生与 D6 的作物/保存/重启范围，但真实运行仍 Deferred，并与视觉/双语/听感产品验收分开。 |

## 推荐开发顺序

后续先按下面的当前批次推进，再进入预排内容包。一个批次只有在“玩家路径、失败边界、持久化和定向测试”
同时满足后才进入下一批；正式硬件与真人输入证据仍可集中封板。

| 批次 | 主任务 | 本批交付 | 退出条件 |
| ---- | ------ | -------- | -------- |
| 0：恢复基线（Done） | `BLD-1`，跟随 `H1/Q1/Q2` | 修复 DbgHelp 链接传播；跑 Windows Debug/Release 完整门禁；保存一份不批准预算的 Release 性能参考快照。 | 主干恢复可编译；H1 已闭环；Q1 schema、Q2 计时和本地参考快照均通过。 |
| 1：世界入口（Done） | `K4` | 世界管理命令层、主菜单世界列表、创建/打开/重命名、备份恢复和可恢复删除。 | 44 项定向断言通过；普通启动进入主菜单，自动化直进路径兼容；真人输入记录现归 Physical Input v2。 |
| 2：制作闭环（Done） | `G2` | 纯制作预览、最大可制作数量、原子领取、玩家制作区、工作台 3x3 和 UI。 | Debug 定向门禁已通过；预览、材料、产物、容量、关闭、连点、方块生命周期和重载均守恒。 |
| 3：工具成长（Done） | `G3` | 数据驱动工具表、渐进破坏、采集等级/速度/掉落、耐久和版本化保存。 | 手、木、石三级采集能力形成升级循环；旧存档迁移、工具恢复及双配置自动门禁均通过。 |
| 4：产品外壳（Done） | `G4` | 暂停状态机、设置草稿、应用/取消/默认值和版本化用户配置。 | 暂停门、配置迁移、原子保存、即时 FOV/视距、重启生效边界和双配置自动门禁均通过；真人 UI 操作归 Physical Input v2，观感归独立产品体验合同。 |
| 5：反馈层（Done） | `G5` | 音频事件、2D/3D/listener/音量接口、Windows `waveOut` 与 dummy 后端、程序化基础提示。 | 七类提示与业务事件接线完成；403/23 项回归和双配置隐藏客户端通过，缺设备/定义不阻断游戏。 |
| 6：可玩 Alpha（Done） | `G6` | 从干净启动串联世界创建、采集、工作台、工具升级、战斗、保存退出和继续游戏。 | 十步正常 API 旅程、跨系统数据守恒、v1-v3 迁移和 v4 重开恢复均通过。 |
| 7：Alpha 开发检查点（Done） | `H2`、`Q1/Q2` 跟随 | 已冻结 G6 旅程和 v3 迁移样本，批准 Alpha 性能基线，建立脱敏 sidecar/离线符号工具骨架。 | 双配置 420/420 与 16/16、隐藏客户端、性能复测、匹配/错误符号均通过；证据见 `alpha-development-checkpoint-v1.md`。 |
| 8：内容扩展（Done） | `N1-N6` | 已完成目标、成长、恢复、战斗掉落、生态探索、统一 UI、配方/目标可读性、键位辅助与声音反馈。 | N6 未改写已冻结的玩法、生成和存档语义；491/95/27 项定向断言及双配置完整门禁通过，合同见 `product-experience-contract-v1.md`。 |
| 9：发行候选封板（人工项延后） | Physical Input v2、产品体验 | H2-H3、Q1-Q3 和最终 Windows 自动门禁已完成；R3 v1 只有部分非正式自测。 | v1 十二项不再自动关闭当前 D4/D6；未来分别完成扩展物理输入与视觉/双语/听感记录，不阻塞 Stage 10。 |

## Stage 9 / Beta 预排批次

下列批次已经落成可回归路线，`FS1-FS3`、`N7A/N7B`、`N8A/N8B`、`N9A/N9B`、`N10`、
`N11A/N11B`、`N12A/N12B/N12C` 与 `BETA-RC` 均为 `Done`；它们不计入拆分前
77 项历史总账。详细范围见 `docs/beta-gameplay-roadmap.md`，FS1-FS3 的合同见
`docs/first-session-visual-baseline-contract-v1.md`。
详细范围、版本预案、回归触发器和批次合同见 `docs/beta-gameplay-roadmap.md`。

| 批次 | 状态 | 优先级 | 计划交付 | 启动/退出边界 |
| ---- | ---- | ------ | -------- | ------------- |
| FS1 首次进入正确性 | Done | P0 | 确定性安全陆地出生、附近木材、随机建议种子和受限旧占位世界救援。 | 双配置 532/532、目录 45/45、真实隐藏客户端通过；不改 seed/terrain 身份，不移动有进度玩家。 |
| FS2 天空、水面与颜色 | Done | P0 | 独立水面表现、风格化云层、雾色和整体颜色基线。 | 固定森林/海岸/昼夜隐藏截图、双配置 536/536、隐藏 Ogre 客户端及相关性能比较通过。 |
| FS3 体素美术与 HUD | Done | P1 | 统一图集、快捷栏图标、手持物和基础交互反馈。 | 双配置 539/539、资源包 27/27、47 项清单、来源记录、隐藏 HUD/容器截图和焦点守卫通过；真人输入与观感已拆到各自人工合同，RC0 不再承担收口。 |
| RC0 基线收口 | Superseded | P0 | bundle、文档一致性和自动基线已被 BETA-RC 工程封板覆盖。 | 不再作为活跃批次；剩余真人输入归 Physical Input v2，R3 v1 不自动关闭 D4/D6。 |
| N7A 结局状态与文本键 | Done | P0 | 已交付独立持久结局状态、奖励 epoch、世界列表标记和语义化中英文 key 骨架。 | 保存 v9、v1-v8 迁移、恢复、回退和幂等通过；Debug/Release 560/560、目录 48/48、资源包 28/28、49 项清单及完整门禁通过。 |
| N7B 路标激活与首个胜利闭环 | Done | P0 | 已交付 2 铁锭原子激活、2+1 固定守卫、3 铁锭一次性奖励、胜利覆盖层和 5 个终局目标。 | 双配置世界回归 580/580；阶段重开、死亡、重复事件、卸载、满背包和中途备份恢复通过；nominal/stress 各 5 秒通过；应用保持 Playing。 |
| N8A 战斗可读性 | Done | P0 | 已交付攻击前摇/恢复、方向性受击、双方击退、受击打断和持剑右键格挡。 | enemy registry v2、四态 FSM、遮挡、目标失效、每 tick 预算与调试快照通过；双配置 596/596、配方/敌人 100/100、资源包 28/28，nominal/stress 各 5 秒零失败。 |
| N8B 远程敌人与投射物 | Done | P1 | 已交付 registry v3、自然 Spitter、有界瞬态投射物、正面持剑格挡和 Ogre 表现。 | 双配置 611/611、配方/敌人 104/104、资源包 28/28；遮挡、寿命、距离、容量、死亡/卸载/重载清理及 nominal/stress 各 60 秒通过。 |
| N9A 确定性结构框架 | Done | P1 | 已复用 cell hash/`StructureBuilder`，固定结构归属、footprint、投影和覆盖优先级，并保持 terrain v2 输出。 | 16 项新增断言使双配置世界回归达到 627/627；VS2017/v141、完整 Windows 门禁、当前身份 Q1 与两组 60 秒 soak 通过。合同见 `docs/exploration-structures-contract-v1.md`。 |
| N9B 遗迹、营地与生态战利品 | Done | P1 | 已交付 terrain v3 下两类有界结构、seed 稳定箱子战利品和生态权重。 | 18 项新增断言使双配置世界回归达到 645/645；固定快照、跨区块加载顺序、库存/结构修改持久化、terrain v1/v2 不回填、VS2017/v141、完整门禁、Q1 与两组 60 秒 soak 均通过。合同见 `docs/exploration-structures-contract-v1.md`。 |
| N10 食物、冶炼与资源经济 | Done | P1 | 已交付 4 种食物、3 类冶炼、2 类燃料、19 个精确配方及经济可达性校验。 | VS2017/v141 双配置 117/117 配方与 650/650 世界回归、完整门禁、同身份 Q1 和两组 60 秒 soak 通过；守恒、来源/消耗、无环、容量和保存预算见 `docs/resource-economy-contract-v1.md`。 |
| N11A 难度档案 | Done | P1 | 已交付三档版本化难度；创建时选择，暂停菜单显式应用，下个 fixed tick 原子生效。 | 保存 v10、v1-v9→Normal、集中伤害/生成/掉落参数、metadata/Q1 身份和非法值拒绝通过；VS2017/v141 双配置 658/658 世界、53/53 目录，完整门禁、同身份 Q1 与两组 60 秒 soak 通过。合同见 `docs/difficulty-replay-contract-v1.md`。 |
| N11B 胜利后事件 | Done | P2 | 已交付三次有界回响试炼：每次显式启动、两波固定守卫、一次性 2 铁锭奖励。 | 保存 v11、payload v2、死亡取消、中途恢复、三次封顶和替换核心防重置通过；双配置 666/666 世界、59/59 目录、完整门禁、同身份 Q1 与两组 60 秒 soak 通过。合同见 `docs/difficulty-replay-contract-v1.md`。 |
| N12A 本地化完成 | Done | P1 | 已完成 `en-US`/`zh-CN`、341 个严格 key、Noto Sans SC、长文本布局、语义字幕、Credits 和许可。 | settings v3/v0-v2 迁移、双配置 675/675、32/32 资源包、51 项清单、完整门禁和同身份启动/进世界 Q1 通过；双语真人截图与可读性按独立产品体验合同延期，不标 PASS。 |
| N12B 正式采样音效 | Done | P1 | 已交付 v3 schema、9 个正式 WAV、启动缓存、Windows/dummy 后端、并发上限、Credits 和许可清单。 | 双配置 681/681 世界、34/34 资源包、隐藏真实后端、61 项清单、性能和完整门禁通过；真人听感延期。 |
| N12C 低密度音乐 | Done | P2 | 已交付单通道流式环境音乐及暂停、停止、淡入淡出、降级和清理。 | settings v4、1 条原创 20 秒曲目、352 个双语 key、64 项清单、双配置 699/699 世界与 38/38 资源包、真实 WaveOut、完整门禁和同身份 Q1 通过；真人听感按独立产品体验合同延期。 |
| BETA-RC 集中封板 | Done（工程） | P0 | 全迁移、正式性能、双档长稳、崩溃符号和新干净包已经复核。 | 六类 Q1、两档 1800 秒 Q3、R3 自动预检和 bundle 校验完成；R3 v1 未正式签署，扩展物理输入与产品体验分别延后，因此不建本地标签、不 push。 |

当前开发顺序为 `FS1 -> FS2 -> FS3 -> N7A -> N7B -> N8A -> N8B -> N9A -> N9B -> N10 -> N11A ->
N11B -> N12A -> N12B -> N12C -> BETA-RC`；RC0 已被 BETA-RC 取代，剩余人工项单独延后。
N11B/N12C 均已
纳入并完成，其余 P0/P1 基础批次未因伸缩边界删减。

批次 2/3 的历史范围没有加入烹饪、材料组替换、修理或附魔；熔炉和铁级工具现已由 N2 独立
交付。批次 5 不迁移 MiniGame 的 FMOD、直播 DSP 或平台资源层。Alpha 之后的目标系统和
熔炉/铁级成长、食物恢复、战斗掉落、生态探索和产品体验已由 N1-N6 交付；RC0 自动化基线
已经闭环，BETA-RC 工程封板也已完成；R3 维持部分自测、其余延期。Stage 9 没有后续批次，
当前 V10E 已由 Codex 基于六张真实 Release 原图完成视觉关闭；Off/On 最终性能均未超过护栏，
因此未消耗性能例外。VISUAL-RC 随后使用隐藏且不抢焦点的客户端完成 Windows 工程封板；
产品方向评审已选择 Stage 11，下一开发批次为 P11-0；macOS 原生与正式产品体验保持 Verify。
以上新增批次
仍不回写拆分前 77 项历史总账。

开发过程中按 `docs/validation-matrix.md` 选择与改动相关的最低必要验证。不要为了延后
发布验收而取消数据守恒、存档兼容、定向自动测试或主干编译。
