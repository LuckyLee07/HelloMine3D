#!/usr/bin/env bash
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ "${1:-}" = "--root" ]; then
    if [ "$#" -ne 2 ]; then
        echo "Usage: $0 [--root <repository-root>]" >&2
        exit 2
    fi
    ROOT_DIR="$(cd "$2" && pwd)"
elif [ "$#" -ne 0 ]; then
    echo "Usage: $0 [--root <repository-root>]" >&2
    exit 2
fi

CHECKED=0
FAILURES=0

require_file() {
    local category="$1"
    local relative_path="$2"
    CHECKED=$((CHECKED + 1))
    if [ ! -s "$ROOT_DIR/$relative_path" ]; then
        echo "[ASSET_CHECK] MISSING $category: $relative_path" >&2
        FAILURES=$((FAILURES + 1))
    fi
}

require_directory() {
    local category="$1"
    local relative_path="$2"
    CHECKED=$((CHECKED + 1))
    if [ ! -d "$ROOT_DIR/$relative_path" ]; then
        echo "[ASSET_CHECK] MISSING $category directory: $relative_path" >&2
        FAILURES=$((FAILURES + 1))
    fi
}

BLOCK_DATABASE="src/HelloMine3D/World/Block/BlockDatabase.cpp"
OGRE_PROGRAM="media/ogre/HelloMine3D.program"
OGRE_MATERIAL="media/ogre/HelloMine3D.material"

require_file "block registry" "$BLOCK_DATABASE"
require_file "Ogre program" "$OGRE_PROGRAM"
require_file "Ogre material" "$OGRE_MATERIAL"
require_file "font" "media/fonts/rs.ttf"
require_file "render config template" "bin/Mine.cfg"
require_file "resource config template" "bin/MineResources.cfg"
require_file "resource-pack config template" "bin/resource-packs.txt"
require_file "startup resource manifest" "media/resource-manifest.txt"
require_file "base smelting registry" "media/smelting/Base.smelting"
require_directory "recipe resources" "media/recipes"
require_directory "objective resources" "media/objectives"
require_directory "audio resources" "media/audio"

while IFS= read -r audio_path; do
    require_file "audio definition" "$audio_path"
done < <(find "$ROOT_DIR/media/audio" -type f -name '*.audio' 2>/dev/null |
    sed "s#^$ROOT_DIR/##" | sort)

while IFS= read -r recipe_path; do
    require_file "recipe" "$recipe_path"
done < <(find "$ROOT_DIR/media/recipes" -type f -name '*.recipe' 2>/dev/null |
    sed "s#^$ROOT_DIR/##" | sort)

while IFS= read -r objective_path; do
    require_file "objective" "$objective_path"
done < <(find "$ROOT_DIR/media/objectives" -type f -name '*.objective' 2>/dev/null |
    sed "s#^$ROOT_DIR/##" | sort)

if [ -s "$ROOT_DIR/$BLOCK_DATABASE" ]; then
    while IFS= read -r block_name; do
        require_file "block definition" "media/blocks/$block_name.block"
    done < <(sed -nE \
        's/.*addBlock\(BlockId::[A-Za-z0-9_]+, "([^"]+)".*/\1/p' \
        "$ROOT_DIR/$BLOCK_DATABASE")
fi

while IFS= read -r shape_name; do
    require_file "block shape" "media/shapes/$shape_name.shape"
done < <(awk '$1 == "Shape" { getline; print $1 }' \
    "$ROOT_DIR"/media/blocks/*.block | tr -d '\r' | sort -u)

if [ -s "$ROOT_DIR/$OGRE_PROGRAM" ]; then
    while IFS= read -r shader_name; do
        require_file "shader" "media/ogre/$shader_name"
    done < <(awk '$1 == "source" { print $2 }' \
        "$ROOT_DIR/$OGRE_PROGRAM" | tr -d '\r')
fi

if [ -s "$ROOT_DIR/$OGRE_MATERIAL" ]; then
    while IFS= read -r texture_name; do
        require_file "texture" "media/textures/$texture_name"
    done < <(awk '$1 == "texture" { print $2 }' \
        "$ROOT_DIR/$OGRE_MATERIAL" | tr -d '\r')

    while IFS= read -r texture_name; do
        require_file "cube texture" "media/textures/$texture_name"
    done < <(awk '$1 == "cubic_texture" {
                     mode = $NF
                     if (mode == "combinedUVW" && NF == 3) {
                         name = $2
                         extension = name
                         sub(/^.*\./, ".", extension)
                         base = name
                         sub(/\.[^.]*$/, "", base)
                         print base "_fr" extension
                         print base "_bk" extension
                         print base "_lf" extension
                         print base "_rt" extension
                         print base "_up" extension
                         print base "_dn" extension
                     }
                     else {
                         for (i = 2; i < NF; ++i) {
                             print $i
                         }
                     }
                 }' "$ROOT_DIR/$OGRE_MATERIAL" | tr -d '\r')
fi

if [ -s "$ROOT_DIR/bin/MineResources.cfg" ]; then
    while IFS= read -r resource_path; do
        resource_path="${resource_path%$'\r'}"
        case "$resource_path" in
            ../*) require_directory "resource location" "${resource_path#../}" ;;
            *) require_directory "resource location" "bin/$resource_path" ;;
        esac
    done < <(sed -nE 's/^[[:space:]]*FileSystem=(.*)$/\1/p' \
        "$ROOT_DIR/bin/MineResources.cfg")
fi

if [ "$FAILURES" -ne 0 ]; then
    echo "[ASSET_CHECK] status=FAIL checked=$CHECKED failures=$FAILURES" >&2
    exit 1
fi

echo "[ASSET_CHECK] status=PASS checked=$CHECKED failures=0"
