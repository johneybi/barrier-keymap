#!/usr/bin/env sh

set -eu

src_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${1:-/tmp/barrier_iohid_user_device_probe}

clang++ -std=c++17 -Wall -Wextra -Werror \
    "$src_dir/barrier_iohid_user_device_probe.cpp" \
    -framework CoreFoundation \
    -framework IOKit \
    -o "$out"

if [ -n "${BARRIER_SIGN_IDENTITY:-}" ]; then
    codesign --force --sign "$BARRIER_SIGN_IDENTITY" \
        --entitlements "$src_dir/iohid-user-device-probe.entitlements" \
        "$out"
fi

echo "$out"
