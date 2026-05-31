#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ACTION=""

if [ "$#" -gt 0 ] && [[ "$1" != --* ]]; then
    ACTION="$1"
    shift
fi

if ! command -v premake5 >/dev/null 2>&1; then
    echo "premake5 not found. Install with: brew install premake"
    exit 1
fi

if [ -z "$ACTION" ]; then
    case "$(uname -s)" in
        Darwin) ACTION="xcode4" ;;
        Linux) ACTION="gmake" ;;
        *) ACTION="vs2022" ;;
    esac
fi

cd "$ROOT_DIR/premake"
premake5 --file=premake.lua "$@" "$ACTION"

echo "Premake generated $ACTION files under $ROOT_DIR/build/."
