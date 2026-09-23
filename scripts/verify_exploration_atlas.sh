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

    markers_binary="$output_dir/exploration-markers-test-$configuration"
    "${CXX:-clang++}" -std=c++17 "${architecture[@]}" \
        "${optimisation[@]}" -Wall -Wextra -Werror \
        "$project_root/tools/tests/exploration_markers_test.cpp" \
        -o "$markers_binary"
    "$markers_binary"

    store_binary="$output_dir/exploration-map-store-test-$configuration"
    "${CXX:-clang++}" -std=c++17 "${architecture[@]}" \
        "${optimisation[@]}" -Wall -Wextra -Werror \
        "$project_root/tools/tests/exploration_map_store_test.cpp" \
        "$project_root/src/HelloMine3D/World/Exploration/ExplorationMapStore.cpp" \
        "$project_root/src/HelloMine3D/World/Storage/StorageTransaction.cpp" \
        -o "$store_binary"
    "$store_binary"
done
