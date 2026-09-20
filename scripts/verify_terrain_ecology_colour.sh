#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIGURATION="${1:-Debug}"
case "$CONFIGURATION" in Debug|Release) ;; *) echo "Usage: $0 [Debug|Release] [new-output-directory]" >&2; exit 2;; esac
LOG_DIR="${2:-$ROOT_DIR/build/ecology-colour-$(date +%Y%m%d%H%M%S)-$CONFIGURATION}"
if [[ -e "$LOG_DIR" ]]; then echo "Refusing to replace previous evidence: $LOG_DIR" >&2; exit 2; fi
mkdir -p "$LOG_DIR"
OPTIONS=(-std=c++17 -Wall -Wextra -Werror -I"$ROOT_DIR/src/external/glm" -I"$ROOT_DIR/src/HelloMine3D")
if [[ "$CONFIGURATION" == Debug ]]; then OPTIONS+=(-O0 -g -fsanitize=undefined); else OPTIONS+=(-O3 -DNDEBUG); fi
shasum -a 256 "$ROOT_DIR/src/HelloMine3D/World/Block/TerrainEcologyColour.h" \
    "$ROOT_DIR/tools/tests/terrain_ecology_colour_test.cpp" > "$LOG_DIR/sources-sha256.txt"
clang++ "${OPTIONS[@]}" "$ROOT_DIR/tools/tests/terrain_ecology_colour_test.cpp" \
    -o "$LOG_DIR/ecology_colour_test" > "$LOG_DIR/build.log" 2>&1
"$LOG_DIR/ecology_colour_test" | tee "$LOG_DIR/test.log"
echo "[ECOLOGY-COLOUR] configuration=$CONFIGURATION logs=$LOG_DIR"
