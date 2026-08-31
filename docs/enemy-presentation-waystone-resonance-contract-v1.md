# P11E 敌人表现与路标共鸣合同 v1

> Architecture Lab 迁移说明（2026-08-31）：工程合同保持冻结；轮廓/姿态可辨识、战斗与共鸣
> 功能映射到 `docs/ai-assisted-gameplay-acceptance-v1.md`。人类危险感、掉落价值感和战斗乐趣
> 为 `NOT_CLAIMED`。

状态：`Engineering Done`。敌人轮廓、玩法状态驱动的关键姿态、死亡表现隔离、身份化掉落、
Waystone 共鸣机制、保存边界、双配置构建和自动回归已关闭；AI 动态辨识与完整守护战场景为
`NOT_RUN`，人类危险感、掉落价值感和战斗乐趣不登记为 PASS。

## 目标与范围

P11E 只重做既有 Stalker、Brute、Spitter 与两类 Waystone 守护者的表现，并为既有路标战增加一个
会改变操作时机的共鸣反制。它不新增敌人、材料、音频样本、伤害类型或持久玩法状态，也不修改
敌人的 AABB、伤害、攻击距离、攻击时序、自然生成上限或 Stage 10 渲染管线。

## 轮廓与关键姿态

| 类型 | 部件数 | 轮廓职责 |
| ---- | ------ | -------- |
| Stalker | 6 | 窄躯干、长臂和短腿，突出快速接近与单臂攻击。 |
| Brute | 6 | 宽躯干、粗臂和宽腿，突出重击蓄力。 |
| Spitter | 7 | 横向躯干、四足与独立 muzzle，突出远程压制。 |
| Waystone Stalker/Brute | 7 | 在原型轮廓上增加 crest，保持守护者身份。 |

- 单个 profile 的硬上限为 8 个立方体部件；未知/兼容类型继续使用 1 个立方体，不因缺少 profile
  阻止世界载入。
- 姿态只读取 `ActorSnapshot`。Idle 提供头部偏转，Chase 提供相反相位的四肢步态，Windup 按
  原型提供单臂、双臂或 muzzle 蓄力，Recover 提供跟随动作，命中反馈只施加有界 roll/scale。
- 攻击姿态直接使用既有固定 tick 战斗状态与剩余 tick；渲染层不生成伤害、不推进 FSM，也不反向
  改写 actor 位置、碰撞盒或判定窗口。已有 HUD/字幕/战斗音频继续作为低表现能力下的危险提示。

## 死亡表现隔离

- 正常死亡从模拟 actor 列表移除后，生成一个只读的表现快照，持续精确 8 个固定 tick；同时存在的
  死亡快照最多 32 个，超过上限时按 FIFO 丢弃最旧项。
- 死亡快照可供 `OgreActorRenderer` 播放侧倒、下沉和轻微缩放，但不会进入碰撞、选取、攻击、自然
  生物数量、Waystone 守护者数量、保存状态或区块卸载所有权。
- 卸载、距离清理和显式移除不是死亡，不创建死亡快照；重开世界也不会恢复任何死亡姿态。

## 身份化掉落与 Alpha 兼容

六个正式敌人定义不再把 Dirt 作为共同掉落。P11E 不追加材料身份，全部复用现有资源经济：

| 敌人 | 保底/身份掉落 |
| ---- | ------------- |
| Natural compatibility mob | Plant Fiber ×1 |
| Stalker | Plant Fiber ×1、Wheat ×1..2、Raw Meat ×1 |
| Brute | Plant Fiber ×1、Coal Ore ×1、Wheat ×1、Raw Meat ×1..2 |
| Spitter | Plant Fiber ×1、Wheat Seeds ×1..2 |
| Waystone Stalker | Iron Ore ×1 |
| Waystone Brute | Iron Ingot ×1 |

