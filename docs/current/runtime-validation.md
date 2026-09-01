# Runtime Validation

This document describes the automated runtime evidence and the separate
AI/Computer Use black-box boundary. Automated harnesses do not drive the
desktop; real-window functional playability follows
`docs/current/ai-assisted-gameplay-acceptance-v1.md` and is currently `NOT_RUN`.

Five implemented automated layers exist. Together they are the engineering
gate for the foundation and the completed milestones archived in
`docs/archive/project-ledger-2026-08-17.md`. AI interaction is a sixth, independent
evidence type and must not be inferred from these automated passes.

| Layer | Target | Answers |
| ----- | ------ | ------- |
| Focused headless tests | `HelloMine3DCoordinateTests`, `HelloMine3DMeshDirtyTests`, `HelloMine3DSaveLoadSmoke`, `HelloMine3DEntityLifecycleSmoke`, `HelloMine3DWorldCatalogueSmoke`, `HelloMine3DStorageTransactionSmoke`, `HelloMine3DWorldBackupSmoke` | Do the isolated algorithms and persistence boundaries behave? |
| World runtime smoke | `HelloMine3DWorldRuntimeSmoke` | Does the real `World` / `ChunkManager` / `WorldManager` / actor stack behave? |
| Stability and resource smokes | `HelloMine3DSoak`, `HelloMine3DResourcePackSmoke`, `HelloMine3DRecipeSmoke` | Do long-lived world transitions remain bounded, and are the frozen effective-resource and recipe views correct? |
| Client smokes | `tools/run_render_capture.ps1`, `tools/run_perf_baseline.ps1` | Does the assembled client still render and stay within its frame budget? |
| Distribution smoke | `tools/package_windows_release.ps1` | Can the Release client start and fail correctly from an isolated self-contained root? |
| AI interactive/visual acceptance | `docs/current/ai-assisted-gameplay-acceptance-v1.md` (`AI-01..AI-08`) | Can an AI use the clean Release package through normal OS input, and are the specified observable results present? Current result: `NOT_RUN`. |

## AL-A0 Evidence Dimensions

`AL-A0` freezes three orthogonal evidence dimensions. A result in one column
must not be promoted into another:

| Dimension | A0 state | What the state proves | What it does not prove |
| --------- | -------- | --------------------- | ---------------------- |
| Automated engineering | `PASS` for the frozen PLAYABILITY-RC evidence; the current A0 documentation-only rerun is recorded in `docs/reports/architecture-lab-baseline-v1.md` | VS2017/v141 build graph, deterministic tests, resource/format rules, Q1/Q3, crash/package boundaries and code-level architecture invariants. | Normal OS input, focus transitions, AI understanding, human fun or device feel. |
| AI/Computer Use | `AI-01..AI-08=NOT_RUN` | Only that no conforming OS-level run has been recorded yet. | A hidden client, injected input, fixture, screenshot or headless smoke is not an AI PASS. |
| Human/physical subjective scope | `NOT_CLAIMED` | The project intentionally makes no claim for human fun, aesthetics, retention, comfort, listening preference or physical-device feel. | `NOT_CLAIMED` is not a failure and is not a deferred engineering task. |

The A0 audit started at documentation commit
`4930023fb2f3022daac9968c10a1a0b76e1ac392`; the frozen runtime code identity is
`320e293c2f1db7f46aba776ddccdcf94369f2d05`. The clean PLAYABILITY-RC ZIP is
identified by SHA-256
`422F97E87046D4B6D5FC4BB99C37886FF37C4461A152C1162FA66A972B12F459`.
No OS-level Computer Use capability was available in this A0 task, so `AI-08`
remains `NOT_RUN` and AL-A0 makes no `AI Playability PASS` declaration.

## AL-A2 Chunk Runtime Boundary Evidence

AL-A2 changes runtime ownership without changing the public World facade or
save data. Two complementary gates close the boundary:

```powershell
& .\tools\validate_world_responsibility_map.ps1
& .\tools\validate_chunk_runtime_boundary.ps1 -Root (Get-Location).Path
```

The first keeps the 78-method public surface and frozen hash unchanged. The
second checks that the legacy queue/worker fields and implementations no longer
live in `World`, the concrete budgets remain frozen, `ChunkRuntime` owns the
coordination, and the source still orders `beginMeshJob -> off-lock build ->
finishMeshJob` without introducing `ChunkResidency`.

The real `HelloMine3DWorldRuntimeSmoke` supplies behavioral evidence:

| Assertions | AL-A2 property |
| ---------- | -------------- |
| `S0.5/*` | Interior, Chunk-edge and section-edge edits dirty only required loaded sections. |
| `M2/*` | Queue keys deduplicate, drain FIFO, preserve the two-section frame budget and do no work when empty. |
| `V5/*` | One background loader remains safe during load-center churn and resumes mesh progress. |
| `M6/*`, `M7/*` | Snapshot/off-lock work counts remain correct and frustum planning prioritizes without filtering. |
| `E5/*` | Stale upload acknowledgement cannot promote a newer block revision. |
| `S2.4/*` | Dirty data is saved before unload and survives reload without remaining dirty. |

Full VS2017/v141 Debug/Release and clean-package results are frozen in
`docs/reports/architecture-lab-a2-chunk-runtime-report-v1.md`. This automated
evidence proves behavior preservation and ownership, not real-window play or
human experience; `AI-01..AI-08` remain independently classified.

The 2026-09-01 AL-A2 closeout ran
`scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow` and passed
both configurations: World runtime `832/832` twice, resource packs `80/80`,
recipes `122/122`, startup negatives `15/15`, both short soaks and all remaining
headless gates. The isolated package contains 104 entries and has SHA-256
`D1320264A105671C711E1CEF90D9F18382DFA1FB8D59D806C1F72DF80E4802AD`.
Because the run deliberately skipped a focus-stealing window, its real-window
result is `DEFERRED`, not `PASS`; AI acceptance remains `NOT_RUN`.

## AL-A3 Simulation Runtime Evidence

AL-A3 preserves `World::tick(int)` as the public entry and delegates its prior
fixed-tick body to one concrete `WorldSimulation`. Three static gates compose:

```powershell
& .\tools\validate_world_responsibility_map.ps1
& .\tools\validate_chunk_runtime_boundary.ps1 -Root (Get-Location).Path
& .\tools\validate_world_simulation_boundary.ps1 -Root (Get-Location).Path
```

The A1 gate keeps the 78-method public surface and hash unchanged. The A3 gate
checks the eight frozen phases and source call order, single delegation, raw
last-tick timing, caller-owned Ogre pause gate and absence of Scheduler,
Registry, Budget or deferred-work abstractions.

The `HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A3` path supplies behavioral evidence:

| Assertions | AL-A3 property |
| ---------- | -------------- |
| `AL-A3/initial-snapshot-has-no-completed-tick` | Observation starts empty and cannot masquerade as a completed tick. |
| `AL-A3/phase-identity-and-order-are-frozen` | Eight copied timing rows retain the contract order. |
| `AL-A3/tick-context-is-propagated` | Caller tick and the existing 20 Hz delta reach the snapshot. |
| `AL-A3/raw-last-tick-timings-are-observable` | Each phase and whole tick expose a usable raw sample. |
| `AL-A3/caller-owned-pause-gate-freezes-simulation` | Rejected application flow emits no World tick. |
| `AL-A3/resume-advances-exactly-one-tick` | Resume keeps the existing bounded scheduler behavior. |

The ordinary Debug World runtime passes `838/838`, including all existing
random-tick, Actor, combat, population, furnace, progression, save and
determinism cases. Full VS2017/v141 Debug/Release and clean-package identity are
frozen in `docs/reports/architecture-lab-a3-world-simulation-report-v1.md`.
Raw timing is diagnostic only: this batch makes no new Q1/Q3 performance claim,
and AI acceptance remains independently classified.

## AL-A4 Event / Command / Query Boundary Evidence

AL-A4 preserves the existing frame-owned interaction FIFO and synchronous,
World-local fact delivery while making their meanings enforceable. Four static
gates now compose:

```powershell
& .\tools\validate_world_responsibility_map.ps1
& .\tools\validate_chunk_runtime_boundary.ps1 -Root (Get-Location).Path
& .\tools\validate_world_simulation_boundary.ps1 -Root (Get-Location).Path
& .\tools\validate_event_command_query_boundary.ps1 -Root (Get-Location).Path
```

The reviewed public surface remains 78 method names, now with `addCommand`
classified as a Command and SHA-256
`8B2CDDF30B70DA91D5EF4944D7E1397BC9434EB0E129B9313DA471143F653EC4`.
The A4 gate rejects a second event-as-command vocabulary, undeclared production
subscriber effects, missing recursion/diagnostic guards and future
Network-to-Ogre or Machine-to-UI/Presentation dependency drift.

The `HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A4` path passes `35/35`; eight new
assertions provide the boundary evidence:

| Assertions | AL-A4 property |
| ---------- | -------------- |
| `AL-A4/current-events-are-immutable-domain-facts` | Current facts expose immutable type/category identity. |
| `AL-A4/observer-delivery-is-synchronous-and-declared` | Observer delivery remains immediate and explicitly classified. |
| `AL-A4/observer-cannot-hide-nested-publication` | Observe-only handlers cannot turn notification into a hidden command chain. |
| `AL-A4/declared-domain-reaction-may-publish-bounded-fact` | The documented Waystone-style domain reaction may publish a nested fact. |
| `AL-A4/recursive-publication-has-hard-depth-limit` | Synchronous recursion stops at eight active dispatch frames. |
| `AL-A4/diagnostic-event-cannot-drive-domain-mutation` | Diagnostic facts are rejected for mutation handlers. |
| `AL-A4/subscription-membership-is-snapshotted-per-publication` | Handler-side membership changes affect later publications only. |
| `AL-A4/handler-exception-restores-dispatch-boundary` | Exceptions propagate without corrupting depth/policy state. |

The 2026-09-01 closeout ran
`scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow`. Debug and
Release each pass the complete `846/846` World runtime stack; resource packs
pass `80/80`, recipes `122/122`, startup negatives `15/15`, both short soaks
and every remaining headless target. The isolated package contains 104 entries
and has SHA-256
`0E33793CD2F504662712C79184AACECF544C5548C39E496431D2ACACCE07FA77`.
The Release executable is 8,760,832 bytes with SHA-256
`AE184322BD709A0801C0602BFBBDB3A690D35113F4004BD5D450D6732F076E5F`.
Real-window launch is `DEFERRED`, AI acceptance remains `NOT_RUN`, and human
subjective experience remains `NOT_CLAIMED`.

## AL-A5 Simulation Phase Metrics Evidence

AL-A5 keeps the AL-A3 phase order and last-tick publication model. A fifth
static architecture gate composes with A1-A4:

```powershell
& .\tools\validate_simulation_metrics_boundary.ps1 `
  -Root (Get-Location).Path
```

The gate freezes exactly four metric identities: `ActorSimulation`, `Combat`,
`BlockRandomTick` and `Population`. It checks elapsed/processed/deferred/budget,
three budget scopes, four derived statuses, the developer Simulation panel,
existing hard limits, persistence exclusion and the absence of speculative
Scheduler/Registry/system slots.

`HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A5` passes `13/13` in VS2017/v141 Debug and
Release. Seven A5 assertions add the following evidence to the six A3 checks:

| Assertions | AL-A5 property |
| ---------- | -------------- |
| `AL-A5/metric-identities-exclude-empty-system-slots` | Only four implemented work contracts appear, in frozen order. |
| `AL-A5/budget-status-vocabulary-is-frozen` | Unbudgeted, WithinBudget, AtBudget and WorkDeferred have total, stable meanings. |
| `AL-A5/metric-elapsed-matches-a3-phase-timing` | A metric and its A3 phase row share one elapsed sample. |
| `AL-A5/actor-work-is-unbudgeted-and-counted` | PlayerActor plus invoked live actors are observed without inventing a limit. |
| `AL-A5/combat-uses-existing-per-tick-budget` | Projectile processed/deferred values use the unchanged 32-step limit. |
| `AL-A5/random-tick-uses-existing-per-tick-budget` | Active-section work uses the unchanged four-section limit. |
| `AL-A5/population-is-per-cycle-and-resets-next-tick` | A real Normal cycle reports `16/16`, then a non-cycle tick resets processed/deferred to zero. |

The metrics are copied diagnostics, not performance acceptance. AL-A5 defines
no average, percentile, time threshold or new Q1/Q3 result. The complete
VS2017/v141 Debug/Release gate passes with `853/853` WorldRuntime checks in
both configurations, `80/80` resource-pack checks, `122/122` recipe checks,
`15/15` startup negatives and two short soaks with zero failures. The isolated
package contains 104 entries and has SHA-256
`4DA15FA1799804FB61710FA285DD0E6818BE1B42CE26AFA25E1F777DF3334009`;
the 8,762,880-byte Release executable has SHA-256
`14416A52E882EBDE15A41FD8A251AF46D74EE0388A297BFD77596C8097296F2A`.
Real-window launch is `DEFERRED`, AI acceptance remains `NOT_RUN`, and human
subjective experience remains `NOT_CLAIMED`. Full closeout is recorded in
`docs/reports/architecture-lab-a5-simulation-metrics-report-v1.md`.

## AL-A6 Architecture Lab Documentation Pipeline Evidence

AL-A6 adds a sixth static Architecture Lab gate without changing runtime code:

```powershell
& .\tools\validate_architecture_lab_documentation.ps1 `
  -Root (Get-Location).Path -SelfTest
```

The positive path checks the single living tutorial, seven AL-A0..AL-A6
manifest rows, one implemented Part, six real Sections, repository-bounded
evidence paths, task-ledger completion states and seven non-empty logical
headings per Section. Four isolated negative fixtures prove that a malformed
manifest, missing evidence, empty logical section or placeholder Part fails.

The validator runs from `scripts\verify_build.ps1` after the AL-A1..AL-A5
architecture gates and before resource/build work. Its focused result is
`batches=7 parts=1 sections=6 negative_fixtures=4 status=PASS`. The complete
VS2017/v141 gate passes `853/853` WorldRuntime checks in both configurations,
`80/80` resource-pack checks, `122/122` recipe checks, `15/15` startup
negatives and two zero-failure short soaks. The 104-entry clean package has
SHA-256 `0E387185600E957F8BC6ED4B46333817B0C12F0725322F9F939337455C245990`;
the 8,762,880-byte Release executable has SHA-256
`30F441A59FBF55AAEB6964BA8D43B08E008BA869EE4AB7392FD49CAC5D6741D2`.
Real-window launch is `DEFERRED`, AI acceptance remains `NOT_RUN`, and human
subjective experience remains `NOT_CLAIMED`. This evidence proves
documentation identity and maintenance structure; it does not prove prose
quality, gameplay, Computer Use or human experience.

## B1 Chunk Residency State Machine Evidence

B1 adds a seventh static architecture gate while preserving the AL-A1 public
surface and AL-A2 budgets:

```powershell
& .\tools\validate_chunk_residency_state_machine.ps1 `
  -Root (Get-Location).Path
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "B1"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

