# Normal input operation record

Executor: /root/terrain_normal_acceptance, independent from implementer /root. All UI actions below used public mcp__cua_repl App methods. No gameplay API, keyboard event synthesis, fixture, teleport or save edit.

1. `cua.getApp(absolute T1-v5 app)` launched normal menu. First call took 306.34 seconds, eventually succeeded. AX exposed only native window, not custom menu controls. Used screenshots and coordinate clicks.
2. Click Single Player; fill name T1 Normal 20260807 and seed 20260807; Create then Play. Normal difficulty. Forest terraces visible, HP20.
3. `pressKey('w')` three times then `pressKey('space')`. Scene changed; enemy attack caption; HP10 then HP2. Escape paused. This single displacement was not reproducible as a controlled route.
4. Read Settings normally: 1280x720, windowed, render distance8, directional shadows Off, post-processing unchecked, FOV90, mouse sensitivity0.050, Hold sprint/sneak, Full feedback, English, UI1.00x, hints/captions enabled. Cancel.
5. Selected Casual in pause difficulty dropdown; Apply then Save and Main Menu. World listing remained Normal and said difficulty queued for next simulation tick. Treat change as not applied, not a Casual PASS.
6. Created separate T1 Casual 20260807 seed20260807, Casual. Name replacement via super+a and ctrl+a appended text on retries; triple click correctly selected full field. No duplicate world created from erroneous name.
7. Play then Escape: HP20, forest slope. Unpause; W8, Space, W8; screenshot; pause. No definite near-terrain translation.
8. Unpause; `pressKey('w+space')` failed with `Computer Use server error -10005: keyPressIncludedMultipleNonModifierKeys(ComputerUse.XKeysymString.KeyPress(keys: [ComputerUse.XKeysym.w, ComputerUse.XKeysym.space]))`. Remaining actions in that call did not run.
9. D25 taps; screenshot; Escape. No near-terrain translation; HP10. Public API is pressKey(key:string), drag(from:Vec2,to:Vec2); no duration/hold/down/up documented.
10. Unpause; `drag([640,388],[640,388])`; screenshot showed Combat hit cue, HP8 and still empty inventory; Escape. This was a normal primary mouse action but did not obtain a resource.
11. Capture t1-06; click Save and Main Menu; Single Player; Play T1 Casual; immediately Escape. Same scene and HP8. Capture t1-07. Save and Quit. Subsequent getAXState returned App quit, expected successful exit.
12. Launch T0-v4 via authorized macos_window_evidence.py --launch. getApp then screenshot. Existing T0 Baseline world left untouched. Create T0 Casual 20260807 using triple-click fields, seed20260807, Casual. Play and pause after observing shore.
13. Unpause; W16, Space, D25 taps; screenshot; pause. Near shore unchanged; distant land became visible as chunks loaded. Capture t0-04.
14. Unpause; S16, A25 taps; screenshot; pause. Near shore unchanged. No confirmed return route.
15. Save and Main Menu; Single Player; Play T0 Casual; Escape. Same near shore/HP20. Capture t0-05. Save and Quit; expected App quit result.
16. Read own world.meta fields as supporting evidence: Casual positions equal their spawn, inventories empty. A broad metadata enumeration also printed the pre-existing T0 Baseline metadata; it was not used as evidence, opened in game, or modified.

Capture failures preserved: t1-01/02 old helper in sandbox could not match CUA-launched executable; parent fixed exact executable-path matching. t1-03/04 still failed in sandbox. Elevated exact-window capture t1-05 onward succeeded. No screenshot of whole desktop and no edited screenshots. First four failures do not constitute menu/game failures. Each successful capture JSON stores timestamp, original PNG hash, package hash, dimensions and window identity.
