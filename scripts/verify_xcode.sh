#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
BIN_DIR="$ROOT_DIR/bin"
LOG_DIR="$BUILD_DIR/xcode-validation-$(date +%Y%m%d%H%M%S)"
CLIENT="HelloMine3D"
TESTS=(
    HelloMine3DCoordinateTests
    HelloMine3DMeshDirtyTests
    HelloMine3DSaveLoadSmoke
    HelloMine3DEntityLifecycleSmoke
    HelloMine3DWorldRuntimeSmoke
    HelloMine3DSoak
    HelloMine3DResourcePackSmoke
    HelloMine3DRecipeSmoke
    HelloMine3DWorldCatalogueSmoke
    HelloMine3DStorageTransactionSmoke
    HelloMine3DWorldBackupSmoke
)

if [ "$(uname -s)" != "Darwin" ]; then
    echo "[XCODE_VERIFY] This command must run on macOS." >&2
    exit 2
fi

for tool in premake5 xcodebuild python3; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "[XCODE_VERIFY] Required tool not found: $tool" >&2
        exit 2
    fi
done

mkdir -p "$LOG_DIR"
echo "[XCODE_VERIFY] Validate performance contracts"
python3 "$ROOT_DIR/tools/validate_perf_comparison.py" |
    tee "$LOG_DIR/performance_contracts.log"
"$ROOT_DIR/xcode.sh"
echo "[XCODE_VERIFY] Validate generated project graph"
XCODE_GRAPH_LOG="$LOG_DIR/xcode_project_graph.log"
python3 "$ROOT_DIR/tools/validate_xcode_project_graph.py" \
    --self-test \
    --build-dir "$BUILD_DIR" | tee "$XCODE_GRAPH_LOG"
if ! grep -F "[XCODE_GRAPH] status=PASS projects=29" \
    "$XCODE_GRAPH_LOG" >/dev/null; then
    echo "[XCODE_VERIFY] Generated project graph summary is missing." >&2
    exit 1
fi

build_target() {
    local target="$1"
    local configuration="$2"
    local project="$BUILD_DIR/$target/$target.xcodeproj"
    local log="$LOG_DIR/${configuration}_${target}_build.log"

    if [ ! -d "$project" ]; then
        echo "[XCODE_VERIFY] Generated project is missing: $project" >&2
        exit 1
    fi

    echo "[XCODE_VERIFY] Build $configuration $target"
    xcodebuild \
        -quiet \
        -parallelizeTargets \
        -project "$project" \
        -target "$target" \
        -configuration "$configuration" \
        -arch x86_64 \
        CODE_SIGNING_ALLOWED=NO \
        build 2>&1 | tee "$log"

    reject_build_warning "$log" "member of multiple groups" \
        "duplicate Xcode project reference"
    reject_build_warning "$log" \
        "Building targets in manual order is deprecated" \
        "manual Xcode target ordering"
    reject_build_warning "$log" "/src/HelloMine3D/.*: warning:" \
        "first-party compiler warning"
}

reject_build_warning() {
    local log="$1"
    local pattern="$2"
    local label="$3"

    if grep -E "$pattern" "$log" >/dev/null; then
        echo "[XCODE_VERIFY] Unexpected $label in $log" >&2
        grep -E "$pattern" "$log" >&2
        exit 1
    fi
}

run_binary() {
    local name="$1"
    local configuration="$2"
    local executable="$BIN_DIR/$name"
    local log="$LOG_DIR/${configuration}_${name}_run.log"

    if [ ! -x "$executable" ]; then
        echo "[XCODE_VERIFY] Expected executable is missing: $executable" >&2
        exit 1
    fi

    echo "[XCODE_VERIFY] Run $configuration $name"
    (
        cd "$BIN_DIR"
        "$executable"
    ) 2>&1 | tee "$log"

    if [ "$name" = "HelloMine3DWorldRuntimeSmoke" ] &&
       ! grep -F "[VALIDATION] checks=343 failures=0" "$log" >/dev/null; then
        echo "[XCODE_VERIFY] World runtime summary is missing or failed." >&2
        exit 1
    fi
    if [ "$name" = "HelloMine3DWorldCatalogueSmoke" ] &&
       ! grep -F "[WORLD_CATALOGUE_TEST] checks=28 failures=0" \
           "$log" >/dev/null; then
        echo "[XCODE_VERIFY] World catalogue summary is missing or failed." >&2
        exit 1
    fi
    if [ "$name" = "HelloMine3DStorageTransactionSmoke" ] &&
       ! grep -F "[STORAGE_TRANSACTION_TEST] checks=16 failures=0" \
           "$log" >/dev/null; then
        echo "[XCODE_VERIFY] Storage transaction summary is missing or failed." >&2
        exit 1
    fi
    if [ "$name" = "HelloMine3DWorldBackupSmoke" ] &&
       ! grep -F "[WORLD_BACKUP_TEST] checks=16 failures=0" \
           "$log" >/dev/null; then
        echo "[XCODE_VERIFY] World backup summary is missing or failed." >&2
        exit 1
    fi
}

