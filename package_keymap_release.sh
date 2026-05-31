#!/usr/bin/env sh

set -eu

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <build-dir> <artifact-name> <output-dir>" >&2
    exit 2
fi

build_dir=$1
artifact_name=$2
output_dir=$3
artifact_dir="$output_dir/$artifact_name"

if [ ! -x "$build_dir/bin/barriers" ]; then
    echo "missing executable: $build_dir/bin/barriers" >&2
    exit 1
fi

if [ ! -x "$build_dir/bin/barrierc" ]; then
    echo "missing executable: $build_dir/bin/barrierc" >&2
    exit 1
fi

rm -rf "$artifact_dir"
mkdir -p "$artifact_dir"

cp "$build_dir/bin/barriers" "$artifact_dir/"
cp "$build_dir/bin/barrierc" "$artifact_dir/"
cp doc/barrier.conf.example-remaps "$artifact_dir/"
cp doc/key-remaps.md "$artifact_dir/"
cp doc/release-checklist.md "$artifact_dir/"
cp README.md "$artifact_dir/"
cp LICENSE "$artifact_dir/"

tar -C "$output_dir" -czf "$output_dir/$artifact_name.tar.gz" "$artifact_name"
shasum -a 256 "$output_dir/$artifact_name.tar.gz" > "$output_dir/$artifact_name.tar.gz.sha256"

echo "$output_dir/$artifact_name.tar.gz"
