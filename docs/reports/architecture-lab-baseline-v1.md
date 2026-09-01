# HelloMine3D Architecture Lab Baseline v1

日期：2026-09-01

批次：`AL-A0 — Latest Architecture Baseline`

工作流状态：`Done`

范围：文档审计与验证；无受跟踪 Gameplay、runtime、resource、build input 或存档格式变更。完整
门禁会重建本机 EXE/PDB/ZIP，因 PDB GUID 与归档时间戳产生新的本地构建产物 identity；这不是
运行时代码身份或 PLAYABILITY-RC 历史证据的改写。

## 1. Baseline Identity

| Identity | Value | Meaning |
| -------- | ----- | ------- |
| A0 audit-start commit | `4930023fb2f3022daac9968c10a1a0b76e1ac392` | 开始 A0 文档修改前的仓库 HEAD。 |
| Frozen runtime code commit | `320e293c2f1db7f46aba776ddccdcf94369f2d05` | P11E 最终 Gameplay/render runtime；P11F 没有修改客户端玩法或渲染源码。 |
| PLAYABILITY-RC evidence commit | `93f834ac3e28daa25c78a6c701ceb236cf4d0baa` | P11F 构建、采集、打包与封板证据提交。 |
| AL-A0 documentation commit | 本报告所在的本地 commit | Git commit 不能可靠地在自身内容中嵌入自己的 SHA；以 `git log -1 -- docs/reports/architecture-lab-baseline-v1.md` 解析。 |
| Windows toolchain | Visual Studio 2017 / v141 / x64 Debug + Release | 当前正式 Windows 工程身份。 |
| Frozen PLAYABILITY-RC executable | `8,752,640` bytes; SHA-256 `2B6B824636D3D3B6E442231CFA0114D7A181298AFE06B546789B72546B317347` | P11F 报告冻结的历史发行候选身份；A0 开始时本机文件与之相符。 |
| Frozen PLAYABILITY-RC package | SHA-256 `422F97E87046D4B6D5FC4BB99C37886FF37C4461A152C1162FA66A972B12F459` | P11F 历史 104 项干净包；原报告与其结论不回写。 |
| A0 gate executable | `8,752,640` bytes; SHA-256 `7934F773724228AE732A59814AFA0CD61C9136C30BD6302B747781F19000B220` | 同一 runtime source 在 2026-09-01 完整门禁中重新链接的本机 Release EXE。 |
| A0 gate package | SHA-256 `0E18AAA9DC45C1663CBC5CAC6E83DB1995EA65DA47B16B546A2310C75979A6FB` | 本次验证生成的 104 项 clean-root package；resource manifest 仍为 84 项。它是验证产物，不替换或重新发布 PLAYABILITY-RC。 |

A0 不创建 tag、不 push、不重新发布包，也不把文档 commit 冒充运行时代码身份。

## 2. Frozen Data and Content Identities

| Domain | Version / count | Code or evidence source |
| ------ | --------------- | ----------------------- |
| World save | v12 | `WorldSaveFormatVersion` |
| Terrain generation | v4 | `CurrentTerrainGenerationVersion` / Mountain terrain |
| Runtime settings | v8 | `RuntimeSettingsFormatVersion` |
| Objective definitions | v3 | `ObjectiveSaveState::CurrentDefinitionVersion`; `media/objectives/Base.objective` |
| Enemy definitions | v3 | `media/enemies/Base.enemy` |
| Exploration reward | v1 | `ExplorationRewards::CurrentVersion` |
| Difficulty profile | v1 | `CurrentDifficultyProfileVersion` |
| Post-victory event | v1 | `PostVictoryEvents::CurrentVersion` |
| Audio definitions | v3 | `AudioDefinitionRegistry::SupportedFormatVersion` |
| Music definitions | v1 | `MusicDefinitionRegistry::SupportedFormatVersion` |
| Resource manifest | 84 entries | P11F package/startup gate |
| Localized catalogues | 411 keys each | P11F bilingual resource gate |

这些身份互相正交。world save v12 不允许静默升级旧 world 的 terrain version，也不代表目标、敌人、
设置或音频共享同一个版本号。

## 3. Architecture Audit Result

完整模块、World API、成员、线程、保存、事件、tick 与 snapshot 责任图已冻结在
`docs/current/architecture.md`。A0 对实际源码得到以下结论：

- 全部 17 个第一方顶层目录和 5 个根级配置/输入源文件均已记录；容易漏掉的 `Audio/`、
  `Presentation/`、`Sandbox/`、`Actor/`、`Feedback/` 已纳入同一责任表。
- `World` 当前同时是 facade、组合根和多系统实现；公开 API 横跨 Query、Mutation、Tick、
  Streaming、Persistence、Actor/Combat、Progression 和 Diagnostics。