The static gate freezes 7 Data, 5 Mesh and 4 Render states, verifies three
separate owners and mutation assertions, requires all state-family diagnostics
and rejects legacy combined enums plus unapproved B2-B9 vocabulary. The focused
runtime passes `38/38`: transition graphs/names, generated residence, separate
counts, Mesh `Dirty -> Clean`, dirty eviction/save/reload, injected save-failure
rollback, stale upload rejection, existing light-only invalidation and unload
persistence, plus direct deterministic structure generation and reload.

Ogre upload acceptance is checked through copied live-section revisions after
the unchanged `void World::acknowledgeSectionMeshUploads` call. This keeps the
78-method public surface hash unchanged while ensuring an unloaded or newer
revision never reaches `GpuResident`. Full closeout evidence is frozen in
`docs/reports/architecture-lab-b1-chunk-residency-report-v1.md`. AI scenarios
remain `NOT_RUN` and human subjective experience remains `NOT_CLAIMED`.

## B2 Streaming Demand Model Evidence

B2 adds an eighth static architecture gate without changing the AL-A1 public
surface, B1 state ownership or existing loader budgets:

```powershell
& .\tools\validate_streaming_demand_model.ps1 `
  -Root (Get-Location).Path
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "B2"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

The static gate freezes four reason slots, priorities `400/300/200/100`, exact
epoch expiry, de-duplicated planning, publication sites and copied diagnostics.
At B2 closeout it rejected B3-B9. The current composed gate admits only the
separately contracted B3 scheduler and B4 generation cancellation while still
rejecting B5-B9 capabilities. It passes alongside AL-A1..B1; the public World map remains 78
methods with SHA-256
`8B2CDDF30B70DA91D5EF4944D7E1397BC9434EB0E129B9313DA471143F653EC4`.

The focused runtime passes `26/26`: twelve B2 checks cover stable bounded
refresh, overlap merge, per-reason replacement, teleport priority, forward
motion, camera-turn replanning, exact expiry and integrated Player/Camera/
Preload/Teleport publication; existing WorldManager checks remain in the same
focus path. The complete VS2017/v141 gate passes `875/875` WorldRuntime in both
configurations, `80/80` resource-pack, `122/122` recipe, `15/15` startup
negatives and two zero-failure short soaks. The 104-entry package SHA-256 is
`9955E51AD94C7E5600325329031432E16C1859F936FA880F68F65F3B80369503`.
AI scenarios remain `NOT_RUN`; human subjective experience remains
`NOT_CLAIMED`.

## B3 Generic World Job Scheduler Evidence

B3 adds a ninth static architecture gate while preserving B1 lifecycle owners,
B2 demand ordering and the AL-A2 one-worker budgets:

```powershell
& .\tools\validate_world_job_scheduler.ps1 `
  -Root (Get-Location).Path
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "B3"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

At B3 closeout the static gate froze exactly two real types, three states and three outcomes;
the current composed gate permits B4's `Cancelled` outcome/generation token and B5's separately contracted
admission/pressure vocabulary while preserving
deterministic priority/plan/epoch/type/id order, one in-flight slot and one
loader worker. It requires the real `ChunkRuntime`/`ChunkManager` calls,
developer diagnostics and all nine B3 test identities while continuing to reject B6-B9
Spatial Interest and far-representation concepts.

The focused runtime passes `9/9`. Pure checks cover duplicate pending-key
rejection, ordering, `Pending -> InFlight -> Completed`, invalid completion,
pending replacement with in-flight preservation, completion values and copied
metrics. A live background `World` check observes both
`ChunkLoadOrGenerate` and `ChunkMeshBuild`; the existing B1 stale-revision
assertion remains in the composed gate. B2 and B1 focused regressions pass
`26/26` and `38/38` in the same VS2017/v141 Debug build.

The complete `scripts\verify_build.ps1 -VisualStudioVersion 2017
-SkipRealWindow` gate passes with Debug `2044 warnings / 0 errors`, Release
`2023 warnings / 0 errors`, `884/884` WorldRuntime in both configurations,
`80/80` resource-pack checks, `122/122` recipe checks, `15/15` startup
negatives and two zero-failure short soaks. The clean 104-entry package
SHA-256 is
`B983889B5553FF0DBFEAF6C14D2E349CC81AD1205C314CC39C0894C2D1CD9459`;
the final status is `PASS real_window=DEFERRED`. AI scenarios remain `NOT_RUN`;
human subjective experience remains `NOT_CLAIMED`.

## B4 World Job Cancellation & Generation Token Evidence

B4 adds a tenth static Architecture Lab gate and extends only the two B3 job
types. The implementation-stage commands are:

```powershell
& .\tools\validate_world_job_cancellation.ps1 `
  -Root (Get-Location).Path -Implementation
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "B4"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

The static gate freezes a non-zero uint64 generation token, one new
`Cancelled` outcome, six semantic invalidation call sites, the World-mutex plus
generation/commit linearization boundary, detached Chunk load protocol,
random-tick isolation, exact mesh rollback and copied diagnostics. The current
composed gate permits B5's separately contracted pressure/admission extension
while still rejecting B6 Spatial Interest and all far, machine or network job
vocabulary.

The focused runtime passes `10/10`. Pure scheduler checks cover monotonic
invalidation, pending clear with in-flight preservation, stale/zero request
rejection, current-generation B3 ordering, exact cancellation completion and
counter identities. Integration checks prove a cancelled detached candidate
publishes no Chunk event or random-tick index mutation, a current candidate
commits once and registers its active sections, cancelled mesh returns to
`Dirty` without adoption, 300 concurrent replans/invalidations remain bounded
and deadlock-free, and a live background World advances generation while still
executing current work. B3, B2 and B1 focused regressions pass `9/9`, `26/26`
and `38/38` in the same VS2017/v141 Debug build.

The complete `scripts\verify_build.ps1 -VisualStudioVersion 2017
-SkipRealWindow` gate passes in Debug and Release with `894/894` WorldRuntime,
`80/80` resource-pack, `122/122` recipe and `15/15` startup-negative checks,
plus two zero-failure short soaks. Debug/Release rebuilds finish with
`2044/2023` warnings and zero errors. Hidden clients, crash diagnostics and
the executable inventory pass. The Release executable is 8,792,064 bytes and
hashes to `C79BEFE8C4E06478C15D12A831596BB147C38BBC7BD3F2BAD291409D5803E206`;
the 104-entry isolated ZIP is 17,053,910 bytes and hashes to
`A9A7CC9AF528F3C725ACC13A62A69FB6D10718AD25F7F4796F5E47B70453C33A`.
The final result is `PASS real_window=DEFERRED`. AI scenarios remain `NOT_RUN`;
human fun, aesthetics, comfort, retention and physical input feel remain
`NOT_CLAIMED`.

## B5 Streaming Backpressure Evidence

B5 adds an eleventh static Architecture Lab gate and bounds both scheduler
admission and the three existing consumers. The implementation-stage commands
are:

```powershell
& .\tools\validate_streaming_backpressure.ps1 `
  -Root (Get-Location).Path -Implementation
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "B5"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

The static gate passes with pending caps `128/128/128`, watermarks `96/48`,
loader commit/upload/unload limits `8/8/8`, exactly one loader declaration,
copied diagnostics and all twelve B5 test identities. It also checks the
retained plan/cursor refill implementation and rejects B6 Spatial Interest,
Far representation, machine/network vocabulary and future simulation policy.

The focused runtime passes `12/12`. Pure checks cover the exact vocabulary,
normal admission, duplicate/stale distinction, hard-cap rejection,
deterministic worst-pending shedding, pressure hysteresis, generation
invalidation and a 180-request ordered drain/refill. Integration checks observe
a live radius-eight plan larger than the 96-job window while pending/peak stay
bounded, at most eight authoritative commits per loader pass, deterministic
eight-section upload selection and an 8-then-2 truthful unload backlog. B4,
B3, B2 and B1 focused regressions pass `10/10`, `9/9`, `26/26` and `38/38`.

The complete `scripts\verify_build.ps1 -VisualStudioVersion 2017
-SkipRealWindow` run passes in Debug and Release with `906/906` WorldRuntime,
`80/80` Resource Pack, `122/122` Recipe, `15/15` startup negatives, both short
soaks at zero failures and both validation-only clients. Debug/Release rebuilds
finish at `2044/0` and `2023/0` warnings/errors. The Release executable SHA-256
is `3377D89FF0E33D53142FA7F0F3405B8955A6B095C8E6FFEB29FB0AB74E6F1204`;
the 104-entry isolated package SHA-256 is
`7D126B31B78F3A4E8F8C90A5D769028EC686C0D4F708D1F6B2E2979BD164050B`.
The result is `PASS real_window=DEFERRED` and B5 is `Done`. AI scenarios remain
`NOT_RUN`; human subjective experience remains `NOT_CLAIMED`.

## B6 Spatial Activation Evidence

B6 adds a twelfth static Architecture Lab gate and a twelve-check focused
runtime case. The implementation-stage commands are:

```powershell
& .\tools\validate_spatial_activation.ps1 `
  -Root (Get-Location).Path -Implementation
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "B6"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

The static gate freezes four classifications, nested resident/near/simulation
flags, a radius-two Player simulation request, deterministic B2 derivation,
copied diagnostics, one loader and the unchanged B5 `8/8/8` consumer limits.
It requires real plan/mesh/upload/unload consumers and rejects Far, B7-B9 and
D2 fidelity vocabulary.

The VS2017/v141 Debug focused runtime passes `12/12`. Pure checks cover exact
vocabulary/hierarchy, absent coordinates, all four source policies, overlap
merge, stable ordering/revision and radius shrink. Integration checks prove
resident-only work stops before mesh follow-up, copied mesh publication hides
resident-only loaded sections, active Resident interest prevents unload until
expiry and then resumes with an 8-then-1 backlog, and a far Actor remains in the
unchanged unbudgeted fixed-tick metric because Simulation Requested is only
published.

The complete `scripts\verify_build.ps1 -VisualStudioVersion 2017
-SkipRealWindow` run passes in Debug and Release with `918/918` WorldRuntime,
`80/80` Resource Pack, `122/122` Recipe, `15/15` startup negatives, both short
soaks at zero failures and both validation-only clients. Debug/Release rebuilds
finish at `2044/0` and `2023/0` warnings/errors. The Release executable SHA-256
is `D86A532AA3673011A8CBF1993DB48EF613B9922911324D34836A7CF75FA476B6`;
the 104-entry isolated package SHA-256 is
`C8E260E00CF76C952150EBC3DC851A7EDE5E13FE63A58F98B77DC103723EFA3C`.
The result is `PASS real_window=DEFERRED` and B6 is `Done`. AI scenarios remain
`NOT_RUN`; human subjective experience remains `NOT_CLAIMED`.

## World Runtime Smoke

`src/HelloMine3D/Tests/WorldRuntimeSmokeMain.cpp` links the whole game runtime
except the Ogre client shell and drives the real gameplay classes through
scripted scenarios. It creates no window or graphics context: FreeImage checks
the packed atlas, mesh UV calculation is pure data, and the `World/` data layer
has no renderer types.

```powershell
bin\HelloMine3DWorldRuntimeSmoke.exe
```

Output is one line per assertion plus a summary:

```text
[VALIDATION] PASS S0.5/chunk-boundary-marks-neighbor
[VALIDATION] FAIL S6.4/ore-decorator-produces-ore :: coal=0 iron=0
[VALIDATION] checks=429 failures=0
[VALIDATION] status=PASS
```

Check ids are `<milestone>/<behaviour>`, so a failure points straight at the
milestone that regressed. The process exits non-zero when any check fails.

### Isolation

Every scenario runs against a fresh directory under `bin/validation_runs/`,
so a validation run never reads or writes `bin/saves/`. Terrain seed, player
position and player rotation are forced per scenario through the same
`HELLOMINE3D_*` environment variables the capture scripts use.

### Coverage

