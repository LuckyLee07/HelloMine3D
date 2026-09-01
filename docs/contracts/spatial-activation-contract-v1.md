# B6 Spatial Activation Contract v1

> Status: Frozen after B6 verification
>
> Workflow state: Done
>
> AI gameplay result: `NOT_RUN`
>
> Human subjective claims: `NOT_CLAIMED`

## 1. Problem and current evidence

B2 merges Player, Camera, TeleportDestination and Preload into deterministic
Chunk targets. B3-B5 then send every target through the same load-to-mesh
pipeline, while `collectSectionMeshSnapshot` exposes every loaded section and
unload eligibility is only the current camera render-distance square. This
cannot express a real distinction between data needed for a preload, data that
needs a near Ogre representation, and space whose simulation is interesting.

B6 introduces that distinction without changing simulation fidelity. It does
not add Far data, a Far renderer, LOD jobs or D2 Full/Reduced/Dormant behavior.

## 2. Frozen vocabulary and invariants

`World/Streaming/SpatialInterest.*` owns these three independent outputs for a
Chunk coordinate:

```cpp
struct SpatialInterest {
    bool requiresResidentData;
    bool requiresNearRepresentation;
    bool requestsSimulation;
    std::uint32_t reasonMask;
};
```

The legal hierarchy is:

```text
requestsSimulation => requiresNearRepresentation => requiresResidentData
```

The observable classifications are `Outside`, `ResidentData`,
`NearRepresentation` and `SimulationRequested`. They are derived vocabulary,
not Chunk lifecycle states. B6 deliberately has no
`requiresFarRepresentation` field and does not pre-register a B7-B9 slot.

## 3. Source policy and spatial rings

The first policy is intentionally tied to current real demands:

| Demand reason | Resident data | Near representation | Simulation request |
| ------------- | ------------- | ------------------- | ------------------ |
| Player | within its B2 radius | within its B2 radius | within Chebyshev radius 2 |
| Camera | within its B2 radius | within its B2 radius | no |
| TeleportDestination | within its bounded radius | no; Player/Camera claims it on the next normal update | no |
| Preload | within its bounded radius | no | no |

Overlapping reasons merge by logical OR and retain the existing B2 reason
mask. The two-Chunk simulation-request radius corresponds to the current
32-block local gameplay neighborhood, but is only a published request. B6 does
not use it to skip, reduce or reschedule Actor, combat, crop, furnace, random
tick, population, encounter or Gameplay work.

The three concrete rings are therefore:

```text
SimulationRequested — resident + near + simulation request
NearRepresentation  — resident + near
ResidentData        — resident only
Outside             — no current demand
```

## 4. Snapshot ownership and determinism

`SpatialInterestModel` is a pure, Ogre-free derivation of the immutable B2
`ChunkDemandSnapshot`. It emits one de-duplicated cell per coordinate, sorted
by x then z, and carries the source demand revision. Queries are read-only and
return `Outside` for absent coordinates.

`ChunkRuntime` owns the current copied snapshot under its existing demand
mutex. It rebuilds only after a semantic demand revision: initial Player
publication, Player/Camera movement or radius change, transient demand
publication/expiry. Camera-orientation-only priority replans do not mutate the
spatial snapshot. The snapshot is derived runtime state and is never saved.

## 5. Real consumers

- B3 planning admits only cells with `requiresResidentData`.
- A completed load follows into `ChunkMeshBuild` only while the current cell
  requires near representation; resident-only demand stops after data is
  resident.
- `collectSectionMeshSnapshot` exposes live/ready sections only for current
  near-representation cells, and upload acknowledgement rechecks that interest.
- Ogre removes a former near visual through the existing live-section
  reconciliation; B6 adds no renderer-owned world truth.
- Distant unload preserves every current resident-interest cell, then applies
  the unchanged B5 limit of eight unloads per update to all other eligible
  chunks.
- `requestsSimulation` is published only through the copied spatial snapshot
  and diagnostics. No current simulation system consumes it.

## 6. Diagnostics

Copied developer diagnostics expose source revision, total cells, Resident
Data, Near Representation and Simulation Requested counts. Counts are nested
and bounded by the B2 target cap. No coordinate list is copied into the Ogre
panel and no diagnostic value influences behavior.

## 7. Automated evidence

`HELLOMINE3D_WORLD_SMOKE_FOCUS=B6` must prove:

1. exact field/classification vocabulary and hierarchy;
2. an absent coordinate is `Outside`;
3. Player produces nested simulation and near rings;
4. Camera produces near representation without simulation;
5. TeleportDestination and Preload are resident-only;
6. overlapping reasons merge flags and reason masks deterministically;
7. output order and demand revision are stable;
8. radius changes remove obsolete cells without stale interest;
9. resident-only demand does not enter the mesh follow-up path;
10. mesh snapshots exclude resident-only loaded sections;
11. current resident interest prevents camera-distance unload while the
    existing eight-per-update bound remains intact;
12. simulation-interest publication does not change existing fixed-tick
    behavior or metrics.

`tools/validate_spatial_activation.ps1` must freeze module ownership, exact
vocabulary, the three source policies, diagnostics, one loader, unchanged B5
budgets and the absence of B7-B9/D2 behavior. B5/B4/B3/B2/B1 focused
regressions and every AL-A1..A6/B1..B6 static gate must continue to pass.

## 8. Compatibility and exit boundary

B6 preserves save v12, terrain v4, settings v8, resources, Gameplay, the
AL-A1 78-method World public surface, one loader, B3 ordering, B4 generation
cancellation and B5 caps/watermarks/8-8-8 consumer budgets. It does not
implement Far representation, render/simulation world separation, LOD jobs,
simulation degradation, Track C/D or any Extended capability.

B6 becomes `Done` only after VS2017/v141 Debug and Release pass the complete
`scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow` gate, the
isolated package identity is recorded, documents/tutorial/report are
synchronized and one independent local commit is created. Real-window launch
remains `DEFERRED`; `AI-01..AI-08=NOT_RUN`; human fun, aesthetics, comfort,
retention and physical input feel remain `NOT_CLAIMED`.
