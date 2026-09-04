# Architecture Lab C1 Block Capability Report v1

> Status: Done
>
> Batch: `C1 Block Capability Model`
>
> Date: 2026-09-04

## 1. Approved problem

The real pressure was the existing container UI probing Furnace and then Chest
and invoking both concrete implementations. C1 was separately approved after
B10. The batch was limited to a capability access protocol for the two
persisted providers already present in normal gameplay.

## 2. Implemented boundary

`BlockDefinition` now owns one optional `BlockCapabilityDefinition` alongside
its behavior. The existing `BlockDatabase` declares:

| Block | InventoryProvider | MachineProcessor |
| ----- | ----------------- | ---------------- |
| Chest | Chest adapter | none |
| Furnace | Furnace adapter | Furnace adapter |

`BlockCapabilityAccess` validates loaded block identity against its expected
block-entity record and returns small value handles. `InventoryProvider`
exposes copied slots and delegates commands to the existing container rules;
`MachineProcessor` exposes copied Furnace progress/fuel data but owns no tick,
recipe or persistence state. Every access revalidates current identity, so a
handle becomes harmless after the block changes.

`OgreUserInterface::drawContainer` is the real consumer. It now discovers and
operates the open container through the capability handles; it no longer
includes or probes the Chest/Furnace concrete container classes.

## 3. Deliberately absent work

No Capability Registry, `MechanicalPort`, item transport, storage provider,
Machine Runtime, mechanical node/network, automation recipe, C2-C11, Track D,
B7-B9 or Extended capability was added. The missing `MechanicalPort` is a
deliberate demand-driven decision: no approved concrete mechanical node exists
in C1.

## 4. Focused evidence

VS2017/v141 Debug and Release build both `HelloMine3DWorldRuntimeSmoke` and
the Ogre client successfully. The hidden focused command is:

```powershell
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "C1-CAP"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

It passes `17/17` in both configurations. Coverage includes both provider declarations, slot roles,
Chest automatic insertion, Furnace dedicated/output rules, copied processor
progress, ordinary/missing/mismatched/malformed records, stale-handle failure
and save/reopen derivation from existing v12 state. The ordinary Debug runtime
passes `937/937` after adding these seventeen assertions.

`tools/validate_block_capability_model.ps1` passes with two concrete providers,
two capabilities, no capability Registry and no mechanical port. It is wired
into `scripts/verify_build.ps1`.

## 5. Complete-gate evidence and identity

The implementation does not change the 24 recipes, eight tools, 34 objectives,
resource economy, victory path, terrain v4, settings v8, save v12, Chest or
Furnace payloads, World public surface, streaming budgets or simulation order.

The complete
`scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow` gate
passes in both Debug and Release: WorldRuntime `937/937`, resource pack
`80/80`, recipe/economy `122/122`, startup-error negatives `15/15`, both
short soaks with zero failures, both hidden validation clients and the exact
executable inventory. The Release executable is 8,815,616 bytes with SHA-256
`8CB7FF4C2BEB700AF88082D58F53C7848EFA43E0BA03FBAA4F69C35D1875CC12`.

The isolated Windows package contains 104 entries and hashes to
`1618ACD7995FE5181169B0B46A5D4F479F63FA1CCB8B533B358ED694A3846EB6`.
The full result is `PASS real_window=DEFERRED`. Real-window/Computer Use is not
required for this non-visual protocol batch: `AI-01..AI-08=NOT_RUN`; human
fun, aesthetics, comfort, retention and physical input feel remain
`NOT_CLAIMED`.
