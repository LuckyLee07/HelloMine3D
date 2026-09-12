# M1 经典树冠与 T1 正常玩法补验

2026-09-12，macOS 15.7.3 / arm64 主机；Release 游戏程序为 x86_64。起始 commit
`1f2800c9d69bdee7cab0d51bd4adfd2b6dd6c43c`。本轮验收没有改动游戏运行时代码、shader 或材质。
M1 与 T1 均继续为 `Doing`；本文的正常界面操作是实现者自测，不是独立 AI 验收。

## 工程与包身份

打包时的 `1f2800c` 工作区快照在 macOS Xcode Debug/Release 的 13 个测试目标、WorldRuntime 各 1032/1032、
客户端 validate 与 120 帧真实窗口探针均 `PASS`，站立接触 Y=67。完整日志：
`build/m1-closure-20260912/xcode-gate-r3.log`、`build/xcode-validation-20260912180801/`。
首次默认 Xcode 缓存无法在沙箱写入，第二次使用工作区 DerivedData 后真实 GL3 窗口探针受沙箱
上下文限制；第三次在桌面环境完成原门禁。两次失败日志分别保留为 `xcode-gate.log` 和
`xcode-gate-r2.log`，不计作 PASS。

干净当前 Release 包：`build/m1-closure-20260912/HelloMine3D-Current.app`，109 项发行文件，
可执行 SHA-256 `f1a3621830709fb0efb22dda92d9f018f77ddc5f9b8ebf37c67ba68939190c3e`。
正常输入只运行该包的校验副本 `normal-input/Runtime.app`，未修改原包。
本轮测试期间 19:22 另有未参与本轮的花草顶点 shader 与采集脚本工作区改动出现，时间晚于
18:14 的当前包身份；本文构建/性能/正常界面结论**不覆盖这些后来改动**，也未清除它们。

## 经典树冠性能：旧包与当前包分别 FAIL

继续使用冻结的暖野 v1 包与 M1 经典树冠包、seed 20260807、相同林地/密林位置和图形设置。
每档暖机 5 秒、采集 30 秒、各 3 轮；18 次经典树冠采集均 `CAPTURED`，且记录
`standard=1 leaf_geometry=cube`。原 v1 基线仍取最初冻结的 18 次采集，不因本轮复测改写。
`open -n -W` 首次采集超时的失败记录原样保留；此后采集工具增加可选直接运行已验证包内
wrapper 的方式，并保存启动方法、环境、程序身份、帧 CSV、原图和日志。

| 旧候选包场景 | 阴影档 | 帧 P95 比值 | 帧 P99 比值 | ≤1.10 护栏 |
| --- | --- | ---: | ---: | --- |
| 林地 | Off | 1.038 | 0.903 | PASS |
| 林地 | Medium | 1.068 | 0.938 | PASS |
| 林地 | High | **1.215** | 0.992 | **FAIL（P95）** |
| 密林 | Off | 1.027 | 0.997 | PASS |
| 密林 | Medium | 0.940 | 0.691 | PASS |
| 密林 | High | 1.048 | 1.007 | PASS |

密林三档常驻 terrain buffer 比值约 1.00，满足 ≤1.35 的网格护栏。逐项原始汇总：
`build/m1-closure-20260912/classic-crown-{forest,dense}-performance.json`；逐帧记录在
`build/warm-v2-m1-20260912/classic-crown-direct-perf-*/performance/` 和 `dense/` 下。
旧候选包林地 High 冻结对照 P95 三轮中位数为 14.525→17.655 ms；P99 为
23.659→23.472 ms。该包可执行 SHA-256 是
`fbac855a56bd53a34d97fe2ce0aea559c269f34df43615495c0593fa8cacde13`，
不能直接代表随后修改过可执行文件的当前包。

针对启动/焦点差异，先冻结同启动方式复测计划 `matched-high-plan.md`，再直接运行 v1 包三轮。
诊断性同方式对照 P95 为 9.736→17.655 ms（1.813，仍 FAIL），P99 为
22.961→23.472 ms（1.022，PASS），结果在 `matched-high-comparison.json`。
两组 High 三轮均有明显快慢分布，慢帧在采样期不同时间段出现；已确认设置一致，最终均为
361 个区块、约 36.73 MB 地形 buffer。现有证据不足以定位因果，不能把焦点或机器波动当作
消除失败的理由。

