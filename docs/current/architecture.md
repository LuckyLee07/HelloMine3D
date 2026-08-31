# HelloMine3D Architecture Notes

This cleanup follows the same repository-level direction as `HelloOgre3D`: keep build entry points
and documents at the root, place product code under `src/<ProjectName>/`, isolate vendored code
under `src/external/`, and keep runtime assets out of the source tree.

## Directory Map

| HelloOgre3D pattern | HelloMine3D path | Purpose |
| ------------------- | ---------------- | ------- |
| `src/HelloOgre3D/` | `src/HelloMine3D/` | First-party game and rendering code. |
| `src/external/` | `src/external/` | Vendored ImGui, GLM, OIS and optional Tracy source used directly by the build. |
| `media/` | `media/` | Shaders, block and shape definitions, textures, and fonts. |
| `bin/` | `bin/` | Runtime config, ImGui state, and executable output. |
| `docs/` | `docs/` | Architecture notes and screenshots used by documentation. |
| root scripts | `scripts/` | Build/run/debug commands. |

## Code Boundaries

`Ogre/OgreMain.cpp` and `Ogre/OgreBootstrap.cpp` are the client shell. They own
Ogre startup, the GL3Plus window, OIS dispatch, frame timing and update/render
sequencing. `HelloMine3D.exe` is the only client target.

`Core/` contains cross-cutting runtime primitives that are not purely rendering, gameplay, or
input. `Core/Camera.*` lives here because it is consumed by the renderer, world culling, matrix
helpers, and the application shell.

`World/` is the gameplay simulation boundary. It owns chunk lifetime, terrain generation, block
queries, block mutation events, chunk mesh update scheduling, and versioned world metadata.
Cube geometry remains optimized by the greedy mesh path; non-cube geometry is
loaded from validated `media/shapes/*.shape` resources referenced by block
definitions, so adding another resource shape does not change the mesh builder.
Classic overworld generation runs explicit base-terrain, world-space cave,
ore, plant and tree passes in that order. Cave sampling uses global voxel
coordinates, so output is seed-deterministic and independent of chunk load
order while protected surface, water and bottom buffers remain intact.
Structure roots are likewise selected from the seed and world block
coordinates. A target chunk scans a six-block origin halo and projects only
its own structure fragments, so trees and cacti can cross chunk boundaries
without making generation depend on neighbouring chunk load order.
World metadata version 5 retains player and live mob/item subtype state plus
the version-3 immutable catalogue identity, timestamps and build identity. It
keeps the version-4 ten-bit playable-Alpha compatibility mask and adds a
versioned objective completion/progress state. The read-only catalogue still
discovers version 1-4 metadata without renaming its directory. Versions 1-3
migrate to an empty objective state; version 4 maps its Alpha bits to stable
objective ids when the world is opened through the management boundary.
`World/Storage/StorageTransaction.*` is the common synchronous publication
boundary for world metadata and binary chunks. It writes a same-directory
candidate, durably flushes it, validates it through the real format reader and
atomically replaces the published path only after success. One bounded failed
candidate is retained for diagnosis; loaders never treat pending or failed
siblings as authoritative. Chunk dirty state clears only after publication,
and failed saves prevent unload. Aggregate save count/total/max duration flows
through renderer-independent debug stats to ImGui and performance capture; the
full failure contract is documented in `docs/contracts/storage-transaction-contract-v1.md`.
`World/Storage/WorldBackup.*` builds bounded whole-world snapshots on top of
that boundary. A strict manifest freezes every world/chunk path, byte count and
content fingerprint; both backup candidates and restore candidates must pass
the real format readers before publication. Restore retains the previous
primary in one `recovery.failed` directory and rolls back already-published
files if an interruption occurs. The selected backup is never modified; its
layout and default limits are frozen in `docs/contracts/world-backup-contract-v1.md`.
Random block simulation extends `BlockBehavior`: sections index only block
states that currently opt in, and `World` rotates those active sections with a
four-section budget on the fixed 20Hz tick. Each visit samples three uniformly
distributed voxel positions, so sparse content is not guaranteed an immediate
callback. The first real behavior advances
immature tall grass to its mature metadata state; unload and storage reload
remove and rebuild the index without scanning unrelated blocks.

