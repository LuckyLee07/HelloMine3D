#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIGURATION="${1:-Debug}"
case "$CONFIGURATION" in
    Debug|Release) ;;
    *) echo "Usage: $0 [Debug|Release] [new-output-directory]" >&2; exit 2 ;;
esac
LOG_DIR="${2:-$ROOT_DIR/build/rock-landmark-planner-$(date +%Y%m%d%H%M%S)-$CONFIGURATION}"
if [[ -e "$LOG_DIR" ]]; then
    echo "Refusing to replace previous evidence: $LOG_DIR" >&2
    exit 2
fi
mkdir -p "$LOG_DIR"
ROCK_TEST_CXX="${CXX:-clang++}"
OPTIONS=(-std=c++17 -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/src/HelloMine3D")
if [[ "$CONFIGURATION" == Debug ]]; then OPTIONS+=(-O0 -g); else OPTIONS+=(-O3 -DNDEBUG); fi

git -C "$ROOT_DIR" rev-parse HEAD > "$LOG_DIR/source-commit.txt"
uname -a > "$LOG_DIR/platform.txt"
"$ROCK_TEST_CXX" --version >> "$LOG_DIR/platform.txt"
printf '%s\n' "configuration=$CONFIGURATION" >> "$LOG_DIR/platform.txt"
SOURCE_FILES=(
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/TerrainGenerator.h"
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/TerrainFoundation.h"
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/LocalTerrainPlanner.h"
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/AdventureTerrainPlanner.cpp"
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/AdventureTerrainPlanner.h"
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/AdventureWaterPlanner.cpp"
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/AdventureWaterPlanner.h"
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Ecology/AdventureEcologyPlanner.cpp"
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Ecology/AdventureEcologyPlanner.h"
    "$ROOT_DIR/src/HelloMine3D/World/Block/ChunkBlock.h"
    "$ROOT_DIR/src/HelloMine3D/World/Block/BlockId.h"
    "$ROOT_DIR/tools/tests/rock_landmark_planner_test.cpp"
    "$ROOT_DIR/scripts/verify_rock_landmark_planner.sh"
)
shasum -a 256 "${SOURCE_FILES[@]}" > "$LOG_DIR/sources-sha256.txt"

# The production header's sizeof(cache) static_assert is evaluated by this
# standalone compile. Retain its declaration as the capacity proof; the test
# exercises eviction and complete identity without exposing private cache data.
rg -n 'thread_local std::array<Patch|Local terrain patch cache stays bounded' \
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/LocalTerrainPlanner.h" \
    > "$LOG_DIR/cache-bound-proof.txt"
python3 - "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/LocalTerrainPlanner.h" <<'PY'
import re
import sys
from pathlib import Path

source = Path(sys.argv[1]).read_text()
capacity = r'thread_local\s+std::array\s*<\s*Patch\s*,\s*128\s*>\s+cache\b'
bound = r'static_assert\s*\(\s*sizeof\s*\(\s*cache\s*\)\s*<=\s*20\s*\*\s*1024'
if not re.search(capacity, source) or not re.search(bound, source):
    raise SystemExit('The local cache must retain 128 entries and its compiled 20 KiB upper bound')
print('[ROCK30] PASS local-cache-128-entry-and-20-KiB-declarations')
PY

"$ROCK_TEST_CXX" "${OPTIONS[@]}" \
    "$ROOT_DIR/tools/tests/rock_landmark_planner_test.cpp" \
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/AdventureTerrainPlanner.cpp" \
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Terrain/AdventureWaterPlanner.cpp" \
    "$ROOT_DIR/src/HelloMine3D/World/Generation/Ecology/AdventureEcologyPlanner.cpp" \
    -o "$LOG_DIR/rock_landmark_planner_test" > "$LOG_DIR/build.log" 2>&1
shasum -a 256 "$LOG_DIR/rock_landmark_planner_test" > "$LOG_DIR/binary-sha256.txt"
file "$LOG_DIR/rock_landmark_planner_test" >> "$LOG_DIR/platform.txt"
shasum -a 256 "${SOURCE_FILES[@]}" > "$LOG_DIR/sources-sha256.after-build.txt"
cmp "$LOG_DIR/sources-sha256.txt" "$LOG_DIR/sources-sha256.after-build.txt"
"$LOG_DIR/rock_landmark_planner_test" "$LOG_DIR/samples.csv" | tee "$LOG_DIR/test.log"

set +e
"$LOG_DIR/rock_landmark_planner_test" "$LOG_DIR/without-rock-field.csv" \
    --without-rock-field | tee "$LOG_DIR/without-rock-field.log"
ROCK_NEGATIVE_EXIT=${PIPESTATUS[0]}
set -e
printf '%s\n' "$ROCK_NEGATIVE_EXIT" > "$LOG_DIR/without-rock-field-exit.txt"
python3 - "$LOG_DIR/without-rock-field.log" "$ROCK_NEGATIVE_EXIT" <<'PY'
import re
import sys
from pathlib import Path

log = Path(sys.argv[1]).read_text()
failures = re.findall(r'^\[ROCK30\] FAIL (.+)$', log, flags=re.MULTILINE)
expected = {
    f'{seed}/{name}'
    for seed in (42, 20260807, 239701883)
    for name in ('nonzero-rock-height', 'nonzero-rock-core', 'connected-column-footprint')
}
if sys.argv[2] != '1' or set(failures) != expected or len(failures) != len(expected):
    raise SystemExit('The no-rock negative must fail only the same three existence checks for all seeds')
if '[ROCK30_RESULT] mode=without-rock-field' not in log:
    raise SystemExit('The no-rock negative did not complete its test run')
print('[ROCK30] PASS without-rock-field-negative-rejected-by-existence-checks')
PY
shasum -a 256 "${SOURCE_FILES[@]}" > "$LOG_DIR/sources-sha256.after-tests.txt"
cmp "$LOG_DIR/sources-sha256.txt" "$LOG_DIR/sources-sha256.after-tests.txt"
echo "[ROCK30] configuration=$CONFIGURATION logs=$LOG_DIR"
