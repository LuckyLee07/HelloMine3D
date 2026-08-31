# HelloMine3D Sandbox Architecture Lab
## 沙盒游戏系统设计 + 大世界架构：长期迭代与教程开发规划 v1.0

> 文档状态：Proposed
> 适用仓库：`LuckyLee07/HelloMine3D`
> 规划定位：在现有 VISUAL-RC / Beta 工程基线之后，新开一条 **Architecture Lab Track**。
> 核心目标：把 HelloMine3D 从“可玩的 C++ Voxel Sandbox”演进为一套可教学、可验证、可展示的 **沙盒系统设计 + 大世界架构实验工程**。
> 验收定位：自动化证明确定性和工程边界，AI/Computer Use 证明真实窗口中的功能可玩性；
> 人类乐趣、审美和物理设备手感不冒充 PASS。

---

# 0. 文档定位

HelloMine3D 现有产品线已经完成大量玩法、存档、性能、诊断、视觉和发行工程工作。新的 Architecture Lab 不应直接继续堆“更多内容”，也不建议简单命名为 Stage 11 / Stage 12，因为现有 Stage 编号已经承担产品开发含义。

本规划另起一条架构主线：

- **Track A — Sandbox Foundation Refactor**
- **Track B — Large World Architecture**
- **Track C — Emergent Sandbox Systems**
- **Track D — Large Scale Simulation**

它同时承担三种职责：

1. **工程迭代路线**：告诉项目下一步真正应该改什么。
2. **架构学习路线**：每一个系统都回答“为什么最终要这样设计”。
3. **教程内容路线**：每个里程碑都对应可独立阅读的一章或几章教程。

最终项目定位建议：

> **HelloMine3D — A C++ Voxel Sandbox Architecture Lab**

这里的 “Architecture Lab” 不等于纯引擎样例或静态工程展示。项目没有外部玩家验收团队，
但每项架构必须由真实游戏需求触发，并通过正常菜单、输入、世界状态和保存重开在游戏里被使用。
权威验收边界见 `docs/current/ai-assisted-gameplay-acceptance-v1.md`。

不是重点展示“做了多少 Minecraft 功能”，而是展示：

- World / Chunk / Section 如何组织；
- 无限世界如何 Streaming；
- 后台任务如何调度、取消和限流；
- Simulation World 与 Render World 如何解耦；
- LOD 如何从渲染延伸到 Simulation；
- 玩家搭建出来的复杂机械、物流、仓储网络如何动态维护；
- 数千机器、方块和 Actor 如何在预算内稳定运行；
- 存档、迁移、调试、性能和回归如何贯穿整个系统生命周期。

---

# 1. 总体目标

## 1.1 最终能力目标

完成本路线后，HelloMine3D 应具备以下能力：

### 世界架构

- 稳定的 World / Chunk / Section 数据边界；
- 明确的 Chunk Residency 生命周期；
- Camera / Player 驱动的 World Streaming；
- 可取消、可限流、可观测的后台 Job Pipeline；
- Near / Mid / Far 多层世界表示；
- Far Terrain / LOD；
- Simulation Activation；
- Save / Load / Backup / Migration 对新系统持续兼容。

### 沙盒系统

- 数据驱动 Block Behavior；
- 通用 Machine Framework；
- 通用 Dynamic Network Graph；
- Create-style Mechanical Network；
- Belt / Transport；
- Processing Machine Pipeline；
- AE2-style Storage Network；
- Auto Crafting Dependency DAG。

### 大规模模拟

- Fixed Tick Phase；
- Tick Budget；
- Spatial Activation；
- Full / Reduced / Dormant Simulation；
- Actor AI LOD；
- 远距离机器解析式推进；
- 大规模 Debug / Profiling；
- 30 分钟以上 deterministic soak；
- 存档恢复后网络和机器状态继续正确。

### 教学和展示

每个核心阶段必须同时留下：

- 设计文档；
- 失败方案；
- 架构图；
- 最小可运行 Demo；
- 自动化测试；
- 性能数据；
- 教程章节；
- 一组明确的 Trade-off。

## 1.2 可玩载体硬约束

Architecture Lab 必须同时保持三层结果：

1. **工程正确**：自动测试、存档迁移、资源、性能、长稳和发行包证据通过。
2. **功能可玩**：AI 通过 OS 级 Computer Use 在干净 Release 包里使用正常输入完成真实流程。
3. **声明诚实**：AI 只证明功能可操作和可观察，不把结果写成人类乐趣、舒适度或审美 PASS。

每个 Sprint 的新系统必须在游戏内有可观察结果；每个 Track 结束必须从主菜单走通既有主线和
该 Track 的真实 Demo。没有 Computer Use 环境时记录为 `NOT_RUN`，可以形成
`Engineering Done`，但不能写成 `AI Playability PASS`。

---

# 2. 非目标

本路线默认不把以下内容作为主线目标：

- 完整复刻 Minecraft；
- 完整复刻 Create / AE2 / Mekanism 等 Mod；
- 直接翻译任何 Mod 的 Java 源码；
- 引入第三方 Mod 原始模型、贴图和音频；
- MMO / 大型多人服务器；
- UE World Partition 级别编辑器工作流；
- 完整物理破坏；
- 无限复杂的电路模拟；
- 为了 ECS 而强制 ECS 化整个项目；
- 为了“架构漂亮”而一次性重写现有稳定代码。

核心原则：

> **先让真实需求逼出架构，再抽象。**

---

# 3. 当前工程基线与改造原则

当前 HelloMine3D 已经不是简单 Voxel Demo，它已有：

- `World / ChunkManager / Chunk / ChunkSection`
- Greedy Meshing
- Chunk Mesh Dirty Queue
- 后台加载
- 固定 Tick
- 确定性地形生成
- Block Behavior
- Block Entity
- Event Bus
- Actor
- Crafting / Tool / Combat / Crop
- World Storage Transaction
- Backup / Restore
- Diagnostics / Crash Dump
- Runtime Performance Metrics
- Ogre Rendering
- ImGui Debug UI

因此新路线 **不从重新造 Chunk 开始**。

需要做的是：

> **把当前“已经能跑”的实现，逐步演进成职责明确、可扩展、可验证的大世界系统。**

---

# 4. 目标架构

建议最终演进到下面的高层结构：

