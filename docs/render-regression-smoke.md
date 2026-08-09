# Render Regression Smoke

This document records the July 2026 fix for the pure-blue runtime render
regression and the non-intrusive screenshot smoke path used to validate it.

## Problem

The game window could clear to the sky color and show no terrain. Older
reference screenshots under `docs/screenshots/` still showed terrain correctly,
so the issue was in the current runtime render path rather than the saved
reference images.

The root cause was the chunk submit condition in
`src/HelloMine3D/Renderer/RenderMaster.cpp`. `RenderMaster::drawChunk()` checked
`ChunkMesh::faces > 0`, but `ChunkSection::bufferMesh()` uploads mesh data to
OpenGL and then clears the client-side mesh data. After that upload,
`faces` can be zero even though the GPU model has valid indices, so the section
was skipped and only the clear/sky color remained visible.

## Fix

The current render path is stabilized by these changes:

| Area | File | Fix |
| ---- | ---- | --- |
| Chunk submit | `src/HelloMine3D/Renderer/RenderMaster.cpp` | Submit solid/water/flora meshes based on `ChunkMesh::getModel().getIndicesCount() > 0` instead of the cleared CPU face count. |
| Render order | `src/HelloMine3D/Renderer/RenderMaster.cpp` | Render skybox before chunk passes with depth writes disabled for the skybox. |
| Texture state | `src/HelloMine3D/Renderer/WaterRenderer.cpp`, `src/HelloMine3D/Renderer/FloraRenderer.cpp` | Bind the block texture atlas in water/flora passes so previous GL texture state cannot leak into those passes. |
| GPU buffer cleanup | `src/HelloMine3D/Renderer/Model.cpp` | Track EBO handles in `m_buffers` so model cleanup deletes them. |
| Shader time | `src/HelloMine3D/Main.cpp` | Advance `g_timeElapsed` each frame so animated render passes receive time. |

## Screenshot Smoke

Use the runtime readback capture mode by default. It reads the rendered back
buffer from inside the process with `glReadPixels`; it does not use desktop
`CopyFromScreen`, so occluded windows do not capture the wrong application.

Command:

```powershell
powershell -ExecutionPolicy Bypass -File tools\run_render_capture.ps1 -StopExisting -CaptureMs 4000,6000 -Seconds 8 -PlayerRotation "20 118.4 0"
```

Expected behavior:

1. The script starts `bin\HelloMine3D.exe`.
2. The window is shown with `SW_SHOWNOACTIVATE` / `SWP_NOACTIVATE`.
3. Runtime capture mode disables player input and block interaction so the run
   does not warp the mouse.
4. The process writes `new_04000ms.png` and `new_06000ms.png` under
   `bin\render_capture_<runId>\`.
5. The process exits automatically after the requested captures.

Pass condition:

- The script prints `[RENDER_CAPTURE] status=PASS`.
- The output PNG files are non-empty.
- The images show terrain blocks and textures, not a solid blue or black frame.

## Current Verified Run

Last verified command output:

```text
[RENDER_CAPTURE] runId=20260807190230074-46036
[RENDER_CAPTURE] capturesMs=4000,6000 seconds=12 prefix=new mode=RuntimeReadback noActivate=true
[RENDER_CAPTURE] seed=296595
[RENDER_CAPTURE] playerPosition=2766 102 2905
[RENDER_CAPTURE] playerRotation=20 118.4 0
[RENDER_CAPTURE] captured E:\Workspace\MineCraft3D\bin\render_capture_20260807190230074-46036\new_04000ms.png
[RENDER_CAPTURE] captured E:\Workspace\MineCraft3D\bin\render_capture_20260807190230074-46036\new_06000ms.png
[RENDER_CAPTURE] status=PASS outputDir=E:\Workspace\MineCraft3D\bin\render_capture_20260807190230074-46036
```

Verified images:

- `bin/render_capture_20260807190230074-46036/new_04000ms.png`
- `bin/render_capture_20260807190230074-46036/new_06000ms.png`

Both images show visible terrain with grass, dirt, stone and sand textures,
water, flora (tall grass and roses), and sky. No pure-blue or black frame.

Earlier verified run, kept for comparison:
`bin/render_capture_20260707173412631-53208` (recorded while the repository
still lived at `E:\Workspace\MineCraft`).

## Implementation Notes

Runtime capture is implemented in
`src/HelloMine3D/Diagnostics/RuntimeRenderCapture.*` and enabled through these
environment variables, normally set by `tools/run_render_capture.ps1`:

| Variable | Meaning |
| -------- | ------- |
| `HELLO_RENDER_CAPTURE` | Enables runtime capture when set to a true value. |
| `HELLO_RENDER_CAPTURE_DIR` | Output directory for PNG captures. |
| `HELLO_RENDER_CAPTURE_PREFIX` | Filename prefix, defaulting to `capture`. |
| `HELLO_RENDER_CAPTURE_MS` | Comma or whitespace separated capture times in milliseconds. |
| `HELLO_RENDER_CAPTURE_MAX_DELTA_MS` | Ignores very large first-frame deltas before accumulating capture time. |
| `HELLO_RENDER_CAPTURE_EXIT` | Closes the window after all requested captures complete. |
| `HELLOMINE3D_SAVE_DIR` | Routes the run to an isolated save directory. |
| `HELLOMINE3D_SEED` | Optional deterministic terrain seed override. |
| `HELLOMINE3D_PLAYER_POSITION` | Optional deterministic player position override. |
| `HELLOMINE3D_PLAYER_ROTATION` | Optional deterministic player rotation override. |
| `HELLOMINE3D_SHOW_DEBUG_INFO` | Starts with the F1 debug panels visible. The script sets it with `-ShowDebugInfo`. |

`WindowScreenshot` mode is kept only as a fallback/manual diagnostic path. It
uses desktop screenshot APIs and can capture another foreground window if the
game client is occluded, so do not use it as the default regression check.

## When To Rerun

Rerun this smoke after changes to:

- `RenderMaster`, chunk/water/flora renderers, shaders, or texture binding.
- `ChunkMesh`, `ChunkSection`, `Model`, or GPU buffer upload/cleanup.
- world startup, seed/player restore, save directory routing, or camera setup.
- SFML window creation, GL context creation, or frame sequencing in `Main.cpp`.
