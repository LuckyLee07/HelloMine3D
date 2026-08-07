# 区块流式加载回归分析

2026-08-07 记录。本文档保存一次地形刷新回归的完整诊断、修复和残留问题，
避免后续再犯同样的错误。

## 症状

地形刷新明显变慢，且"越看越缺"。性能基线显示流式加载**从头到尾没有追上过**：

| 时间 | 已加载区块 | section 总数 | 仍脏 | 已上 GPU |
| ---- | ---------- | ------------ | ---- | -------- |
| 1.0s | 227 | 1578 | 1544 | 34 |
| 4.1s | 260 | 1802 | 1639 | 162 |
| 7.1s | 289（满） | 2013 | 1733 | 279 |
| 9.1s | 289 | 2013 | 1609 | 403 |

区块加载 7 秒跑满，但 mesh 构建只有约 46 个/秒，2013 个 section 要 44 秒才建完。

一个关键排除项：**Debug 与 Release 的 mesh 重建数几乎相同**（449 vs 456）。
如果是算力瓶颈，Release 至少快 3–5 倍。所以瓶颈是调度，不是计算。

## 根因

全部来自 `7a229d8 refactor: 整合区块流式加载与世界状态管理`（23 文件，+1557/−232）。
该提交同时做了三件事：修坐标与线程安全 bug、接入存档、重写调度策略。前两件是对的，
回归在第三件。

| # | 改动 | 影响 |
| - | ---- | ---- |
| 1 | sleep 从「每圈一次」变成「每个目标一次」 | `kTargetsPerPass = 1` 使内层循环每建 1 个 mesh 就 `sleep(2ms)`。Windows 计时器精度约 15.6ms，吞吐上限约 64/s |
| 2 | 视锥过滤被删除 | 旧 `Chunk::makeMesh(camera)` 只建 `isBoxInFrustum` 为真的 section；新代码建全部 2013 个 |
| 3 | 建完塞回队尾 | 一个 chunk 的 7 个 section 分 7 轮建完，每轮等 289 元素队列转一圈 |
| 4 | 丢失「建到就回最内圈重来」 | 旧代码 `break` 后从最近处重扫，永远先补眼前的 |
| 5 | 卸载改为同步存盘且在主锁内 | 旧代码 `chunkMap.erase(itr)` 直接删；新代码走 `unloadChunk()` 写文件，而 `renderWorld` 全程持有 `m_mainMutex` |
| 6 | GPU 上传限 4 帧 | 未上传的 section 无法绘制，等于给「地形多久可见」加了 240/s 的硬上限 |

### 为什么没被发现

性能基线工具（`tools/run_perf_baseline.ps1`）是 `ab46a10` 才加的，比 `7a229d8` 晚两个提交。
这次重构落地时没有任何能发现它的手段。

## 修复

不能直接回滚：旧代码有 `std::max(cameraX - i, 0)` 的负坐标 clamp（S0.1 修的）和
worker 直读 `const Camera&` 的数据竞争（S0.3 修的）。要恢复的是行为，不是代码。

| 对应 | 修复 |
| ---- | ---- |
| #1 | `World::loadChunks()` 改为**墙钟时间预算**：每轮工作至预算耗尽再让出锁，而不是每个 mesh 睡一次 |
| #3 #4 | 建出 mesh 的目标 `push_front`（继续做完这个 chunk），仅在邻居未就绪时 `push_back` |
| #5 | 新增 `World::unloadDistantChunks()`，从 `renderWorld` 移到 `World::update()`，并限制每次最多卸载 8 个 |
| #6 | `kMeshBufferBudgetPerFrame` 4 → 32 |
| — | `renderWorld` 中把 camera chunk 计算提到循环外 |

### 为什么用时间预算而不是固定次数

第一版把 `sleep` 直接换成 `std::this_thread::yield()`。Release 下效果很好，
**Debug 下却灾难性地退化**：只采到 1 帧，单次 `update` 耗时 12.8 秒。

原因是 mesh 构建在持有世界锁的情况下进行，而 `std::mutex` 在 Windows 上不保证公平。
Debug 下单个 section 约 6ms，Release 约 1.5ms。worker 一旦不睡就持续抢锁，主线程被饿死。

**原来那个「每个 mesh 睡一次」其实无意中充当了限流器，保住了帧率。**

固定次数预算在两种配置下表现差异过大。墙钟时间预算可以自平衡：
Release 每轮做很多个 section，Debug 做少数几个，两者都不会饿死主线程。

## 效果

同一场景（`-Seed 296595 -PlayerPosition "2766 102 2905"`），10 秒窗口：

| | 修复前 (Debug) | 第一版 yield (Debug) | 时间预算 (Debug) | 时间预算 (Release) |
| --- | --- | --- | --- | --- |
| `sampled_fps` | 59.1 | **18.7** | 60.1 | 60.1 |
| `update_p95_ms` | 0.156 | **89.9** | 0.108 | 0.033 |
| `frames_over_33ms` | 2 | 41 | **0** | **0** |
| mesh 重建 | 449 | 1615 | 1228 | **2013** |
| 结束时仍脏 | 1564 | 411 | 604 | **0** |

Release 完全追平：全部 2013 个 section 建完并上传，帧率与卡顿均无回退。

## 后续：halo cache（M3，已完成）

上一轮遗留的根本限制是「mesh 构建持有世界锁」。`ChunkMeshBuilder` 通过
`World::getBlock()` 读取邻居方块，需要 chunk map 保持稳定，所以整个构建都在锁内。

