#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

"$ROOT_DIR/scripts/build.sh"
gdb "$ROOT_DIR/bin/HelloMine3D"
