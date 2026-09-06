# T0/T1 地形基线与新版基础地貌

日期：2026-09-06。当前状态：T0 Done；T1 实现、兼容、macOS 门禁与固定诊断 PASS，必要正常输入验收 BLOCKED，Goal 未完成。
合同：[terrain-foundation-v5-contract-v1](../contracts/terrain-foundation-v5-contract-v1.md)。

## 结果与边界

terrain v5 追加了独立的连续高度/生态采样路径，使用 floor lattice、固定 uint64 hash/salt 和有界
坐标运算。新世界默认 v5；显式 v1–v4 保留原输出，已保存区块继续读取玩家修改。
宽尺度陆块、丘陵、山域、山脊和谷地连续组合后才分配生态，避免生态阈值直接制造高度断层。
148 以上使用连续软压缩，最后保留 1..176 安全界限；本批保持既有坡面岩土覆盖、树木、矿物
次数/高度、洞穴与结构投影职责。未扩展到水系、森林重做、3D 山体、D2 或新渲染系统。

| 固定生产采样指标 | v4 基线 | v5 | 冻结门槛结果 |
| --- | ---: | ---: | --- |
| 宏观高度范围 | 9..176 | 37..165 | PASS |
| 高度 63 占比 | 36.9314% | 1.6270% | PASS（每 seed/象限另验） |
| 高度 176 截顶占比 | 0.3125% | 0 | PASS |
| 宏观一格坡差 P99 | 2 | 1 | PASS |
| 宏观一格坡差 ≤1 占比 | 95.9047% | 99.6958% | PASS |
| 加密局部/边界最大坡差 | 15 | 2 | PASS（上限 3） |
| 规划地表低于水位占比 | 42.9121% | 20.9220% | 描述性指标 |

v5 的 32 个 seed/象限组合全部包含六种既有生态；最小高度标准差 14.8085，最少 97 种高度，
高度 63 最大占比 2.7832%。这些结果覆盖预先固定的样本，不声称证明了所有 seed 的所有坐标。
[基线统计](terrain-t0-t1-evidence/baseline-v4-summary.json)、
[新版统计](terrain-t0-t1-evidence/v5-summary.json)、
[统计与旧版兼容结果](terrain-t0-t1-evidence/v5-compatibility-statistical-result.json)。

[原版高度图](terrain-t0-t1-evidence/baseline-v4-height-maps.png) /
[新版高度图](terrain-t0-t1-evidence/v5-height-maps.png)。

## 版本、存档与资源验证

- 当前 Release 二进制重新采样显式 v1–v4：32 份 surface SHA256、128 份实际区块
  block/metadata/entity hash 与 T0 全部相同。不是复制原始 CSV 充当重跑。
- T1 聚焦 23/23 PASS：signed int 极值/种子采样、8 seed 四象限正逆生成、独立生成器并发、
  危险区块坐标在 halo 展开前拒绝、v5 玩家改块卸载/重开保留、无效 v0/v6 存档身份拒绝且旧文件保留。
- 8 seed 均通过实际出生支撑、两格净空和干燥检查；真实生成区块中找到木材/高草/煤/铁。
  搜索到全部四类资源的 chunk 半径分别为 0、1、0、0、0、5、0、3（按合同 seed 顺序）。
  seed 20260809/325322 使用安全陆地回退，不能称为出生区块内就有木材。
- 每 seed 均发现有效路标、遗迹和营地计划候选；这证明候选生成，正常步行和结构接近另列。
- P11-2 的 v4 地貌/洞口/身份回归 9/9 PASS；旧版 fixture 显式固定 v4，新版另验。
- 纯 TerrainFoundation 的 INT_MIN/INT_MAX 采样和 chunk hash 另通过 clang UBSan。

[聚焦摘要](terrain-t0-t1-evidence/t1-focused-resources-r2-summary.txt)、
[完整 Release 摘要](terrain-t0-t1-evidence/t1-full-release-r2-summary.txt)、
[旧版洞口回归](terrain-t0-t1-evidence/t1-p11-2-summary.txt)。

## macOS 构建与包

`bash scripts/verify_xcode.sh`：Debug/Release 各 13 个测试目标，WorldRuntime 各 1014/1014；
31 个生成工程图检查、客户端 validate 与 120 帧实际窗口探针全部 PASS，站立保存 Y=67。
没有首方编译警告。当前日志目录 `build/xcode-validation-20260906155049`，
[日志指纹与门禁摘要](terrain-t0-t1-evidence/macos-gate.json)。

起始 commit `bb83a7d`；已有 TODOLIST 对照入口和地形对照报告均保留。
本地提交：`6dbf7eb` 冻结 T0 与合同；`850dd85` 实现 v5 并通过上述门禁。
没有推送、发布或标签操作。

| 干净包 | 可执行 SHA256 |
| --- | --- |
| `build/terrain-t0-t1-20260906/HelloMine3D-T0-v4.app` | `c0446b47bbdcf4b4e2ef69248e4d466288db1a10700c821e37ca2fb2e6480424` |
| `build/terrain-t0-t1-20260906/HelloMine3D-T1-v5.app` | `221f50c5aa4669ea8a54ab8fa8f121bede39d58b2fa967307bd1224d119997c7` |