### 做法

新增 `SectionMeshInput`：18×18×18 的方块快照，加上 `shouldMakeLayer()` 需要的
层实心标记（自身 y-1/y/y+1，以及四个水平邻居的 y 层）。

worker 的每个目标拆成三段：

```
锁内   beginMeshJob()   确保邻域已加载 → 找一个脏 section → 快照
锁外   buildMesh()      只读快照，完全不碰世界
锁内   finishMeshJob()  校验后安装
```

`finishMeshJob()` 会拒绝陈旧结果：`ChunkSection` 新增 `m_blockRevision`，每次
`setBlock()` 递增。若构建期间发生了方块编辑或同步重建，revision 不匹配，结果被丢弃，
section 保持 dirty 等待下一轮。这防止了锁外构建覆盖掉玩家的编辑。

主线程的方块编辑仍走 `ChunkSection::makeMesh()` 同步路径（快照+构建一次做完）——
单次编辑只影响少数 section，不值得为它引入异步。

### 顺带修掉一个渲染 bug

原 `buildMesh()` 用一个递增指针遍历方块，但 `shouldMakeLayer(y)` 为假时只 `continue`、
**不推进指针**。一个层是 256 个连续索引，所以只要跳过任意一层，之后所有方块都会
读到低 256 格的数据。

后果是地表被渲染成地下的方块。修复前后同一 seed、同一机位的截图对比：
原本一大片「沙地/石头」实际是被错读的草地，修复后正确显示为长满花草的草原。

改用按坐标读取（`m_pInput->getBlock(x, y, z)`）后此问题自然消失。

### 顺带：卸载扫描节流

`unloadDistantChunks()` 原本每帧扫描全部已加载 chunk。相机不跨 chunk 时不可能有
chunk 离开视距，所以改为仅在相机 chunk 变化时扫描（预算截断时保留 backlog 标记）。
这一项单独又带来约 26% 的 mesh 吞吐提升，因为主线程抢锁次数大幅下降。

### 清理

`ChunkManager::makeMesh()` / `Chunk::makeMesh()` / `Chunk::makeMeshes()` 这条链
在拆分后已无调用者，一并删除，避免两套并行的构建路径。

### 验证

`HelloMine3DWorldRuntimeSmoke` 新增 4 项断言（共 93 项）：

| 断言 | 内容 |
| ---- | ---- |
| `M3/halo-matches-world` | 18³ 全部 5832 个格子（含跨 chunk 边界）与 `World::getBlock()` 逐一相等 |
| `M3/block-revision-advances` | `setBlock()` 后 revision 递增 |
| `M3/edit-visible-in-new-snapshot` | 新快照能看到刚才的编辑 |
| `M3/section-available` | 目标 section 存在 |

## 残留问题

### R2 视锥过滤（回归 #2）未恢复

有意不恢复，原因有二：

1. 旧实现让 worker 直接持有 `const Camera&` 计算视锥，正是 S0.3 修掉的数据竞争。
   要安全恢复必须让主线程每帧发布视锥快照。
2. 过滤（而非排序）会导致转身时出现空洞。吞吐修好后，这个优化的收益已经不明显。

如果将来把视距提到 16 以上（约 7600 个 section），应该考虑按视锥**排序优先级**
而不是过滤。

### R3 空 section 未跳过

地下被完全包围的 section 建出来是空 mesh，纯浪费。加一个 non-empty/solid 计数
即可跳过。参见 `docs/minigame-reference.md` 第 7 条。

### R4 基准的 vsync 状态会漂移

2026-08-07 的测量中途，同一台机器、同一脚本、同一参数下 `sampled_fps` 从 60 跳到
1200 以上，`display_p95_ms` 从 15.8ms 掉到 0.02ms——**垂直同步失效了**。

用 A/B 确认过这与代码无关：把源码回退到 M3 之前重新构建，同样是 1167fps。
推测与窗口未激活时的桌面合成状态有关。

影响很实际：无 vsync 时主循环从 60Hz 变成 1200Hz，`World::update()` 的加锁次数
涨 20 倍，会把 worker 挤掉。**跨 vsync 状态的运行不可比**。

对比前先看 `sampled_fps`：若远高于显示器刷新率，说明该次运行 vsync 未生效。

### R5 渲染截图脚本存在竞态

进程完成两次截图后按 `HELLO_RENDER_CAPTURE_EXIT` 自行退出，但脚本的轮询有时会
报 `Process exited before runtime captures completed` 并返回非零，尽管两张 PNG
都已正确写出、stderr 为空。代码变快后更容易触发。属于工具缺陷，不是游戏问题。

## 教训

1. **性能工具要先于性能敏感的重构落地。** 这次回归存活了两个提交才被发现。
2. **一次提交只做一件事。** `7a229d8` 混了三件事，回归藏在其中一件里。
3. **基准场景必须钉死。** 脚本默认从 `world.meta` 读位置，而游戏退出会重写它，
   导致运行间不可比。现在必须显式传 `-Seed` 和 `-PlayerPosition`。
4. **Debug 和 Release 都要测。** 第一版修复在 Release 下完美，在 Debug 下灾难。
5. **数字异常时先证伪自己的改动。** vsync 漂移一度看起来像是 M3 造成的性能回退，
   把源码回退重新构建做 A/B，两分钟就排除了。没有这一步会去改根本没坏的东西。
6. **性能 bug 可能同时是正确性 bug。** 那个方块指针错位既拖慢了构建，也让地表
   渲染成了地下方块，而它存在了很久都没被发现。