- `World` 拥有 `ChunkManager`、`ActorManager`、`PlayerActor`、每世界 EventBus、WorldSave、
  WorldBackup 和多套玩法状态；`SandboxRuntime` 拥有 Player 和 fixed-tick 编排。
- Chunk 后台路径已存在 `snapshot -> off-lock build -> revision validation -> commit`，但没有 B1 的
  三套正式状态机、B3 通用 scheduler、B4 cancellation token 或 B5 backpressure contract。
- Ogre 只拥有 GPU/UI/scene mirror，Simulation 层没有 Ogre 类型；Actor、projectile、terrain mesh、
  debug 和 feedback 均通过 copied snapshot/value 边界发布。
- 当前线程只有主/Ogre、单个 Chunk loader、音乐流 worker 和预创建 crash writer 这几类生产线程；
  测试线程不计入 runtime ownership。

## 4. Current Ownership and Dependency Risks

| Fact | A0 interpretation | Future gate |
| ---- | ----------------- | ----------- |
| `World <-> Actor` 双向协作 | World 拥有 ActorManager；Actor tick 接收 `World&`。这是现状，不伪装成无环。 | AL-A1 只画责任图；没有批准不得重写。 |
| Event protocol 位于 Sandbox、bus 实例位于 World | typed events 是同步事实分发；另有 `IWorldEvent` 帧队列。 | AL-A4 才能冻结 Command/Query/Event 规则。 |
| Player 与 PlayerActor 分担状态 | Player 拥有运动/库存/UI；PlayerActor/World 拥有战斗生命并在保存时覆盖 health。 | 任何迁移必须保持 v12 round-trip。 |
| World 同时维护 loader/mesh/random tick/combat/progression | 当前边界可工作但扩张风险高。 | AL-A1/A2/A3 必须逐批独立批准。 |
| Renderer snapshot boundary 已存在 | B7 的目标一部分已由现状满足，但路线图仍是 Extended 候选。 | 不把既有 snapshot 夸大成完整 Render/Simulation split。 |

## 5. Main Runtime Chains

### Fixed tick and frame

```text
Ogre frameStarted
 -> OS/OIS input + application-flow gate
 -> SandboxRuntime::update
    -> FixedTickScheduler (20 Hz, bounded catch-up)
       -> Player::update
       -> WorldManager::tick
          -> World::tick
    -> selection/action arbitration
    -> World::update(Camera)
       -> queued IWorldEvent -> bounded unload -> bounded sync mesh rebuild
 -> terrain/actor/projectile/UI snapshot sync
 -> audio/music update
 -> frameEnded performance/capture
```

### Chunk mesh publication

```text
main/worker lock: ChunkManager::beginMeshJob -> SectionMeshInput snapshot
worker off-lock: ChunkMeshBuilder::buildMesh
main/worker lock: ChunkManager::finishMeshJob(revision check)
main/Ogre: WorldMeshSnapshot -> GPU upload -> revision acknowledgement
```

### Persistence

```text
World::save
 -> ChunkManager / ChunkStorage / StorageTransaction
 -> WorldSave / StorageTransaction
 -> WorldBackup verified snapshot
```

## 6. Frozen Q1 Performance Source

权威证据目录：`docs/baselines/playability-rc-windows-hidden-v1/`。硬件为 NVIDIA GeForce GTX
1050 Ti、驱动 `30.0.14.7212`、隐藏 1600×900 Release 窗口；运行时代码 `320e293`、save v12、
seed `20260820`。六类 baseline/repeat 比较均为 `PASS`，A0 不重跑也不放宽阈值。

| Scenario | Baseline | Repeat |
| -------- | -------- | ------ |
| Usable menu / first controllable world | `996.539 / 610.143 ms` | `715.915 / 398.768 ms` |
| General frame P95 / P99 | `6.819 / 11.358 ms` | `7.594 / 11.367 ms` |
| Save transaction | `133.741 ms` | `189.593 ms` |
| Backup restore | `53.179 ms` | `63.072 ms` |
| Fast streaming frame P95 / P99 | `15.234 / 19.676 ms` | `14.730 / 20.647 ms` |
| Fast streaming chunk-visible P95 | `32.224 ms` | `52.797 ms` |
| Scaled gameplay frame P95 / P99 | `9.850 / 12.873 ms` | `9.215 / 12.166 ms` |

两轮 scaled gameplay 均为 361 loaded chunks、910,222 resident terrain vertices、1,750,560 indices、
36,129,344 terrain buffer bytes。A0 没有运行新的硬件性能采集，因为运行时代码与身份没有变化。

## 7. Frozen Q3 Scale Soak Source

`tools/run_release_candidate_soak.ps1` 使用 Release、seed `20260820`、schedule v2；nominal/stress
各 1,800 秒，均为 `PASS`：