```text
                     Application / Ogre Shell
                              │
                              ▼
                         World Facade
                              │
          ┌───────────────────┼────────────────────┐
          │                   │                    │
          ▼                   ▼                    ▼
     World Model         World Runtime        World Persistence
          │                   │                    │
          │          ┌────────┼─────────┐          │
          │          │        │         │          │
          ▼          ▼        ▼         ▼          ▼
       Chunk      Streaming Simulation Actor     Save/Backup
       Block       Scheduler  Scheduler Runtime   Migration
       Entity          │         │
                       │         │
                 ┌─────┴───┐ ┌───┴──────────────┐
                 ▼         ▼ ▼                  ▼
               Jobs      LOD Network          Machine
                            Graph              Runtime
```

渲染侧：

```text
Authoritative World State
          │
          ▼
   Immutable Snapshot
          │
          ▼
       Ogre Layer
          │
    ┌─────┼────────┐
    ▼     ▼        ▼
 Terrain Actor     UI
 Render  Render   Debug
```

关键约束：

> **Ogre 不拥有 Gameplay Truth。**

> **Worker Thread 不直接修改 Authoritative World。**

> **后台任务只消费 immutable snapshot，最终通过 revision / generation token 在主线程提交。**

---

# 5. World 重构原则

当前 `World` 不需要删除，也不应该一次性重写。

它应该从：

```text
World = 所有功能实现者
```

逐步变成：

```text
World = 生命周期 + Facade + 系统组合根
```

推荐最终拥有：

```cpp
class World {
public:
    void fixedTick();
    void updateStreaming(const WorldViewPoint&);
    bool save();

    WorldQuery& query();
    WorldCommand& command();

private:
    ChunkService             m_chunks;
    WorldStreamingService    m_streaming;
    WorldSimulation          m_simulation;
    ActorRuntime             m_actors;
    NetworkRuntime           m_networks;
    WorldPersistence         m_persistence;
    SandboxEventBus          m_events;
};
```

注意：

- 不要求真的叫 `Service`；
- 不建议引入 Service Locator；
- 依赖关系尽量通过构造参数明确；
- 每次只提取一个稳定边界；
- `World` 可以长期保留兼容 Facade API。

---

# 6. 四阶段总路线

| Track | 主题 | 核心成果 |
|---|---|---|
| A | Sandbox Foundation Refactor | World 职责拆分、Runtime 边界、教程基础设施 |
| B | Large World Architecture | Streaming、Job、LOD、Spatial Activation |
| C | Emergent Sandbox Systems | Machine、Mechanical、Transport、Storage、Auto Craft |
| D | Large Scale Simulation | Simulation LOD、AI LOD、Reduced Simulation、综合 Demo |

推荐采用 **两周一个 Sprint** 的节奏；每个 Sprint 只关闭一个核心架构问题。

完整主线约 34 个 Sprint。可选内容不纳入核心关闭条件。

这 34 个 Sprint 是完整能力目录，不是自动获批的连续 backlog 或工期承诺。只有进入
`docs/current/todolist.md` 的下一批才是当前开发任务；后续 Track 必须由前一阶段暴露出的真实玩法或
规模问题触发，不能为了展示抽象而自动启动。

---

# 7. Track A — Sandbox Foundation Refactor

目标：

> 在不破坏当前玩法和存档的前提下，把 HelloMine3D 从“World 聚合实现”整理成适合继续承载 Streaming、Network 和 Simulation 的架构。

---

## A0 — Architecture Baseline Freeze

### 目标

在架构改造前冻结一个可信基线。

### 交付

更新现有现行文档，并新增一份合并基线报告：

```text
docs/current/architecture.md
docs/current/runtime-validation.md
docs/reports/architecture-lab-baseline-v1.md
```

基线报告内部用章节区分 architecture、dependency、performance 和 AI playability，不为同一
Sprint 再拆四份平行状态文档。

记录：

- World 当前公开 API；
- World 当前成员职责；
- ChunkManager 职责；
- Thread ownership；
- Save ownership；
- Event ownership；
- 主要 Tick 链；
- Render Snapshot 链；
- 当前性能基线；
- 当前自动化回归命令。
- 当前 AI/Computer Use 场景、未声明范围和 `NOT_RUN/PASS` 证据身份。

### 验收

- 架构改造前后能比较；
- 每个后续 Sprint 都知道是否引入性能或行为回归；
- 不改变任何 Gameplay。
- 使用既有干净 PLAYABILITY-RC 包完成或明确记录 `AI-08=NOT_RUN`，不得把隐藏客户端当作
  Computer Use PASS。

### 教程

**Chapter 00：从 Minecraft Clone 到 Sandbox Architecture**

核心问题：

> 一个“能跑”的游戏，什么时候开始需要架构？

---

## A1 — World Responsibility Map

### 问题

当前 World 同时承担：

- Chunk；
- Light；
- Random Tick；
- Natural Mob；
- Combat；
- Projectile；
- Difficulty；
- Waystone；
- Save；
- Background Loading；
- Mesh Queue；
- Event。

第一步不是重构，而是建立责任地图。

### 设计

把 World 方法分类：

```text
World Query
World Mutation
Simulation
Streaming
Persistence
Actor
Combat
Progression
Diagnostics
```

定义三个 API 概念：

```text
Query
Command
Runtime Tick
```

例如：

```cpp
struct WorldQuery {
    ChunkBlock getBlock(BlockPos) const;
    LightLevel getLight(BlockPos) const;
};

struct WorldCommand {
    BlockMutationResult setBlock(...);
    ActorId spawnActor(...);
};
```

早期可以只是 Facade Wrapper，不急着移动代码。

### 验收

- World.h API 被分类；
- 新增系统不得继续随意往 World 增加方法；
- 旧调用不要求立即迁移。

### 教程

**Chapter 01：God Object 为什么会自然产生？**

---

## A2 — Chunk Runtime Boundary

### 目标

把 Chunk 生命周期相关调度从 World 中提取。

推荐形成：

```text
World
  │
  └── ChunkRuntime
        ├── ChunkManager
        ├── ChunkUpdateQueue
        ├── MeshWorkPlanner
        └── Residency bookkeeping
```

### 重点

保留现有：

```text
beginMeshJob
build off-lock
finishMeshJob
```

把它明确升级为通用模式：

> Snapshot → Worker → Revision Validation → Commit

### 交付

建议新增：

```text
World/Chunk/ChunkRuntime.*
World/Chunk/ChunkResidency.*
```

或等价命名。

### 验收

- World 不再自己维护全部 Chunk Update Queue；
- 单方块编辑仍只 dirty 必要 section；
- 保存与 unload 语义不变；
- 所有原有 Chunk regression 继续通过。

### 教程

**Chapter 02：Chunk 不只是一个数组**

**Chapter 03：Derived Data 与 Authoritative Data**

---

## A3 — Simulation Runtime

### 目标

