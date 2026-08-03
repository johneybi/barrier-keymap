#!/bin/bash

set -euo pipefail

root_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${1:-$root_dir/build-macos-gui}"
qt_prefix="${QT_PREFIX:-$(brew --prefix qt@5)}"

cmake -S "$root_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBARRIER_BUILD_GUI=ON \
    -DBARRIER_BUILD_TESTS=ON \
    -DBARRIER_BUILD_INSTALLER=ON \
    -DCMAKE_PREFIX_PATH="$qt_prefix" \
    -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR:-$(brew --prefix openssl@3)}" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5

cmake --build "$build_dir" --parallel
"$build_dir/bin/unittests"
"$build_dir/bin/guiunittests"

echo "Application: $build_dir/bundle/Barrier.app"
echo "Disk image:  $build_dir/bundle/Barrier-*.dmg"
