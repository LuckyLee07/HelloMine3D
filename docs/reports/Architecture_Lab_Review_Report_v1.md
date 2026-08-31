# HelloMine3D Architecture Lab 文档评审与优化报告

## 1. 评审结论

本次评审基于以下三份现行文档：

- `docs/current/todolist.md`
- `docs/current/architecture-lab-roadmap-v1.md`
- `docs/README.md`

整体判断：当前版本已经从“技术路线图”升级为“长期工程治理体系”。最重要的提升，不是增加了更多系统，而是建立了四层文档治理、唯一任务账本、候选能力与已批准任务分离、工程证据与可玩证据分离，以及“真实玩法需求驱动架构”的约束。

当前体系已经具备长期维护价值，但仍存在几处需要修正的结构性问题：

1. A0 基线仍引用旧的 `VISUAL-RC`，与当前 `PLAYABILITY-RC / Stage 11 P11F` 基线不一致。
2. Track C 中 `Generic Network Graph` 抽象出现过早，与文档自己的“先有真实系统，再抽象”原则冲突。
3. A5 中 `ISandboxSystem` 也有类似的过早抽象风险。
4. A2 与 B1 在 Chunk Residency / State Machine 上存在职责重叠。
5. Track B/C/D 的“Optional 能力”与 Track Exit 条件存在不一致。
6. `todolist.md` 已定义为唯一当前任务账本，但仍保留过多 Stage 9~11 历史细节。
7. 教程从 34 章压缩到 5 章后，文件治理更好，但教学颗粒度略显过粗。
8. AI Playability 状态已经基本正交化，但仍建议进一步明确为独立验收维度。

推荐下一版目标：保留当前文档治理框架；修正上述逻辑冲突；将 Track 设计改成“Core 能力 + Extended 能力”；让抽象真正由两个以上真实系统共同逼出。

---

## 2. 当前版本已经可以定型的设计

### 2.1 唯一当前任务账本

`docs/current/todolist.md` 只负责当前状态、下一批准批次、阻塞和验证状态。Architecture Lab roadmap 只承担长期能力目录。

推荐长期保持：

```text
Architecture Roadmap
        ↓
长期能力候选池

current/todolist.md
        ↓
当前批准任务

真实问题 / 新需求
        ↓
从 Roadmap 选择下一能力
```

不要让 roadmap 中的 Sprint 自动变成 backlog。

### 2.2 文档四层治理

```text
docs/current/
docs/contracts/
docs/reports/
docs/archive/
```

- `current/`：当前状态、当前架构、当前验证规则和长期方向。
- `contracts/`：冻结数据语义、失败边界、迁移和 Exit Contract。
- `reports/`：RC、性能证据、调查、AI/视觉记录和一次性检查点。
- `archive/`：被替代路线、历史协议、旧总账和旧验收规则。

这套结构建议直接定型。

### 2.3 Capability Map ≠ Backlog

34 个 Sprint 只是能力目录，不是连续开发承诺。后续所有 Track 都应遵守：

```text
真实问题
   ↓
失败边界
   ↓
游戏内可观察需求
   ↓
批准 Sprint
```

### 2.4 Architecture Lab 必须依附真实游戏

Architecture Lab 不应退化成 NetworkGraph Demo、Streaming Demo 或 LOD Demo，而应始终是：

```text
真实游戏
   ↓
出现系统需求
   ↓
简单实现
   ↓
出现规模 / 维护 / 正确性问题
   ↓
架构演进
```

---

## 3. 当前最重要的结构性问题

### 3.1 A0 基线已经过期

Roadmap 第一阶段仍写“冻结当前 VISUAL-RC 架构 / 性能基线”，但当前实际项目基线已经推进到 Stage 11 / P11F / PLAYABILITY-RC。

建议改为：

> 冻结当前 PLAYABILITY-RC / Stage 11 P11F 的架构、功能与性能基线。

A0 应记录：PLAYABILITY-RC identity、World public API、World responsibilities、Chunk ownership、Thread ownership、Tick order、Save ownership、Render snapshot、Performance baseline、AI playability state。

---

## 4. Track C：当前最大的架构逻辑冲突

### 4.1 当前顺序的问题

当前路线：

```text
C1 Capability
C2 Generic Machine
C3 Generic Network Graph
C4 Mechanical Network
...
C8 Storage Network
```

问题在于 `Generic Network Graph` 在真正出现两个网络系统之前已经被设计出来，而 roadmap 自己又明确主张：

```text
Mechanical Network
 ↓
Storage Network
 ↓
发现共同结构
 ↓
抽 NetworkGraph
```

### 4.2 推荐改成“实例先于抽象”

