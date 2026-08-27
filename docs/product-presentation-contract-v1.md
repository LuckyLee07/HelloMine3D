# N12 产品表现合同 v1

本文固定 Beta 首轮本地化、字体、字幕、Credits、采样音效和低密度音乐的所有权与降级边界。
N12A 冻结文本和字体部分；N12B/N12C 只在各自批次补充已验证的音频细节，不回写玩法、
世界存档身份或资源包 v1 的既有覆盖权限。

## 本地化资源

- `media/text/en-US.text` 是语义 key 的权威目录，`zh-CN.text` 必须具有完全相同且排序稳定的
  key 集合；启动冻结时集合不一致即拒绝发布，不能把不完整翻译带入运行时。
- 文件头保持 `# HelloMine3D localized text v1`。locale 使用 `ll-RR`，key 使用小写 ASCII
  点分段语义名，值必须是合法 UTF-8、非空且不超过 1024 字节。
- 覆盖主菜单、世界列表、暂停/设置、HUD、目标、配方/容器、死亡/胜利、字幕、Credits 和
  F1 诊断入口。存档中的目标 id、材料 id、声音 id 和世界阶段仍是稳定身份，不保存翻译文本。
- 正常发布要求完整 key 对齐。防御性 lookup 仍保留：未知 locale 回退 `en-US`，缺翻译回退
  同名英文，所有 locale 都缺失时显示 `[semantic.key]`；每类诊断去重并受 128 条上限约束。
- 运行时状态消息若来自严格领域服务，保留其原始诊断文本作为最后降级；玩家稳定路径使用
  语义 key。开发统计值不翻译，诊断窗口标题和进入提示必须翻译。

## 语言选择与设置迁移

- 首轮只支持 `en-US` 和 `zh-CN`，默认 `en-US`。语言在暂停设置页选择，成功原子保存后立即
  影响 UI、目标、材料名和字幕，不需要重启，也不改变当前世界身份。
- `bin/config.txt` 升为 settings v3，新增 `locale <ll-RR>`。legacy v0、v1 和 v2 均以
  `en-US` 原子迁移；旧版本混入 `locale`、未知 locale、重复字段和尾随数据一律拒绝。
- Defaults 把语言恢复为 `en-US`；Cancel 恢复会话快照；保存失败时语言和其他设置一样不进入
  已应用状态。

## 中文字体与排版

- ImGui 使用随包发布的 Noto Sans SC 2.004 字体，来源为 Google Fonts 的 `notosanssc` 目录，按
  SIL Open Font License 1.1 分发；字体文件、OFL 原文、来源 URL 和 SHA-256 都进入资产清单。
- 字体文件 SHA-256 为 `A3041811A78C361B1DE50F953C805E0244951C21C5BD412F7232EF0D899AF0DA`；
  字体图集覆盖 ImGui 默认拉丁字符与常用简体中文范围。
  字体文件缺失或解析失败时确定性回退 ImGui 默认字体并输出一次有界诊断；客户端仍可启动、
  保存和退出，中文缺字属于可见降级而不是崩溃。
- UI 缩放范围保持 0.75-1.75。核心窗口使用可换行文本、稳定 `##id` 和可滚动区域；翻译文本
  不参与游戏逻辑比较。自动验证覆盖长文本测量、最小窗口和两端 UI 缩放的有界布局模型，
  真人可读性按 `docs/manual-product-experience-acceptance-v1.md` 独立记录，不能由自动测试
  伪装成 PASS。

## 字幕与 Credits

- 声音定义的稳定 cue id 映射到 `audio.<cue-id>.caption`；音频定义中的英文 caption 只作为
  无本地化目录时的防御性降级，不作为正常显示来源。
- 字幕启用后显示 2.5 秒；相同 cue 刷新时长，战斗警告可替换环境字幕。暂停时 UI 继续可读，
  关闭字幕立即清空。N12B 改用采样音效时 cue id 和字幕 key 不变。
