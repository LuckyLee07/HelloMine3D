# Layout candidate — independent normal-window retest

Status: FINISHED. Targeted creation-form and Journey HUD visual checks PASS for six combinations. Additional existing-world seed-column check FAIL (en-US 1.75). This is not an overall AI-07, AI-01, or AI-06 PASS.

Initial prompt: parent /root NEW_TASK beginning “新布局候选完整Xcode Debug/Release门禁PASS，GUI交给你独占”, specifying layout-candidate.zip, six combinations, real name/seed/nondefault-difficulty creation, and normal exit. Parent later requested preserving the newly found seed-column defect while continuing this unchanged package. Build-gate claims were not used as GUI evidence.

Identity and boundaries:

- ZIP: /private/tmp/hm3d-independent-ai01-6916867/layout-candidate.zip; SHA256 b2bb8ff42086c439a3648367abee92c305357e75206b1938fae7a48ab4989929.
- Exact app selected: /private/tmp/hm3d-independent-ai01-6916867/layout-retest/HelloMine3D-Layout-20260905.app. Separately extracted; original packages/reports retained. ZIP executable permissions restored.
- Actual Mach-O Contents/Resources/bin/HelloMine3D SHA256 4bece0fc19a03a8268bfd3808d237a15244f3a0184646691aceb9568eb9e25f2. Launcher is distinct from Mach-O. Both package and binary hashes independently checked, including after exit. A clean code-commit identifier was not supplied for this candidate; package/binary hashes identify this run.
- Platform: macOS 15.7.3 (24G419), arm64; Apple M1 Pro, OpenGL 4.1 (release-log platform observations). Normal window: 1280x720 client, genuine inline screenshots 1280x748 including title bar. Render distance 8, directional shadows off, post-processing off, FOV90, mouse sensitivity0.05, windowed. Initial en-US/1.00, final zh-CN/1.75; all changes through Settings/Apply.
- Shell default workspace was /Users/lizi/Desktop/Workspace/HelloMine3D; record/copy/hash commands explicitly used working_directory /private/tmp/hm3d-independent-ai01-6916867. App launcher runs from Contents/Resources/bin. No repository/source/test reading performed.
- repository_accessible=true; context_isolation=PARTIAL. Tool permission root allows read :root, including repository; this is a behavioral read restriction, not a technical sandbox preventing repository access. Writable roots include workspace, /private/tmp, configured temporary directory and visualization directory. Operational reads limited to release packages and this acceptance directory.
- No config/save editing, fixture, gameplay API, teleport, held-key movement, external app, minimize, or application switching used. All world changes came from normal UI and game simulation.

Run timing and evidence:

- Date 2026-09-05, Asia/Shanghai. Release log starts14:23:25 and normal shutdown14:36:00. Report/copies collected14:36–14:38. Exact per-click wall timestamps are not exposed in this report; original ordered CUA tool transcript supplies step order and call durations. Do not interpret aggregate time as frame performance.
- Screenshots are actual inline mcp__cua_repl.js getAXStateAndScreenshot outputs. No documented screenshot-file save/hash API was available; no fabricated PNG paths or screenshot hashes are supplied. References below are tool-call titles in this agent's transcript.

| Locale / scale | Creation form: labels, fields, full current ten-digit seed, Create fully visible | Journey progress 0/11 and neighboring HUD | Existing world row |
| --- | --- | --- | --- |
| en-US / 1.00 | PASS; actual Create produced Layout Base | PASS; digits fully visible | Name, ten-digit seed, Normal, Play/Delete readable |
| en-US / 1.75 | PASS; actual Layout Check, 1234567890, Casual, Create exercised | PASS; digits fully visible, taller bar fits | FAIL: ten-digit seed trailing character clipped/tight against difficulty; Play/Delete remain complete |
| en-US / 0.75 | PASS | PASS | Full Layout Check /1234567890/Casual readable |
| zh-CN / 0.75 | PASS | PASS | Full name/seed/休闲 readable |
| zh-CN / 1.00 | PASS | PASS; original numeric clipping not reproduced | Full seed and controls readable |
| zh-CN / 1.75 | PASS | PASS; wrapped Chinese objective, 0/11 fully visible | Ten digits readable, little gap before difficulty; no independent Chinese clipping assertion |

Create execution distinction: actual Create clicks were performed in en1.00 and en1.75. In the other four combinations, its full label/bounds and unobstructed position were visually checked, not clicked to create four redundant worlds. Thus actual creation behavior was not independently exercised six times.

