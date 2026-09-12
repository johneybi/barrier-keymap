#!/bin/sh
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
stage=$(mktemp -d /private/tmp/keystitch-ime-lab.XXXXXX)
app="$stage/KeyStitch IME Lab.app"
mkdir -p "$app/Contents/MacOS"
cp "$source_dir/Info.plist" "$app/Contents/Info.plist"
xcrun swiftc -swift-version 5 -module-cache-path "$stage/modules" \
    "$source_dir/main.swift" -framework AppKit -framework WebKit \
    -o "$app/Contents/MacOS/ime-lab"
codesign --sign - "$app"
codesign --verify --strict "$app"
printf '%s\n' "$app"
# Deliberately do not launch it or modify /Applications.
