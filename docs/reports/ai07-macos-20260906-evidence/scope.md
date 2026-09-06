# 2026-09-06 AI-07 evidence scope

Status: IN PROGRESS. This run adds attributable persisted images; it does not
redefine full AI-07 around UI-only coverage.

Authoritative current contract:
`docs/current/ai-assisted-gameplay-acceptance-v1.md`, section4.2 and AI-07 row.
The fixed visual matrix is enumerated in the historical
`docs/reports/visual-release-candidate-report-2026-08-28.md`, section
“最终视觉矩阵”. That report is scope context, never a current macOS execution.

| Requirement | Current run / remaining proof |
| --- | --- |
| Original-size UI, bilingual, three scales | Current run: en-US/zh-CN ×0.75/1.00/1.75, Worlds and pause/HUD plus representative Main/Settings/Credits/empty crafting; persist raw CUA images |
| Acquisition metadata and immutable identity | Each capture records sequence/time, locale/scale, window/render config and package/binary identity; file hashes after unchanged-byte export |
| V10A contact/AO, leaves/seams; V10B foreground and five ecologies | Only visible spawn scene may be observed here; fixed-scene breadth is not implied |
| V10C day/night/coast/cloud boundaries and motion | Full matrix still missing; static UI images cannot close it |
| V10D Off/Medium/High × noon/dusk | Not covered by the six UI combinations |
| V10E luminance steps, post on/off day/night,1024x768 settings | Not covered by1280x720 UI checks |
| Stage11 attack/pose/outline/particles/state transitions | Requires ordered actual gameplay frames and normal input; stationary/menu evidence is insufficient |
| Audible cues, captions, pause/mute lifecycle | Real backend and accessible listening/recording still required; dummy or captions alone never PASS |
| Package-only blind AI-06 | Separate scenario; current examiner repository access remains PARTIAL, not blind isolation |

Normal runtime and save compatibility retain their code-revision-specific
engineering evidence. No fresh build/soak is claimed for this image-only run.
A unique app bundle identifier separates this QA package from the existing
user app; executable/resource content remains identified by the frozen inventory.
