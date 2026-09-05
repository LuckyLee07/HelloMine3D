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
| Fix current-document inconsistencies | Done for current evidence | README, ledger, architecture/tutorial and validation routes updated; final check after validation |
| Execute relevant automated checks | Done (current engineering scope) | Path-identity assertion fixed; post-cursor gmake Debug/Release 13 suites PASS; Xcode double-configuration gate PASS |
| AI-01 | BLOCKED overall | Independent menu/save/load/settings steps and shortcut repair pass; focus follow-up completed with native restore/switching unresolved; movement/look/held-key checks tool-blocked. |
| AI-02..AI-05, AI-08 | NOT_RUN | Hashed clean macOS Release package, OS input, normal gameplay; no fixtures |
| AI-07 | BLOCKED overall; bounded layout repairs PASS | Six combinations passed form/HUD follow-up; en/zh1.75 and zh0.75 seed-row follow-up passed with actual Play. Full visual/dynamic/audio and persistent screenshot evidence remain open. |
| AI-06 | BLOCKED | Actual package-only filesystem access unavailable; fresh subagent alone remains PARTIAL and cannot supply blind PASS. |
| New macOS gameplay/visual acceptance | BLOCKED (partial results retained) | Explicitly authorized by subsequent owner steering; independent macOS evidence |
| Windows-specific verification | Postponed by owner | Not a required exit item of this macOS Goal; do not claim Windows PASS |
| Final delivery | Done for available engineering scope | Code/evidence are committed together locally; clean delivery identity is recorded under build/goal-20260905/layout-delivery.json. Formal acceptance gaps remain; no push/release/tag. |

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

All three discovered layout defects have completed implementation, final Xcode
Debug/Release gate `build/xcode-validation-20260905143633` and bounded independent
normal-window follow-up. Evidence directories retain original failures, six
form/HUD combinations, and final en/zh1.75 plus zh0.75 seed-column checks with
actual Play. Final tested Mach-O SHA is
`c0446b47bbdcf4b4e2ef69248e4d466288db1a10700c821e37ca2fb2e6480424`.
This local layout batch commits code and evidence together. Clean repackaging
identity/commit is in `build/goal-20260905/layout-delivery.json`; repackaging the
same binary is not a new GUI run. Both examiners have released GUI and finished.
No further independent engineering task remains in the frozen scope without a
new failure or capability change. D2 stays Candidate after evidenced non-entry.
Restore/switching, sustained/relative input, strict AI-06 filesystem isolation,
persistent screenshots and real audio still prevent full formal acceptance.
Windows is postponed. Do not repeatedly rerun unchanged blockers or mark Goal
Done. Resume on the minimum capability conditions in the completion audit.

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

### Post-commit verification update

- Local baseline commit: `ea7a85d9d8ec0dcec524f8cad436ddb363e34cbd`.
  No push or tag. The committed source matches the source used by the recorded
  post-cursor gmake checks; earlier package metadata retains its original
  starting commit + tracked-diff identity rather than being rewritten.
- Release short soak completed: nominal 120 seconds PASS, stress 120 seconds
  PASS. Commands, executable and source hashes are in
  `build/goal-20260905/macos-release-short-soak/identity.json`; per-profile logs
  and generated summaries remain alongside it. Not a full 1800-second Q3 run.
- Clean package preflight from `/private/tmp`, using only the external package's
  Resources root, fixed documented seed 20260809 and position 3038/66/1922,
  `HELLOMINE3D_VALIDATE_ONLY=1`, exited 0. Log:
  `build/goal-20260905/macos-clean-package-preflight.log`. This validates packaged
  resources/renderer startup, not normal gameplay or source-access isolation.
- Current-document link audit passes 98 relative links, missing=0, evidence
  `build/goal-20260905/current-document-links.json` tied to ea7a85d.
- Xcode session 70054 remains live; at this update it is running Debug world
  runtime tests. Continue observing that handle/log; do not start a duplicate.
  Short-soak and package-preflight sessions have finished successfully.

## Xcode Release floating-point configuration repair (new batch)

