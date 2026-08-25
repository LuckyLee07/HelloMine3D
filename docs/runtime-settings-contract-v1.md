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
| `UserSettings` | 窗口宽高、全屏、视距、FOV、鼠标灵敏度、反转 Y、主/UI/效果/环境/音乐音量、UI 缩放、语言、声音字幕、操作提示和九项玩法键位 | 可编辑 |
| `WorldCreationConfig` | 可选世界 seed | 不可见、不可编辑 |

恢复默认值只重新创建 `UserSettings`。应用时以当前完整 `Config` 为基底替换用户设置，
所以已有的 seed 原样保留；已创建世界的真实身份仍以世界元数据为准。

## 设置会话

1. 打开设置页时，`RuntimeSettingsSession` 同时保存已应用快照和可编辑草稿。
2. `Cancel` 丢弃草稿并恢复快照；`Defaults` 只覆盖草稿，不立即保存。
3. `Apply` 先做完整范围校验，再把草稿作为命令交给应用层。
4. 应用层先原子发布配置；失败时内存配置、相机和世界都保持旧值，设置页保留草稿和错误。
5. 成功后才更新内存配置。视觉/逻辑相机 FOV、灵敏度、反转 Y、视距、音量、UI 缩放、辅助选项和键位立即生效；窗口尺寸和全屏
   保存成功但明确提示重启生效。

第一版不在运行中重建 Ogre 窗口。视距通过线程安全原子值更新，并递增加载计划修订号，
使后台加载器重算工作序列，同时主线程开始有界卸载远区块。

## 文件格式

`bin/config.txt` 使用严格文本格式，N12C 后当前版本为 4：

```text
settings_version 4
renderdistance 8
fullscreen 0
windowsize 1280 720
fov 90
mousesensitivity 0.05
invertmousey 0
mastervolume 1
uivolume 1
effectsvolume 1
ambientvolume 1
musicvolume 0.65
uiscale 1
locale en-US
audiocaptions 1
actionhints 1
key_move_forward w
key_move_backward s
key_move_left a
key_move_right d
key_jump space
key_sneak left_shift
key_sprint left_control
key_open_crafting e
key_consume_food r
seed random
```

- 空行和 `#` 后注释可忽略；重复字段、未知字段、尾随数据和非有限浮点数拒绝。
- 无 `settings_version` 的旧文件按 legacy v0 读取；显式版本 1/2/3 仍按各自字段边界读取。
  缺失字段使用默认值，成功校验后立即原子迁移到 v4；v0-v2 的 locale 固定迁移为 `en-US`，
  v0-v3 的音乐音量固定迁移为默认 0.65。版本 1 混入版本 2 字段、v0-v2 混入 v3 `locale`，
  或 v0-v3 混入 v4 `musicvolume` 都拒绝。
- 未知版本明确拒绝，不尝试猜测或降级。
- 视距范围为 1-32，窗口为 640×480 至 7680×4320，FOV 为 45-120，灵敏度为
  0.005-1.0，各音量为 0.0-1.0，UI 缩放为 0.75-1.75；locale 只接受 `en-US`/`zh-CN`；
  九项玩法键位不得重复。

## 原子性与失败

序列化结果通过 `StorageTransaction` 写入同目录 `.pending`，执行 durable flush、用严格
解析器复读候选，再原子替换目标。任何写入、flush、校验或替换失败都不覆盖上一份配置；
失败候选进入单个有界 `.failed` 文件并向 UI 返回明确错误。

## 自动验证

`HelloMine3DWorldRuntimeSmoke` 的 G4 用例覆盖：

- 缺失文件生成 v4、五类音量、语言、辅助选项和九项键位默认值；
- legacy v0、版本 1/2/3 自动迁移，旧版本越界字段和未知版本拒绝；
- 辅助选项与自定义键位往返，重复/未知键位和 UI 缩放越界拒绝；
- 草稿应用计划、取消、默认值、显示重启分类和非法范围；
- 成功保存保留 seed，替换前故障保持上一份有效文件；
- 逻辑相机 FOV 和世界视距即时更新；
- 主菜单/暂停拒绝模拟推进，恢复后继续推进，非法重复暂停拒绝。

Debug/Release 世界目标和隐藏客户端都必须通过。真实鼠标点击、Escape 手感、焦点切换与
窗口重启后的显示模式仍由 R3 的目标 Windows Release 记录关闭。
