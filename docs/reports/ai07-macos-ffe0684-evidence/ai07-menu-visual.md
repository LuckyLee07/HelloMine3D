# AI-07 bounded menu visual inspection
Status: FINISHED, bounded visual FAIL with two layout findings; no whole AI-07 PASS.
Identity: source ffe0684 (parent supplied), ZIP bea8cc3cde598ca675aa496515dff4ebcebcbb1d9c287442dbf4595a1fc1d450 and Mach-O bf07e9b3ff9a39e1b8a0aca375e94a75ead8eade4d1239c11c0204fe8077cbcd independently measured. App /private/tmp/hm3d-independent-ai01-6916867/visual-retest/HelloMine3D-ffe0684.app. macOS15.7.3/24G419 arm64, window client1280x720 screenshot1280x748. No external app/switch/minimize. UI-only scale and locale edits. Default renderdistance8, shadowOff, postprocessingOff, FOV90, sensitivity.050, fullscreenOff.
Initial mandate is parent NEW_TASK beginning “请继续有界 AI-07 菜单/本地化/布局视觉子项”, in task /root/macos_ai01_acceptance. repository_accessible=true; context_isolation=PARTIAL. Shell workingdir /Users/lizi/Desktop/Workspace/HelloMine3D; all absolute file accesses release/evidence paths. Read root config :root means repository readable; actual OS root boundary not exhaustively verified. Write roots supplied: project, /private/tmp, per-user tmpdir and visualization path from initial environment. No source/tests/internal guides read. App launcher cwd its Resources/bin.
Started2026-09-05T05:35:08Z, getApp12.2s. Normal UI created AI07 Visual seed528334149 Normal. No material acquired; only empty crafting inspected. No separate objective window discovered; objective HUD and pause objective text observed.
Screenshots are real inline CUA getScreenshot records identified by locale/scale/panel tool titles. Documented API has no file-save/hash helper; no PNG files or image hashes fabricated. Settings Apply confirms exact0.75/1.00/1.75 choices before inspection.

| Combination | Observed panels / result |
|---|---|
| en-US1.00 | Main, Worlds, Settings, Credits top+scroll bottom, pause, empty crafting, objective HUD. No material truncation/overlap or missing glyph in menu panels; navigation/close usable. Credits scroll works. |
| en-US0.75 | Same full set; small typography but no missing glyph/overlap, Close/Cancel/Save Main/Play functional. |
| en-US1.75 | Main/pause/Settings usable. Crafting bottom initially offscreen but scroll reaches Craft/Close. Credits wraps and scrolls to bottom, Close fixed/reachable. Worlds FAIL: Create at far right visibly clipped (“Cre…”), seed edit field shows only prefix of value. Return/Play remain usable. Objective HUD progress counter at bottom appears vertically clipped and is retained for comparison with Chinese. |

Repro defect:1280x720, English, setUIscale1.75 Apply, Save and Main Menu, Single Player. Fixed Worlds row overflows panel right edge; Create is visibly cut. Screenshot tool “en-US 1.75 Worlds 布局记录”. No file/config changes used to produce it.

## Final per-combination matrix (supersedes preliminary broad wording)
All six requested combinations were selected through Settings Apply and inspected at1280x720. Each included Main, Worlds, Settings, Credits, pause, empty crafting plus objective HUD/pause objective text. “PASS” below only covers that bounded visual/navigation subitem, not entire AI-07.

| Locale/scale | Main / Settings / Credits / pause / empty craft | Worlds creation form | Existing world row | Objective HUD progress digits |
|---|---|---|---|---|
| en-US0.75 | PASS; small readable text, working return/close | PASS observed | PASS | Readable small digits; no confirmed clipping |
| en-US1.00 | PASS; Credits bottom reached with scroll | PASS observed | PASS | UNCONFIRMED on earlier screenshot, not inferred from Chinese |
| en-US1.75 | PASS; crafting and Credits scroll reach bottom/Close | FAIL: rightmost Create clipped; seed prefix only | PASS; Seed528334149, Play/Delete fully visible | FAIL; digits vertically clipped/nearly invisible |
| zh-CN0.75 | PASS; full Credits content fits, controls reachable | PASS observed | PASS | PASS observed;0/11 distinguishable |
| zh-CN1.00 | PASS; Credits top/bottom reachable | PASS observed | PASS | FAIL;0/11 lower part clipped in blue progress strip |
| zh-CN1.75 | PASS; all inspected Chinese glyphs rendered; large panels scroll | Create visible (shorter label), but seed field only prefix | PASS; seed528334149 and进入/删除 complete | FAIL; digits vertically clipped/nearly invisible |