- The authorized Xcode run completed Debug automation and probes, then failed
  the existing first-party warning gate in Release_HelloMine3D_build.log:
  ResourceEconomyVerifier.cpp infinity sentinel is incompatible with generated
  `-ffast-math`. No warning filter or expectation was relaxed.
- Generator inspection showed workspace `optimize "Full"` maps Xcode Release to
  `-ffast-math`. `floatingpoint "Strict"` did not remove that generated flag in
  the installed generator; that trial was not retained. The selected repair
  overrides only Xcode Release to `optimize "Speed"`: generated optimization
  level remains 3 while fast-math is absent. VS and gmake options are untouched.
- Existing RecipeSmoke tests include missing acquisition sources and unreachable
  required-material rejection; those retain their original expectations and
  exercise the infinity/isfinite behavior under the repaired Release settings.
- Full unmodified acceptance route restarted after the terminal failure:
  `bash scripts/verify_xcode.sh`, log
  `build/goal-20260905/macos-xcode-verification-float-fix.log`. It is in progress,
  not PASS. This batch will be committed only after applicable verification.
- The source patch from initial commit to ea7a85d matches the short-soak source
  hash exactly (`00b0bac996b3da83333a80990bd61e6639e52c0f826249a130892905741be47f`),
  saved as `build/goal-20260905/ea7a85d-source.patch` for regression comparison.

### Xcode floating-point repair verification — PASS

- `bash scripts/verify_xcode.sh` completed with exit 0 and terminal PASS.
  Original unchanged checks passed for both Debug and Release: all 13 test
  executables (World 991, Recipe 126, Resources 80, Catalogue 59, transaction
  16, backup 19, timing 12, crash diagnostics 21), renderer validation and
  window probes, surface contact y=66, startup/world-entry timing fields.
- Both configurations passed `N10/missing-source-dead-chain-is-rejected`.
  No infinity/isfinite logic, expected outcome or warning gate was changed.
- Build/individual test/probe logs:
  `build/xcode-validation-20260905102920`; aggregate log:
  `build/goal-20260905/macos-xcode-verification-float-fix.log`.
  The Xcode graph validator also passed all 31 projects and nine self-tests.
- Verified build identity in `build/goal-20260905/xcode-verified-build-identity.json`:
  Xcode Release x86_64 on macOS 15.7.3 arm64 host; executable SHA-256
  `aeb8a2b0f6c46f1ba69363de63d681ea6e1832e02bcf95e96b77aec81a6a0a8b`.
  Premake diff SHA-256:
  `f1ceb5715a9d0699093deb08ee8b7a8be0415f4600eee92ff4c5c03a4d8283b7`.
- This closes the Xcode engineering gate only. Scripted fixed-position probes
  do not close independent normal-input AI scenarios or real audio.

- Xcode Release startup-negative follow-up completed all 15 original fixtures
  with exit 0: `python3 tools/validate_startup_errors_macos.py --output
  build/goal-20260905/macos-xcode-release-startup`. Aggregate:
  `build/goal-20260905/macos-xcode-release-startup.log`. Each case preserves
  stderr/report evidence; this is macOS stderr-only behavior, not MessageBoxW.

## Verified local delivery — Xcode package

- Configuration repair committed locally as
  `69168670698ab67e7d345bf5e28cd9eeb099c80e`; initial repair/investigation batch
  remains `ea7a85d9d8ec0dcec524f8cad436ddb363e34cbd`.
- `python3 tools/package_macos_release.py --configuration Release --output
  /private/tmp/HelloMine3D-6916867.app` created a clean external package from
  the verified Xcode Release executable. At packaging, the tracked diff was
  empty. Source commit, executable and resource hashes are in build-identity.json.
- Archive: `build/goal-20260905/HelloMine3D-macOS-6916867.zip`, SHA-256
  `4b0548e57895d4e1cbc185881055e5c494e6a4fbb7dfb2ece7899df10c108d66`.
  Explicit-file ZIP read-back passed all 106 inventory hashes and exact archive
  membership (107 files including inventory). Evidence:
  `build/goal-20260905/xcode-6916867-archive-check.json`.
