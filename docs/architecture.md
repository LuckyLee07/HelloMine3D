# MineCraft3D Architecture Notes

This cleanup follows the same repository-level direction as `HelloOgre3D`: keep build entry points
and documents at the root, place product code under `src/<ProjectName>/`, isolate vendored code
under `src/external/`, and keep runtime assets out of the source tree.

## Directory Map

| HelloOgre3D pattern | MineCraft3D path | Purpose |
| ------------------- | ---------------- | ------- |
| `src/HelloOgre3D/` | `src/MineCraft3D/` | First-party game and rendering code. |
| `src/external/` | `src/external/` | Vendored GLAD and ImGui-SFML bridge code. |
| `media/` | `media/` | Shaders, block definitions, textures, and fonts. |
| `bin/` | `bin/` | Runtime config, ImGui state, and executable output. |
| `docs/` | `docs/` | Architecture notes and screenshots used by documentation. |
| root scripts | `scripts/` | Build/run/debug commands. |

## Code Boundaries

`Main.cpp` and `Application.*` are the client shell. They own window creation, input dispatch,
frame timing, update/render sequencing, and application-level event handling.

`Core/` contains cross-cutting runtime primitives that are not purely rendering, gameplay, or
input. `Core/Camera.*` lives here because it is consumed by the renderer, world culling, matrix
helpers, and the application shell.

`World/` is the gameplay simulation boundary. It owns chunk lifetime, terrain generation, block
queries, block mutation events, and chunk mesh update scheduling.

`Player/`, `Item/`, `Input/`, and `Physics/` are gameplay support modules. `Input/` owns keyboard
helpers and the current vestigial `Controller` WIP class; player-specific movement behavior should
move to `Player/` when it becomes active.

`Renderer/`, `Shaders/`, `Texture/`, and `GL/` are the runtime rendering layer. `Renderer/` owns
render passes plus shared render data objects such as `Model.*`, `Mesh.h`, and `RenderInfo.h`.
Together these modules own OpenGL buffering, shader program setup, texture upload, and
skybox/water/flora/chunk rendering.

`Debug/` contains ImGui-backed diagnostic UI. It is intentionally separate from the main
application shell and from future player-facing UI.

`Entity/` contains the base entity data shape used by player, camera, matrix helpers, and future
world actors.

`Maths/` and `Util/` are shared support modules. `Util/ResourcePaths.h` centralizes runtime file
lookup so code no longer reaches into hard-coded `Res/` or `Shaders/` directories.

## Build Boundary

Premake is the canonical build entry point. It uses the same source and asset boundaries to
generate IDE projects and Makefiles under `build/`, then emits the executable to `bin/`, matching
the runtime layout. New source files should be introduced through the shared `src/MineCraft3D`
layout first.
