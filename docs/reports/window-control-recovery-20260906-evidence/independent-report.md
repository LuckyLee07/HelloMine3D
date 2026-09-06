# Fresh-context window control recovery

Result: independent-context CUA window control recovered. This is not a complete AI07 or blind-play PASS.

Target: `local.hellomine3d.ai07.20260906`, already running according to the initial `cua.getState()` (tool wall time 0.4831 seconds). No repository source or internal tutorial was read.

Exactly one `cua.getApp(bundleID)` call was made. Its recorded internal UTC start was 2026-09-06T04:44:31.419Z, and return was 2026-09-06T04:47:32.329Z: 180910 ms elapsed (tool wall time 187.0480 seconds). The requested 60000 ms tool timeout was not a hard cutoff. Binding returned the HelloMine3D standard window AX tree.

The initial screenshot showed the main menu. One normal coordinate click at [640,311] opened Single Player. After the required AX refresh, a screenshot visibly confirmed Worlds, including an existing InputProbe20260906 world. One normal coordinate click at [291,120] selected Back to Main Menu. After AX refresh, the final screenshot visibly confirmed the main menu again. Completed at 2026-09-06T04:48:37.655Z.

Only those two clicks were sent. No keyboard input, world creation, world launch, deletion, settings change, or game exit was performed. The game remains running at its main menu. AX exposed only the window chrome, so game controls were located from fresh screenshots. No ComputerUseInactive error occurred.

The persistent CUA session retains `game` and three Uint8Array screenshots: `mainScreenshot` (60040 bytes), `worldsScreenshot` (82014 bytes), and `returnedMainScreenshot` (63711 bytes), for later export. UI actions and observations used only the documented public CUA API.

## Evidence export status

Export remains incomplete. Pure JavaScript base64 encoding and CRC32 computation succeeded for all three original Uint8Array values, producing persistent `captureRecords` and `captureJSON` variables without printing base64 to model output. CRC32 values are main `80e2f0e5`, worlds `b9f891f4`, and returned-main `61b3a2b2`.

The single subsequent call `var editor = await cua.getApp('com.apple.TextEdit');` with timeout_ms 60000 was interrupted by the parent. The tool returned `aborted by user after 325.9s`, without an AX result or confirmed editor binding. No paste or save action was performed, so this export attempt has not saved a capture file. No filesystem decoding, image export, or SHA256 validation has occurred.

The JavaScript session was not reset. The game handle, screenshot arrays, and encoded records were present immediately before the interrupted editor call; their state after interruption has not been rechecked. No further GUI input or binding attempts were made. The successfully verified game-menu round trip remains valid independently of this unfinished auxiliary export.