- Actual CUA window supplement on this package: main menu → Single Player →
  empty Worlds list → Back to Main Menu → Quit. Normal input worked, the app
  exited normally, no fixture or validation override was used. Window 1280x748,
  English defaults. No world created. CUA screenshots/actions remain in this
  task transcript. Same implementer context: **not formal AI-01 or AI-06 PASS**.
- Completion audit is in
  [todolist-goal-completion-audit-2026-09-05.md](todolist-goal-completion-audit-2026-09-05.md).
  All useful non-dependent engineering work is now complete. Independent
  executor permission was requested again during this continuation and remains
  unanswered. Do not interpret elapsed time as consent. Remaining isolation and
  audio requirements are documented; no same-condition retry is planned.

## Owner-authorized independent acceptance resumed

- Owner replied “好的，允许” to the pending independent acceptance request.
  This authorizes a separate acceptance subagent; it does not waive package-only
  isolation, audio evidence, or any original completion condition.
- Fresh-context subagent `macos_ai01_acceptance` is assigned only AI-01
  client-shell using normal OS input. It owns the test GUI exclusively while
  the parent updates records. No implementation history was forked.
- Clean archive was hash-verified and extracted with executable modes restored
  to `/private/tmp/hm3d-independent-ai01-6916867`; original package.zip is copied
  there, and output belongs in its `evidence` directory. Current package/source
  identity remains 6916867 and the recorded SHA-256 above.
- The subagent was instructed not to read repository/source/tests. Actual tool
  roots still permit repository access, so `repository_accessible=true` and
  `context_isolation=PARTIAL` must be recorded. This is eligible for independent
  scripted AI-01 evidence, not strict AI-06 blind PASS.
- AI-01 is now Doing, pending actual per-step results. AI-02..AI-08 are not
  silently advanced. The prior repeated-blocker audit is reset by new owner
  authorization and useful resumed work. Isolation/audio gaps remain open.

## Independent AI-01 finding: macOS modifier chords

- Owner subsequently authorized all project permissions; implementation,
  validation, independent acceptance and necessary permission requests proceed
  autonomously. This does not change acceptance truth or the earlier no-push/
  no-release/no-tag constraint.
- Independent examiner successfully reached Worlds and created AI01-6916867,
  seed 1547913298, Normal, then entered terrain and paused via Escape. It
  reported both Cmd+A and Ctrl+A inserting `a` instead of selecting text.
- The examiner's original getApp call returned after 4378.3700 seconds. Record
  this as CUA wall-clock delay with undetermined cause, not proven game startup
  latency. The same call eventually returned a valid window; no restart was
  inferred from observation timeouts.
- Synthetic non-visible Cocoa regression in `tools/tests/cocoa_keyboard_test.mm`
  reproduced three failures against the original OIS library: quick command
  chord event state, combined modifier transitions, and held modifier state
  before queued release. Two release-state checks already passed. Raw log:
  `build/goal-20260905/cocoa-keyboard-before.log`.
- Repair masks Cocoa device-dependent modifier bits, handles each aggregate
  transition, corrects OIS modifier bit updates, and preserves dispatched key
  state separately from the final physical snapshot while callbacks execute.
  The same five checks now pass: `cocoa-keyboard-after.log`. This synthetic
  diagnostic is not substituted for actual GUI acceptance.
- Diagnostic command: `clang++ -std=c++17 -arch x86_64 -I src/external/ois/includes
  tools/tests/cocoa_keyboard_test.mm build/External/ois/lib/x64/Release/libois.a
  -framework Cocoa -framework Carbon -framework IOKit
  -o /private/tmp/hm3d-cocoa-keyboard-test`, then run that executable. Initial
  linkage against the Xcode library reported deployment-target warnings;
  the repaired OIS-only gmake rebuild and rerun completed with zero failures.
- Full Xcode Debug/Release gate is running for this source change; log:
  `build/goal-20260905/macos-xcode-keyboard-verification.log`.
  Independent examiner continues the original immutable package for other
  AI-01 steps. A new package will be provided for focused real-input retest
  after engineering checks; no mid-run replacement of its original package.

