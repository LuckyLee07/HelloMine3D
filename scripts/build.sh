#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG="debug"

if [ "${1:-}" = "release" ]; then
    CONFIG="release"
fi

"$ROOT_DIR/scripts/premake.sh" gmake

make -C "$ROOT_DIR/build" config="$CONFIG"

echo "Built MineCraft3D ($CONFIG) into $ROOT_DIR/bin/"
