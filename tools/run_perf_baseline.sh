#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN_DIR="$ROOT_DIR/bin"
CLIENT="$BIN_DIR/HelloMine3D"
RUN_ID="$(date +%Y%m%d%H%M%S)-$$"
OUTPUT_DIR="${1:-$BIN_DIR/perf_baseline_macos_$RUN_ID}"
SAVE_DIR="$OUTPUT_DIR/save"
SUMMARY="$OUTPUT_DIR/summary.txt"
FRAMES="$OUTPUT_DIR/frames.csv"
STDOUT_LOG="$OUTPUT_DIR/process.stdout.log"
STDERR_LOG="$OUTPUT_DIR/process.stderr.log"
SCENE_ID="q1-pre-stage8-nominal-v1"
PLAYER_POSITION="3038 66 1922"

if [ "$(uname -s)" != "Darwin" ]; then
    echo "[PERF_BASELINE] This launcher currently supports macOS only." >&2
    exit 2
fi
if [ ! -x "$CLIENT" ]; then
    echo "[PERF_BASELINE] Release client is missing: $CLIENT" >&2
    exit 2
fi

mkdir -p "$OUTPUT_DIR" "$SAVE_DIR"

cleanup() {
    if [ -n "${CLIENT_PID:-}" ] && kill -0 "$CLIENT_PID" >/dev/null 2>&1; then
        kill -TERM "$CLIENT_PID" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT INT TERM

echo "[PERF_BASELINE] scene=$SCENE_ID output=$OUTPUT_DIR"
(
    cd "$BIN_DIR"
    env \
        HELLOMINE3D_ROOT="$ROOT_DIR" \
        HELLOMINE3D_SAVE_DIR="$SAVE_DIR" \
        HELLOMINE3D_RESOURCE_PACKS="" \
        HELLOMINE3D_SEED="20260809" \
        HELLOMINE3D_PLAYER_POSITION="$PLAYER_POSITION" \
        HELLOMINE3D_PLAYER_ROTATION="0 0 0" \
        HELLOMINE3D_WORLD_TIME="6000" \
        HELLO_PERF_CAPTURE="1" \
        HELLO_PERF_CAPTURE_DIR="$OUTPUT_DIR" \
        HELLO_PERF_CAPTURE_WARMUP_MS="3000" \
        HELLO_PERF_CAPTURE_DURATION_MS="10000" \
        HELLO_PERF_CAPTURE_EXIT="1" \
        "$CLIENT"
) >"$STDOUT_LOG" 2>"$STDERR_LOG" &
CLIENT_PID=$!

deadline=$((SECONDS + 60))
while kill -0 "$CLIENT_PID" >/dev/null 2>&1; do
    if [ "$SECONDS" -ge "$deadline" ]; then
        echo "[PERF_BASELINE] Client timed out." >&2
        exit 1
    fi
    sleep 0.2
done

set +e
wait "$CLIENT_PID"
status=$?
set -e
CLIENT_PID=""
if [ "$status" -ne 0 ]; then
    sed -n '1,240p' "$STDERR_LOG" >&2
    echo "[PERF_BASELINE] Client failed with status $status." >&2
    exit 1
fi
if [ ! -s "$SUMMARY" ] || [ ! -s "$FRAMES" ]; then
    echo "[PERF_BASELINE] Missing summary or frame samples." >&2
    exit 1
fi

head_commit="$(git -C "$ROOT_DIR" rev-parse HEAD)"
source_diff_hash="$(git -C "$ROOT_DIR" diff -- src premake | shasum -a 256 | awk '{print $1}')"
if git -C "$ROOT_DIR" diff --quiet -- src premake; then
    build_id="$head_commit"
else
    build_id="${head_commit}+dirty-${source_diff_hash:0:12}"
fi

gpu="$(system_profiler SPDisplaysDataType | awk -F': ' '/Chipset Model:/ {print $2; exit}')"
gpu="${gpu:-unknown}"
os_version="$(sw_vers -productVersion)"
os_build="$(sw_vers -buildVersion)"
binary_arch="$(lipo -archs "$CLIENT" 2>/dev/null | tr ' ' ',')"
binary_arch="${binary_arch:-$(uname -m)}"
manifest_hash="$(shasum -a 256 "$ROOT_DIR/media/resource-manifest.txt" | awk '{print $1}')"
vsync="$(awk -F= '$1 == "VSync" {print tolower($2); exit}' "$BIN_DIR/Mine.cfg")"
case "$vsync" in
    yes|true|on|1) vsync="on" ;;
    *) vsync="off" ;;