| Scenario | Milestones | What it proves |
| -------- | ---------- | -------------- |
| `caseDebugPanelStartupOption` | V3, S7.1 | False/true environment values are parsed consistently and `HELLOMINE3D_SHOW_DEBUG_INFO` controls the initial panel state before capture begins. |
| `caseFixedTickScheduler` | V1, S1.5 | The runtime scheduler emits exactly 200 ticks over 10 seconds and bounds catch-up work to five ticks per frame. |
| `caseBlockTextureCoordinates` | E0 | Atlas UV generation is stable and does not require a graphics context. |
| `caseRuntimeConfigOwnership` | A3 | A missing per-user config is regenerated with the documented defaults, and edited window, FOV, fullscreen, render-distance, mouse-sensitivity and vertical-look-inversion values load without touching repository-owned templates. |
| `caseConfiguredWorldSeed` | A4 | An integer seed loaded from an isolated config becomes the seed of two new worlds and reproduces every sampled terrain block without an environment override. |
| `caseOreTextures` | P4 | Coal and iron definitions use dedicated atlas slots, the packed atlas loads at 256x256, and both tile hashes differ from stone and each other. |
| `caseBlockDataDiagnostics` | A2 | Isolated malformed definitions prove missing keys, invalid enums, out-of-range atlas coordinates and duplicate ids name both the exact file and offending key. |
| `caseBlockTextureCoordinates` / terrain material profile | V10B1 | The default 256/16/16 profile preserves the original first/last half-pixel UVs, while a 512/32/16 fixture proves CPU coordinates follow validated parameters. ResourcePackSmoke adds strict header/key/range/partition/PNG/shader checks and effective-pack override coverage; set `HELLOMINE3D_WORLD_SMOKE_FOCUS=V10B1` for the four world-side checks. On 2026-08-27 VS2017/v141 Debug/Release resource-pack runs passed 54/54, both focused runs passed 4/4, the final Release world run passed 718/718, and 12 real-client startup failure cases named missing/invalid resources before gameplay. The tracked Release forest screenshot matches 216,000 static V10A foreground pixels exactly. Windows is Done; macOS Release shader/window evidence remains Verify. |
| `validate_terrain_atlas.ps1` / V10B2 asset contract | V10B2 | The machine-readable layout freezes 37 unique bilingual semantics and opaque/cutout/translucent/icon rules; the validator checks 106 conditions including 22 block face maps, all 34 material icon coordinates, empty tiles, HUD profile wiring and byte-identical regeneration. VS2017/v141 Debug/Release affected targets pass, ResourcePackSmoke is 56/56, Release WorldRuntimeSmoke is 718/718, startup diagnostics cover 13 cases, the generated manifest has 67 entries, and the hidden clean package has 85 files. Three tracked Release readbacks cover the fixed forest/HUD, terrain-v3 ruin and camp. Engineering is complete; the project owner must still self-launch a visible Release for the required 5-10 minute developer visual check. |
| `caseBlockSelection` | P3 | One ray result identifies the solid target and adjacent placement voxel, skips water, respects reach and reports an empty miss. |
| `casePlayerControllerInput` | V2, S5.3 | Synthetic input proves frame sampling is idempotent and fixed-tick movement has normalized diagonals, sprint strafe, target-velocity acceleration/braking, flying jump and held sneak. The same path covers interpolated camera position, configured mouse look and hotbar selection. |
| `casePlayerSweptCollision` | V2 | A player falling more than ten blocks in one fixed tick cannot skip a one-block floor; resting contact remains stable and the next jump succeeds. |
| `caseHeightMapEdits` | V4 | The cached column height matches a brute-force scan after generation, breaking the highest opaque block, and placing a block above the old top. |
| `caseBackgroundLoaderStress` | V5, S0.3 | During 240 load-center changes, concurrent block reads stay valid, chunk/section counters remain internally consistent, and the worker resumes mesh progress at a new stable center. |
| `caseSpawnPreload` | S0.6 | Spawn preloading uses chunk coordinates and produces a full 3x3 loaded neighbourhood on solid ground. |
| `caseManagedWorldFirstSpawn` | FS1, K4, N5 | The real catalogue create/open path starts uninitialized, deterministically replaces the placeholder with non-ocean solid ground, two-block clearance and nearby oak, persists/reopens the same spawn, safely rescues only short empty placeholder sessions, preserves non-natural actors and never relocates a progressed fixture. |
| `caseNegativeCoordinates` | S0.1 | Negative and cross-zero world coordinates map to the right chunk and local block, both on write and read back. |
| `caseNoImplicitChunkCreation` | S0.2 | Reading or writing a block outside the loaded set does not create chunks. |
| `caseMeshDirtyPropagation` | M2, S0.4, S0.5 | Interior and boundary edits dirty the correct sections. Five queued sections are deduplicated, rebuilt FIFO with a two-per-update budget, and drained across three frames. |
| `casePersistence` | P2, S1.3, S2.1, S2.4, S2.5, S2.6, S6.1, K1-K3 | Block edits, world metadata, seed, spawn point, player transform/inventory, mobs and item entities all survive a relaunch. Actor subtype state and id allocation are compared after restore; successful real world/chunk transactions expose aggregate total/max duration and explicit world save publishes one validated backup. |
| `caseChestContainer` / `caseWorkbenchCrafting` | D2, G2-G3 | Container transfers conserve totals, persist and spill on break; workbench focus blocks world actions. G3 additionally proves the v1 chest rejects durability-bearing tools instead of losing instance state. |
| `caseToolMiningProgression` | G3 | Hand/wood/stone mining times are ordered, tier gates drops, progress resets on target/tool change and clamps long frames, completed breaks consume exact durability, broken tools advance safely, invalid durability cannot replace the last valid save, and legacy inventory rows upgrade to subformat 2. |
| `caseAudioFeedback` | G5 | Strict seven-cue parsing, categories, 2D/3D, five successful domain-event mappings, failed-craft silence, master/category volume, pause/mute/suspend, missing definitions, detach, ambient cadence, missing-file dummy degradation and global/per-cue voice bounds. |
| `caseSectionMeshInput` | M1, M3 | The 18x18x18 block halo matches direct world reads, edits advance the section revision, and accepted mesh builds accumulate timing plus solid/glass/water/flora face and vertex metrics. |
| `caseTransparentBlockRules` | L4 | Glass definitions and materials round-trip, transparent cubes use the glass pass, leaves use static cutout and flora keeps its animated pass. Shared glass faces are removed within and across chunks while opaque/glass boundaries retain exactly one face. |
| `caseBlockBehaviorDispatch` | C1 | Every definition owns a behavior; default drops delegate to static definitions, while glass registers a no-drop policy. The real break path consumes that policy without adding an item or spawning an entity. |
| `caseMetadataBackedBehavior` | C2 | Tall grass uses one block id with named immature/mature metadata. Direct strategy dispatch, biome output and real break interactions prove immature grass has no drop while mature grass drops the configured item. |
| `caseRandomTickScheduling` | C7 | Only immature tall grass opts into random ticks. Five indexed sections prove the four-per-tick round-robin budget, then unload removes the index and storage reload rebuilds it before growth resumes. |
| `caseResourceDrivenBlockShapes` | C3 | Flora definitions resolve a shared `Cross` shape, exact face vertices come from the resource, an isolated single-quad fixture parses without builder changes, and a real section emits the expected flora mesh. |
| `caseSunlightStorage` | L1 | Sunlight conversion stays within 0-15, open and roofed columns differ, the full 18x18x18 halo carries sunlight, mesh brightness and greedy boundaries respect light values, and storage reload rebuilds derived sunlight. |
| `caseVertexLighting` | V10A | Versioned CPU-only corner lighting covers AO 0/1/3, AO-disabled smooth-light preservation, transparent neighbours, lower-error diagonal selection, four logical corner values in the existing 32-byte stream, reconstruction-safe greedy output, byte-stable rebuild order and matching cross-section vertices. Coplanar byte-identical solid vertices may share indices; M4 proves 14 faces use 36 rather than 56 vertices while retaining 84 indices and an 8-block texture-repeat span. Set `HELLOMINE3D_WORLD_SMOKE_FOCUS=V10A` for the related 39-check subset. On 2026-08-27 VS2017/v141 Debug/Release each passed 39 focused checks and the final Release run passed all 716 checks. Sixteen tracked AO/no-AO RuntimeReadbacks cover eight scenes and a visible-window cave check passes without seams or transparent-leaf occlusion. Stable final Q1 repeats record fast-streaming frame P95/P99 +14.2%/+13.6%, mesh average +26.1% and solid vertices +19.9%; scaled-gameplay misses the old P95 limit by 0.045 ms, with resident vertices/indices/buffer +10.8%/+41.8%/+15.7% and mesh average +23.2%. An AO-disabled diagnostic isolates the index growth to exact AO, and two slower sampling experiments were rejected. The project owner explicitly accepted this bounded V10A exception on 2026-08-27; later batches did not inherit it and VISUAL-RC final Q1/Q3 passed on 2026-08-28. |
| `caseBlockLightStorage` | L2 | A data-driven level-14 emitter falls off by one per voxel, opaque blocks stop propagation, the halo carries block light, meshes choose the stronger light source, and storage reload rebuilds derived values. |
| `caseLocalRelightAfterEdits` | L3 | Edits update one sunlight column and locally remove/refill block light across chunk boundaries; mesh revisions and a bounded section queue track light-only changes, while chunk load/unload reconciles boundary light. |
| `caseSectionMeshUploadSnapshot` | E5 | Ogre-facing CPU mesh snapshots include live section identity and block revision; stale upload acknowledgements cannot overwrite a newer edit. |
| `caseUnloadPersistence` | S2.4 | Unloading a chunk flushes it to storage first, and reloading restores the edit instead of regenerating. |
| `caseChunkFormatRejection` | S2.2, S2.3 | Chunk file paths are deterministic, and a corrupted magic is rejected with a diagnostic rather than loaded as garbage. |
| `caseTerrainDeterminism` | S6.1, S6.4, C4 | The same seed produces identical terrain over 175k sampled blocks, ore layout is stable, underground cave indices repeat exactly, caves are non-empty, and protected surface layers remain solid. |
| `caseTerrainStructures` | S6.2, S6.3, S6.5, C5 | Tree and plant decorators run, biomes produce varied surfaces, structures cross chunk boundaries, generation ignores adjacent-chunk load order, and structures are identical after a save/reload roundtrip. |
| `caseInteractionAndEvents` | S3.1, S3.3, S3.4, S3.5, S4.2, S4.5, P5 | Break/place/use go through the interaction system, produce configured drops, consume items, publish events with the target identity, and block metadata survives a roundtrip. |
| `caseChunkEvents` | S4.3 | Generate, load, save and unload each publish their chunk event. |
| `caseActors` | P1, S4.4, S5.1, S5.2, S5.5, S5.6, C6 | Mobs spawn, chase nearby players, wander outside chase range, reject repeated damage during invulnerability, die and get culled; item entities spawn and are picked up, publishing the matching events. Immutable actor snapshots track transforms and omit dead or picked-up ids. |
| `casePlayableVerticalSlice` | D6 | After one deterministic fixture, only normal gameplay paths acquire/plant/grow/harvest a crop, transfer Wheat into a chest, spawn and defeat a natural Mob, pick up its physical drop, replant, save and relaunch with restored state. |
| `caseWorldManager` | S1.2, S1.4, S1.5, S4.5 | World creation, lookup, save, load, same-world teleport, cross-world rejection and world-time advance. |

### Not covered by automation

| Gap | Current treatment |
| --- | --- |
| Real window, focus and normal gameplay input | OS-level Computer Use runs `AI-01..AI-08`; hidden harnesses and injected events cannot close it. Current result is `NOT_RUN`. |
| Physical-device distance, force and comfort | `NOT_CLAIMED`; the historical R3/Physical Input protocols remain available for optional developer self-test. |
| Human fun, aesthetic preference, listening preference and retention | `NOT_CLAIMED`; AI may only report functional completion, observable defects and AI-understandability proxies. |

## Acceptance Contracts

This table states the evidence contract. Current implementation status and
remaining acceptance gaps are in `docs/current/todolist.md`; historical task detail is
in `docs/archive/project-ledger-2026-08-17.md`, and completed runs are recorded in
`Current Verified Runs` below.

| Scope | Required extension | Evidence required to close |
| ----- | ------------------ | -------------------------- |
| R1 performance comparison | Compare two performance summaries only after build configuration, scene identity, vsync regime and final chunk/section residency are compatible. Apply the documented P95 +15%, P99 +20% and material `frames_over_50ms` warnings as a non-zero regression result. | Deterministic pass/fail/incomparable fixtures, a current baseline self-compare and one intentional regression fixture. Record both input run ids and the comparison result. |
| D1-D2 stateful container | Extend focused save/load and world runtime coverage for block-entity ownership, unique positions, container transfer, block removal and compatibility. Add a client fixture for container UI without bypassing the P5 use path. | Debug/Release assertions and `AI-02` normal container interaction. |
| D3-D4 live mobs and combat | Cover deterministic spawn candidates, population caps, safe placement, restore deduplication, actor targeting, accepted/suppressed damage, death/drop ordering and player respawn. Validation-only spawn helpers remain fixture-only and cannot satisfy D6. | Focused actor tests, world assertions and `AI-03` attack/damage/death/respawn. |
| D5 crop loop | Cover support checks, metadata stages, random-tick budgets, unloaded-section exclusion, immature/mature drops, harvest/replant and persistence. | World assertions plus asset and generated-manifest checks for every new block, material, shape or texture. |
| R2 long-running soak | Use a fixed seed and versioned action schedule to repeat load-centre movement, block edits, actor/item lifecycle and save/reload while sampling memory, process handles, dirty queues and mesh progress. Developer runs may be shorter; milestone evidence is at least 30 minutes. | Timestamped summary and bounded logs outside Git. Fail non-zero on invariant errors, save corruption, stalled mesh progress or unbounded counter growth; record build configuration, seed, duration and peak/final counters. |
| R3 input boundary | Version 1 and Physical Input v2 remain truthful historical physical protocols and keep `NOT_RUN`; they are `SUPERSEDED` as current Architecture Lab gates. | `R3-AI-Functional` uses `AI-01..AI-04`; physical-device feel is `NOT_CLAIMED`. |
| D6 playable slice | Run the normal fixed-seed player path through crop acquisition/planting, container use, a live mob encounter, combat, loot pickup, save, relaunch and restored-state inspection. | Full Windows gate, applicable headless assertions and `AI-04`; no fixture, injection, teleport or save edit in the formal AI steps. |
| R4 formal race detection | Build and run the loader churn/world smoke with Clang ThreadSanitizer on a supported native host. | `bash scripts/verify_tsan.sh` first requires an isolated race probe to produce the expected report/exit 66, then records Xcode's selected compiler and host identity, proves native architecture plus TSan runtime linkage/instrumentation, rejects suppressions, and requires the three V5 checks plus the complete 346/0 world result with zero project reports. |
| R5 clean-root package | Validate a generated Release distribution from an isolated directory, with no access to the source/build tree. Check the packaged inventory, required notices, validation-only startup, real-window startup and negative missing/stale resource cases. | Archive inventory/hash, isolated-root path, positive startup results and distinct negative diagnostics. Generated archives and logs remain untracked. |
| X1-X3 resource packs | Exercise resolver precedence, fallback, version rejection, path-escape rejection, effective manifests and per-resource-class overrides from isolated roots. The effective view is frozen before Ogre construction. | No-pack compatibility, valid-pack Debug/Release startup, missing/stale/duplicate/path-escape failures, base/packed decoded captures, R1 comparison and R5 packaging coverage. |
| K1-K4 durable worlds | Treat catalogue reads as non-mutating, publish saves transactionally, retain bounded verified backups and route create/open/rename/recover/delete through renderer-independent commands. Inject failures at every publication boundary. | Legacy/malformed/path-escape fixtures, preservation/recovery evidence and `AI-01` for the normal world screen. |
| H1-H3 local crash diagnostics | Generate one local Windows minidump for a controlled Release crash, pair it with versioned sanitized context, preserve offline symbols separately and exercise the same path from a clean package. No upload or telemetry is allowed in this milestone. | Non-empty dump, exact build identity, no personal-path leakage, at least one symbolized HelloMine3D frame, no dump on ordinary exits, deterministic package inventory without symbols, and a successful next-start local recovery prompt. |
| Q1-Q3 expanded budgets | Version scene identity and required metrics for cold start, world entry, save, restore, fast streaming and scaled gameplay. Keep R1 frame thresholds and R2 long-running invariants; add non-zero gates only after current Release baselines and thresholds are approved. | Pass/regression/invalid/incomparable fixtures for every metric; compatible before/after summaries; explicit startup/save/restore and chunk-visible budgets; scale/cap evidence; and a repeated formal soak after persistence/gameplay changes. |
| G1-G6 progression slice | Cover strict recipes, atomic crafting, workbench focus, tool class/tier/speed/durability/drop policy, settings persistence, bounded audio events and the clean-package stage-8 player journey. | Debug/Release state-conservation/persistence, assets, Q comparisons, deterministic package, save-failure recovery, H diagnostics and `AI-08` normal gameplay. |

### Stage 8 Planned Evidence

K/H/Q/G rows above define the stage-level acceptance contract. K1-K4、G1-G6、
H1-H3 和 Q1-Q3 已完成；目标 Windows 的六类 RC Release 基线/复测、nominal/stress
各 1800 秒正式长稳、离线符号归档、下次启动提示和干净包受控崩溃已有证据；剩余任务
状态以 `docs/current/todolist.md` 为准。
New harnesses must first prove their own positive, negative, invalid and
incomparable fixtures before their output can be added to `Current Verified
Runs`. The automated Stage-8 result remains frozen; real-window functional
coverage now belongs to the AI scenarios and stays `NOT_RUN` until executed.

### Historical R3 / Physical Input v2 Records

Protocol v1 is defined in `docs/archive/manual-input-acceptance-v1.md`; its exact
machine-checkable skeleton is `docs/archive/manual-input-record-v1.template.txt`.
The twelve ordered cases cover focus recovery, WASD/sprint, two-axis mouse
look and its `L` toggle, flight/sneak/vertical controls, number and physical
wheel hotbar selection, break, attack, place, container use/transfers,
button/Escape container close and final application/window close. The client
now maps the physical OIS wheel delta into the same bounded hotbar selection
path as the already-tested keyboard delta.

