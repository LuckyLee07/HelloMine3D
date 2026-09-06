# Independent window control recovery — 2026-09-06

Status: window control recovery PASS in a new independent context. Actual
main-menu→Worlds→main-menu round-trip and screenshots succeeded at04:48:37.655UTC.
This is not complete AI-07 or AI-06 acceptance. Image export is incomplete: TextEdit binding was interrupted after325.9s.

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
for subsequent bounded UI acceptance; the three captured images
remain cached. A separate TextEdit binding did not return and was interrupted
after325.9s, before paste/save. This does not invalidate the completed game
menu round-trip, but no persisted screenshot artifact is claimed. Do not reset
the working CUA session or repeat the same editor binding without a changed path. Full gameplay/AI-06 isolation
and real-audio gaps remain separate; missing historical images stay missing.
