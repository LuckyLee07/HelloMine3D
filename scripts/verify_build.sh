#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TESTS=(
    HelloMine3DCoordinateTests
    HelloMine3DMeshDirtyTests
    HelloMine3DSaveLoadSmoke
    HelloMine3DEntityLifecycleSmoke
    HelloMine3DWorldRuntimeSmoke
)

for config in debug release; do
    echo "[BUILD_VERIFY] Build $config"
    "$ROOT_DIR/scripts/build.sh" "$config"

    for test_name in "${TESTS[@]}"; do
        test_path="$ROOT_DIR/bin/$test_name"
        if [ ! -x "$test_path" ]; then
            echo "Expected test executable was not built: $test_path" >&2
            exit 1
        fi
        echo "[BUILD_VERIFY] $config $test_name"
        "$test_path"
    done
done

echo "[BUILD_VERIFY] status=PASS"