- 主菜单提供 Credits 入口。Credits 展示项目名和每项已经随包发布的第三方表现资产名称、
  作者或来源、许可证和本地许可证路径；N12A 先列出字体，N12B/N12C 在各自实际加入音效/音乐
  后追加对应条目。界面文案本地化，许可证原文不翻译。

## N12B/N12C 音频边界

- N12B 的采样文件定义必须记录逻辑路径、类别、2D/3D、gain、并发上限和 caption key；缺文件、
  缺设备或后端失败进入静音降级，不阻止世界加载、保存或退出。缓存和声部上限由 N12B 冻结。
- N12B 冻结 `Base.audio` v3 与 9 个项目原创 WAV：44.1 kHz/mono/PCM16，单文件最大 512 KiB、
  10-3000 ms、最多 32 个唯一采样、解码缓存最大 4 MiB、全局 16 声部。当前解码缓存为
  312,230 字节；文件只在启动冻结阶段读取，`waveOut` 提交路径不做文件 I/O 或波形合成。
- `audio`、`audio-sample`、`license` 在 resource-pack v1 均为 base-only。定义或采样缺失允许
  静默降级，随包音频许可证缺失仍由清单/发行门禁拒绝。
- N12C 冻结 `Base.music` v1 和唯一 `overworld.quiet` 通道：项目原创 20 秒 WAV 为
  44.1 kHz/mono/PCM16，进入世界等待 6 秒、淡入 2.5 秒、淡出 1.8 秒，自然结束后间隔
  45-90 秒。Windows 后台线程最多提交 3 个 4096-frame 缓冲，不预载整首曲目。
- 暂停、设备挂起、切世界、回主菜单、静音、音乐音量归零、设备失败和退出都有幂等回收语义；
  后台线程不能越过音频运行时生命周期。完整边界见 `docs/streamed-music-contract-v1.md`。
- 正式资产全部进入生成 manifest、隔离发行包和许可证检查。资源包是否可覆盖采样/音乐要在
  对应批次升版时显式声明，不能借用当前 text/presentation-font 的 base-only 规则。

## 验收边界

- 自动门禁：两语 key 完全一致、非法 UTF-8/缺 key/缺字体/未知 locale、settings v0-v4 迁移、
  语义 id 映射、字幕时长、Credits 资产、长文本和极端缩放均有确定结果；Debug/Release 隐藏
  客户端及全量 Windows 门禁通过。
- R3 当前状态固定为“部分人工自测完成，其余延期”。已完成记录保留，延期项不标 PASS，且不
  阻塞 N12A-N12C 的自动开发门禁；Beta-RC 封板时再单独处理尚需真人判断的可读性与听感。

## N12A 已验收基线（2026-08-26）

- `en-US`/`zh-CN` 各 341 个 key 严格对齐；34 个材料 id、28 个目标的标题/指令/完成反馈与
  当前 9 个 cue id 均有语义映射。非法 UTF-8、超长值、未知 locale、缺 key 和 fallback 均有
  正反例；settings v3 以及 v0/v1/v2→v3 迁移通过。
- Noto Sans SC 2.004 与 OFL 原文进入 51 项 manifest 和 71 文件发行包；字体缺失、签名错误、
  超过 32 MiB、极端窗口/UI 缩放、长文本换行和 Credits 资产均有确定降级。字体 SHA-256 为
  `A3041811A78C361B1DE50F953C805E0244951C21C5BD412F7232EF0D899AF0DA`，OFL SHA-256 为
  `1C05C68C34F9708415AADA51F17E1B0092D2CEA709BF4A94CD38114F9E73D7D9`。
- VS2017/v141 Debug/Release 客户端 0 错误；双配置世界 675/675、目录 59/59、资源包 32/32，
  完整 Windows 门禁、隐藏客户端、38 个性能夹具、受控崩溃与打包均通过。同身份隐藏启动和
  进世界两场比较均 `PASS`。