又预先固定 `compat-high-plan.md`，在**同一 M1 可执行文件**中仅选用户兼容材质 profile，
固定林地 High 运行三轮。日志均确认 `standard=0 leaf_geometry=cube`；P95 为
9.079/9.113/9.116 ms，中位数 9.113 ms，P99 中位数 9.940 ms；最终同为 361 区块、
约 36.73 MB 地形 buffer。结果在 `build/m1-closure-20260912/compat-high-r{1,2,3}/`。
它把后续排查范围收窄到标准材质路径及其资源交互；仍无法单独判定纹理数组、采样代码或
GPU 缓存的具体责任，不据此修改未证实的 shader 语义。兼容路径不能代替标准路径交付。

为核实**打包时的 `1f2800c` 快照**，以本轮干净包另跑标准材质林地/密林、Off/Medium/High 共 18 轮，
全数 `CAPTURED`、`standard=1 leaf_geometry=cube`，仍使用冻结 v1 基线和原护栏。
当前包显式使用 settings v9 `visualdetail standard`；比较脚本只允许它与 v1 settings v8
隐式标准路径有此版本/选项差异，其余图形设置、seed、位置、姿态和时间均逐轮相同。

| 当前包场景 | 阴影档 | 帧 P95 比值 | 帧 P99 比值 | ≤1.10 护栏 |
| --- | --- | ---: | ---: | --- |
| 林地 | Off | 1.037 | 0.907 | PASS |
| 林地 | Medium | 1.059 | 0.912 | PASS |
| 林地 | High | 0.629 | 0.430 | PASS |
| 密林 | Off | 1.035 | 1.014 | PASS |
| 密林 | Medium | **1.156** | 1.019 | **FAIL（P95）** |
| 密林 | High | 1.019 | 0.985 | PASS |

当前包密林常驻 terrain buffer 三档比值均约 1.00，通过 ≤1.35 网格护栏。
当前包密林 Medium P95 三轮为 11.955/9.657/23.053 ms，冻结 v1 为
10.341/9.420/23.407 ms；中位数 10.341→11.955 ms（1.156）。两组都有高变异，
但合同只允许三轮中位数，不能剔除慢轮或改用最好值。无可靠证据证明具体运行时代码原因。
逐轮结果与同配置核验：`build/m1-closure-20260912/current-standard-performance.json`；
当前包 18 组原始帧/图/日志在 `current-standard-*/`，预声明计划为 `current-full-plan.md`。
**当前 M1 性能退出项仍为 `FAIL`，失败档位是密林 Medium。**

## 正常界面与玩法

通过当前干净包的真实菜单创建 Casual 世界 `M1 T1 Normal`，seed 20260807，terrain v5；
进入林地、暂停保存、从菜单重开，并完整退出/重启进程后再次从世界列表打开。
设置界面实际应用简体中文和英语各 0.85、1.00、1.25 倍，共六组；暂停页布局与语言均可见。
完整进程重启后再次打开设置，简体中文与 1.00 倍保持。六张原始窗口 PNG、截图元数据、
创建/进入画面在 `build/m1-closure-20260912/normal-input/`。这组为
`DEVELOPER_SELF_TEST`，不关闭独立 `AI-07` 的动态、音频和隔离要求。

正常输入连续 40 次短按 W 并试按 Space。存档位置出现约 17 格偏移，但同期持续遭敌攻击，
位移不能归因为受控步行。空库存且未取得木材；采集、坡面往返、结构接近和修改后重开均
没有可靠正常输入证据。公开 CUA App API 仅提供瞬时 `pressKey`，无按住/松开或有效的
W+Space 组合。因此 M1 正常移动/采集和 T1 必需路线仍 `BLOCKED`；不能以固定机位探针替代。
当前副本最终经游戏菜单正常保存退出。未进行 AI-06 所要求的 package-only 文件系统隔离，
AI-01/AI-07 仍非整体 PASS，AI-02..05/08 未运行。

## 恢复点

M1 剩余：定位并解决当前包密林 Medium P95 护栏、实际移动观察树冠动态、
正常采集/换物及完整独立视觉验收。
T1 剩余：用可持续按键的公开输入或真人键鼠补充步行、跳坡、采集、结构往返及保存重开。
自动门禁和已通过的六组静态界面不因上述缺项重复执行；新改动或新的失败另行定向验证。
