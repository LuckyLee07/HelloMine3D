# Performance Baseline

This document defines the repeatable baseline used to catch refresh, loading,
and rendering regressions in HelloMine3D.

## Goal

The render screenshot smoke verifies that the frame is visually valid. It is
not a performance benchmark because runtime readback with `glReadPixels` can
distort frame timing. Performance is measured separately by
`tools/run_perf_baseline.ps1`.

The baseline is intentionally simple:

- fixed seed, player position, and player rotation when available;
- isolated save directory per run;
- non-activating window display on Windows;
- no player input or mouse warping while capture is enabled;
- per-frame CSV plus a small summary file.

## Command

Default steady-scene run:

```powershell
powershell -ExecutionPolicy Bypass -File tools\run_perf_baseline.ps1 -StopExisting -WarmupMs 3000 -DurationMs 10000
```

The script writes output under:

```text
bin/perf_baseline_<runId>/
```

Important outputs:

| File | Purpose |
| ---- | ------- |
| `frames.csv` | Per-frame raw data for graphing or later comparison. |
| `summary.txt` | Average, percentile, spike count, and final world/chunk counters. |
| `process.stdout.log` | Runtime stdout, including performance capture enable/summary logs. |
| `process.stderr.log` | Runtime stderr. |

## Captured Metrics

`frames.csv` records these timing columns:

| Column | Meaning |
| ------ | ------- |
| `dt_ms` | SFML frame delta used by game simulation. |
| `event_ms` | Event polling and dispatch time. |
| `update_ms` | Application update time, including sandbox update and chunk update work. |
| `render_ms` | World/render submission time before ImGui and display. |
| `debug_gui_ms` | ImGui begin/render cost. |
| `render_capture_ms` | Runtime screenshot capture cost. Should be near zero in this benchmark. |
| `display_ms` | `window.display()` cost, including vsync wait. |
| `frame_ms` | Measured wall-clock frame time for the main-loop body. |

It also records world counters:

| Column | Meaning |
| ------ | ------- |
| `existing_chunks`, `loaded_chunks` | Chunk residency. |
| `sections` | Total loaded chunk sections. |
| `mesh_dirty_sections` | Sections waiting for mesh rebuild. |
| `cpu_ready_sections` | Sections with CPU mesh ready. |
| `gpu_buffered_sections` | Sections uploaded to GPU. |
| `queued_chunk_updates` | Pending section update queue size. |
| `mesh_rebuilds` | Cumulative mesh rebuild count. |
| `actor_count` | Runtime actor count. |
| `terrain_seed` | Seed used by the run. |

## Reading The Summary

Use these values first:

- `frame_p95_ms`: stable refresh cost; good first regression indicator.
- `frame_p99_ms`: spike behavior.
- `frames_over_33ms`: frames slower than roughly 30 FPS.
- `frames_over_50ms`: visible hitch candidates.
- `update_p95_ms`: chunk/gameplay update pressure.
- `render_p95_ms`: render submission pressure.
- `display_p95_ms`: vsync/driver/display wait; high values here may not mean CPU render logic got slower.
- `last_mesh_rebuilds`, `last_loaded_chunks`, `last_gpu_buffered_sections`: sanity checks that two runs are comparing similar world state.

Because the benchmark window is displayed without foreground activation,
Windows may schedule it differently from the active desktop app. In that mode,
`sampled_fps` and the `dt_ms` percentiles can show background scheduling gaps.
Use the stage timings (`frame_*`, `update_*`, `render_*`, `display_*`) as the
primary non-intrusive comparison points. If foreground, player-visible FPS is
needed, run a separate interactive benchmark with explicit permission to focus
the game window.

## Current Verified Run

Latest verified local run:

```text
bin/perf_baseline_20260807190255313-41064
```

Command:

```powershell
powershell -ExecutionPolicy Bypass -File tools\run_perf_baseline.ps1 -StopExisting -WarmupMs 3000 -DurationMs 10000
```

Build configuration: **VS2022 x64 Debug**. Always compare runs from the same
configuration; a Release build changes the update and mesh-build timings
substantially.

Key summary:

| Metric | Value |
| ------ | ----- |
| `frames` | 591 |
| `sampled_fps` | 59.100 |
| `avg_fps` | 67.152 |
| `frame_p95_ms` | 16.430 |
| `frame_p99_ms` | 16.522 |
| `frame_max_ms` | 113.800 |
| `update_p95_ms` | 0.156 |
| `render_p95_ms` | 1.238 |
| `display_p95_ms` | 15.858 |
| `frames_over_33ms` | 2 |
| `frames_over_50ms` | 2 |
| `last_loaded_chunks` | 289 |
| `last_sections` | 2013 |
| `last_mesh_dirty_sections` | 1564 |
| `last_gpu_buffered_sections` | 447 |
| `last_mesh_rebuilds` | 449 |
| `terrain_seed` | 296595 |

The run is vsync bound: `frame_p95_ms` sits at the 16.4 ms refresh interval and
`display_p95_ms` accounts for almost all of it, while `update_p95_ms` and
`render_p95_ms` stay well under 1.3 ms. The two frames over 50 ms are startup
spikes captured after warmup.

Two things to watch in later comparisons:

- `last_mesh_dirty_sections` is 1564 of 2013 sections. Most loaded sections
  never get meshed inside a 13 second run. Confirm this is the intended chunk
  loader budget rather than starvation before optimizing mesh building.
- This run replaces an older baseline (`bin/perf_baseline_20260707201831896-39828`,
  73 frames at 7.3 sampled fps) that was recorded before the SFML build tree was
  rebuilt. Do not compare the two directly.

## Baseline Policy

For now this benchmark is informational. Use it to create comparable run
folders before and after risky changes. Once a stable current baseline is
accepted, add threshold checks in a later compare script:

- warn when `frame_p95_ms` grows by more than 15%;
- warn when `frame_p99_ms` grows by more than 20%;
- warn when `frames_over_50ms` increases materially;
- warn when final chunk/section counts differ enough that two runs are not
  comparable.

## When To Run

Run this baseline after changes to:

- chunk loading, unloading, mesh dirty queues, or mesh building;
- player/world update flow;
- renderer submission, shader/texture state, or `window.display()` sequencing;
- save/load paths that can affect startup or chunk residency.

Pair this with `docs/render-regression-smoke.md`: performance baseline checks
timing; render smoke checks that the captured frame still looks correct.