### Independent AI-01 result and keyboard follow-up

- Original 6916867 package AI-01 is BLOCKED overall. Menu, create/save/load,
  pause/resume, language persistence and normal close were observed successfully.
  Movement/look, verified Cmd+Tab and held-key checks could not be established
  with the available CUA operations. Repository access remains possible, so
  context isolation is PARTIAL and this is not AI-06 evidence.
- Durable examiner evidence: `ai01-macos-6916867-evidence/report.md` and
  `sha256.json`. The original report's failures are preserved.
- First keyboard candidate passed two independent Cmd+A replacement/release
  rounds, but Ctrl+A still inserted `a`; see `keyboard-retest.md`. Candidate
  archive SHA-256: `201f4c7ba6c5eb2d67b3f6c3f370727b0c9618221f554d0d2e4c0105a791f3fc`.
  Its full Xcode gate passed at `build/xcode-validation-20260905122918`.
- A second focused regression reproduced printable text on both shortcut chords
  (`cocoa-keyboard-text-before.log`, 2 failures). The repair retains physical key
  events while suppressing Control/Command text, uses native translated text,
  and removes unsafe fixed-size/empty-string access. Ten synthetic checks now
  pass (`cocoa-keyboard-text-after.log`), including empty/native/long text.
- Current source identity: f0e63d2 plus uncommitted Cocoa keyboard repair;
  full Debug/Release Xcode rerun is recorded in
  `build/goal-20260905/macos-xcode-keyboard-text-verification.log`.
  Next: independently retest the new immutable package, then commit this batch.
- A separate read-only focus audit found that render-active state does not
  reliably represent macOS key-window/application focus. Address after the
  keyboard candidate is frozen; it must not alter that candidate mid-acceptance.

- Second keyboard candidate full Xcode Debug/Release gate: PASS, all 13 suites
  and client probes, `build/xcode-validation-20260905124507`. Candidate ZIP
  SHA-256 `61de89b1ce26da6f873f4ee708953393f9af4f66978ce8bee5b7f4de05ecd877`,
  executable `419b401bbdcf6e3d5520276ce224701b69d23ab49178930e600882a3e3d31158`.
  Build identity retained in `ai01-macos-6916867-evidence/keyboard2-build-identity.json`.
  Normal-window independent retest is running against this immutable package.

- Independent second-candidate retest: PASS for two Cmd+A replacement/release
  rounds, two Ctrl+A no-extra-text rounds, ordinary input, Shift uppercase and
  Backspace. Normal menu Quit succeeded. Full record and hashes are retained in
  `ai01-macos-6916867-evidence/keyboard2-retest.md`; this bounded fix does not
  close the blocked AI-01 overall scenario.
- Keyboard batch document checks: 46 local links resolve; `git diff --check`
  passes. No Windows gate, physical held-key or audio PASS is inferred.

## macOS native input focus repair

- Preceding verified keyboard batch committed locally as `fb8f275`.
- Read-only audit found render-active state is kept alive in the background and
  is not a native key-window predicate. OIS also kept held keys/modifiers when
  focus loss prevented receipt of key-up. `clearTransientInput()` did not clear
  OIS or buffered UI key state; simply fixing the focus predicate was insufficient.
- Applied native WINDOW_FOCUSED query (key window AND active app), shared it
  with cursor policy, and added macOS-only transition diagnostics. CocoaKeyboard
  now observes window/app loss, cancels stale queued input, clears polling and
  modifier state, and delivers releases to buffered clients. Events from inactive
  or foreign windows are excluded. Duplicate notifications preserve pending releases.
- New non-visible Cocoa focus test contains the existing 10 shortcut/text checks
  plus 14 focus checks. Same test against pre-focus source fails 8 focus checks:
  `macos-focus-20260905-evidence/focus-baseline-test.log`. Temporary draft passes
  all 24 and preserves all 10 original keyboard assertions. This is automated
  evidence using synthetic focus getters/notifications, not OS acceptance.