- 双语真人截图、可读性与听感在本批完成时随人工项延期；当前改由
  `docs/manual-product-experience-acceptance-v1.md` 独立记录。本合同不把它们记作 PASS，N12A
  的自动开发门禁完成后继续 N12B。

## N12B 已验收基线（2026-08-26）

- `Base.audio` v3 的 9 个稳定 cue/caption id 分别引用 9 个项目原创 WAV；全部是 44.1 kHz、
  mono、PCM16。严格路径/RIFF/格式/时长/容量解析、相同路径去重和缺失/损坏静音降级通过；
  启动冻结报告 9 个定义、9 个唯一采样、312,230 字节解码缓存和零降级。
- Windows `waveOut` 只复制冻结 PCM 并应用 gain/pan/attenuation，不再在播放路径读文件或合成
  波形；dummy、暂停、停止、声部竞争和设备失败均保留有界语义。61 项 manifest、原创音频
  MIT 许可、双语 Credits 和 resource-pack v1 的 `audio-sample` base-only 拒绝均通过。
- VS2017/v141 Debug/Release 客户端与世界/资源包目标通过，世界为 681/681、资源包为 34/34；
  隐藏校验和真实三帧客户端均退出 0，真实后端为 `windows-waveout real=1`。完整 Windows 门禁、
  38 个性能夹具、受控崩溃和 81 文件打包通过，发行 ZIP SHA-256 为
  `1F7AEBFF35A796053A739D99CE060C14A7E39459A818DC1A6B609C5A778C416F`。
- 同身份隐藏启动的可用菜单为 `711.836/712.408 ms`，进入可控世界为
  `398.068/416.950 ms`，两项比较均 `PASS`。正式听感与真实无设备机器体验按独立产品体验合同延期，
  不标为 `PASS`；N12B 自动开发门禁完成后继续 N12C。

## N12C 自动验收基线（2026-08-26）

- `Base.music` v1 严格冻结唯一 `overworld.quiet`，项目原创 `quiet-horizons.wav` 为 20 秒、
  44.1 kHz、mono、PCM16；定义、WAV、MIT 许可和双语 Credits 进入 64 项 manifest。
- settings v4 新增独立音乐音量，v0-v3 缺省原子迁移为 0.65。双语目录各 352 个 key；定义/
  流格式、路径、时长一致性、首次延迟、淡入淡出、暂停/挂起/恢复、静音、音量归零、自然结束
  间隔、缺失/损坏资源和退出清理均有自动断言。
- Windows 真实三帧隐藏启动报告 `windows-waveout-stream real=1`、1 条曲目、1,764,000 字节 PCM、
  `degraded=0` 并退出 0。正式听感、真实暂停/切世界过渡和无设备机器体验仍按独立产品体验合同延期，
  不标为 `PASS`。
- VS2017/v141 Debug/Release 世界为 699/699、资源包为 38/38。完整 Windows 门禁通过 64 项
  manifest、38 个性能夹具、十三个测试目标、双配置隐藏客户端、启动负例、131,321 字节受控
  dump 和 84 文件干净发行包；符号归档 SHA-256 为
  `8283f062ff83cc6f050e92f5ff3b69aa48553307071888ba2d94867bad33ec87`，发行 ZIP SHA-256 为
  `DABE356BC511A110838DC1CD98BEE9392D0FC03FFBA1D40DB8271B6BD5392E2A`。
- 两轮同身份隐藏真实流式音乐采集的可用菜单为 `716.658/740.937 ms`、首次可控世界为
  `376.473/425.927 ms`，帧 P95/P99 为 `6.864/10.138 ms` 与 `6.388/10.043 ms`，均无超过
  50 ms 的帧；启动和进世界比较均 `PASS`。N12C 自动开发门禁完成后进入 BETA-RC。
