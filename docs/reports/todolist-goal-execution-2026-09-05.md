# TODOLIST Goal execution — 2026-09-05

Status: In progress. This report does not close the Goal or claim AI PASS.

## Frozen scope and authority

- Starting commit: `81d945dcf7a4bdbd6d98a1652b2776fca37665fa` (D1).
- Starting branch: `master`, aligned with the locally recorded `origin/master`.
- Starting worktree: no tracked changes; untracked user file
  `18-81d945d-20260905.bundle` was present. Later inventory could not find it;
  this task has not deleted it. Its current location is unverified.
- Scope source: `docs/current/todolist.md` at the starting commit; local exact
  copy: `build/goal-20260905/todolist.start.md`.
- No applicable `AGENTS.md` was found in the repository or its ancestor paths.
- The owner authorized remaining ledger work, evidence-based D2 investigation
  and conditional implementation, pending acceptance, and current-document fixes.
- B7-B9, C4-C11, D3-D8 and Extended remain outside this Goal. A newly listed
  next candidate does not expand this frozen scope.

## Work ledger

| Item | Status | Dependency / exit evidence |
| --- | --- | --- |
| Freeze starting state and discover validation environment | Done | Starting identity and later macOS-first scope steering recorded |
| Audit D2 entry against real distant Actor/Machine workload | Done (investigation only) | Debug + two Release probes; retain Candidate with reasons, no activation implementation |
| Fix current-document inconsistencies | Doing | README, ledger, architecture/tutorial and validation routes updated; final check after validation |
| Execute relevant automated checks | Doing | Path-identity assertion fixed; post-cursor gmake Debug/Release 13 suites PASS; Xcode gate in progress |
| AI-01..AI-05, AI-07, AI-08 | NOT_RUN | Hashed clean macOS Release package, OS input, normal gameplay; no fixtures |
| AI-06 | NOT_RUN | Independent fresh executor with package-only filesystem access and 30-minute record |
| New macOS gameplay/visual acceptance | Doing | Explicitly authorized by subsequent owner steering; independent macOS evidence |
| Windows-specific verification | Postponed by owner | Not a required exit item of this macOS Goal; do not claim Windows PASS |
| Final delivery | Todo | Review diff, document evidence and blockers; local commits may be created, no push/release/tag |

## Environment and evidence boundaries

Initial host: macOS Darwin 24.6.0, arm64. Xcode and CMake are installed;
`pwsh` was not found on PATH. Existing generated build trees and old binaries
are not evidence for the starting commit until regenerated/rebuilt.

Windows inspection found VS2022/v143 in the usual installation paths, no
VS2017/v141 installation there, and a running Windows VM. CLI execution is
edition-limited and UI input did not work reliably. The owner then postponed
Windows-specific verification. macOS gmake x86_64 checks count for the newly
approved macOS scope; they are not Windows or native arm64 PASS. Historical D1
Windows 991/991 results remain historical evidence, not a new execution.

## Execution journal

- Created Goal and read the starting ledger, validation matrix, AI acceptance
  contract, D1 implementation/report and relevant D2 roadmap constraints.
- Confirmed D1 executes three real workload plans in `WorldSimulation`.
- Found README still reports C3/980 and excludes Track D; validation matrix
  lacks a D1 row/route. These will be corrected without changing old reports.
- Scope steering: the owner explicitly authorized using the Windows VM while
  preserving their active PCD build. CLI start/exec both return an edition
  restriction; screenshots work, but input does not produce the expected
  action and selecting VM-specific app surfaces times out. No user build was
  stopped or changed.
- Subsequent owner instruction explicitly makes **macOS the current Goal
  validation platform**, with Windows-specific validation postponed. This
  supersedes the initial Windows-only exit dependency, not historical evidence
  or the AI-06 isolation/audio requirements. Proceed with current-source macOS
  Debug/Release automation, isolated package and corresponding window checks.
- macOS gmake/Apple clang Debug and Release WorldRuntime both pass 991/991;
  D1 focus passes 24/24 in both configurations. Target architecture is x86_64
  on an arm64 host (translation), not native arm64/TSan.
- The valid D2 probe passes 3/3 fixture checks in Debug and twice in Release.
  Gameplay fields match across the Release repeats; diagnostic timings vary.
  Entry decision is documented in
  `simulation-activation-entry-investigation-2026-09-05.md`: retain Candidate,
  no production activation/runtime change.