esac
window="$(awk '$1 == "windowsize" {print $2 "x" $3; exit}' "$BIN_DIR/config.txt")"
window="${window:-unknown}"
fullscreen="$(awk '$1 == "fullscreen" {print $2; exit}' "$BIN_DIR/config.txt")"
fullscreen="${fullscreen:-unknown}"
render_distance="$(awk '$1 == "renderdistance" {print $2; exit}' "$BIN_DIR/config.txt")"
render_distance="${render_distance:-unknown}"
fov="$(awk '$1 == "fov" {print $2; exit}' "$BIN_DIR/config.txt")"
fov="${fov:-unknown}"
save_format="$(awk '$1 == "version" {print $2; exit}' "$SAVE_DIR/world.meta")"
save_format="${save_format:-unknown}"

{
    printf 'comparison_schema=2\n'
    printf 'comparison_scene_id=%s\n' "$SCENE_ID"
    printf 'comparison_platform=macos\n'
    printf 'comparison_architecture=%s\n' "$binary_arch"
    printf 'comparison_build_id=%s\n' "$build_id"
    printf 'comparison_gpu=%s\n' "$gpu"
    printf 'comparison_driver=macOS-%s-%s\n' "$os_version" "$os_build"
    printf 'comparison_vsync_regime=%s\n' "$vsync"
    printf 'comparison_window=%s\n' "$window"
    printf 'comparison_fullscreen=%s\n' "$fullscreen"
    printf 'comparison_fov=%s\n' "$fov"
    printf 'comparison_resource_manifest_sha256=%s\n' "$manifest_hash"
    printf 'comparison_resource_packs=none\n'
    printf 'comparison_world_fixture=seed-20260809-position-3038_66_1922-time-6000\n'
    printf 'comparison_save_format=%s\n' "$save_format"
    printf 'comparison_storage_class=local-default\n'
    printf 'comparison_render_distance=%s\n' "$render_distance"
} >>"$SUMMARY"

require_key() {
    local key="$1"
    if ! grep -E "^${key}=.+$" "$SUMMARY" >/dev/null; then
        echo "[PERF_BASELINE] Summary is missing $key." >&2
        exit 1
    fi
}

for key in \
    build_configuration frame_p95_ms frame_p99_ms frames_over_50ms \
    simulation_tick_hz last_loaded_chunks last_sections \
    comparison_schema comparison_scene_id comparison_platform \
    comparison_architecture comparison_build_id comparison_gpu \
    comparison_driver comparison_vsync_regime comparison_window \
    comparison_fullscreen comparison_fov \
    comparison_resource_manifest_sha256 comparison_world_fixture \
    comparison_save_format comparison_storage_class \
    comparison_render_distance; do
    require_key "$key"
done

if ! grep -F 'build_configuration=Release' "$SUMMARY" >/dev/null; then
    echo "[PERF_BASELINE] The captured client is not a Release build." >&2
    exit 1
fi

echo "[PERF_BASELINE] status=PASS summary=$SUMMARY"
grep -E '^(frames|avg_fps|frame_p95_ms|frame_p99_ms|frames_over_33ms|frames_over_50ms|simulation_tick_hz|last_loaded_chunks|last_sections|comparison_.*)=' "$SUMMARY" || true