- Full applied-source Xcode gate running:
  `build/goal-20260905/macos-xcode-focus-verification.log`. After it passes, rerun
  both targeted tests against the newly built archive, package, and independently
  observe real native focus loss/return and menu/text/cursor behavior.

- Applied-source initial focus full gate PASS:
  `build/xcode-validation-20260905125632` (both configurations, all 13 suites,
  client probes). The two targeted tests also pass against the rebuilt Debug
  archive; copied logs are under `macos-focus-20260905-evidence/`.
- Targeted command per test: `clang++ -arch x86_64 -std=c++17
  -mmacosx-version-min=26.2 -fblocks -I src/external/ois/includes
  tools/tests/<test>.mm build/External/ois/lib/x64/Debug/libois.a
  -framework Cocoa -framework IOKit -framework ForceFeedback -framework Carbon
  -o /private/tmp/<test>`, then run. Tests are `cocoa_keyboard_test` and
  `cocoa_keyboard_focus_test`; AppKit XPC diagnostics did not prevent completion.
- Post-gate diagnostic-only follow-up routes native focus transitions into the
  existing MineOgre.log instead of uncaptured stdout, to corroborate subsequent
  normal-window observations. A final rebuild remains necessary.

- Mouse follow-up reproduced the same focus-loss gap: left/right/middle device
  and buffered-client buttons remained down without mouseUp. GameplayFocusGate
  correctly blocked background actions but could not re-arm with stale buttons.
  Focus loss now clears device buttons/deltas and notifies buffered releases;
  ordinary menu capture changes retain button semantics. Background capture
  performs the same reset as a fallback.
- UI focus handling cancels queued/held input before AddFocusEvent(false), so
  synthetic releases cannot activate a previously pressed widget, including
  loss/return within a single frame. Calls are scoped to native macOS focus
  transitions. No Windows behavior is claimed tested by this change.
- Final combined gate is running in
  `build/goal-20260905/macos-xcode-native-input-focus-final.log`.

- Final mouse regression has 27 checks (9 per left/right/middle button), including
  duplicate notifications, ordinary capture changes, fresh clicks and inactive
  capture fallback. Baseline fails 15; repaired archive passes all 27. Logs:
  `macos-focus-20260905-evidence/mouse-baseline-test.log` and `mouse-applied-test.log`.
- Four pure ImGui dependency-semantic checks pass: control release can activate,
  focus-loss release does not, same-frame clear/loss/release/gain does not, and
  subsequent normal click works. `imgui-focus-semantics-applied.log` records this
  design check; it does not pretend to exercise the renderer/UI production wiring.
- Mouse command uses the same Cocoa compile/link flags as keyboard plus
  `-I src/HelloMine3D`, `tools/tests/cocoa_mouse_focus_test.mm` and
  `src/HelloMine3D/GameplayInput.cpp`. ImGui command uses
  `tools/tests/imgui_focus_semantics_test.cpp`, `-I src/external/imgui` and
  `build/External/imgui/lib/x64/Debug/libimgui.a`. Both run non-visible, x86_64
  Debug archives, on the recorded macOS host.

- Final combined Xcode gate PASS (Debug + Release, all 13 suites and client
  probes): `build/xcode-validation-20260905130430`. Source is fb8f275 plus
  the focus batch; package build identity and exact source hashes are retained
  under `macos-focus-20260905-evidence/`.
- Frozen focus ZIP SHA-256:
  `a51fd9bcc374b3f05de8bcef93703c3f122dd0e721dca324537d1271846b95bd`;
  Mach-O SHA-256:
  `bf07e9b3ff9a39e1b8a0aca375e94a75ead8eade4d1239c11c0204fe8077cbcd`.
  Independent examiner now has exclusive GUI ownership for focus/minimize/menu
  retest; ordinary Calculator is explicitly permitted only as the focus-switch
  target. No held-key physical-input PASS is requested or inferred.

