# HelloMine3D AI 辅助玩法验收规范 v1

状态：`Active`（验收模型已生效；首份 Windows Computer Use 记录为 `NOT_RUN`）

最后更新：2026-08-31

## 1. 定位

HelloMine3D 是个人架构学习与工程展示项目，不以招募外部玩家、商业发行或收集真人体验样本为
前提。项目定位可以是 **Architecture Lab**，但载体必须始终是一款能够通过正常界面和输入完成
真实玩法闭环的游戏。

因此，原有“真人验收统一延期”的做法由本规范取代：

- 可以确定性判断的正确性、兼容性、性能和资源边界继续由自动化验证；
- 必须通过真实窗口、菜单、键鼠路径和保存重开的功能流程由 AI + Computer Use 黑盒执行；
- 截断、重叠、破面、闪烁、状态可辨识等可观察问题由 AI 视觉检查；
- 项目所有者仍可补充开发者自测，但它不再是默认退出条件；
- 人类乐趣、舒适度、审美偏好和物理设备手感不由 AI 冒充，统一明确为 `NOT_CLAIMED`。

本规范只改变后续项目退出模型，不回写历史事实。此前的 Physical Input、产品体验合同和
PLAYABILITY-RC 报告仍准确描述它们生成时的证据状态。

## 2. 状态模型

记录必须把“谁/如何执行”和“结果/声明范围”分开，不使用一个含糊的 `Done` 同时表达全部含义。

### 2.1 证据类型

| evidence_type | 含义 |
| ------------- | ---- |
| `AUTOMATED` | 测试程序、资源校验、迁移夹具、性能比较、soak 或发行包脚本。 |
| `AI_INTERACTIVE` | AI 通过 OS 级 Computer Use 操作真实 Release 客户端。 |
| `AI_VISUAL` | AI 实际检查原尺寸截图、多帧序列或可访问的视频证据。 |
| `DEVELOPER_SELF_TEST` | 项目所有者在真实设备上的补充自测。 |
| `LEGACY_HUMAN` | 历史真人/物理输入合同；保留审计意义，不再是 Architecture Lab 默认门禁。 |

### 2.2 执行结果

| result | 含义 |
| ------ | ---- |
| `PASS` | 在记录的构建、环境、步骤和声明范围内通过。 |
| `FAIL` | 已执行且观察到不符合合同的结果。 |
| `BLOCKED` | 已开始执行，但被环境、工具或产品缺陷阻断。 |
| `NOT_RUN` | 尚未执行；不得改写成 PASS，也不写成“默认延期”。 |

### 2.3 声明状态

| claim_state | 含义 |
| ----------- | ---- |
| `CLAIMED` | 有与声明范围匹配的 PASS 证据。 |
| `NOT_CLAIMED` | 项目主动不声明该主观或物理体验质量，不表示功能失败。 |
| `OUT_OF_SCOPE` | 当前路线明确不支持的目标或平台范围。 |
| `SUPERSEDED` | 历史退出条件仍保留，但已被新的项目定位和证据合同取代。 |

例如，Computer Use 能够证明 `functional_playability=CLAIMED`，但同一记录必须同时写明
`human_fun=NOT_CLAIMED` 和 `physical_input_feel=NOT_CLAIMED`。

## 3. AI 可以与不可以关闭的范围

| 范围 | 当前处理 |
| ---- | -------- |
| 菜单、移动、视角、制作、容器、战斗、死亡、保存和重开 | `AI_INTERACTIVE` 可关闭功能范围。 |
| Alt+Tab、最小化、窗口关闭、焦点恢复 | 仅 OS 级 Computer Use 可关闭；事件注入和 headless 测试只是预检。 |
| UI 截断/重叠、缺字、破面、严重过黑、闪烁、轮廓和关键姿态 | `AI_VISUAL` 可关闭可观察范围。动态项必须使用多帧、视频或连续窗口观察。 |
| 音频事件存在、字幕语义、暂停/静音/切换生命周期 | 自动化加可访问录音/Computer Use 可关闭功能范围。 |
| 鼠标实际移动距离、设备力度、键位舒适度、眩晕或疲劳 | `NOT_CLAIMED`；可保留开发者自测。 |
| “是否好玩”“奖励是否令人兴奋”“画面是否漂亮”“音乐是否悦耳” | `NOT_CLAIMED`，不得由 AI 结论改写为人类体验 PASS。 |
| Windows VS2017/v141 Release | Architecture Lab 的主要运行平台。 |
| macOS | 已有历史 Xcode/原生/TSan 证据继续有效；新的原生视觉或玩法运行保持 `NOT_RUN`，除非后续批次明确把 macOS 加入退出范围。 |

AI 的有效声明是“该代理在给定环境下能够理解并完成流程”或“证据中未观察到合同列出的可见
缺陷”，不是“人类玩家普遍会喜欢或感到舒适”。

## 4. 黑盒运行规则

用于关闭 `AI_INTERACTIVE`，以及为正式 `AI_VISUAL` 产生运行证据的记录必须满足：

