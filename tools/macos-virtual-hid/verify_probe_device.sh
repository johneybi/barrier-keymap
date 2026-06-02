#!/usr/bin/env sh

set -eu

vendor_id=4617
product_id=19266
src_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
probe=${BARRIER_VHID_PROBE:-/tmp/barrier_virtual_hid_keyboard_probe}
karabiner_cli="/Library/Application Support/org.pqrs/Karabiner-Elements/bin/karabiner_cli"
probe_args=

usage() {
    echo "usage: $0 [--send-test-key]"
    echo
    echo "Builds and runs the Barrier VirtualHID keyboard probe with sudo,"
    echo "then checks whether Karabiner sees vendor_id=$vendor_id product_id=$product_id."
}

case "${1:-}" in
    "")
        ;;
    "--send-test-key")
        probe_args="--send-test-key"
        ;;
    "-h"|"--help")
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

if [ ! -x "$karabiner_cli" ]; then
    echo "missing karabiner_cli: $karabiner_cli" >&2
    exit 1
fi

"$src_dir/build_probe.sh" "$probe" >/dev/null

echo "Enabling Karabiner Modify events entry for Barrier input keyboard..."
"$src_dir/enable_karabiner_modify_events.py" >/dev/null

echo "Requesting sudo so the probe can access Karabiner's root-only VirtualHID socket..."
sudo -v

probe_log=$(mktemp "${TMPDIR:-/tmp}/barrier-vhid-probe.XXXXXX.log")
probe_pid=

cleanup() {
    if [ -n "$probe_pid" ]; then
        sudo kill "$probe_pid" >/dev/null 2>&1 || true
        wait "$probe_pid" 2>/dev/null || true
    fi
    rm -f "$probe_log"
}

trap cleanup EXIT INT TERM

echo "Starting probe..."
if [ -n "$probe_args" ]; then
    sudo "$probe" "$probe_args" >"$probe_log" 2>&1 &
else
    sudo "$probe" >"$probe_log" 2>&1 &
fi
probe_pid=$!

sleep 3

if ! kill -0 "$probe_pid" >/dev/null 2>&1; then
    echo "probe exited early:" >&2
    sed -n '1,120p' "$probe_log" >&2
    exit 1
fi

echo "Probe output:"
sed -n '1,120p' "$probe_log"

echo
echo "Checking Karabiner connected devices..."
devices=$("$karabiner_cli" --list-connected-devices)

printf '%s\n' "$devices" | python3 -c '
import json
import sys

vendor_id = int(sys.argv[1])
product_id = int(sys.argv[2])
devices = json.load(sys.stdin)

matches = []
karabiner_outputs = []

for device in devices:
    identifiers = device.get("device_identifiers", {})
    if (
        identifiers.get("vendor_id") == vendor_id
        and identifiers.get("product_id") == product_id
        and identifiers.get("is_keyboard") is True
    ):
        matches.append(device)

    if (
        identifiers.get("vendor_id") == 1452
        and identifiers.get("product_id") == 591
        and identifiers.get("is_keyboard") is True
    ):
        karabiner_outputs.append(device)

print(f"Barrier input virtual keyboard matches: {len(matches)}")
for device in matches:
    print(json.dumps(device, ensure_ascii=False, indent=2))

print(f"Karabiner output virtual keyboard matches: {len(karabiner_outputs)}")

if not matches:
    sys.exit(1)
' "$vendor_id" "$product_id"

echo
echo "PASS: Karabiner sees the Barrier input virtual keyboard as a distinct device."
