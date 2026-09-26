#!/usr/bin/env bash
set -euo pipefail

# Frozen B7 same-host protocol. Each timed sample is a fresh process, and the
# generator timer covers only the bounded v23-affected chunk set. Build work,
# hashing, geometry accounting, and report generation happen outside it.

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
COMPARATOR="$ROOT_DIR/tools/compare_adventure_underground_generation.py"
TARGET_NAME="HelloMine3DWorldRuntimeSmoke"
CURRENT_STEP="argument validation"
OUTPUT_DIR=""
CAPTURE_COUNT=0

usage()
{
    echo "Usage: $0 <fresh-evidence-directory>" >&2
}

on_exit()
{
    local status=$?
    if [ "$status" -ne 0 ] && [ -n "$OUTPUT_DIR" ] && [ -d "$OUTPUT_DIR" ]; then
        {
            echo "status=FAIL"
            echo "exit_code=$status"
            echo "completed_processes=$CAPTURE_COUNT"
            echo "failed_step=$CURRENT_STEP"
            echo "Evidence is intentionally preserved; use a new directory to retry."
        } > "$OUTPUT_DIR/FAILED.txt"
    fi
}
trap on_exit EXIT

if [ "$#" -ne 1 ]; then
    usage
    exit 2
fi

for required in c++ file git make premake5 python3; do
    if ! command -v "$required" >/dev/null 2>&1; then
        echo "Required command is unavailable: $required" >&2
        exit 2
    fi
done

OUTPUT_DIR="$(python3 - "$1" <<'PY'
import pathlib
import sys
print(pathlib.Path(sys.argv[1]).expanduser().resolve(strict=False))
PY
)"
case "$OUTPUT_DIR" in
    "$ROOT_DIR"|"$ROOT_DIR/src"|"$ROOT_DIR/src/"*|\
    "$ROOT_DIR/premake"|"$ROOT_DIR/premake/"*|\
    "$ROOT_DIR/scripts"|"$ROOT_DIR/scripts/"*|\
    "$ROOT_DIR/tools"|"$ROOT_DIR/tools/"*)
        echo "Evidence directory must not be inside a source/protocol directory: $OUTPUT_DIR" >&2
        exit 2
        ;;
esac
if [ -e "$OUTPUT_DIR" ]; then
    echo "Evidence directory already exists; previous evidence will not be overwritten: $OUTPUT_DIR" >&2
    exit 2
fi
mkdir -p "$OUTPUT_DIR/build-logs" "$OUTPUT_DIR/binaries" \
    "$OUTPUT_DIR/runs" "$OUTPUT_DIR/geometry"

source_fingerprint()
{
    python3 - "$ROOT_DIR" <<'PY'
import hashlib
import pathlib
import subprocess
import sys

root = pathlib.Path(sys.argv[1])
listed = subprocess.check_output([
    "git", "-C", str(root), "ls-files", "-co", "--exclude-standard", "-z",
    "--", "src/HelloMine3D", "premake", "scripts", "tools",
])
paths = sorted(pathlib.Path(raw.decode("utf-8"))
               for raw in listed.split(b"\0") if raw)
value = hashlib.sha256(b"HelloMine3D-B7-source-v1\0")
for relative in paths:
    absolute = root / relative
    if not absolute.is_file():
        raise SystemExit(f"Fingerprint input is not a regular file: {relative}")
    encoded = relative.as_posix().encode("utf-8")
    payload = absolute.read_bytes()
    value.update(len(encoded).to_bytes(8, "big"))
    value.update(encoded)
    value.update(len(payload).to_bytes(8, "big"))
    value.update(payload)
print(value.hexdigest())
PY
}

file_sha256()
{
    python3 - "$1" <<'PY'
import hashlib
import pathlib
import sys
print(hashlib.sha256(pathlib.Path(sys.argv[1]).read_bytes()).hexdigest())
PY
}

