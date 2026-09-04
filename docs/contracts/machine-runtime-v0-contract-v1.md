# C2 Machine Runtime v0 Contract v1

> Status: Frozen after C2 verification
>
> Workflow state: Done
>
> AI gameplay result: `NOT_RUN`
>
> Human subjective claims: `NOT_CLAIMED`

## 1. Real problem and approved scope

Furnace is currently the only real timed processor. Its input matching,
output-capacity check, progress mutation and completion transfer are embedded in
the concrete container, so extracting a runtime from Furnace alone would be a
speculative abstraction. C2 adds one playable second processor, the
Hand-Cranked Crusher, and extracts only the state transition proven common by
those two machines.

C2 is limited to `Machine Runtime v0`, the Crusher and the smallest extension
of C1 capabilities required to operate it. It does not authorize a mechanical
network, automatic transport, a capability/recipe registry or any C3+ work.

## 2. Frozen playable Crusher

The new append-only identities are `BlockId::Crusher = 26` and
`Material::ID::Crusher = 42`; every older numeric identity remains unchanged.
The block is obtained through one new workbench-only shaped recipe:

```text
Cobblestone  IronIngot    Cobblestone
OakPlank     empty        OakPlank
Cobblestone  Cobblestone  Cobblestone
                       -> Crusher x1
```

This appends recipe 25 without changing the input, output or discovery behavior
of the existing 24 recipes. It uses only normally obtainable existing
materials, requires iron progression, is optional, and does not enter the 34
objective definitions or the victory path. The existing 8 tools and all of
their progression, durability and harvest semantics remain unchanged.

The versioned, code-owned C2 processing definition is exactly:

```text
id       = hellomine:crush_cobblestone
input    = Cobblestone x1
output   = Sand x1
duration = 40 fixed ticks
```

There is exactly one Crusher process in C2. It is deliberately not backed by a
generic Registry: one definition does not establish a runtime extension model.

The Crusher has one insert/extract input slot and one extract-only output slot.
Using the placed block through the normal gameplay command supplies a 20-tick
crank pulse and opens its container. The processor capability exposes the same
bounded command to the normal UI. Stored manual power is capped at 40 ticks;
use at the cap changes nothing and never creates or consumes an item. Thus a
single item requires two pulses and power cannot become an unbounded bank.

## 3. Machine Runtime v0 semantics

`MachineRuntime` is an Ogre-free, persistence-free state-transition helper. A
concrete adapter supplies its copied recipe, input/output slots, progress and
available power; the Runtime returns status and applies at most one fixed tick.
It owns only behavior already shared by Furnace and Crusher:

```text
recipe match -> output capacity -> power -> one progress tick -> atomic output
```

The copied observation is `MachineState { status, recipeId, progressTicks }`.
Status precedence and meaning are frozen:

| Status | Meaning |
| ------ | ------- |
| `Idle` | Input is empty and no power is currently stored. |
| `MissingInput` | Stored power exists but input is empty, or the input has no matching concrete recipe. |
| `BlockedOutput` | A matching input exists but output identity/capacity cannot accept one completion. |
| `NoPower` | Input and output are ready but the concrete power adapter has no available tick. |
| `Running` | This fixed tick may consume one power tick and advance progress. |

`BlockedOutput` pauses both progress and power. `MissingInput` and `NoPower`
also preserve partial progress. Completion consumes exactly the recipe input,
produces exactly its output, resets progress to zero and reports completion to
the concrete adapter. Invalid definitions or impossible state fail closed.

Furnace remains owner of fuel loading, burn totals, lighting, its existing
`SmeltCompletedEvent` and payload. Crusher remains owner of crank admission,
its two slots and payload. Neither concrete serializer persists the derived
status or recipe id.

## 4. Persistence, economy and compatibility

Crusher uses a strict payload:

```text
v1|<input-id>,<amount>|<output-id>,<amount>|<progress>|<crank-remaining>
```

Only the C2 recipe input/output are legal; timers are bounded by the frozen
duration/cap. Placement creates the entity, breaking removes it and spills both
non-empty slots, and unload/reload plus save/reopen preserve exact progress and
power without offline catch-up.