```text
Machine Runtime
      ↓
Mechanical Network v0
      ↓
Mechanical Debugger
      ↓
Storage Network v0
      ↓
发现重复
      ↓
Extract Dynamic Network Core
```

Mechanical v0 可以先拥有 `MechanicalNode / MechanicalConnection / MechanicalComponent`；Storage v0 再拥有自己的结构。当两个系统共同出现 Node、Edge、Component、Merge、Split、TopologyDirty 后，再提炼 `DynamicNetworkCore`。

这会把“抽象不是预先想出来的，而是两个真实系统共同逼出来的”变成整个课程最有价值的教学段落之一。

---

## 5. A5：不要过早创建 ISandboxSystem

当前 A5 示例中的 `ISandboxSystem` 有提前抽象风险。当前项目真正需要先解决的是 Simulation 的顺序、耗时、预算、延迟和可观察性，而不是统一继承体系。

建议把 A5 从：

> System Registry & Tick Budget Foundation

改成：

> Tick Phase / Metrics / Budget Vocabulary

第一阶段只建立：

```text
Simulation
  Block Tick      0.18 ms
  Actor           0.32 ms
  Combat          0.06 ms
  Population      0.11 ms
```

以及类似：

```cpp
struct SimulationPhaseMetrics {
    Duration elapsed;
    size_t processed;
    size_t deferred;
};
```

等未来 Machine、Network、AI、Transport 等真实系统出现并需要统一调度时，再决定是否需要 `SimulationScheduler / ISandboxSystem`。

---

## 6. A2 与 B1 的职责需要重新划线

推荐：

### Track A 只做现有行为重构

```text
World
  ↓
ChunkUpdateQueue
Mesh planning
Loader coordination

迁移到

ChunkRuntime
```

A2 Exit：旧功能行为完全相同、ownership 更清晰、World 减少职责、不引入新的 Chunk 生命周期。

### Track B 才改变 Runtime 行为

B1 正式引入：

```text
Data Residency
Mesh State
Render State
```

一句话概括：

```text
A = Refactor existing behavior
B = Introduce new runtime behavior
```

---

## 7. 推荐引入 Core / Extended Track

当前 Optional 能力与 Track Exit 条件存在冲突，建议所有主要 Track 分成 Core 与 Extended。

### Track B-Core — Bounded Streaming

```text
B1 Chunk State
B2 Demand Model
B3 Job Scheduler
B4 Cancellation
B5 Backpressure
B6 Spatial Activation
```

Exit：Chunk Streaming 在有界 Queue、明确 Priority、Cancellation 和 Activation 下稳定工作。

### Track B-Extended

只有当完整 Chunk Rendering 成为真实视距 / 内存 / GPU 瓶颈后，再启动：

```text
Render / Simulation Split
Far Terrain
LOD
Far Cache
```

### Track C-Core — Emergent Gameplay

```text
Machine Runtime
Mechanical Network
Topology Split / Merge
Mechanical Debugger
```

Exit：玩家能真正搭建、拆除和重连动态机械网络。

### Track C-Extended

按真实玩法需求启动：

```text
Storage
Transport
Processing
Reservation
Auto Craft
```

### Track D-Core — Scale

先做：

```text
Tick Phase
Activation
AI LOD
```

只有规模问题出现后，再批准：

```text
Machine Reduced Simulation
Network Aggregate Simulation
Simulation Cell
```

---

## 8. 教程建议：5 个物理文档 + 30~40 个 Section

当前把 34 章压缩为 5 章，从文档维护角度是正确的；但教学颗粒度不应真的只剩 5 个知识点。

建议：

### Part 00 — 从可玩 Sandbox 到 Architecture Lab

```text
0.1 World 为什么变成 God Object
0.2 Authoritative State
0.3 Derived State
0.4 Query / Command / Event
0.5 Fixed Tick
0.6 Runtime Ownership
0.7 Refactor Safety Net
```

### Part 01 — Bounded World Streaming

```text
1.1 Chunk Residency
1.2 Mesh State
1.3 Render State
1.4 Streaming Demand
1.5 Priority
1.6 Async Job
1.7 Cancellation
1.8 Revision Token
1.9 Backpressure
1.10 Spatial Activation
1.11 Optional Far Representation
```

### Part 02 — Emergent Sandbox Systems

```text
2.1 Machine Runtime
2.2 Mechanical Network
2.3 Graph Split / Merge
2.4 Topology Dirty
2.5 Mechanical Debugger
2.6 Storage Network
2.7 Extract Generic Network Core
2.8 Transport
2.9 Reservation
2.10 Auto Craft DAG
```

### Part 03 — Large Scale Simulation

```text
3.1 Tick Phase
3.2 Budget
3.3 Activation
3.4 AI LOD
3.5 Reduced Simulation
3.6 Aggregate Simulation
3.7 Persistence
3.8 Debuggability
```

