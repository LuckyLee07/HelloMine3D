# B10 Large World Stress & Acceptance Report v1

> B10 Core acceptance result: `PASS`
>
> Workflow state: `Done`
>
> AI gameplay result: `NOT_RUN`
>
> Human subjective claims: `NOT_CLAIMED`

## Scope and compatibility

B10 closes the implemented Track B Core with reproducible headless evidence over
the real `World`, loader worker, B1 residency states, B2 demand, B3 scheduler,
B4 cancellation, B5 backpressure, B6 spatial activation, mesh publication and
save-v12 persistence paths. It adds no new runtime layer, worker, job family,
Far representation, simulation fidelity or gameplay feature. Gameplay,
resource economy, victory flow, terrain v4, settings v8 and the 78-method World
public surface remain unchanged.

The frozen contract is
`docs/contracts/large-world-stress-acceptance-contract-v1.md`. B7-B9, Track C,
Track D and all Extended capabilities remain outside this batch.

## Schedule and tooling

`HelloMine3DSoak.exe --profile track-b-core` schedule v3 runs five phases in
one Release process and save root: 600 seconds Straight Run, then 300 seconds
each of Teleport Storm, Turnaround, Render Distance Churn and Edit & Leave.
The formal LW5 performs ten complete edit/save/far-demand/reopen checks. Two
isolated 10-second probes compare 25 deterministic fields while excluding OS-
scheduled wall time, memory and asynchronous queue depth.

`tools/run_large_world_stress_acceptance.ps1` produces versioned summaries;
`tools/validate_large_world_stress_acceptance.ps1` validates both implementation
and evidence. Raw saves, logs and per-second/process CSV samples stay ignored
under `bin/soak_runs/`.

## Failures found and repaired

The first 1800-second attempt timed out after 1981.176 seconds with its last
world snapshot at 1639 seconds. It reached 8960 manager entries, 8831 loaded
Chunks and 2,465,361,920 peak private bytes. The long run exposed two composed
production defects:

- cancelled detached loads transitioned to semantic `Absent` but retained a
  manager-map tombstone, while the bounded unload scan spent its first eight
  slots before checking `Resident` eligibility;
- synchronous mesh-neighbour reads called `getOrCreateChunk`, so a Query could
  retain missing edge coordinates as `Absent` authoritative entries.

`cancelChunkLoadJob` now erases the detached reservation after the guarded
transition. The unload scan filters `Resident` before admission, counts only
successful removals and preserves truthful backlog. `findAdjacent` makes mesh-
neighbour inspection non-creating, with an unavailable neighbour treated as
the existing non-solid/air boundary.

A second pre-correction formal attempt retained zero `Absent` entries and a
maximum of only 223 Chunks, but timed out at 1984.735 wall seconds with its last
world snapshot at 1649 seconds. Its 118,312,960-byte peak private set and 232
peak handles were within bounds. The remaining failure was in the test load:
LW5 inherited a five-second developer cadence, expanding ten persistence checks
in the 300-second schedule into sixty whole save/reopen/backup-copy cycles.
The contract did not require that amplification. LW5 is now frozen at ten
persistence checks in both schedules; the 180-second shutdown grace and all
runtime thresholds remain unchanged.

Both failure summaries are retained in
`docs/baselines/architecture-lab-b10-windows-hidden-v1/`. No failure was
deleted, no threshold was relaxed and no performance exception was used.

## Focused and diagnostic evidence

The B10 focused runtime passes `2/2`: 32 ineligible Loading reservations cannot
starve ten distant Resident unloads (`8+2` completed), and mesh-neighbour input
beside missing Chunks does not grow the authoritative map. Composed B1/B4/B5
focused regressions remain `38/38`, `10/10` and `12/12`.

After both production repairs, a 300-second five-phase diagnostic passes with
6000 ticks, ten persistence checks, maximum 172 existing/loaded Chunks, zero
retained `Absent`, 58,544,128 peak private bytes, 23,273,472 steady private
growth, 230 peak handles and 305.576 wall seconds. Its two probes match on
schedule digest `066d55e9a8806872` and persistence digest
`21d218bd7ddaa211`.

## Formal B10 acceptance

Executed on the VS2017/v141 Release x64 binary:

```powershell
& .\tools\run_large_world_stress_acceptance.ps1 `
  -Formal -DurationSeconds 1800 -ProbeDurationSeconds 10 -Seed 20260902
& .\tools\validate_large_world_stress_acceptance.ps1 -Evidence
```

The final result is `PASS`: 1800 completed seconds, 36,000 ticks, zero failures,
905 movements, 7200 block edits, 1800 Actor lifecycles and ten successful
save/reopen persistence checks. All phase tick counts are exact. Maximums are
216 existing/loaded Chunks, 14 dirty updates, 28 Actors, 630 dirty sections,
95 pending jobs, one in-flight job, and consumer budgets `8/8/8`. All seven
Data Residency counts balance on every sample; maximum and final retained
`Absent` are both zero.

The process completes in 1824.453 wall seconds without timeout. Peak private
bytes are 137,551,872, peak working set 142,618,624, peak handles 229 and peak
threads four. Post-warmup private growth is 120,004,608 bytes and handle growth
is one, within the unchanged Q3 bounds. The two final probes match on the same
digests listed above; asynchronous metrics are explicitly not compared.

## Full gate closeout

The final composed command is:

```powershell
& .\scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
```

The first invocation stopped before compilation because the AL-A6 documentation
gate correctly required this manifest-referenced report to exist. That failed
preflight remains part of the audit trail. After the report was added, the same
command passed the complete VS2017/v141 Debug and Release graph: both
WorldRuntime runs report `920/920`, resource packs `80/80`, recipes `122/122`,
startup negatives `15/15`, and all 13 test executables, hidden validation
clients, crash diagnostics and executable-inventory checks pass. The isolated
package contains 104 entries and hashes to
`E13203F8E18382A4D13ABA22DFB975DDB189B2858F74DAAE340CE5A4A8F34B14`.
The 8,811,520-byte Release executable hashes to
`FC563EF53D629AC20966CCB830F602DEB3E6E5D126BC3ED756E365E4E34ED37C`.

The final Q1 command also passes all six baseline/repeat comparisons. At the
unchanged `rc-ring-12-chunks-v2` identity, fast-streaming frame P95 is
`4.796/4.508 ms`, Chunk-visible P95 is `204.013/192.133 ms`, and queue peak is
`5/5`, below the unchanged 1,000 ms visibility and 512-entry queue limits.
Save total/stall is `121.275/135.304 ms`; backup restore total is
`52.128/53.393 ms`. No Q1 threshold or performance exception changed.

## Claim boundary

No focus-stealing real window is part of B10: `real_window=DEFERRED`.
`AI-01..AI-08=NOT_RUN`; human fun, aesthetics, comfort, retention and physical
input feel remain `NOT_CLAIMED`. Headless stress proves boundedness,
recoverability and deterministic action identity, not visual quality or player
experience.
