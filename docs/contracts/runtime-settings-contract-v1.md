# G4 暂停与运行时设置合同 v1

本文固定 G4 的暂停语义、用户设置所有权、文件格式和失败边界。它约束
`Config`、`RuntimeConfig`、`GameApplicationFlow`、`SandboxRuntime` 与 Ogre/ImGui
设置页，避免 UI 直接改变世界身份或在保存失败后伪装成已应用。

## 状态与模拟

- `GameApplicationFlow::acceptsWorldSimulation()` 只在 `Playing` 返回 true。
- Ogre 每帧仍处理窗口事件、ImGui、菜单和加载完成，但非 `Playing` 状态不调用
  `SandboxRuntime::update`，因此固定 tick、AI、作物、掉落物、战斗与世界时间全部冻结。
- 进入暂停会取消当前采集进度并清空瞬时输入；恢复后不会补跑暂停期间的 delta。
- 暂停和设置页均捕获键盘与鼠标。设置页打开时 Escape 先取消草稿；原子保存仍在进行时
  Escape 只被消费，不恢复世界输入。

## 所有权边界

`Config` 由两个显式部分组成：

| 所有者 | 字段 | 设置页权限 |
| ------ | ---- | ---------- |
| `UserSettings` | 窗口宽高、全屏、视距、方向阴影档位、后处理开关、FOV、请求的第一/第三人称视角、鼠标灵敏度、反转 Y、主/UI/效果/环境/音乐音量、UI 缩放、语言、声音字幕、操作提示、九项键盘玩法键位、四项鼠标世界动作绑定、冲刺/潜行模式及动作反馈强度 | 可编辑 |
| `WorldCreationConfig` | 可选世界 seed | 不可见、不可编辑 |

恢复默认值只重新创建 `UserSettings`。应用时以当前完整 `Config` 为基底替换用户设置，
所以已有的 seed 原样保留；已创建世界的真实身份仍以世界元数据为准。

## 设置会话

1. 打开设置页时，`RuntimeSettingsSession` 同时保存已应用快照和可编辑草稿。
2. `Cancel` 丢弃草稿并恢复快照；`Defaults` 只覆盖草稿，不立即保存。
3. `Apply` 先做完整范围校验，再把草稿作为命令交给应用层。
4. 应用层先原子发布配置；失败时内存配置、相机和世界都保持旧值，设置页保留草稿和错误。
5. 成功后才更新内存配置。视觉/逻辑相机 FOV、请求视角、灵敏度、反转 Y、视距、方向阴影档位、后处理、音量、UI 缩放、辅助选项、键位和动作反馈强度立即生效；窗口尺寸和全屏
   保存成功但明确提示重启生效。

视角设置只保存玩家请求的 `first|third`。近墙裁切回退、相机平滑历史和当前有效视角都是
运行期表现状态，不进入配置文件；回退解除后仍恢复玩家原先请求的第三人称。

第一版不在运行中重建 Ogre 窗口。视距通过线程安全原子值更新，并递增加载计划修订号，
使后台加载器重算工作序列，同时主线程开始有界卸载远区块。

V10p 的 `prepareApply` 重载向 UI 返回 `RuntimeSettingsValidationIssue`：失败类型、字段，
以及无效或冲突绑定的动作、按键／鼠标键与冲突双方。旧字符串接口及
`validateUserSettings` 的英文 `std::runtime_error::what()` 保持兼容，验证先后顺序不变。
校验失败或会话未打开时，输出应用计划保持不变；失败草稿保留，Cancel 仍恢复原快照。

设置页按当前已应用语言将结构化问题映射为修改建议，不解析异常字符串；保存失败显示
本地化主提示，底层诊断只作为悬停详情。设置内容与反馈各自滚动，新反馈只复位反馈区，
Defaults／Cancel／Apply 位于固定操作区。反馈不改变输入所有权、原子发布或 settings v11。

## 文件格式

`bin/config.txt` 使用严格文本格式；画面细节为 v9、小地图范围为 v10，B8 玩家视角后当前版本为 v11：

```text
settings_version 11
renderdistance 8
visualdetail standard
directionalshadowquality off
postprocessingquality off
fullscreen 0
windowsize 1280 720
fov 90
cameraperspective first
mousesensitivity 0.05
invertmousey 0
mastervolume 1
uivolume 1
effectsvolume 1
ambientvolume 1
musicvolume 0.65
minimaprange 128
uiscale 1
locale en-US
audiocaptions 1
actionhints 1
sprintmode hold
sneakmode hold
feedbackintensity full
key_move_forward w
key_move_backward s
key_move_left a
key_move_right d
key_jump space
key_sneak left_shift
key_sprint left_control
key_open_crafting e
key_consume_food r
mouse_break_attack primary
mouse_use secondary
mouse_place secondary
mouse_guard secondary
seed random
```

