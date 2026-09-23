#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
output_dir="$project_root/build/exploration-atlas-check"
mkdir -p "$output_dir"

architecture=()
if [[ "$(uname -s)" == "Darwin" ]]; then
    architecture=(-arch x86_64)
fi

for configuration in Debug Release; do
    optimisation=(-g)
    if [[ "$configuration" == "Release" ]]; then
        optimisation=(-O3 -DNDEBUG)
    fi
    binary="$output_dir/exploration-atlas-test-$configuration"
    "${CXX:-clang++}" -std=c++17 "${architecture[@]}" \
        "${optimisation[@]}" -Wall -Wextra -Werror \
        "$project_root/tools/tests/exploration_atlas_test.cpp" -o "$binary"
    "$binary"
done
