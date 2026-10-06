#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIGURATION="${1:-Debug}"
case "$CONFIGURATION" in
    Debug|Release) ;;
    *) echo "Usage: $0 [Debug|Release] [new-output-directory]" >&2; exit 2 ;;
esac
LOG_DIR="${2:-$ROOT_DIR/build/crafting-result-feedback-$(date +%Y%m%d%H%M%S)-$CONFIGURATION}"
if [[ -e "$LOG_DIR" ]]; then
    echo "Refusing to replace previous evidence: $LOG_DIR" >&2
    exit 2
fi
mkdir -p "$LOG_DIR"
OPTIONS=(-std=c++17 -Wall -Wextra -Werror)
if [[ "$(uname -s)" == Darwin ]]; then OPTIONS+=(-arch x86_64); fi
if [[ "$CONFIGURATION" == Debug ]]; then OPTIONS+=(-O0 -g); else OPTIONS+=(-O3 -DNDEBUG); fi
clang++ "${OPTIONS[@]}" "$ROOT_DIR/tools/tests/crafting_result_feedback_test.cpp" \
    "$ROOT_DIR/src/HelloMine3D/Item/CraftingSession.cpp" \
    "$ROOT_DIR/src/HelloMine3D/Item/Inventory.cpp" \
    "$ROOT_DIR/src/HelloMine3D/Item/ItemStack.cpp" \
    "$ROOT_DIR/src/HelloMine3D/Item/Material.cpp" \
    "$ROOT_DIR/src/HelloMine3D/Item/RecipeRegistry.cpp" \
    "$ROOT_DIR/src/HelloMine3D/Item/ToolRegistry.cpp" \
    "$ROOT_DIR/src/HelloMine3D/Util/ResourcePackResolver.cpp" \
    -o "$LOG_DIR/crafting_result_feedback_test" > "$LOG_DIR/build.log" 2>&1
"$LOG_DIR/crafting_result_feedback_test" "$ROOT_DIR/media/recipes/Base.recipe" \
    2>&1 | tee "$LOG_DIR/test.log"