Ordered observations:

1. Exact-path normal launch returned in about10.44s. Initial en1.00 creation form placed labels above controls; name/seed and difficulty/Create occupy two rows. Default seed1213457900 fully visible. Cmd+A replaced name with Layout Base; Create succeeded. Play loaded forest; screenshot confirmed world before separate Escape. Journey0/11 now fully legible. Health changed20→8 during the normal-world wait; no combat conclusion or workaround. Save/Main retained world.
2. Through Settings set1.75, verified value then Apply. Paused en1.75 Journey0/11 complete. Worlds creation form now fully displays Create and seed field. Tool screenshot “布局复测 en-US1.75 修复后创建表单” also exposed existing Layout Base seed1213457900 clipped at the following Normal column. This additional defect was sent promptly to parent; current package was not replaced.
3. en1.75 actual field operations: Cmd+A then Layout Check; seed Cmd+A then1234567890; dropdown visibly offered Casual/Normal/Challenging; selected Casual (nondefault) and captured correct values before Create. Create produced Layout Check row. Entered its Play, waited for actual coastal world screenshot, then separately Escape. Screenshot “载入后暂停核对非默认Casual难度” clearly shows Difficulty: Casual. No save metadata used to infer this UI result.
4. Set en0.75 via Settings/Apply. “布局复测 en-US0.75 Journey数字” shows full0/11; “布局复测 en-US0.75 创建表单与完整世界种子” shows full form and exact Layout Check / Seed1234567890 / Casual row. This smaller-scale row confirms the created seed visually despite en1.75 clipping.
5. Play Layout Check, capture rendered world then Escape. Settings language dropdown selected Simplified Chinese, Apply at0.75. “布局复测 zh-CN0.75 Journey数字” and “布局复测 zh-CN0.75 创建表单” show complete digits/labels/input/Create, plus correct existing row.
6. Reload normally, pause; set1.00, screenshot value then Apply. “布局复测 zh-CN1.00 Journey数字” shows full0/11, eliminating the specific prior zh1.00 vertical crop in this package. “布局复测 zh-CN1.00 创建表单” shows complete labels and ten-digit field value1452089535, with Create unobstructed.
7. Reload normally, pause; set1.75, screenshot value then Apply. “布局复测 zh-CN1.75 Journey数字” shows objective wrap and complete0/11, separate from FPS panel and health/hotbar. “布局复测 zh-CN1.75 创建表单与十位世界种子” shows full field1452089535 and 创建. Existing rows1234567890/1213457900 are readable but closely abut their difficulty columns.
8. Return main, select 退出. Tool “布局复测正常退出” returned “Computer Use server error -10005: App quit”; expected quit outcome corroborated by OGRE shutdown14:36:00. No operation remains pending. GUI returned to parent before evidence-copy work.

Defect reproduction retained: 1280x720, en-US, UI scale1.75; create/use a world with a ten-digit seed, open Worlds; compare creation input (full seed) with existing row (trailing digit clipped at difficulty). Layout Base1213457900 and Layout Check1234567890 both exposed it. The two targeted fixes passed; this third issue is separate and remains FAIL for this binary. No failed-click retries were needed in the remaining combinations. Occasional CUA latency was not treated as game failure; no unresolved tool call at exit.

After-GUI read-only corroboration:

- layout-settings.txt: final locale zh-CN, uiscale1.75, fullscreen0, windowsize1280 720. SHA256 ca94506a47815465a578cd439945e90bd0d9e80fe9c7305db1ace3fe36558d80.
- layout-ogre.log: normal shutdown14:36:00. SHA256 89cc3c5484fa9f371fdb1306e3319f8ec231e5168f8be25bbd6bce1d8a5bfff8.
- layout-world-3c68a9c48263680f49ed736edbadd487.meta: world_name Layout Check, seed1234567890, difficulty_id0; UI independently showed Casual/休闲. Health20 on final save.
- layout-world-9e31230f102f8c7cd05fb496dbe6edac.meta: Layout Base, seed1213457900, difficulty_id1; UI showed Normal. Health8 corroborates observed decrease.

Scope limits: unchanged panels were not systematically rerun. Dynamic day/night/terrain/combat, real audio, movement/held keys, focus/minimize, broad AI-07 and AI-06 are NOT_RUN in this bounded retest. No whole-suite PASS is asserted.
