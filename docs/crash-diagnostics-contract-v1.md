# Local Crash Diagnostics Contract v1

H1 establishes a local-only Windows minidump boundary. The implementation is
present and the portable policy is verified as of 2026-08-17; H1 remains
`Doing` until the controlled Release crash harness passes on the target Windows
machine.

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

`HelloMine3DCrashDiagnosticsSmoke` freezes 12 renderer-independent assertions:

- stable defaults and trigger names;
- exact trigger parsing and invalid-value rejection;
- working-directory semantics for relative overrides;
- equal, parent and child save/crash overlap rejection;
- non-empty project-root enforcement;
- platform install identity and zero dumps during an ordinary smoke exit.

The target-Windows Release harness is:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\validate_crash_diagnostics.ps1 `
  -ExePath bin\HelloMine3D.exe
```

It runs ordinary validation-only and three-frame real-window cases with zero
dumps, then runs the controlled first-frame crash in isolated directories. It
requires a non-zero exit, exactly one non-empty `.dmp`, the pre-crash active
world publication marker, a successful validation-only reopen of the same
world, no pending save candidates and no additional dump on that reopen. The
full Windows build gate invokes this harness only after the Release rebuild.

Current portable evidence is the complete macOS Debug/Release gate at
`build/xcode-validation-20260817075257`: 31-project graph validation, 13 test
targets per configuration, both 12/0 H1 runs, validation-only startup and real
Cocoa window startup. The Windows minidump size, exit code and post-crash world
reopen evidence are intentionally still missing; no macOS result is treated as
a substitute.

H2 sidecars/symbolization and H3 next-start/package UX remain out of scope until
the H1 Windows harness closes this platform evidence gap.
