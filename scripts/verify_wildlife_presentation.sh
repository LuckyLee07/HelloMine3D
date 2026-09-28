#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIGURATION="${1:-Debug}"
case "$CONFIGURATION" in Debug|Release) ;; *) echo 'Usage: verify_wildlife_presentation.sh [Debug|Release] [new-output-directory]' >&2; exit 2;; esac
LOG_DIR="${2:-$ROOT_DIR/build/wildlife-presentation-$(date +%Y%m%d%H%M%S)-$CONFIGURATION}"
if [[ -e "$LOG_DIR" ]]; then echo "Refusing to replace previous evidence: $LOG_DIR" >&2; exit 2; fi
mkdir -p "$LOG_DIR"
OPTIONS=(-std=c++17 -Wall -Wextra -Werror -I"$ROOT_DIR/src/HelloMine3D" -isystem "$ROOT_DIR/src/external/glm")
if [[ "$CONFIGURATION" == Debug ]]; then OPTIONS+=(-O0 -g); else OPTIONS+=(-O3 -DNDEBUG); fi
shasum -a 256 "$ROOT_DIR/src/HelloMine3D/Actor/WildlifePresentation.h" "$ROOT_DIR/tools/tests/wildlife_presentation_test.cpp" > "$LOG_DIR/sources-sha256.txt"
"${CXX:-clang++}" "${OPTIONS[@]}" "$ROOT_DIR/tools/tests/wildlife_presentation_test.cpp" -o "$LOG_DIR/wildlife_test" > "$LOG_DIR/build.log" 2>&1
"$LOG_DIR/wildlife_test" | tee "$LOG_DIR/test.log"
echo "[WILDLIFE_PRESENTATION] configuration=$CONFIGURATION logs=$LOG_DIR"
