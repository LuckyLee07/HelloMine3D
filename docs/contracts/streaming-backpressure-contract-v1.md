# B5 Streaming Backpressure Contract v1

> Status: Frozen after B5 verification
>
> Workflow state: Done
>
> AI gameplay result: `NOT_RUN`
>
> Human subjective claims: `NOT_CLAIMED`

## 1. Problem and measured boundary

B2 can produce `(2r + 1)^2` de-duplicated Chunk targets for one demand centre:
the default render distance 8 therefore produces 289 targets and the supported
maximum 32 can produce 4,225. B3/B4 currently replace the pending vector with
the complete plan. One loader consumes it, while the render thread copies and
uploads every `CpuReady` section it observes in a frame. The source is finite,
but neither boundary expresses stable queue latency or overload policy.

B5 adds pressure control to the two real B3 job types and existing mesh/unload
consumers. It does not add a worker, a main-thread completion dispatcher, B6
Spatial Interest, far representation, a generic scheduler or a future job
family.

## 2. Scheduler limits and watermarks

The frozen limits are:

```text
MaxPendingTotal             128
MaxPendingGeneration        128
MaxPendingMesh              128
PendingHighWatermark         96
PendingLowWatermark          48
MaxAuthoritativeCommitsPerPass 8
MaxSectionUploadsPerFrame     8
MaxUnloadsPerUpdate           8
```

Per-type limits are explicit even though the shared cap is currently the
tighter bound. This leaves one unambiguous invariant: `pendingJobs <= 128` and
each real type is independently bounded without inventing future job slots.

The pressure state is `Normal`, `Elevated` or `Saturated`. It enters
`Elevated` at 96 pending jobs, enters `Saturated` at 128, and leaves an elevated
or saturated episode only after pending work falls to 48 or less. This
hysteresis is derived runtime state and is not saved.

## 3. Explicit admission and deterministic shedding

`WorldJobAdmissionResult` contains exactly:

- `Accepted`;
- `AcceptedAfterShedding`;
- `Duplicate`;
- `StaleGeneration`;
- `RejectedAtCapacity`.

Duplicate and stale-generation checks run before pressure policy and preserve
B3/B4 counters. Below the hard cap, a valid unique request is accepted. At the
cap, the incoming request is compared with the worst pending request using the
existing B3 order: priority descending, plan order ascending, demand epoch
descending, type rank and job id. A strictly more important request sheds that
one worst pending job and is accepted; an equal or worse request is rejected.
In-flight work is never shed by B5.

Plan replacement remains generation-atomic. If the plan is larger than the
admitted window, only the highest B2/B3-ranked window enters the scheduler;
the remainder is current demand, not hidden `WorldJob` state.

## 4. Demand-plan refill

`ChunkRuntime` retains the current immutable vector of B2-derived requests and
a monotonic cursor for the active generation. Initial planning fills to the
high watermark, not the hard cap. When pending work reaches the low watermark,
the one loader refills toward the high watermark before sleeping.

Follow-up work is admitted before refill. Because one completed job releases a
shared-cap slot, a current-generation load-to-mesh or same-type follow-up is
not lost. A semantic B4 invalidation clears pending jobs and the retained plan
cursor; a priority-only B2 replan replaces pending order while preserving the
single in-flight job. No deferred plan survives its generation.

The retained plan is bounded by the existing render-distance and four-slot B2
demand contract. It is not a second job queue, does not carry job ids or job
states, and never appears in persistence.

## 5. Commit, upload and unload consumers

The loader retains the existing 6 ms and 64-target pass ceilings and adds at
most eight authoritative commit intervals per pass. This is deliberately a
loader-pass budget rather than the roadmap's earlier illustrative
`MaxCommitPerFrame`: B4 commits on the loader thread and B5 does not create a
fake main-thread dispatcher.

`collectSectionMeshSnapshot` offers at most eight `CpuReady` sections per
render frame. Selection reuses the current Player-demand coordinate, then
orders by squared distance, Manhattan distance, x, z and section y. The
snapshot records total ready and deferred-ready counts. Unselected sections
remain `CpuReady` and are offered on later frames; no mesh is discarded.

Chunk unload retains the existing maximum of eight per `World::update` and now
records the last count and whether another pass is required. B5 changes no
unload eligibility or persistence semantics.

## 6. Diagnostics

Copied developer diagnostics expose:

- pending generation/mesh/total, peak counts and deferred demand-plan count;
- current pressure level, limits and watermarks;
- accepted, accepted-after-shedding, duplicate, stale and capacity-rejected
  admissions;
- shed totals by real job type, pressure transitions and saturation episodes;
- last loader-pass commit count and its cap;
- total/offered/deferred CPU-ready sections and upload cap;
- last unload count, unload backlog and unload cap.

The existing Demand/Queued/InFlight/Ready/Cancelled values remain visible in
the same developer panel.

## 7. Automated evidence

`HELLOMINE3D_WORLD_SMOKE_FOCUS=B5` must prove:

1. exact limit, pressure and admission vocabularies;
2. normal admission below the high watermark;
3. duplicate and stale requests retain B3/B4 behavior;
4. low-priority work cannot exceed the hard cap;
5. higher-priority work deterministically sheds the worst pending request;
6. high/low watermark hysteresis and transition counters;
7. invalidation clears pending pressure without touching in-flight work;
8. an oversized plan retains B3 order and can be drained/refilled without
   losing a target;
9. the live runtime exposes a plan larger than the high watermark while its
   scheduler remains bounded;
10. loader-pass authoritative commits never exceed eight;
11. upload selection is deterministic, nearest-first and limited to eight;
12. unload processing remains limited to eight and truthfully reports backlog.

B4, B3, B2 and B1 focused regressions and every AL-A1..A6/B1..B5 static gate
must continue to pass.

## 8. Compatibility and non-goals

B5 preserves save v12, terrain v4, settings v8, resources, Gameplay, the
AL-A1 78-method World surface, one loader worker, B3 lifecycle/order and B4
generation/commit cancellation. It does not implement B6 Spatial Interest,
B7-B9, Track C/D, a worker pool, adaptive timing, per-biome policy or an empty
Registry/Simulation Scheduler.

B5 becomes `Done` only after VS2017/v141 Debug and Release pass the full
`scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow` gate, a
104-entry isolated package is recorded, all current documents/tutorial/report
are synchronized and one independent local commit is created.
`real_window=DEFERRED`, `AI-01..AI-08=NOT_RUN`, and human fun, aesthetics,
comfort, retention and physical input feel remain `NOT_CLAIMED`.