建立统一 Simulation 调度入口。

当前 Random Tick、Mob Population、Combat 等都是 Simulation。

建议形成：

```cpp
class WorldSimulation {
public:
    void fixedTick(WorldTickContext&);
};
```

内部第一阶段仍可以调用旧实现：

```text
Fixed Tick
 ├── Block Simulation
 ├── Actor Simulation
 ├── Combat
 ├── Population
 └── Gameplay Runtime
```

### 关键

不是马上把所有玩法搬进去。

先建立：

```text
Tick Phase
Tick Context
Tick Metrics
```

例如：

```cpp
struct WorldTickContext {
    int tick;
    float dt;
    SimulationBudget budget;
};
```

### 验收

- Fixed Tick 顺序文档化；
- 每个 phase 独立计时；
- Pause gate 仍正确；
- Replay / deterministic tests 不变。

### 教程

**Chapter 04：Fixed Tick 是沙盒模拟的时间骨架**

---

## A4 — Event / Command / Query Boundary

### 目标

明确：

```text
Command = 请求世界改变
Event   = 已经发生的事实
Query   = 读取世界状态
```

例如：

```text
BreakBlockCommand
      ↓
World mutation
      ↓
BlockChangedEvent
      ↓
Objective / Audio / Network / Render dirty
```

禁止：

```text
Event Handler
  ↓
无限隐式修改
  ↓
Event
  ↓
Event
```

### Event Bus 规则

事件分两类：

```text
Domain Event
Diagnostic Event
```

Domain Event：

- BlockPlaced
- BlockRemoved
- InventoryChanged
- MachineCompleted
- NetworkTopologyChanged

Diagnostic Event 不驱动 Gameplay。

### 验收

- 新 Network 系统不直接 hook Ogre；
- 新 Machine 系统不依赖 UI；
- 事件递归必须有明确边界；
- Event Handler 的 mutation 行为文档化。

### 教程

**Chapter 05：事件系统不是“全局广播”**

---

## A5 — System Registry & Tick Budget Foundation

### 目标

为后面机器、Network、AI 建统一系统调度骨架。

```cpp
class ISandboxSystem {
public:
    virtual void fixedTick(SystemTickContext&) = 0;
};
```

但不要过度 OOP。

真正需要的是：

```text
System
Priority
Budget
Metrics
Activation Policy
```

### 第一批系统

- BlockRandomTickSystem
- ActorSimulationSystem
- CombatSystem

### 验收

ImGui 能看到：

```text
Simulation
  Block Tick     0.18 ms
  Actor          0.32 ms
  Combat         0.06 ms
```

### 教程

**Chapter 06：为什么大型沙盒不能让所有对象每帧 Tick**

---

## A6 — Architecture Lab Documentation Pipeline

### 目标

建立以后每章教程统一格式。

每一个 Milestone 必须包含：

```text
01-problem.md
02-naive-solution.md
03-failure.md
04-design.md
05-implementation.md
06-validation.md
07-tradeoffs.md
```

实际仓库可压缩成单篇 Markdown，但逻辑必须保留。

统一章节模板：

1. 问题场景；
2. 最简单实现；
3. 为什么失败；
4. 新需求；
5. 架构演进；
6. C++ 数据结构；
7. Runtime Flow；
8. Debug 方法；
9. Benchmark；
10. Trade-off；
11. Exercises。

### Track A 完成定义

当下面条件全部满足，A 才算完成：

- World 不再继续无边界扩张；
- Chunk Runtime 有独立责任边界；
- Simulation Runtime 建立；
- Event / Command / Query 规则冻结；
- Tick Metrics 可观测；
- 旧存档、玩法、视觉全部保持。

---

# 8. Track B — Large World Architecture

目标：

> 解决“玩家快速移动时，世界数据、生成、Mesh、渲染和模拟怎样在预算内持续进入和离开内存”。

---

## B1 — Chunk Residency State Machine

不要把所有状态塞成一个 enum。

Chunk 至少存在三套互相正交的状态。

### Data Residency

```text
Absent
  ↓
Requested
  ↓
Loading / Generating
  ↓
Resident
  ↓
EvictRequested
  ↓
Saving
  ↓
Absent
```

### Mesh State

```text
Clean
Dirty
Queued
Building
CpuReady
```

### Render State

```text
NotResident
UploadPending
GpuResident
Stale
```

Render State 最终归 Ogre。

### 原则

避免：

```text
LoadedMeshDirtyCpuReadyGpuBufferedSaving...
```

这种组合状态爆炸。

### 验收

- 每个转换有合法入口；
- Debug UI 可显示当前状态；
- 非法转换 assert；
- unload 不会丢 dirty save；
- stale mesh 不会提交。

### 教程

**Chapter 07：Chunk State Machine**

---

## B2 — Streaming Demand Model

当前加载中心只是 Streaming 的一种需求。

未来需求来源可能包括：

```text
Player
Camera
Teleport Destination
Active Machine Network
Actor
Preload
Debug Camera
```

定义：

```cpp
struct ChunkDemand {
    ChunkCoord coord;
    DemandReason reason;
    int priority;
    uint64_t epoch;
};
```

### Priority

可以组合：

```text
distance
frustum visibility
movement direction
age
reason
```

不要只用距离。

### 验收

- 快速向前移动时前方 Chunk 优先；
- 转身不会让旧请求永久霸占队列；
- Teleport 可以临时提权。

### 教程

**Chapter 08：Streaming 的本质是 Demand Scheduling**

---

## B3 — Generic World Job Scheduler

### Job 类型

```text
ChunkIOJob
ChunkGenerationJob
ChunkMeshJob
FarLODJob
Optional LightingJob
```

统一：

```cpp
struct WorldJob {
    JobType type;
    JobPriority priority;
    CancellationToken token;
    Revision expectedRevision;
};
```

### Scheduler

需要：

```text
Pending Queue
In-flight Limit
Worker Count
Completed Queue
Cancellation
Backpressure
Metrics
```

### 禁止

```text
无限 push future
```

### 重要指标

```text
queued
inFlight
cancelled
completed
staleDiscarded
queueLatency
workerTime
commitTime
```

### 教程

**Chapter 09：后台线程不是性能魔法**

---

## B4 — Cancellation & Generation Token

典型问题：

```text
玩家在 A
请求 Chunk X
后台开始生成

玩家迅速跑到 B
Chunk X 已经不需要

Job 仍然完成
主线程又把 X 装回来
```

解决：

```text
Demand Epoch
Job Token
Chunk Revision
```

提交：

```cpp
if (!token.valid())
    discard();

if (chunk.revision != expected)
    discard();
```