- Added reproducible Cocoa regression runner `scripts/verify_cocoa_input.sh`.
  Current Release archives pass via exact command
  `HELLOMINE3D_INPUT_TEST_MIN_MACOS=26.2 bash scripts/verify_cocoa_input.sh Release`;
  logs: `build/cocoa-input-20260905130936-Release`, durable aggregate
  `macos-focus-20260905-evidence/release-input-regression.log`. It covers keyboard
  10, keyboard/focus 24 (including those same 10), mouse 27 and ImGui semantics 4.
  The minimum target matches these Xcode archives; x86_64 on arm64/Rosetta is
  explicit. The runner neither displays windows nor closes OS acceptance.

- Focus GUI examiner was interrupted once after an external-app tool call hung:
  `cua.getApp('com.apple.calculator')` reported aborted after 688.7 seconds.
  Its preceding actual game screenshot showed correct `Focusax` text replacement.
  Calculator-to-game switching, minimize and world entry had not yet occurred;
  no outcomes are inferred from the delay. Parent read-only game log contained
  native focus transitions at 13:09:16 and 13:09:45, diagnostic evidence only.
- Examiner is restoring its control session once using reset/getState without
  relaunching the game. External-app switching remains BLOCKED; continue the
  independent existing-window checks if recovery succeeds. No repeated app
  launch or permission re-request; this was a tool delay, not an approval denial.

- Control-session recovery succeeded: reset/getState returned in 7.6 seconds;
  bundle-ID selection was ambiguous across old packages, so the examiner bound
  the running candidate by its exact returned path without relaunching it.
- Native minimize produced `[INPUT_FOCUS] focused=0` at 13:25:04/frame 98715.
  Subsequent AX Raise/title click did not produce a logged return to focus=1.
  Name replacement no longer responded, while mouse Create created Focusax
  (seed 1808602498). This is preserved as an observed input failure with product
  cause undetermined: CUA could not establish restored native activation.
- The exposed AX tree/API has no readable/settable minimized-state attribute or
  explicit unminimize action. Do not invent a private API or treat AX focus and
  a window screenshot as proof of global native activation. Restore and external
  switching remain BLOCKED; examiner is finishing one bounded Play/Esc check
  and native close. The remaining non-switching visual cases can use a subsequent
  normal launch; no whole AI-01 PASS is inferred.

- Independent focus follow-up completed: BLOCKED overall, with successful basic
  text, mouse Create/Play and normal native close. Created Focusax, seed
  1808602498, id `world-9e31230cde3c648b7c54ec7ae3a39778`. Restored text and Esc
  were unresponsive without a logged native focus=1; cause remains unassigned
  to product. `focus-retest.md` and `focus-ogre.log` are retained with hashes.
- This local focus batch preserves completed engineering work and unresolved
  real-window acceptance explicitly. It is not labelled fully accepted or Done.
  Final current-document audit corrected two stale aggregate AI NOT_RUN claims
  in the roadmap and validation matrix. Frozen candidate's 11 source/test file
  hashes still match; `git diff --check` passes.

## Clean focus delivery and AI-07 menu subset

- Focus engineering batch committed locally as
  `ffe0684c21ff7aaa9e16ba5a6fea2cb3e5085f66`; no push/release/tag.
