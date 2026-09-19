# 投射物模型与朝向修复 r27

状态：实现、双配置聚焦回归、GPU、画面对照、24 次性能与干净候选交付通过；正常新建/进入/保存退出自测通过。持续输入与完整战斗未计完成。
起始 commit `c28e9ca`。本批后立即实施地形多样化，优先级见[综合目标](../current/visual-upgrade-goal-prompt-2026-09-17.md)。

## 问题和改动

正常场景原图 `build/visual-upgrade-20260917/normal-r25/03-world-empty-hand.png` 显示近镜头
纯紫色拉长方盒占据明显视野。旧模型为沿 Z 拉长 1.8 倍的立方体，欧拉角组合还会将局部前向的
X 分量反转。修复接入生产 `OgreActorRenderer`，无需开启诊断：

- 改为 18 顶点、32 三角形的封闭分面滴形，按真实碰撞直径等比缩放；单个不透明 draw section。
- 用速度向量建立右手正交基，局部 -Z 为前端，包括竖直飞行。
- 使用独立部件编号 10，前端淡亮、尾端深紫；昼夜曝光、雾和关闭表面增强的平色回退保留。
- 校验拒绝非有限半径。World 碰撞、伤害、速度、20 Hz 步进、寿命、32 个上限和存档语义不变。

未增加光晕、脉冲、额外粒子或透明排序；本批没有新增投射物运动插值。

## 已验证范围

产物根目录 `build/visual-upgrade-20260917/`；图片和运行数据仅保留本地。
平台为 macOS / AppleClang / x86_64，GPU 为 Apple M1 Pro 的实际 CGL 上下文。

| 检查 | 结果 | 本地产物 |
| --- | --- | --- |
| 客户端、WorldRuntime、ResourcePack 双配置构建 | PASS | `projectile-r27/build-r2/results.json` |
| 网格与速度基 | Debug/Release 各 51 个断言通过（含逐三角形索引检查） | `projectile-r27/math-{debug,release}-r1/` |
| 投射物行为 | Debug/Release 各 15/15 | `projectile-r27/build-r2/HelloMine3DWorldRuntimeSmoke-*-run/` |
| 资源 | Debug/Release 各 114/114 | `projectile-r27/build-r2/HelloMine3DResourcePackSmoke-*-run/` |
| 生产普通/阴影 actor shader | 100/100，包括新增前后端、夜间、雾、回退、局部坐标和状态隔离 | `projectile-r27/gpu-r1.log` |
| 故障负例 | 旧朝向在速度对齐失败；旧 shader 在前后端区分失败，均符合预期 | `projectile-r27/negative-old-orientation/`、`gpu-old-negative.json` |
| 画面对照 | 限定范围 PASS，10 段诊断、98 张原始帧，实际审阅 12 张 | `projectile-r27/visual-verification.json` |

两版使用完全相同的有界诊断快照展示，控制版保持 r26 的旧模型/方向/材质。不是把诊断参数带入
普通游戏；诊断不写入 World 投射物或玩家存档。近景、补充中远距离、Medium/High/Off、夜间及
大气关闭回退已观察。每版飞行段采集 41 个不同实际时间点（25 fps）；已审阅的相邻帧显示外形
闭合、无新增闪烁效果，20 Hz 位置步进仍会使相邻采样保持同一位置，诊断的四米循环亦不属于
正常战斗。不能据此声明完整战斗或持续输入已通过。

候选 Release SHA-256：`c5acd40a2753675b3aa6f82fdde124b85e3808c42dec216864bdb3801bc0838a`。
329 份第一方源码清单 SHA-256：`b6875482260b8560c37a831cee057f9e482f05d7445167f514223f1f1fee8a5c`。
控制版 SHA-256：`5a4c9503e08c282e32d52c35306241016175288aeb00a058f40fd21bcd323d5c`。
隔离 checkout 的历史 git 起点不代表当前源代码；逐文件清单与冻结源码一致才作为构建身份。

首次控制版构建因诊断表达式中 GLM 向量先乘 int 编译失败；修正标量表达式后双配置通过。
失败保留在 `build-r1/` 和 `build-runner-r1.log`。未删除检查或放宽阈值。

## 性能、交付与正常路径

24 次实际运行及逐帧核验通过，原始 runner session 95511 退出 0；没有筛除慢样本。
Off/High × 常驻/快速流送 × 三轮（AB/BA/AB），每次预热 5 秒、采样 30 秒、2560×1440
隐藏窗口，无截图读回。以下是候选/控制的三轮中位数比值，门槛均为 1.10。

| 场景 | P95 比值 | P99 比值 |
| --- | --- | --- |
| off-steady | 0.925343 | 0.948151 |
| off-streaming | 1.032493 | 1.037372 |
| high-steady | 0.970099 | 0.982547 |
| high-streaming | 0.956372 | 0.906545 |

审计 `projectile-performance-r27-r1/audit-r1.json` 核验包/源码/依赖、无读回、实际像素尺寸、
每帧时间与固定 tick、设置一致、无采样中睡眠。工作包恢复候选与用户当前设置，防睡眠断言释放。
干净交付 `delivery-r27/HelloMine3D-Visual-Candidate.app`：111 个分发文件、329 份源码、
无窗口 validate-only 退出 0，普通中文 1280×720 配置，不带用户存档。前后原图见该目录
`COMPARISON.md`，交付包源码/资源/可执行文件身份与上述候选一致。

随后由实现者使用公开 CUA 从正常菜单建立 `新世界ProjectileR27-20260919`（seed 20260807），
进入自然场景，观察到自然 Spitter 及其小型分面紫色投射物；原生 `03-natural-combat.png`
也保留敌人上方的小投射物。通过正常暂停、保存返回菜单和退出结束；原生查询确认没有工作包
进程/窗口。设置字节未变，原六个世界及其备份共 20 份既有 world.meta 哈希未变。
检查时未启用诊断夹具，进程游戏环境只有 HELLOMINE3D_ROOT。详见 `normal-r27/verification.json`。

这属于实现者限定自测，不是独立黑盒、完整战斗或持续输入验收。生命在截图中出现 4/8/20，
中间死亡/重生未直接观察，不据此声明持久化或战斗平衡通过。首次原生截图因 52×20 辅助窗口
造成歧义被拒绝，后续明确选择真实游戏窗口；一次 CUA app-change 提示后重新读取状态，再正常
完成暂停/保存/退出。失败原件与实际名称中保留的“新世界”前缀均如实记录。

本批结束后立即进入地形多样化，实现优先级高于其他视觉打磨；综合 Goal 保持 active。
