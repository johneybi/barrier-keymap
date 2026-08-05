#!/bin/sh

set -eu

usage() {
    echo "usage: $0 <official Barrier.app> <patched barrierc> <output app>" >&2
    exit 2
}

[ "$#" -eq 3 ] || usage

SOURCE_APP=$1
PATCHED_CLIENT=$2
OUTPUT_APP=$3

[ -d "$SOURCE_APP/Contents" ] || {
    echo "error: not a macOS app bundle: $SOURCE_APP" >&2
    exit 1
}

[ -x "$SOURCE_APP/Contents/MacOS/barrier" ] || {
    echo "error: the source bundle does not contain the Barrier GUI" >&2
    exit 1
}

[ -f "$PATCHED_CLIENT" ] || {
    echo "error: patched barrierc not found: $PATCHED_CLIENT" >&2
    exit 1
}

[ ! -e "$OUTPUT_APP" ] || {
    echo "error: output already exists: $OUTPUT_APP" >&2
    exit 1
}

ditto "$SOURCE_APP" "$OUTPUT_APP"
cp "$PATCHED_CLIENT" "$OUTPUT_APP/Contents/MacOS/barrierc"
chmod 755 "$OUTPUT_APP/Contents/MacOS/barrierc"

INFO_PLIST="$OUTPUT_APP/Contents/Info.plist"
plutil -replace CFBundleDisplayName -string "Barrier Keymap" "$INFO_PLIST"
plutil -replace CFBundleName -string "Barrier Keymap" "$INFO_PLIST"
plutil -replace CFBundleIdentifier -string "com.johneybi.barrier-keymap" "$INFO_PLIST"
plutil -replace CFBundleShortVersionString -string "2.4.0-f19" "$INFO_PLIST"
plutil -replace CFBundleVersion -string "2.4.0-f19" "$INFO_PLIST"
plutil -remove NSLocalNetworkUsageDescription "$INFO_PLIST" 2>/dev/null || true
plutil -insert NSLocalNetworkUsageDescription -string \
    "Connect to the Barrier server on your local network." "$INFO_PLIST"

codesign --force --deep --sign - "$OUTPUT_APP"
codesign --verify --deep --strict --verbose=2 "$OUTPUT_APP"

echo "Created $OUTPUT_APP"
echo "Add this exact app to Privacy & Security > Accessibility before launching it."

