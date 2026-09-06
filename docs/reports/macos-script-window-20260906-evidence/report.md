# Script-based macOS window evidence — 2026-09-06

User explicitly requested program/script launching and screenshots. Added
`tools/macos_window_evidence.py` with a read-only Objective-C window lookup.
Normal launch uses LaunchServices (`open`); screenshot uses `screencapture`
restricted to a visible window belonging to the exact package path and bundle ID.
Existing app processes are reused. No gameplay inputs are injected.

Runtime revision: 0e0c569a4097dea4c73234bbabb1c2b656f8078c.
Tooling developed on repository HEAD 46c1c24; runtime binary is unchanged.
Commands, platform, configuration, package identity, exact timestamps and hashes
are in each JSON. This is tooling verification, not independent gameplay PASS.

- Initial Swift prototype: FAIL, SDK/compiler mismatch; retained in worlds.json.
- Final Clang helper compiled successfully. Sandboxed desktop lookup/launch:
  FAIL, LaunchServices -10827; retained in worlds-clang.json.
- Same final command with desktop execution access: CAPTURED, 05:30:00.742–
  05:30:00.983 UTC (about 0.24 seconds), PID62510/window26168. Existing
  app was reused; no open command was needed in this successful run.
- Parent visually inspected worlds-desktop.png: genuine Worlds creation form,
  difficulty dropdown open with Normal selected; 2560×1496 pixels. No blank
  frame, desktop fallback or fabricated scene. JSON visual_review remains the
  script's original PENDING; this paragraph records subsequent human-style
  visual review by the implementing assistant.

Launch-from-stopped was not repeated in this run; prior desktop `open` evidence
is in ../ai07-macos-20260906-evidence/script-launch.json. Existing-window reuse
and capture are verified here. No runtime rebuild was required for tooling.
The independent world-creation approval rejection remains unresolved and was
not bypassed. Full six-combination UI acceptance remains NOT_RUN.

Usage from a desktop terminal (choose a new output name for each capture):

```sh
python3 tools/macos_window_evidence.py --app /path/to/HelloMine3D.app \
  --configuration Release --launch --output /tmp/hm3d-evidence/frame-001.png
```

Requires Python3, Xcode command-line Clang and macOS desktop/screen recording
access. The small native helper compiles once into ignored build cache.
Missing/ambiguous windows fail with evidence; no whole-desktop fallback occurs.
