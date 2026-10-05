#!/usr/bin/env bash
# Link the real per-configuration client objects after a normal client build.
# This oracle creates Ogre scene nodes and software buffers, never a window.
set -euo pipefail

if [[ $# -ne 4 && $# -ne 5 ]]; then
    echo 'Usage: verify_wildlife_renderer.sh Debug|Release repository-root real-world-oracle-file new-output-directory [real-cadence-oracle-file]' >&2
    exit 2
fi
CONFIGURATION="$1"
case "$CONFIGURATION" in Debug|Release) ;; *) echo 'Expected Debug or Release.' >&2; exit 2;; esac
ROOT_DIR="$(cd "$2" && pwd)"
ORACLE_FILE="$3"
OUTPUT_DIR="$4"
CADENCE_FILE="${5:-}"
TEST_SOURCE="$ROOT_DIR/tools/tests/wildlife_renderer_transform_test.cpp"
SCRIPT_SOURCE="$ROOT_DIR/scripts/verify_wildlife_renderer.sh"
OBJECT_DIR="$ROOT_DIR/build/HelloMine3D/obj/x64/$CONFIGURATION/HelloMine3D.build/Objects-normal/x86_64"
if [[ ! -d "$OBJECT_DIR" || ! -f "$ORACLE_FILE" || ! -f "$TEST_SOURCE" || -e "$OUTPUT_DIR" ]]; then
    echo 'Missing built client objects / formal test / World oracle, or output directory already exists.' >&2
    exit 2
fi
if [[ -n "$CADENCE_FILE" && ! -f "$CADENCE_FILE" ]]; then
    echo 'Missing real ActorManager cadence oracle.' >&2
    exit 2
fi

OBJECTS=()
while IFS= read -r object; do
    if [[ "$object" == *.o && "${object##*/}" != OgreMain.o ]]; then OBJECTS+=("$object"); fi
