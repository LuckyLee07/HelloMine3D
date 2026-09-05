# Independent normal-window focus candidate retest
Final status: **BLOCKED**, with observed partial successes. No overall AI-01 or AI-06 PASS. Window was normally closed at local 13:30:25; GUI control returned.

## Identity / boundary
initial_prompt: parent /root NEW_TASK in /root/macos_ai01_acceptance beginning “请独立执行一个正常窗口焦点复测”, and bounded recovery instructions after Calculator tool interruption. ZIP SHA256 independently measured a51fd9bcc374b3f05de8bcef93703c3f122dd0e721dca324537d1271846b95bd. Actual Contents/Resources/bin/HelloMine3D SHA256 independently measured bf07e9b3ff9a39e1b8a0aca375e94a75ead8eade4d1239c11c0204fe8077cbcd.
Exact app: /private/tmp/hm3d-independent-ai01-6916867/focus-retest/HelloMine3D-Focus-Candidate-20260905.app. Separately extracted; executable permissions restored only; old packages preserved. Source identity not supplied separately; report tied to measured bytes.
macOS 15.7.3 (24G419), arm64; normal 1280x720 client, 1280x748 screenshot with title bar; English locale. Shell working_directory=/Users/lizi/Desktop/Workspace/HelloMine3D (default, previously pwd verified); all paths read/written absolute release/evidence paths. App launcher working_directory=its Contents/Resources/bin.
repository_accessible=true; context_isolation=PARTIAL. Configured readable root :root; repository is still technically readable. Effective OS access not exhaustively tested. Writable roots configured: project, /private/tmp, /private/var/folders/8n/qqg7p5691wq8gmblxz23zrqc0000gn/T, and supplied visualization directory /Users/lizi/.codex/visualizations/2026/09/05/01a06f1c-1365-7881-abbf-75558c89757b. No repository source/tests/internal guides read; no fixture, injected gameplay API or save editing. Calculator was explicitly authorized as a switch target but its getApp never completed; no personal app operated.

## Actual chronology / evidence
Times UTC unless noted; screenshots are actual inline CUA tool records, no local image paths fabricated.
1. 05:08:52 identity verified. getApp exact app completed in 17.952 s, normal English menu observed. Single Player -> Worlds (0 active), default seed 1808602498, Normal.
2. Click name; super+a; typeText Focus; ordinary a, x. Screenshot exactly Focusax, fully replaced New World. Base Cmd+A and subsequent ordinary input PASS.
3. cua.getApp('com.apple.calculator') was the only pending call. Tool eventually reported aborted by user after 688.7 s. Parent interrupted after prolonged no-return. No Calculator window/activation was observed, no return-from-Calculator step executed. This is tool blocking; game startup/functionality cannot be blamed for it.
4. Bounded recovery: js_reset succeeded; getState(timeout 30000) returned in 7.6162 s, listing focus candidate isRunning=true; Calculator absent. Binding running bundle ID returned ambiguity because old Cursor-Fix app shares ID. Used exact candidate path returned by tool, completed in 30.1751 s; no new launch intended/performed. Screenshot preserved Focusax and 0 worlds. No repeat Calculator launch.
5. Native AX minimize button clicked, focused-element line disappeared in subsequent full AX tree. AX Raise restored focused-element line. App-only screenshot preserved Worlds/Focusax with no accidental world creation. However AX exposes no AXMinimized value or unminimize action: only Raise/minimize/close/zoom. Therefore Raise cannot certify actual OS restoration/global activation.
6. After Raise, click name -> super+a -> typeText FocusReturn -> ordinary a,x: screenshot still Focusax, no text change. One BackSpace and click Create: text still Focusax, but Create succeeded, active world Focusax / Seed 1808602498 / Normal. Pointer action was effective; keyboard input was not.
7. After successful Create click, another click name -> super+a -> FocusRetry -> a,x still left Focusax unchanged. After parent instruction, one title-bar click followed by name -> super+a -> TitleReturn -> a,x also unchanged. These are retained failed observations, not converted into a PASS or automatically attributed to application regression.
8. Read-only log cross-check after these GUI steps showed only local 13:09:16 focused=0 frame=0; 13:09:45 focused=1 frame=2823; 13:25:04 focused=0 frame=98715. No focused=1 after native minimize, Raise or title-bar click. This corroborates incomplete focus restoration and explains why keyboard behavior cannot prove active-window acceptance. The log does not replace GUI evidence.
9. One Play click succeeded and displayed real grass/dirt hillside, health20/20, journey0/33. One Escape press did not open pause (screenshot remained gameplay). No repeated key/movement attempts. Pause/resume and resumed menu pointer coverage BLOCKED because effective game input focus was not restored.
10. Native close button clicked after fresh AX tree. Subsequent AX read reported procNotFound; requested close ended process. Copied final log after GUI work to evidence/focus-ogre.log. Local 13:30:25 OGRE Shutdown confirms orderly shutdown. Log still has no focused=1 after 13:25:04.

World created through normal menu: Focusax; seed1808602498; identity directory world-9e31230cde3c648b7c54ec7ae3a39778. No world/save contents modified by tools.

## Results
| Subitem | Result | Evidence and limits |
|---|---|---|
| Baseline Cmd+A / ordinary text | PASS | Focusax screenshot before interruption/minimize |
| External application out/back | BLOCKED | Calculator getApp aborted after688.7s; no two-app focus observation |
| Cmd+Tab | NOT_RUN / UNVERIFIED | No certified OS switch; Raise not substituted |
| Native minimize / focus loss | PASS for loss observation | AX focus disappears; log focused=0 at13:25:04 |
| Restore/global activation | BLOCKED | Raise AX focus line returns but log remains0; no public unminimize property |
| Keyboard after restore | BLOCKED, failed observed attempts retained | FocusReturn/FocusRetry/TitleReturn/BackSpace did not change text |
| Mouse menu creation / load | PASS | Create then Play succeed, actual world rendered |
| Pause/resume / pointer after resume | BLOCKED | One Escape did not open pause while log still0 |
| Held-key/stuck-key guarantee | BLOCKED | No documented hold/release-duration interface; not attempted again |
| Normal close | PASS | Native close + procNotFound + orderly OGRE Shutdown |

Visual evidence: inline screenshots named by corresponding tool titles in this task; public CUA documentation lacks filesystem screenshot-save helper. Raw focus records and shutdown are preserved in focus-ogre.log. No further scope (AI-07 etc.) performed.
