# N7 路标胜利合同 v1

> Architecture Lab 迁移说明（2026-08-31）：本合同冻结历史工程证据；文中 R3、真人和延期
> 描述保留当时语境，当前退出模型见 `docs/ai-assisted-gameplay-acceptance-v1.md`。

本文固定 N7A/N7B 的结局事实、状态转换、保存迁移、一次性奖励和文本资源边界。N7A 只交付
独立状态、存储/UI 只读投影和本地化骨架；N7B 才把普通玩法中的路标激活、守卫战、奖励和
胜利覆盖层接入这些模型。目标列表耗尽、敌人数量为零或 UI 可见性都不能被推断为胜利。

## 权威状态与转换

`VictoryFlow` 是唯一可以修改 `WorldOutcomeState` 的领域边界。存储、世界列表、HUD 和覆盖层
只读取快照。

| 阶段 | 含义 | 允许的下一步 |
| ---- | ---- | ------------ |
| `Unstarted` | 路标尚未激活，奖励 epoch 必须为 0。 | `activate` -> `Activated` |
| `Activated` | 激活仪式已完成，尚未进入守卫战。 | `beginEncounter` -> `Encounter` |
| `Encounter` | 有界遭遇正在进行。 | `abandonEncounter` -> `Activated`；`resolveVictory` -> `Victorious` |
| `Victorious` | 胜利已经成为持久事实，一次性奖励尚未领取。 | `claimReward` -> `RewardClaimed` |
| `RewardClaimed` | 胜利和对应 epoch 的奖励均已完成。 | 无 |

- 同一命令和同一 epoch 的重复提交返回 `AlreadyApplied`，不发布第二次事件或奖励；跳阶段、
  epoch 为 0、错误 epoch 或终态后的不同 epoch 返回 `Rejected`，且状态零修改。
- `Victorious` 要求 `rewardEpoch > 0 && claimedRewardEpoch == 0`；`RewardClaimed` 要求两个
  epoch 相等且非零。其他阶段的两个 epoch 都必须为 0。
- 世界列表完成标记只由 `Victorious`/`RewardClaimed` 派生。奖励是否领取不会撤销胜利，
  目标系统也不能覆盖这个结论。

## 保存 v9 与恢复

- 世界保存从 v8 升为 v9，新增 `world_outcome_phase`、`world_outcome_reward_epoch` 和
  `world_outcome_claimed_epoch`。三个字段在 v9 必须同时存在；非法阶段、不一致 epoch、旧版
  混入新字段和未来版本均拒绝，失败不能替换最后一份有效保存。
- v1-v8 读取语义保持不变，升级时精确初始化为 `Unstarted/0/0`。迁移不改变 seed、terrain
  generation version、世界 id、已有目标、玩家库存或 actor。
- v9 保存、备份和恢复都携带同一份结局事实。`Encounter` 可以被保存；N7B 恢复时必须先
  对账已有遭遇 actor，再决定恢复有界波次，禁止在已有守卫仍存在时重复生成。
- 死亡会由 N7B 通过明确命令放弃当前遭遇并回到 `Activated`；暂停和路标区块卸载只冻结
  遭遇，不改变持久阶段。恢复备份后以备份中的状态和 epoch 为准。

## N7B 路标与奖励边界

- 激活使用正常交互和现有可获得材料；材料扣除与 `activate` 成功必须守恒，满背包、死亡、
  暂停、界面占用、距离或方块身份不符时均零修改。
- 守卫战只复用现有 Stalker/Brute 或其数据参数变体，固定波数、单波数量、生成半径、活动
  actor 上限和每 tick 工作预算。无通用 Boss 框架，也不增加应用级 `Victory` 状态。
- 只有最后一波按正常伤害/死亡路径完成后才调用 `resolveVictory`。覆盖层展示后应用仍处于
  `Playing`，玩家可以继续沙盒。
- 奖励先验证库存容量，再在同一世界状态中加入物品并提交匹配 epoch 的 `claimReward`。
  满背包保持 `Victorious` 和待领取提示；保存发布失败时不能把半份结果伪装成已领取。
- N7B 增加 3-5 个有顺序的终局目标，但目标事件只是可读进度，不能成为结局权威。

