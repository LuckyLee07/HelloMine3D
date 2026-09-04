# C3 Mechanical Topology Model v0 Contract v1

> Status: Frozen for C3 implementation
>
> Workflow state: Done
>
> AI gameplay result: `NOT_RUN`
>
> Human subjective claims: `NOT_CLAIMED`

## 1. Real problem and approved scope

C2 provides one real, optional machine whose placement already has spatial
meaning: the Hand-Cranked Crusher. C3 uses that playable object to prove the
dynamic-graph problem before any power simulation exists. Loaded Crushers that
touch on a block face form one concrete Mechanical component. Placing or
removing a Crusher, unloading or reloading its Chunk, and reopening a save must
produce an explainable merge or split.

C3 is limited to a Crusher-only topology and a normal container-UI observation
of that topology. It is not a generic network framework and does not authorize
C4 power propagation, a new block/item/recipe, automatic processing, transport,
storage, crafting, a Registry or any C4-C11/Track D work.

## 2. Frozen topology vocabulary and identity

The concrete `Mechanical` module owns exactly the C3 vocabulary:

```text
MechanicalNodeId       = exact world block position
MechanicalNetworkId    = lexicographically smallest node position in a component
MechanicalPort         = one of the six block faces of a real Crusher node
MechanicalConnection   = one canonical face-adjacent node pair
MechanicalComponent    = connected nodes and their connections
```

Node and network identity is deterministic and collision-free. Position order
is X, then Y, then Z. A component id is therefore stable while its anchor node
remains present; adding or removing a lower-positioned node may intentionally
change it. Connections exist only across `-X/+X/-Y/+Y/-Z/+Z`, and each pair is
counted once.

`BlockDefinition` declares the Crusher as the only `MechanicalPort` provider.
Chest and Furnace remain non-mechanical in C3. A copied node observation exposes
the node id, network id, component node count, connection count, connected-face
mask and topology revision. It never grants mutation authority.

## 3. Authoritative state and rebuild semantics

The topology is derived synchronously from loaded world truth. A position is a
node only when all three facts agree:

1. the Chunk is currently loaded;
2. the block is `BlockId::Crusher`;
3. a matching Crusher block entity has a valid strict C2 payload.

Missing, stale, mismatched or malformed state fails closed and is absent from
the topology. Normal block/entity mutation points reconcile the affected
position. Successful Chunk load replaces that Chunk's derived node set;
successful unload removes it only after the existing persistence boundary has
succeeded. A failed unload must leave both Chunk and topology present.

C3 uses a deterministic breadth/depth connectivity rebuild over currently
loaded Crusher nodes after an actual topology change. Duplicate observations
are no-ops and do not advance the revision. Full rebuild is an explicit v0
tradeoff: the graph is bounded by loaded Crushers, the algorithm is simple to
verify, and no speculative incremental core is introduced. `dirty` is visible
only while a synchronous rebuild is in progress; the published copied snapshot
is coherent.

The topology is accessed only under the existing World main-thread mutex. It
has no worker, event subscriber, background queue or independent persistence.

## 4. Player-visible behavior and compatibility

Using a normally placed Crusher continues to open the C2 container and provide
the same bounded hand-crank command. The container additionally shows:

```text
Network: <anchor world position>
Nodes: <component node count>
Connections: <component connection count>
```

Players can therefore place adjacent Crushers, remove the middle node, replace
it, leave/re-enter the Chunk and save/reopen while observing merge and split.
Topology has no effect on C2 processing: every Crusher still requires its own
manual crank pulses and neither supplies power to another.

No numeric block/material identity changes. The recipe catalogue remains 25,
the tool catalogue remains 8 and the objective catalogue remains 34. World save
v12, Chunk v2, terrain v4, settings v8, Furnace payload v1 and Crusher payload
v1 remain byte-compatible. Topology/component ids are never serialized; they
are rebuilt from authoritative loaded blocks/entities.

## 5. Automated evidence

`HELLOMINE3D_WORLD_SMOKE_FOCUS=C3-TOPOLOGY` must cover at least:

1. empty, isolated, horizontal/vertical/negative-coordinate adjacency and
   canonical connection counting through pure deterministic topology cases;
2. three-node merge, middle-node split, reconnect and stable no-op revision;
3. normal Crusher placement/removal through gameplay interaction;
4. capability discovery, copied observation and stale/malformed fail-closed
   behavior;
5. cross-Chunk unload/reload with successful removal/restoration and no phantom
   edge;
6. save/reopen reconstruction with the same deterministic component id and
   counts under save v12;
7. retained C2 manual-power, payload, economy and prior regression behavior.

`tools/validate_mechanical_topology.ps1` freezes the concrete types, six-face
adjacency, deterministic rebuild, authoritative World/Chunk hooks, Crusher
capability/UI observation, focused test path, non-persistence and absence of C4
or generic-network vocabulary. It is added to `scripts/verify_build.ps1`; all
prior gates remain mandatory.

## 6. Debug and failure evidence

The existing World debug snapshot adds copied topology totals: loaded nodes,
connections, components, revision, rebuild count, nodes visited by the last
rebuild and dirty state. The normal Crusher container is the required player
surface; no standalone C5 topology debugger is implemented.

Invalid payloads, stale capability handles and unloaded nodes return no
observation instead of retaining a phantom network. The original failing run
and its inputs must be retained if the complete gate exposes a defect.

## 7. Explicit non-goals and exit

C3 does not implement RPM, direction, torque, stress, power generation or
transmission, Water Wheels, Shafts, Gears, Drills, belts, pipes, item logistics,
storage indexing, reservations, automatic crafting, offline simulation,
rotation-dependent ports, `INetwork`, `GenericNode`, `DynamicNetworkCore`, a
generic Registry, C4-C11, Track D or B7-B9.

C3 becomes `Done` only when its topology, normal player observation,
focused/negative/static evidence and synchronized current documents pass the
complete VS2017/v141 Debug and Release
`scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow` gate and
one independent local commit is created. Real-window and `AI-01..AI-08` remain
`NOT_RUN`; fun, aesthetics, comfort, retention and physical-input feel remain
`NOT_CLAIMED`.
