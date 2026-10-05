#!/usr/bin/env bash
# Real production renderer objects, immutable resources, Ogre software VBOs.
# This script never constructs a RenderSystem, window or graphics context.
set -euo pipefail
if [[ $# -ne 3 ]]; then
    echo 'Usage: verify_item_material_binding.sh Debug|Release repository-root new-output-directory' >&2
    exit 2
fi
CONFIGURATION="$1"
case "$CONFIGURATION" in Debug|Release) ;; *) echo 'Expected Debug or Release.' >&2; exit 2;; esac
ROOT_DIR="$(cd "$2" && pwd)"
OUTPUT_DIR="$3"
OBJECT_DIR="$ROOT_DIR/build/HelloMine3D/obj/x64/$CONFIGURATION/HelloMine3D.build/Objects-normal/x86_64"
TEST_SOURCE="$ROOT_DIR/tools/tests/item_material_binding_test.cpp"
SCRIPT_SOURCE="$ROOT_DIR/scripts/verify_item_material_binding.sh"
if [[ ! -d "$OBJECT_DIR" || ! -f "$TEST_SOURCE" || -e "$OUTPUT_DIR" ]]; then
    echo 'Missing fresh normal client objects/test, or output directory already exists.' >&2
    exit 2
fi
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"
OBJECTS=()
while IFS= read -r object; do
    if [[ "$object" == *.o && "${object##*/}" != OgreMain.o ]]; then OBJECTS+=("$object"); fi