### Part 04 — Capstone

```text
4.1 Integrated Architecture
4.2 Failure Review
4.3 Performance Before / After
4.4 Save Migration
4.5 Final Trade-offs
```

---

## 9. todolist.md 需要继续瘦身

`current/todolist.md` 最好真正做到：打开以后 30 秒内知道今天该做什么。

推荐未来只保留：

1. Project Goal
2. Current Baseline
3. Current Approved Batch
4. Queued（最多 1~2 个）
5. Current Blockers
6. Pending Verification
7. Historical Summary

详细 Stage 9~11 历史应只保留摘要，并引用 archive / reports / contracts。

---

## 10. 状态建议改成三个正交维度

### Engineering

```text
TODO
DOING
DONE
```

### AI Playability

```text
NOT_RUN
PASS
FAIL
BLOCKED
```

### Human Experience

```text
NOT_CLAIMED
```

于是一个任务可以明确表示：

```text
Engineering: DONE
AI Playability: NOT_RUN
Human Experience: NOT_CLAIMED
```

AI Playability 应是独立验收维度，而不是架构依赖。Computer Use 环境不可用时，可以记录 `NOT_RUN / BLOCKED` 后继续工程开发，但不能把上一 Track 写成 AI PASS。

---

## 11. 推荐后的总体 Architecture Lab 结构

### Track A — Architecture Safety

```text
A0 Latest Baseline
A1 Responsibility Map
A2 ChunkRuntime Extraction
A3 WorldSimulation Extraction
A4 Command / Query / Event
A5 Phase Metrics & Budget Vocabulary
A6 Tutorial / Documentation
```

### Track B-Core — Bounded Streaming

```text
B1 Chunk State
B2 Demand
B3 Scheduler
B4 Cancellation
B5 Backpressure
B6 Spatial Activation
```

### Track B-Extended

触发后再做：

```text
Render / Simulation Split
Far Terrain
LOD
```

### Track C-Core — Emergent Gameplay

```text
Machine Runtime
      ↓
Mechanical Network
      ↓
Topology Debug
      ↓
Storage Network
      ↓
Extract Shared Network Core
```

### Track C-Extended

```text
Belt
Transport
Processing
Reservation
Auto Craft
```

### Track D-Core — Scale

```text
Tick Phase
Activation
AI LOD
```

### Track D-Extended

```text
Machine Reduced Simulation
Network Aggregation
Simulation Cell
Advanced Persistence
```

---

## 12. 推荐立即修改清单

### P0

1. `VISUAL-RC` → `PLAYABILITY-RC / Stage 11 P11F`。
2. Track C 中 Generic Network Graph 后移到 Mechanical + Storage 之后。
3. A5 删除或降级 `ISandboxSystem`，改为 Phase / Metrics / Budget Vocabulary。
4. 明确 A2 = refactor，B1 = new lifecycle。

### P1

5. 引入 Core / Extended，解决 Optional 与 Exit 条件冲突。
6. 瘦身 `todolist.md`，只保留 Current / Next / Blocker / Verification。
7. 状态正式拆成 Engineering / AI Playability / Human Experience 三维。

### P2

8. 教程采用 5 个物理文档 + 30~40 个概念 Section。

---

## 13. 最终评价

当前版本已经从“一套完整而漂亮的技术课程蓝图”，升级成“一套能够长期运行的个人大型游戏工程研发治理体系”。

最重要的三个进步：

```text
Capability Map ≠ Backlog

Engineering Evidence ≠ Playability Evidence

Architecture Lab ≠ Engine Demo
```

下一步最值得强化的思想：

```text
Concrete System
      ↓
Concrete System
      ↓
Repeated Problem
      ↓
Abstraction
```

而不是：

```text
Future Requirement
      ↓
Guess Abstraction
      ↓
Build Generic Framework
```

如果完成上述修正，Architecture Lab 的整体路线会从“规划很全面”进一步提升到：

> 架构演进逻辑本身就是教程内容。

这会成为 HelloMine3D 最有辨识度的地方。

---

## 14. 推荐近期启动顺序

如果 Architecture Lab 下一批正式获批，只批准：

```text
AL-A0
Latest Architecture Baseline
```

完成后再批准：

```text
AL-A1
World Responsibility Map
```

然后：

```text
AL-A2
ChunkRuntime Extraction
```

不要一次批准完整 Track A，更不要现在就启动 Create / AE2 / Far Terrain。

近期真正需要形成的是：

```text
PLAYABILITY-RC
      ↓
Architecture Baseline
      ↓
World Responsibility Map
      ↓
Safe Runtime Extraction
```

当这条链路完成，Architecture Lab 才算真正正式启动。
