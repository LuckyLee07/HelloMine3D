# B2 Streaming Demand Model Contract v1

> Status: Frozen after B2 verification
>
> Runtime behavior: replace the single implicit load center with a bounded,
> mergeable and expiring description of the loading demand that already exists
>
> Authoritative task status: `docs/current/todolist.md`

## 1. Purpose and approved scope

B2 makes the current Player, Camera, teleport destination and explicit preload
sources visible to one `ChunkRuntime` demand model. It changes how the existing
loader derives its pending target order; it does not add a generic job system,
another queue, another worker or new world representation.

The model is derived runtime state. It is not Gameplay state, is never saved,
and does not change save v12, terrain v4, settings v8, resources, recipes,
objectives or the frozen 78-method `World` public surface.

## 2. Frozen vocabulary and values

The exact implemented reasons are:

```text
Player
Camera
TeleportDestination
Preload
```

Unimplemented proposal reasons such as Machine, Actor and DebugCamera are not
pre-registered. A concrete demand value contains:

```cpp
struct ChunkDemand {
    VectorXZ coord;
    ChunkDemandReason reason;
    int priority;
    std::uint64_t epoch;
    std::uint64_t expiresAfterEpoch;
    int radius;
};
```

Priorities and lifetimes are stable B2 policy:

| Reason | Priority | Lifetime after refresh | Source |
| ------ | -------- | ---------------------- | ------ |
| `TeleportDestination` | 400 | 120 demand epochs | successful same-world teleport |
| `Player` | 300 | 2 demand epochs | current player Chunk on `World::update` |
| `Camera` | 200 | 2 demand epochs | current camera Chunk on `World::update` |
| `Preload` | 100 | 30 demand epochs | existing explicit/spawn/respawn preload path |

One slot exists per implemented reason, so the active raw set is bounded to
four values without introducing B5 queue backpressure. Refreshing a reason
replaces its previous coordinate. Refreshing identical semantic data extends
its expiry but does not churn the plan revision.

## 3. Epoch and expiry semantics

- The demand epoch advances once per `World::update` before Player and Camera
  are refreshed.
- A demand is active through `expiresAfterEpoch` and expires when the current
  epoch becomes greater than that value.
- Expiry and semantic replacement increment a monotonic demand revision.
- Teleport and Preload are transient. A previous destination therefore cannot
  permanently dominate future work.
- Player and Camera remain active while normal updates refresh them; their
  short lifetime makes loss of either source observable and bounded.

Epochs are local runtime counters. They are not wall-clock timestamps and are
not serialized.

## 4. Merge and target planning

Each active demand expands to its inclusive square radius. Target coordinates
from overlapping demands are de-duplicated. A merged target retains the union
of reason bits, the highest reason priority, the newest contributing epoch and
the shortest distance to a contributing center.

The existing loader receives one deterministic target sequence ordered by:

1. higher merged reason priority;
2. current frustum intersection;
3. ahead / lateral / behind rank relative to the latest Player Chunk motion;
4. newer contributing epoch;
5. shorter squared then Manhattan distance to a contributing center;
6. stable `x`, then `z` coordinate order.

This ordering intentionally uses more than distance. A camera turn still bumps
the existing mesh-priority revision, and a semantic demand change bumps the
demand revision; either event discards and rebuilds the derived pending target
plan. This is pending-plan replacement, not B4 in-flight cancellation.

## 5. Integration and ownership

- `ChunkRuntime` owns `ChunkDemandModel`, its lock, Player motion and the last
  planned-target count.
- `World::update` refreshes Player and Camera through the existing runtime
  boundary.
- The existing public `World::preloadAround` records `Preload` and retains its
  synchronous compatibility behavior.
- `WorldManager` records `TeleportDestination` through a private World bridge
  before publishing the existing `PlayerTeleportEvent`; cross-world rejected
  teleports create no demand.
- Spawn and respawn remain `Preload`; no speculative PlayerSpawn reason is
  added.
- `ChunkManager` and the B1 Data/Mesh states do not own demand values.

The existing camera/render-distance unload rectangle remains unchanged. B2
does not redefine simulation or representation interest; that belongs to B6.

## 6. Debug visibility

The copied developer snapshot and panel expose:

- current demand epoch and revision;
- active raw demand count and one count for each of the four reasons;
- cumulative expired demand count;
- last de-duplicated planned target count.

Diagnostics are copied values. Reading them cannot refresh, expire or reorder
demand.

## 7. Frozen budgets and explicit non-goals

B2 preserves:

- one loader thread;
- the 6 ms loader pass budget;
- 64 targets per pass and one Chunk load per target;
- the two-section synchronous rebuild budget;
- the eight-Chunk unload budget;
- B1 Data, Mesh and Render state vocabularies and owners.

B2 does not introduce `WorldJob`, a Generic Job Scheduler, worker pools,
priority classes shared by job types, cancellation tokens, in-flight
cancellation, backpressure/watermarks, Spatial Interest, activation/LOD, Far
Terrain or B7-B9 Extended work.

## 8. Acceptance

B2 may be marked `Done` only when:

1. a focused static validator freezes the four reasons, values, owner,
   integration sources, diagnostics and absence of B3-B9 capabilities;
2. deterministic runtime checks prove one-slot replacement, overlap merge,
   exact expiry, teleport priority, forward-motion ordering and turn-triggered
   replanning;
3. integrated World/WorldManager checks prove Player+Camera merge, explicit
   Preload visibility, successful teleport elevation and rejected cross-world
   teleport non-mutation;
4. the existing synchronous teleport/preload gameplay behavior remains green;
5. the AL-A1 through B1 gates still pass;
6. `scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow`
   passes Debug/Release and the isolated 104-entry package;
7. roadmap, architecture, runtime validation, tutorial, task ledger and a B2
   report are synchronized before the independent local commit.

AI gameplay remains `NOT_RUN` unless separately executed through the active
package-only protocol. Human fun, aesthetics, comfort and physical input feel
remain `NOT_CLAIMED`.
