# TODOLIST Goal completion audit — 2026-09-05

Status: **Not complete**. Engineering evidence and formal gameplay acceptance
are assessed separately. This audit does not change the frozen scope.

Baseline commits: `ea7a85d9d8ec0dcec524f8cad436ddb363e34cbd` (runtime fixes)
and `69168670698ab67e7d345bf5e28cd9eeb099c80e` (verified Xcode configuration).
Scope origin: `81d945dcf7a4bdbd6d98a1652b2776fca37665fa`.
Full commands, failures and artifact paths are in the
[execution record](todolist-goal-execution-2026-09-05.md).

| Owner requirement | Current proof / gap | Assessment |
| --- | --- | --- |
| Freeze starting TODO, commit, workspace, dependencies and exits | Starting TODO copy/hash and work ledger recorded before implementation; no applicable AGENTS.md found | Proven |
| Complete remaining approved implementation without replaying finished batches | Existing completed batches retained; macOS menu cursor and shortcut defects repaired; catalogue path-identity regression repaired; native focus engineering regression passes; independent restore/switching remains tool-blocked | Input repairs pass focused regression; all three layout repairs pass Xcode gates and bounded independent visual follow-up; formal gameplay remains open |
| Investigate real D2 Simulation Activation entry, implement only if justified | Debug and repeated Release real Furnace/Crusher workload probes; bounded D1 delay, no unacceptable activation-specific boundary established | Evidence supports retaining Candidate; no D2 implementation or Done claim |
| Stay within initial scope | B7-B9, C4-C11, D3-D8 and Extended remain excluded | No expansion |
| Preserve main gameplay and save compatibility | World 991, recipe 126, storage 16, backup 19, catalogue 59 and other suites pass in latest layout Xcode Debug/Release gate 20260905143633; earlier gmake and ordinary Debug save/menu/reload observations retain their recorded revisions | Automated compatibility proven within those suites; full normal mainline-to-victory unverified |
| Execute necessary platform automation | Earlier gmake Debug/Release 13 suites and startup negatives 15 pass at their recorded revisions; latest layout code passes Xcode Debug/Release 13 suites and probes; input repair has its recorded focused regressions | Engineering gates passed; Windows explicitly postponed |
| Isolated distribution | Current separate QA ZIP8df716f5 at source31023c6 retains runtime0e0c569/c0446b47 and passes106inventory/107exact files; desktop script/open startup reached resource initialization on2026-09-06; sandbox GL failure and prior first-layout AppleDouble failure retained | Package integrity and desktop renderer initialization proven; current independent window binding pending; not proof of blind executor isolation |
| Performance / pressure appropriate to changes | D2 diagnostics and nominal/stress 120 seconds each passed at the initial ea7a85d source snapshot; latest layout gate reran comparison-contract checks, not those soak profiles | Short regression only; no new formal six-scene Q1 or 1800-second Q3 claim |
| AI-01..AI-05 and AI-08 | Implementer observed menu, create/play/save/reopen, crafting and pause input in actual windows; independent AI-01 report recorded; movement/look/held-key checks blocked by tool capability; remaining scenarios not run | AI-01 BLOCKED overall; other formal scenarios NOT_RUN; supplemental observations cannot close them |
| AI-06 package-only blind 30 minutes | Current executor read source; available task filesystem permits repository reads; no package-only executor provisioned | BLOCKED by isolation; no blind or AI-understandability PASS |
| AI-07 visual/localization/dynamic/audio matrix | Six language/scale combinations passed form/HUD repair follow-up; ten-digit seed column passed en/zh1.75 and zh0.75 stable-frame follow-up with actual Play; full matrix image coverage remains missing; raw CUA screenshot export through a normal editor is now proven; macOS audio/music factories select dummy backend | Bounded repairs PASS; overall AI-07 BLOCKED, remaining visual matrix NOT_RUN; real audio BLOCKED, captions/logs cannot substitute |
| Documentation matches code/evidence | Current docs linked to execution/investigation; 98 local links verified; original historical reports unchanged | Current results recorded with limits; layout repair acceptance is bounded and does not close formal scenarios |
| Retain existing edits and bundle | No pre-existing tracked edits at start; no task action deleted bundle; subsequent search could not find original file | Current bundle location unverified; no claim that it is still present |
| Local submissions only | Batches ea7a85d,6916867,f0e63d2,fb8f275,ffe0684,0e0c569 and31023c6 are local; new2026-09-06 startup evidence is being recorded; no push, release or tag | Proven for this task's actions |
| Mark Goal complete only after all required acceptance passes | Independent normal-play, package-only blind and real-audio evidence absent | Completion condition not met |

## Minimum conditions for remaining formal acceptance

1. Computer Use operations that can establish native app activation/unminimize,
   sustain/release keys and supply relative mouse motion. The independent
   acceptance context is already available and has executed real steps; remaining
   movement/look/restore coverage needs those tool capabilities. Complete the
   mainline scenarios only through normal input, without fixtures or save edits.
2. For strict AI-06, an execution environment whose readable filesystem roots
   include only the extracted package and acceptance outputs. Merely changing
   cwd or giving a fresh prompt cannot meet this requirement in this workspace.
3. For the mandatory real-audio subcase, a supported audible runtime/backend
   and accessible audio evidence. Current macOS dummy backend cannot supply it;
   do not silently enlarge this Goal into implementing a new audio platform.

Windows was postponed by the owner. These macOS acceptance gaps remain explicit
and are not converted into Windows PASS, formal AI PASS, or overall Goal Done.

Screenshot persistence is no longer a tool-capability blocker: see
[export probe](cua-screenshot-export-20260905-evidence/report.md). It proves
unchanged byte export only; formal scenario image coverage still needs execution.
