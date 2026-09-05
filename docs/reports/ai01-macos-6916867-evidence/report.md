# AI-01 macOS independent normal-window acceptance
Status: BLOCKED (some subitems PASS). Not AI-06 blind-play acceptance.

## Initial mandate and boundary
Inspect only the supplied release app and files below /private/tmp/hm3d-independent-ai01-6916867. Use ordinary GUI to create/load world, move/look, pause/resume, Cmd+Tab, minimize/restore, close, persist settings/restart. Do not read source/tests/internal guides; no fixture, gameplay API, inventory injection, teleport, forced position or save editing. Read-only logs/settings may corroborate UI after play. All authored records belong in evidence.
repository_accessible=true; context_isolation=PARTIAL. Tools retain repository read access despite requested procedural boundary; no claim of actual sandbox isolation. No source file was read. Packaged README was read (contains architecture/test descriptions as well as user-facing documentation); none of its test commands executed.

## Identity
Source version supplied: 69168670698ab67e7d345bf5e28cd9eeb099c80e.
package.zip SHA256 verified: 4b0548e57895d4e1cbc185881055e5c494e6a4fbb7dfb2ece7899df10c108d66.
Actual Mach-O Contents/Resources/bin/HelloMine3D SHA256 verified: aeb8a2b0f6c46f1ba69363de63d681ea6e1832e02bcf95e96b77aec81a6a0a8b.
Launcher Contents/MacOS/HelloMine3D SHA256: e65eed5f5391e16f83a646c3cce1e7bd1af65cb761b301cf0848183e82a3eb7a. Initially compared launcher with supplied binary hash, then resolved difference by reading launcher and checking actual Mach-O. No package changes.
macOS 15.7.3 (24G419), arm64. Initial timestamp 2026-09-05T03:06:45Z.
Initial screenshot: 1280x748 window image including title bar; visible client is approximately 1280x720. Locale appears English. Graphics settings not yet inspected. World/seed not yet created.

## Evidence and steps
CUA documentation exposes screenshot bytes but no documented filesystem-save API. Screenshots are genuine inline CUA tool records in this task, not fabricated local paths.
1. Read CUA documentation / inventory. No unrelated app operated.
2. Hash/package/OS read-only verification as above.
3. cua.getApp('/private/tmp/hm3d-independent-ai01-6916867/HelloMine3D-6916867.app') returned window HelloMine3D after reported wall time 4378.3700 seconds. No duplicate launch attempted. Tool delay cannot be attributed to game startup without further evidence.
4. First game.getScreenshot() returned in 6.0554 s: normal window, blue sky background, HelloMine3D menu with Single Player, Credits, Quit. Tool screenshot is evidence S01.
5. ~04:21–04:27Z: Single Player opened Worlds (0 active), default seed 1547913298, Normal. Name field Cmd+A and Ctrl+A each inserted `a` instead of selecting. Retried with 65 ordinary BackSpace presses and typed AI01-6916867; screenshot confirmed correct field. Create succeeded; Worlds row AI01-6916867 / Seed 1547913298 / Normal and World created. New-form seed randomized after creation, distinct from created row.
6. Play loaded rendered hillside with stone/grass, health 20/20, empty hotbar, Journey 0/33. Hints: E Crafting, R Eat held food, Mouse secondary Guard while aiming at an enemy with a sword, Esc Pause. No audio listening performed.
7. D x20 and S x20 ordinary pressKey taps did not produce visually confirmable translation. Esc opened Paused. W x10 while paused followed by Resume returned same terrain/crosshair composition, health unchanged. Pause/resume UI works; movement/no-stuck-key assurance BLOCKED: documented CUA has no key-down/up or hold duration. Cannot classify lack of movement as application failure from these taps alone.
8. Ordinary mouse drag (640,390) to (750,400) did not visibly change camera orientation. No documented pure relative mouse movement API; look coverage BLOCKED pending distinction between CUA event behavior and game.
9. Raised native window using exposed AX Raise; tried S, then super+Tab. Tool returned but application-specific state/screenshot did not establish actual OS switch. Cmd+Tab/return coverage BLOCKED; no false PASS.
10. Esc -> Settings: 1280x720, fullscreen unchecked, render distance 8, directional shadows Off, post-processing unchecked, FOV 90, sensitivity .050, invert Y unchecked, sprint/sneak Hold, action feedback Full, English, UI 1.00x, action hints enabled. Audio captions enabled; no audible test.
11. Changed language dropdown English -> Simplified Chinese, clicked Apply. Chinese paused UI and message 设置已保存并应用 observed.
12. Native minimize AX button clicked. Focused-element line disappeared, screenshot retained paused frame. AX Raise restored focused element and fresh FPS, same paused composition. Minimize/restore action and focus change observed, but desktop itself not available as evidence. No crash during observed sequence.
13. ~04:30–04:31Z: Save and Main Menu returned Chinese main menu. Native AX close button ended process; subsequent AX read returned procNotFound, not an unexplained crash. Same exact app path restarted in 8.2509 s (contrasts earlier tool delay); Chinese menu remained. Worlds showed AI01-6916867, seed 1547913298, Normal. Play reloaded same hillside, health 20, journey 0/33.
14. ~04:32:59Z: Esc then Chinese 保存并退出. CUA returned App quit. Read-only final Ogre log confirms orderly OGRE Shutdown at local 12:32:59. Test window is closed.
15. ~04:33Z read-only corroboration: config.txt settings_version 8, locale zh-CN, 1280x720, renderdistance 8, shadows/postprocessing off. world.meta identifies world-9e31230dc21d00bc4c1582cf3a140b77, AI01-6916867, seed 1547913298. Final position equals spawn 152.5 127 264.5, rotation 0 0 0, corroborating that attempted movement/look were not achieved. Logs/settings/world metadata copied to evidence after GUI work, without editing originals.

