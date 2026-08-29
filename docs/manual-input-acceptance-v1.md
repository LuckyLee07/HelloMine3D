# R3 Physical Input Acceptance Protocol v1

This protocol records the original twelve-case validation gap between
deterministic controller tests and the real Windows OIS keyboard/mouse path. It
is deliberately physical: no
tool may synthesize desktop input, move the pointer, or mark a case on behalf
of the operator.

## Current status and scope limit

Version 1 remains a valid historical input baseline and its validator continues
to check the exact record schema. It is no longer sufficient by itself to close
the current D4, D6 or project-wide R3 `Verify` state:

- `case.attack_mob` observes attacking a mob, but does not require the player to
  take damage, die and respawn;
- the twelve cases do not cover crop acquisition/planting, an explicit save,
  relaunch and restored-state inspection required by D6;
- they predate Stage 9 guard/ranged/difficulty paths and do not cover visual,
  bilingual-readability or listening acceptance.

Physical Input v2 is now defined by `docs/physical-input-acceptance-v2.md` and
adds the missing player-input journeys required to close D2/D4/D6/R3. Its real
run remains Deferred. Visual, bilingual-readability and listening evidence must
use a separate product-experience checklist; it must not be appended to this
physical-input schema. A v1 `PASS` therefore means only "all twelve v1 cases
passed on the recorded identity".

## Preconditions

1. Build the exact commit in `Release` and run `bin\HelloMine3D.exe` from the
   repository `bin` directory.
2. Use an isolated save directory and record the GPU/driver, window mode and
   client size. Because a new world has no obtainable chest before G1/G2, a
   preparation-only three-frame launch may use
   `HELLOMINE3D_CONTAINER_FIXTURE=1` to persist one ordinary chest. Stop that
   process, then run the complete physical sequence with every fixture,
   render-capture and performance-capture variable unset; the actual input
   run must never use a mode that disables or freezes normal input paths.
3. Copy `docs\manual-input-record-v1.template.txt` to an evidence file outside
   Git while testing. Replace every `NOT_RUN` value with `PASS`, `FAIL` or
   `BLOCKED`, and describe every non-pass result under `deviations`.
4. Validate the final record with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_manual_input_record.ps1 -RecordPath <record.txt> -RequirePass
```

## Physical sequence

Run the cases in order so focus and UI capture failures cannot be hidden by a
later restart.

| Record key | Physical action | Pass condition |
| ---------- | --------------- | -------------- |
| `case.focus_recovery` | Move with `W`, Alt-Tab away while holding no key, return, then press/release `W` again. | No movement continues while unfocused; focus returns without a stuck key or click. |
| `case.wasd_movement` | Walk forward/back/left/right with `W/S/A/D`, then hold Ctrl while moving. | Direction matches each key and Ctrl visibly increases speed. |
| `case.mouse_look` | Move the physical mouse horizontally and vertically, press `L`, move again, then press `L` once more. | View follows both axes, stops while mouse-look is disabled, and resumes without a jump. |
| `case.flight_sneak` | Press `F`, use Space/Shift to rise/descend, press `F` to land; hold and release Shift while grounded. | Flight toggles once per press, vertical controls work only as designed, and sneak is active only while grounded Shift is held. |
| `case.hotbar_numbers` | Press number keys `1` through `5`. | The highlighted hotbar slot follows every key exactly. |
| `case.hotbar_wheel` | Roll the physical wheel one detent in each direction. | Selection advances one slot with wrapping and reverses in the other direction. |
| `case.break_block` | Aim at the prepared chest and hold left mouse until it breaks. | The outlined chest is removed, its configured drop enters inventory and its contents spill as ordinary item entities. |
| `case.attack_mob` | Aim at a naturally spawned Mob and left-click at a clear line of sight until it dies. | The nearer actor is selected, health/death behavior is rate-limited, and loot appears. |
| `case.place_block` | Select the recovered chest and right-click an adjacent face that has room. | Exactly one adjacent chest is placed and one held item is consumed. |
| `case.container_use_transfer` | Place a chest, right-click it, then click one hotbar stack and one chest stack. | The container opens through normal use; both transfers conserve totals and world movement/look do not leak through the UI. |
| `case.container_close` | Close once with the button, reopen, then close with Escape. | Both paths return to gameplay, restore mouse-look and require no extra click. |
| `case.window_close` | Press Escape with no container open, then repeat once using the window close button after relaunch. | Each path closes cleanly without a hang or crash dialog. |

## Automated preflight

Before arranging the physical sequence, run the deterministic logical and
background-window preflight against the exact configuration that will be used
for R3:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_r3_automated_preflight.ps1 -Configuration Release -Build
```

The preflight maps existing controller, interaction, container, combat and D6
checks to the twelve R3 cases, adds focused checks for neutral input release,
frame-local mouse look, hotbar wrapping, flight vertical controls and container
capture release, and verifies that a hidden client never owns the foreground or
creates a visible top-level window. Its evidence is written under `tmp/` by
default, records the tested commit plus clean/dirty worktree state, and records
each mapped case as `AUTOMATED_LOGIC_PASS`, plus
`physical_input_result=NOT_RUN` and
`r3_closure=NOT_ELIGIBLE` even when every automated check passes.

This preflight detects deterministic regressions before operator time is spent;
it does not exercise a physical device, foreground focus recovery, Escape/UI
event routing or the native window close button, so it cannot close R3, D2, D4
or D6.

## Record rules

- `protocol_version` must be `1`.
- `commit` is the tested Git commit, not merely the current branch name.
- `configuration` is `Debug` or `Release`; records intended as a baseline for
  the later D6 journey must use `Release`, but v1 alone cannot close D6.
- `overall_result=PASS` is valid only when every case is `PASS` and
  `deviations=none`.
- `overall_result=PASS` proves only the v1 scope described above; it does not
  automatically close the current D4, D6 or project-wide R3 state.
- A `FAIL` or `BLOCKED` result is still useful evidence, but it does not close
  R3, D2, D4 or D6.
- Repeat the whole protocol after any OIS, window-system, container UI,
  combat, hotbar or player-facing input change.
