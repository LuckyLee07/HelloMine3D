# G5/N6/N12B 音频反馈合同 v1

> Architecture Lab 迁移说明（2026-08-31）：工程/资源/生命周期证据保持冻结；可访问的 cue、
> 字幕、暂停/静音/退出功能进入 `AI-07`。人类听感与音乐偏好为 `NOT_CLAIMED`。

本文固定 HelloMine3D 第一版基础音频的数据、事件、播放、降级和验证边界。目标是给正常
玩家动作提供一致反馈，同时保证音频永远不能阻止世界加载、保存或退出。本合同不引入
FMOD、流式音乐、音频资源热更新或可执行资源包扩展。

## N12B 数据格式与资产所有权

基础定义位于 `media/audio/Base.audio`，首行必须是：

```text
# HelloMine3D audio definitions v3
```

每条非注释行使用固定字段；逻辑路径、字幕 key 和英文降级字幕都带引号：

```text
sample <id> <category> <mode> "<logical_path>" <gain> <max_voices> "<caption_key>" "<fallback_caption>"
```

- `category` 只允许 `ui`、`effects`、`ambient`。
- `mode` 只允许 `2d`、`3d`。
- `logical_path` 必须是 `media/audio/samples/*.wav` 下不含反斜杠、盘符、空段、`.` 或 `..`
  的规范路径；运行时不接受绝对路径和目录逃逸。
- `gain` 必须在 0-1，单 cue 并发数必须在 1-8；未知字段和尾随数据拒绝整份定义。
- `caption_key` 必须精确等于 `audio.<cue-id>.caption`。英文降级字幕必须包含 1-96 个可打印
  字符；本地化目录正常时仍由该语义 key 决定显示文本。
- id 必须唯一；未知字段、重复 id、非法数值或缺少必需提示都会拒绝整份定义。
- v3 必须同时定义 `ui.click`、`block.break`、`block.place`、`item.pickup`、
  `craft.success`、`combat.hit`、`combat.windup`、`combat.guard` 和 `ambient.wind`。

九个固定 WAV 由 `tools/generate_n12b_audio_samples.ps1` 离线确定性生成和筛选，运行时不再生成
正弦、方波或噪声。资产统一为 44,100 Hz、单声道、PCM16、小端 RIFF/WAVE，单个文件必须为
44-524,288 字节且持续 10-3,000 ms。文件、生成脚本、逐文件 SHA-256 和来源记录进入仓库；
`media/audio/samples/LICENSE-HelloMine3D-Audio.txt` 以 MIT License 授权这些项目原创采样。

清单分别使用 `audio`、`audio-sample` 和 `license` 类别。三类在 resource-pack v1 中均为
base-only，不能被现有包覆盖；未来允许替换音效时必须显式升版资源包合同，而不是复用纹理或
字体覆盖权限。缺采样属于可降级启动资源；许可证缺失仍是发行错误。

## 事件所有权

业务层只发布“动作已经成功”这一事实，不接触播放设备。`AudioRuntime` 订阅活动世界的
`SandboxEventBus`，并把事件映射为提示：

| 业务事实 | 提示 | 位置 |
| -------- | ---- | ---- |
| 方块成功破坏 | `block.break` | 3D，目标方块位置 |
| 方块成功放置 | `block.place` | 3D，放置位置 |
| 物品实体成功进入库存 | `item.pickup` | 3D，拾取位置 |
| 实体实际受到伤害 | `combat.hit` | 3D，受击位置 |
| 敌人进入攻击前摇 | `combat.windup` | 3D，敌人位置 |
| 玩家成功格挡 | `combat.guard` | 3D，格挡位置 |
| 制作事务成功提交 | `craft.success` | 2D |
| 成功的菜单/UI 操作 | `ui.click` | 2D |
| 世界持续运行满八秒 | `ambient.wind` | 2D |

一次业务动作最多产生一次声音。失败的放置、破坏、拾取、攻击或制作不发布成功事件，UI
也不能用额外点击声重复制作成功声。世界切换时先解绑旧事件总线，再绑定新世界；保存、
关闭或切换失败时恢复原绑定，避免悬空订阅和重复播放。

