# B4 World Job Cancellation & Generation Token Contract v1

> Status: Frozen after B4 verification
>
> Workflow state: Done
>
> AI gameplay result: `NOT_RUN`
>
> Human subjective claims: `NOT_CLAIMED`

## 1. Problem and scope

B4 extends the B3 typed World job scheduler so a semantic streaming-plan
change can invalidate obsolete pending and in-flight work before that work
publishes authoritative Chunk data or CPU mesh state. It covers the two B3
job types only:

- `ChunkLoadOrGenerate`;
- `ChunkMeshBuild`.

B4 does not add a worker pool, queue capacity, watermarks, admission/shedding
policy, Spatial Interest, far terrain, machine/network work, a generic
Simulation Scheduler or any B7-B9/Track C/D capability.

## 2. Generation token

`WorldJobGenerationToken` is a copied, unsigned 64-bit generation identity.
The scheduler starts at generation 1. Every accepted request and resulting
`WorldJob` carries the exact current token; token 0 is invalid.

The following semantic plan changes invalidate the current generation:

- Player or Camera demand coordinate/radius changes;
- TeleportDestination or Preload demand is added, replaced or expires;
- render distance changes;
- chunk meshes are reset;
- loader shutdown begins.

Camera/frustum priority-only reordering does not invalidate an in-flight job:
it changes pending order on the next B3 plan replacement but does not make the
Chunk result semantically unwanted.

Invalidation is monotonic, clears obsolete pending jobs immediately and marks
an existing in-flight token stale without blocking its off-lock CPU/IO work.
Repeated invalidation cannot wrap to token 0; overflow is a hard logic error.

## 3. Cooperative cancellation boundaries

Cancellation is cooperative. A worker checks the copied token:

1. before starting expensive detached work;
2. after detached work and before authoritative commit;
3. before publishing a same-token follow-up job.

Work already running may finish and be discarded. B4 explicitly permits this
"white work"; it does not promise pre-emption inside terrain generation,
storage reads or mesh construction.

`WorldJobOutcome` is extended by exactly one value, `Cancelled`. A cancelled
job still follows `Pending -> InFlight -> Completed`; cancellation is an
outcome, not a fourth scheduler state.

## 4. Atomic commit boundary

Token validation and authoritative commit are linearized by
`ChunkRuntime`'s generation/commit boundary while holding the existing World
mutex. A demand invalidation may run during detached work, but it cannot race
between the final token check and publication.

Two revision guards remain independent:

- generation token rejects work made obsolete by a newer streaming plan;
- B1 Chunk/section revision rejects work made obsolete by authoritative data
  mutation within the same generation.

A generation rejection is reported as `Cancelled`; a B1 revision rejection
remains `CommitRejected`.

## 5. Detached Chunk load/generation

The B3 load/generate job is split into three phases:

1. under the World lock, reserve one missing neighbour and create a detached
   candidate with the existing `Requested -> Loading` lifecycle;
2. outside the World lock, read storage or deterministically generate the
   candidate;
3. under the World lock and atomic generation boundary, either publish the
   candidate and existing events/light reconciliation, or discard it.

Cancellation rolls the reserved placeholder from `Loading -> Absent`. This is
the only B4 extension to the B1 Data Residency graph. A synchronous load that
won the same coordinate first remains authoritative and is never overwritten
by the detached candidate.

Detached storage hydration must not mutate the live World's random-tick index.
No load/generated event, block-light reconciliation, save dirtiness or
resident Chunk is published until the candidate commit succeeds.

## 6. Mesh cancellation

Mesh input capture and CPU construction retain the B3 lock split. If the token
becomes stale, `ChunkMeshBuild` discards the built client data and rolls the
exact still-Building section back to `Dirty`. A section whose block revision
already changed is already dirty and is not overwritten.

No cancelled mesh reaches `CpuReady`, render upload or mesh rebuild metrics.

## 7. Scheduler admission and plan replacement

- `submit` rejects token 0 and stale-token requests;
- `replacePending` accepts only the exact current token;
- invalidation clears all old pending work while preserving the one in-flight
  record until it completes as `Cancelled` or wins an earlier valid commit;
- a cancelled job publishes no follow-up;
- B3 ordering among current-generation jobs is unchanged.

The queue remains bounded by replacement of the current finite B2 plan. B4
does not claim a hard capacity or pressure policy; those belong to B5.

## 8. Diagnostics

`WorldJobSchedulerDebugStats` adds copied counters for:

- current generation;
- generation invalidations;
- pending jobs cancelled by invalidation;
- stale submit rejections;
- stale plan replacement rejections;
- completed `Cancelled` jobs.

The developer panel exposes these values with existing pending/in-flight,
type/outcome totals and last timing values. Token/cancellation diagnostics are
derived state and are excluded from save v12.

## 9. Automated evidence

B4 focused checks must prove:

1. token and `Cancelled` vocabulary;
2. monotonic invalidation, pending clear and in-flight preservation;
3. stale/zero submit and stale plan rejection without counter corruption;
4. current-generation B3 ordering remains deterministic;
5. cancellation completion and diagnostic identities are exact;
6. detached load/generate cancellation publishes no resident data or events;
7. a valid detached candidate commits once and preserves storage/generation
   behavior;
8. stale mesh work returns the section to `Dirty` and never adopts its output;
9. rapid move/reverse/teleport/render-distance-style invalidation stress has
   no stale commit, deadlock or pending accumulation;
10. the real background World pipeline observes both valid work and at least
    one generation advance without changing Gameplay/save identities.

The composed gate retains B3, B2 and B1 focused regressions and all AL-A1..A6
static boundaries.

## 10. Compatibility and exit

B4 preserves save v12, terrain v4, settings v8, resource identities, gameplay,
the AL-A1 78-method public World surface, one loader worker and the existing
6 ms / 64-target pass budgets.

B4 may be marked `Done` only after VS2017/v141 Debug and Release pass the full
`scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow` gate, the
clean package is recorded, documents/tutorial/report are synchronized and one
independent local commit is created. `real_window=DEFERRED`, `AI-01..AI-08`
remain `NOT_RUN`, and human fun/feel/aesthetics remain `NOT_CLAIMED`.
