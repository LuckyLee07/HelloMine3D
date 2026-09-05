#!/usr/bin/env bash
# Non-visible Cocoa/ImGui regressions; never a replacement for OS acceptance.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIGURATION="${1:-Debug}"
case "$CONFIGURATION" in Debug|Release) ;; *) echo "Usage: $0 [Debug|Release]" >&2; exit 2;; esac
if [[ "$(uname -s)" != Darwin ]]; then
    echo "Cocoa input regressions require macOS." >&2
    exit 2
fi
LOG_DIR="$ROOT_DIR/build/cocoa-input-$(date +%Y%m%d%H%M%S)-$CONFIGURATION"
mkdir -p "$LOG_DIR"
OIS_ARCHIVE="$ROOT_DIR/build/External/ois/lib/x64/$CONFIGURATION/libois.a"
IMGUI_ARCHIVE="$ROOT_DIR/build/External/imgui/lib/x64/$CONFIGURATION/libimgui.a"
COMMON=(-arch x86_64 -std=c++17 -fblocks)
# Match the deployment target of the tested archives when explicitly supplied.
if [[ -n "${HELLOMINE3D_INPUT_TEST_MIN_MACOS:-}" ]]; then
    COMMON+=("-mmacosx-version-min=$HELLOMINE3D_INPUT_TEST_MIN_MACOS")
fi
for TEST_NAME in cocoa_keyboard_test cocoa_keyboard_focus_test cocoa_mouse_focus_test; do
    EXTRA_SOURCES=()
    if [[ "$TEST_NAME" == cocoa_mouse_focus_test ]]; then
        EXTRA_SOURCES+=("$ROOT_DIR/src/HelloMine3D/GameplayInput.cpp")
    fi
    clang++ "${COMMON[@]}" -I"$ROOT_DIR/src/external/ois/includes" \
        -I"$ROOT_DIR/src/HelloMine3D" "$ROOT_DIR/tools/tests/$TEST_NAME.mm" \
        "${EXTRA_SOURCES[@]}" "$OIS_ARCHIVE" -framework Cocoa \
        -framework IOKit -framework ForceFeedback -framework Carbon \
        -o "$LOG_DIR/$TEST_NAME" > "$LOG_DIR/$TEST_NAME-build.log" 2>&1
    "$LOG_DIR/$TEST_NAME" 2>&1 | tee "$LOG_DIR/$TEST_NAME.log"
done
clang++ "${COMMON[@]}" -I"$ROOT_DIR/src/external/imgui" \
    "$ROOT_DIR/tools/tests/imgui_focus_semantics_test.cpp" "$IMGUI_ARCHIVE" \
    -o "$LOG_DIR/imgui_focus_semantics_test" > "$LOG_DIR/imgui-focus-build.log" 2>&1
"$LOG_DIR/imgui_focus_semantics_test" | tee "$LOG_DIR/imgui-focus.log"
echo "[COCOA_INPUT] status=PASS configuration=$CONFIGURATION logs=$LOG_DIR"
