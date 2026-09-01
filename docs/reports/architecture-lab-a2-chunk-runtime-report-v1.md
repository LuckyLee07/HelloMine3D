# HelloMine3D AL-A2 Chunk Runtime Boundary Report v1

日期：2026-09-01

批次：`AL-A2 — Chunk Runtime Boundary`

工作流状态：`Done`

本报告记录既有 Chunk runtime 协调的责任迁移及其自动验证。它不声明新的玩法、性能、AI
可玩性或人类主观体验结论。

## 1. Implementation Result

`World` 继续作为对外 facade、组合根和共享锁所有者，但不再直接拥有 Chunk section 更新队列、
loader worker、mesh 优先级快照、load-center revision 或 unload scan。新增的 `ChunkRuntime` 持有
`ChunkManager` 与 world mutex 的非拥有引用，并集中管理这些派生工作与线程协调。

责任链为：

```text
World facade / composition root
  -> ChunkRuntime: queue, planning, worker, preload/unload coordination,
                   copied mesh publication
     -> ChunkManager: authoritative Chunk/block/light/dirty/storage state
```

所有原调用者继续经过相同的 78 项 `World` 公开 API；没有添加 facade wrapper，也没有要求
`SandboxRuntime`、`WorldManager` 或 Ogre 调用者迁移。

## 2. Preserved Protocols and Budgets

- 单次 `World::update` 的同步 mesh rebuild 预算仍为 2 个 section；队列仍为去重 FIFO。
- 单 worker pass 仍受 6 ms、64 targets、每 target 最多加载 1 Chunk 限制；active/idle sleep 仍为
  1/10 ms。
- distant unload 仍限制为每次 update 最多 8 Chunks。
- mesh 工作仍遵循 `beginMeshJob -> off-lock build -> finishMeshJob`，提交前验证 block revision。
- frustum、距离与坐标排序只改变优先级，不过滤工作目标。
- unload 前保存、save v12、terrain identity、资源、配方和 Gameplay 均未改变。

本批没有引入 `ChunkResidency`、生命周期状态机、通用 Job Scheduler、取消令牌、背压、AL-A3
Simulation Runtime 或 B1 实现。

## 3. Static Boundary Evidence

| Gate | Result |
| ---- | ------ |
| `tools\validate_world_responsibility_map.ps1` | `PASS`；78 methods，分类集合不变，public-surface SHA-256 `3C53F56C425F0395354C8A5CE966E96CDA8BC93D836699955E03BA965A664AD8`。 |
| `tools\validate_chunk_runtime_boundary.ps1` | `PASS`；World legacy queue fields `0`，runtime queue fields `2`，loader workers `1`，mesh protocol `begin-build-finish`。 |
| Source scope audit | `PASS`；没有 `ChunkResidency` 或 AL-A3 实现，World/ChunkRuntime/ChunkManager 责任与合同一致。 |
| `git diff --check` | `PASS`；仅报告仓库既有的 LF/CRLF checkout 提示，无 whitespace error。 |

上述两个 validator 已接入 `scripts\verify_build.ps1`，后续完整 Windows 门禁会继续阻止公开面或
Chunk runtime 责任回退。

## 4. Behavioral Regression Evidence

真实 `HelloMine3DWorldRuntimeSmoke` 在 Debug 与 Release 各通过 `832/832`。其中与本批直接相关的
断言包括：

| Assertions | Result and meaning |
| ---------- | ------------------ |
| `S0.5/*` | `PASS`；单 block/light 编辑只 dirty 必要的已加载 section。 |
| `M2/*` | `PASS`；去重 FIFO、每帧 2 section 预算、完整 drain 与空队列 no-op 保持。 |
| `V5/*` | `PASS`；单后台 loader 在 load-center churn 下安全并持续推进 mesh 工作。 |
| `M6/*`, `M7/*` | `PASS`；snapshot/off-lock 工作量和 frustum priority 语义保持。 |
| `E5/*` | `PASS`；stale upload acknowledgement 不会提升较新的 block revision。 |
| `S2.4/*` | `PASS`；dirty Chunk unload 前保存，reload 后数据存在且不再 dirty。 |

Debug/Release short soak 也均以 `failures=0` 结束。

## 5. Full Windows Gate and Artifact Identity

执行命令：

```powershell
& .\scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
```

| Evidence | Result |
| -------- | ------ |
| Toolchain | Visual Studio 2017 / v141 / x64 Debug + Release |
| Full solution builds | `PASS`；两种配置均无编译错误；既有 Ogre/第三方兼容警告不改变门禁结果。 |
| Headless suite | `PASS`；所有 gate targets 在 Debug/Release 通过。 |
| World / resource / recipe | `832/832` twice；`80/80`；`122/122` |
| Startup negative diagnostics | `15/15` |
| Resource identity | 84 manifest entries；278 atlas checks；38 performance fixtures；11 Stage 10 supplement checks |
| Crash/package boundary | `PASS`；validation-only crash diagnostics 和 isolated clean-root package 通过。 |
| Release executable | 8,754,176 bytes；SHA-256 `5020885CAD249914A21ED6DA26E4E7D8967760CB7ACA7D6C3F2C86759ACC0C59` |
| Verification package | 104 entries；SHA-256 `D1320264A105671C711E1CEF90D9F18382DFA1FB8D59D806C1F72DF80E4802AD` |

该 ZIP 是本地 AL-A2 验证产物，不替换历史 PLAYABILITY-RC 发行身份，不创建 tag，也不 push。
本批保持既有预算常量并复用冻结 Q1/Q3 基线；没有重跑正式硬件性能采集，也没有申请性能例外。

## 6. Evidence Boundary

为避免再次抢占项目所有者当前窗口，本次完整门禁显式使用 `-SkipRealWindow`。因此：

| Dimension | Result | Declaration |
| --------- | ------ | ----------- |
| Automated engineering | `PASS` | AL-A2 ownership extraction and preserved automated behavior are claimed. |
| Gate real-window launch | `DEFERRED` | No real-window PASS is claimed by this run. |
| `AI-01..AI-08` | `NOT_RUN` | No AI playability or AI-understandability PASS is claimed. |
| Human fun/aesthetics/comfort/input feel | not executed | `NOT_CLAIMED` |

Hidden/validation-only Ogre startup、headless smoke 和干净包启动边界都不是 Computer Use 验收，不能
替代 `docs/current/ai-assisted-gameplay-acceptance-v1.md`。

## 7. Exit and Next Approval Gate

AL-A2 合同的五项退出条件均已满足，合同状态冻结为 `Frozen after AL-A2 verification`。本报告所在
本地 commit 是 AL-A2 的提交身份，可用
`git log -1 -- docs/reports/architecture-lab-a2-chunk-runtime-report-v1.md` 解析。

下一候选仅为 `AL-A3 — Simulation Runtime`，状态仍是 `Queued`。AL-A2 完成不构成 AL-A3、B1
或任何 Extended 能力的批准；开始下一批前必须再次获得项目所有者独立批准。
