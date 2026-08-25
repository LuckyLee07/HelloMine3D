#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
BIN_DIR="$ROOT_DIR/bin"
HOST_ARCH="$(uname -m)"
LOG_DIR="$BUILD_DIR/tsan-validation-$(date +%Y%m%d%H%M%S)"
PRODUCT_DIR="$LOG_DIR/products"
OBJECT_DIR="$LOG_DIR/obj"
DERIVED_DATA_DIR="$LOG_DIR/DerivedData"
MODULE_CACHE_DIR="$LOG_DIR/ModuleCache"
PROJECT="$BUILD_DIR/HelloMine3DWorldRuntimeSmoke/HelloMine3DWorldRuntimeSmoke.xcodeproj"
TARGET="HelloMine3DWorldRuntimeSmoke"
BINARY="$PRODUCT_DIR/$TARGET"
BUILD_LOG="$LOG_DIR/build.log"
RUN_LOG="$LOG_DIR/world-runtime.log"
PROBE_SOURCE="$ROOT_DIR/tools/fixtures/tsan-race-probe.cpp"
PROBE_BINARY="$LOG_DIR/tsan-race-probe"
PROBE_LOG="$LOG_DIR/tsan-race-probe.log"

if [ "$(uname -s)" != "Darwin" ]; then
    echo "[TSAN_VERIFY] ThreadSanitizer validation requires macOS." >&2
    exit 2
fi

case "$HOST_ARCH" in
    arm64|x86_64)
        ;;
    *)
        echo "[TSAN_VERIFY] Unsupported native architecture: $HOST_ARCH" >&2
        exit 2
        ;;
esac

for tool in premake5 xcodebuild file nm otool rg; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "[TSAN_VERIFY] Required tool not found: $tool" >&2
        exit 2
    fi
done

mkdir -p \
    "$PRODUCT_DIR" \
    "$OBJECT_DIR" \
    "$DERIVED_DATA_DIR" \
    "$MODULE_CACHE_DIR"
XCODE_CLANG="$(xcodebuild -find clang++)"
XCODE_SDK="$(xcodebuild -version -sdk macosx Path)"
if [ ! -x "$XCODE_CLANG" ]; then
    echo "[TSAN_VERIFY] Xcode C++ compiler is missing: $XCODE_CLANG" >&2
    exit 2
fi
if [ ! -d "$XCODE_SDK" ]; then
    echo "[TSAN_VERIFY] Xcode macOS SDK is missing: $XCODE_SDK" >&2
    exit 2
fi
if [ ! -f "$PROBE_SOURCE" ]; then
    echo "[TSAN_VERIFY] Detector probe source is missing: $PROBE_SOURCE" >&2
    exit 2
fi
{
    echo "host_arch=$HOST_ARCH"
    sw_vers
    xcodebuild -version
    echo "xcode_clang=$XCODE_CLANG"
    echo "xcode_sdk=$XCODE_SDK"
    "$XCODE_CLANG" --version
} >"$LOG_DIR/toolchain.txt"

if rg -n \
    'no_sanitize.*thread|disable_sanitizer_instrumentation|fno-sanitize=thread|__tsan_default_suppressions' \
    "$ROOT_DIR/src/HelloMine3D" \
    "$ROOT_DIR/premake" >"$LOG_DIR/suppression-scan.txt"; then
    echo "[TSAN_VERIFY] First-party sanitizer suppression is forbidden." >&2
    cat "$LOG_DIR/suppression-scan.txt" >&2
    exit 1
fi

echo "[TSAN_VERIFY] Calibrate ThreadSanitizer with an expected race"
if ! "$XCODE_CLANG" \
    -isysroot "$XCODE_SDK" \
    -std=c++17 \
    -g \
    -O1 \
    -fsanitize=thread \
    "$PROBE_SOURCE" \
    -o "$PROBE_BINARY" >"$LOG_DIR/tsan-race-probe-build.log" 2>&1; then
    cat "$LOG_DIR/tsan-race-probe-build.log" >&2
    exit 1
fi
set +e
env TSAN_OPTIONS='halt_on_error=1:abort_on_error=0:exitcode=66:report_bugs=1' \
    "$PROBE_BINARY" >"$PROBE_LOG" 2>&1
probe_status=$?
set -e
if [ "$probe_status" -ne 66 ] ||
   ! grep -F 'WARNING: ThreadSanitizer: data race' \
       "$PROBE_LOG" >/dev/null; then
    echo "[TSAN_VERIFY] Detector probe did not report the expected race." >&2
    cat "$PROBE_LOG" >&2
    exit 1
fi

echo "[TSAN_VERIFY] Generate Xcode projects"
"$ROOT_DIR/xcode.sh" >"$LOG_DIR/generation.log" 2>&1
if [ ! -d "$PROJECT" ]; then
    echo "[TSAN_VERIFY] Generated project is missing: $PROJECT" >&2
    exit 1