No separate objective window was opened; objective HUD and pause objective text are the bounded inspected surfaces. Lower Settings controls not exhaustively inspected at every scale; visible Settings region and bottom Apply/Cancel were checked. This limitation prevents any claim that every setting row or every game panel was visually certified. HUD debug metrics retain technical English in Chinese locale (Release/Stream/Mesh dirty), recorded as visible; not interpreted as missing Chinese glyphs.

## Concrete defect evidence / screenshot references
1. English Worlds creation row overflow, en-US1.75,1280x720: Settings->scale1.75->Apply->Save and Main Menu->Single Player. Real inline tool screenshot titled “en-US 1.75 Worlds 布局记录” shows Create cut at right panel edge, only “Cre…” visible; new seed field displays prefix84482… instead of full844825479. Existing row Seed528334149/Normal/Play/Delete all fit. Chinese1.75 screenshot “zh-CN 1.75 Worlds 创建行与现有世界行记录” has complete创建 button but same narrow seed prefix. No forced config used.
2. HUD progress label vertical clipping: screenshots “en-US 1.75 暂停与目标布局”, “zh-CN 1.75 暂停与目标文本记录” and “zh-CN1.00应用后暂停菜单记录”. After normal Apply, left Journey/旅程 pane's bottom blue progress strip has insufficient visible height for0/11; at1.75 digits almost disappear. Central pause objective description wraps/fits separately. Chinese0.75 “zh-CN0.75暂停与目标HUD记录” shows distinguishable0/11, retained as scale comparison. Earlier en-US1.00 HUD judgement is unconfirmed; preliminary broad menu success does not certify it.

## Retained timing / exceptions / scope limits
Original sequence: en1.00 -> en0.75 -> en1.75 -> zh1.75 -> zh1.00 -> zh0.75. Work started05:35:08Z, completed normal Quit06:17:22Z (14:17:22local). Individual screenshots are chronologically ordered inline CUA records, with requested locale/scale/panel in tool titles; no image-file hashes available.
One load+Escape batch before switching to Chinese did not produce paused menu; subsequent expected Settings clicks landed in gameplay with no visible world edit. Screenshot exposed that state, then a separate Escape after load succeeded and Settings was opened normally. This retry is retained; not called a game failure because initial Escape was submitted adjacent to loading. Later loads separated observation and Escape. A few CUA calls took33–36s despite timeout_ms30000, but returned without abort; no duplicate launch or external app used. No unexpected exit.
A read-only final log cross-check (ai07-ogre.log) reports GL_VENDOR Apple, GL_RENDERER Apple M1 Pro, GL_VERSION4.1.0.0, and normal OGRE Shutdown at14:17:22. It includes several focused0/1 transitions around14:08 although this agent never initiated app switch/minimize; cause not established and not claimed as successful focus acceptance. GUI returned to ordinary input subsequently.
Final settings saved via UI: localezh-CN, UIscale0.75, window1280x720. ai07-settings.txt copied after shutdown; no configuration file editing performed. Prior worlds/saves not edited by tools.
Only static/menu transition samples and a stationary coast scene were observed. Full day/night, terrain variety, combat dynamics, item-rich crafting, victory/defeat/recovery screens, movement and true audio are NOT_RUN. Screenshot persistence/hash requirement is BLOCKED by documented CUA capability; visual evidence is genuine inline records only. repository_accessible=true/context_isolation=PARTIAL remains true, so no AI-06 blind-play certification.
