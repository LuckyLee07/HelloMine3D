#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIGURATION="${1:-Debug}"
case "$CONFIGURATION" in Debug|Release) ;; *) echo "Usage: $0 [Debug|Release] [new-output-directory]" >&2; exit 2;; esac
LOG_DIR="${2:-$ROOT_DIR/build/hud-interaction-$(date +%Y%m%d%H%M%S)-$CONFIGURATION}"
if [[ -e "$LOG_DIR" ]]; then echo "Refusing to replace previous evidence: $LOG_DIR" >&2; exit 2; fi
mkdir -p "$LOG_DIR"
OPTIONS=(-std=c++17 -Wall -Wextra -Werror)
if [[ "$(uname -s)" == Darwin ]]; then OPTIONS+=(-arch x86_64); fi
if [[ "$CONFIGURATION" == Debug ]]; then OPTIONS+=(-O0 -g); else OPTIONS+=(-O3 -DNDEBUG); fi
clang++ "${OPTIONS[@]}" "$ROOT_DIR/tools/tests/hud_interaction_test.cpp" \
    "$ROOT_DIR/src/HelloMine3D/GameplayInput.cpp" \
    -o "$LOG_DIR/hud_interaction_test" > "$LOG_DIR/build.log" 2>&1
"$LOG_DIR/hud_interaction_test" | tee "$LOG_DIR/test.log"