fi

echo "[TSAN_VERIFY] Build Debug $HOST_ARCH with ThreadSanitizer"
if ! xcodebuild \
    -quiet \
    -parallelizeTargets \
    -project "$PROJECT" \
    -scheme "$TARGET" \
    -configuration Debug \
    -arch "$HOST_ARCH" \
    -derivedDataPath "$DERIVED_DATA_DIR" \
    CODE_SIGNING_ALLOWED=NO \
    CLANG_MODULE_CACHE_PATH="$MODULE_CACHE_DIR" \
    COMPILER_INDEX_STORE_ENABLE=NO \
    ENABLE_THREAD_SANITIZER=YES \
    CONFIGURATION_BUILD_DIR="$PRODUCT_DIR" \
    OBJROOT="$OBJECT_DIR" \
    SYMROOT="$PRODUCT_DIR" \
    build >"$BUILD_LOG" 2>&1; then
    tail -n 200 "$BUILD_LOG" >&2
    exit 1
fi

if rg -n 'Building targets in manual order is deprecated' "$BUILD_LOG"; then
    echo "[TSAN_VERIFY] Xcode used deprecated manual target ordering." >&2
    exit 1
fi
if rg -n '/src/HelloMine3D/.*: warning:' "$BUILD_LOG"; then
    echo "[TSAN_VERIFY] First-party compiler warning detected." >&2
    exit 1
fi
if [ ! -x "$BINARY" ]; then
    echo "[TSAN_VERIFY] Instrumented executable is missing: $BINARY" >&2
    exit 1
fi

file "$BINARY" >"$LOG_DIR/binary.txt"
otool -L "$BINARY" >"$LOG_DIR/linked-libraries.txt"
nm -u "$BINARY" >"$LOG_DIR/sanitizer-symbols.txt"
if ! grep -F "Mach-O 64-bit executable $HOST_ARCH" \
    "$LOG_DIR/binary.txt" >/dev/null; then
    echo "[TSAN_VERIFY] Instrumented executable is not native $HOST_ARCH." >&2
    cat "$LOG_DIR/binary.txt" >&2
    exit 1
fi
if ! grep -F 'libclang_rt.tsan_osx_dynamic.dylib' \
    "$LOG_DIR/linked-libraries.txt" >/dev/null; then
    echo "[TSAN_VERIFY] ThreadSanitizer runtime is not linked." >&2
    exit 1
fi
for symbol in ___tsan_init ___tsan_read1 ___tsan_write1; do
    if ! grep -F "$symbol" "$LOG_DIR/sanitizer-symbols.txt" >/dev/null; then
        echo "[TSAN_VERIFY] ThreadSanitizer instrumentation is missing: $symbol" >&2
        exit 1
    fi
done

echo "[TSAN_VERIFY] Run the 560-check world stack"
set +e
(
    cd "$BIN_DIR"
    env TSAN_OPTIONS='halt_on_error=1:abort_on_error=0:exitcode=66:report_bugs=1' \
        "$BINARY"
) >"$RUN_LOG" 2>&1
status=$?
set -e

if [ "$status" -ne 0 ]; then
    echo "[TSAN_VERIFY] Instrumented world stack failed with exit $status." >&2
    tail -n 200 "$RUN_LOG" >&2
    exit "$status"
fi

if rg -n 'WARNING: ThreadSanitizer|ThreadSanitizer: reported' "$RUN_LOG"; then
    echo "[TSAN_VERIFY] ThreadSanitizer reported a data race." >&2
    exit 1
fi

for expected in \
    '[VALIDATION] PASS V5/load-center-churn-completes' \
    '[VALIDATION] PASS V5/concurrent-block-reads-valid' \
    '[VALIDATION] PASS V5/background-loader-makes-progress' \
    '[VALIDATION] checks=560 failures=0' \
    '[VALIDATION] status=PASS'; do
    if ! grep -F "$expected" "$RUN_LOG" >/dev/null; then
        echo "[TSAN_VERIFY] Expected runtime evidence is missing: $expected" >&2
        exit 1
    fi
done

{
    echo "status=PASS"
    echo "configuration=Debug"
    echo "architecture=$HOST_ARCH"
    echo "sanitizer=thread"
    echo "sanitizer_runtime=libclang_rt.tsan_osx_dynamic.dylib"
    echo "sanitizer_instrumentation=present"
    echo "sanitizer_suppressions=0"
    echo "detector_probe=PASS"
    echo "detector_probe_exit=66"
    echo "world_checks=560"
    echo "world_failures=0"
    echo "v5_checks=3"
    echo "tsan_reports=0"
} >"$LOG_DIR/summary.txt"

echo "[TSAN_VERIFY] status=PASS arch=$HOST_ARCH checks=560 tsan_reports=0 logs=$LOG_DIR"
