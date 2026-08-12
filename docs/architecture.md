# HelloMine3D Architecture Notes

This cleanup follows the same repository-level direction as `HelloOgre3D`: keep build entry points
and documents at the root, place product code under `src/<ProjectName>/`, isolate vendored code
under `src/external/`, and keep runtime assets out of the source tree.

## Directory Map

| HelloOgre3D pattern | HelloMine3D path | Purpose |
| ------------------- | ---------------- | ------- |
| `src/HelloOgre3D/` | `src/HelloMine3D/` | First-party game and rendering code. |
| `src/external/` | `src/external/` | Vendored ImGui, GLM and OIS source used directly by the build. |
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
World metadata version 2 persists player state plus live mob/item subtype state;
version 1 saves remain readable with an empty actor list and upgrade on the
next world save.

`Player/`, `Item/`, and `Physics/` are gameplay support modules.
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

`Diagnostics/` contains renderer-independent performance/debug options. The
ImGui platform and render integration stays in `Ogre/OgreUserInterface.*`.

`Entity/` contains the base entity data shape used by player, camera, matrix helpers, and future
world actors.

`Maths/` and `Util/` are shared support modules. `Util/ResourcePaths.h` centralizes runtime file
lookup so code no longer reaches into hard-coded `Res/` or `Shaders/` directories.

## Build Boundary

Premake is the canonical build entry point. It uses the same source and asset boundaries to
generate IDE projects and Makefiles under `build/`, then emits the executable to `bin/`, matching
the runtime layout. New source files should be introduced through the shared `src/HelloMine3D`
layout first.