### 验收

建立 Stress：

```text
高速直线移动
高速折返
随机 Teleport
Render Distance 频繁修改
```

要求：

- 没有 stale commit；
- Queue 有界；
- 内存最终回落；
- 没有死锁。

### 教程

**Chapter 10：为什么异步系统必须允许“白做”**

---

## B5 — Streaming Backpressure

### 问题

后台线程比主线程快：

```text
CPU Mesh
CPU Mesh
CPU Mesh
CPU Mesh
      ↓
Upload backlog
```

或者主线程比后台快：

```text
Player outruns generation
```

需要预算：

```text
MaxPendingGeneration
MaxPendingMesh
MaxCommitPerFrame
MaxUploadPerFrame
MaxUnloadPerFrame
```

### 原则

> **吞吐量不是越大越好，稳定延迟比峰值吞吐更重要。**

### 验收

Debug 显示：

```text
Demand     132
Queued      42
InFlight     4
Ready        7
Cancelled   81
```

### 教程

**Chapter 11：Backpressure 是大世界稳定性的核心**

---

## B6 — Spatial Activation

加载和模拟不是一回事。

定义多个 Spatial Ring：

```text
Ring A — Full Simulation
Ring B — Reduced Simulation
Ring C — Render Only
Ring D — Far LOD
Ring E — Unloaded
```

示例值仅作为可配置默认：

```text
0-8 chunks      Full
9-20            Reduced
21-64           Render / LOD
65+             Far representation
```

真实数值由性能测试决定。

### 新模块

```text
World/Streaming/SpatialInterest.*
```

输出：

```cpp
enum class ActivationLevel {
    Full,
    Reduced,
    RenderOnly,
    Far,
    Dormant
};
```

### 教程

**Chapter 12：Loaded 不等于 Active**

---

## B7 — Render World vs Simulation World

这是整个教程的重要转折。

```text
Simulation Chunk
    ≠
Render Chunk
```

模拟可能只需要：

```text
Block Data
Machine State
Actor State
```

而渲染需要：

```text
Mesh
Material
Light
LOD
```

目标：

Ogre 只消费：

```text
WorldMeshSnapshot
ActorSnapshot
FarTerrainSnapshot
```

### 验收

- Rendering 不能修改 World；
- Renderer 暂停/重建不影响 Simulation Truth；
- Headless tests 不需要 Ogre。

### 教程

**Chapter 13：世界不是画面**

---

## B8 — Far Terrain Data Model

参考 Distant Horizons 的“问题”，不要复制实现。

需要解决：

> 远处山体不值得保留完整 block mesh。

第一版可以非常简单：

```text
Chunk
 ↓
Surface Height Samples
 ↓
Color / Material Summary
 ↓
Far Tile
```

例如：

```cpp
struct FarTerrainCell {
    int16_t height;
    uint16_t surfaceMaterial;
    uint8_t light;
    uint8_t biome;
};
```

### Level

```text
LOD0 full block mesh
LOD1 2x2 summary
LOD2 4x4
LOD3 8x8
LOD4 16x16
```

不要一开始实现复杂 quadtree。

先做固定分层。

### 教程

**Chapter 14：从 Block Mesh 到 World Representation**

---

## B9 — Far Terrain Build Pipeline

```text
Resident Chunk
      ↓
Surface Extraction
      ↓
LOD Build Job
      ↓
Far Terrain Cache
      ↓
Ogre Far Renderer
```

必须回答：

- block 修改后哪一级 LOD dirty？
- chunk unload 后 Far 数据是否保留？
- Far 数据是否单独存盘？
- seed 可否重新生成？
- 玩家改造后的地形怎么办？

第一版建议：

```text
Generated Terrain → 可重建
Player Modified → 需要持久化 Dirty Summary
```

### 验收

- 视距明显扩大；
- GPU / CPU 不按完整 Chunk 线性增长；
- 修改地形后远景最终一致。

### 教程

**Chapter 15：LOD 也是缓存一致性问题**

---

## B10 — Large World Stress & Acceptance

建立固定测试：

### LW1 Straight Run

连续高速移动 10 分钟。

### LW2 Teleport Storm

随机跳到远距离位置。

### LW3 Turnaround

反复 180° 转身。

### LW4 Render Distance Churn

反复修改视距。

### LW5 Edit & Leave

修改 Chunk → 立即离开 → 保存 → 重开。

### 关键指标

不是死卡一个绝对毫秒值，而是：

- 不破坏现有正式 Q1 / Q3；
- Streaming 新增 phase 必须单独计时；
- Queue 必须有界；
- 内存必须达到稳定平台；
- stale result 必须可统计；
- 保存不得丢失；
- 30 分钟 stress 无 deadlock / crash。

### Track B 完成 Demo

**Infinite Trek Demo**

玩家持续向一个方向飞行/移动：

- 前方持续生成；
- 后方稳定卸载；
- 远方地形存在；
- Frame time 没有周期性巨大尖峰；
- Debug UI 显示整个 Streaming Pipeline。

---

# 9. Track C — Emergent Sandbox Systems

目标：

> 从“写一个机器”提升到“设计一种能让玩家自己组合出复杂系统的玩法基础设施”。

---

## C1 — Block Capability Model

不要不断写：

```cpp
if (block == Furnace)
if (block == Chest)
if (block == Gear)
```

为 Block Entity 引入轻量能力接口：

```text
InventoryProvider
MachineProcessor
MechanicalPort
ItemTransportPort
StorageProvider
```

注意：

- 不要求 Component 化所有 Block；
- Capability 是访问协议，不等于 ECS。

### 教程

**Chapter 16：数据驱动之后，下一步是 Capability**

---

## C2 — Generic Machine Framework

定义一个通用 Processing Machine：

```text
Input
  ↓
Recipe Match
  ↓
Progress
  ↓
Output
```

状态：

```cpp
struct MachineState {
    MachineStatus status;
    RecipeId recipe;
    int progressTicks;
};
```

Runtime：

```text
Idle
Running
BlockedOutput
MissingInput
NoPower
```

### 第一批机器

只需：

```text
Crusher
Furnace-like Processor
```

重点不是内容，而是框架。

### 教程

**Chapter 17：Machine 应该 Tick 自己吗？**

---

## C3 — Generic Dynamic Network Graph

这是沙盒系统主线核心。

数据结构：

```cpp
NodeId
NetworkId
PortId
Edge
NetworkComponent
```

拓扑变化：

```text
BlockPlaced
BlockRemoved
ChunkLoaded
ChunkUnloaded
BlockRotated
```