run_client_probe() {
    local configuration="$1"
    local mode="$2"
    local log="$LOG_DIR/${configuration}_${CLIENT}_${mode}.log"
    local save_dir="$LOG_DIR/${configuration}_${mode}_save"
    local -a environment=(
        "HELLOMINE3D_ROOT=$ROOT_DIR"
        "HELLOMINE3D_SAVE_DIR=$save_dir"
        "HELLOMINE3D_SEED=20260809"
        "HELLOMINE3D_PLAYER_POSITION=3038 66 1922"
        "HELLOMINE3D_PLAYER_ROTATION=0 0 0"
    )

    if [ "$mode" = "validate" ]; then
        environment+=("HELLOMINE3D_VALIDATE_ONLY=1")
    else
        environment+=("HELLOMINE3D_EXIT_AFTER_FRAMES=120")
    fi

    echo "[XCODE_VERIFY] Run $configuration client $mode probe"
    set +e
    (
        cd "$BIN_DIR"
        env "${environment[@]}" "$BIN_DIR/$CLIENT"
    ) >"$log" 2>&1 &
    local client_pid=$!
    (
        sleep 60
        if kill -0 "$client_pid" >/dev/null 2>&1; then
            echo "[XCODE_VERIFY] Client probe timed out." >>"$log"
            kill -TERM "$client_pid" >/dev/null 2>&1 || true
            sleep 2
            kill -KILL "$client_pid" >/dev/null 2>&1 || true
        fi
    ) &
    local watchdog_pid=$!

    wait "$client_pid"
    local status=$?
    kill "$watchdog_pid" >/dev/null 2>&1 || true
    wait "$watchdog_pid" >/dev/null 2>&1 || true
    set -e

    if [ "$status" -ne 0 ]; then
        cat "$log" >&2
        echo "[XCODE_VERIFY] Client $mode probe failed with status $status." >&2
        exit 1
    fi

    if [ "$mode" = "validate" ]; then
        if ! grep -F "[OGRE_VALIDATION] renderer=OpenGL 3+" "$log" >/dev/null; then
            cat "$log" >&2
            echo "[XCODE_VERIFY] Validation probe did not register GL3Plus." >&2
            exit 1
        fi
    else
        if ! grep -F "[OGRE_TERRAIN]" "$log" >/dev/null; then
            cat "$log" >&2
            echo "[XCODE_VERIFY] Window probe did not reach terrain startup." >&2
            exit 1
        fi

        local world_meta="$save_dir/world.meta"
        local player_y
        player_y="$(awk '$1 == "player_position" {print $3; exit}' "$world_meta")"
        if [ -z "$player_y" ] ||
           ! awk -v y="$player_y" 'BEGIN {exit !(y >= 65 && y <= 67)}'; then
            cat "$world_meta" >&2
            echo "[XCODE_VERIFY] Player did not remain on the surface: y=$player_y" >&2
            exit 1
        fi
        echo "[XCODE_VERIFY] Surface contact y=$player_y"
    fi
}

for configuration in Debug Release; do
    build_target "$CLIENT" "$configuration"
    for test_name in "${TESTS[@]}"; do
        build_target "$test_name" "$configuration"
        run_binary "$test_name" "$configuration"
    done
    run_client_probe "$configuration" validate
    run_client_probe "$configuration" window
done

echo "[XCODE_VERIFY] status=PASS logs=$LOG_DIR"
