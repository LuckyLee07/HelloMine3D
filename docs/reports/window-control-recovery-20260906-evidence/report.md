# Independent window control recovery — 2026-09-06

Status: window control recovery PASS in a new independent context. Actual
main-menu→Worlds→main-menu round-trip and screenshots succeeded at04:48:37.655UTC.
This is not complete AI-07 or AI-06 acceptance. Image export subsequently completed through normal Finder launch and TextEdit
save. All three original JPEG files passed byte-count/CRC32/SHA256 checks and
were visually opened by the parent.

The owner explicitly confirmed no concurrent window operation and asked for
recovery. No occupancy confirmation or project permission request is needed.

## Observations

- ChatGPT desktop26.901.51231 (build8109), bundlecom.openai.codex.
- Old independent-context calls repeatedly coincided with desktop log errors
  `Received automatic approval review for unknown conversation`, targeting
  `mcp_elicitation:cua_repl`. These are routing errors, not explicit permission
  denials. The selected raw entries are in routing-observations.json.
- At04:40:26UTC, the managed computer-use service reported replacing its stale
  process because it exited, and made a respawned service available. Parent
  did not kill the service or change approval/security configuration.
- Started the same QA app through the already verified normal desktop open
  command. Package/binary identities remain8df716f5/c0446b47.
- New independent agent fresh_window_recovery used no inherited conversation
  history or repository source. getState returned in0.4831s and found the app.
- Its single getApp began04:44:31.419UTC and returned04:47:32.329UTC:
  internal180910ms, tool-reported187.048s. Requested60000ms timeout was not a
  hard deadline. HelloMine3D window AX returned; this alone is not an input PASS.

The new context and service replacement occurred together, so evidence does
not distinguish which change enabled binding. We do not claim a repaired
Codex implementation or definitive root cause. No plugin reinstall, blanket
permission expansion, OS security change or private tool call was performed.

## References and next verification

Official [troubleshooting](https://learn.chatgpt.com/docs/reference/troubleshooting)
recommends a new focused chat for stuck states and identifies the desktop log
location. Official [Computer Use](https://learn.chatgpt.com/docs/computer-use)
distinguishes app approval from Screen Recording/Accessibility permissions.
Neither source proves this specific routing defect's cause.

Independent menu verification is retained in independent-report.md. No world
was created/entered and no setting changed. Reuse this working context/handle
for subsequent bounded UI acceptance. Initially the three captured images
remained cached: TextEdit binding did not return and was interrupted after325.9s
before paste/save. The changed launch path below subsequently completed export.
Full gameplay/AI-06 isolation and real-audio gaps remain separate; missing
historical images stay missing.

## Normal Finder launch follow-up

Parent reused its existing Finder CUA handle: getAXState→super+shift+g→
setValue /System/Applications/TextEdit.app→Return. Fresh AX showed TextEdit
selected. super+o opened the selected app; read-only process inspection confirmed
TextEdit PID61991. No exec/AppleScript application launch or private UI API was
used for this editor. Independent examiner then bound the already running
editor and continued the unchanged-byte export. This was a changed launch path,
not another automatic-launch retry.

The already running editor bound in33.6803s. Independent examiner pasted the
previously encoded three captures into a new plain-text TextEdit document and
normally saved window-recovery-captures.json.txt outside the repository. Strict
base64 decode produced unchanged JPEG bytes (60040/82014/63711 bytes), with all
recorded CRC32 values matching. Original images and captures.metadata.json are
retained here. Parent opened all three and confirmed main-menu/Worlds/main-menu
content. The saved editor document was normally closed; both handles retained.
The preceding failed automatic editor binding remains part of the chronology.

New independent six-combination en/zh UI-scale validation is now delegated to
the same working context. These three recovery images do not cover that matrix.