Records are key/value text with protocol version, date, exact commit,
configuration, GPU/driver, window mode/size, operator, every case result,
overall result and deviations. Validate a real run with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_manual_input_record.ps1 -RecordPath <record.txt> -RequirePass
```

The validator rejects missing/duplicate/unknown fields, invalid metadata,
unknown case results and any claimed overall pass with a non-pass case or
deviation. A future human-operated `-RequirePass` record would still prove the
twelve v1 cases only.

P11A now defines Physical Input v2 in `docs/archive/physical-input-acceptance-v2.md`,
with the exact skeleton `docs/archive/physical-input-record-v2.template.txt` and validator
`tools/validate_physical_input_v2_record.ps1`. Its thirteen ordered cases add
input arbitration, hold/toggle modes, sensitivity/inversion, pause, Alt+Tab,
minimize/restore, settings restart, death/respawn and the crop/save/reload journey.
`scripts\verify_build.ps1` validates both tracked `NOT_RUN` templates. No
automation or Computer Use run may promote either physical template to `PASS`.

As of 2026-08-25 the operator had reported only partial informal self-testing
(items 1 and 8 plus block breaking/pickup). No full record was produced. Under
the 2026-08-31 Architecture Lab policy, both physical protocols remain
`NOT_RUN/SUPERSEDED` as current gates; their functional subset is mapped to
`AI-01..AI-04`, while physical feel is `NOT_CLAIMED`.

## Current Verified Runs

| Layer | Command | Result |
| ----- | ------- | ------ |
| Headless test targets | The thirteen `HelloMine3D*Tests.exe` / `*Smoke.exe` / `*Soak.exe` binaries enumerated by `scripts\verify_build.ps1` | All thirteen pass in Windows Debug and Release. The current world target reports 596/0, catalogue 48/0, recipe/enemy 100/0 and resource-pack 28/0; the remaining targets, hidden clients, crash diagnostics and package gate also pass (2026-08-25). |
| World runtime smoke | `bin\HelloMine3DWorldRuntimeSmoke.exe` | `checks=596 failures=0`; N8A adds sixteen readable-melee, guard, feedback, target-safety, reload and work-budget assertions on top of the N7A/N7B outcome and encounter stack, with the earlier N1-N6, FS1-FS3 and core regression cases remaining green (Windows Debug/Release, 2026-08-25). |
| R4 native ThreadSanitizer | `bash scripts/verify_tsan.sh`; `docs/current/thread-sanitizer-validation.md` | Apple M1 Pro native arm64, Xcode 26.2 and Apple Clang 17 first detect the isolated calibration race with exit 66, then build the real Debug world target with `libclang_rt.tsan_osx_dynamic.dylib` plus verified read/write instrumentation symbols. The complete world stack passes 346/0, all three V5 concurrent-loader checks pass, no first-party suppression is present and TSan reports zero project races. Evidence: `build/tsan-validation-20260817083434` (2026-08-17). |
| K1/K4 world catalogue and management | `bin\HelloMine3DWorldCatalogueSmoke.exe`; `docs/contracts/world-catalogue-contract-v1.md`; `docs/contracts/world-management-contract-v1.md` | The current 53 checks cover the K1/K4/Q2 catalogue and command boundary, including save-v10 difficulty identity and rejection of invalid profile versions/ids or v9 fields. Debug/Release pass on Windows; malformed future versions and identity/path failures remain explicit (2026-08-25). |
| K2 storage transactions | `bin\HelloMine3DStorageTransactionSmoke.exe`; `docs/contracts/storage-transaction-contract-v1.md` | Sixteen focused assertions cover initial and replacement generations, the five deterministic failures for both world and chunk files, validator rejection and stale-pending quarantine. Every failure leaves the published generation byte-stable/loadable, removes `.pending` and bounds diagnostics to `.failed`; success proves durable flush, real-loader validation, atomic publication, bytes and elapsed duration. Existing world/chunk compatibility fixtures and the 346-check real world stack pass in both configurations (2026-08-17). |
| K3 world backup and restore | `bin\HelloMine3DWorldBackupSmoke.exe`; `docs/contracts/world-backup-contract-v1.md` | Nineteen focused assertions cover the original strict manifest/fingerprint, validation, policy, rotation, quarantine, interruption, rollback, legacy and complete-state recovery cases plus Q2 complete success/failure timings. The selected backup remains untouched and the real world save path publishes one validated snapshot (2026-08-17). |
| A1 asset references | `bash scripts/check_assets.sh` | The repository passes 45 current block/shape/shader/texture/font/recipe/config checks after adding the G1 base recipe directory/file. An isolated copy with `HelloMine3DTerrain.vert` omitted returns 1 and names the missing referenced shader (current positive run 2026-08-17). |
| A2 block diagnostics | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseBlockDataDiagnostics`) | Five malformed fixtures verify missing `ShaderType`, invalid `MeshType`, atlas coordinate `16 0`, light level 16, and duplicate id 3 all report the full source path and exact key. Debug/Release pass (2026-08-12). |
| A3 runtime config ownership | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseRuntimeConfigOwnership`) | Three assertions delete an isolated `config.txt`, verify regeneration with the documented defaults, then load customised values. `Mine.cfg` and `MineResources.cfg` remain the only tracked `bin/` templates (2026-08-09). |
| A4 configured world seed | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseConfiguredWorldSeed`) | Three assertions load seed `20260811` from an isolated config, create two fresh worlds without `HELLOMINE3D_SEED`, and compare 2,299 terrain samples with zero mismatches. Debug/Release pass (2026-08-09). |
| B5 capture polling race | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_render_capture.ps1 -ValidateCapturePolling`; ten real captures plus `ffmpeg -v error -i <png> -f null -` | Rejects a truncated non-empty PNG, then ten writers transition from partial data to a structurally complete PNG and finish with `runs=10 status=PASS`. Ten GTX 1050 Ti / OpenGL 4.6 captures also pass the script and independent decoder validation (`46,343-49,604` bytes). The poller requires a valid signature, bounded chunks and terminal `IEND`; relative paths resolve from the repository root and the native process handle is primed before fast exit (2026-08-12). |
| B3 Xcode generation preflight | `python3 tools/validate_xcode_project_graph.py --self-test --build-dir build`; `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_xcode_generation.ps1` | One version-1 JSON contract fixes the exact 31-project workspace and on-disk inventory. Nine portable fixtures cover the valid graph, invalid contract count/path, missing workspace membership, an unexpected stale project, duplicate `ProjectRef`, duplicate group child, multi-group membership and an undeclared project reference. The actual macOS graph passes with 354 groups, 2,578 unique memberships and 54 cross-project references. PowerShell consumes the same inventory and mirrors the PBX checks, but its updated native run remains pending on target Windows (2026-08-17). |
| B3 native Xcode gate | `bash scripts/verify_xcode.sh` on macOS | Before compiling, the gate requires all nine graph fixtures and the generated 31-project graph to pass. It builds the client and thirteen tests in Debug/Release, performs twenty-six test executions, two validation-only bootstraps and two real Cocoa 120-frame launches. Both world runs report 346/0, K1 reports 30/0, K2 reports 16/0, K3 reports 19/0, Q2 reports 12/0 and H1 reports 12/0; both window probes retained `Y=66` and valid startup/world-entry summaries. Parallel target builds reject duplicate-reference, manual-order and first-party compiler warnings. The complete H1 portable run passed under `build/xcode-validation-20260817075257` (2026-08-17). |
| Q1 optional Tracy client | `./xcode.sh --with-tracy`; Debug/Release `xcodebuild`; validation-only and three-frame Cocoa probes; regenerate with `./xcode.sh` | Vendored Tracy 0.13.1 compiles and links on Apple M1 Pro x86_64/Rosetta in both configurations. Enabled startup reports `enabled=1 on_demand=1`; validation-only and the real frame loop exit 0. Regenerated default projects contain the Tracy source/link boundary but no enable macros; both default configurations build and validation reports `enabled=0` with the same GL3Plus checks (2026-08-16). |
| W2 Windows startup errors | `tools\validate_startup_errors.ps1`; also part of `scripts\verify_build.ps1` after each client configuration | Three isolated roots omit the required terrain shader, atlas texture or stone block definition. Every client returns 1, names the category and exact resource in stderr, and writes the identical user-facing payload with `ui=MessageBoxW` and `dialog_requested=true`; automation suppresses the modal call after recording it. Ordinary Windows startup calls `MessageBoxW`, while validate-only runs remain non-interactive. |
| W3 generated resource manifest | `tools\generate_resource_manifest.ps1 -Check`; `tools\validate_resource_manifest.ps1`; Debug/Release validation-only client runs | G5 adds one base audio source, bringing the checked-in manifest to 40 sorted unique entries: 19 blocks, one shape, 10 shaders, one texture, one font, one base recipe, one base tool source, one base audio source, two Ogre resource scripts and three runtime templates. Generator check plus missing/stale negatives and hidden startup pass. |
| W4 terrain buffer measurement | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseTerrainBufferMetrics`); Debug/Release validation-only client; debug panel and performance CSV/summary | Compile-time and two `W4/*` checks fix the current terrain layout at 32 bytes per vertex and four bytes per index. The deterministic client fixture measures 27,176 vertices, 40,764 indices and a 1,032,688-byte (0.985 MiB) payload. The archived L4 cumulative geometry gives a conservative 12.456 MiB upper bound and already sustained 60.10 FPS / 17.54 ms P95, so the layout remains uncompressed. Live runs now report current resident counts and bytes rather than relying on cumulative rebuild counters. Debug/Release pass with `checks=255 failures=0` (2026-08-12). |
| L1 sunlight storage | `bin\HelloMine3DWorldRuntimeSmoke.exe`; hardware render capture; `tools\run_perf_baseline.ps1` | Nine L1 assertions pass in Debug and Release. `bin/render_capture_l1_surface_20260812/new_01500ms.png` independently decodes and shows bright open terrain against darker opaque occlusion on a GTX 1050 Ti / OpenGL 4.6. The 10-second performance run records 601 frames, 60.14 FPS, 17.75 ms frame P95, no frames over 33 ms, and 0.466/1.363 ms average/max section mesh build time (2026-08-12). |
| L2 block-light storage | `bin\HelloMine3DWorldRuntimeSmoke.exe`; `tools\run_render_capture.ps1`; `tools\run_perf_baseline.ps1` | Seven L2 assertions plus one A2 range fixture pass in Debug and Release. The hardware PNG independently decodes. The matching 10-second run records 601 frames, 60.14 FPS, 17.69 ms frame P95, no frames over 33 ms, and 0.467/0.988 ms average/max mesh build time; geometry counts match L1 exactly (2026-08-12). |
| L3 local relight | `bin\HelloMine3DWorldRuntimeSmoke.exe`; `tools\run_render_capture.ps1`; `tools\run_perf_baseline.ps1` | Nine L3 assertions pass in Debug and Release, including cross-chunk edit propagation, opaque add/remove, single-column sunlight, bounded mesh invalidation, reload reconciliation and unload cleanup. The PNG independently decodes. The 10-second run records 601 frames, 60.09 FPS, 17.58 ms frame P95, no frames over 33 ms, and 0.477/0.998 ms average/max mesh build time (2026-08-12). |
| L4 transparent rules | `bin\HelloMine3DWorldRuntimeSmoke.exe`; `HELLOMINE3D_TRANSPARENT_FIXTURE=1` Ogre validation/capture; `tools\run_perf_baseline.ps1` | Ten L4 assertions pass in Debug and Release. Ogre validation reports one transparent section with 68 vertices/102 indices; `bin/render_capture_l4_20260812_full/new_05000ms.png` shows the deterministic glass/leaf/flora fixture and independently decodes. The 10-second run records 601 frames, 60.10 FPS, 17.54 ms frame P95, no frames over 33 ms, and 0.480/1.169 ms average/max mesh build time (2026-08-12). |
| C1 block behavior | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseBlockBehaviorDispatch`) | Four C1 assertions verify default and special drop dispatch plus the real no-drop glass break path. The registry owns behavior lifetime and the interaction system has no glass-specific branch (Debug and Release, 2026-08-12). |
| C2 metadata behavior | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseMetadataBackedBehavior`) | Four C2 assertions verify one `TallGrass` id selects different drops by maturity metadata, natural biome grass is mature, and both states behave correctly through the real break path (Debug and Release, 2026-08-12). |
| C3 resource shapes | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseResourceDrivenBlockShapes`); asset reference check | Four C3 assertions verify shared shape resolution, exact resource vertices, isolated shape extensibility and real flora mesh output. The asset equivalent reports 45/45 references present; `ChunkMeshBuilder` contains no crossed-quad geometry (Debug and Release, 2026-08-12). |
| C4 cave generation | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseTerrainDeterminism`) | Three C4 assertions find 3,766 underground air cells across nine chunks, reproduce their exact indices with the same seed, and observe zero air cells in the protected surface layers. Existing ore, biome and structure checks also pass (Debug and Release, 2026-08-12). |
| C5 cross-chunk structures | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseTerrainStructures`) | Two C5 assertions find 100 tree-block links across internal chunk boundaries and compare all 20,480 blocks in each of two adjacent chunks after forward and reverse generation order. The save/reload check also preserves all 100 links (Debug and Release, 2026-08-12). |
| C6 mob chase and damage immunity | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseActors`); `bin\HelloMine3DEntityLifecycleSmoke.exe` | Three C6 assertions move a nearby mob 0.12 blocks toward the player in one fixed tick, reject a second hit without changing health or publishing another damage event, and accept damage again after the 0.5-second immunity expires. The focused lifecycle smoke also rejects an immediate repeated event-bus hit (Debug and Release, 2026-08-12). |
| C7 bounded random ticks | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseRandomTickScheduling`) | Six C7 assertions use deterministic uniform voxel samples to prove per-metadata behavior opt-in, an active-section-only index, three attempts per visited section, a four-section fixed-tick budget, round-robin completion, unload cleanup and storage reload rebuilding. Debug/Release pass with `checks=245 failures=0`; active/processed/dispatched counters are available to the debug panel and performance capture (2026-08-12). |
| W1/FS2 time of day, fog, sky and water | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseWorldEnvironment`, `caseWorldManager`); hidden pinned-time runtime readbacks | Existing W1 anchors, wrapping, daylight/fog and celestial checks remain green. FS2 adds four bounded/wrapping assertions for cloud and water palettes, a dedicated Fresnel/depth/fog water shader, moving stylised clouds, recalibrated fog/sky colours and readable night exposure. Debug/Release pass `checks=536 failures=0`; the Release hidden Ogre client uploads 3 water sections / 1,508 vertices / 2,262 indices and exits 0. The tracked FS2 forest/coast noon/dusk/night GTX 1050 Ti images were produced through true hidden runtime readback on 2026-08-24. |
| E0 dependency boundary | `rg "SFML|sf::|GLfloat|GLuint|glad" src/HelloMine3D/World` | No matches (2026-08-09). |
| E1 engine build | `tools\premake\premake5 --os=windows --file=premake/premake.lua vs2022`, then full Debug/Release solution builds | Ogre 1.10 core, GLSupport, GL3Plus, FreeImage dependency chain, dedicated Ogre FreeType, zlib, zzip and OIS compile in both configurations; hardware render and performance smokes pass on a GTX 1050 Ti / OpenGL 4.6 (2026-08-12). |
| E2 bootstrap and skybox | Validation-only startup plus pinned-time runtime readbacks | Debug and Release register `OpenGL 3+ Rendering Subsystem`, 2 resource locations and OIS. The current shader renders its gradient, sun, moon and stars without sky textures; Apple M1 Pro / OpenGL 4.1 day/night readbacks confirm the active path (2026-08-16). `docs/screenshots/validation-skybox-panel-outline.png` records the retired cube-map path. |
| E3 terrain bridge | Set `HELLOMINE3D_VALIDATE_ONLY=1`, `HELLOMINE3D_SEED=20260809`, `HELLOMINE3D_PLAYER_POSITION=264 96 8`, then run `bin\HelloMine3D.exe` | Debug and Release build real terrain and validate every position, atlas-UV, repeat-UV, light and index stream before GPU upload, including section-local bounds and index ranges. Hardware terrain capture and the L4 performance baseline pass (2026-08-12). |
| E4 water/flora bridge | Use the E3 command | Release validates 20 solid/cutout sections (9,296 vertices), 3 water sections (1,736 vertices), and 12 flora sections (216 vertices). The L4 fixture additionally validates one glass section (68 vertices); every path has valid CPU streams and an explicit render queue (2026-08-12). |
| E4 Ogre diagnostics | Add `HELLO_RENDER_CAPTURE=1`, `HELLO_RENDER_CAPTURE_MS=0,250,1000`, and an isolated `HELLO_RENDER_CAPTURE_DIR` to the E4 validation command | Debug and Release report `capture_config=valid`, `capture_enabled=true`, and `capture_targets=3`. Both scripts target the sole client executable (2026-08-09). |
| E4 Ogre HUD/ImGui | Add `HELLOMINE3D_SHOW_DEBUG_INFO=1`, then repeat with `off` | Debug and Release report `hud_config=valid`, five hotbar slots, selected slot zero, and matching enabled/disabled state. `docs/screenshots/validation-skybox-panel-outline.png` confirms the hardware-rendered HUD and live panel (2026-08-12). |
| E5 single render path | `rg -n "SFML|sf::|glad|imgui.?sfml|HelloMine3DOgreBootstrap" src/HelloMine3D premake tools` plus full Debug/Release builds and all five tests | No old client/render dependencies remain; versioned CPU mesh upload checks pass, `HelloMine3D.exe` is the only client target, and the hardware PNG/CSV smokes pass (2026-08-12). |
| P1 actor bridge | Add `HELLOMINE3D_SPAWN_VALIDATION_ACTORS=1` to a hardware capture, or use `tools\run_render_capture.ps1 -SpawnValidationActors` | Debug/Release validation report `actor_count=2`, `mob_count=1`, `item_count=1`; five P1 snapshot assertions pass. `docs/screenshots/validation-actors.png` shows the green mob and amber item together (2026-08-12). |
| P2 actor persistence | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`casePersistence`) | Current world metadata version 5 retains the version-2 actor payload and version-3 identity alongside objective state, restores a mob and item with subtype fields intact, retains ids, advances the next id, and reads/upgrades a version-1 fixture; eight P2 assertions pass (updated 2026-08-17). |
| M1 mesh build metrics | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseSectionMeshInput`) | Five M1 assertions verify per-build timing and cumulative face/vertex counters; the performance CSV/summary and Ogre debug panel expose the same values (2026-08-09). |
| M2 bounded dirty queue | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseMeshDirtyPropagation`) | Five queued sections rebuild as `2 + 2 + 1`; duplicate edits retain one FIFO entry and empty updates add no rebuilds (2026-08-09). |
| P3 selection outline | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseBlockSelection`); hardware capture | Five assertions cover solid selection, placement adjacency, water skipping, reach and misses. The yellow Ogre outline is visible in `docs/screenshots/validation-skybox-panel-outline.png` (2026-08-12). |
| P4 ore textures | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseOreTextures`); `tools\run_render_capture.ps1 -ShowOreFixture` | Four assertions cover dedicated atlas slots and distinct hashes. `docs/screenshots/validation-ores.png` visibly distinguishes coal and iron in the live world (2026-08-12). |
| FS3 voxel art and HUD | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseOreTextures`); `tools\build_fs3_texture_atlas.ps1`; `tools\run_render_capture.ps1 -ShowHudFixture -WorldTime 6000`; `tools\validate_background_client.ps1` | Three FS3 assertions cover the full material icon map, dedicated chest/workbench/furnace cells and ten populated, distinct item cells. Debug/Release report 539/539, resource packs 27/27 and the generated manifest 47 entries. `validation-fs3-hud-noon.png` shows five icons, selection, counts/durability and a held stone sword; `validation-fs3-container-hud.png` covers container coexistence. The hidden client uses `WS_EX_NOACTIVATE`, creates no visible top-level window and skips OIS capture; the focus guard reports `foreground_owned=false visible_window=false exit_code=0` (2026-08-24). |
| M4 opaque greedy meshing | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseGreedyMeshing`) | Six assertions reduce an `8x1x8` single-material slab from 160 naive faces to 6, preserve material boundaries and atlas repetition, and prove water/flora keep their original topology. Debug/Release validation-only startup reduces the fixed-scene solid vertex count from 20,796 to 8,396 while water/flora remain unchanged; hardware captures retain block-scale texture repetition (2026-08-12). |
| M6 enclosed-section skip | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseEnclosedSectionSkip`) | Nine assertions seal a section with opaque neighbours, prove background and synchronous paths schedule no mesh build or metrics update, then open one top neighbour and require exactly one visible face and one recorded rebuild (2026-08-09). |
| M7 frustum-priority work order | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseFrustumMeshPriority`) | Four assertions retain all 1,089 chunk targets at view distance 16, prioritise the forward half, reverse priority after turning, and preserve distance ordering when no main-thread frustum snapshot is available (2026-08-09). |
| P5 block use interaction | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseInteractionAndEvents`) | Four assertions route use through `BlockInteractionSystem`, publish one `BlockUseEvent` with the target position/id, and keep air use as a no-op. Right-click queues use before the existing adjacent placement action (2026-08-09). |
| Render smoke | see `docs/current/render-regression-smoke.md` | L4 plus tracked skybox/panel/outline, actor and ore captures all pass on OpenGL 4.6; evidence is under `docs/screenshots/validation-*.png`. |
| Performance baseline | see `docs/current/performance-baseline.md`; `tools\capture_release_candidate_performance.ps1` | Alpha 场景保留原批准基线；2026-08-20 又批准六类 `release-candidate-windows-hidden-v1` Release baseline/repeat。冷启动 `1080.027/1136.785 ms`、进入可控 `739.218/804.583 ms`、保存 `195.821/160.612 ms`、恢复 `53.861/58.516 ms`、快速移动可见 P95 `36.147/49.097 ms`、规模场景 P95 `16.558/16.303 ms`，六对均返回 `PASS`。 |
| R1 performance comparison | `tools\validate_perf_comparison.ps1`; also part of `scripts\verify_build.ps1` | Four fixtures distinguish `PASS` (real L4 self-compare), `REGRESSION` (P95/P99/long frames), `INCOMPARABLE` (scene and residency) and `INVALID` (missing metric). The comparator uses exit codes 0/2/3/4 and exact diagnostic reasons (2026-08-13). |
| Q1 schema-3 performance contract | `python3 tools/validate_perf_comparison.py`; `tools\validate_perf_comparison.ps1`; `tools\capture_release_candidate_performance.ps1` | Contract v1 still derives 524 schema-3 cases plus eight legacy cases; PowerShell 同合同通过 37 项，并专门证明不同难度 ID 返回 `INCOMPARABLE`。World-entry/streaming/scaled/playable 场景现把难度 profile/version 纳入身份；历史批准预算不被放宽（2026-08-25）。 |
| Q2 bounded operation timing | `HelloMine3DOperationTimingSmoke`; K1/K3/world smokes; `docs/contracts/operation-performance-timing-contract-v1.md` | Twelve collector assertions cover disabled capture, complete success/failure records and summaries for all six operations, monotonic phases, counters, 32-record ring and overflow。实际 Windows Release 世界保存与备份恢复各采集两次，阶段单调、身份一致且低于 300/500 ms 预算；schema-3 比较全部通过，Q2 为 `Done`（2026-08-20）。 |
| H1 local Windows minidump | `HelloMine3DCrashDiagnosticsSmoke`; `tools\validate_crash_diagnostics.ps1`; `docs/contracts/crash-diagnostics-contract-v1.md` | The Breakpad audit selected the existing Windows SDK DbgHelp boundary and explicitly records Breakpad as not selected. Twelve portable path, overlap, trigger and install assertions pass in both configurations. The target Windows Release harness now runs with a hidden background render window, produces one 117,741-byte dump and a non-zero controlled exit, reopens the published active world, finds zero pending save candidates and performs no upload. H1 is complete (2026-08-17). |
| H2-H3 local diagnostics and recovery | `HelloMine3DCrashDiagnosticsSmoke`; `tools\validate_crash_diagnostics.ps1`; `tools\archive_windows_symbols.ps1`; `docs/contracts/crash-sidecar-contract-v1.md` | Debug/Release 21/21；最终 124,513 字节 Release dump 的模块/线程/内存/异常流被实际读取，混合栈包含受控触发和 `OgreBootstrap::frameEnded` 等项目帧，错误 PDB 返回 3。七项独立符号 ZIP hash 为 `1e614f64e7ac2f8be55d39cf26aa817a14bb1e30fdc9237644cc044df01b2ca3`；干净包崩溃后下次启动恰好发现一份本地报告，可打开/复制/忽略，上传始终关闭（2026-08-20）。 |
| K4 world management and entry | `HelloMine3DWorldCatalogueSmoke`; hidden three-frame menu/direct-world probes; `docs/contracts/world-management-contract-v1.md` | The focused target passes 44 K1/K4/Q2 assertions: structured commands preserve stable ids/directories, accept same display names, upgrade legacy metadata in place, reject corrupt/path-escape input, bound recoverable deletion and roll back interrupted backup restore. The Debug client opens an empty main menu without creating the catalogue, and the save-directory compatibility path loads terrain in a hidden background window. Normal Release interaction is mapped to `AI-01` (2026-08-17 evidence; 2026-08-31 mapping). |
| D1 stateful-block lifecycle | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseBlockEntityLifecycle`); `bin\HelloMine3DSaveLoadSmoke.exe`; full `scripts\verify_build.ps1` | Eleven world assertions cover owned create/find/update/remove, unique positions, invalid type and position rejection, atomic failed loads, negative world-coordinate translation, unload/reload persistence and automatic removal when the block changes. The focused fixture proves version 1 chunks remain readable with empty block-entity state and zero metadata. Debug/Release and every startup/resource/performance fixture pass with `checks=266 failures=0` (2026-08-13). |
| D2 chest container | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseChestContainer`); `tools\run_render_capture.ps1 -ShowContainerFixture`; full `scripts\verify_build.ps1` | Twelve assertions cover P5 placement/use, bounded slots, total-preserving transfers, events, malformed payload rejection, save/reload, input suppression and spill-on-break. Debug/Release pass with `278/278`; the Release readback shows all slots and Close/Escape guidance. Engineering is Done; `AI-02=NOT_RUN` covers normal open/transfer/close/focus (2026-08-13 evidence; 2026-08-31 mapping). |
| D3 natural-mob population | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseNaturalMobPopulation`); fixed-seed `tools\run_render_capture.ps1`; `tools\run_perf_baseline.ps1`; R1 comparison; full `scripts\verify_build.ps1` | Thirteen assertions cover deterministic bounded candidates, safe ground/headroom, local/world caps, chunk unload/reload, counters, save restoration and spatial duplicate rejection. Debug/Release pass with `checks=291 failures=0`. `validation-natural-mobs.png` is a 1584x861 GTX 1050 Ti / OpenGL 4.6 Release readback from the normal simulation path: the green Mob is visible and the panel reports four actors, `4 / 12` natural Mobs and four additions. The matching 60.1 FPS sample records P95/P99 `16.693/17.633 ms`, zero >33/50 ms frames and R1 `PASS` (2026-08-13). |
| D4 combat, death and respawn | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseCombatAndRespawn`); `tools\run_render_capture.ps1 -ShowCombatFixture`; same-session D3/D4 performance A/B; full `scripts\verify_build.ps1` | Fourteen assertions cover target/occlusion, damage immunity, cooldown, Mob death/drop, player death/respawn, velocity/inventory/container/HUD state. Debug/Release pass with `305/305`; the Release combat readback and R1 comparison pass. Engineering is Done; `AI-03=NOT_RUN` covers normal attack/damage/death/respawn (2026-08-13 evidence; 2026-08-31 mapping). |
| D5 wheat crop loop | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`caseWheatCropLoop`); `tools\run_render_capture.ps1 -ShowCropFixture`; generated resource manifest; same-session D4/D5 performance A/B; full `scripts\verify_build.ps1` | Thirteen assertions cover block/material registration, metadata-driven render height, initial seed acquisition, rejected support without consumption, accepted planting on dirt, one-stage random-tick progress, bounded maturity, immature/mature drops, harvest/replant, unloaded-section exclusion and stage persistence. Debug/Release pass with `checks=318 failures=0`; the asset gate reports 41 manifest entries. `validation-wheat-crop.png` is a decoded 1584x861 GTX 1050 Ti / OpenGL 4.6 Release readback showing the four fixed metadata stages from 25% to 100% height, while the panel reports three immature crops in the random-tick index. The same-session D4 control and D5 candidate used identical Release scene/residency and actual uncapped pacing; both had zero >50 ms frames and R1 reports `PASS` (2026-08-13). |
| D6 playable vertical slice | `bin\HelloMine3DWorldRuntimeSmoke.exe` (`casePlayableVerticalSlice`); `tools\run_render_capture.ps1 -ShowVerticalSliceFixture`; R1 comparison | Nine assertions drive crop, chest, population, combat, loot, replanting, save and relaunch through gameplay APIs after fixture creation; world checks, readback and R1 pass. Engineering is Done; `AI-04=NOT_RUN` must repeat the journey from a clean package without fixture/injection/save edits (2026-08-17 evidence; 2026-08-31 mapping). |
| R2 deterministic world soak | `tools\run_world_soak.ps1 -DurationSeconds 1800 -Seed 20260813 -Formal` | Accepted Release schedule-v1 run completes 36,000 fixed ticks, 361 load-centre moves, 1,800 block edits, 900 actor cycles and 179 save/reloads with zero failures. Peak private/working memory is `22,790,144/27,377,664` bytes, peak handles/threads `234/4`, steady growth `4,845,568 bytes/1 handle`; no timeout and both child/wrapper summaries report `PASS` (2026-08-13). |
| Q3 release-candidate scale soak | `tools\run_release_candidate_soak.ps1`; `docs\contracts\q3-scale-soak-contract-v1.md` | BETA-RC Release schedule-v2 nominal/stress each completes 1,800 seconds and 36,000 fixed ticks with zero failures. Nominal records 361 moves, 1,800 edits, 900 actor cycles and 179 save/reloads；stress records 901/7,200/1,800/359。Peak private bytes are `19,623,936/26,513,408`, steady growth `2,236,416/6,799,360`, peak handles/threads are `251/4` and `252/4`, steady handle growth is zero, and all child/wrapper summaries plus the tracked manifest report `PASS`（2026-08-26）。 |
| R3 / Physical Input v2 protocol（历史） | `tools\validate_manual_input_record.ps1 -AllowNotRun`; `tools\validate_physical_input_v2_record.ps1 -AllowNotRun` | v1 的 12 项与 v2 的 13 项模板通过严格 schema 校验；没有产生物理 PASS，模板保持 `NOT_RUN`。2026-08-31 起它们 `SUPERSEDED` 为当前门槛，功能范围由 `AI-01..AI-04` 承接。 |
| R5 Windows package | `tools\package_windows_release.ps1 -IncludePack example-stone`; full `scripts\verify_build.ps1` | The isolated 66-file distribution passes exact inventory, hidden validation-only startup, missing-shader and stale-extra-resource negatives, controlled package crash and next-start report discovery. It excludes symbols, crashes, saves, logs and tests；the deterministic ZIP hashes to `AB5E07D151E8C9973815F5969AA281CE7CBD57413A446825CC9FC12C04AAD81C`（2026-08-20）。 |
| X1-X3 resource packs | `HelloMine3DResourcePackSmoke.exe`; `tools\validate_resource_packs.ps1`; packaged `example-stone` | Twenty-seven resolver assertions pass, including the rules that resource-pack v1 rejects recipe, tool, audio, objective, smelting, food and enemy ownership while missing base audio remains non-fatal. The current no-pack client freezes 46 base entries；the accepted 66-file package enables `example-stone` without weakening the base manifest（2026-08-20）。 |
| G1-G3 recipe, crafting and tools | `HelloMine3DRecipeSmoke`; `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DResourcePackSmoke`; hidden crafting client | The focused target passes 64 assertions: 40 strict recipe cases, 14 G2 crafting cases and 10 G3 tool/crafting/durability cases. The world stack passes 365/0 and the resource-pack target 20/0. Debug and Release clients each freeze 39 resources, five base recipes and two tools in a hidden background window and exit 0. Physical hold/focus evidence remains in R3/G6; the contract is `docs/contracts/tool-progression-contract-v1.md` (updated 2026-08-17). |
| G4 pause and settings | `HelloMine3DWorldRuntimeSmoke`; hidden Debug/Release client; `docs/contracts/runtime-settings-contract-v1.md` | The world stack now passes 386/0. Twenty-one G4 assertions cover version-1 generation, legacy migration, bounded defaults/drafts, cancel, restart classification, invalid ranges, atomic publication failure, seed preservation, live logical-camera FOV/world-distance updates and the `Playing`-only simulation gate. Debug and Release clients load/migrate settings, initialize the pause/settings UI and exit 0 in a hidden background window. Physical focus and visual inspection remain assigned to R3 (updated 2026-08-17). |
| G5 basic audio feedback | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DResourcePackSmoke`; manifest validators; hidden Debug/Release clients; `docs/contracts/audio-feedback-contract-v1.md` | Seventeen G5 world assertions raise the stack to 403/0; resource-pack coverage is 23/0 and the manifest has 40 entries with three positive/negative cases. Seven cues cover UI, break/place, pickup, craft, hit and ambient feedback; failed actions stay silent, volume/pause/mute/concurrency/lifecycle rules hold, and missing definitions/devices select dummy without blocking startup. Both configurations build and complete validation-only plus real three-frame hidden launches with exit 0 (2026-08-17). |
| G6 playable Alpha journey | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DWorldCatalogueSmoke`; hidden Debug/Release clients; `docs/contracts/playable-alpha-contract-v1.md` | One migration check plus 16 journey checks raise the world stack to 420/0. The fixture uses only initial resource placement, then drives real mining, crafting, workbench, tool durability/tier, natural population, combat, pickup, management, save and reopen APIs. Version 4 persists ten bounded flags; versions 1-3 default to an empty journey and migrate, while unknown bits preserve the last good save. The catalogue target passes 45/0 and storage/backup remain 16/0 and 19/0 (2026-08-17). |
| N1 objective and first-run guidance | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DResourcePackSmoke`; manifest/startup validators; `docs/contracts/objective-system-contract-v1.md` | Nine N1 world assertions raise the stack to 429/0. The strict registry freezes 11 base definitions across eight types; event progress, read-only snapshots, unknown ids, no-reward completion and true reopen semantics pass. Version 5 persists bounded completion/progress records, versions 1-3 migrate empty and version 4 maps its ten flags. Resource-pack coverage is 24/0 and the base manifest has 41 entries (2026-08-17). |
| N2 furnace, smelting and iron progression | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DRecipeSmoke`; `HelloMine3DResourcePackSmoke`; manifest/startup validators; `docs/contracts/smelting-progression-contract-v1.md` | N2 raises the world stack to 447/0. Strict base-only smelting, three dedicated slots, exact fixed-tick progress, unload/reload without catch-up, output/fuel pause rules, payload rejection, atomic transfer, break spill and real iron-sword damage/durability pass. Recipe/tool coverage is 65/0, resource-pack coverage is 25/0, the base manifest has 43 entries, and the objective registry freezes 16 definitions across nine types (2026-08-17). |
| N3 food and recovery | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DRecipeSmoke`; `HelloMine3DResourcePackSmoke`; manifest/startup validators; `docs/contracts/food-recovery-contract-v1.md` | N3 raises the world stack to 463/0. Strict base-only food definitions, three-wheat bread, successful-only atomic consumption, bounded recovery, exact fixed-tick cooldown, dead/full/paused/UI rejection, next-tick respawn, v6 health/cooldown persistence, v5 objective-preserving migration and consume-item objectives pass. Recipe/food coverage is 76/0, resource-pack coverage is 26/0, the base manifest has 44 entries, and the objective registry freezes 18 definitions across ten types (2026-08-17). |
| N4 combat, enemies and loot | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DRecipeSmoke`; `HelloMine3DResourcePackSmoke`; full `scripts\verify_build.ps1`; `docs/contracts/combat-depth-contract-v1.md` | N4 raises the world stack to 477/0. Strict base-owned Stalker/Brute definitions, three sword tiers, AABB reach, fixed-tick cooldown, successful-hit-only durability, exact-once bounded loot, contact-damage differences, three objectives and v6→v7 migration pass. Recipe/enemy/tool coverage is 92/0, resource-pack coverage is 27/0, the base manifest has 45 entries, and the objective registry freezes 21 definitions. Debug and Release full rebuilds complete with zero errors; all thirteen test targets, hidden clients, seven startup negatives, resource packs, controlled dump, executable inventory and the 65-file clean-root package pass (2026-08-20). |
| N5 ecology and exploration | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DRecipeSmoke`; `HelloMine3DResourcePackSmoke`; full `scripts\verify_build.ps1`; `docs/contracts/ecology-exploration-contract-v1.md` | N5 raises the world stack to 487/0. Terrain v2 adds one bounded, seed-stable cross-chunk Waystone per eligible 4×4-chunk cell, while v1-v7 worlds migrate to save v8 but retain terrain v1 and never gain the landmark in ungenerated chunks. Five biome identities, Desert→Brute pressure, load-order independence, one tier-3 luminous core, two exploration objectives and invalid-version atomicity pass. Recipe/material/enemy coverage remains 92/0, resource-pack coverage is 27/0, the base manifest has 46 entries, the objective registry freezes 23 definitions, and all 36 performance comparison fixtures pass with terrain generation version as an identity field. Debug and Release full rebuilds complete with zero errors; all thirteen test targets, hidden clients, seven startup negatives, resource packs, controlled dump, executable inventory and the 66-file clean-root package pass (2026-08-20). |
| N6 product experience | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DRecipeSmoke`; `HelloMine3DResourcePackSmoke`; full `scripts\verify_build.ps1`; `docs/contracts/product-experience-contract-v1.md` | N6 raises the world stack to 491/0 and recipe/crafting coverage to 95/0 while resource-pack coverage remains 27/0. Settings v2 migrates versions 0/1 atomically, validates nine remappable actions, UI scaling, captions and hints; audio definition v2 supplies seven bounded captions; food outcomes, ordered objective history and inventory-neutral 2×2/3×3 recipe loading pass. The base manifest remains 46 entries and all 36 performance fixtures pass. Debug and Release full rebuilds finish with zero errors; all thirteen tests, hidden clients, seven startup negatives, the 124,513-byte controlled dump, executable inventory and isolated 66-file package pass. The package archive hashes to `AB5E07D151E8C9973815F5969AA281CE7CBD57413A446825CC9FC12C04AAD81C`. R3's 12-case schema passes but the human physical-input run remains deliberately `NOT_RUN` (2026-08-20). |
| FS1 first-world entry | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DWorldCatalogueSmoke`; hidden Release validation/30-frame client; `docs/contracts/first-session-visual-baseline-contract-v1.md` | Debug/Release world runs pass 532/0, including 40 FS1 checks; both catalogue runs pass 45/0. Seed 0 repeats at `296.5 82 40.5`, seed 20260807 starts at `168.5 69 264.5`; all samples have solid ground, two-block clearance, non-ocean biome, nearby oak, persisted reopen and unchanged terrain identity. The bounded legacy rescue keeps dropped items and ignores progressed saves. A real Release validation client initializes an isolated seed-0 save in 0.508 s, then the Ogre window uploads solid/water/flora terrain for 30 hidden frames and exits 0 (2026-08-21). |