assert_source_unchanged()
{
    local current
    current="$(source_fingerprint)"
    if [ "$current" != "$SOURCE_FINGERPRINT" ]; then
        echo "$current" > "$OUTPUT_DIR/source-fingerprint-mismatch.txt"
        echo "Source/protocol files changed after the frozen fingerprint." >&2
        return 1
    fi
}

assert_quiet_host()
{
    local process_name
    local process_pattern
    local status
    if ! command -v pgrep >/dev/null 2>&1; then
        echo "pgrep is required to exclude overlapping game/build processes." >&2
        return 1
    fi
    for process_name in HelloMine3D "$TARGET_NAME" xcodebuild clang clang++ cc1 make gmake ninja; do
        process_pattern=${process_name//+/[+]}
        if pgrep -x "$process_pattern" >/dev/null 2>&1; then
            echo "Refusing a paired capture while $process_name is already running." >&2
            return 1
        else
            status=$?
            if [ "$status" -ne 1 ]; then
                echo "Unable to inspect running processes with pgrep (status $status)." >&2
                return 1
            fi
        fi
    done
}

record_environment_identity()
{
    python3 - "$OUTPUT_DIR/environment.csv" "$SOURCE_FINGERPRINT" \
        "$DEBUG_BINARY" "$DEBUG_HASH" "$RELEASE_BINARY" "$RELEASE_HASH" <<'PY'
import csv
import hashlib
import pathlib
import platform
import re
import shutil
import subprocess
import sys

output, source_fingerprint, debug_binary, debug_hash, release_binary, \
    release_hash = sys.argv[1:]


def command_path(name):
    value = shutil.which(name)
    if not value:
        raise SystemExit(f"Unable to resolve tool identity: {name}")
    return str(pathlib.Path(value).resolve())


def command_first_line(arguments):
    value = subprocess.check_output(
        arguments, stderr=subprocess.STDOUT, text=True).splitlines()
    if not value or not value[0].strip():
        raise SystemExit(f"Tool emitted no identity: {arguments[0]}")
    return value[0].strip()


def file_sha256(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def binary_architecture(path):
    description = command_first_line(["file", "-b", path])
    if re.search(r"(?:^|[^A-Za-z0-9_])x86_64(?:$|[^A-Za-z0-9_])", description) \
            or "x86-64" in description:
        return "x86_64"
    if re.search(r"(?:^|[^A-Za-z0-9_])(?:arm64|aarch64)(?:$|[^A-Za-z0-9_])",
                 description):
        return "arm64"
    raise SystemExit(f"Unsupported binary architecture: {description}")


actual_debug_hash = file_sha256(debug_binary)
actual_release_hash = file_sha256(release_binary)
if actual_debug_hash != debug_hash or actual_release_hash != release_hash:
    raise SystemExit("Frozen binary changed before environment recording")

cxx = command_path("c++")
premake = command_path("premake5")
make = command_path("make")
row = {
    "schema": "1",
    "source_fingerprint": source_fingerprint,
    "platform_system": platform.system(),
    "platform_release": platform.release(),
    "platform_machine": platform.machine(),
    "host_architecture": command_first_line(["uname", "-m"]),
    "compiler_path": cxx,
    "compiler_version": command_first_line([cxx, "--version"]),
    "compiler_default_target": command_first_line([cxx, "-dumpmachine"]),
    "premake_path": premake,
    "premake_version": command_first_line([premake, "--version"]),
    "make_path": make,
    "make_version": command_first_line([make, "--version"]),
    "configured_architecture": "x86_64",
    "debug_binary_architecture": binary_architecture(debug_binary),
    "release_binary_architecture": binary_architecture(release_binary),
    "debug_binary_sha256": actual_debug_hash,
    "release_binary_sha256": actual_release_hash,
}
fields = (
    "schema", "source_fingerprint", "platform_system", "platform_release",
    "platform_machine", "host_architecture", "compiler_path",
    "compiler_version", "compiler_default_target", "premake_path",
    "premake_version", "make_path", "make_version",
    "configured_architecture", "debug_binary_architecture",
    "release_binary_architecture", "debug_binary_sha256",
    "release_binary_sha256",
)
with pathlib.Path(output).open("w", newline="", encoding="utf-8") as stream:
    writer = csv.DictWriter(stream, fieldnames=fields)
    writer.writeheader()
    writer.writerow(row)
PY
}

run_capture()
{
    local configuration=$1
    local seed=$2
    local round_number=$3
    local version=$4
    local binary=$5
    local binary_hash=$6
    local parent_directory="$OUTPUT_DIR/runs/$configuration/seed-$seed"
    local run_directory="$parent_directory/r${round_number}-v${version}"
    local log_base="$parent_directory/r${round_number}-v${version}"
    mkdir -p "$parent_directory"
    if [ -e "$run_directory" ]; then
        echo "Capture directory unexpectedly exists: $run_directory" >&2
        return 1
    fi
    CURRENT_STEP="capture $configuration seed=$seed round=$round_number version=$version"
    assert_quiet_host
    env \
        LC_ALL=C \
        HELLOMINE3D_WORLD_SMOKE_FOCUS=ADVENTURE_UNDERGROUND_PERF \
        HELLOMINE3D_UNDERGROUND_PERF_DIR="$run_directory" \
        HELLOMINE3D_UNDERGROUND_PERF_VERSION="$version" \
        HELLOMINE3D_UNDERGROUND_PERF_SEED="$seed" \
        HELLOMINE3D_UNDERGROUND_PERF_ROUND="$round_number" \
        HELLOMINE3D_UNDERGROUND_PERF_CONFIG="$configuration" \
        HELLOMINE3D_UNDERGROUND_EXECUTABLE_SHA256="$binary_hash" \
        "$binary" > "$log_base.stdout.log" 2> "$log_base.stderr.log"
    if ! grep -Fq "[UNDERGROUND_GENERATION_PERF] status=CAPTURED" \
        "$log_base.stdout.log"; then
        echo "Capture did not emit its completion marker: $run_directory" >&2
        return 1
    fi
    CAPTURE_COUNT=$((CAPTURE_COUNT + 1))
    printf '%s,%s,%s,%s,%s,%s\n' "$CAPTURE_COUNT" "$configuration" "$seed" \
        "$round_number" "$version" "$binary_hash" \
        >> "$OUTPUT_DIR/capture-order.csv"
}

CURRENT_STEP="recording frozen source identity"
SOURCE_FINGERPRINT="$(source_fingerprint)"
echo "$SOURCE_FINGERPRINT" > "$OUTPUT_DIR/source-fingerprint-before.txt"
git -C "$ROOT_DIR" rev-parse HEAD > "$OUTPUT_DIR/git-head.txt"
git -C "$ROOT_DIR" status --short > "$OUTPUT_DIR/git-status-before.txt"
uname -a > "$OUTPUT_DIR/platform.txt"
if command -v sw_vers >/dev/null 2>&1; then
    sw_vers >> "$OUTPUT_DIR/platform.txt"
fi
{
    echo "sequence,configuration,seed,round,terrain_version,executable_sha256"
} > "$OUTPUT_DIR/capture-order.csv"

CURRENT_STEP="generating gmake projects"
"$ROOT_DIR/scripts/premake.sh" gmake \
    > "$OUTPUT_DIR/build-logs/premake-gmake.log" 2>&1

for configuration in Debug Release; do
    case "$configuration" in
        Debug) make_configuration=debug_x64 ;;
        Release) make_configuration=release_x64 ;;
        *) echo "Internal configuration error" >&2; exit 2 ;;
    esac
    CURRENT_STEP="building $configuration $TARGET_NAME"
    built_binary="$ROOT_DIR/bin/$TARGET_NAME"
    # Both make configurations publish to the same generated bin path. Remove
    # the previous configuration's executable so Make cannot treat a newer
    # Debug link as an up-to-date Release product (or vice versa).
    rm -f "$built_binary"
    make -C "$ROOT_DIR/build" -j2 "config=$make_configuration" "$TARGET_NAME" \
        > "$OUTPUT_DIR/build-logs/$configuration-$TARGET_NAME.log" 2>&1
    frozen_binary="$OUTPUT_DIR/binaries/$TARGET_NAME-$configuration"
    if [ ! -x "$built_binary" ]; then
        echo "Expected build product is unavailable: $built_binary" >&2
        exit 1
    fi
    cp "$built_binary" "$frozen_binary"
    chmod 0555 "$frozen_binary"
    file_sha256 "$frozen_binary" > "$frozen_binary.sha256"
