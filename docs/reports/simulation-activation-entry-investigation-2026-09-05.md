# D2 Simulation Activation entry investigation — 2026-09-05

Decision: **entry is not established by the evidence collected here; retain
`Candidate / not approved` for implementation.** Investigation is authorized
by the owner's TODOLIST Goal. This is not a completed D2 implementation or a
proof that activation will never be needed.

## Question and starting identity

Starting runtime commit: `81d945dcf7a4bdbd6d98a1652b2776fca37665fa`.
The Goal authorizes D2 implementation only when a real distant Actor/Machine
workload demonstrates that D1 item budgets are insufficient. The existing D1
Windows gate is recorded in its frozen report. This investigation does not
rerun or relabel that Windows evidence.

D1 already explicitly permits slowdown under overload. Showing `N > B` or
an object deferred for one tick is therefore not, by itself, a failure of its
contract or a sufficient reason to choose Full/Reduced/Dormant semantics.

## Production-path audit

- `WorldSimulation::fixedTick` collects loaded Furnace/Crusher positions,
  sorts Furnace before Crusher then X/Y/Z, and admits at most 32 invocations.
  Empty machines still occupy the list. Distance is not an admission input.
- The Actor workload admits at most 64 live managed Actors with a rotating
  insertion-order index. PlayerActor and other mandatory phases remain outside
  that budget. MobActor already stops chasing outside its chase radius;
  natural population has world/local caps and unload despawns. Those existing
  rules do not constitute D2 activation or an Actor AI LOD implementation.
- B6 `requestsSimulation` is a spatial-interest observation. It is not yet
  consumed by the simulation. Its existence does not authorize D2.
- C3 topology has no per-tick workload. Automatic factories, belts and network
  simulation cannot be invoked as existing workload evidence.

## Reproducible bounded machine probe

The diagnostic-only `D2-ENTRY` focus in
`src/HelloMine3D/Tests/WorldRuntimeSmokeMain.cpp` uses the real World,
preload path, Furnace/Crusher containers and fixed-tick scheduler. It adds no
production runtime change and is not included in the default regression run.

Fixtures use seed `20260807`, player `(8,90,8)`, a powered Crusher at
`(8,90,10)`, and 0/8/32 empty Furnaces at X `-72..-65`, Y `90`, Z `8..11`.
Those Furnaces are about 73–80 blocks away horizontally and remain loaded.
One cobblestone and 40 crank ticks are provided to the Crusher. Each fixture
runs 20 fixed ticks. This uses automated fixture APIs, not normal-input AI
acceptance; no claim of a player having built this layout is made.

```sh
bash scripts/premake.sh gmake
make -C build -j2 config=debug_x64 HelloMine3DWorldRuntimeSmoke
cd bin
HELLOMINE3D_WORLD_SMOKE_FOCUS=D2-ENTRY ./HelloMine3DWorldRuntimeSmoke
```

For Release, use `config=release_x64`. Binaries share the same output name;
run the selected configuration before rebuilding another. Timings are
observations only and are not substituted for formal Q1 measurements.

| Distant idle Furnaces | Near Crusher progress after tick 1 | Progress after tick 20 | Remaining power | Ticks with deferred work |
| --- | --- | --- | --- | --- |
| 0 | 1 | 20 | 20 | 0 |
| 8 | 1 | 20 | 20 | 0 |
| 32 | 0 | 19 | 21 | 20 |

The valid Debug run reports three successful fixture-initialization checks.
Total machine-phase time across 20 ticks was approximately 0.311 / 1.209 /
3.348 ms for the three fixtures on this host. These are short observations
under shared host load, not portable performance guarantees.

Both Release runs also pass 3/3 fixture checks and reproduce all gameplay
columns exactly. Release machine-phase totals are 0.121/0.296/0.897 ms and
0.110/0.356/0.956 ms respectively. These values are not an approved threshold
and do not support a platform-independent performance claim.

The first probe attempt omitted distant preload. Its 8/32-machine fixtures
failed initialization and are **invalid evidence**, retained at
`build/goal-20260905/debug-d2-entry.log`. Adding the real preload path repaired
the fixture; `debug-d2-entry-2.log` is the valid Debug run. The failed attempt
does not demonstrate any production regression.

## Interpretation and implementation decision

There is a concrete potential concern: enough distant empty machines consume
admission that a nearby powered machine may miss a step. At 33 total machines,
the observed first-tick delay and subsequent service agree with D1's frozen
`ceil(33/32) = 2` stable-set service window; input/power state remains intact.
The 9-machine fixture does not defer work. Neither fixture establishes a
formal performance failure or an unacceptable normal-gameplay interaction.

This evidence supports tracking the distance-insensitive budget trade-off. It
does not yet select D2 over a narrower future change such as proven idle-work
filtering, nor establish safe distance/cadence rules for real machine progress,
burning, combat or persistent state. No normal-input window acceptance of this
layout was completed. Inventing hundreds of machines solely to exceed the
known budget would not remedy that missing evidence.

Consequently this Goal does **not** introduce D2 runtime vocabulary, change
budgets, change the D1 overload contract or implement D3+ by implication.
Reopen the entry decision when a reproducible normal-play session or an
approved representative scale scene identifies a concrete unacceptable delay
or performance boundary attributable to distant work. Record the required
near-field behavior and test the proposed activation policy against existing
save/reopen, machine and combat semantics before implementation.

## Evidence scope

Host: macOS Darwin 24.6.0 arm64; Apple clang 17.0.0; generated target is macOS
**x86_64**, executed through the host's translation support, not a new arm64
or TSan gate. Current-source results and artifact hashes are tracked in
[the Goal execution record](todolist-goal-execution-2026-09-05.md).
Windows VS2017/v141, AI-01..AI-08, audio and package-only blind acceptance are
not closed by these probes. Save v12, machine progression and D1 admission
behavior remain unchanged. Later macOS warning cleanups and the catalogue
test-path correction are recorded separately in the Goal execution record.