- Asset path checks pass 75 checks; portable performance-comparison contract
  validation passes 8 legacy and 580 contract cases. These do not represent
  new Q1 performance captures.
- Started full macOS Debug build of client and all test targets. Existing
  first-party unused-function/capture warnings were observed in baseline
  source; a successful compile is not a warning-clean Xcode gate claim.
- Initial 13-target Debug suite: 12 exited zero; Catalogue reported 58/59.
  Added diagnostics proved that create returns `/var/...` while enumeration
  canonicalizes the same directory to `/private/var/...`; rename and open both
  succeeded. The assertion now uses filesystem equivalence and additionally
  checks the opened world's id/path. No expected id/name or production
  behavior was relaxed. Targeted rerun passes 59/59.
- Removed first-party unused helpers/captures, guarded Windows-only cursor
  fields and corrected UI constructor initialization order. These changes
  address macOS warning checks without changing gameplay semantics.
- The first unconfigured client preflight exited 1 because seed 0 yielded no
  water in the inspected region. The documented Xcode seed/position
  (`20260809`, `3038 66 1922`) passes unchanged validation with five water
  sections. Both logs are retained; this is a fixture-invocation correction.
- Updated stale Xcode expected counts (World 560→991, Catalogue 30→59, crash
  diagnostics 12→21); positive/zero-failure requirements remain strict.
  Generated Xcode graph passes 31 projects and all nine validator self-tests.
- Started `MAKEFLAGS='-j2 -B' bash scripts/verify_build.sh` to rebuild both
  configurations and dependency objects, then run all 13 executables. The
  prior incremental graph contained dependency objects compiled with a newer
  deployment target; clean rebuilding removes that ambiguous package input.
- Added a manifest-based macOS app packager and a macOS startup-negative
  runner that reads the existing literal 10 missing/5 malformed fixtures.
  macOS checks require stderr-only error reports, not Windows MessageBoxW.
- First macOS startup-negative runner attempt incorrectly ordered its mini
  manifest; ten missing-file cases rejected the manifest itself. This runner
  defect was corrected by sorting the entries as required by the existing
  manifest contract. The original log remains; the repaired Debug run passes
  all 15 cases with the original expected diagnostics unchanged.
- The clean Debug build and all 13 executables now pass, including World
  991/991, Catalogue 59/59, Recipe 126/126, Resources 80/80, transaction 16/16,
  backup 19/19, operation timing 12/12 and crash diagnostics 21/21 (the latter
  tests the unsupported-platform boundary, not Windows dumps). Full clean
  Release build is in progress.
- Debug-only isolated app preflight at
  `/private/tmp/HelloMine3D-Debug-Preflight-20260905.app` opens the normal main
  menu with a changing sky. Its executable hashes to
  `2eb942a885ed7333786642a28771ea5d882df39380f2c50dd1224facbe2e522e`.
  CUA clicks, double-clicks, Raise and Tab did not transition Single Player.
  This is supplemental Debug observation, not AI acceptance of Release. User
  feedback confirms the native cursor is stuck on focusing the menu: this is
  a product input defect, not merely a CUA limitation.
- Source audit confirms macOS has no real sound/music output backend:
  AudioRuntime's platform factory returns null and MusicRuntime selects dummy
  outside Windows. Data/events/captions can be validated, but real audio PASS
  cannot be claimed. No new audio backend is implied by this Goal.

## Resume point

Current gmake Debug/Release automation and startup negatives have passed after
cursor repair. Poll Xcode session 70054 and short-soak session 31410 before any
restart; logs are in `build/goal-20260905/`. Inspect the latest sections below
for package hashes. Independent acceptance permission remains unanswered;
strict AI-06 additionally needs package-only filesystem access. Windows is
postponed by the owner; required unexecuted macOS acceptance prevents completion.

## macOS menu cursor repair (implemented; focused regression passed)

- Owner reproduced the stuck cursor before Single Player. Root cause: bundled
  OIS CocoaMouse unconditionally disassociated/hid the system cursor both in
  initialization and mouseEntered, independent of GameApplicationFlow.
- Required behavior: native pointer in main menu, world list, pause, containers,
  crafting and blocking modals; relative capture only during focused Playing
  with mouse look enabled; balanced restore on focus loss and shutdown.