done

CURRENT_STEP="checking frozen inputs before capture"
assert_source_unchanged
assert_quiet_host

DEBUG_BINARY="$OUTPUT_DIR/binaries/$TARGET_NAME-Debug"
RELEASE_BINARY="$OUTPUT_DIR/binaries/$TARGET_NAME-Release"
DEBUG_HASH="$(cat "$DEBUG_BINARY.sha256")"
RELEASE_HASH="$(cat "$RELEASE_BINARY.sha256")"

CURRENT_STEP="recording platform and toolchain identity"
record_environment_identity

# Paired order is deliberately AB / BA / AB for each seed. Keeping the two
# versions adjacent reduces temporal drift without pretending they are one run.
for configuration in Debug Release; do
    if [ "$configuration" = Debug ]; then
        binary=$DEBUG_BINARY
        binary_hash=$DEBUG_HASH
    else
        binary=$RELEASE_BINARY
        binary_hash=$RELEASE_HASH
    fi
    for seed in 42 20260807 239701883; do
        for round_number in 1 2 3; do
            if [ "$round_number" -eq 2 ]; then
                version_order="23 22"
            else
                version_order="22 23"
            fi
            for version in $version_order; do
                run_capture "$configuration" "$seed" "$round_number" \
                    "$version" "$binary" "$binary_hash"
            done
        done
    done