### 关键设计

新增连接很容易：

```text
Merge Components
```

删除连接更困难：

```text
Potential Split
  ↓
Connectivity Search
  ↓
Create New Components
```

因此不要单纯依赖 Union-Find。

建议：

- Network 规模中小时使用 BFS / DFS；
- topology dirty 后局部 rebuild；
- 以后再做增量算法。

### 教程

**Chapter 18：玩家建造行为就是动态图编辑**

---

## C4 — Create-style Mechanical Network

Node：

```text
Source
Transmission
Consumer
Converter
```

状态：

```text
RPM
Direction
Torque / Stress Capacity
Stress Used
```

第一版：

```text
Water Wheel
Shaft
Gear
Mechanical Drill
```

### 传播

```text
Source RPM
   ↓
Shaft
   ↓
Gear Ratio
   ↓
Consumer
```

### 第一阶段不要实现

- 复杂轴承；
- 移动结构；
- 超大型 contraption；
- 无限精确物理。

只解决：

> **动态图上的动力传播。**

### 教程

**Chapter 19：Create 教给我们的不是齿轮，而是 Gameplay Graph**

---

## C5 — Mechanical Topology Debugger

Debug UI：

```text
Network #12
Nodes: 42
Sources: 2
Consumers: 9
RPM: 32
Capacity: 128
Stress: 96
```

World overlay：

```text
节点
边
方向
network id
```

### 教程

**Chapter 20：复杂系统没有可视化就无法维护**

---

## C6 — Item Transport / Belt

不要让每个 Belt Item 都成为完整 Actor。

可以用：

```cpp
struct BeltItem {
    ItemStack stack;
    float normalizedPosition;
};
```

Belt Segment：

```text
Input Port
Lane
Output Port
```

更新：

```text
position += speed * dt
```

进入下一个 segment 时做 ownership transfer。

### 重点

- Backpressure；
- Output blocked；
- Merge；
- Split；
- Chunk unload；
- Save / Load。

### 教程

**Chapter 21：Transport Simulation 与 Entity Simulation 的区别**

---

## C7 — Processing Pipeline

组合：

```text
Ore
 ↓
Drill
 ↓
Belt
 ↓
Crusher
 ↓
Belt
 ↓
Furnace
 ↓
Chest
```

此时要求：

- Machine 不知道 Belt；
- Belt 不知道 Recipe；
- Inventory 通过接口交换；
- Event 用于观察，不承担核心 transfer。

### 教程

**Chapter 22：组合性来自边界，而不是继承层次**

---

## C8 — AE2-style Storage Network

基于同一 Dynamic Graph Framework，新建：

```text
Storage Network
```

Node：

```text
Storage Provider
Cable
Terminal
Controller
Importer
Exporter
```

第一版只做：

```text
Chest
Cable
Terminal
```

Terminal Query：

```text
Iron      1842
Coal       892
Wood      2210
```

### Storage Index

不能每次打开 UI 都遍历所有 Chest。

维护：

```cpp
unordered_map<ItemId, int64_t> networkInventoryIndex;
```

通过 Inventory Changed 更新。

### 正确性

必须保证：

```text
Indexed Amount == Sum(Provider Inventory)
```

加入定期 Debug Verification。

### 教程

**Chapter 23：Index 是性能优化，也是新的 Truth 风险**

---

## C9 — Storage Reservation

为了 Auto Craft，必须引入：

```text
Available
Reserved
Consumed
```

否则多个 Job 会同时认为材料可用。

```cpp
struct Reservation {
    JobId owner;
    ItemId item;
    int amount;
};
```

### 教程

**Chapter 24：资源预订与事务思想**

---

## C10 — Auto Crafting DAG

Request：

```text
100 Iron Pickaxe
```

Planner：

```text
Iron Pickaxe
   ├── Iron Ingot
   │     └── Iron Ore
   └── Stick
         └── Plank
               └── Log
```

步骤：

```text
Recipe Discovery
Cycle Detection
Existing Inventory Subtraction
Reservation
Job DAG
Execution
Completion
```

状态：

```text
Planned
WaitingInput
Ready
Running
Blocked
Completed
Failed
Cancelled
```

### 必须测试

- Recipe cycle；
- 缺材料；
- 中途拆网络；
- Provider unload；
- Save / Reload；
- Job cancel；
- 多 Job 竞争资源。

### 教程

**Chapter 25：Auto Crafting 本质上是依赖规划系统**

---

## C11 — Sandbox Factory Demo

最终 Demo：

```text
Water Wheel
    ↓
Mechanical Network
    ↓
Drill
    ↓
Belt
    ↓
Crusher
    ↓
Storage Network
    ↓
Auto Craft
```

玩家真正搭一个自动工厂。

### Track C 完成条件

- Network 拆/连正确；
- Machine 与 Transport 解耦；
- Storage Index 守恒；
- Auto Craft 可恢复；
- Chunk unload 后网络不崩；
- Save / Load 后继续工作；
- Debug UI 能解释“为什么机器没有工作”。

---

# 10. Track D — Large Scale Simulation

目标：

> 同一个世界里有越来越多机器、Network、Actor、Crop 后，不能依赖“所有对象每 Tick 更新”。

---

## D1 — Simulation Phase Scheduler

固定 Tick：

```text
1 Input Commit
2 World Commands
3 Block Simulation
4 Machine
5 Network
6 Actor AI
7 Combat
8 Population
9 Post Simulation Events
10 Snapshot
```

每一 phase：

```text
Budget
Used
Skipped
Deferred
```

### 教程

**Chapter 26：Tick 顺序就是 Gameplay Contract**

---

## D2 — Activation Level

每个 Spatial Cell：

```text
Full
Reduced
Dormant
```

### Full

- Actor AI；
- Machine；
- Belt；
- Network；
- Crop；
- Combat。

### Reduced

- Machine 低频；
- Crop Batch；
- Network aggregate；
- Actor simple update。

### Dormant

- 不 Tick；
- 只保留持久状态。

### 教程

**Chapter 27：Simulation LOD**

---

## D3 — Machine Reduced Simulation

不要：

```text
10000 Furnace × 20Hz
```

远距离机器可以：

```text
lastUpdateTick
currentTick
elapsed
```

然后：

```text
completedCycles = elapsed / recipeDuration
```

受：

```text
Input
Output Capacity
Energy
```

约束。

### 关键

解析式推进必须和 Full Simulation 结果一致或有明确近似边界。

### 教程

**Chapter 28：时间跳跃比低频 Tick 更高级吗？**

---

## D4 — Network Reduced Simulation

近距离：