## 后端与播放规则

- 启动时一次性严格解码并冻结采样库；相同逻辑路径只缓存一份。唯一采样最多 32 个，解码后
  PCM 总量最多 4 MiB；当前 9 个 cue/9 个唯一采样共 312,230 字节。
- Windows 默认使用系统 `waveOut` 输出立体声 PCM；播放时只从冻结的单声道采样复制并应用
  gain、距离衰减和左右声像，不在提交路径打开文件或合成波形。初始化失败时自动退回 dummy。
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

启动先冻结有效资源视图，再读取音频定义、解析全部被引用的采样并创建运行时。缺少或无法
解析 `media/audio/Base.audio` 时冻结空定义；任何必需采样缺失、截断、格式不符或超界时冻结
空采样库。两类情况都选择 dummy、保留有界诊断且不属于启动资源预检的致命错误；定义仍有效
时字幕可继续显示，听觉提交计入 `missingSamples` 后静默返回。真实后端打开失败、单次播放失败
或 UI 回调异常也只能增加诊断/统计，不能中断玩家命令、世界保存或进程退出。

正常日志至少说明定义数量和所选后端：

```text
[AUDIO_REGISTRY] frozen=1 definitions=9 samples=9 unique_samples=9 decoded_bytes=312230 degraded=0
[AUDIO] backend=windows-waveout real=1 definitions=9 samples=9 unique_samples=9 decoded_bytes=312230 degraded=0
```

## 验证合同

关闭 N12B 更新至少需要：

1. 严格 v3 解析、必需提示、规范路径、字幕 key、重复/范围错误和缺文件降级测试。
2. 九个正式 WAV 的 RIFF 大小、PCM16/单声道/44.1 kHz、时长、单文件/总缓存上限、共享路径
   去重、损坏格式和缺文件降级测试。
3. 方块破坏/放置、拾取、受击和制作五类业务事件各精确提交一次，失败制作零提交。
4. 2D/3D 分类、分类/主音量、暂停、静音、挂起、缺少提示、解绑、环境节拍和并发上限
   测试。
5. 资源包 v1 拒绝定义、采样和许可证覆盖；缺少基础音频/采样时有效资源视图和启动预检仍可
   完成，许可证缺失则失败。
6. Windows Debug/Release 编译、`WorldRuntimeSmoke`、`ResourcePackSmoke`、资源清单正负例，
   以及强制 dummy 的隐藏校验启动和隐藏三帧真实启动全部通过。

2026-08-17 的 G5 历史关闭证据为：资源清单 40 项，清单门禁 3/3，资源包门禁 23/23，世界
运行时门禁 403/403；Debug 与 Release 的验证模式和三帧运行均在隐藏窗口中以退出码 0 完成。

N12B 的 2026-08-26 正式证据为：9 个原创 WAV 的固定哈希/格式检查、严格 v3 定义、312,230
字节解码缓存、共享路径去重、损坏/缺失静音降级和生命周期自动断言通过；资源清单为 61 项，
双语目录各 346 个 key。VS2017/v141 Debug/Release 客户端、681/681 世界和 34/34 资源包目标
通过；隐藏校验和真实三帧客户端均退出 0，真实运行报告 `windows-waveout real=1`、9/9 采样和
`degraded=0`。完整门禁通过十三个测试目标、38 个性能夹具、128,209 字节受控 dump 和 81 文件
包；发行 ZIP SHA-256 为 `1F7AEBFF35A796053A739D99CE060C14A7E39459A818DC1A6B609C5A778C416F`。
同身份可用菜单 `711.836/712.408 ms`、首次可控世界 `398.068/416.950 ms` 均比较 `PASS`。
历史人工听感没有形成 PASS。当前 cue/字幕/暂停/静音/退出等可观察功能映射到 `AI-07`，
真实无设备降级继续由自动边界和未来明确批准的目标机器验证；人类听感为 `NOT_CLAIMED`。
