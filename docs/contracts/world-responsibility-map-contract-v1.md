# AL-A1 World Responsibility Map Contract v1

> Status: Frozen after AL-A1 verification
>
> Runtime behavior: unchanged
>
> Authoritative task status: `docs/current/todolist.md`

## 1. Purpose and scope

This contract freezes how the existing `World` public surface is described and
how a future public entry can be admitted. AL-A1 is a responsibility-mapping
batch, not a behavior refactor:

- classify every existing public method in `World.h`;
- establish a machine-checked public-surface identity;
- require an explicit responsibility review for every future public change;
- keep all existing callers and compatibility escape hatches working;
- do not add a wrapper, move implementation, or begin AL-A2.

## 2. Orthogonal classifications

Every public method has exactly one API concept and one primary responsibility.
The authoritative row set is the marked table in
`docs/current/architecture.md`.

API concepts:

| Concept | Contract |
| ------- | -------- |
| `Query` | Observes a value and does not commit a new Gameplay result. An existing lazy residency/cache side effect must be documented rather than hidden. |
| `Command` | May commit state while preserving the existing rejection, atomicity, event and persistence semantics. |
| `Runtime Tick` | Advances time or bounded runtime work. A subsystem may not add another parallel tick entry merely for convenience. |

Primary responsibilities are limited to `World Query`, `World Mutation`,
`Simulation`, `Streaming`, `Persistence`, `Actor`, `Combat`, `Progression` and
`Diagnostics`. A method with several implementation effects is classified by
the capability its caller requests; secondary effects belong in the boundary
note.

## 3. Frozen v1 identity

- unique public method names: `78`;
- `Query`: `45`;
- `Command`: `31`;
- `Runtime Tick`: `2`;
- normalized `World.h` public-surface SHA-256:
  `3C53F56C425F0395354C8A5CE966E96CDA8BC93D836699955E03BA965A664AD8`.

Overloads share one map row. The hash additionally protects overloads,
signatures, public constants and nested public declarations that are not
represented by the unique-name count.

## 4. Compatibility escape hatches

`getChunkManager`, `getActorManager`, `getEventBus` and `getPlayer` remain
available because existing code depends on them. They are legacy non-owning or
mutable escape hatches, not templates for new APIs. AL-A1 does not migrate their
callers and makes no claim that the current facade is already decomposed.

## 5. Admission rule for a future World API change

A public-surface change must complete all of the following in the same batch:

1. identify the real playable or operational need and why an existing boundary
   cannot express it;
2. assign one API concept and one primary responsibility;
3. document mutation, rejection, threading, event and persistence effects in
   the responsibility-map row;
4. update the frozen public-surface hash after reviewing the complete diff;
5. pass `tools/validate_world_responsibility_map.ps1` and all validation routed
   by the behavioral change.

Updating the table and hash is evidence of review, not automatic permission to
grow `World`. A new mutable manager getter, renderer-facing type, unbounded tick
entry or speculative future-system slot fails this contract unless a separately
approved architecture batch explicitly changes the boundary.

## 6. Failure semantics and validation

`tools/validate_world_responsibility_map.ps1` fails when:

- the public-surface hash changes;
- a public method lacks a map row;
- a row is stale or duplicated;
- a concept or responsibility is outside the frozen vocabulary;
- the machine-readable markers are missing or malformed.

The check runs directly and from the complete Windows gate:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  tools\validate_world_responsibility_map.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File `
  scripts\verify_build.ps1 -VisualStudioVersion 2017
```

## 7. Evidence and next-batch boundary

AL-A1 changes architecture documentation and the verification graph only. It
does not change Gameplay, save data, assets, runtime threading, performance
identity or the Release package's observable game behavior. Existing formal
Q1/Q3 evidence therefore remains applicable; human subjective claims remain
`NOT_CLAIMED`, and `AI-01..AI-08` remain `NOT_RUN` unless a separate conforming
Computer Use record is produced.

AL-A2 may only be started after separate owner approval. Its previewed scope is
limited to moving existing Chunk Update, Mesh Work and loader coordination;
new residency/mesh/render state machines remain a Track B concern.
