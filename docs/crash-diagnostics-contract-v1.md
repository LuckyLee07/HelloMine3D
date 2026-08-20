# Local Crash Diagnostics Contract v1

H1 establishes a local-only Windows minidump boundary. The implementation is
present and verified on target Windows as of 2026-08-17; the controlled Release
crash, post-crash save reopen and no-upload boundary all pass, so H1 is `Done`.
H2 的脱敏 sidecar、混合栈离线符号化和独立符号归档，以及 H3 的下次启动本地提示，
均已在 2026-08-20 的 Release Candidate 验证中闭环；详细合同见
`docs/crash-sidecar-contract-v1.md`。

## Backend audit and selection

Google Breakpad was evaluated before integration. Its official Windows guide
requires building or vendoring `exception_handler.lib`, and its handler also
covers invalid-parameter and pure-virtual-call failures plus optional
out-of-process generation. Those features are useful for a broader crash
service, but this repository has no Breakpad dependency or crash server and H1
requires one minimal Windows-only local dump path. The client already links the
Windows SDK `DbgHelp` import library.

The selected H1 backend is therefore the Windows SDK `MiniDumpWriteDump` path:

```text
backend=windows-dbghelp
windows_sdk=10.0.22621.0
breakpad_revision=not-selected
breakpad_license=not-applicable
upload=disabled
```

No Breakpad source, binary or notice is packaged. This is an explicit audit
decision, not an unpinned dependency. If a later milestone needs cross-platform
or out-of-process collection, it must reopen the decision and pin the selected
Breakpad/Crashpad revision and license before changing this contract.

The implementation follows Microsoft's in-process fallback guidance by
creating a dedicated dump-writer thread during startup. The unhandled-exception
filter only publishes the exception pointer, signals that thread and waits at
most 15 seconds. An interlocked one-shot guard bounds recursive handler entry,
and `CREATE_NEW` prevents overwrite or a second dump for the same process.

Primary references:

- [Breakpad Windows integration](https://github.com/google/breakpad/blob/main/docs/windows_client_integration.md)
- [Breakpad Windows handler API](https://github.com/google/breakpad/blob/main/src/client/windows/handler/exception_handler.h)
- [Microsoft MiniDumpWriteDump](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/nf-minidumpapiset-minidumpwritedump)
- [Microsoft SetUnhandledExceptionFilter](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setunhandledexceptionfilter)

## Startup and storage boundary

`OgreMain.cpp` resolves and installs crash diagnostics before calling
`runOgreBootstrap`, so no Ogre object exists when the Windows filter is
installed. Platform exception types are confined to
`Diagnostics/WindowsCrashDiagnostics.cpp`; the public configuration and trigger
API contains only portable C++ types.

The default dump directory is `bin/crashes`, while the default active world is
`bin/saves/default`. `HELLOMINE3D_CRASH_DIR` may select an isolated harness
directory. Both defaults and overrides are normalized before installation, and
startup rejects either directory being equal to, below, or above the other.
The package probes place saves and crashes in separate sibling trees outside
the copied distribution.

Installation may create the empty crash directory on Windows. Ordinary exit,
validation-only startup, caught startup errors and a real-window exit must not
create a `.dmp`. The backend has no network or upload path.

## Controlled crash contract

The only accepted opt-in value is:

```powershell
$env:HELLOMINE3D_CONTROLLED_CRASH = "after-first-frame"
```

Unknown values fail startup instead of silently enabling a crash. At the first
completed rendered frame the client finishes startup/world-entry timing,
publishes the active world through the real transactional save and backup path,
flushes the diagnostic marker, then raises the private non-continuable Windows
exception. The ordinary game path does none of this when the variable is
absent. Non-Windows clients reject a requested controlled crash because they do
not install a compatible dump backend.

## Verification

`HelloMine3DCrashDiagnosticsSmoke` freezes 21 renderer-independent assertions:

- stable defaults and trigger names;
- exact trigger parsing and invalid-value rejection;
- working-directory semantics for relative overrides;
- equal, parent and child save/crash overlap rejection;
- non-empty project-root enforcement;
- platform install identity and zero dumps during an ordinary smoke exit;
- sidecar roundtrip, path sanitization, unknown-field rejection and mismatched
  build-identity rejection;
- pending/invalid/orphan/acknowledged report discovery, bounded ordering,
  sanitized clipboard text and persistent acknowledgement.

The target-Windows Release harness is:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\validate_crash_diagnostics.ps1 `
  -ExePath bin\HelloMine3D.exe
```

It runs ordinary validation-only and three-frame hidden-window cases with zero
dumps, then runs the controlled first-frame crash in isolated directories. It
requires a non-zero exit, exactly one non-empty `.dmp` and one sanitized
`.crash.txt`, the pre-crash active-world publication marker, a successful
validation-only reopen of the same world, no pending save candidates and no
additional dump on that reopen. It also reads the actual minidump
module/thread/memory/exception streams, requires a bounded hybrid stack with at
least one current-project frame, rejects a wrong PDB, archives matching symbols
outside the package, then starts the client again and requires exactly one
local pending-report prompt. The full Windows build gate invokes this harness
only after the Release rebuild.

Current Windows evidence is a 124,513-byte Release dump with a non-zero
controlled exit, successful save reopen, no pending candidate and no upload.
The matching EXE/PDB resolves a bounded project stack including
`CrashDiagnosticsPlatform::triggerControlledCrash`; when native `StackWalk64`
cannot advance beyond the saved system context, the tool labels and uses its
bounded saved-stack scan instead of claiming a native unwind. A wrong PDB
returns exit code 3 and `symbol-identity-mismatch`. The separate seven-entry
symbol ZIP hashes to
`1e614f64e7ac2f8be55d39cf26aa817a14bb1e30fdc9237644cc044df01b2ca3`.
Debug/Release both pass 21/21 portable assertions. H2 and H3 are `Done`; upload
remains hard-disabled and R3 human input remains a separate release gate.