```text
逐节点传播 / 调试
```

远距离：

```text
Network Aggregate State
```

例如 Mechanical：

```text
Total Capacity
Total Stress
RPM
```

Storage：

```text
Inventory Index
Pending Jobs
```

避免远处网络每 Tick 遍历全部 Node。

### 教程

**Chapter 29：从 Node Simulation 到 Aggregate Simulation**

---

## D5 — Actor AI LOD

分层：

```text
L0 Full AI
L1 Simplified State Machine
L2 Periodic Update
L3 Dormant / Statistical
```

例如：

### L0

感知、追逐、战斗。

### L1

简单 Wander / Goal。

### L2

每秒/数秒更新一次位置和状态。

### L3

只记录：

```text
alive
region
health
goal
```

### 教程

**Chapter 30：1000 NPC 不应该拥有同样昂贵的大脑**

---

## D6 — World Simulation Cell

Chunk 是存储单位，不一定是 Simulation 调度单位。

可以引入：

```text
Simulation Cell = NxN Chunks
```

负责：

- Activation；
- System Lists；
- Budget；
- Aggregates。

第一版可直接：

```text
1 Cell = 4x4 Chunk
```

值可调整。

### 教程

**Chapter 31：空间分区不只有一种粒度**

---

## D7 — Persistent Runtime State

需要升级 Storage Contract。

新增持久状态：

```text
Machine State
Network Node State
Network Job State
Auto Craft Job
Reduced Simulation Timestamp
```

### 原则

不要存：

```text
可完全重新推导的 transient cache
```

例如：

```text
Network Connected Component
Storage Index
```

如果能稳定从 Node 重建，优先重建。

存真正不可推导的：

```text
Machine Progress
Inventory
Reservations / Job identity
```

### 教程

**Chapter 32：什么应该存盘，什么应该重建**

---

## D8 — Large Scale Debugger

ImGui 新页：

```text
World Runtime
Streaming
Jobs
Simulation
Network
Machines
Storage
LOD
Actors
Persistence
```

重要视图：

### Streaming Timeline

```text
Request → Start → Worker → Commit
```

### Network Inspector

点击一个机器：

```text
Network #42
Reason not running: StressExceeded
```

### Simulation Heatmap

显示：

```text
Full / Reduced / Dormant Cell
```

### 教程

**Chapter 33：Debuggability 是架构的一部分**

---

# 11. 最终 Capstone — Autonomous Outpost

最终不是 Boss Fight，而是：

> **一个玩家建造的自动化前哨基地，在玩家离开、Streaming、LOD、存档、重开之后仍保持正确运行。**

---

## Demo Flow

### 1. 大世界进入

```text
Far Terrain
  ↓
玩家接近
  ↓
Full Chunk Streaming
```

### 2. 建立动力

```text
Water Wheel
 ↓
Shaft
 ↓
Gear
```

### 3. 自动采矿

```text
Mechanical Drill
 ↓
Ore
```

### 4. 自动物流

```text
Belt
 ↓
Crusher
 ↓
Furnace
```

### 5. 仓储

```text
Storage Cable
 ↓
Terminal
```

### 6. Auto Craft

玩家申请：

```text
20 Pickaxe
```

系统自动规划依赖。

### 7. 玩家离开

区域变：

```text
Full
 ↓
Reduced
 ↓
Dormant
```

### 8. 玩家返回

系统恢复：

- Machine progress；
- Inventory；
- Network；
- Craft Job；
- Far Terrain → Full Terrain；
- Actor activation。

### 9. Save / Exit / Reload

状态继续正确。

---

# 12. Capstone 架构图

```text
                         WORLD
                           │
          ┌────────────────┼─────────────────┐
          │                │                 │
       Streaming       Simulation       Persistence
          │                │                 │
    ┌─────┴─────┐     ┌────┴─────┐      Save / Load
    │           │     │          │
 Full Chunk   Far LOD Machine   Actor
                      │          │
                      ▼          ▼
                   Network     AI LOD
                      │
        ┌─────────────┼──────────────┐
        │             │              │
   Mechanical      Transport       Storage
        │             │              │
        └────────── Automation ───────┘
                       │
                    Auto Craft
```

---

# 13. 教程章节总目录

教程按 Track 维护，不按 34 个 Sprint 生成 34 份文件。默认只保留五章；每章使用“问题场景 →
最简实现 → 为什么失败 → 架构演进 → 数据结构 → 验证 → 取舍”的统一结构：

| 章节 | 覆盖内容 | 主要 Milestone |
| ---- | -------- | -------------- |
| 00 从可玩 Sandbox 到 Architecture Lab | 基线、Facade、事件、Tick、所有权和重构安全网。 | A0-A5 |
| 01 有界 Chunk Streaming | Chunk 正交状态、Job Scheduler、优先级、取消、背压和指标。 | B1-B6；B7-B9 仅在真实需求触发时追加 |
| 02 从机器到动态网络图 | Capability、通用机器、拓扑分裂/合并、跨区块和一种实际网络。 | C1-C3 + 首个获批网络 |
| 03 大规模模拟预算 | Tick 相位、空间激活、Actor AI LOD、持久化与调试。 | D1、D2、D5；其余按对象规模触发 |
| 04 可玩载体 Capstone | 完整 Demo、迁移、性能前后对比、失败复盘和总 Trade-off。 | 最终获批 Track |

现有 HelloMine3D 已实现内容只作为证据和失败复盘引用，不重新写一套百科。Far Terrain、完整
机械/传输/存储/自动合成链等未批准能力，只能作为对应章节的候选小节，不能提前创建空文档。

教程统一放在未来的一份 `docs/current/architecture-lab-tutorial.md` 中；只有内容超过可维护边界
且至少两个 Track 已实际完成时，才允许按 Track 拆分。

---

# 14. 教程和开发里程碑映射

每个 Sprint 只更新所属 Track 章节的一小节。Track 退出时再冻结该章的完整问题、演进、数据结构、
验证和取舍；没有实际实现与证据的候选 Sprint 不进入教程正文。

---

# 15. 每个 Sprint 的标准交付物

任何架构 Sprint 必须同时交付：

## Code

- 核心实现；
- 单元测试；
- 集成测试；
- Debug Metrics。

## Contract

例如：

```text
chunk-residency-contract-v1.md
world-job-scheduler-contract-v1.md
mechanical-network-contract-v1.md
storage-network-contract-v1.md
simulation-lod-contract-v1.md
```

## Tutorial

不为每个 Sprint 新建教程文件；更新第 13 节定义的所属 Track 章节，至少解释：

