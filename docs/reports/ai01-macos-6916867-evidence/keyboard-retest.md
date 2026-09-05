# Independent keyboard candidate retest
Result: **PASS for Cmd+A whole-field replacement and ordinary input after release (2 rounds)**. Ctrl+A still inserts `a`; this observation is not represented as fixed. This bounded result does not make overall AI-01 PASS.

## Identity / isolation
Initial prompt: parent /root message in this task /root/macos_ai01_acceptance beginning “原包报告结束后请继续一个有界独立复测：新 /private/tmp/hm3d-independent-ai01-6916867/keyboard-candidate.zip”. It requests two normal-GUI rounds of Cmd+A replacement / released-modifier ordinary input, observation of Ctrl+A, then normal quit; no source or test reading.
Parent-supplied source identity: f0e63d2 + tracked_diff bc61ec2065cff047a5bc2dca8a9e253138bddb334cb782d9af0a79841f8c385d.
Measured ZIP SHA256: 201f4c7ba6c5eb2d67b3f6c3f370727b0c9618221f554d0d2e4c0105a791f3fc.
Measured actual Mach-O SHA256: 87a032e44205dbc6ac58e49f4b85c9926901e401c878907c2b491eddfc2152d1.
Extracted separately: /private/tmp/hm3d-independent-ai01-6916867/keyboard-retest/HelloMine3D-Keyboard-Candidate-20260905.app. Only executable permission restored on launcher and Mach-O; no source/package code edited; original app preserved.
Shell working_directory: /Users/lizi/Desktop/Workspace/HelloMine3D (tool default, initially verified by pwd). Commands use absolute release/evidence paths. App working directory per packaged launcher: candidate Contents/Resources/bin.
OS: macOS 15.7.3 (24G419), arm64; normal 1280x720 client, English locale.
accessible_roots: filesystem configuration permits read :root; repository remains technically readable. Writable roots supplied are /Users/lizi/Desktop/Workspace/HelloMine3D, /private/tmp, /private/var/folders/8n/qqg7p5691wq8gmblxz23zrqc0000gn/T, /Users/lizi/.codex/visualizations/2026/09/05/01a06f1c-1365-7881-abbf-75558c89757b. Effective OS-level access is not exhaustively probed; cannot certify exact least-privilege boundary. repository_accessible=true; context_isolation=PARTIAL. Independent task conducted GUI observations without reading repository source/tests; this is not technically blind AI-06.
No world created; default form seed 860786669 is only form metadata, world_identity=N/A.

## Normal GUI sequence, ~04:35–04:41 UTC 2026-09-05
1. cua.getApp exact candidate path launched in 9.1244 seconds. Screenshot showed normal English main menu. Click Single Player; Worlds has 0 active worlds, New World field.
2. Round 1: click Name field; pressKey(super+a). Screenshot visibly highlights full New World string, no stray a. typeText(RoundOne); ordinary pressKey(a), pressKey(x). Screenshot field exactly RoundOneax. PASS.
3. Round 2: pressKey(super+a); typeText(RoundTwo); ordinary a, x. Screenshot field exactly RoundTwoax. PASS; first string replaced completely and ordinary inputs append correctly after modifier release.
4. Ctrl observation 1: ctrl+a, typeText(C1), ordinary x. Screenshot shows RoundTwoaxaC1x (additional a from Ctrl+A); no full replacement or line-start movement evident.
5. Ctrl observation 2: super+a -> typeText(ControlTwo), then ctrl+a alone. Screenshot explicitly shows ControlTwoa. Ordinary x, y produce ControlTwoaxy. Ctrl+A inserts a under this CUA/app route; subsequent ordinary inputs work. Not counted as Cmd+A failure, not claimed fixed.
6. Back to Main Menu -> Quit. CUA returned App quit after requested normal exit. No unexpected exit during this retest.

## Evidence
True screenshots exist inline in this task's CUA getScreenshot tool records: selected New World, RoundOneax, RoundTwoax, RoundTwoaxaC1x, ControlTwoa, ControlTwoaxy, final main menu. Documented screenshot API has no filesystem-write helper, so no PNG paths fabricated. Original report.md and corroboration files remain separate and unchanged by candidate behavior. No build/test pass fact used as substitute for GUI results.
