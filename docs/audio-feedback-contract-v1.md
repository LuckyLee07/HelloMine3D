# G5/N6 基础音频反馈合同 v1

本文固定 HelloMine3D 第一版基础音频的数据、事件、播放、降级和验证边界。目标是给正常
玩家动作提供一致反馈，同时保证音频永远不能阻止世界加载、保存或退出。本合同不引入
FMOD、流式音乐、音频资源热更新或可执行资源包扩展。

## 数据格式

基础定义位于 `media/audio/Base.audio`，首行必须是：

```text
# HelloMine3D audio definitions v2
```

每条非注释行使用固定字段和一个带引号字幕：

```text
sound <id> <category> <mode> <waveform> <frequency_hz> <duration_ms> <gain> <max_voices> "<caption>"
```

- `category` 只允许 `ui`、`effects`、`ambient`。
- `mode` 只允许 `2d`、`3d`。
- `waveform` 只允许 `sine`、`square`、`noise`。
- 频率、时长、增益和单提示并发数必须落在解析器规定的有界范围内。
- 字幕必须包含 1-96 个可打印字符；未知、缺失或越界字幕拒绝整份定义。
- id 必须唯一；未知字段、重复 id、非法数值或缺少必需提示都会拒绝整份定义。
- v2 必须同时定义 `ui.click`、`block.break`、`block.place`、`item.pickup`、
  `craft.success`、`combat.hit` 和 `ambient.wind`。

当前声音由轻量后端按定义实时合成，不依赖外部音频 SDK 或压缩音频文件。定义文件作为
`audio` 条目进入基础资源清单；resource-pack v1 不允许覆盖该类别，未来如需替换声音，
必须先建立独立的版本化资源包合同。

## 事件所有权

业务层只发布“动作已经成功”这一事实，不接触播放设备。`AudioRuntime` 订阅活动世界的
`SandboxEventBus`，并把事件映射为提示：

| 业务事实 | 提示 | 位置 |
| -------- | ---- | ---- |
| 方块成功破坏 | `block.break` | 3D，目标方块位置 |
| 方块成功放置 | `block.place` | 3D，放置位置 |
| 物品实体成功进入库存 | `item.pickup` | 3D，拾取位置 |
| 实体实际受到伤害 | `combat.hit` | 3D，受击位置 |
| 制作事务成功提交 | `craft.success` | 2D |
| 成功的菜单/UI 操作 | `ui.click` | 2D |
| 世界持续运行满八秒 | `ambient.wind` | 2D |

一次业务动作最多产生一次声音。失败的放置、破坏、拾取、攻击或制作不发布成功事件，UI
也不能用额外点击声重复制作成功声。世界切换时先解绑旧事件总线，再绑定新世界；保存、
关闭或切换失败时恢复原绑定，避免悬空订阅和重复播放。

## 后端与播放规则

- Windows 默认使用系统 `waveOut` 输出立体声 PCM；初始化失败时自动退回 dummy。
- 非 Windows、定义缺失、无音频设备或显式设置
  `HELLOMINE3D_AUDIO_BACKEND=dummy` 时使用 dummy。
- dummy 保留事件、并发和统计语义，但不打开设备、不发声，供无设备环境和自动化使用。
- 全局同时播放上限为 16；每个提示还受自身 `max_voices` 限制。
- 3D 提示根据 listener 的位置与朝向计算距离衰减和左右声道平移；2D 提示不受世界位置
  影响。
- 主音量先与 `ui`、`effects`、`ambient` 分类音量相乘；设置应用后立即更新。
- 暂停世界时停止新的效果和环境提示与环境计时；菜单 UI 提示仍可提交。
- 静音、零音量或挂起时听觉播放被抑制；恢复后继续接受新事件，不补播已经跳过的事件。
  N6 声音字幕在静音和零音量下仍提交，关闭字幕选项后不提交；暂停仍会先抑制非 UI 事件，
  因而不会生成与未发生世界声音对应的字幕。
- 环境提示只在世界模拟正常推进时累计时间，暂停期间不追赶播放。

## 启动与失败语义

启动先冻结有效资源视图，再读取音频定义并创建运行时。缺少或无法解析
`media/audio/Base.audio` 时记录诊断，冻结空定义并选择 dummy；该情况不是启动资源预检
的致命错误。真实后端打开失败、单次播放失败或 UI 回调异常也只能增加诊断/统计，不能
中断玩家命令、世界保存或进程退出。

正常日志至少说明定义数量和所选后端：

```text
[AUDIO_REGISTRY] frozen=1 definitions=7 degraded=0
[AUDIO] backend=dummy real=0 definitions=7 degraded=1 reason=dummy backend requested
```

## 验证合同

关闭 G5 至少需要：

1. 严格解析、必需提示、重复/范围错误和缺文件降级测试。
2. 方块破坏/放置、拾取、受击和制作五类业务事件各精确提交一次，失败制作零提交。
3. 2D/3D 分类、分类/主音量、暂停、静音、挂起、缺少提示、解绑、环境节拍和并发上限
   测试。
4. 资源包 v1 拒绝音频覆盖；缺少基础音频时有效资源视图和启动预检仍可完成。
5. Windows Debug/Release 编译、`WorldRuntimeSmoke`、`ResourcePackSmoke`、资源清单正负例，
   以及强制 dummy 的隐藏校验启动和隐藏三帧真实启动全部通过。

2026-08-17 的关闭证据为：资源清单 40 项，清单门禁 3/3，资源包门禁 23/23，世界运行时
门禁 403/403；Debug 与 Release 的验证模式和三帧运行均在隐藏窗口中以退出码 0 完成。
人工听感、物理输入和最终发行包仍按 R3/N6/Release Candidate 计划后置，不影响本合同的
自动化完成状态。