```text
Why
Naive Solution
Failure
Design
Implementation
Validation
Trade-off
```

## Demo

必须可在游戏里被观察到。

## Playable Evidence

- 每个 Sprint 至少定义受影响的 AI 交互或视觉场景；
- 每个 Track 使用干净 Release 包运行一次 `AI-08 full-playable-carrier`；
- 正式场景禁止 fixture、传送、物品注入、存档编辑和直接调用 Gameplay API；
- AI 记录功能可玩性，人类主观体验保持 `NOT_CLAIMED`。

## Performance

至少记录：

```text
Before
After
Scene
Hardware
Build
Metrics
```

---

# 16. Definition of Done

一个 Milestone 只有同时满足以下条件才 Done：

### Correctness

- 自动测试通过；
- Save / Load 正确；
- Chunk unload/reload 正确；
- 边界情况覆盖。

### Architecture

- Ownership 明确；
- Thread ownership 明确；
- 不新增隐式全局依赖；
- Renderer 不反向拥有 Gameplay 状态。

### Performance

- 新系统有 metrics；
- 无无界 Queue；
- 无无界 Cache；
- 无明显 Frame Spike 回归；
- 大规模场景有 stress。

### Debug

- 失败可以解释；
- 不允许只有“机器没动”；
- 必须能回答原因。

### Documentation

- Contract；
- 所属 Track 教程章节；
- Trade-off（写入同一章节，不另建平行文档）；
- Regression 命令。

### Playability

- 新能力必须通过正常游戏路径获得或触发；
- 既有主菜单到胜利、保存和重开链路不能被架构改造破坏；
- Sprint 可在 AI 场景 `NOT_RUN` 时标记 `Engineering Done`，但 Track 只有相关 Computer Use
  记录 PASS 后才能标记 `AI Playability PASS`；
- 不以调试面板、隐藏夹具或截图数量代替真实操作；
- 不声明人类乐趣、审美、舒适度或物理设备手感。

---

# 17. 测试体系

## Unit

适合：

```text
World Coordinates
Graph
Recipe DAG
Reservation
Priority
State Machine
LOD Aggregation
```

## Property Tests

适合：

```text
Network split/merge
Inventory conservation
Generation determinism
Save round-trip
```

例如 Storage：

```text
Sum(Providers) == NetworkIndex
```

Auto Craft：

```text
Initial Items
+ Produced
- Consumed
= Final Items
```

## Integration

```text
Place Machine
Connect
Break
Reconnect
Unload Chunk
Reload
Save
Restart
```

## Stress

```text
10 min movement
30 min factory
Teleport storm
1000 machines
100 networks
100 actors
```

数量作为测试场景目标，不代表所有对象必须 Full Tick。

---

# 18. 性能策略

不要一开始为所有新系统拍脑袋规定绝对毫秒。

第一阶段采用：

> **Baseline + Regression Budget**

流程：

```text
Before Feature
 ↓
Capture Baseline
 ↓
Feature
 ↓
Same Scene
 ↓
Compare
```

每个子系统必须有：

```text
count
time
queue
budget
denied/deferred
```

例如：

```text
Mechanical
  Networks             28
  Active Nodes        612
  Topology Rebuilds     3
  Solve P95          0.21 ms
```

Streaming：

```text
Pending       42
InFlight       4
Cancelled    182
Stale          9
Commit P95  0.17 ms
```

Simulation：

```text
Full Cells       9
Reduced Cells   40
Dormant Cells  152
Deferred Jobs    7
```

---

# 19. 推荐源码目录演进

不要一次移动全部代码。

目标目录可以逐渐形成：

```text
src/HelloMine3D/

World/
├── Block/
├── Chunk/
├── Generation/
├── Light/
├── Streaming/
│   ├── ChunkDemand.*
│   ├── SpatialInterest.*
│   ├── WorldJobScheduler.*
│   └── FarTerrain.*
├── Simulation/
│   ├── WorldSimulation.*
│   ├── SimulationCell.*
│   └── SimulationBudget.*
├── Network/
│   ├── NetworkGraph.*
│   ├── Mechanical/
│   ├── Transport/
│   └── Storage/
├── Machine/
├── Storage/
└── World.*

Gameplay/
├── Crafting/
├── Objectives/
└── Automation/
```

这里是目标，不是 A0 就执行的目录重排。

---

# 20. Mod Case Study 使用方式

只在首个相关 Track 实际批准后新增一份合并研究：

```text
docs/reports/architecture-lab-case-studies.md
```

Create、AE2、Distant Horizons、Mekanism 和 Ars Nouveau 分别作为其中的二级章节，不按 Mod
拆成五份文件。

统一模板：

1. Mod 要解决什么问题；
2. 它受 Minecraft API 哪些约束；
3. 哪些是 Minecraft 特有；
4. 哪些是通用架构思想；
5. HelloMine3D 的需求是什么；
6. HelloMine3D 独立设计；
7. Benchmark；
8. Trade-off。

原则：

> **Concept Port，不做 Code Port。**

---

# 21. 第三方代码和素材边界

为 Architecture Lab 单独维护一份来源记录：

```text
docs/reports/architecture-lab-provenance.md
```

记录：

```text
Project
Repository
License
Studied Component
Copied Code? No
Copied Asset? No
Notes
```

默认策略：

- 可以阅读源码；
- 可以学习算法和架构；
- 自己重新设计接口和实现；
- 不复制纹理；
- 不复制模型；
- 不复制音频；
- 不逐行 Java→C++ 翻译；
- 引用论文 / 官方文档 / 项目链接。

---

# 22. Git 开发策略

建议每个 Milestone 一个 branch：

```text
arch/a1-world-boundary
arch/b3-world-job-scheduler
arch/c4-mechanical-network
```

Commit 尽量按演进过程保留：

```text
1 baseline
2 naive implementation
3 benchmark / test exposing problem
4 architecture change
5 final validation
```

这对教程非常有价值。

因为教程能引用：

> “这个 commit 是为什么失败的”。

---

# 23. Release / Tag 策略

不占用现有产品版本编号。

建议：

```text
arch-a0-baseline
arch-a-complete
arch-b-streaming
arch-b-large-world
arch-c-automation
arch-d-simulation
sandbox-architecture-v1
```

最终：

```text
sandbox-architecture-v1
```

作为教程第一版的冻结代码。

---

# 24. 推荐 Sprint 顺序

## Semester A

```text
Sprint 01 A0 Baseline
Sprint 02 A1 Responsibility Map
Sprint 03 A2 Chunk Runtime
Sprint 04 A3 Simulation Runtime
Sprint 05 A4 Event / Command / Query
Sprint 06 A5-A6 Budget + Tutorial Pipeline
```