## 语义文本资源

- 文本资源使用 `# HelloMine3D localized text v1`、规范 locale 和
  `text semantic.key "UTF-8 value"`。N7A 提供 `en-US` 权威回退和 `zh-CN` 骨架；key 使用
  小写语义路径，不把玩家文案继续写成数字 id 或新硬编码字符串。
- 未知 locale 回退 `en-US`；当前 locale 缺 key 时回退同名英文；所有 locale 都缺失时显示
  `[semantic.key]`。三种情况记录去重且有上限的开发诊断，不能因缺翻译崩溃。
- 文本注册表从冻结的资源视图加载且只冻结一次。资源包 v1 不拥有 `text` 类，避免未版本化
  覆盖改变胜利合同；N12A 再统一完整产品文案、字体、语言设置和截图验收。

## 自动验收与人工边界

N7A 至少覆盖完整状态转换、幂等/错误 epoch、非法恢复、v9 往返和最后有效保存、v8 迁移、
世界列表投影、双语言 key、三级 fallback、非法文本原子拒绝、资源包越权和 Debug/Release
编译。N7B 再增加从干净新世界可完成的正常胜利路径、各阶段保存重开、死亡/暂停/卸载、满
背包、重复事件、备份恢复、内容上限、短时 nominal/stress soak 和胜利后继续游玩。

R3 真人输入当前只有部分自测，其余按用户决定延后；该事实不会被自动化标为 `PASS`，也不
阻塞 N7A/N7B 的数据守恒、迁移和定向回归。

## N7A 冻结证据

2026-08-25 的 Windows 验收结果：VS2017 工程使用 v141 工具集完成 Debug/Release 编译；
完整门禁另以 VS2022 生成图执行双配置全量重建。世界运行时为 560/560，世界目录 48/48，
资源包 28/28，资源清单 49 项，性能比较 36 个夹具，启动错误 8 类；十三个测试目标、两套
隐藏客户端、135,993 字节受控 dump、可执行文件清单和 69 文件干净包均通过。发行 ZIP 的
本次本地 SHA-256 为 `3ABBC7EB6E57FB881D7998A0135A30AB9F6963EDF5075C40E1B7F68A74A51DD0`，
只证明当前 N7A 工作区，不替代提交身份或最终 Beta 封板。

这份证据关闭 N7A 的自动化边界，不声称 N7B 玩法闭环或 R3 真人输入已经完成。

## N7B 冻结证据

2026-08-25 的实现把普通路标交互接入既有结局模型：激活原子消耗 2 个铁锭；第一波生成
2 个 `hellomine:waystone_stalker`，第二波生成 1 个 `hellomine:waystone_brute`，驻留守卫上限
为 2；胜利 epoch 固定为 1，一次性奖励为 3 个铁锭。两个守卫变体只来自遭遇，不进入自然
刷怪；5 个追加终局目标只消费激活、带类型死亡和领取事件，不拥有或反推胜利状态。

`HelloMine3DWorldRuntimeSmoke` 增加 20 项 N7B 断言，覆盖严格路标 payload、库存 revision、
激活守恒、Activated/Encounter/Victorious/RewardClaimed 保存重开、死亡放弃、重复死亡去重、
区块卸载冻结和回载、满背包待领取、一次性奖励、胜利后继续方块编辑，以及遭遇中备份恢复
后的波次/actor 对账。Debug/Release 世界栈均为 580/580；VS2017 v141 的 Debug/Release
客户端和 Release 定向目标编译通过。固定 seed 的 nominal/stress 短时 soak 各 5 秒、零失败。

完整 Windows 门禁还通过 49 项资源、36 个性能比较夹具、28/28 资源包、8 类启动错误、
十三个测试目标、两套隐藏客户端、131,817 字节受控 dump、可执行文件清单和 69 文件干净包。
发行 ZIP SHA-256 为
`9F52C3C9F6F9DE60F85D7864ED707A60FD358B15E062CF9E85807ABA8494BDD2`。这些自动证据关闭
N7B；R3 仍只记录部分非正式真人自测，其余按用户决定延后，不能写成真人验收 `PASS`。