done

if [ "$CAPTURE_COUNT" -ne 36 ]; then
    echo "Frozen protocol expected 36 completed processes, got $CAPTURE_COUNT" >&2
    exit 1
fi

CURRENT_STEP="collecting non-timed Release geometry evidence"
GEOMETRY_DIRECTORY="$OUTPUT_DIR/geometry/Release"
assert_quiet_host
env \
    LC_ALL=C \
    HELLOMINE3D_WORLD_SMOKE_FOCUS=ADVENTURE_UNDERGROUND_GEOMETRY \
    HELLOMINE3D_UNDERGROUND_GEOMETRY_DIR="$GEOMETRY_DIRECTORY" \
    HELLOMINE3D_UNDERGROUND_PERF_CONFIG=Release \
    HELLOMINE3D_UNDERGROUND_EXECUTABLE_SHA256="$RELEASE_HASH" \
    "$RELEASE_BINARY" \
    > "$OUTPUT_DIR/geometry/Release.stdout.log" \
    2> "$OUTPUT_DIR/geometry/Release.stderr.log"
if ! grep -Fq "[UNDERGROUND_GEOMETRY] status=CAPTURED" \
    "$OUTPUT_DIR/geometry/Release.stdout.log"; then
    echo "Geometry capture did not emit its completion marker." >&2
    exit 1
fi

CURRENT_STEP="checking frozen inputs after capture"
source_fingerprint > "$OUTPUT_DIR/source-fingerprint-after.txt"
assert_source_unchanged

CURRENT_STEP="validating and comparing evidence"
python3 "$COMPARATOR" --root "$OUTPUT_DIR" \
    --output "$OUTPUT_DIR/comparison.json" \
    > "$OUTPUT_DIR/comparison.stdout.log" \
    2> "$OUTPUT_DIR/comparison.stderr.log"

CURRENT_STEP="finalizing evidence"
{
    echo "status=PASS"
    echo "completed_processes=$CAPTURE_COUNT"
    echo "paired_order=AB/BA/AB"
    echo "configurations=Debug,Release"
    echo "seeds=42,20260807,239701883"
} > "$OUTPUT_DIR/COMPLETE.txt"
echo "[B7_GENERATION_PROTOCOL] status=PASS evidence=$OUTPUT_DIR"
