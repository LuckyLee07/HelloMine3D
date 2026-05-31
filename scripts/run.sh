#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
EXECUTABLE="$ROOT_DIR/bin/HelloMine3D"

if [ ! -x "$EXECUTABLE" ]; then
    "$ROOT_DIR/scripts/build.sh" "${1:-}"
fi

exec "$EXECUTABLE"