- Clean working tree repackaged with source ffe0684 and empty tracked-diff SHA
  `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.
  ZIP `build/goal-20260905/HelloMine3D-macOS-ffe0684.zip` SHA-256
  `bea8cc3cde598ca675aa496515dff4ebcebcbb1d9c287442dbf4595a1fc1d450`;
  executable remains `bf07e9b3ff9a39e1b8a0aca375e94a75ead8eade4d1239c11c0204fe8077cbcd`.
  This maps the tested runtime to a clean local commit; no new binary execution
  result is invented by repackaging. Identity: `ai07-macos-ffe0684-evidence/`.
- Independent AI-07 menu/locale/scale subset is running from that clean package.
  Six combinations: en-US/zh-CN × 0.75/1.00/1.75, ordinary menu/world/settings/
  credits/pause/crafting/objective views where reachable. No world fixtures,
  position changes, file edits or unsupported audio/dynamic/full-matrix claims.

### AI-07 discovered layout regressions

- Independent original clean ffe0684 package: en-US 1.00 and 0.75 normal
  reachable panels pass the observed checks. At 1280x720/en-US/1.75 the Worlds
  creation row clips Create at the right edge and shows only part of the seed.
  Existing-world row seed and Play/Delete remain complete; do not alter them
  based on hypothetical overflow.
- At 1.75 in both languages the Journey HUD progress overlay (0/11) is
  vertically clipped by its bar. Pause objective text remains complete.
- Repair in OgreUserInterface: create form uses two stretch columns with labels
  above controls; seed field receives its column width and Create has its own
  cell. The objective progress bar uses current frame height to fit scaled text.
  No controls are removed and scale limits are unchanged. The first form change
  compiled Debug/Release; combined HUD/form change still needs the final gate
  and independent retest. Original candidate is immutable; remaining Chinese
  combinations are still being examined before the new package is frozen.

- Original zh-CN 1.75: Create label fits (shorter text), but seed editor is still
  truncated. Existing-world seed and Enter/Delete fit. Menu/settings/credits/
  pause/empty crafting text and return paths pass observed checks; Credits and
  crafting scroll to their bottom. HUD progress digits clip in both languages.
  Chinese 1.00/0.75 checks remain in progress before original-package close.

- Combined form/HUD Debug and Release client compilation completed successfully,
  with no compiler warnings in the two `layout-combined-*-build.log` files.
  Independent code review found no blocking behavior/ID regression: business
  values/call remain the same; bar ratio/text are unchanged. Required new-package
  checks include actual name/seed/difficulty editing + Create result, high-scale
  English/Chinese HUD readability and no adjacent overlap, plus low-scale checks.

- Owner explicitly authorized all project permissions and autonomous progress;
  continue local edits, checks and batch commits without repeated authorization.
  Required sandbox escalations remain tool-enforced; no current approval rejection.
- Original zh-CN 1.00 Journey progress digits are also visibly clipped; the
  earlier coarse panel observation does not supersede this specific FAIL.

- Original six-combination observation finished, including zh-CN 0.75 with
  readable small progress digits. Normal Quit is confirmed by OGRE Shutdown
  at 14:17:22 local time. Examiner released GUI before full layout Xcode gate.
- Final layout gate command: `bash scripts/verify_xcode.sh`, macOS15.7.3,
  Xcode/Apple Clang x86_64 Debug and Release, source ffe0684 plus the recorded
  OgreUserInterface layout diff. Aggregate log `build/goal-20260905/
  macos-xcode-layout-final.log`; result still pending.

- Original AI-07 report, application log and final UI settings are retained in
  `ai07-macos-ffe0684-evidence/` with SHA-256 manifest. Final matrix supersedes
  preliminary broad panel wording: en-US1.00 HUD is UNCONFIRMED; zh-CN1.00
  and both 1.75 HUD samples fail. Screenshot evidence is genuine inline CUA
  output; persistent image-file/hash evidence remains unavailable, not invented.

### Final layout gate and frozen follow-up

- `bash scripts/verify_xcode.sh` exited 0: Debug/Release 13 suites, graph,
  performance contracts and client validate/window probes PASS. Current logs:
  `build/xcode-validation-20260905141759`; retained aggregate
  `ai07-macos-layout-evidence/xcode-gate.log`.
- Package command: `python3 tools/package_macos_release.py --configuration
  Release --output /private/tmp/HelloMine3D-Layout-20260905.app`, then
  `ditto -c -k --keepParent` into the immutable layout candidate ZIP.
  ZIP SHA `b2bb8ff42086c439a3648367abee92c305357e75206b1938fae7a48ab4989929`;
  Mach-O SHA `4bece0fc19a03a8268bfd3808d237a15244f3a0184646691aceb9568eb9e25f2`.
  Source identity/hash and build metadata retained in
  `ai07-macos-layout-evidence/`.
- Examiner now owns GUI for six-combination form/HUD follow-up and real
  name/seed/nondefault-difficulty creation. Old package/report preserved.
  This is bounded repair acceptance, not full AI-07 completion.

- Layout candidate follow-up confirms en-US1.00 and1.75 creation form/HUD
  repairs. Real ten-digit generated seed1213457900 exposed a third defect:
  at en-US1.75 the existing-world seed overlaps/clips against difficulty.
  Earlier nine-digit seed observation remains valid but did not cover this case.
- Follow-up repair removes the seed column's fixed150px initial width, letting
  ImGui size that fixed column from its actual content. Other columns/business
  actions unchanged. Current candidate remains immutable and examiner finishes
  its matrix before a new build/package follows. No final PASS/commit yet.

- First layout ZIP read-back: all106 declared file hashes match, but exact
  membership FAIL because `ditto` added130 AppleDouble metadata entries.
  Candidate is preserved unchanged; this does not invalidate observed binary
  behavior, but it is not the final distribution. Next package will use explicit
  file-only ZIP creation (as prior clean delivery), then verify exact membership.
  Original failure: `ai07-macos-layout-evidence/archive-check.json`.

- Candidate1 business follow-up: normal en-US1.75 input created Layout Check
  with seed1234567890 and visible nondefault Casual difficulty; pause after
  entry explicitly displayed Difficulty: Casual. HUD progress is complete.
  This verifies the changed form path; existing-row clipping remains separate.

- First layout candidate finished all six form/HUD combinations successfully.
  Independent en-US1.75 existing-row ten-digit seed remains FAIL; zh-CN1.75
  seed is readable but tight against difficulty. App exited normally and GUI
  was returned before final seed-width Xcode gate started.
  Gate command `bash scripts/verify_xcode.sh`, aggregate
  `build/goal-20260905/macos-xcode-layout-seed-final.log`; pending result.

### Seed-column final gate and follow-up

- Final `bash scripts/verify_xcode.sh` exited0, all Debug/Release13 suites
  and client probes PASS: `build/xcode-validation-20260905143633`. Aggregate
  retained in `ai07-macos-layout-seed-evidence/xcode-gate.log`.
- New external package `/private/tmp/HelloMine3D-Layout-Seed-20260905.app`
  was archived with Python ZipFile explicit regular-file traversal. Read-back
  checked every declared SHA and exact member set:106 entries,107 total files
  PASS. No AppleDouble entries. Old failed archive remains unchanged.
- ZIP SHA `a006b55c253aff4d31c5b78a0fe9be9eaea928d22c48cc407c742e0e4985bf61`;
  executable SHA `c0446b47bbdcf4b4e2ef69248e4d466288db1a10700c821e37ca2fb2e6480424`.
  Source hash/identity: `ai07-macos-layout-seed-evidence/package-identity.json`.
- Independent follow-up now checks only en/zh1.75 and a low-scale existing
  world row, normal ten-digit seed creation and actual Play; no need to rerun
  unchanged panels. Final repair acceptance remains pending.

- Final independent seed-column follow-up completed PASS: en-US1.75,
  zh-CN1.75 and zh-CN0.75 each observed stable two-frame complete ten-digit
  seed, visible gap before difficulty, readable name and intact Play/Delete.
  Actual high-scale Play → rendered world → Esc succeeded; HUD digits remained
  complete. Normal menu Quit returned App quit with no pending tool call.
  GUI is released; examiner is retaining final report/log/settings.
- This closes the three discovered layout defects within bounded normal-window
  checks. It does not close full AI-07 or other missing formal scenarios.

- Final report/log/settings/metadata retained with SHA manifest in
  `ai07-macos-layout-seed-evidence/`. Independent documentation review confirms
  bounded repair PASS is distinguished from overall AI-07 BLOCKED and Goal
  not complete. Prior gmake/startup-negative/soak results retain their revisions.
- Local batch includes these three UI repairs, the original and both follow-up
  evidence sets, and current-document synchronization. No remote publication.
  Final clean package is produced after committing, with the same tested binary;
  its identity is recorded outside the commit to avoid recursive commit/hash data.

- Final staged whitespace audit reports trailing spaces only in raw captured
  `.log` artifacts (182 lines). These are preserved byte-for-byte to keep
  recorded hashes valid; no log normalization or git whitespace rule change.
  Source, documentation, JSON, settings and metadata whitespace checks pass.
