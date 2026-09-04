# Architecture Lab C2 Machine Runtime Report v1

> Status: Done
>
> Batch: `C2 Machine Runtime v0`
>
> Date: 2026-09-04

## 1. Approved problem

Furnace was the only real timed processor, so extracting a generic machine
framework from it alone would have encoded guesses. C2 was separately approved
with one playable second processor: a Hand-Cranked Crusher that can be crafted,
placed, operated through normal Use/UI commands, unloaded, saved and reopened.

The batch was limited to the behavior proven common by Furnace and Crusher. It
did not authorize `MechanicalPort`, topology, power transmission, automatic
transport/storage/crafting, a generic Registry, C3-C11, Track D or Extended
capabilities.

## 2. Playable implementation

The append-only block/material identities are `BlockId::Crusher = 26` and
`Material::ID::Crusher = 42`. Recipe 25 is a workbench-only shaped recipe using
five Cobblestone, two Oak Planks and one Iron Ingot. The only C2 process is the
code-owned `hellomine:crush_cobblestone`: one Cobblestone becomes one Sand after
40 fixed ticks.

A normal Use both opens the container and adds 20 ticks of manual power, capped
at 40. The same bounded command is available through the generic container UI,
so one completion requires two actual crank inputs. The Crusher has one
insert/extract input slot and one extract-only output slot. Missing input,
blocked output and exhausted power pause without losing partial progress or
stored power; completion consumes and produces atomically.

## 3. Runtime and ownership boundary

`MachineRuntime` is a pure, Ogre-free and persistence-free transition helper.
It derives `Idle`, `MissingInput`, `BlockedOutput`, `NoPower` or `Running` from a
copied recipe, slots, progress and available power, then applies at most one
fixed tick. It owns no block entity, registry, thread, scheduler or serialized
state.

Furnace now delegates its shared recipe/output/power/progress transition while
retaining fuel loading, burn accounting, lighting, `SmeltCompletedEvent` and
its byte-compatible v1 payload. Crusher retains crank admission, its two slots
and its own strict payload. `WorldSimulation` ticks both only in the existing
`BlockEntitySimulation` phase, preserving the eight-phase order and 20 Hz
ownership.

C1 capability access now has three real provider blocks. Chest remains
inventory-only; Furnace and Crusher expose `InventoryProvider` plus
`MachineProcessor`. The processor view contains copied status, recipe,
progress/power and manual-power support, and all commands revalidate current
block and block-entity identity before delegating to the concrete owner.

## 4. Persistence and economy

Crusher payload v1 stores input, output, progress and remaining crank power.
Strict parsing rejects unknown items, invalid slots and out-of-range timers.
Breaking spills both slots; chunk unload/reload and world save/reopen preserve
exact progress and power without offline catch-up.

Chunk v2 already stores validated block-entity type/payload pairs, so world save
remains v12 and needs no migration. Terrain v4, settings v8 and Furnace payload
v1 remain unchanged. The code-owned resource-economy verification input advances
to schema v2 only to add explicit machine transformations. It proves Crusher
reachability and keeps the combined crafting/smelting/machine graph acyclic and
conservative without changing the 8 tools, 34 objectives or victory path.

## 5. Focused evidence

The C2 static boundary gate is:

```powershell
& .\tools\validate_machine_runtime.ps1 -Root (Get-Location).Path
```

It passes with two concrete processors, five statuses, one Crusher process,
crank cap 40, save v12, no Registry and no `MechanicalPort`. The focused runtime
path is:

```powershell
$env:HELLOMINE3D_WORLD_SMOKE_FOCUS = "C2-MACHINE"
& .\bin\HelloMine3DWorldRuntimeSmoke.exe
```

VS2017/v141 Debug and Release both pass `51/51`: 25 retained Furnace checks and
26 C2 checks cover pure state precedence, exact tick/completion behavior,
normal craft/place/use/capability flow, slot and crank bounds, conservation,
malformed/stale/mismatched access, break spill, unload/reload and save/reopen.
Recipe/economy validation passes `126/126`; the terrain-atlas validator passes
`281/281`, both locales contain 425 keys and the resource manifest contains 85
entries.

## 6. Complete-gate evidence and identity

The final command was:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow
```

It passes full VS2017/v141 Debug and Release rebuilds with zero errors. In both
configurations WorldRuntime passes `963/963`; Recipe passes `126/126` and
Resource Pack passes `80/80`. Both short soaks finish with zero failures, both
hidden validation clients pass, startup-error diagnostics pass `15/15`, crash
diagnostics and executable inventory pass, and prior Architecture Lab gates
remain green.

The Release executable is 8,832,000 bytes with SHA-256
`EB9FC163047A967F5022BD5839F431BA94FD80C02CD6759FE3A72D24B1744DB9`.
The isolated Windows package contains 105 entries and hashes to
`B4D73704A93B4377EB26336592448B4E31439387BA6198B49C59704517775739`.
The final engineering result is `PASS real_window=DEFERRED`.

No OS Computer Use or new visual/physical-input judgment was required for this
headless architecture batch. `AI-01..AI-08` therefore remain `NOT_RUN`; human
fun, aesthetics, comfort, retention and physical-input feel remain
`NOT_CLAIMED`.

## 7. Exit and next authority

C2 is `Done` after the synchronized documentation and independent local commit.
C3 is only a candidate. C2 does not grant implementation authority for C3-C11,
B7-B9, Track D, `MechanicalPort`, networks or automatic logistics.
