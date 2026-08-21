#!/bin/sh

set -eu

if [ "$#" -ne 4 ]; then
    echo "usage: $0 INPUT_LEAP_GUI INPUT_LEAPC INPUT_LEAPS OUTPUT_APP" >&2
    exit 2
fi

gui="$1"
client="$2"
server="$3"
app="$4"
contents="$app/Contents"
version="${INPUTLEAP_KEYMAP_VERSION:-3.0.3}"
macdeployqt="${MACDEPLOYQT:-}"
use_vhid="${INPUTLEAP_USE_KARABINER_VHID:-0}"
use_vhid_root="${INPUTLEAP_USE_KARABINER_VHID_ROOT:-0}"
bundle_id="com.johneybi.input-leap-keymap.client"
if [ "$use_vhid" = "1" ]; then
    bundle_id="com.johneybi.input-leap-keymap.vhid"
fi

if [ -z "$macdeployqt" ]; then
    macdeployqt="$(command -v macdeployqt || true)"
fi
if [ -z "$macdeployqt" ]; then
    echo "macdeployqt was not found; set MACDEPLOYQT or add it to PATH" >&2
    exit 1
fi

rm -rf "$app"
mkdir -p "$contents/MacOS" "$contents/Resources"
cp "$gui" "$contents/MacOS/input-leap"
if [ "$use_vhid" = "1" ]; then
    cp "$client" "$contents/MacOS/input-leapc-vhid"
    if [ "$use_vhid_root" = "1" ]; then
        cp "$(dirname "$0")/privileged_client_launcher.sh" \
            "$contents/MacOS/input-leapc"
        cp "$(dirname "$0")/privileged_client_helper.sh" \
            "$contents/MacOS/input-leapc-root-helper.sh"
    else
        cp "$client" "$contents/MacOS/input-leapc"
    fi
else
    cp "$client" "$contents/MacOS/input-leapc"
fi
cp "$server" "$contents/MacOS/input-leaps"
chmod 755 "$contents/MacOS/input-leap" "$contents/MacOS/input-leaps"
if [ "$use_vhid" = "1" ]; then
    if [ "$use_vhid_root" = "1" ]; then
        chmod 755 "$contents/MacOS/input-leapc-vhid" \
            "$contents/MacOS/input-leapc" \
            "$contents/MacOS/input-leapc-root-helper.sh"
    else
        chmod 755 "$contents/MacOS/input-leapc-vhid" "$contents/MacOS/input-leapc"
    fi
else
    chmod 755 "$contents/MacOS/input-leapc"
fi
cp "$(dirname "$0")/bundle/InputLeap.app/Contents/Resources/InputLeap.icns" \
    "$contents/Resources/InputLeap.icns"
cp LICENSE "$contents/Resources/LICENSE.txt"

cat > "$contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>English</string>
    <key>CFBundleDisplayName</key>
    <string>Input Leap Keymap</string>
    <key>CFBundleExecutable</key>
    <string>input-leap</string>
    <key>CFBundleIconFile</key>
    <string>InputLeap.icns</string>
    <key>CFBundleIdentifier</key>
    <string>$bundle_id</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>Input Leap Keymap</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>$version</string>
    <key>CFBundleVersion</key>
    <string>$version</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSLocalNetworkUsageDescription</key>
    <string>Connect to the Input Leap server on your local network.</string>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
EOF

client_binary="$contents/MacOS/input-leapc"
if [ "$use_vhid" = "1" ]; then
    if [ "$use_vhid_root" = "1" ]; then
        client_binary="$contents/MacOS/input-leapc-vhid"
    else
        client_binary="$contents/MacOS/input-leapc"
    fi
fi

"$macdeployqt" "$app" -no-strip \
    -executable="$client_binary" \
    -executable="$contents/MacOS/input-leaps"

# Keep Accessibility permission associated with the app across ad-hoc beta
# updates instead of allowing codesign to use the changing binary cdhash.
codesign --force --deep --sign - \
    --identifier "$bundle_id" \
    --requirements "=designated => identifier \"$bundle_id\"" \
    "$app"
codesign --verify --deep --strict --verbose=2 "$app"
codesign -d -r- "$app" 2>&1 | grep -F "identifier \"$bundle_id\"" >/dev/null