Plant Fiber 是三类自然敌人的共同“战斗战利品”信号，因此 `alpha.collect_mob_loot` 改为拾取
Plant Fiber；无论玩家先击败哪一种自然敌人，十步 Alpha 旅程都不会因身份化掉落而卡住。Wheat、
Coal Ore、Wheat Seeds 与 Raw Meat 继续表达各原型的额外价值，Waystone 守护者仍使用独立矿物奖励。

## Waystone 共鸣

- 在主结局守护战或胜利后第 1/2 波中再次使用同一个 Waystone Core，会尝试发出一次共鸣脉冲。
- 只有 core 中心 8 格内、存活、已登记且正处于 `Windup` 的 Waystone 守护者会被打断；脉冲不造成
  伤害、不直接移除敌人，只施加 1.25 knockback，并进入 12 tick Recover，原因记为
  `resonance_interrupted`。
- 成功脉冲进入精确 80 tick 冷却。冷却中返回 `ResonanceCharging`；范围内没有合格 Windup 目标时
  返回 `ResonanceNoTarget` 且不消耗冷却，避免误触惩罚。
- 三种结果都有严格对齐的 `en-US`/`zh-CN` 语义反馈 key。共鸣只改变时机选择，不提高守护者血量、
  数量或奖励，也不复制新的普通波次。

## 版本、上限与保存边界

- world save 保持 v12，terrain 保持 v4，settings 保持 v8，enemy registry 保持 v3，目标定义保持
  v3；`BlockId` 26 项与 `Material::ID` 42 项不变。
- 双语文本由各 408 增至各 411 key；资源 manifest 路径数量仍为 84。
- 共鸣冷却、战斗 FSM、关键姿态与死亡表现全部是运行时瞬态，既不序列化也不迁移；重开世界从
  cooldown 0 和既有安全战斗状态开始。
- 自然生物仍受世界 12/局部 4 上限约束。多部件 profile 与死亡快照各有独立硬上限，不能通过
  生成或死亡风暴形成无界渲染对象。

## 自动验收证据

2026-08-30 的 VS2017/v141 验证结果：

- Debug/Release P11E 焦点各 `35/35`，覆盖轮廓/姿态、死亡快照、身份掉落、双语反馈、共鸣范围、
  80 tick 冷却、save v12 隔离和重开复位。
- Release 完整 `HelloMine3DWorldRuntimeSmoke` 为 `832/832`；Alpha G6 实际击杀自然敌人、拾取
  Plant Fiber、保存并重开全链通过。
- Recipe Debug/Release 各 `122/122`，ResourcePack Debug/Release 各 `80/80`；资源 manifest
  `84` 项并通过缺项/陈旧项负例。
- 主客户端 Debug/Release 均成功使用 VS2017 v141 编译链接，多部件 renderer 已进入真实客户端；
  输出只包含既有 OGRE/FreeImage 编码、弃用语法和第三方警告。

## AI/Computer Use 验收边界

以下项目映射到 `AI-05`、`AI-07` 和 `AI-08`，当前为 `NOT_RUN`：

1. 在 Release 窗口中不显示名称，分别从中远距离识别 Stalker、Brute、Spitter 和 Waystone 守护者，
   记录误判与需要靠近到的距离。
2. 实际观察 Idle、Chase、Windup、Recover、受击和死亡姿态，记录攻击窗口是否比旧单立方体清晰，
   以及是否存在抖动、穿插或不舒适反馈。
3. 用每类自然敌人的正式掉落完成一次采集/恢复/制作计划，验证身份奖励能够进入后续路线。
4. 完整走通一次 Waystone 守护战，至少成功用共鸣打断一次 Windup，并记录误触、冷却判断、死亡原因
   和策略变化。
5. 在活动桌面上补采适用 Q1；macOS 原生窗口仅在另行批准目标平台工作时执行，当前为
   `NOT_RUN`。

未执行的 AI 场景不能写成 PASS；人类辨识舒适度、奖励价值感和战斗乐趣为 `NOT_CLAIMED`。