- Repair connects Cocoa capture to the existing application capture policy,
  removes unconditional enter/initialize capture, and uses native window
  coordinates for free-pointer motion and clicks, including resized views.
- The preceding clean Debug/Release build and all 13 suites passed in
  `build/goal-20260905/macos-full-verification.log`. That is pre-repair evidence;
  the Cocoa repair requires fresh builds and normal-window verification.
- Current Debug rebuild: `MAKEFLAGS=-j2 bash scripts/build.sh debug`, log
  `build/goal-20260905/macos-cursor-debug-build.log`. No UI PASS claimed yet.

### Cursor repair verification

- `MAKEFLAGS=-j2 bash scripts/verify_build.sh` completed PASS after the repair:
  both macOS x86_64 Debug and Release, all 13 suites, including World 991
  and P11A focus/input regression. Log:
  `build/goal-20260905/macos-cursor-verification.log`.
- Normal-window CUA supplemental verification used the Debug package
  `/private/tmp/HelloMine3D-Cursor-Fix-20260905.app`, executable SHA-256
  `f223c17e8db2743d8097b4964dbae55aaeebc6f29abac21eca4698031a4dac7a`.
  Observed actual actions: Single Player opens Worlds; Create makes New World
  seed 834721664; Play enters terrain; Escape opens pause; Save and Main Menu
  returns to menu; Single Player/Play reloads the saved world; E opens crafting;
  native click Close dismisses crafting; Escape then Save and Quit exits.
  Screenshots and input calls are in this task's CUA transcript. Menu/crafting/
  pause buttons followed their native pointer coordinates. This is regression
  evidence by the implementer, not independent AI-01–08 or blind AI-06 PASS.
- Focus loss and resize are implemented but not yet separately exercised in
  this normal-window pass. Do not infer full input/platform acceptance.
- New Release package:
  `build/goal-20260905/HelloMine3D-macOS-Cursor-Fix.app`.
  Its build-identity.json and distribution-sha256.txt record current identity.
  The old preflight packages remain unchanged; users must open the new package
  to receive this fix. Windows testing remains postponed.

- Release normal-window confirmation also passed: click Single Player reached
  the empty world list in the new Release app (executable SHA-256
  `b52198ad1f929aadca68c5dc519fa66bf0a964d4fff99c8a0e7c507b9c3f07e5`).
  Left the window on Worlds for the owner to use. No test world was created
  in this Release package. Release startup-negative results are in
  `build/goal-20260905/macos-cursor-release-startup.log`.

## First local batch and remaining verification

- Owner explicitly requested local commits after each validated batch. The
  first batch includes the D2 entry decision, macOS catalogue assertion repair,
  cursor repair, portable packaging/startup checks and current-document sync.
  No remote push, release or tag is authorized or performed.
- Current Release startup negatives: all 15 PASS in
  `build/goal-20260905/macos-cursor-release-startup.log`.
- Clean package generated outside the repository at
  `/private/tmp/HelloMine3D-Goal-Release-20260905.app`; the same Release executable
  SHA-256 `b52198ad1f929aadca68c5dc519fa66bf0a964d4fff99c8a0e7c507b9c3f07e5`.
  Archive `build/goal-20260905/HelloMine3D-macOS-Goal-Release-clean.zip` SHA-256
  `7d659ef13d68531e44f6c77f19fafab52dc4e4c407f43c7a7b26c55aa5df964a`.
  Read-back verified every 106 inventory entries and exactly 107 archive files
  including the inventory. JSON evidence: `macos-clean-release-archive-check.json`.
  The first ditto archive included unlisted AppleDouble metadata and was rejected;
  it remains for audit. The clean ZIP was recreated from explicit files using
  Python zipfile, preserving executable mode and without dropping inventory checks.
- `bash scripts/verify_xcode.sh` first failed because sandbox denied DerivedData
  writes. The authorized standard-cache rerun is active, log
  `macos-xcode-verification-authorized.log`. This is not yet Xcode PASS.
- Release nominal/stress 120-second soak is active, evidence directory
  `macos-release-short-soak`. This is a short regression, not formal 1800-second Q3.
- A current read-only search found the starting bundle absent from the repository,
  Workspace and /private/tmp. Do not claim its current preservation or recreate an
  artifact under its identity. The original bundle import commit is unchanged.
