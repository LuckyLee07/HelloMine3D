# HelloMine3D

HelloMine3D is a single-player voxel sandbox written in C++17. You can create a
world, mine and place blocks, craft equipment, explore, fight enemies, and follow
an objective path to a victory event. The game also serves as an
**Architecture Lab**: its world, persistence, and rendering systems are developed
and checked against documented behavior.

The client uses Ogre 1.10 / GL3Plus for rendering. This is an active development
project; see the [current task list](docs/current/todolist.md) for the latest
scope and acceptance status.

![Voxel coast at noon](docs/screenshots/validation-v10c-coast-noon.png)

## Features

- Procedural biomes, caves, ores, vegetation, and explorable structures in a streamed voxel world.
- Mining, building, crafting, tool progression, food, furnace smelting, and a hand-cranked Crusher.
- Enemies, combat, objectives, a waystone victory loop, and post-victory trials.
- World management with versioned saves, backups, restore, and recoverable deletion.
- Remappable controls, English and Chinese interfaces, audio, ambient occlusion,
  block light, fog, and clouds, plus optional shadows and post-processing.

## Build and run

Build from the repository root. The project builds Ogre and other C++
dependencies from vendored source; you do not need separate dependency binaries.

### macOS and Linux (Make)

Install a C++17 toolchain, GNU Make, and Premake 5. macOS needs Xcode command-line
tools; Linux also needs the OpenGL/X11 development packages for your distribution.

```sh
bash scripts/build.sh release
bash scripts/run.sh
```

The executable is written to `bin/HelloMine3D`. For an Xcode project on macOS,
run `./xcode.sh` and open `build/HelloMine3D.xcworkspace`. The native build and
test route is `bash scripts/verify_xcode.sh`.

### Windows (Visual Studio)

The maintained Windows toolchain is Visual Studio 2017 with the v141 C++ tools.
Generate the solution with the bundled Premake:

```powershell
.\tools\premake\premake5.exe --os=windows --file=premake/premake.lua vs2017
```

Open `build\HelloMine3D.sln`, build the `HelloMine3D` target for x64 Debug or
Release, then run `bin\HelloMine3D.exe`.

On first launch the game creates a local `bin/config.txt`; graphics, audio,
language, and controls can be changed in the in-game settings. Default movement
uses WASD, Space to jump, Ctrl to sprint, Shift to sneak, E to open crafting, and
R to consume held food. Mouse buttons handle mining/attack and context-sensitive
use, placement, or guard. Tab opens the HUD cursor and pauses the single-player
simulation. Hover a hotbar slot for item details or click to select it. Click the
quest card to open the journey journal, or the minimap to inspect an explored
surface overview and switch to the regional 3D relief map. Tab, Escape or the
page close button resumes the game. L and the grave/backtick key remain
alternatives when not rebound to another gameplay action.

## Development and verification

The [validation matrix](docs/current/validation-matrix.md) selects checks by
change type. These are the full build-and-test entry points:

| Platform | Command |
| --- | --- |
| macOS / Xcode | `bash scripts/verify_xcode.sh` |
| macOS / Make | `MAKEFLAGS='-j2 -B' bash scripts/verify_build.sh` |
| Linux | `bash scripts/verify_build.sh` |
| Windows / VS2017 | `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_build.ps1 -VisualStudioVersion 2017` |

Asset and data changes can be checked with `bash scripts/check_assets.sh`.
Automated tests cover code and resource behavior; real-window and normal-input
acceptance are recorded separately in the
[current task list](docs/current/todolist.md).

The main source lives in [`src/HelloMine3D/`](src/HelloMine3D/), runtime assets
in [`media/`](media/), build rules in [`premake/`](premake/), and build/test
helpers in [`scripts/`](scripts/). The rendering client reads world snapshots
and sends commands to the game; the world owns gameplay state and persistence.
See the [architecture guide](docs/current/architecture.md) and
[Architecture Lab tutorial](docs/current/architecture-lab-tutorial.md) for the
design and its tradeoffs.

For contributors and coding agents, start with [AGENTS.md](AGENTS.md). The
[documentation index](docs/README.md) points to current decisions, feature
contracts, and validation reports.
