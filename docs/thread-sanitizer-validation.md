# ThreadSanitizer Validation

R4 closes the formal data-race evidence gap for the background chunk loader.
The Windows V5 stress scenario remains useful integration coverage, but it is
not presented as a substitute for compiler instrumentation.

## Supported boundary

The dedicated gate runs only on a native 64-bit macOS process. It accepts
`arm64` or `x86_64`, builds the real `HelloMine3DWorldRuntimeSmoke` target with
Apple Clang and `ENABLE_THREAD_SANITIZER=YES`, and rejects a translated or
wrong-architecture product. This matches Clang's documented Darwin arm64 and
x86_64 support.

The build setting applies to the target and all of its generated source
dependencies. The gate then verifies that the executable links
`libclang_rt.tsan_osx_dynamic.dylib` and imports representative
`__tsan_init`, `__tsan_read*` and `__tsan_write*` instrumentation symbols.
First-party `no_sanitize("thread")`, instrumentation-disable attributes,
compiler opt-outs and default suppressions are rejected before generation, so
a clean result is not obtained by hiding known reports.

Before building the game target, the gate compiles an isolated deterministic
race fixture with the same Xcode compiler. The calibration is valid only when
TSan names a data race and returns the configured exit code 66. The fixture is
never linked into a project target; it proves that the detector is active
before the clean project result is accepted.

References:

- [Apple: diagnosing memory, thread and crash issues early](https://developer.apple.com/documentation/xcode/diagnosing-memory-thread-and-crash-issues-early)
- [Clang ThreadSanitizer supported platforms and usage](https://clang.llvm.org/docs/ThreadSanitizer.html)

## Reproducible gate

Run from the repository root:

```bash
bash scripts/verify_tsan.sh
```

The script regenerates Xcode projects and keeps products, objects, DerivedData
and Clang's module cache in an isolated timestamped output under
`build/tsan-validation-*`. It records Xcode's selected compiler and SDK,
requires a native sanitizer-linked and sanitizer-instrumented binary, no
first-party compiler warnings, no sanitizer suppressions and a zero process
exit. `TSAN_OPTIONS` disables abort-on-error, fixes `halt_on_error=1` and exit
code 66 so the first race cannot be mistaken for a passing test.

The instrumented executable runs the complete 346-check world stack, not a
mock worker. In particular it requires all three V5 results:

- load-centre churn completes;
- concurrent block reads remain valid;
- the real background loader makes progress.

The gate additionally rejects both `WARNING: ThreadSanitizer` and the terminal
reported-warning summary. `summary.txt`, `toolchain.txt`,
binary/link/instrumentation identity, the build log and complete world log
remain outside Git in the timestamped evidence directory.

## Current evidence

The accepted 2026-08-17 run used an Apple M1 Pro with native arm64 macOS,
Xcode 26.2 (Apple Clang 17). The instrumented binary linked the macOS TSan
runtime, all 346 world checks passed, the three V5 checks passed and TSan
reported zero races after the detector calibration reported its expected race
and exited 66. Its generated evidence is under
`build/tsan-validation-20260817083434`; the path remains ignored and is also
recorded in `docs/runtime-validation.md`.

R4 does not prove that every possible scheduling interleaving is race-free.
It proves that the documented, concurrent V5 workload and the complete current
world regression ran under a supported compiler/runtime detector without a
report. Background-loader or synchronization changes must rerun this gate.
