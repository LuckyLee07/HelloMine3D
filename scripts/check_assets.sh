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

if [ -s "$ROOT_DIR/$BLOCK_DATABASE" ]; then
    while IFS= read -r block_name; do
        require_file "block definition" "media/blocks/$block_name.block"
    done < <(sed -nE \
        's/.*addBlock\(BlockId::[A-Za-z0-9_]+, "([^"]+)"\);.*/\1/p' \
        "$ROOT_DIR/$BLOCK_DATABASE")
fi

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
                     for (i = 2; i <= NF && $i != "separateUV"; ++i) {
                         print $i
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