## Final results
Overall AI-01: **BLOCKED / NOT PASS**. Required movement, look and OS switching coverage remains incomplete. No AI-06 claim.

| Subitem | Result | Evidence / scope |
|---|---|---|
| Menu navigation | PASS | Main -> Worlds -> game -> Paused -> Settings -> main, inline CUA screenshots |
| Create/save/load | PASS | Created through UI; restarted app; same name/seed loaded actual terrain |
| Movement | BLOCKED | D/S taps no displacement; no documented key-hold API; world-after.meta still spawn |
| Mouse look | BLOCKED | Drag no orientation change; no relative mouse-move API; rotation remains 0 |
| Pause/resume | PASS | Esc opens paused; Resume restores same frame composition |
| Input isolation / no stuck keys | BLOCKED | Paused W taps did not visibly leak, but meaningful held/released movement untested |
| Cmd+Tab out/back | BLOCKED | super+Tab attempted; no reliable desktop foreground transition evidence |
| Minimize/restore | PASS (bounded) | Native minimize removed AX focus; Raise restored focus and live frame; no position jump observed from stationary state |
| Native window close | PASS | Native close ended process; same app restarted successfully |
| Settings persistence | PASS | English -> Chinese Apply; Chinese restored after process restart; settings-after.txt locale zh-CN |
| No unexpected exit | PASS for exercised steps | Both exits requested; restart-ogre.log orderly shutdown |
| Text selection modifier compatibility | FAIL observation, cause unassigned | Cmd+A/Ctrl+A inserted a; ordinary BackSpace recovered; parent investigating candidate |
| Audio | NOT_RUN | Not required for AI-01; audio not heard |

Evidence files: report.md, settings-after.txt, restart-ogre.log, world-after.meta under this evidence directory. Visual evidence is the actual inline CUA getScreenshot records in task /root/macos_ai01_acceptance; no local image files claimed. Tool APIs expose app-only screenshots and undocumented holding/relative pointer/system switching gaps limit what can be certified. Source version is parent-supplied; binary/package hashes were independently measured. Repository access remained technically available throughout, and context isolation is PARTIAL.

## Execution boundary clarification
initial_prompt: original parent NEW_TASK message in /root/macos_ai01_acceptance beginning “你是独立玩法验收者，用户已明确授权独立子代理验收”, identifying 6916867 package and AI-01 scope. Shell working_directory was tool default /Users/lizi/Desktop/Workspace/HelloMine3D (pwd verified); all read/write command targets were absolute release/evidence paths. App launcher sets its own working directory to original Contents/Resources/bin. accessible_roots configuration permits read :root, including repository; write roots are project, /private/tmp, supplied per-user tmpdir and visualization directory. Effective OS access was not exhaustively tested; exact least-privilege root set not certified. Procedural read boundary was release directory, with no repository file read. repository_accessible=true; context_isolation=PARTIAL. Separate candidate result is in keyboard-retest.md and does not retroactively change this original-package report.
