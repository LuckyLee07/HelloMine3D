#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIGURATION="${1:-Debug}"
case "$CONFIGURATION" in Debug|Release) ;; *) echo "Usage: $0 [Debug|Release] [new-output-directory]" >&2; exit 2;; esac
LOG_DIR="${2:-$ROOT_DIR/build/reference-shapes-$(date +%Y%m%d%H%M%S)-$CONFIGURATION}"
if [[ -e "$LOG_DIR" ]]; then echo "Refusing to replace previous evidence: $LOG_DIR" >&2; exit 2; fi
mkdir -p "$LOG_DIR"
OPTIONS=(-std=c++17 -Wall -Wextra -Werror -I"$ROOT_DIR/src/external/glm" -I"$ROOT_DIR/src/HelloMine3D")
if [[ "$CONFIGURATION" == Debug ]]; then OPTIONS+=(-O0 -g); else OPTIONS+=(-O3 -DNDEBUG); fi
python3 -B "$ROOT_DIR/tools/export_architectural_kit.py" --check > "$LOG_DIR/authoring-check.log"
shasum -a 256 "$ROOT_DIR/src/HelloMine3D/World/Block/BlockShape.cpp" "$ROOT_DIR/src/HelloMine3D/World/Block/BlockShape.h" "$ROOT_DIR/src/HelloMine3D/World/Block/BlockGeometry.h" "$ROOT_DIR/tools/tests/reference_shape_test.cpp" "$ROOT_DIR/media/sources/architectural-kit-v1.json" "$ROOT_DIR/tools/export_architectural_kit.py" "$ROOT_DIR"/media/shapes/{StoneStep,StoneWindowFrame,StoneBrick,StoneSlab,StoneCornice,ClayTileStep,ClayTileEave,TimberBeam,TimberRailing,StoneWindowSill,StonePlanter,Lantern}.shape "$ROOT_DIR"/media/blocks/{StoneStep,StoneWindowFrame,StoneBrick,StoneSlab,StoneCornice,ClayTileStep,ClayTileEave,TimberBeam,TimberRailing,StoneWindowSill,StonePlanter,Lantern}.block > "$LOG_DIR/sources-sha256.txt"
clang++ "${OPTIONS[@]}" "$ROOT_DIR/src/HelloMine3D/World/Block/BlockShape.cpp" "$ROOT_DIR/tools/tests/reference_shape_test.cpp" -o "$LOG_DIR/reference_shape_test" > "$LOG_DIR/build.log" 2>&1
"$LOG_DIR/reference_shape_test" "$ROOT_DIR" "$LOG_DIR" | tee "$LOG_DIR/test.log"
echo "[REFERENCE-SHAPE] configuration=$CONFIGURATION logs=$LOG_DIR"