v5 包在 `850dd85` 干净工作区打包，独立携带资源，macOS x86_64 Release；
[包身份](terrain-t0-t1-evidence/v5-package-identity.json)。性能采集前两包配置文件和资源 manifest 一致。

## 固定画面对照

每包均 seed 20260807、初始 world time 6000、rotation `0 0 0`，同窗口/档位/视距；
三位置均未调整 Y。每场景 5s/10s 原始 readback 已采集并检查，场景名沿用 T0，不能据名称
推断新版该位置仍是相同生态。以下链接为 10s 原图；5s 图与元数据保留在原始输出目录。

| 固定机位 | v4 原图 | v5 原图与实际变化 |
| --- | --- | --- |
| forest `(256,90,256)` | [林地海岸](terrain-t0-t1-evidence/baseline-forest.png) | [海床](terrain-t0-t1-evidence/v5-forest.png)：规划高度 55，重力后机位在水下 |
| negative `(-256,70,-256)` | [浅水平面](terrain-t0-t1-evidence/baseline-negative.png) | [海床缓坡](terrain-t0-t1-evidence/v5-negative.png)：规划高度 47 |
| mountain `(1024,140,1024)` | [岩石地形](terrain-t0-t1-evidence/baseline-mountain.png) | [缓坡林地](terrain-t0-t1-evidence/v5-mountain.png)：规划高度 75 |

另补充新版负域 [山麓](terrain-t0-t1-evidence/v5-ridge-foot.png) 与
[山脊](terrain-t0-t1-evidence/v5-ridge-crest.png)：seed 相同，XZ 分别 `(-480,-480)` / `(-480,-640)`，
规划高度 119 / 159。它们保留原三场景，不参与固定性能中位数。
岩石材质仍较单一，山顶也有整数高度形成的宽平段；本批没有将这些观察改称材质或审美改善。

[客户端原始图像/逐帧数据指纹](terrain-t0-t1-evidence/v5-client-artifacts.json)。

以上均是 **DEVELOPER_DIAGNOSTIC**，有强制起点；不替代正常玩法或证明沿途可步行。

## 性能成本

每种客户端场景暖机 5s、采集 30s，共三轮，比较中位数；未调整合同门槛。

| 场景/指标 | v4 | v5 | v5 允许上限 |
| --- | ---: | ---: | ---: |
| steady frame P95 | 10.782 ms | 12.704 ms | 14.938 ms |
| steady frame P99 | 13.075 ms | 13.555 ms | 17.690 ms |
| steady update P95 | 3.822 ms | 2.420 ms | 5.278 ms |
| streaming frame P95 | 14.576 ms | 13.710 ms | 19.491 ms |
| streaming frame P99 | 16.690 ms | 15.591 ms | 22.028 ms |
| streaming update P95 | 10.977 ms | 10.333 ms | 14.221 ms |

客户端门禁 PASS。steady P95 增加 17.8%，streaming P95 下降 5.9%；不能概括为帧时间全面改善。
同机位的生态和可见几何变化也是本次成本的一部分。steady 常驻 terrain buffer 峰值中位数
44.24 MB→28.45 MB，streaming 40.15 MB→28.50 MB；CPU-ready P95 中位数分别 6→0、15→3。
steady v5 的可见 mesh 构建在暖机内完成；测量段新增 rebuild 为 0，v4 为 770。
streaming 测量段 rebuild 中位数 2481→2802，累计 mesh 成本差值 2900.06→2782.65 ms。

update 包括上传和其他主线程工作；当前没有独立 upload timer，不将其称为纯上传耗时。
原始 frames.csv、累计 mesh 差值、buffer 和 backlog 均保留；没有新增硬预算例外。
[三轮完整比较](terrain-t0-t1-evidence/client-performance-comparison.json)。

独占本任务构建/图形工作期间的 32 区块 headless 三轮中位数：v4 156.695 ms，v5 93.154 ms，
比例 0.5945，满足 ≤1.5 门槛。它不包含 CSV 采样/I/O，也不代表客户端帧时间。
[原始耗时与二进制身份](terrain-t0-t1-evidence/headless-comparison.json)。

## 正常输入验收：部分 PASS，路线 BLOCKED

[独立验收报告](terrain-t0-t1-evidence/normal-acceptance/report.md)、
[操作与失败记录](terrain-t0-t1-evidence/normal-acceptance/operations.md)、
[只读存档摘要](terrain-t0-t1-evidence/normal-acceptance/save-summary.json)。

两版正常菜单创建同 seed 世界、保存退出、菜单重开、静止位置/健康值保留及最终正常退出 PASS。
新版正常出生为密林草土阶梯坡，旧版为海岸沙滩；这是各版本自然出生范围对照，出生坐标不同。

**受控往返步行、跳跃坡面通行、正常采集 BLOCKED；结构进入和采集修改重开 NOT_RUN。**
公开 CUA App API 只有瞬时 `pressKey`，无持续按下/松开接口；W/D/S/A 多次短按未产生可重复的
受控位移，`w+space` 明确报 `keyPressIncludedMultipleNonModifierKeys`。两版 Casual 世界保存
位置均仍等于出生点。首次 v5 Normal 有约 17.2 格位移，但同时受到攻击，不能排除击退影响。

