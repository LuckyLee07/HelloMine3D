# T0/T1 地形 Goal 执行报告

## 范围与身份

用户授权 T0 地形基线和 T1 新版基础地貌，含实现、macOS 验证、干净包、窗口对照、正常步行、
文档和本地提交。合同：[terrain-foundation-v5-contract-v1](../contracts/terrain-foundation-v5-contract-v1.md)。
起始 commit `bb83a7d`；起始改动为当前账本中的对照报告入口与未跟踪的
`terrain-visual-minigame-comparison-2026-09-06.md`，均为本会话上一阶段成果，保留并纳入交付。
MiniGame 只读参考；Windows 验证后置；无推送、发布或标签授权。

## 进度

- T0：基线采样、兼容夹具、三场景画面与三轮客户端性能已完成；量化阈值在 T1 实现前冻结。
- T1：Doing。T0 与冻结合同已提交 `6dbf7eb` 后开始生产生成器实现。
- 已读取 AGENTS、TODOLIST、对照报告、验证技能、相关架构、v4 合同和验证矩阵。
- macOS x86_64 Release 原客户端构建成功。首次 sandbox 构建因 Xcode module cache 写入失败
  （exit 65），保留 `baseline-client-build.log`；正常缓存权限重试成功，日志
  `build/terrain-t0-t1-20260906/baseline-client-build-retry.log`。没有调整生产代码或编译参数。
- 原版隔离包：`build/terrain-t0-t1-20260906/HelloMine3D-T0-v4.app`，106 文件；可执行 SHA256
  `c0446b47bbdcf4b4e2ef69248e4d466288db1a10700c821e37ca2fb2e6480424`。

## T0 结果

`T0-SURVEY` 为真实 WorldRuntimeSmoke 中的离线入口，不接入客户端。8 seed 每版本采样
463056 点、32 个实际生成区块；v1–v4 共 1852224 点和 128 个区块。四次运行均 PASS。
输出在 `build/terrain-t0-t1-20260906/baseline-v1` 到 `baseline-v4`，可通过同协议重现。
兼容夹具：[v1–v4 fingerprints](../../tools/fixtures/terrain/t0-v1-v4.json)。

v4 汇总：宏观 133128 点，高度 9..176；高度 63 占 36.9314%，截顶占 0.3125%；宏观
坡差 P99=2，加密边界最大坡差 15。负域基础地貌退化是本批主问题。
[详细统计](terrain-t0-t1-evidence/baseline-v4-summary.json)、
[高度图](terrain-t0-t1-evidence/baseline-v4-height-maps.png)、
[原始产物 hash](terrain-t0-t1-evidence/baseline-artifacts.json)。

原版固定诊断画面已检查：
[林地](terrain-t0-t1-evidence/baseline-forest.png)、
[负坐标浅水平面](terrain-t0-t1-evidence/baseline-negative.png)、
[山地](terrain-t0-t1-evidence/baseline-mountain.png)。三场景均使用原包的生产地形，5s/10s
readback 原图保留在 build；它们启用了强制位置，类型为 DEVELOPER_DIAGNOSTIC，不算 AI 玩法。
另以正常菜单创建 `T0 Baseline 20260807`、Play、暂停、Save and Quit；对应窗口原图与 JSON
在 `baseline-menu.*`、`baseline-normal-paused.*`。未将这个简短流程声明为完整步行验收。

独占本任务构建/采样工作期间采集 steady/streaming 各 3 次，暖机 5s、测量 30s：

| 场景 | frame P95 中位数 | frame P99 中位数 | update P95 中位数 |
| --- | --- | --- | --- |
| steady | 10.782 ms | 13.075 ms | 3.822 ms |
| fast-streaming | 14.576 ms | 16.690 ms | 10.977 ms |

[三轮明细](terrain-t0-t1-evidence/baseline-performance-medians.json)。CPU mesh 与上传相关
buffer/backlog 可由原始 frames.csv 复核；没有独立 upload timer，不将 update 冒充上传耗时。
分析器和验证器自测均 PASS；验证器能拒绝区块漂移、地表漂移、负域平台、过大坡差和缺生态。

## 验证状态

