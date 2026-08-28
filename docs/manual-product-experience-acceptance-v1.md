# HelloMine3D 人工产品体验验收 v1

本文把视觉风格、双语可读性和听感从 R3 物理输入协议中拆出。它定义开发期间可以关闭单个
视觉批次的轻量记录，以及发行封板才需要的正式产品体验记录；本文建立的是验收合同，不表示
任何尚未执行的检查已经 `PASS`。

最后更新：2026-08-28。

## 与 R3 的边界

- `docs/manual-input-acceptance-v1.md` 只保留十二项历史物理输入基线；未来 Physical Input v2
  负责真实键鼠、窗口焦点、容器、战斗、死亡/重生、作物和保存重启。
- 本文不验证物理鼠标幅度、按键释放、窗口焦点恢复或原生关闭按钮，不能关闭 D2/D4/D6/R3。
- 固定截图、自动布局测试和 dummy 音频本身只证明确定性/失败边界；必须由明确承担结果的审阅者
  逐项观察并填写 `PASS/FAIL/BLOCKED`，生成脚本不得自行判定。

## A：开发者视觉检查

V10A、V10B1-B3、V10C-V10E 每批关闭前执行一次真实 Release 视觉检查。审阅者可以是项目所有者，
也可以是经项目所有者明确授权、实际逐图/逐帧观察证据的开发者或代理。证据按批次性质选择：

- 只改变静态资产/颜色参数且没有时间行为的批次，可以检查至少三个代表场景的隐藏
  RuntimeReadback 原尺寸固定图；
- 涉及动画、闪烁、相机运动、窗口缩放或时间稳定性的批次，必须使用带时间间隔的多帧序列、
  视频或 5-10 分钟真实 Release 窗口观察，单帧不能关闭时间行为；
- 自动化可以采集证据但不能生成主观结果，记录中的 `result/reason` 必须来自承担结论的审阅者。

记录至少包含：

```text
record_version=1
kind=developer_visual
batch=V10A
date=YYYY-MM-DD
commit=<40-hex>
configuration=Release
gpu=<adapter>
driver=<driver>
window=<width>x<height>;fullscreen=<0|1>
graphics=<shadow/post/other applicable identities>
scenes=<fixed scene ids inspected>
result=<PASS|FAIL|BLOCKED>
reason=<one concise sentence>
```

`PASS` 只表示该批合同中的可见问题在记录证据覆盖范围内得到改善，且没有观察到破面、严重过黑、
不可读或明显不适；只有多帧/连续观察覆盖的批次才可以同时声明没有闪烁或运动异常。它不要求
R3 的十二项、正式发行签字或物理输入结果；`FAIL/BLOCKED` 必须保留原因并阻止该视觉批次关闭。

未执行的预填记录可以暂用 `result=NOT_RUN`，并把 `date/gpu/driver/window` 保持为 `NOT_RUN`；
这只是可提交的待办状态，不属于验收结果。`tools/validate_developer_visual_record.ps1 -AllowNotRun`
只检查结构，关闭批次必须改用 `-RequirePass`。已关闭的 V10B2/V10B3/V10C 记录分别位于
`docs/developer-visual-record-v10b2.txt`、`docs/developer-visual-record-v10b3.txt` 和
`docs/developer-visual-record-v10c.txt`；V10C 使用十张 Release 原图及正午多帧序列，不以单帧
替代运动检查。

## B：正式视觉与可读性

VISUAL-RC/1.0-RC 使用同一最终构建身份完成：

1. 森林正午、海岸正午、森林黄昏、森林夜晚、洞穴入口、树冠下方、遗迹墙角和营地夜景；
2. 阴影 `Off/Medium/High` 与后处理 `Off/On` 的适用组合，确认关闭路径可用且高档无痤疮、
   悬浮、过曝、HUD 模糊或持续闪烁；
3. `en-US`/`zh-CN` 在 0.75、1.0、1.75 UI scale 的主菜单、世界列表、设置、容器、配方、目标、
   暂停、胜利和 Credits；确认无截断、重叠、缺字和不可辨识对比度；
4. 记录每个类别的 `PASS/FAIL/BLOCKED` 与一句理由。自动截图可以附作 before/after，但不能替
   操作者填写结果。

## C：正式听感

使用同一 Release 构建分别检查：

- UI、破坏、放置、拾取、制作、攻击命中、敌人前摇/格挡和环境提示是否可辨且不过度重复；
- 音乐首次进入、暂停/恢复、切世界、回主菜单、静音、音量归零和退出过渡；
- 系统有设备正常播放，真实无设备机器安全降级且不阻断保存/退出；
- 中英文声音字幕与实际 cue 语义一致。

听感 `PASS` 不关闭物理输入，Physical Input `PASS` 也不关闭听感。

## 正式记录规则

- 正式记录必须包含日期、40 位提交、配置、GPU/驱动、音频设备、窗口/分辨率、图形档位、
  操作者和偏差；视觉/可读性/听感分别给出结果，不允许用一个总结果掩盖单项失败。
- VISUAL-RC 可以在工程门禁完成后标记 `Done（工程）`，但任一适用人工类别未执行时必须保持
  `NOT_RUN/Verify`，不得创建 1.0 标签。
- 任何影响 shader、材质、HUD、字体、本地化、音频资源、混音或生命周期的后续改动，只重跑
  受影响类别；影响构建身份或公共设置格式时重跑全部正式类别。
