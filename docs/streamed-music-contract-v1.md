# N12C 单通道流式音乐合同 v1

本文固定 N12C 的资源格式、单通道播放状态、后台流式边界、失败降级和设置迁移。
它不引入 FMOD、多音乐通道、实时 DSP、动态下载或按生物群系编排。

## 资源与身份

- `media/music/Base.music` 使用严格头 `# HelloMine3D music definitions v1`，且只允许
  `overworld.quiet` 一个稳定 track id。
- 每条记录依次声明逻辑 WAV 路径、gain、时长、淡入、淡出、首次延迟、最小和最大重播间隔。
  路径必须是规范的 `media/music/tracks/*.wav`，禁止绝对路径、反斜杠、空段和 `..`。
- 当前 `quiet-horizons.wav` 是项目原创的 20 秒低密度曲目，44.1 kHz、mono、PCM16；许可为
  MIT，许可证随资产发布并在双语 Credits 中展示。
- 定义、WAV 和许可证均进入生成资源清单。resource-pack v1 将 `music`、`music-track` 和
  `license` 保持 base-only；缺定义或曲目允许静音降级，缺许可证仍由发行门禁拒绝。

定义文件最多 64 KiB。流文件必须是规范 44 字节 RIFF/WAVE 头，长度 10-180 秒且不超过
32 MiB；声明时长必须与文件帧数完全一致。当前资产由
`tools/generate_n12c_music_track.ps1` 确定性生成并用固定 SHA-256 校验：曲目为
`2B17B204D9580DB67CAE410946C4AD0D494A3C9132F2F41178D152E79F45987F`，许可证为
`2090735E2B82765C0636B9CE22DF79C34B2DFF3007D1F0E0844C0A6883DC90F1`。

## 播放状态与节奏

运行时只有 `stopped`、`waiting`、`fading-in`、`playing`、`fading-out`、`paused` 和
`degraded` 七种可观测状态：

1. 进入世界后等待 6 秒，再以 2.5 秒淡入曲目；同一世界活动期间不会并发启动第二条音乐。
2. 曲目自然结束后进入确定性的 45-90 秒间隔，再允许下一次播放。
3. 暂停以 1.8 秒淡出后暂停设备；恢复从零增益重新淡入，不重建第二个播放实例。
4. 回主菜单、切离世界、静音、主音量或音乐音量归零时淡出并停止；退出使用幂等立即停止并
   等待后台线程结束。
5. 设备挂起期间流位置不推进；设备恢复后保留暂停事实，只有世界恢复时才继续播放。

最终增益为 `masterVolume * musicVolume * track.gain`。设置页新增独立音乐音量；
`bin/config.txt` 升为 settings v4，v0-v3 缺少该字段时迁移为默认值 0.65。v1-v3 混入
`musicvolume`、未知/重复字段、非有限值或 0-1 以外数值均拒绝。

## Windows 流式后端

- Windows 默认后端为 `windows-waveout-stream`，独立于短音效声部，只占一个 WaveOut 输出。
- 一个后台线程从磁盘顺序读取单声道 PCM，以当前原子 gain 转成立体声后提交；同时最多保留
  3 个 4096-frame 缓冲，预读约 24 KiB，不把整首曲目解码进内存。
- `stop` 先请求停止并 reset 设备，再 join 线程；自然结束、读取失败、暂停、静音和退出都不得
  让线程越过 `MusicRuntime` 生命周期。
- 无设备、初始化/读取失败、非 Windows、缺资源，或显式选择 dummy 时进入有界静音路径；
  世界加载、设置保存和进程退出不因音乐失败而失败。
- `HELLOMINE3D_MUSIC_BACKEND=dummy` 用于无设备自动化；
  `HELLOMINE3D_MUSIC_IMMEDIATE=1` 仅供生命周期测试跳过首次等待，不改变资源身份。

## 自动验收

N12C 至少覆盖：

- 定义身份、路径穿越、重复 id、gain/时长/fade/gap 范围和 WAV 头/长度/时长一致性；
- 首次延迟、淡入、单实例、低密度间隔、暂停/挂起/恢复、静音、音量归零和立即退出；
- 缺定义、缺曲目、损坏 WAV、声明时长不符和 dummy 降级；
- 流进度小于完整曲目，证明启动未把整首曲目载入缓存；
- settings v4 及 v0-v3 原子迁移、双语 key/Credits/许可、64 项清单和资源包 base-only；
- VS2017/v141 Debug/Release 世界、资源包与客户端，以及隐藏真实 WaveOut 三帧启动。

真人听感、实际暂停/切世界的听觉过渡和真实无设备机器体验在本批完成时随人工项延期；当前
改由 `docs/manual-product-experience-acceptance-v1.md` 独立记录。
自动格式与生命周期测试不能把这些项目标记为 `PASS`，但不阻塞 N12C 自动开发门禁或后续
BETA-RC 工程封板。

## 2026-08-26 完成证据

- VS2017/v141 Debug/Release 世界为 699/699，资源包为 38/38；隐藏 validation-only 与真实
  三帧客户端均退出 0，真实后端为 `windows-waveout-stream real=1` 且无降级。
- 完整 Windows 门禁通过 64 项 manifest、38 个性能夹具、十三个测试目标、双配置隐藏客户端、
  启动负例、131,321 字节受控 dump 和 84 文件干净发行包。发行 ZIP SHA-256 为
  `DABE356BC511A110838DC1CD98BEE9392D0FC03FFBA1D40DB8271B6BD5392E2A`。
- 两轮同身份、真实音乐立即启动的隐藏 Q1 采集均通过；可用菜单为 `716.658/740.937 ms`，
  首次可控世界为 `376.473/425.927 ms`，帧 P95 为 `6.864/6.388 ms`，均无超过 50 ms 的帧。