| N7A world outcome and localized text foundation | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DWorldCatalogueSmoke`; `HelloMine3DResourcePackSmoke`; full `scripts\verify_build.ps1`; `docs\contracts\waystone-victory-contract-v1.md` | Fifteen N7A assertions raise the Debug/Release world stack to 560/0: ordered and idempotent outcome transitions, reward epochs, invalid restore, v9 round trip/last-good preservation, v1-v8 migration, locale key parity, exact lookup, bounded fallback diagnostics and atomic invalid-text rejection all pass. Catalogue coverage is 48/0 and projects completion only from persisted victorious/claimed phases; resource-pack coverage is 28/0 and rejects unversioned text ownership. The base manifest has 49 entries. Full Windows verification passes 36 performance fixtures, thirteen test targets in both configurations, hidden clients, eight startup negatives, a 135,993-byte controlled dump, executable inventory and an isolated 69-file package (2026-08-25). |
| N7B Waystone victory loop | `HelloMine3DWorldRuntimeSmoke`; VS2017 v141 client/target builds; full `scripts\verify_build.ps1`; short nominal/stress `HelloMine3DSoak`; `docs\contracts\waystone-victory-contract-v1.md` | Twenty N7B assertions raise the Debug/Release world stack to 580/0. They cover strict block-entity state, revision-guarded activation, five event-driven finale objectives, bounded 2+1 guardian waves, phase reopen, death reset, duplicate death suppression, chunk unload/reload, pending full-inventory reward, one-time claim, continued sandbox play and mid-encounter backup restore. VS2017 Debug/Release client and Release targeted builds pass. Fixed-seed nominal/stress profiles each pass for five seconds with zero failures. The full gate passes 49 resources, 36 performance fixtures, all thirteen targets and both hidden clients, eight startup negatives, a 131,817-byte dump and an isolated 69-file package with SHA-256 `9F52C3C9F6F9DE60F85D7864ED707A60FD358B15E062CF9E85807ABA8494BDD2` (2026-08-25). |
| N8A readable melee encounters | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DRecipeSmoke`; VS2017 v141 client/target builds; full `scripts\verify_build.ps1`; short nominal/stress `HelloMine3DSoak`; `docs\contracts\combat-encounter-contract-v1.md` | Sixteen N8A world assertions raise the Debug/Release stack to 596/0. Enemy registry v2, explicit Idle/Chase/Windup/Recover transitions, exact-once telegraph/resolve, target escape, occlusion, four-way damage feedback, front-only weapon guard, guard recovery/durability, both knockback paths, hit interruption, transient reload reset and per-tick 16-ray/32-chase budgets pass. Recipe/enemy coverage is 100/0 and resource-pack coverage remains 28/0. VS2017 v141 Debug/Release client and related targets pass. Fixed-seed 20260825 nominal/stress profiles each complete five seconds/100 ticks with zero failures. The full gate passes 49 resources, 36 performance fixtures, all thirteen targets and hidden clients, eight startup negatives, a 130,721-byte dump and an isolated 69-file package with SHA-256 `2A54DCDEB2CE5C5C320B0095C5CEC2213AE461548C8A2DBFD822CC897D57E919` (2026-08-25). |
| N8B ranged enemy and transient projectiles | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DRecipeSmoke`; `HelloMine3DResourcePackSmoke`; VS2017 v141 client/target builds; full `scripts\verify_build.ps1`; 60-second nominal/stress `HelloMine3DSoak`; `docs\contracts\combat-encounter-contract-v1.md` | Fifteen new projectile behavior assertions plus the natural-archetype update raise the Debug/Release world stack to 611/0. Registry v3 strict parsing and the formal base resource raise recipe/enemy coverage to 104/0; resource-pack coverage remains 28/0. The natural Spitter, exact launch/hit/knockback, continuous wall/player collision, front guard, global/type/local capacity, 32-step tick budget, target/owner/chunk/player cleanup, independent lifetime/distance/active-area expiry and save exclusion all pass. VS2017 v141 Debug/Release client and related targets pass; fixed-seed 20260825 nominal/stress each complete 60 seconds with zero failures. The full gate passes 49 resources, 36 performance fixtures, all thirteen targets and hidden clients, eight startup negatives, a 129,201-byte dump and an isolated 69-file package with SHA-256 `724D1C06CF32EF3805131F814167ECB9296392A8228520D5B45AD79FD308FBFA`. R3 remains partially human-tested with the rest deferred, not automatically passed (2026-08-25). |
| N9A deterministic structure planning | `HelloMine3DWorldRuntimeSmoke`; VS2017 v141 client/target builds; full `scripts\verify_build.ps1`; Q1 fast-streaming comparison; 60-second nominal/stress `HelloMine3DSoak`; `docs\contracts\exploration-structures-contract-v1.md` | Sixteen N9A assertions raise the Debug/Release world stack to 627/0. They freeze the terrain-v2 Waystone identity and output while covering seed/version/thread determinism, floor ownership at negative coordinates, footprint and plan bounds, global-random isolation, conflict priority and stable tie-breaking, no neighbor loads, cross-chunk discovery, projection load-order independence and one core. VS2017/v141 Debug/Release client and related targets pass. The same-identity 30-second Q1 comparison passes; fixed-seed 20260825 nominal/stress each complete 60 seconds/1200 ticks with zero failures. The full gate passes 49 resources, 36 fixtures, thirteen targets and hidden clients, eight startup negatives, a 131,929-byte dump and an isolated 69-file package with SHA-256 `36FF6C813463046EB0FA980DA1C0657E4C4BFBC98D16208DBD442E51CAF341E8`. R3 remains partially human-tested with the rest deferred (2026-08-25). |
| N9B ruins, camps and persistent loot | `HelloMine3DWorldRuntimeSmoke`; VS2017 v141 client/target builds; full `scripts\verify_build.ps1`; same-identity terrain-v3 Q1 fast-streaming comparison; 60-second nominal/stress `HelloMine3DSoak`; `docs\contracts\exploration-structures-contract-v1.md` | Eighteen N9B assertions raise the Debug/Release world stack to 645/0. Terrain v3 selects one of Waystone/Ruin/RaiderCamp per 4×4-chunk cell; forest ruins and Desert/Grassland camps use bounded footprints, exact load-order-independent projection and one standard persistent chest. Fixed seed 20260807 freezes anchors `(222,70,400)` / `(436,102,37)` and loot amounts `1/4/4` / `3/6/2`. Emptying loot, depositing player items, damaging a structure and reopening preserve authored state; terrain v1/v2 identities never upgrade or backfill. VS2017/v141 Debug/Release client and target builds pass. Two 30-second terrain-v3 Q1 runs compare PASS; fixed-seed 20260825 nominal/stress each complete 60 seconds/1200 ticks with zero failures. The full gate passes 49 resources, 36 fixtures, thirteen targets and hidden clients, eight startup negatives, a 127,129-byte dump and an isolated 69-file package with SHA-256 `D3E51E95559333D2E412521522BA2A2052659EB766572F1028A47B02BAE7A380`. R3 remains partially human-tested with the rest deferred (2026-08-25). |
| N10 food, smelting and resource economy | `HelloMine3DRecipeSmoke`; `HelloMine3DWorldRuntimeSmoke`; VS2017 v141 client/target builds; full `scripts\verify_build.ps1`; same-identity N10 scaled Q1 comparison; 60-second nominal/stress `HelloMine3DSoak`; `docs\contracts\resource-economy-contract-v1.md` | Four foods, three smelts, two fuels and nineteen exact recipes pass the version-1 economy reachability/source/sink/cycle verifier. Debug/Release VS2017/v141 recipe and world targets report 117/0 and 650/0; five N10 world assertions cover food/smelting/fuel behavior, v9 material round-trip, 5-slot/495-item capacity and bounded save size/time, while FS3 proves fifteen unique visible icons. The full gate passes 49 resources, 36 fixtures, thirteen targets and hidden clients, eight startup negatives, a 131,009-byte dump and an isolated 69-file package with SHA-256 `A39BFAE92E25775A461DD7251A9D48DF15067812287650CCE65E72EA448050AD`; the symbol archive hashes to `34ebbe24d74572b471237243f32b5e3cdd85e943a3106a5d5eab75b3e6a947c0`. Two 30-second scaled runs compare PASS at P95 `2.631/2.707 ms`; fixed-seed 20260825 nominal/stress each complete 60 seconds/1200 ticks with zero failures. R3 is only partially human-tested, with the rest explicitly deferred; it is not marked PASS and does not block N11-N12 (2026-08-25). |
| N11A versioned difficulty profiles | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DWorldCatalogueSmoke`; VS2017 v141 client/target builds; full `scripts\verify_build.ps1`; same-identity scaled Q1; 60-second nominal/stress `HelloMine3DSoak`; `docs\contracts\difficulty-replay-contract-v1.md` | Casual/Normal/Challenging centralize outgoing/incoming damage, natural-spawn pressure and integer loot scaling. Save v10, v1-v9→Normal migration, create/list/pause UI, next-fixed-tick application, persistence and seed/terrain isolation pass. VS2017/v141 Debug/Release report 658/0 world and 53/0 catalogue checks. The full gate passes 49 resources, 37 performance fixtures, thirteen targets and hidden clients, eight startup negatives, a 134,873-byte dump, symbol SHA-256 `350ae322003ec12f3bf690a8994f2dbb6300188876f8ea9927280ab254bda4e6` and a 69-file package SHA-256 `1015BD8AB7D4060FCB8A4CCAA8EB76D876A5DD47E8E84DB2DEBD86CEE0BB4851`. Two Normal/v1 30-second runs compare PASS at P95 `2.657/2.642 ms`; nominal/stress each complete 60 seconds/1200 ticks with zero failures. R3 remains partial/deferred and is not marked PASS (2026-08-25). |
| N11B bounded post-victory events | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DWorldCatalogueSmoke`; VS2017 v141 client/target builds; full `scripts\verify_build.ps1`; same-identity scaled Q1; 60-second nominal/stress `HelloMine3DSoak`; `docs\contracts\difficulty-replay-contract-v1.md` | Three explicitly started Echo Trials reuse bounded two-wave Waystone encounters and award exactly two Iron Ingots once per trial, capped at three completions. Save v11, Waystone payload v2/v1 compatibility, v1-v10 migration, death cancellation, exact mid-event reopen, inventory-safe claiming and replacement-core anti-reset pass. Debug/Release world and catalogue targets report 666/0 and 59/0. The full gate passes 49 resources, 38 performance fixtures, thirteen targets and hidden clients, eight startup negatives, a 131,617-byte dump, symbol SHA-256 `37734d8d8f9342f5c22cd1199fa9ad5cd72b42cfd7fb88169c6736cef3e5eaf9` and a 69-file package SHA-256 `15DA6F7D583C1946E8373C65C614B7A8A8213CE7A76F5B33BA6E0689AF52F268`. Two v11/Normal/post-progress-zero 30-second runs compare PASS at P95 `2.722/2.712 ms` and P99 `7.881/7.835 ms`; nominal/stress each complete 60 seconds/1200 ticks with zero failures. R3 remains partial/deferred and is not marked PASS (2026-08-25). |
| N12A bilingual product presentation | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DResourcePackSmoke`; VS2017/v141 client builds; full `scripts\verify_build.ps1`; same-identity hidden startup/world-entry Q1; `docs\contracts\product-presentation-contract-v1.md` | Settings v3 adds immediate `en-US`/`zh-CN` selection with atomic v0-v2 migration. Both strict catalogues freeze 341 identical keys and cover every current material, objective and cue semantic id. Noto Sans SC 2.004/OFL, exact catalogue-derived glyph coverage, missing-font fallback, bounded long-text/extreme-scale layout, 2.5-second priority captions and localized Credits pass. Debug/Release world targets report 675/0, catalogue 59/0 and resource-pack 32/0. The full gate passes 51 resources, 38 performance fixtures, thirteen targets and hidden clients, startup negatives; the final Release binary then passes a 130,393-byte controlled dump, symbol SHA-256 `b522aac514a37c764b459e025e5c51472c4a64e9b37f1c806dede81778a6a1ed` and a 71-file package SHA-256 `8F147F8EFBBBF37B24D05CF1985E06B9CB27E2643FB2708C9A860E56298AE43B`. Same-identity usable-menu timing is `696.232/819.027 ms` and first-controllable world entry `349.761/376.942 ms`; both comparisons pass. R3 remains partially completed with bilingual human screenshots/readability deferred and is not marked PASS (2026-08-26). |
| N12B sampled sound effects | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DResourcePackSmoke`; VS2017/v141 client/target builds; full `scripts\verify_build.ps1`; hidden validation/three-frame clients; same-identity hidden startup/world-entry Q1; `docs\contracts\audio-feedback-contract-v1.md` | Audio v3 keeps nine stable cue/caption identities while replacing runtime synthesis with nine project-authored 44.1 kHz mono PCM16 WAVs. Strict RIFF/path/format checks, shared-path deduplication, missing/corrupt silent degradation, 32-sample/4-MiB cache bounds, 16 voices, lifecycle and base-only resource-pack rules pass; startup freezes 9/9 samples and 312,230 decoded bytes. Both text catalogues contain 346 keys and the manifest contains 61 entries. VS2017/v141 Debug/Release client and world/resource-pack targets pass at 681/0 and 34/0; validation and real three-frame runs exit zero, with `windows-waveout real=1` and no degradation. The full gate passes 38 fixtures, thirteen targets, startup negatives, a 128,209-byte dump, symbol SHA-256 `0265d81ff4ec8e9e021f21345805418ecebd1bc883d771cf8be8a682b86f9aae` and an 81-file package SHA-256 `1F7AEBFF35A796053A739D99CE060C14A7E39459A818DC1A6B609C5A778C416F`. Same-identity usable-menu timing is `711.836/712.408 ms` and first-controllable world entry `398.068/416.950 ms`; both compare PASS. Human listening and a physical no-device machine remain deferred with R3 and are not marked PASS (2026-08-26). |
| N12C streamed low-density music | `HelloMine3DWorldRuntimeSmoke`; `HelloMine3DResourcePackSmoke`; deterministic music generator; VS2017/v141 client/target builds; full `scripts\verify_build.ps1`; hidden validation/three-frame clients; same-identity hidden startup/world-entry Q1; `docs\contracts\streamed-music-contract-v1.md` | Music v1 freezes one `overworld.quiet` track backed by a project-authored 20-second 44.1 kHz mono PCM16 WAV. Strict definition/RIFF/path/duration checks, settings v4 and v0-v3 migration, 6-second initial delay, 2.5/1.8-second fades, 45-90-second gaps, pause/suspend/mute/zero-volume/exit cleanup, missing/corrupt degradation and base-only resources pass. WaveOut streams through at most three 4096-frame buffers instead of preloading 1,764,000 PCM bytes. Both text catalogues contain 352 keys and the manifest contains 64 entries. VS2017/v141 Debug/Release world and resource-pack targets pass at 699/0 and 38/0; validation and real three-frame runs exit zero with `windows-waveout-stream real=1` and no degradation. The full gate passes 38 fixtures, thirteen targets, startup negatives, a 131,321-byte dump, symbol SHA-256 `8283f062ff83cc6f050e92f5ff3b69aa48553307071888ba2d94867bad33ec87` and an 84-file package SHA-256 `DABE356BC511A110838DC1CD98BEE9392D0FC03FFBA1D40DB8271B6BD5392E2A`. Same-identity usable-menu timing is `716.658/740.937 ms`, first-controllable world entry is `376.473/425.927 ms` and frame P95 is `6.864/6.388 ms`; both comparisons pass. Human listening, audible transition judgment and a physical no-device machine remain deferred with R3 and are not marked PASS (2026-08-26). |
| BETA-RC engineering audit | `tools\capture_release_candidate_performance.ps1`; `tools\run_release_candidate_soak.ps1`; `tools\validate_r3_automated_preflight.ps1`; `docs\reports\beta-release-candidate-report-2026-08-26.md` | The frozen N12C runtime remains green under the full VS2017/v141 gate, controlled dump, symbols, licences and clean 84-file package. The formal collector now identifies save format v11; cold start, world entry, save, restore, fast streaming and scaled gameplay baseline/repeat comparisons all pass without threshold changes. Both formal Q3 profiles pass for 1,800 seconds. R3 automated logic passes, while the evidence explicitly records physical input `NOT_RUN` and closure `NOT_ELIGIBLE`; the partially completed human checklist stays deferred, so no Beta tag is created (2026-08-26). |
| V10D optional directional shadows | `HelloMine3DWorldRuntimeSmoke` (`HELLOMINE3D_WORLD_SMOKE_FOCUS=V10D`); `HelloMine3DResourcePackSmoke`; `tools\validate_stage10_visual_performance.ps1`; `docs\contracts\directional-shadow-contract-v1.md` | Settings v5 adds strict Off/Medium/High with v0-v4→Off migration. One bounded float32 directional shadow texture gives solid terrain and actors cast shadows while dedicated receiver shaders keep Off on the exact V10C path; capability/setup failure atomically falls back to Off. VS2017/v141 Debug/Release focused settings pass 21/21, resources pass 75/75, final Release world passes 742/742, manifest/startup/tier contracts and forced fallback pass. Six final 1280x720 Release images pass developer review with intact HUD. On the same 361-chunk/1833-section scaled-gameplay fixture, Medium/High frame P95 is 11.529/11.957 ms versus Off 12.041 ms, so the owner's contingency exception is recorded but not consumed. Windows is Done; macOS remains Verify (2026-08-28). |
| V10E lightweight post-processing | `HelloMine3DWorldRuntimeSmoke` (`HELLOMINE3D_WORLD_SMOKE_FOCUS=V10E`); `HelloMine3DResourcePackSmoke`; `tools\validate_stage10_visual_performance.ps1`; `docs\contracts\post-processing-contract-v1.md` | Settings v6 adds strict Off/On post-processing with v0-v5→Off migration. A single bounded Ogre compositor applies a tone curve, deterministic low-amplitude dither and eight-tap very-light bloom while post-viewport HUD/UI remains unprocessed; Off installs no compositor and capability/setup failure atomically falls back to Off. VS2017/v141 Debug/Release focused settings pass 22/22, resources pass 80/80, final Release world passes 743/743, the 77-entry manifest, 15 startup negatives, 11 performance-contract cases and real supported/forced-fallback clients pass. Six final Release images pass developer review; same-identity three-run medians give On/Off frame P95 `12.99/14.28 ms` and P99 `16.45/17.13 ms`, so no exception is used. Windows is Done; macOS remains Verify (2026-08-28). |
| VISUAL-RC engineering audit | `scripts\verify_build.ps1`; `tools\capture_release_candidate_performance.ps1`; `tools\run_release_candidate_soak.ps1`; `tools\validate_xcode_generation.ps1`; `docs\reports\visual-release-candidate-report-2026-08-28.md` | Final runtime identity `d6172bd` passes the VS2017/v141 Debug/Release gate with 77 manifest entries, 743/743 world, 80/80 resource-pack, 117/117 recipe, 15/15 startup negatives, a 145,169-byte controlled dump and a 97-file clean package. Six Q1 baseline/repeat comparisons pass; nominal/stress formal Q3 each complete 1,800 seconds/36,000 ticks with zero failures, peak private memory 20,824,064/30,011,392 bytes and steady growth 4,177,920/13,840,384 bytes. The complete Stage 10 developer visual matrix passes. Windows-generated Xcode projects pass 239 static graph checks with `native_build=NOT_RUN`; `AI-07=NOT_RUN`，Physical Input v2 为历史 `NOT_RUN/SUPERSEDED`，macOS 原生只在另行批准目标平台批次时执行（2026-08-31 映射）。 |
| P11A core input feel | `HelloMine3DWorldRuntimeSmoke` (`HELLOMINE3D_WORLD_SMOKE_FOCUS=P11A`); `HelloMine3DResourcePackSmoke`; VS2017/v141 client builds | 四项世界动作单一仲裁、鼠标绑定/冲突、settings v7 迁移、线性相对视角增量、Y 轴反转、hold/toggle、UI 所有权和焦点门均已实现。Debug/Release 定向各 88/88，Release 资源包 80/80、完整世界 772/772，客户端双配置编译通过；`AI-01..AI-04=NOT_RUN`，物理手感 `NOT_CLAIMED`（2026-08-31 映射）。 |
| P11B action feedback | `HelloMine3DWorldRuntimeSmoke` (`HELLOMINE3D_WORLD_SMOKE_FOCUS=P11B`); `HelloMine3DResourcePackSmoke`; VS2017/v141 client builds; `docs/contracts/action-feedback-contract-v1.md` | settings v8 迁移、裂纹/粒子、表现层命中停顿、冷却、战斗反馈、音量微变体和掉落物上限已实现。Debug/Release 定向各 84/84，Release 世界 786/786、资源包 80/80，客户端双配置通过；`AI-05/AI-07=NOT_RUN`，人类打击感/舒适度 `NOT_CLAIMED`。 |
| P11-1 minimum building and tool split | `HelloMine3DWorldRuntimeSmoke` (`HELLOMINE3D_WORLD_SMOKE_FOCUS=P11-1`); `HelloMine3DRecipeSmoke`; `HelloMine3DResourcePackSmoke`; `tools\validate_terrain_atlas.ps1`; VS2017/v141 client builds; `docs/contracts/minimum-building-tools-contract-v1.md` | 木板/圆石闭环、单方块门、axe/shovel、24 配方/4 冶炼/8 工具和身份追加已通过定向 39/39、配方 121/121、资源 80/80、Release 世界 794/794、图集 274/274 和 84 项 manifest；`AI-05=NOT_RUN`，人类建造乐趣/速度手感 `NOT_CLAIMED`。 |
| P11C first thirty minutes | `HelloMine3DWorldRuntimeSmoke` (`HELLOMINE3D_WORLD_SMOKE_FOCUS=P11C`); `HelloMine3DRecipeSmoke`; `HelloMine3DResourcePackSmoke`; VS2017/v141 client builds; `docs/contracts/first-thirty-minutes-contract-v1.md` | 目标 v3/34 项、三项并行机会、分支进度、配方发现/保存和 v1/v2 归一已通过定向 41/41、配方 121/121、资源 80/80、Release 世界 803/803；`AI-06=NOT_RUN`，只声明 AI 可理解性，人类首次体验/留存 `NOT_CLAIMED`。 |
| P11D exploration rewards | `HelloMine3DWorldRuntimeSmoke` (`HELLOMINE3D_WORLD_SMOKE_FOCUS=P11D`); `HelloMine3DRecipeSmoke`; `HelloMine3DResourcePackSmoke`; `HelloMine3DWorldCatalogueSmoke`; `tools\validate_terrain_atlas.ps1`; VS2017/v141 client builds; `docs/contracts/exploration-reward-contract-v1.md` | 三类能力、save v12、奖励 v1/v0 和旧世界迁移已通过定向 30/30、Release 世界 810/810、配方 121/121、资源 80/80、目录 59/59、图集 278/278；`AI-05=NOT_RUN`，人类奖励价值感 `NOT_CLAIMED`。 |
| P11-2 terrain contours and cave entrances | `HelloMine3DWorldRuntimeSmoke` (`HELLOMINE3D_WORLD_SMOKE_FOCUS=P11-2`); `HelloMine3DRecipeSmoke`; `HelloMine3DResourcePackSmoke`; `HelloMine3DWorldCatalogueSmoke`; VS2017/v141 client builds; `docs/contracts/terrain-contours-cave-entrances-contract-v1.md` | terrain v4 Mountain/洞口、v1-v3 冻结和 save v12 v3/v4 身份已通过定向 9/9、Release 世界 820/820 和正式 Q1；`AI-05/AI-07=NOT_RUN`，人类风景审美 `NOT_CLAIMED`。 |
| P11E enemy presentation and Waystone resonance | `HelloMine3DWorldRuntimeSmoke` (`HELLOMINE3D_WORLD_SMOKE_FOCUS=P11E`); `HelloMine3DRecipeSmoke`; `HelloMine3DResourcePackSmoke`; `tools\validate_resource_manifest.ps1`; VS2017/v141 client builds; `docs/contracts/enemy-presentation-waystone-resonance-contract-v1.md` | 多部件轮廓、关键姿态、8-tick 死亡表现、身份掉落和 Waystone 共鸣已通过定向 35/35、Release 世界 832/832、配方 122/122、资源 80/80、84 项 manifest 与正式 Q1；`AI-05/AI-07=NOT_RUN`，人类危险感/价值感/战斗乐趣 `NOT_CLAIMED`。 |

| P11F PLAYABILITY-RC engineering audit | `scripts\verify_build.ps1 -VisualStudioVersion 2017`; `tools\capture_release_candidate_performance.ps1`; `tools\run_release_candidate_soak.ps1`; `tools\validate_crash_diagnostics.ps1`; `tools\package_windows_release.ps1`; `docs\reports\playability-release-candidate-report-2026-08-31.md` | Frozen runtime identity `320e293` passes the VS2017/v141 Debug/Release gate with 84 manifest entries, 278 atlas checks, 38 performance fixtures, 11 Stage 10 supplement checks, 832/832 world, 80/80 resource-pack, 122/122 recipe and 15/15 startup negatives. Six formal Q1 comparisons and both 1,800-second Q3 profiles pass; the controlled crash produces a 136,615-byte dump, and the 104-entry package SHA-256 is `422F97E87046D4B6D5FC4BB99C37886FF37C4461A152C1162FA66A972B12F459`. Windows engineering is Done; current AI scenarios are `NOT_RUN`, human subjective experience is `NOT_CLAIMED`, and the historical report is not rewritten (2026-08-31). |
| AL-A0 latest architecture baseline | Source/API/ownership audit; `git diff --check`; changed-Markdown reference validation; World renderer-boundary probe; `scripts\verify_build.ps1 -VisualStudioVersion 2017`; `docs\reports\architecture-lab-baseline-v1.md` | No tracked Gameplay/runtime/resource/build-input change. Debug/Release rebuild and all gates pass with 832/832 world, 80/80 resource-pack, 122/122 recipe, 15/15 startup negatives and real window PASS. Formal Q1/Q3 are cited from unchanged runtime identity rather than rerun. The A0 rebuild creates a distinct local 104-entry verification package SHA-256 `0E18AAA9DC45C1663CBC5CAC6E83DB1995EA65DA47B16B546A2310C75979A6FB`; it does not replace the historical P11F package identity. `AI-01..AI-08=NOT_RUN`, human subjective experience remains `NOT_CLAIMED` (2026-09-01). |
| AL-A1 World responsibility map | `tools\validate_world_responsibility_map.ps1`; `scripts\verify_build.ps1 -VisualStudioVersion 2017`; `docs/contracts/world-responsibility-map-contract-v1.md` | The machine-checked map covers all 78 unique public `World` method names as 45 Queries, 31 Commands and 2 Runtime Ticks across nine responsibilities; normalized public-surface SHA-256 is `3C53F56C425F0395354C8A5CE966E96CDA8BC93D836699955E03BA965A664AD8`. The complete VS2017/v141 Debug/Release gate passes with 832/832 world, 80/80 resource-pack, 122/122 recipe, 15/15 startup negatives, a 147,777-byte controlled dump, symbol archive SHA-256 `4716391d29d2bc92c11258d1755d9aac3f86ccee962414a8b684dd3692ffeec3`, 104-entry package SHA-256 `00F7239139580FD71D5EF9518335CA7A193E5A184825C07630F3F96979713054` and real window PASS. No tracked Gameplay/runtime/resource input changed, so formal Q1/Q3 remain cited from the frozen runtime identity. `AI-01..AI-08=NOT_RUN`; human subjective experience remains `NOT_CLAIMED` (2026-09-01). |
| AL-A5 Simulation phase metrics | `tools\validate_simulation_metrics_boundary.ps1`; `HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A5`; `scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow`; `docs/reports/architecture-lab-a5-simulation-metrics-report-v1.md` | Exactly four real phase metrics freeze copied elapsed/processed/deferred/budget scope/status without changing AL-A3 order or existing hard limits. Debug/Release focused runs pass 13/13; the complete gate passes 853/853 WorldRuntime twice, 80/80 resource-pack, 122/122 recipe, 15/15 startup negatives and two zero-failure short soaks. The 104-entry package SHA-256 is `4DA15FA1799804FB61710FA285DD0E6818BE1B42CE26AFA25E1F777DF3334009`. Real-window launch is `DEFERRED`, AI acceptance is `NOT_RUN`, and human subjective experience is `NOT_CLAIMED` (2026-09-01). |
| AL-A6 Architecture Lab documentation pipeline | `tools\validate_architecture_lab_documentation.ps1 -SelfTest`; `scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow`; `docs/reports/architecture-lab-a6-documentation-pipeline-report-v1.md` | The focused gate passes seven manifest rows, one implemented Part, six non-empty Sections and four negative fixtures. The complete VS2017/v141 gate passes 853/853 WorldRuntime twice, 80/80 resource-pack, 122/122 recipe, 15/15 startup negatives and two zero-failure short soaks. The 104-entry package SHA-256 is `0E387185600E957F8BC6ED4B46333817B0C12F0725322F9F939337455C245990`. Real-window launch is `DEFERRED`, AI acceptance is `NOT_RUN`, and human subjective experience is `NOT_CLAIMED` (2026-09-01). |
| B1 Chunk residency state machine | `tools\validate_chunk_residency_state_machine.ps1`; `HELLOMINE3D_WORLD_SMOKE_FOCUS=B1`; `scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow`; `docs/reports/architecture-lab-b1-chunk-residency-report-v1.md` | Seven Data, five Mesh and four Ogre Render states are separately owned and assertion-guarded. Focused runtime passes 38/38, including light-only invalidation, save-failure rollback, dirty eviction, stale upload, unload persistence and direct deterministic structure generation/reload. The complete VS2017/v141 gate passes 863/863 WorldRuntime twice, 80/80 resource-pack, 122/122 recipe, 15/15 startup negatives, two zero-failure short soaks and a 104-entry package with SHA-256 `F76F5A1429C869FDA94FDD37BF34F8266866B5F665D1E550CA04F15CCD7ECF88`. World public API, AL-A2 budgets, Gameplay and save v12 stay unchanged. Real-window launch is `DEFERRED`, AI acceptance remains `NOT_RUN` and human subjective experience remains `NOT_CLAIMED` (2026-09-01). |
| B2 Streaming demand model | `tools\validate_streaming_demand_model.ps1`; `HELLOMINE3D_WORLD_SMOKE_FOCUS=B2`; `scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow`; `docs/reports/architecture-lab-b2-streaming-demand-report-v1.md` | Four bounded Player/Camera/TeleportDestination/Preload slots drive de-duplicated deterministic planning by reason, frustum, motion, age and distance. Focused runtime passes 26/26; the complete gate passes 875/875 WorldRuntime twice, 80/80 resource-pack, 122/122 recipe, 15/15 startup negatives and two zero-failure short soaks. The 104-entry package SHA-256 is `9955E51AD94C7E5600325329031432E16C1859F936FA880F68F65F3B80369503`. No B3-B9 capability is implemented; real-window is `DEFERRED`, AI acceptance is `NOT_RUN`, and human subjective experience remains `NOT_CLAIMED` (2026-09-01). |
| B3 Generic World Job Scheduler | `tools\validate_world_job_scheduler.ps1`; `HELLOMINE3D_WORLD_SMOKE_FOCUS=B3`; `scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow`; `docs/reports/architecture-lab-b3-world-job-scheduler-report-v1.md` | Two real typed jobs use deterministic pending order, one in-flight slot, completed records and copied metrics while retaining one loader and B1 revision commit. Focused runtime passes 9/9, B2 passes 26/26 and B1 passes 38/38. The complete gate passes 884/884 WorldRuntime twice, 80/80 resource-pack, 122/122 recipe, 15/15 startup negatives and two zero-failure short soaks; the 104-entry package SHA-256 is `B983889B5553FF0DBFEAF6C14D2E349CC81AD1205C314CC39C0894C2D1CD9459`. Real-window is `DEFERRED`, AI acceptance remains `NOT_RUN` and human subjective experience remains `NOT_CLAIMED` (2026-09-01). |
| B4 World Job Cancellation & Generation Token | `tools\validate_world_job_cancellation.ps1`; `HELLOMINE3D_WORLD_SMOKE_FOCUS=B4`; `scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow`; `docs/reports/architecture-lab-b4-world-job-cancellation-report-v1.md` | A copied uint64 generation token, cooperative `Cancelled` outcome, six invalidation boundaries, detached Chunk candidate and linearized commit prevent stale load/mesh publication while retaining one worker. Focused B4/B3/B2/B1 runs pass 10/10, 9/9, 26/26 and 38/38. The complete gate passes 894/894 WorldRuntime twice, 80/80 resource-pack, 122/122 recipe, 15/15 startup negatives and two zero-failure short soaks; the 104-entry package SHA-256 is `A9A7CC9AF528F3C725ACC13A62A69FB6D10718AD25F7F4796F5E47B70453C33A`. Real-window is `DEFERRED`, AI acceptance remains `NOT_RUN` and human subjective experience remains `NOT_CLAIMED` (2026-09-01). |
| B5 Streaming Backpressure | `tools\validate_streaming_backpressure.ps1 -Implementation`; `HELLOMINE3D_WORLD_SMOKE_FOCUS=B5`; `scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow`; `docs/reports/architecture-lab-b5-streaming-backpressure-report-v1.md` | Caps 128/128/128, watermarks 96/48, explicit deterministic admission/shedding, retained-plan refill and commit/upload/unload limits 8/8/8 pass the static gate and 12/12 focused runtime checks. B4/B3/B2/B1 regressions pass 10/10, 9/9, 26/26 and 38/38. The complete gate passes 906/906 WorldRuntime twice, 80/80 resource-pack, 122/122 recipe, 15/15 startup negatives, both short soaks and both hidden clients; the 104-entry package SHA-256 is `7D126B31B78F3A4E8F8C90A5D769028EC686C0D4F708D1F6B2E2979BD164050B`. The result is `PASS real_window=DEFERRED`; AI acceptance remains `NOT_RUN` and human subjective experience remains `NOT_CLAIMED` (2026-09-02). |
| B6 Spatial Activation | `tools\validate_spatial_activation.ps1`; `HELLOMINE3D_WORLD_SMOKE_FOCUS=B6`; `scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow`; `docs/reports/architecture-lab-b6-spatial-activation-report-v1.md` | Four nested Outside/Resident/Near/Simulation classifications derive deterministically from B2 demand. Real plan/mesh/upload/unload consumers honor resident/near interest while simulation request remains publication-only. B6 focused runtime passes 12/12 and B5/B4/B3/B2/B1 regressions pass 12/12, 10/10, 9/9, 26/26 and 38/38. The complete gate passes 918/918 WorldRuntime twice, 80/80 resource-pack, 122/122 recipe, 15/15 startup negatives, both short soaks and both hidden clients; the 104-entry package SHA-256 is `C8E260E00CF76C952150EBC3DC851A7EDE5E13FE63A58F98B77DC103723EFA3C`. The result is `PASS real_window=DEFERRED`; AI acceptance remains `NOT_RUN` and human subjective experience remains `NOT_CLAIMED` (2026-09-02). |
| AI-assisted gameplay baseline | `docs\current\ai-assisted-gameplay-acceptance-v1.md` | `AI-01..AI-08 result=NOT_RUN`; no OS-level Computer Use record has been produced in this documentation batch. |

The current 2026-08-12 runs use an NVIDIA GTX 1050 Ti with OpenGL 4.6. The
earlier GDI Generic limitation no longer applies to these hardware-backed PNG
and performance records.

## When To Run

Run the full set before closing any milestone. Run the world runtime smoke
after any change to chunk addressing, chunk storage, block interaction, the
event bus, actors, or terrain generation. Also run R1 after timing/residency
changes, R2 after chunk/actor/persistence/background-worker changes, affected
`AI-01..AI-08` scenarios after player-facing input/UI/gameplay/visual changes,
and R5 after manifest, resolver or packaging changes. Add each accepted run to
the table above rather than relying on an untracked local success. Historical
Physical Input protocols are rerun only if the owner explicitly reopens
physical-device claims.
