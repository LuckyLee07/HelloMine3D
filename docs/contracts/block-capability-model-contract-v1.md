# C1 Block Capability Model Contract v1

> Status: Frozen after C1 verification
>
> Workflow state: Done
>
> AI gameplay result: `NOT_RUN`
>
> Human subjective claims: `NOT_CLAIMED`

## 1. Real problem and approved scope

The playable game already has two persisted inventory-bearing blocks. Chest
owns a nine-slot general inventory; Furnace owns input, fuel and output slots
plus processing progress. The container UI currently discovers which one is
open by probing Furnace and then Chest and calls both concrete implementations
directly. That repeated concrete-type dispatch is the real C1 pressure.

C1 introduces a lightweight access protocol for those two existing providers.
It does not create a machine runtime, a mechanical block or a network merely to
justify an abstraction.

## 2. Frozen capability vocabulary

Only two capabilities have concrete providers in this batch:

```text
InventoryProvider — Chest, Furnace
MachineProcessor  — Furnace
```

`MechanicalPort` remains a C3 concern because C1 has no concrete mechanical
node. `ItemTransportPort`, `StorageProvider` and every other Extended
capability remain absent. C1 must not add empty interfaces, placeholder slots
or registration APIs for them.

Capability identity is declared on the existing `BlockDefinition` owned by
`BlockDatabase`; there is no second Registry. A declaration includes the
expected persisted block-entity type, inventory adapter kind and processor
adapter kind. Discovery succeeds only when the loaded block, its definition
and its block-entity record agree.

## 3. Access protocol

`BlockCapabilityAccess` is an Ogre-free query boundary. It returns small value
handles rather than exposing mutable block-entity payloads:

- `InventoryProvider` exposes a copied slot view and delegates insert/extract
  commands to the existing Chest/Furnace rules;
- Chest uses automatic insertion; Furnace requires an explicit input or fuel
  slot and keeps output extract-only;
- `MachineProcessor` exposes copied progress/fuel state for the existing
  Furnace and does not own ticking, recipes or persistence;
- every operation revalidates current block/entity identity through the
  concrete container, so a stale handle fails closed after block replacement.

The existing Chest/Furnace serializers remain the only owners of payload
format and validation. Capability handles never cache authoritative inventory
or processing state.

## 4. Real consumer and observable evidence

`OgreUserInterface::drawContainer` must discover the open block through
capabilities. It may render a processor-specific layout when
`MachineProcessor` exists and a general inventory layout otherwise, but it
must not probe both concrete container types to choose the UI.

The focused runtime test and static gate are the observable developer evidence.
C1 does not require a new gameplay recipe, objective, save field or screen.

## 5. Automated evidence

`HELLOMINE3D_WORLD_SMOKE_FOCUS=C1-CAP` must prove:

1. Chest declares only `InventoryProvider` and exposes nine slots;
2. Furnace declares `InventoryProvider` plus `MachineProcessor` and exposes
   its three role-preserving slots and progress state;
3. a normal block, a missing record and an unknown/mismatched record expose no
   capability;
4. Chest automatic insertion/extraction still follows its existing rules;
5. Furnace rejects output insertion and preserves input/fuel/output rules;
6. a handle obtained before block replacement fails closed;
7. capability state remains derived from existing persisted payload after a
   save/reopen path, without a new save version;
8. the focused test does not create a mechanical node, transport or network.

`tools/validate_block_capability_model.ps1` must freeze the two concrete
capabilities, the two providers, UI consumption, absence of a second Registry
and absence of C2/C3/Extended vocabulary. It is added to the complete Windows
gate. Existing Chest, Furnace, resource-economy and playable-journey tests
remain mandatory regressions.

## 6. Compatibility boundary

C1 preserves the 24 recipes, eight tools, 34 objectives, resource economy,
victory path, terrain v4, settings v8 and save v12. It does not modify block or
material IDs, serialized Chest/Furnace payloads, recipe ownership, smelting
timing, fixed-tick order, World public surface, streaming budgets or render
ownership.

If implementation requires any such change, C1 pauses for a new approval
instead of silently expanding scope.

## 7. Explicit non-goals and exit

C1 does not implement C2-C11, Track D, B7-B9, Machine Runtime, MechanicalPort,
network topology, automation recipes, item transport, shared network core,
generic component/ECS infrastructure or a capability Registry.

C1 becomes `Done` only after focused tests and negative cases pass, the static
gate passes, VS2017/v141 Debug and Release pass the complete
`scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow` gate,
documents/tutorial/report are synchronized and one independent local commit is
created. Real-window and `AI-01..AI-08` remain `NOT_RUN`; fun, aesthetics,
comfort, retention and physical-input feel remain `NOT_CLAIMED`.