T0 基线统计/旧版快照/固定画面/客户端性能：PASS（开发者诊断范围）。
T1 统计门禁与初步确定性/存档检查：PASS；完整回归、新版图形/步行与性能仍待执行。
Windows：本次后置，不改写历史证据。人类审美与乐趣：NOT_CLAIMED。

## T1 候选记录

candidate1：已构建 Release 并采样。宏观高度 37..164，高度 63 占 1.5399%，截顶为 0，
六生态占比均超过 1%，单格坡差 ≤1 占 98.6367%。32 区块单轮生成 92.247 ms；这个单轮
结果不能替代合同要求的三轮性能中位数。
**统计门禁 FAIL**：seed 0/1/42/8675309 的加密边界坡差为 5/4/5/4，超过冻结上限 3。
原候选代码、采样和结果保留在 `build/terrain-t0-t1-20260906/candidate1`；
[失败条目](terrain-t0-t1-evidence/candidate1-statistical-result.json)。
candidate2 将谷地噪声尺度由 260 拓宽到 400、过渡上界从 0.22 拓宽到 0.45；不调整门槛。

candidate2：统计门禁 PASS，宏观高度 37..165，高度 63 占 1.6270%，截顶为 0，
单格坡差 ≤1 占 99.6958%，宏观坡差 P99=1；加密局部与边界最大坡差均 ≤3。
原始与派生证据在 `build/terrain-t0-t1-20260906/candidate2/v5`。
初步 T1 聚焦 7 项 PASS：全 signed int 采样、8 seed 正逆和独立并发、越界 chunk 拒绝、
保存修改卸载及重开、未知存档版本拒绝。纯基础采样另通过 clang UBSan 的 INT 极值检查。

资源检查首轮 FAIL（23 checks / 8 failures），保留 `t1-focused-resources.log`。
定位到测试以完整 ChunkBlock 比较 TallGrass，漏计带 Mature metadata 的高草；修正为 ID 计数。
同时首次 5×5 区块观察显示 seed 20260809/325322 的实际安全出生使用无树回退；后续测量
从出生点逐圈搜索资源的实际距离（最大 16 chunk），不把出生点附近有资源视为已证明可步行。

资源/出生/结构聚焦复跑 23/23 PASS；木材/高草/煤/铁在出生点最远 5 chunk 半径内找到
（seed 20260809 为 5，325322 为 3，其余为 0..1）。这是存在性和实际生成证据，步行单列。
完整 Release 首轮 1014 项中 1 FAIL：S4.3 固定位置在 v5 已为 Stone，测试重复写 Stone 没有
产生 dirty/save 事件；改为依据当前块执行真实变化，保留事件断言。复跑 1014/1014 PASS。
P11-2 的 v4 地貌/洞口/身份专用回归 9/9 PASS。
当前二进制重新采样 v1–v5，旧 32 surface SHA / 128 chunk hash 全部一致，v5 冻结统计全部 PASS。
32 区块 headless 三轮中位数 v4 156.695 ms，v5 93.463 ms，比例 0.5965，满足 ≤1.5 门槛；
此结果不代表帧时间。客户端双配置门禁正在执行。

macOS 完整门禁首轮：Debug 13 个工程测试（含 WorldRuntime 1014/1014）通过；客户端
validate probe FAIL，water sections/vertices/indices 为 0。新 v5 出生夹具位于干燥陆地，旧
probe 仅显式摆放 glass/flora，水体依赖自然地形。现将水体加入 `!uploadToOgre` 的已有验证
夹具，保留原水体 mesh 非零断言，不修改正常玩法。失败目录
`build/xcode-validation-20260906154546`。窗口站立场景改用同 seed 的实际 v5 安全出生
`88.5 67 104.5`，Y 仍要求 65..67；与基线固定画面对照协议无关。

macOS 复跑完整门禁 PASS：Debug/Release 各 13 个测试目标，WorldRuntime 各 1014/1014，
31 个生成工程图检查、客户端 validate 与 120 帧窗口探针全部通过，站立保存 Y=67。
日志目录 `build/xcode-validation-20260906155049`；首轮失败未删除。

## 恢复区

下一步：完成 8 seed 出生/资源/结构检查、旧版本重新采样兼容、完整 macOS 门禁；之后
构建新干净包，固定场景与性能对照、正常窗口步行/保存重开。原版包和失败证据继续保留。