## Semester B

```text
Sprint 07 B1 Chunk State
Sprint 08 B2 Demand Model
Sprint 09 B3 Scheduler I
Sprint 10 B3 Scheduler II
Sprint 11 B4 Cancellation
Sprint 12 B5 Backpressure
Sprint 13 B6 Spatial Activation
Sprint 14 B7 Render/Simulation Split
Sprint 15 B8 Far Terrain Model
Sprint 16 B9-B10 Far Render + Stress
```

## Semester C

```text
Sprint 17 C1 Capability
Sprint 18 C2 Machine
Sprint 19 C3 Network Graph I
Sprint 20 C3 Network Graph II
Sprint 21 C4 Mechanical
Sprint 22 C5 Debug
Sprint 23 C6 Belt
Sprint 24 C7 Processing
Sprint 25 C8-C9 Storage
Sprint 26 C10-C11 Auto Craft + Demo
```

## Semester D

```text
Sprint 27 D1 Tick Scheduler
Sprint 28 D2 Activation
Sprint 29 D3 Machine Reduced Simulation
Sprint 30 D4 Network Reduced Simulation
Sprint 31 D5 Actor AI LOD
Sprint 32 D6 Simulation Cell
Sprint 33 D7-D8 Persistence + Debugger
Sprint 34 Capstone
```

---

# 25. 每个 Semester 的退出标准

## A Exit

> 我们能够解释 HelloMine3D 各 Runtime 系统由谁拥有、在哪个线程修改、如何 Tick、如何发布事件。

并且既有主菜单→胜利→保存重开流程在干净 Release 包中仍可由 AI 正常完成。

## B Exit

> 玩家可以持续移动穿越世界，而 Streaming Queue、Worker、Mesh、LOD、Unload 都在明确预算下稳定工作。

AI 必须通过正常移动完成长距离流送场景；传送风暴只作为补充压力场景，不能替代真实移动。

## C Exit

> 玩家可以搭建一个由动力、传输、机器和仓储组成的自动化工厂，而不是开发者写死一条脚本。

AI 必须实际放置、挖断、重连、卸载/返回并保存重开，证明网络分裂/合并和状态恢复可被正常玩法观察。

## D Exit

> 玩家离开工厂后系统能够降级模拟；返回、保存、重开之后仍保持逻辑正确。

AI 必须从正常游戏入口完成离开、返回和重开流程；Debug Metrics 只解释结果，不负责制造结果。

---

# 26. 项目真正要展示的能力

完成后，HelloMine3D 的核心卖点不应再是：

```text
Minecraft clone written in C++
```

而应变成：

```text
A C++ voxel sandbox architecture laboratory demonstrating:

- deterministic procedural worlds
- chunk streaming
- asynchronous world jobs
- LOD terrain
- spatial simulation
- dynamic gameplay graphs
- automation networks
- storage indexing
- auto-crafting dependency planning
- simulation LOD
- persistence and migration
- performance engineering
- debugging complex runtime systems
```

---

# 27. 最重要的开发原则

## 原则 1

**不要为了教程推翻现有工程。**

教程最有价值的恰恰是：

> 现有实现为什么需要演进。

---

## 原则 2

**每次抽象必须有真实需求。**

不要先写：

```text
INetwork
ISystem
IService
IProcessor
```

然后寻找用途。

应该：

```text
Mechanical Network
 ↓
Storage Network
 ↓
发现共同结构
 ↓
抽 NetworkGraph
```

---

## 原则 3

**保持 Authoritative State 单一。**

特别警惕：

```text
World State
Render State
Network Cache
Storage Index
LOD Cache
```

这些 Derived State 一定要明确：

```text
如何生成
何时 dirty
如何验证
什么时候丢弃
```

---

## 原则 4

**大世界系统的第一敌人是无界工作量。**

任何 Queue、Cache、Tick、搜索都要问：

```text
最坏情况是多少？
谁限制？
超过预算怎么办？
能否延迟？
能否取消？
```

---

## 原则 5

**Debuggability 与正确性同等重要。**

一个机械系统如果只能看到：

```text
机器没工作
```

就是不完整的系统。

必须能回答：

```text
No Input
Output Blocked
No Power
Disconnected
Stress Exceeded
Chunk Inactive
Job Deferred
```

---

# 28. 第一阶段立即执行清单

新的 Architecture Lab 正式启动时，只做下面 6 件事：

### 1

冻结当前 VISUAL-RC 架构 / 性能基线。

### 2

生成 `World Responsibility Map`。

### 3

给现有 World Fixed Tick 做 phase timing。

### 4

把 Chunk Update / Mesh Work 调度提取成 ChunkRuntime。

### 5

定义 Chunk Residency / Mesh / Render 三套正交状态。

### 6

写出教程 Chapter 00：

> **为什么一个能玩的 Minecraft Clone 最终会需要沙盒架构。**

暂时不要开始：

```text
Create
AE2
Far Terrain
```

等 A Track 结束之后再进入大世界和网络系统。

---

# 29. 第一版 Architecture Lab 的成功标准

如果未来只判断这个项目是否成功，我建议看下面五条，而不是代码量：

### 1. 可解释

能画出完整 World Runtime 图，并解释所有 ownership。

### 2. 可扩展

新增 Machine / Network 类型不用改 World 核心。

### 3. 可缩放

把世界扩大、机器增加后，不是线性增加所有对象 Tick。

### 4. 可恢复

Streaming、Save、Unload、Reload 后状态正确。

### 5. 可教学

别人可以沿着 commit + 文档理解：

> **为什么系统从简单版本一步一步变成现在的架构。**

---

# 30. 最终结论

HelloMine3D 接下来的真正主线可以浓缩成：

```text
当前可玩的 Voxel Sandbox
        ↓
World Runtime 重构
        ↓
Streaming State Machine
        ↓
Job Scheduler / Backpressure
        ↓
Spatial Activation
        ↓
Far Terrain / LOD
        ↓
Generic Network Graph
        ↓
Mechanical Automation
        ↓
Storage / Auto Craft
        ↓
Simulation LOD
        ↓
Autonomous Outpost
```

最终形成两根最清晰的技术支柱：

```text
        HelloMine3D

    ┌───────────────┐
    │               │
Large World     Sandbox Systems
    │               │
Streaming       Dynamic Graph
LOD             Machine
Jobs            Automation
Activation      Storage
    │               │
    └───────┬───────┘
            │
     Large Scale Simulation
```

这就是整套教程、项目迭代以及技术作品展示应该围绕的核心。
