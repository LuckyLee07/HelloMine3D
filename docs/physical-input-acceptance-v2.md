# Physical Input Acceptance Protocol v2

> Architecture Lab 状态（2026-08-31）：本协议作为历史真人/物理输入合同保留，十三项定义、
> 模板和校验器语义不变。它已被 `docs/ai-assisted-gameplay-acceptance-v1.md` 取代为当前退出
> 门槛；Computer Use 只能关闭新的 AI 功能合同，不能把本物理合同写成 PASS。物理设备手感为
> `NOT_CLAIMED`。

本协议是 P11A 建立的真实键鼠验收合同。它扩展 R3 v1，覆盖当前输入仲裁、设置、
焦点和 D4/D6 玩家旅程。自动测试和隐藏窗口预检只能证明逻辑边界，不能代替操作者
使用物理设备完成本协议。

## 当前状态

合同、模板和校验器已完成；没有产生正式物理运行记录，当前结果保持 `NOT_RUN`。在个人
Architecture Lab 定位下不再默认安排外部操作者，因此它不是待兑现的 `Deferred` 项，也不
阻塞后续架构批次。任何未来自愿进行的物理运行仍必须遵守本协议，未运行记录不得写成 `PASS`。

## 前置条件

1. 从精确提交构建 VS2017/v141 `Release`，从仓库 `bin` 目录启动
   `HelloMine3D.exe`。
2. 使用隔离的新世界，记录 GPU/驱动、窗口模式、窗口尺寸、键鼠设备和设置档案。
   正式运行不得启用输入、战斗、容器、作物、渲染或性能 fixture。
3. 复制 `docs\physical-input-record-v2.template.txt` 到 Git 外的证据目录，逐项填写
   `PASS`、`FAIL` 或 `BLOCKED`，并在 `deviations` 中说明所有非 PASS 结果。
4. 运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_physical_input_v2_record.ps1 -RecordPath <record.txt> -RequirePass
```

## 真实操作顺序

| 记录键 | 操作 | 通过条件 |
| ------ | ---- | -------- |
| `case.movement_modes` | 分别以 Hold/Toggle 测试前后左右、疾跑和潜行。 | 方向正确；按住/切换边沿准确；暂停或失焦后无粘键。 |
| `case.mouse_look` | 以两档灵敏度移动鼠标，再开启 Y 轴反转。 | 相同物理位移响应稳定；灵敏度单调；反转只改变俯仰。 |
| `case.break_attack` | 左键先对方块、再对无遮挡敌人操作。 | 同一输入只消费一个目标动作，不同时破坏和攻击。 |
| `case.use_place` | 右键依次对可使用方块、普通表面和无效位置操作。 | 可使用目标只 Use；普通表面只 Place；失败不消耗物品。 |
| `case.guard` | 装备可格挡物品，面对敌人按绑定键格挡，再换不可格挡物品。 | 可格挡时只 Guard；不可格挡时按上下文回退且不重复消费。 |
| `case.container_crafting` | 打开容器与制作界面，点击转移/制作并尝试移动、视角和世界动作。 | UI 获得输入所有权；数量守恒；关闭后一次恢复世界输入。 |
| `case.pause_resume` | Escape 暂停/恢复，并在暂停页修改输入设置。 | 暂停时世界动作停止；恢复无跳变、无额外动作。 |
| `case.alt_tab_recovery` | 无按键与按住移动键两种情况下 Alt+Tab 离开并返回。 | 后台无动作；回焦需新边沿；视角不跳。 |
| `case.minimize_recovery` | 最小化窗口，移动鼠标和点击，再恢复。 | 最小化期间无世界动作；恢复首个鼠标样本被丢弃且无误触。 |
| `case.settings_restart` | 修改四项鼠标绑定、疾跑/潜行模式、灵敏度与反转，退出并重启。 | 设置完整恢复；非法冲突无法保存；动作提示与实际绑定一致。 |
| `case.damage_death_respawn` | 正常遭遇敌人，攻击、受伤、死亡并重生。 | 攻防输入可控；死亡只发生一次；重生无卡键且状态符合合同。 |
| `case.crop_save_reload` | 获取种子、种植、使用容器、拾取战利品，显式保存退出并重启检查。 | 作物、容器、物品、玩家与世界状态一致恢复，无调试注入。 |
| `case.window_close` | 分别通过游戏退出流程和窗口关闭按钮结束。 | 两条路径均干净退出，无挂起、崩溃对话框或损坏存档。 |

## 记录规则

- `protocol_version` 必须为 `2`，`configuration` 必须为 `Release`。
- `commit` 是实际测试的 7-40 位 Git 提交 ID；`settings_profile` 必须能定位所用设置。
- `overall_result=PASS` 仅在十三项全部 `PASS` 且 `deviations=none` 时有效。
- `FAIL` 或 `BLOCKED` 是有效证据，但不能关闭 D2/D4/D6/R3。
- 跟踪模板使用 `NOT_RUN`，只允许校验器的 `-AllowNotRun` 模式读取。
- 修改 OIS、窗口焦点、按键/鼠标映射、世界动作、容器/制作 UI、战斗、作物或保存路径后，
  必须重新运行完整协议。
