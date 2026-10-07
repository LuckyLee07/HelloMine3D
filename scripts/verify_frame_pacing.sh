#!/usr/bin/env bash
# Standalone native-architecture policy test; no Ogre, window or World target.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIGURATION="${1:-Debug}"
if [[ $# -gt 2 ]]; then
    echo "Usage: $0 [Debug|Release] [new-output-directory]" >&2
    exit 2
fi
case "$CONFIGURATION" in
    Debug|Release) ;;
    *) echo "Usage: $0 [Debug|Release] [new-output-directory]" >&2; exit 2 ;;
esac
LOG_DIR="${2:-$ROOT_DIR/build/frame-pacing-$(date +%Y%m%d%H%M%S)-$CONFIGURATION}"
if [[ -e "$LOG_DIR" ]]; then
    echo "Refusing to replace previous evidence: $LOG_DIR" >&2
    exit 2
fi
mkdir -p "$LOG_DIR"
NATIVE_ARCH="$(uname -m)"
OPTIONS=(-std=c++17 -Wall -Wextra -Werror)
if [[ "$(uname -s)" == Darwin ]]; then OPTIONS+=(-arch "$NATIVE_ARCH"); fi
if [[ "$CONFIGURATION" == Debug ]]; then OPTIONS+=(-O0 -g); else OPTIONS+=(-O3 -DNDEBUG); fi
shasum -a 256 "$ROOT_DIR/src/HelloMine3D/Presentation/FramePacing.h" \
    "$ROOT_DIR/tools/tests/frame_pacing_test.cpp" "$ROOT_DIR/scripts/verify_frame_pacing.sh" \
    > "$LOG_DIR/sources-sha256.txt"
clang++ "${OPTIONS[@]}" "$ROOT_DIR/tools/tests/frame_pacing_test.cpp" \
    -o "$LOG_DIR/frame_pacing_test" > "$LOG_DIR/build.log" 2>&1
"$LOG_DIR/frame_pacing_test" 2>&1 | tee "$LOG_DIR/test.log"
echo "[FRAME_PACING] configuration=$CONFIGURATION architecture=$NATIVE_ARCH logs=$LOG_DIR"
