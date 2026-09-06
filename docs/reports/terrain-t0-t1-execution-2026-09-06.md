# T0/T1 地形 Goal 执行报告

## 范围与身份

用户授权 T0 地形基线和 T1 新版基础地貌，含实现、macOS 验证、干净包、窗口对照、正常步行、
文档和本地提交。合同：[terrain-foundation-v5-contract-v1](../contracts/terrain-foundation-v5-contract-v1.md)。
起始 commit `bb83a7d`；起始改动为当前账本中的对照报告入口与未跟踪的
`terrain-visual-minigame-comparison-2026-09-06.md`，均为本会话上一阶段成果，保留并纳入交付。
MiniGame 只读参考；Windows 验证后置；无推送、发布或标签授权。

## 进度

- T0：基线采样、兼容夹具、三场景画面与三轮客户端性能已完成；量化阈值在 T1 实现前冻结。
- T1：Todo。在基线及量化标准冻结前不修改生产生成器。
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
T1 实现、回归、新版图形/步行与性能：NOT_RUN。
Windows：本次后置，不改写历史证据。人类审美与乐趣：NOT_CLAIMED。

## 恢复区

下一步：添加调用真实生成器的 T0 survey，采集原始基线和兼容快照，打包/采集当前客户端，
依据实测冻结 T1 验收阈值后实现。基线日志/大产物放 `build/terrain-t0-t1-20260906/`，关键
可审阅摘要与小型兼容夹具随仓库交付。窗口操作只由一个执行者占用。