1. 从带 SHA-256 的干净 Windows Release 包启动，不从开发构建目录借用资源。
2. 通过正常主菜单创建或打开世界，使用正常键鼠、UI 和游戏内动作。
3. 正式步骤不得启用 validation-only、fixture、物品注入、传送、强制位置/旋转、存档编辑、
   调试生成、时间冻结或直接调用 Gameplay API。
4. 执行后可以只读检查日志、存档摘要、截图和性能文件，不能用它们替代窗口内的实际步骤。
5. 记录所有重试、超时、意外弹窗、焦点丢失和偏差；失败后重新运行不得删除第一次失败。
6. 固定构建、窗口尺寸、图形设置、seed 和存档身份；对确定性比较使用相同身份。
7. 盲玩/可理解性检查必须使用独立的新 AI 任务；其初始工作目录为解包后的发行包根目录，
   可访问文件系统范围不得包含仓库、源码、测试名、精确配方路径或内部路线图，只提供发行包、
   默认控制说明和高层目标。
8. 实现者与验收者必须使用不同任务上下文。记录需要列出工作目录、可访问根、初始提示和仓库
   是否可访问；仅改变当前目录但仍能读取仓库不算隔离。无法保证 package-only 访问时，严格
   `AI-06` 记录为 `BLOCKED`，可保留探索观察，但不得据此声明 blind PASS。

脚本化 AI 运行验证功能正确性；盲玩 AI 运行只提供“AI 可理解性代理”证据。二者不能互相替代。

### 4.1 AI-06 隔离合同

`AI-06` 只有同时满足以下条件才能使用 `context_isolation=PACKAGE_ONLY`：

- 独立任务只挂载或授权读取解包后的发行包和本次验收输出目录；
- 仓库根、源码、测试、合同、路线图和既有攻略对验收任务不可访问；
- 记录 package SHA-256、初始工作目录、全部可访问根、初始提示和提供的控制说明；
- 验收过程中没有追加实现细节、配方答案、目标坐标或调试路径。

只能做到“新任务”或“新工作目录”，但仓库仍可读取时，使用
`context_isolation=PARTIAL`，严格 `AI-06 result=BLOCKED`。该次运行仍可作为非门禁观察附在记录中，
但 `functional_playability` 和 AI 可理解性声明保持 `NOT_CLAIMED`。

### 4.2 AI-07 动态与视听证据合同

- UI、双语和布局使用未缩放的原尺寸窗口截图，并记录窗口、locale、UI scale、图形档位和时间点；
- 闪烁、关键姿态、轮廓、粒子和状态切换必须由按时间排序的多帧序列、可访问视频或 Computer
  Use 连续窗口观察关闭；单张静态图不能关闭动态项；
- 每组多帧证据记录捕获时间、原始文件路径和哈希。`tools/run_render_capture.ps1 -CaptureMs ...`
  已能生成多个时间点，可用于开发预检；在它能够直接针对带哈希的发行包可执行文件运行并证明
  身份一致前，不能单独替代正式的干净包 AI-07 证据；
- 首次正式 AI-07 可以直接由 OS 级 Computer Use 在干净包中连续观察并保存截图/视频，无需先
  开发另一套抓帧工具；正式运行不得使用 fixture、validation override 或开发构建目录；
- 声音 cue、字幕和暂停/静音生命周期需要可访问录音或 Computer Use 实际听取记录。只有日志、
  dummy backend 或字幕存在时，不能把音频存在性写成 PASS；无法取得音频证据时相应子项为
  `BLOCKED`；
- AI-07 可以拆成视觉、本地化、动态和音频子记录；总 PASS 必须引用所有必需子记录和 artifacts。

## 5. 首批场景

| 场景 | 覆盖 | 通过条件 |
| ---- | ---- | -------- |
| `AI-01 client-shell` | 主菜单、创建世界、移动/视角、暂停恢复、Alt+Tab、最小化、窗口关闭、设置重启 | 无输入穿透、卡键、回焦跳变或异常退出；设置准确恢复。 |
| `AI-02 container-d2` | D2 | 正常取得/放置箱子，打开、双向转移、按钮/Escape 关闭；数量守恒，UI 拥有输入。 |
| `AI-03 combat-d4` | D4 | 正常遭遇并选中敌人，攻击、受伤、死亡、重生；动作单一消费且无重复死亡/卡键。 |
| `AI-04 journey-d6` | D6 | 作物取得/种植、容器使用、战斗、掉落拾取、显式保存退出、重启检查恢复。 |
| `AI-05 stage11-scripted` | P11-0 至 P11E | 制作并使用火把、建可关闭落脚点、比较工具职责、发现并切换目标、完成三类结构奖励、找到山地/洞口、完成 Waystone 共鸣。 |
| `AI-06 stage11-blind-30m` | P11C 可理解性代理 | 新 AI 在无内部答案的情况下记录 30 分钟行动、里程碑、停滞、查找和改计划；只声明 AI 可理解性。 |
| `AI-07 visual-l10n-audio` | Stage 10/11 产品表现 | 检查固定视觉矩阵、双语/三档 UI scale、关键动态多帧、声音 cue/字幕和暂停/静音生命周期。 |
| `AI-08 full-playable-carrier` | Architecture Lab Track 退出 | 从主菜单进入新世界，经采集、制作、成长、战斗、探索、胜利、保存重开，再完成当 Track 新增的真实游戏 Demo。 |

