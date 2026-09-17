#!/usr/bin/env bash
# Standalone production Timer test; no client, window or system-clock mutation.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIGURATION="${1:-Debug}"
case "$CONFIGURATION" in Debug|Release) ;; *) echo "Usage: $0 [Debug|Release] [new-output-directory]" >&2; exit 2;; esac
if [[ "$(uname -s)" != Darwin ]]; then
    echo "This validation route targets the macOS x86_64 client." >&2
    exit 2
fi
LOG_DIR="${2:-$ROOT_DIR/build/posix-timer-$(date +%Y%m%d%H%M%S)-$CONFIGURATION}"
if [[ -e "$LOG_DIR" ]]; then echo "Refusing to replace previous evidence: $LOG_DIR" >&2; exit 2; fi
mkdir -p "$LOG_DIR"
COMMON=(-arch x86_64 -std=c++17 -D_LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR
    -D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION
    -I"$ROOT_DIR/src/Engine/ogre3d/include")
if [[ "$CONFIGURATION" == Debug ]]; then COMMON+=(-O0 -g); else COMMON+=(-O3 -DNDEBUG); fi
shasum -a 256 "$ROOT_DIR/src/Engine/ogre3d/src/OgrePOSIXTimer.cpp" \
    "$ROOT_DIR/src/Engine/ogre3d/include/OgrePOSIXTimerImp.h" \
    "$ROOT_DIR/tools/tests/posix_timer_test.cpp" > "$LOG_DIR/sources-sha256.txt"
clang++ "${COMMON[@]}" -Dgettimeofday=hello_test_gettimeofday \
    -c "$ROOT_DIR/src/Engine/ogre3d/src/OgrePOSIXTimer.cpp" -o "$LOG_DIR/timer.o" \
    > "$LOG_DIR/build.log" 2>&1
clang++ "${COMMON[@]}" "$ROOT_DIR/tools/tests/posix_timer_test.cpp" "$LOG_DIR/timer.o" \
    -o "$LOG_DIR/posix_timer_test" >> "$LOG_DIR/build.log" 2>&1
"$LOG_DIR/posix_timer_test" | tee "$LOG_DIR/test.log"
echo "[POSIX_TIMER] status=PASS configuration=$CONFIGURATION logs=$LOG_DIR"
