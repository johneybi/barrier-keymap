#!/usr/bin/env sh

set -eu

repo=${KARABINER_VHID_REPO:-/tmp/Karabiner-DriverKit-VirtualHIDDevice}
src_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(CDPATH= cd -- "$src_dir/../.." && pwd)
out=${1:-/tmp/barrier_virtual_hid_helper}

if [ ! -d "$repo/include" ] || [ ! -d "$repo/vendor/vendor/include" ]; then
    echo "missing Karabiner-DriverKit-VirtualHIDDevice checkout: $repo" >&2
    exit 1
fi

clang++ -std=c++23 -Wall -Werror \
    -I"$project_root/src/lib/platform" \
    -I"$repo/vendor/vendor/include" \
    -I"$repo/include" \
    "$src_dir/barrier_virtual_hid_helper.cpp" \
    -o "$out"

echo "$out"
