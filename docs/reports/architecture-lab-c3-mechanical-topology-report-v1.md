# Architecture Lab C3 Mechanical Topology Report v1

> Status: Done
>
> Batch: `C3 Mechanical Topology Model v0`
>
> Date: 2026-09-04

## 1. Approved problem

C2 made the Hand-Cranked Crusher a real craftable, placeable and persisted
machine. C3 was separately approved to use that one real node to demonstrate
the dynamic-graph problem: face-adjacent nodes must merge, a removed bridge must
split the graph, and Chunk unload/reload or world save/reopen must reconstruct
the same explainable topology.

The batch did not authorize power propagation, RPM, direction, torque, stress,
rotation-dependent ports, new blocks or recipes, automatic processing or
logistics, a generic network framework, C4-C11, Track D or Extended abilities.

## 2. Concrete topology and ownership

`MechanicalTopology` owns a Crusher-only graph. A node id is its exact world
position. Six block faces define possible connections; positive X/Y/Z
enumeration records each undirected edge once. A deterministic breadth-first
rebuild sorts nodes by X/Y/Z and uses the minimum node as the component's
network id.

The graph is derived synchronously under the existing World mutex. Loaded
Chunk block/entity records remain authoritative. A node exists only when its
block is Crusher, its entity type matches, and its C2 payload parses strictly.
World block/entity mutations reconcile one position, successful Chunk loads
replace one Chunk's nodes, and a Chunk is removed from the graph only after its
existing save/unload path succeeds. Duplicate observations are no-ops and do
not advance the topology revision.

## 3. Playable observation

Crusher is the only block that declares the concrete six-face
`MechanicalPort`. The capability handle revalidates current block/entity truth
and returns a copied node snapshot; malformed, mismatched, stale or unloaded
state fails closed.

The normal Crusher container now shows network id, component node count and
connection count in both locales. The developer panel additionally shows total
nodes, edges, components, revision, rebuild count, last visited nodes and dirty
state. Topology does not change C2 processing: each Crusher still requires its
own manual crank pulses.

## 4. Persistence and compatibility

No topology, component or network id is serialized. Chunk v2 already persists
the authoritative Crusher block/entity/payload pair, so C3 rebuilds the graph
after load and keeps world save v12 unchanged. Terrain v4, settings v8, Furnace
payload v1, Crusher payload v1, 25 recipes, 8 tools and 34 objectives also
remain unchanged.

The C1 and C2 static gates were updated only to recognize the separately
approved downstream C3 port. They still reject Registry/Extended work, and C2
continues to freeze independent hand power with no network transmission.

## 5. Focused and failure evidence

The C3 static boundary gate is:

```powershell
& .\tools\validate_mechanical_topology.ps1 -Root (Get-Location).Path
```

It passes with one Crusher node kind, six faces, exactly 17 C3 assertions,
deterministic rebuild, save v12, no generic network and no C4 power vocabulary.
The focused runtime path is:

```powershell
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "C3-TOPOLOGY"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

Debug passes `68/68`: 51 retained C2 checks plus 17 C3 checks. The C3 cases
cover empty/isolated graphs, all-axis and negative-coordinate adjacency,
canonical edges, cross-Chunk merge, bridge split, reconnect, no-op revision,
Chunk replace/remove, normal place/break, non-Crusher exclusion,
malformed/stale capability failure, unload/reload and save/reopen.

The first focused run retained an honest `63/68` result. Five cascading checks
failed because the fixture tried to break a tier-1 Crusher without a pickaxe,
therefore received no drop and had no held Crusher for the reconnect step. The
fixture was corrected to obtain four Crushers through the normal inventory
path and reserve one for replacement. No gameplay rule, topology rule or
threshold was weakened.

## 6. Complete-gate evidence and identity

The final command was:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
```

VS2017/v141 Debug and Release rebuild with zero errors. Both configurations
pass WorldRuntime `980/980`; Recipe passes `126/126`, Resource Pack passes
`80/80`, and startup-error diagnostics pass `15/15`. All thirteen headless
targets, both short soaks, both hidden validation clients, crash diagnostics,
resource checks and executable inventory pass.

The Release executable is 8,860,672 bytes with SHA-256
`61F771F24DC46C830C488E7DEFAD0C234B2035C449172A80F2B41F7537244054`.
The isolated Windows package contains 105 entries and hashes to
`8CC3ED1FC37A0F115D57C3C56349BE3278AA4B35D9DAD33975D9B45B3D46776F`.
The final engineering result is `PASS real_window=DEFERRED`.

No OS Computer Use or new subjective visual/input judgment was required for
this headless architecture batch. `AI-01..AI-08` therefore remain `NOT_RUN`;
human fun, aesthetics, comfort, retention and physical-input feel remain
`NOT_CLAIMED`.

## 7. Exit and next authority

C3 is `Done` after synchronized current documentation and an independent local
commit. Its full-rebuild v0 trade-off is explicit and measurable; incremental
topology is not required until real scale evidence justifies it.

C4 and D1 remain unapproved candidates. C3 does not grant implementation
authority for B7-B9, C4-C11, Track D, power propagation, generic networks,
automatic logistics or Extended abilities.
