# HelloMine3D Architecture Lab Tutorial

> Living tutorial status: Part 00 covers the completed AL-A0 baseline and the
> approved AL-A1 responsibility map. Later Parts are added only after their
> implementation batch is approved and verified; this file does not pre-create
> empty Sprint chapters.

## Part 00 — From a playable clone to an architecture lab

HelloMine3D already has a complete playable path: create a world, gather,
craft, upgrade tools, smelt, recover, fight, explore, reach the Waystone ending,
continue after victory, save and reopen. That game is the source of architecture
problems, not decoration around an engine exercise.

AL-A0 first froze the real module, ownership, thread, persistence, tick,
snapshot, performance and validation boundaries. AL-A1 then addresses the
first visible pressure point without pretending that a diagram has already
refactored the program.

### 0.1 Why does a God Object arise naturally?

#### Problem

The earliest useful `World` answers a simple question: where should code go when
it changes the world? As real play is added, the same answer remains locally
convenient for blocks, lighting, random ticks, natural mobs, combat,
projectiles, difficulty, Waystones, saving, background loading, mesh queues and
events. The result is not one obviously bad decision; it is many reasonable
features accumulating behind one trusted facade.

The current header exposes 78 unique public method names across nine primary
responsibilities. Callers also operate at different semantic levels: some ask
questions, some submit commands, while two entries advance runtime work.

#### Naive Solution

The tempting response is either to keep adding one `World` method per feature,
or to immediately split the file into services and move code until it looks
modular. The first choice hides coupling. The second changes ownership and call
order before the existing behavior has been described, making any regression
hard to attribute.

#### Failure

A large facade becomes dangerous when method names stop revealing semantics:

- a query may unexpectedly mutate authoritative state;
- a renderer-facing caller may obtain a mutable manager;
- a subsystem may invent a second tick path;
- saving, events or mesh invalidation may be lost during a mechanical move;
- a new abstraction may be designed for future systems that do not yet exist.

The failure is not file length by itself. It is the inability to review whether
a new entry preserves ownership, mutation and time-flow rules.

#### Design Evolution

AL-A1 takes a smaller step. Every existing public method receives two orthogonal
labels:

```text
API concept:     Query | Command | Runtime Tick
Responsibility:  World Query | World Mutation | Simulation | Streaming |
                 Persistence | Actor | Combat | Progression | Diagnostics
```

The complete map stays beside the current architecture, where it can be checked
against the real header. Existing callers do not migrate. Mutable manager/player
accessors remain visible as compatibility escape hatches and are explicitly
prohibited as patterns for new APIs.

This is an admission gate rather than a final class design. A future wrapper is
allowed only when a separately approved batch has a real caller and ownership
problem to solve.

#### Implementation

`docs/current/architecture.md` contains one row per unique method and a
normalized public-surface hash. The validator extracts the public section of
`src/HelloMine3D/World/World.h`, checks the hash and compares both method sets.
It then runs at the start of `scripts/verify_build.ps1`.

The v1 map contains 45 Queries, 31 Commands and 2 Runtime Ticks. Overloads share
a row, while the hash still detects signature, overload, constant and nested
declaration changes.

#### Validation

Run the focused gate while editing the boundary:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  tools\validate_world_responsibility_map.ps1
```

A PASS proves that every current public method is classified and that the
reviewed header identity has not drifted. The VS2017 full gate proves that this
new check composes with the established build, test and clean-package path. It
does not prove that the God Object has been decomposed, nor does it replace OS
Computer Use or human subjective evidence.

#### Trade-offs

The map adds maintenance whenever the public surface changes. That friction is
intentional: it makes growth visible while remaining much cheaper than a broad
refactor. A hash can be updated mechanically, so the contract also requires a
real-need explanation and boundary note; automation cannot judge architecture
quality on its own.

The design accepts temporary inconsistency: legacy escape hatches and a broad
facade remain. In return, AL-A2 can later move one proven Chunk runtime boundary
against a stable, reviewable baseline instead of guessing what `World` meant.

Related evidence:

- `docs/current/architecture.md`
- `docs/contracts/world-responsibility-map-contract-v1.md`
- `docs/reports/architecture-lab-baseline-v1.md`
- `tools/validate_world_responsibility_map.ps1`