场景可以拆成多次记录，但一个总 `PASS` 只能引用全部必需子记录。`AI-06` 失败可以说明引导或
信息呈现问题，但不能单独证明人类会困惑；它必须以代理证据措辞记录。

## 6. 历史 D2/D4/D6/R3 映射

| 原项目 | Architecture Lab 当前状态 | 关闭方式 |
| ------ | -------------------------- | -------- |
| D2 | `Engineering Done`；`AI_INTERACTIVE=NOT_RUN` | `AI-02` PASS 后标记 `Done（AI Functional）`。 |
| D4 | `Engineering Done`；`AI_INTERACTIVE=NOT_RUN` | `AI-03` PASS 后标记 `Done（AI Functional）`。 |
| D6 | `Engineering Done`；`AI_INTERACTIVE=NOT_RUN` | `AI-04` PASS 后标记 `Done（AI Functional）`。 |
| R3 v1 / Physical Input v2 | 原物理合同 `SUPERSEDED` 为当前退出门槛；记录保持 `NOT_RUN` | 新增 `R3-AI-Functional`，由 `AI-01` 至 `AI-04` 覆盖；物理设备手感 `NOT_CLAIMED`。 |

原 R3/Physical Input 模板不得因本迁移而填写 PASS。项目所有者先前完成的部分自测继续保留为
`DEVELOPER_SELF_TEST` 历史证据。

## 7. Stage 11 映射

| 批次 | AI 证据 | 不声明范围 |
| ---- | ------- | ---------- |
| P11-0、P11-1 | `AI-05` 交互 + `AI-07` 视觉 | 人类建造乐趣、审美偏好。 |
| P11A | `AI-01` 至 `AI-04` | 物理鼠标距离和键鼠舒适度。 |
| P11B | `AI-05` 动态交互 + `AI-07` 多帧/视频 | 人类打击感、眩晕和长期舒适度。 |
| P11C | `AI-06` 盲玩代理 | 人类首次体验、留存和乐趣。 |
| P11D | `AI-05` 证明奖励能力确实改变可执行操作 | 奖励吸引力和主观价值感。 |
| P11-2 | `AI-05` 通行 + `AI-07` 轮廓/洞口可见性 | 人类对风景的审美判断。 |
| P11E | `AI-05` 战斗/共鸣 + `AI-07` 轮廓/姿态 | 人类危险感、掉落价值感和战斗乐趣。 |
| P11F | 自动工程证据保持 PASS；AI 场景当前 `NOT_RUN` | 不再等待不存在的外部玩家签字。 |

## 8. Architecture Lab 门禁

- 每个 Sprint 必须通过受影响的自动测试、存档/迁移、性能和干净构建门槛，并让新结果在游戏内
  可观察。
- Sprint 可以标记 `Engineering Done`，但没有适用的 AI 交互证据时不得写成
  `AI Playability PASS`。
- 每个 Track 结束必须生成干净 Release 包并执行 `AI-08`；Track 新系统必须通过正常玩法获得、
  建造、破坏、保存、卸载/重载和重开，而不是只存在于调试 UI。
- 如果当前任务环境没有 OS 级 Computer Use，记录为 `NOT_RUN`，在具备该能力的 Windows 环境
  执行；不得使用截图数量、隐藏测试或事件注入补写 PASS。
- 架构改造导致既有主菜单到胜利流程不可完成时，该 Track 不得 Done。

## 9. 最小记录字段

每次 AI 验收至少记录：

```text
record_version=1
date=YYYY-MM-DD
commit=<40-hex>
package_sha256=<64-hex>
configuration=Release
evidence_type=<AI_INTERACTIVE|AI_VISUAL>
executor=<agent/model identity>
control_method=<computer-use environment>
os=<version>
gpu_driver=<adapter and driver>
window=<width>x<height>;fullscreen=<0|1>
graphics=<relevant settings>
locale=<en-US|zh-CN>
seed=<world seed>
save_identity=<world id>
scenario=<AI-01..AI-08>
working_directory=<absolute package root>
accessible_roots=<package/output roots>
repository_accessible=<true|false>
context_isolation=<PACKAGE_ONLY|PARTIAL|NOT_APPLICABLE>
initial_prompt=<verbatim prompt or artifact path>
normal_input_only=true
debug_or_fixture_used=false
result=<PASS|FAIL|BLOCKED|NOT_RUN>
retries=<integer>
timeouts=<integer>
unexpected_dialogs=<integer and summary>
artifacts=<screenshots/video/log/save-summary paths>
deviations=<none or exact list>
functional_playability=<CLAIMED|NOT_CLAIMED>
human_fun=NOT_CLAIMED
physical_input_feel=NOT_CLAIMED
```

当前只冻结记录规范，不声称已经产生 Computer Use PASS。后续实现结构化模板或校验器时应另立
小批次，不能把未运行的模板当成验收证据。