`Gameplay/` owns the renderer-independent N1 objective registry and runtime.
The strict current version-2 registry freezes before world construction and
explicitly normalizes persisted version-1 progress. The runtime
consumes existing domain events and observable player state, exposes a read-only
HUD snapshot and persists bounded completion/progress records; it does not own
items, blocks, actors or rewards. `AlphaJourney` remains only as the G6 ten-step
compatibility view. The full boundary is frozen in
`docs/contracts/objective-system-contract-v1.md`. `Player/`, `Item/`, and `Physics/` are
gameplay support modules.
`VictoryFlow` separately owns the persistent N7 world-outcome state and reward
epoch. `WaystoneEncounter` stores one strict block-entity wave payload, while
`World` reconciles bounded guardian actors against that payload across save,
chunk unload and backup restore. Objectives, the world list and Ogre victory
overlay consume read-only snapshots/events and cannot infer or mutate victory.
`PlayerInputState` is a platform-independent command value; the Ogre/OIS shell
collects devices and `PlayerController` applies those commands deterministically.
Mobs hold a non-owning target supplied by `World`, chase the player within a
bounded horizontal radius, and otherwise keep their deterministic wander path.
Damage immunity belongs to `LivingActor`, advances on the fixed simulation
tick, suppresses duplicate health/event changes, and resets when save state is
restored rather than becoming persistent world data.

`Ogre/` is the runtime rendering layer. `ChunkSectionRenderable` owns Ogre GPU
buffers for solid, water and flora meshes; `OgreActorRenderer` mirrors immutable
actor snapshots into simple mob/item scene nodes; `OgreBlockOutline`,
`OgreUserInterface` and `OgreRenderCapture` own selection feedback, HUD/debug UI
and screenshots. Materials and GLSL programs live under `media/ogre/`.
Before Ogre construction, `StartupResourcePreflight` strictly parses the
generated `media/resource-manifest.txt` and requires every listed resource to
exist and be non-empty. The generator derives the sorted inventory from source
registrations and resource references, so the runtime shell does not maintain a
second hard-coded asset list.

`Diagnostics/` contains renderer-independent performance/debug options.
`RuntimeProfiler.h` is the compile-time boundary for optional Tracy zones; it
expands to no-ops in ordinary builds, so world and simulation code do not depend
on Tracy APIs. `OperationPerformanceTiming` is the Q2 fixed-capacity timeline
for startup, catalogue, world-entry, save, backup and restore phases. It follows
the existing performance-capture switch and appends the newest complete record
per operation kind to the ordinary summary without introducing renderer types
into storage code. `CrashDiagnostics` owns the portable H1 directory/trigger
policy. `OgreMain` installs it before Ogre construction; only
`WindowsCrashDiagnostics.cpp` sees Windows exception structures or DbgHelp.
The selected backend pre-creates one dedicated writer thread and has no upload
path. Its contract and the explicit Breakpad audit decision are in
`docs/contracts/crash-diagnostics-contract-v1.md`. The ImGui platform and render integration stays in
`Ogre/OgreUserInterface.*`.

`Entity/` contains the base entity data shape used by player, camera, matrix helpers, and future
world actors.

`Maths/` and `Util/` are shared support modules. `Util/ResourcePaths.h` centralizes runtime file
lookup so code no longer reaches into hard-coded `Res/` or `Shaders/` directories.

## Build Boundary

Premake is the canonical build entry point. It uses the same source and asset boundaries to
generate IDE projects and Makefiles under `build/`, then emits the executable to `bin/`, matching
the runtime layout. New source files should be introduced through the shared `src/HelloMine3D`
layout first.