- 空行和 `#` 后注释可忽略；重复字段、未知字段、尾随数据和非有限浮点数拒绝。
- 无 `settings_version` 的旧文件按 legacy v0 读取；显式版本 1–10 仍按各自字段边界读取。
  缺失字段使用默认值，成功校验后立即原子迁移到 v11；v0-v2 的 locale 固定迁移为 `en-US`，
  v0-v3 的音乐音量固定迁移为默认 0.65。版本 1 混入版本 2 字段、v0-v2 混入 v3 `locale`，
  v0-v3 混入 v4 `musicvolume`、v0-v4 混入 v5 `directionalshadowquality`，或 v0-v5 混入 v6
  `postprocessingquality`，v0-v6 混入 v7 输入字段、v0-v7 混入 v8 `feedbackintensity`、
  v0-v8 混入 v9 `visualdetail`、v0-v9 混入 v10 `minimaprange`，或 v0-v10 混入 v11
  `cameraperspective` 都拒绝。
- v0-v4 的方向阴影固定迁移为 `off`；v5-v11 只接受 `off`、`medium`、`high`，未知档位严格拒绝。
- v0-v5 的后处理固定迁移为 `off`；v6-v11 只接受 `off`、`on`，未知档位严格拒绝。
- v0-v6 的冲刺/潜行模式固定迁移为 `hold`，鼠标动作固定迁移为主键采挖/攻击、副键共享
  使用/放置/格挡。v7 六个新增字段全部必需；模式只接受 `hold|toggle`，鼠标只接受
  `primary|secondary|middle|side1|side2`。
- v0-v7 的动作反馈强度固定迁移为 `full`；v8 字段必需且只接受
  `off|reduced|full`。三档的表现和关键状态回退见 P11B 合同。
- v0-v8 的画面细节固定迁移为 `standard`，v9 起字段必需且只接受
  `standard|compatibility`；v0-v9 的小地图范围固定迁移为 128 米，v10 起字段必需且只接受
  64、128 或 256 米。
- v0-v10 的请求视角固定迁移为第一人称；v11 的 `cameraperspective` 字段必需且只接受
  `first|third`，重复、缺失或未知值严格拒绝。
- 未知版本明确拒绝，不尝试猜测或降级。
- 视距范围为 1-32，窗口为 640×480 至 7680×4320，FOV 为 45-120，灵敏度为
  0.005-1.0，各音量为 0.0-1.0，UI 缩放为 0.75-1.75；locale 只接受 `en-US`/`zh-CN`；
  九项键盘玩法键位不得重复；采挖/攻击鼠标键不得与上下文动作重复，使用/放置/格挡允许按
  P11A 合同共享并在设置页提示。

## 原子性与失败

序列化结果通过 `StorageTransaction` 写入同目录 `.pending`，执行 durable flush、用严格
解析器复读候选，再原子替换目标。任何写入、flush、校验或替换失败都不覆盖上一份配置；
失败候选进入单个有界 `.failed` 文件并向 UI 返回明确错误。

## 自动验证

`HelloMine3DWorldRuntimeSmoke` 的 G4 用例覆盖：

- 缺失文件生成 v11、默认第一人称、默认关闭方向阴影/后处理、五类音量、语言、辅助选项、九项键盘键位、
  四项鼠标动作、两项 hold 模式和 `full` 动作反馈默认值；
- legacy v0、版本 1–10 自动迁移，旧版本越界字段、未知阴影/后处理/输入/反馈/视角档位和未知版本拒绝；
- 请求视角、辅助选项、自定义键鼠、hold/toggle 和反馈强度往返，破坏性重复/未知绑定和 UI 缩放越界拒绝；
- 草稿应用计划、取消、默认值、显示重启分类和非法范围；
- 结构化失败字段与绑定双方、旧诊断兼容、校验顺序、失败计划保持、Cancel 恢复和有效重试；
- 28 个 `settings.error.*` 必需键在两语言中齐全；双方同时漏键也必须拒绝；
- 成功保存保留 seed，替换前故障保持上一份有效文件；
- 逻辑相机 FOV 和世界视距即时更新；
- 主菜单/暂停拒绝模拟推进，恢复后继续推进，非法重复暂停拒绝。

V10p 的 macOS x86_64 Debug／Release `CAMERA_SETTINGS` 各 95 项、`P11A` 各 159 项通过；
工程与实际 UI 的证据范围分别见[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)V10p，
这些聚焦检查不替代正常窗口的双语、滚动、点击与保存失败验收。

Debug/Release 世界目标和可运行的隐藏客户端都必须通过。真实鼠标点击、Escape、焦点切换、
相对视角增量与窗口重启恢复的可观察功能由 `AI-01` 至 `AI-04` 的目标 Windows Release 记录
关闭，当前为 `NOT_RUN`。真实设备距离与手感属于历史 Physical Input v2，保持
`NOT_RUN/SUPERSEDED`，人类物理体验为 `NOT_CLAIMED`。
