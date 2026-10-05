#!/usr/bin/env bash
# Build a real Ogre GPU oracle after a normal production client/engine build.
# GPU execution is opt-in and must be run by the authorized root agent.
set -euo pipefail

if [[ $# -ne 3 && $# -ne 4 ]]; then
    echo 'Usage: verify_gl3plus_texture_macos.sh Debug|Release repository-root new-output-directory [--run]' >&2
    exit 2
fi
CONFIGURATION="$1"
case "$CONFIGURATION" in Debug|Release) ;; *) echo 'Expected Debug or Release.' >&2; exit 2;; esac
ROOT_DIR="$(cd "$2" && pwd)"
OUTPUT_DIR="$3"
RUN_GPU=0
if [[ $# -eq 4 ]]; then
    if [[ "$4" != --run ]]; then echo 'The only optional argument is --run.' >&2; exit 2; fi
    RUN_GPU=1
fi
TEST_SOURCE="$ROOT_DIR/tools/validate_gl3plus_texture_macos.cpp"
SCRIPT_SOURCE="$ROOT_DIR/scripts/verify_gl3plus_texture_macos.sh"
CLIENT_OBJECT="$ROOT_DIR/build/HelloMine3D/obj/x64/$CONFIGURATION/HelloMine3D.build/Objects-normal/x86_64/OgreActorRenderer.o"
TEXTURE_OBJECT="$ROOT_DIR/build/Engine/ogre3d_gl3plus/obj/x64/$CONFIGURATION/ogre3d_gl3plus.build/Objects-normal/x86_64/OgreGL3PlusTexture.o"
if [[ ! -f "$TEST_SOURCE" || ! -f "$CLIENT_OBJECT" || ! -f "$TEXTURE_OBJECT" || -e "$OUTPUT_DIR" ]]; then
    echo 'Missing production objects / formal oracle, or output directory already exists.' >&2
    exit 2
fi

LIBRARIES=(
    "$ROOT_DIR/build/Engine/ogre3d_gl3plus/lib/x64/$CONFIGURATION/libogre3d_gl3plus.a"
    "$ROOT_DIR/build/Engine/ogre3d_glsupport/lib/x64/$CONFIGURATION/libogre3d_glsupport.a"
    "$ROOT_DIR/build/Engine/ogre3d/lib/x64/$CONFIGURATION/libogre3d.a"
    "$ROOT_DIR/build/Engine/ThirdParty/freeimage/lib/x64/$CONFIGURATION/libfreeimage.a"
    "$ROOT_DIR/build/Engine/ThirdParty/libjpeg/lib/x64/$CONFIGURATION/liblibjpeg.a"
    "$ROOT_DIR/build/Engine/ThirdParty/libopenjpeg/lib/x64/$CONFIGURATION/liblibopenjpeg.a"
    "$ROOT_DIR/build/Engine/ThirdParty/libpng/lib/x64/$CONFIGURATION/liblibpng.a"
    "$ROOT_DIR/build/Engine/ThirdParty/libraw/lib/x64/$CONFIGURATION/liblibraw.a"
    "$ROOT_DIR/build/Engine/ThirdParty/libtiff4/lib/x64/$CONFIGURATION/liblibtiff4.a"
    "$ROOT_DIR/build/Engine/ThirdParty/openexr/lib/x64/$CONFIGURATION/libopenexr.a"
    "$ROOT_DIR/build/Engine/ThirdParty/freetype/lib/x64/$CONFIGURATION/libogre_freetype.a"
    "$ROOT_DIR/build/Engine/ThirdParty/zzip/lib/x64/$CONFIGURATION/libzzip.a"
    "$ROOT_DIR/build/Engine/ThirdParty/zlib/lib/x64/$CONFIGURATION/libzlib.a")
for library in "${LIBRARIES[@]}"; do
    if [[ ! -f "$library" ]]; then echo "Missing production library: $library" >&2; exit 2; fi
done
SOURCE_FILES=("$TEST_SOURCE" "$SCRIPT_SOURCE"
    "$ROOT_DIR/src/Engine/ogre3d_gl3plus/src/OgreGL3PlusPixelFormat.cpp"
    "$ROOT_DIR/src/Engine/ogre3d_gl3plus/src/OgreGL3PlusTexture.cpp"
    "$ROOT_DIR/src/Engine/ogre3d_gl3plus/src/OgreGL3PlusTextureBuffer.cpp"
    "$ROOT_DIR/src/Engine/ogre3d_gl3plus/src/OgreGL3PlusHardwarePixelBuffer.cpp"
    "$ROOT_DIR/src/Engine/ogre3d_gl3plus/include/OgreGL3PlusTextureBuffer.h"
    "$ROOT_DIR/src/Engine/ogre3d_gl3plus/include/OgreGL3PlusPrerequisites.h"
    "$ROOT_DIR/src/Engine/ogre3d/include/OgrePixelFormatDescriptions.h"
    "$ROOT_DIR/src/Engine/ogre3d/src/OgrePixelFormat.cpp"
    "$ROOT_DIR/src/Engine/ogre3d/src/OgreImage.cpp"
    "$ROOT_DIR/src/Engine/ogre3d/src/OgreTexture.cpp")
OBJECTS=("$CLIENT_OBJECT" "$TEXTURE_OBJECT")

mkdir -p "$OUTPUT_DIR"
GL3PLUS_TEST_CXX="${CXX:-clang++}"
uname -a > "$OUTPUT_DIR/platform.txt"
"$GL3PLUS_TEST_CXX" --version >> "$OUTPUT_DIR/platform.txt"
git -C "$ROOT_DIR" rev-parse HEAD > "$OUTPUT_DIR/source-commit.txt"
git -C "$ROOT_DIR" diff --binary > "$OUTPUT_DIR/tracked-source.diff"
shasum -a 256 "${SOURCE_FILES[@]}" > "$OUTPUT_DIR/sources-sha256.txt"
shasum -a 256 "${OBJECTS[@]}" > "$OUTPUT_DIR/objects-sha256.txt"
shasum -a 256 "${LIBRARIES[@]}" > "$OUTPUT_DIR/libraries-sha256.txt"
printf '%s\n' \
    'The caller must first build the current production client and dependency archives.' \
    'Text hashes and matching object paths alone do not prove a current-source archive build.' \
    'Compile, link and GPU assertion exits are recorded separately; default operation does not launch.' \
    'GPU mode creates a hidden/noActivate 64px native context, bounded 2D textures and one 2x2x2 3D case.' \
    'Actual Ogre allocations, uploads, subuploads, readbacks, resizes, mip levels and texture blits are exercised.' \
    'Independent GLSL texelFetch writes RGBA32F to preserve negative signed-normalized samples.' \
    '565 comparison uses independent native bit samples and measured channel precision, not byte-exact green claims.' \
    'L8 and LA retain the preexisting engine swizzles, including L8 alpha repeating luminance.' \
    'Every consumed GL error remains in the verdict, including startup and fixture resource teardown.' \
    'The 3D partial-layer check is functional GPU evidence, not an instrumentation-based memory-safety proof.' \
    'The direct upload uses a 16-byte std::vector; ASan evidence requires an explicitly instrumented isolated root build and run.' \
    'No GL calls run after native context destruction; unexpected native shutdown exceptions are fatal.' \
    'This diagnostic does not prove ordinary menu/input/gameplay or another platform/GL context.' \
    > "$OUTPUT_DIR/scope.txt"

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
        echo 'A source, reference object or library changed during this run.' >&2
        status=1
    fi
    printf '%s\n' "$status" > "$OUTPUT_DIR/overall-exit.txt"
    exit "$status"
}
trap finish_receipt EXIT

# Read the actual client and backend object deployment versions, then match the
# newer one. This avoids hiding object deployment mismatches in linker warnings.
GL3PLUS_MIN_OS=$(python3 - "${OBJECTS[@]}" <<'PY'
import struct, sys
versions = []
for path in sys.argv[1:]:
    data = open(path, 'rb').read()
    if struct.unpack_from('<I', data)[0] != 0xfeedfacf:
        raise SystemExit('Expected a little-endian 64-bit production Mach-O object: ' + path)
    offset = 32
    for _ in range(struct.unpack_from('<I', data, 16)[0]):
        command, size = struct.unpack_from('<II', data, offset)
        if command == 0x32:
            version = struct.unpack_from('<I', data, offset + 12)[0]
            versions.append((version >> 16, (version >> 8) & 255, version & 255))
            break
        offset += size
    else:
        raise SystemExit('Production object has no minimum OS build-version command: ' + path)
print('.'.join(map(str,max(versions))))
PY
)
printf '%s\n' "configuration=$CONFIGURATION" "minimum_os=$GL3PLUS_MIN_OS" "gpu_run_requested=$RUN_GPU" >> "$OUTPUT_DIR/platform.txt"
OPTIONS=(-std=c++17 -arch x86_64 "-mmacosx-version-min=$GL3PLUS_MIN_OS" -Wall -Wextra -Werror
    -D_LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR -D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION
    -DOGRE_STATIC_LIB -DOGRE_BUILD_RENDERSYSTEM_GL3PLUS
    -isystem "$ROOT_DIR/src/Engine/ogre3d/include"
    -isystem "$ROOT_DIR/src/Engine/ogre3d/include/OSX"
    -isystem "$ROOT_DIR/src/Engine/ogre3d_gl3plus/include"
    -isystem "$ROOT_DIR/src/Engine/ogre3d_glsupport/include")
if [[ "$CONFIGURATION" == Debug ]]; then OPTIONS+=(-O0 -g); else OPTIONS+=(-O3 -DNDEBUG); fi
COMPILE=("$GL3PLUS_TEST_CXX" "${OPTIONS[@]}" -c "$TEST_SOURCE" -o "$OUTPUT_DIR/texture-oracle.o")
printf '%q ' "${COMPILE[@]}" > "$OUTPUT_DIR/compile-command.txt"
printf '\n' >> "$OUTPUT_DIR/compile-command.txt"
set +e
"${COMPILE[@]}" > "$OUTPUT_DIR/compile.log" 2>&1
GL3PLUS_COMPILE_EXIT=$?
set -e
printf '%s\n' "$GL3PLUS_COMPILE_EXIT" > "$OUTPUT_DIR/compile-exit.txt"
if [[ "$GL3PLUS_COMPILE_EXIT" -ne 0 ]]; then cat "$OUTPUT_DIR/compile.log"; exit "$GL3PLUS_COMPILE_EXIT"; fi

LINK=("$GL3PLUS_TEST_CXX" -arch x86_64 "-mmacosx-version-min=$GL3PLUS_MIN_OS"
    "$OUTPUT_DIR/texture-oracle.o" "${LIBRARIES[@]}"
    -framework Cocoa -framework Carbon -framework IOKit -framework Foundation
    -framework AppKit -framework CoreFoundation -framework OpenGL
    -o "$OUTPUT_DIR/gl3plus-texture-oracle")
printf '%q ' "${LINK[@]}" > "$OUTPUT_DIR/link-command.txt"
printf '\n' >> "$OUTPUT_DIR/link-command.txt"
set +e
"${LINK[@]}" > "$OUTPUT_DIR/link.log" 2>&1
GL3PLUS_LINK_EXIT=$?
set -e
printf '%s\n' "$GL3PLUS_LINK_EXIT" > "$OUTPUT_DIR/link-exit.txt"
if [[ "$GL3PLUS_LINK_EXIT" -ne 0 ]]; then cat "$OUTPUT_DIR/link.log"; exit "$GL3PLUS_LINK_EXIT"; fi
shasum -a 256 "$OUTPUT_DIR/texture-oracle.o" "$OUTPUT_DIR/gl3plus-texture-oracle" > "$OUTPUT_DIR/binary-sha256.txt"
file "$OUTPUT_DIR/gl3plus-texture-oracle" >> "$OUTPUT_DIR/platform.txt"
TEST=("$OUTPUT_DIR/gl3plus-texture-oracle" "$OUTPUT_DIR/ogre.log")
printf '%q ' "${TEST[@]}" > "$OUTPUT_DIR/test-command.txt"
printf '\n' >> "$OUTPUT_DIR/test-command.txt"
if [[ "$RUN_GPU" -eq 0 ]]; then
    printf '%s\n' 'NOT_RUN: compilation only; root owns actual GPU execution.' > "$OUTPUT_DIR/test-status.txt"
    exit 0
fi
set +e
"${TEST[@]}" > "$OUTPUT_DIR/test.log" 2>&1
GL3PLUS_TEST_EXIT=$?
set -e
printf '%s\n' "$GL3PLUS_TEST_EXIT" > "$OUTPUT_DIR/test-exit.txt"
tail -1 "$OUTPUT_DIR/test.log"
exit "$GL3PLUS_TEST_EXIT"