Chunk format v2 already persists arbitrary validated block-entity type/payload
pairs, so world save remains v12 and needs no migration. terrain v4 and settings v8
remain unchanged. Furnace payload remains byte-for-byte `v1` compatible.

`ResourceEconomyContract` advances from schema v1 to v2 only as a code-owned
verification input, adding explicit machine-process transformations. The
Crusher itself becomes a required reachable material. Cobblestone costs 40
acquisition ticks and the conversion consumes 40 manual-power ticks to produce
one Sand, whose direct terrain path costs 20 ticks; the new one-way edge is
therefore neither a progression bypass nor a positive-return loop. The complete
crafting/smelting/machine graph must remain acyclic, reachable and conservative.

## 5. Capability, UI and simulation boundary

C1 is extended only with real `Crusher` adapters:

| Block | InventoryProvider | MachineProcessor |
| ----- | ----------------- | ---------------- |
| Chest | Chest | none |
| Furnace | Furnace | Furnace |
| Crusher | Crusher | Crusher |

The processor view adds generic status, recipe id, power remaining/total and a
`manualPowerSupported` flag. The command is valid only for an open, revalidated
Crusher handle; Furnace and stale/mismatched handles reject it. The UI chooses
labels and slot roles from the capability kind, shows status/progress/power and
offers the crank command without including either concrete container.

`WorldSimulation` keeps the existing eight-phase order and 20 Hz ownership.
Furnace and Crusher both tick only during `BlockEntitySimulation`; C2 adds no
phase, scheduler, background work or offline simulation.

## 6. Automated evidence

`HELLOMINE3D_WORLD_SMOKE_FOCUS=C2-MACHINE` must cover at least:

1. all five Runtime statuses, precedence, one-tick mutation and atomic
   completion through pure deterministic cases;
2. Furnace using the common Runtime while retaining fuel, lighting, timing,
   event and payload-v1 behavior;
3. normal Crusher craft, placement, first Use/open/crank, capability discovery,
   slot admission and output-only insertion rejection;
4. bounded repeated cranks, exact 40-tick completion, no-power pause,
   missing-input and blocked-output pause without power loss;
5. transfer conservation, break spill, malformed payload and stale/mismatched
   capability failures;
6. unload/reload and save/reopen exact v1 Crusher state under save v12;
7. recipe 25, append-only IDs, terrain-atlas/material/text coverage and economy
   schema v2 reachability/source-sink/no-cycle proof.

`tools/validate_machine_runtime.ps1` freezes the two concrete processors, the
single Crusher recipe, shared Runtime call sites, payload/save boundary, focused
test path and absence of Registry, MechanicalPort, Network, transport or C3+
vocabulary. It is added to `scripts/verify_build.ps1` while all prior gates
remain mandatory.

## 7. Explicit non-goals and exit

C2 does not implement C3-C11, Track D, B7-B9, `MechanicalPort`, mechanical
power generation/transmission/topology, belts/pipes, automatic insertion,
storage, automatic crafting, shared network core, a generic machine/capability
Registry, future-machine inheritance, new terrain resources, new objectives or
new victory requirements.

C2 becomes `Done` only when the playable Crusher, shared two-processor Runtime,
focused/negative/economy/static evidence and synchronized documents pass the
complete VS2017/v141 Debug and Release
`scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow` gate and
one independent local commit is created. Real-window and `AI-01..AI-08` remain
`NOT_RUN`; fun, aesthetics, comfort, retention and physical-input feel remain
`NOT_CLAIMED`.

The implementation met the engineering portion of this exit on 2026-09-04:
the C2 focus passes `51/51` in Debug and Release; the complete gate passes
WorldRuntime `963/963`, Recipe `126/126`, Resource Pack `80/80` and startup
negatives `15/15` in its applicable configurations. The clean 105-entry archive
SHA-256 is `B4D73704A93B4377EB26336592448B4E31439387BA6198B49C59704517775739`.
The independent local commit is the final workflow action for this batch; its
identity is intentionally not self-referentially embedded in this contract.