| Metric | Nominal | Stress |
| ------ | ------- | ------ |
| Fixed ticks / failures | `36,000 / 0` | `36,000 / 0` |
| Movement / edit / actor / save-reload | `361 / 1,800 / 900 / 179` | `901 / 7,200 / 1,800 / 359` |
| Peak private / working set | `19,202,048 / 25,964,544` bytes | `25,640,960 / 32,706,560` bytes |
| Peak handles / threads | `229 / 4` | `229 / 5` |
| Steady private / handle growth | `1,286,144 / 1` | `8,892,416 / 0` |
| Timeout / child exit | `false / 0` | `false / 0` |

运行时、场景和工具身份没有差异，因此 A0 引用正式结果而不浪费 3,600 秒重复采集。

## 8. Automated Regression Commands

| Scope | Command | A0 result |
| ----- | ------- | --------- |
| Whitespace/patch integrity | `git diff --check` | `PASS` |
| Local Markdown references | A0 local-reference validator over all changed/untracked Markdown | `PASS` |
| World renderer boundary | `rg -n -e '^\s*#include.*Ogre' -e '^\s*#include.*SFML' -e '\bsf::' -e '\bGLfloat\b' -e '\bGLuint\b' -e '\bglad\b' src/HelloMine3D/World` | `PASS`；最初的宽泛文本搜索命中 3 条注释，被判为 `INVALID` probe 后改用 include/type token 规则，没有隐藏失败。 |
| Full Windows gate | `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify_build.ps1 -VisualStudioVersion 2017` | `PASS`；Debug/Release 均 0 error，世界 `832/832`、资源包 `80/80`、配方 `122/122`、启动负例 `15/15`，real window `PASS`。 |

完整门禁还通过 84 项 resource manifest、278 项 terrain atlas、38 项性能 fixture、11 项 Stage 10
supplement、受控崩溃与符号归档；本次 dump 为 `145,881` bytes，符号归档 build identity 为
`pdb-03444cc1f75d4459903aeafbb71d2a89-1`。Debug/Release 分别产生 2,044/2,023 条既有 Ogre/第三方
兼容警告，但没有编译错误，也没有把警告改写成失败或忽略门禁总状态。

如果完整门禁因环境原因无法运行，必须在本表保留实际 `FAIL/BLOCKED/NOT_RUN`，不得用历史 P11F
PASS 冒充本次执行。正式 Q1/Q3 仍引用第 6-7 节的冻结证据。

## 9. AI / Computer Use Evidence

A0 没有 OS 级 Computer Use 能力。完整门禁的 `real_window=PASS` 只证明 validation-only 客户端能
建立并关闭真实窗口，不是正常输入下的游戏交互；A0 没有用 hidden/validation client 冒充 AI
验收，也没有从 fixture、物品注入、传送、时间冻结、直接 Gameplay API 或截图反推 PASS。

| Scenario | Result | Declaration |
| -------- | ------ | ----------- |
| `AI-01` application/input | `NOT_RUN` | functional playability not newly claimed |
| `AI-02` container | `NOT_RUN` | functional playability not newly claimed |
| `AI-03` combat/respawn | `NOT_RUN` | functional playability not newly claimed |
| `AI-04` vertical slice | `NOT_RUN` | functional playability not newly claimed |
| `AI-05` Stage 11 scripted | `NOT_RUN` | functional playability not newly claimed |
| `AI-06` package-only blind play | `NOT_RUN` | AI-understandability proxy not claimed |
| `AI-07` visual/localization/audio | `NOT_RUN` | observable AI acceptance not claimed |
| `AI-08` full playable carrier | `NOT_RUN` | no `AI Playability PASS` for AL-A0 |

Human fun、aesthetics、retention、comfort、listening preference 与 physical input feel 全部保持
`NOT_CLAIMED`。历史 R3 v1 / Physical Input v2 仍为 `NOT_RUN/SUPERSEDED`，不回写历史报告。

## 10. AL-A0 Exit Checklist

| Exit item | State |
| --------- | ----- |
| 全部第一方顶层模块、职责、权威/派生与依赖方向 | `PASS` |
| World public API 与成员责任 | `PASS` |
| ChunkManager、thread/save/event ownership | `PASS` |
| tick 与 render snapshot 链 | `PASS` |
| 性能、回归、发行包和 AI identity | `PASS`（AI 结果本身为 `NOT_RUN`） |
| Tracked Gameplay/runtime/resource/build inputs changed | `NO` |
| Local verification artifact identity regenerated | `YES`（完整重建的预期结果；已与 P11F 历史 identity 分列） |
| AL-A1/AL-A2 implemented or approved | `NO` |
| A0 consistency validation | `PASS` |
| Local A0 commit | `PASS`；本报告所在 commit，以 `git log -1 -- docs/reports/architecture-lab-baseline-v1.md` 解析 |

A0 完成后唯一下一候选是 `AL-A1 World Responsibility Map`。它仍需项目所有者单独批准；本报告
不批准也不实现 AL-A1。