Normal 受伤至 HP2；另建 Casual 世界重试，新版保存重开保留 HP8，旧版 HP20。库存均空，
不声称正常取得了资源或完成了可达路线。失败属于目前采集/输入路径的证据限制，不能直接
推断为游戏碰撞 FAIL。需要可持续输入的公开接口或一次真实人工正常输入补证。

实现者与验收者上下文分开，仓库仍可访问，`context_isolation=PARTIAL`；不声明 PACKAGE_ONLY、
strict AI-06 blind PASS 或整体 functional_playability。全部游戏已正常退出。

## 失败与修正记录

| 运行 | 原因与处理 | 保留证据 |
| --- | --- | --- |
| T0 首次构建 exit 65 | sandbox 无法写 Xcode module cache；正常缓存权限重试成功，未改编译参数 | `baseline-client-build.log` / `baseline-client-build-retry.log` |
| candidate1 统计 FAIL | seed 0/1/42/8675309 加密边界坡差 5/4/5/4；拓宽谷地尺度/过渡，门槛仍为 3 | [失败条目](terrain-t0-t1-evidence/candidate1-statistical-result.json)，原候选源码/CSV 在 `candidate1/` |
| 首轮资源检查 8 FAIL | 完整 ChunkBlock 比较漏计 Mature 高草 metadata；修正为 ID 计数，并量测安全出生周围真实资源距离 | [失败摘要](terrain-t0-t1-evidence/t1-focused-resources-summary.txt) |
| 完整 Release 1 FAIL | S4.3 在已有 Stone 上重复写 Stone，没有 dirty 事件；改为保证一次真实修改，保留保存事件断言 | [失败摘要](terrain-t0-t1-evidence/t1-full-release-summary.txt) |
| 正常窗口前四次截图 FAIL | CUA 启动的进程未匹配旧 bundle 注册查询；补充内核解析的精确 executable 路径匹配，再使用正常桌面访问权限，后续原图成功；旧包负例查询仍为空 | [完整失败索引](terrain-t0-t1-evidence/normal-acceptance/capture-index.json) |
| 首轮 macOS client validate FAIL | 新地形为干燥陆地，旧 fixture 依赖天然水；仅在 validation-only mesh 夹具显式加水，保留水体 mesh 非零门槛 | `build/xcode-validation-20260906154546/` |

candidate2 及以后没有修改生产地貌输出。所有失败保留，未删除测试或事后放宽统计/性能阈值。

## 重现与证据索引

大产物根：`build/terrain-t0-t1-20260906/`。T0 原始 CSV 为 `baseline-v1/`..`baseline-v4/`；
当前带源码/二进制身份的重跑为 `qualified-survey/v1/`..`v5/`；
[当前采样身份](terrain-t0-t1-evidence/qualified-survey-identity.json)、
[当前原始文件指纹](terrain-t0-t1-evidence/v5-qualified-artifacts.json)、
[T0 原始文件指纹](terrain-t0-t1-evidence/baseline-artifacts.json)。

```sh
# 在仓库根执行；OUT 必须是不存在的新目录，避免覆盖历史证据。
HELLOMINE3D_WORLD_SMOKE_FOCUS=T0-SURVEY \
HELLOMINE3D_TERRAIN_SURVEY_VERSION=5 \
HELLOMINE3D_TERRAIN_SURVEY_DIR="$PWD/OUT/v5" ./bin/HelloMine3DWorldRuntimeSmoke
python3 tools/analyze_terrain_survey.py OUT/v5 --plot
# 同样分别运行 v1..v4 后：
python3 tools/validate_terrain_foundation.py --survey-root OUT
HELLOMINE3D_WORLD_SMOKE_FOCUS=T1 ./bin/HelloMine3DWorldRuntimeSmoke
bash scripts/verify_xcode.sh
python3 tools/compare_terrain_performance.py build/terrain-t0-t1-20260906 \
  --output /tmp/terrain-performance-comparison.json
```

固定窗口复现入口为 `tools/capture_terrain_macos.py --app APP --output NEW_DIR --scene SCENE`；
合法场景与起点直接定义于工具中。分析器/统计验证器自测 PASS，能拒绝旧区块漂移、地表漂移、
平台、过大坡差和缺生态。性能比较读取实际 capture/summary/frames，拒绝错误版本、seed、配置或时长。

Windows 专属构建与运行按用户授权后置；人类审美、趣味性与长期舒适度为 NOT_CLAIMED。

## 恢复区

工程实现与全部自动/固定诊断证据已交付；必需正常步行/采集/结构路线仍受阻，T1 与 Goal 未完成。
v5 输出已建立候选指纹检查，但待必要正常验收后才正式冻结。用户已获知工具限制，并被询问
是否愿意手动补证；没有将等待或缺少答复视为通过。后续只补动态缺项，除非新改动/失败需要，
不重复已通过的完整构建与采样。