done < <(rg --files --hidden --no-ignore --follow "$OBJECT_DIR" | sort)
LIBRARIES=()
while IFS= read -r library; do
    if [[ "$library" == */lib/x64/"$CONFIGURATION"/*.a ]]; then LIBRARIES+=("$library"); fi
done < <(rg --files --hidden --no-ignore --follow "$ROOT_DIR/build/Engine" "$ROOT_DIR/build/External" | sort)
if [[ ${#OBJECTS[@]} -eq 0 || ${#LIBRARIES[@]} -eq 0 ]]; then echo 'Incomplete production linkage.' >&2; exit 2; fi
SOURCES=("$TEST_SOURCE" "$SCRIPT_SOURCE")
while IFS= read -r source; do
    case "$source" in *.h|*.cpp|*.mm) SOURCES+=("$source");; esac
done < <(rg --files "$ROOT_DIR/src/HelloMine3D" | sort)
git -C "$ROOT_DIR" rev-parse HEAD > "$OUTPUT_DIR/source-commit.txt"
git -C "$ROOT_DIR" diff --binary > "$OUTPUT_DIR/tracked-source.diff"
shasum -a 256 "${SOURCES[@]}" > "$OUTPUT_DIR/sources-sha256.txt"
shasum -a 256 "${OBJECTS[@]}" > "$OUTPUT_DIR/objects-sha256.txt"
shasum -a 256 "${LIBRARIES[@]}" > "$OUTPUT_DIR/libraries-sha256.txt"
ITEM_TEST_CXX="${CXX:-clang++}"
uname -a > "$OUTPUT_DIR/platform.txt"
"$ITEM_TEST_CXX" --version >> "$OUTPUT_DIR/platform.txt"
printf '%s\n' "configuration=$CONFIGURATION" "client_objects=$OBJECT_DIR" >> "$OUTPUT_DIR/platform.txt"
printf '%s\n' \
    'Fresh normal client build is required; dependency freshness and actual object/library identities are checked.' \
    'Actual ResourcePackResolver -> runtime profile -> BlockDatabase initialization precedes cache/renderer use.' \
    'Separate default-v2, legal legacy-v1 512/16/32 and368/16/23 processes; singleton/cache state is never changed in-process.' \
    'Actual OgrePlayerRenderer held and OgreActorRenderer ItemEntity drop software VBOs are read through public APIs.' \
    'Independent fixed semantic coordinates and decoded effective PNG alpha perimeter provide expected results.' \
    'Cell identity uses actual float floor(UV*tiles), never exact equality to tile/tiles.' \
    'NEW_HALF_TEXEL assertions are the new safe-origin engineering requirement; pre-fix default failures in this group are not an existing default material misrouting claim.' \
    'No GPU sampling, shader draw, ImGui/inventory/map chain, normal gameplay, or full48-chain PASS is claimed.' \
    > "$OUTPUT_DIR/scope.txt"
finish_receipt()
{
    local status=$?
    trap - EXIT
    shasum -a 256 "${SOURCES[@]}" > "$OUTPUT_DIR/sources-sha256.after.txt"
    shasum -a 256 "${OBJECTS[@]}" > "$OUTPUT_DIR/objects-sha256.after.txt"
    shasum -a 256 "${LIBRARIES[@]}" > "$OUTPUT_DIR/libraries-sha256.after.txt"
    local unchanged=1
    cmp -s "$OUTPUT_DIR/sources-sha256.txt" "$OUTPUT_DIR/sources-sha256.after.txt" || unchanged=0
    cmp -s "$OUTPUT_DIR/objects-sha256.txt" "$OUTPUT_DIR/objects-sha256.after.txt" || unchanged=0
    cmp -s "$OUTPUT_DIR/libraries-sha256.txt" "$OUTPUT_DIR/libraries-sha256.after.txt" || unchanged=0
    if [[ -f "$OUTPUT_DIR/resources-sha256.txt" ]]; then
        python3 - "$ROOT_DIR" "$OUTPUT_DIR" after <<'PY'
import hashlib,json,sys
from pathlib import Path
root,out=map(Path,sys.argv[1:3])
paths=json.loads((out/'resource-inputs.json').read_text())
(out/'resources-sha256.after.txt').write_text(''.join(hashlib.sha256(Path(p).read_bytes()).hexdigest()+'  '+p+'\n' for p in paths))
PY
        cmp -s "$OUTPUT_DIR/resources-sha256.txt" "$OUTPUT_DIR/resources-sha256.after.txt" || unchanged=0
    fi
    printf '%s\n' "$unchanged" > "$OUTPUT_DIR/inputs-unchanged.txt"
    if [[ "$unchanged" -ne 1 ]]; then echo 'Inputs changed during software fixture.' >&2; status=1; fi
    printf '%s\n' "$status" > "$OUTPUT_DIR/overall-exit.txt"
    exit "$status"
}
trap finish_receipt EXIT

# A source newer than its actually linked normal object is a stale baseline,
# never a production assertion result. Dependency files cover inline headers.
python3 - "$OBJECT_DIR" "$ROOT_DIR" > "$OUTPUT_DIR/freshness.log" <<'PY'
import json,re,sys
from pathlib import Path
objects,root=map(Path,sys.argv[1:3])
names=['OgrePlayerRenderer','OgreActorRenderer','BlockDatabase','BlockData','TerrainMaterialProfile','ResourcePackResolver','Material','StartupResourcePreflight','ItemEntity']
receipt=[]
for name in names:
    obj,dep=objects/(name+'.o'),objects/(name+'.d')
    if not obj.is_file() or not dep.is_file(): raise SystemExit('Missing actual normal object/dependencies: '+name)
    text=dep.read_text().replace('\\\n',' ')
    paths=[Path(p.replace('\\ ', ' ')) for p in re.findall(r'/[^\s]+',text)]
    firstparty=[p for p in paths if str(root/'src/HelloMine3D') in str(p)]
    stale=[str(p) for p in firstparty if p.is_file() and p.stat().st_mtime_ns>obj.stat().st_mtime_ns]
    if stale: raise SystemExit('Fresh normal client build required for '+name+': '+','.join(stale))
    receipt.append({'object':str(obj),'dependency_file':str(dep),'firstparty_dependencies':len(firstparty),'fresh_by_dependency_mtime':True})
print(json.dumps(receipt,indent=2))
PY

python3 - "$ROOT_DIR" "$OUTPUT_DIR" <<'PY'
import hashlib,json,sys
from pathlib import Path
from PIL import Image
root,out=map(Path,sys.argv[1:3])
base=Image.open(root/'media/textures/DefaultPack.png').convert('RGBA')
if base.size!=(256,256): raise SystemExit('Expected actual base atlas256x256')
base_values={}
for line in (root/'media/materials/Base.terrain-material').read_text().splitlines():
    if '=' in line: key,value=line.split('=',1); base_values[key]=value
keys=['atlas_texture','atlas_pixels','tile_pixels','tiles_per_row','colour_saturation','green_suppression','green_red_shift','tone_gamma']
packs=[]
for tiles in (32,23):
    pack=out/f'legacy{tiles}-pack'
    packs.append(pack)
    (pack/'media/textures').mkdir(parents=True)
    (pack/'media/materials').mkdir(parents=True)
    (pack/'pack.meta').write_text(f'# HelloMine3D resource pack v1\nname=ItemBindingLegacy{tiles}\nformat=1\n')
    atlas=Image.new('RGBA',(tiles*16,tiles*16),(0,0,0,0))
    # Fixed integer semantic cells retain their physical16px origins.
    atlas.paste(base,(0,0))
    atlas.paste((0,0,0,0),(32,32,48,48))
    for x,y in ((2,3),(9,6),(12,12)): atlas.putpixel((32+x,32+y),(113,171,219,255))
    atlas.save(pack/'media/textures/DefaultPack.png')
    values=dict(base_values,atlas_pixels=str(tiles*16),tile_pixels='16',tiles_per_row=str(tiles))
    (pack/'media/materials/Base.terrain-material').write_text('# HelloMine3D terrain material parameters v1\n'+''.join(k+'='+values[k]+'\n' for k in keys))
paths=[root/'media/resource-manifest.txt']
for line in (root/'media/resource-manifest.txt').read_text().splitlines():
    if line and not line.startswith('#'): paths.append(root/line.split('|',1)[1])
for pack in packs: paths.extend(p for p in pack.rglob('*') if p.is_file())
paths=sorted(set(map(str,paths)))
(out/'resource-inputs.json').write_text(json.dumps(paths,indent=2)+'\n')
(out/'resources-sha256.txt').write_text(''.join(hashlib.sha256(Path(p).read_bytes()).hexdigest()+'  '+p+'\n' for p in paths))
print('[ITEM_BINDING_FIXTURE] real-PNG512x512 and368x368 v1 exact8keys fixed semantic cells, independent three-pixel icon mask')
PY

ITEM_MIN_OS=$(python3 - "$OBJECT_DIR/OgrePlayerRenderer.o" <<'PY'
import struct,sys
data=open(sys.argv[1],'rb').read()
if struct.unpack_from('<I',data)[0]!=0xfeedfacf: raise SystemExit('Expected x86_64 Mach-O object')
offset=32
for _ in range(struct.unpack_from('<I',data,16)[0]):
    command,size=struct.unpack_from('<II',data,offset)
    if command==0x32:
        version=struct.unpack_from('<I',data,offset+12)[0]
        print(f'{version>>16}.{(version>>8)&255}.{version&255}')
        break
    offset+=size
else: raise SystemExit('Missing actual client minimum OS')
PY
)
printf '%s\n' "client_minimum_os=$ITEM_MIN_OS" >> "$OUTPUT_DIR/platform.txt"
OPTIONS=(-std=c++17 -arch x86_64 "-mmacosx-version-min=$ITEM_MIN_OS" -Wall -Wextra -Werror
    -D_LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR -D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION
    -DGLM_ENABLE_EXPERIMENTAL -DOGRE_STATIC_LIB -DOGRE_BUILD_RENDERSYSTEM_GL3PLUS -DFREEIMAGE_LIB
    -I"$ROOT_DIR/src/HelloMine3D" -isystem "$ROOT_DIR/src/external/glm"
    -isystem "$ROOT_DIR/src/Engine/ogre3d/include" -isystem "$ROOT_DIR/src/Engine/ogre3d/include/OSX"
    -isystem "$ROOT_DIR/src/Engine/ThirdParty/freeimage/include")
if [[ "$CONFIGURATION" == Debug ]]; then OPTIONS+=(-O0 -g); else OPTIONS+=(-O3 -DNDEBUG); fi
COMPILE=("$ITEM_TEST_CXX" "${OPTIONS[@]}" -c "$TEST_SOURCE" -o "$OUTPUT_DIR/item-binding.o")
printf '%q ' "${COMPILE[@]}" > "$OUTPUT_DIR/compile-command.txt"; printf '\n' >> "$OUTPUT_DIR/compile-command.txt"
set +e
"${COMPILE[@]}" > "$OUTPUT_DIR/compile.log" 2>&1
ITEM_COMPILE_EXIT=$?
set -e
printf '%s\n' "$ITEM_COMPILE_EXIT" > "$OUTPUT_DIR/compile-exit.txt"
if [[ "$ITEM_COMPILE_EXIT" -ne 0 ]]; then cat "$OUTPUT_DIR/compile.log"; exit "$ITEM_COMPILE_EXIT"; fi
LINK=("$ITEM_TEST_CXX" -arch x86_64 "-mmacosx-version-min=$ITEM_MIN_OS" "$OUTPUT_DIR/item-binding.o" "${OBJECTS[@]}" "${LIBRARIES[@]}"
    -L/usr/local/lib -L/opt/homebrew/lib -framework AudioToolbox -framework Cocoa -framework Carbon
    -framework IOKit -framework Foundation -framework AppKit -framework CoreFoundation -framework OpenGL
    -o "$OUTPUT_DIR/item-binding-test")
printf '%q ' "${LINK[@]}" > "$OUTPUT_DIR/link-command.txt"; printf '\n' >> "$OUTPUT_DIR/link-command.txt"
set +e
"${LINK[@]}" > "$OUTPUT_DIR/link.log" 2>&1
ITEM_LINK_EXIT=$?
set -e
printf '%s\n' "$ITEM_LINK_EXIT" > "$OUTPUT_DIR/link-exit.txt"
if [[ "$ITEM_LINK_EXIT" -ne 0 ]]; then cat "$OUTPUT_DIR/link.log"; exit "$ITEM_LINK_EXIT"; fi
shasum -a 256 "$OUTPUT_DIR/item-binding-test" "$OUTPUT_DIR/item-binding.o" > "$OUTPUT_DIR/binary-sha256.txt"
file "$OUTPUT_DIR/item-binding-test" >> "$OUTPUT_DIR/platform.txt"
ITEM_RESULT=0
for MODE in default legacy32 legacy23; do
    mkdir -p "$OUTPUT_DIR/$MODE"
    RUN=("$OUTPUT_DIR/item-binding-test" "$ROOT_DIR" "$MODE" "$OUTPUT_DIR/$MODE-pack" "$OUTPUT_DIR/$MODE")
    printf '%q ' "${RUN[@]}" > "$OUTPUT_DIR/$MODE/test-command.txt"; printf '\n' >> "$OUTPUT_DIR/$MODE/test-command.txt"
    set +e
    "${RUN[@]}" > "$OUTPUT_DIR/$MODE/test.log" 2>&1
    ITEM_TEST_EXIT=$?
    set -e
    printf '%s\n' "$ITEM_TEST_EXIT" > "$OUTPUT_DIR/$MODE/test-exit.txt"
    tail -1 "$OUTPUT_DIR/$MODE/test.log"
    if [[ "$ITEM_TEST_EXIT" -ne 0 ]]; then ITEM_RESULT=1; fi
done
exit "$ITEM_RESULT"