done < <(rg --files --hidden --no-ignore --follow "$OBJECT_DIR" | sort)
LIBRARIES=()
while IFS= read -r library; do
    if [[ "$library" == */lib/x64/"$CONFIGURATION"/*.a ]]; then LIBRARIES+=("$library"); fi
done < <(rg --files --hidden --no-ignore --follow "$ROOT_DIR/build/Engine" "$ROOT_DIR/build/External" | sort)
if [[ ${#OBJECTS[@]} -eq 0 || ${#LIBRARIES[@]} -eq 0 || ! -f "$OBJECT_DIR/OgreActorRenderer.o" ]]; then
    echo 'Incomplete production object or library inventory.' >&2
    exit 2
fi
SOURCE_FILES=("$TEST_SOURCE" "$SCRIPT_SOURCE" "$ORACLE_FILE")
if [[ -n "$CADENCE_FILE" ]]; then SOURCE_FILES+=("$CADENCE_FILE"); fi
while IFS= read -r source; do
    case "$source" in *.h|*.cpp|*.mm) SOURCE_FILES+=("$source");; esac
done < <(rg --files "$ROOT_DIR/src/HelloMine3D" | sort)

mkdir -p "$OUTPUT_DIR"
WILDLIFE_TEST_CXX="${CXX:-clang++}"
uname -a > "$OUTPUT_DIR/platform.txt"
"$WILDLIFE_TEST_CXX" --version >> "$OUTPUT_DIR/platform.txt"
printf '%s\n' "configuration=$CONFIGURATION" "client_object_directory=$OBJECT_DIR" >> "$OUTPUT_DIR/platform.txt"
git -C "$ROOT_DIR" rev-parse HEAD > "$OUTPUT_DIR/source-commit.txt"
git -C "$ROOT_DIR" diff --binary > "$OUTPUT_DIR/tracked-source.diff"
shasum -a 256 "${SOURCE_FILES[@]}" > "$OUTPUT_DIR/sources-sha256.txt"
shasum -a 256 "${OBJECTS[@]}" > "$OUTPUT_DIR/objects-sha256.txt"
shasum -a 256 "${LIBRARIES[@]}" > "$OUTPUT_DIR/libraries-sha256.txt"
printf '%s\n' 'Compile and link exit codes are distinct from assertion failures.' \
    'Source hashes identify text; the caller must rebuild the corresponding client objects first.' \
    'The version-2 oracle contains copied real World/WildlifeActor histories, not authored renderer endpoints.' \
    'Production OgreActorRenderer::sync, scene nodes and actual eight-corner software VBO readback are exercised.' \
    'Double-precision SAT tests 15 candidate axes; existing model overhang is bounded by the same-pose current-segment endpoint envelope.' \
    'The caller retains production-source mutant builds and their designated rejected assertions separately.' \
    'No normal-input, render-window, graphics-context or GPU-draw acceptance is claimed.' \
    > "$OUTPUT_DIR/scope.txt"
if [[ -n "$CADENCE_FILE" ]]; then
    printf '%s\n' \
        'The version-1 cadence oracle adds six actual ActorManager species/activity traces, sampled every real .05-second tick.' \
        'Snapshots arrive by their .05-second cadence at 30/120Hz in three animation strengths, with newly arriving dt-zero freezes.' \
        'Independent published-polyline projection checks ordered L routes, native travel-speed bounds, in-flight appends and bounded history eviction.' \
        'All copied real voxels receive actual-frame same-pose segment-endpoint envelopes and full 15-axis SAT for overlapping candidates.' \
        >> "$OUTPUT_DIR/scope.txt"
fi

finish_receipt()
{
    local status=$?
    trap - EXIT
    shasum -a 256 "${SOURCE_FILES[@]}" > "$OUTPUT_DIR/sources-sha256.after.txt"
    shasum -a 256 "${OBJECTS[@]}" > "$OUTPUT_DIR/objects-sha256.after.txt"
    shasum -a 256 "${LIBRARIES[@]}" > "$OUTPUT_DIR/libraries-sha256.after.txt"
    local unchanged=1
    cmp -s "$OUTPUT_DIR/sources-sha256.txt" "$OUTPUT_DIR/sources-sha256.after.txt" || unchanged=0
    cmp -s "$OUTPUT_DIR/objects-sha256.txt" "$OUTPUT_DIR/objects-sha256.after.txt" || unchanged=0
    cmp -s "$OUTPUT_DIR/libraries-sha256.txt" "$OUTPUT_DIR/libraries-sha256.after.txt" || unchanged=0
    printf '%s\n' "$unchanged" > "$OUTPUT_DIR/inputs-unchanged.txt"
    if [[ "$unchanged" -ne 1 ]]; then
        echo 'Source, oracle, object or library changed during this run.' >&2
        status=1
    fi
    printf '%s\n' "$status" > "$OUTPUT_DIR/overall-exit.txt"
    exit "$status"
}
trap finish_receipt EXIT

# Match the actual client's minimum OS, instead of the shell compiler's default.
# This reads LC_BUILD_VERSION from the verified x86_64 production object.
WILDLIFE_MIN_OS=$(python3 - "$OBJECT_DIR/OgreActorRenderer.o" <<'PY'
import struct, sys
data = open(sys.argv[1], 'rb').read()
if struct.unpack_from('<I', data)[0] != 0xfeedfacf:
    raise SystemExit('Expected a little-endian 64-bit client Mach-O object.')
offset = 32
for _ in range(struct.unpack_from('<I', data, 16)[0]):
    command, size = struct.unpack_from('<II', data, offset)
    if command == 0x32:
        version = struct.unpack_from('<I', data, offset + 12)[0]
        print(f'{version >> 16}.{(version >> 8) & 255}.{version & 255}')
        break
    offset += size
else:
    raise SystemExit('Client object has no minimum OS build-version command.')
PY
)
printf '%s\n' "client_minimum_os=$WILDLIFE_MIN_OS" >> "$OUTPUT_DIR/platform.txt"
OPTIONS=(-std=c++17 -arch x86_64 "-mmacosx-version-min=$WILDLIFE_MIN_OS" -Wall -Wextra -Werror
    -D_LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR -D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION
    -DGLM_ENABLE_EXPERIMENTAL -DOGRE_STATIC_LIB -DOGRE_BUILD_RENDERSYSTEM_GL3PLUS -DFREEIMAGE_LIB
    -I"$ROOT_DIR/src/HelloMine3D"
    -isystem "$ROOT_DIR/src/external/glm" -isystem "$ROOT_DIR/src/Engine/ogre3d/include"
    -isystem "$ROOT_DIR/src/Engine/ogre3d/include/OSX"
    -isystem "$ROOT_DIR/src/Engine/ThirdParty/freeimage/include")
if [[ "$CONFIGURATION" == Debug ]]; then OPTIONS+=(-O0 -g); else OPTIONS+=(-O3 -DNDEBUG); fi

COMMAND=("$WILDLIFE_TEST_CXX" "${OPTIONS[@]}" "$TEST_SOURCE" "${OBJECTS[@]}" "${LIBRARIES[@]}"
    -L/usr/local/lib -L/opt/homebrew/lib
    -framework AudioToolbox -framework Cocoa -framework Carbon -framework IOKit
    -framework Foundation -framework AppKit -framework CoreFoundation -framework OpenGL
    -o "$OUTPUT_DIR/wildlife-renderer-test")
printf '%q ' "${COMMAND[@]}" > "$OUTPUT_DIR/build-command.txt"
printf '\n' >> "$OUTPUT_DIR/build-command.txt"
set +e
"${COMMAND[@]}" > "$OUTPUT_DIR/build.log" 2>&1
WILDLIFE_BUILD_EXIT=$?
set -e
printf '%s\n' "$WILDLIFE_BUILD_EXIT" > "$OUTPUT_DIR/build-exit.txt"
if [[ "$WILDLIFE_BUILD_EXIT" -ne 0 ]]; then cat "$OUTPUT_DIR/build.log"; exit "$WILDLIFE_BUILD_EXIT"; fi
shasum -a 256 "$OUTPUT_DIR/wildlife-renderer-test" > "$OUTPUT_DIR/binary-sha256.txt"
file "$OUTPUT_DIR/wildlife-renderer-test" >> "$OUTPUT_DIR/platform.txt"
TEST_ARGUMENTS=("$ORACLE_FILE" "$OUTPUT_DIR/ogre.log")
if [[ -n "$CADENCE_FILE" ]]; then TEST_ARGUMENTS+=("$CADENCE_FILE"); fi
printf '%q ' "$OUTPUT_DIR/wildlife-renderer-test" "${TEST_ARGUMENTS[@]}" > "$OUTPUT_DIR/test-command.txt"
printf '\n' >> "$OUTPUT_DIR/test-command.txt"
set +e
"$OUTPUT_DIR/wildlife-renderer-test" "${TEST_ARGUMENTS[@]}" \
    > "$OUTPUT_DIR/test.log" 2>&1
WILDLIFE_TEST_EXIT=$?
set -e
printf '%s\n' "$WILDLIFE_TEST_EXIT" > "$OUTPUT_DIR/test-exit.txt"
tail -1 "$OUTPUT_DIR/test.log"
exit "$WILDLIFE_TEST_EXIT"
