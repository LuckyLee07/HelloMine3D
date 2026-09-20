#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIGURATION="${1:-Debug}"
case "$CONFIGURATION" in Debug|Release) ;; *) echo "Usage: $0 [Debug|Release] [new-output-directory]" >&2; exit 2;; esac
LOG_DIR="${2:-$ROOT_DIR/build/adventure-water-$(date +%Y%m%d%H%M%S)-$CONFIGURATION}"
if [[ -e "$LOG_DIR" ]]; then echo "Refusing to replace previous evidence: $LOG_DIR" >&2; exit 2; fi
mkdir -p "$LOG_DIR"
OPTIONS=(-std=c++17 -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/src/HelloMine3D")
if [[ "$CONFIGURATION" == Debug ]]; then OPTIONS+=(-O0 -g); else OPTIONS+=(-O3 -DNDEBUG); fi
shasum -a 256 "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/AdventureTerrainPlanner.cpp" \
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/AdventureTerrainPlanner.h" \
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/AdventureWaterPlanner.cpp" \
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/AdventureWaterPlanner.h" \
    "$ROOT_DIR/tools/tests/adventure_water_test.cpp" > "$LOG_DIR/sources-sha256.txt"
clang++ "${OPTIONS[@]}" "$ROOT_DIR/tools/tests/adventure_water_test.cpp" \
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/AdventureTerrainPlanner.cpp" \
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/AdventureWaterPlanner.cpp" \
    -o "$LOG_DIR/adventure_water_test" > "$LOG_DIR/build.log" 2>&1
"$LOG_DIR/adventure_water_test" "$LOG_DIR/samples.csv" | tee "$LOG_DIR/test.log"
