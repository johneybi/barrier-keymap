#!/usr/bin/env sh

set -eu

vendor_id=4617
product_id=19266
src_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
probe=${BARRIER_VHID_PROBE:-/tmp/barrier_virtual_hid_keyboard_probe}
karabiner_cli="/Library/Application Support/org.pqrs/Karabiner-Elements/bin/karabiner_cli"
probe_args=
keep_running=0
status_file=${BARRIER_VHID_STATUS_FILE:-/tmp/barrier-vhid-verification-status.json}
log_dir=${BARRIER_VHID_LOG_DIR:-/tmp}

usage() {
    echo "usage: $0 [--send-test-key] [--keep-running] [--status-file PATH] [--log-dir DIR]"
    echo
    echo "Builds and runs the Barrier VirtualHID keyboard probe with sudo,"
    echo "then checks whether Karabiner sees vendor_id=$vendor_id product_id=$product_id."
    echo
    echo "Writes a JSON status file to: $status_file"
}

while [ $# -gt 0 ]; do
    case "$1" in
        "--send-test-key")
            probe_args="--send-test-key"
            ;;
        "--keep-running")
            keep_running=1
            ;;
        "--status-file")
            shift
            if [ $# -eq 0 ]; then
                usage >&2
                exit 2
            fi
            status_file=$1
            ;;
        "--log-dir")
            shift
            if [ $# -eq 0 ]; then
                usage >&2
                exit 2
            fi
            log_dir=$1
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
    shift
done

if [ ! -x "$karabiner_cli" ]; then
    echo "missing karabiner_cli: $karabiner_cli" >&2
    exit 1
fi

status_dir=$(dirname "$status_file")
mkdir -p "$log_dir" "$status_dir"

"$src_dir/build_probe.sh" "$probe" >/dev/null

echo "Enabling Karabiner Modify events entry for Barrier input keyboard..."
"$src_dir/enable_karabiner_modify_events.py" >/dev/null

echo "Requesting sudo so the probe can access Karabiner's root-only VirtualHID socket..."
sudo -v

probe_log=$(mktemp "$log_dir/barrier-vhid-probe.XXXXXX.log")
devices_json=$(mktemp "$log_dir/barrier-vhid-devices.XXXXXX.json")
probe_pid=

cleanup() {
    if [ -n "$probe_pid" ] && [ "$keep_running" -eq 0 ]; then
        sudo kill "$probe_pid" >/dev/null 2>&1 || true
        wait "$probe_pid" 2>/dev/null || true
    fi
    if [ "$keep_running" -eq 0 ]; then
        rm -f "$probe_log" "$devices_json"
    fi
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
printf '%s\n' "$devices" >"$devices_json"

printf '%s\n' "$devices" | python3 -c '
import json
import sys
import time

vendor_id = int(sys.argv[1])
product_id = int(sys.argv[2])
status_file = sys.argv[3]
probe_pid = int(sys.argv[4])
probe_log = sys.argv[5]
devices_json = sys.argv[6]
keep_running = bool(int(sys.argv[7]))
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

status = {
    "checked_at": int(time.time()),
    "barrier_input": {
        "vendor_id": vendor_id,
        "product_id": product_id,
        "matches": matches,
        "match_count": len(matches),
    },
    "karabiner_output": {
        "vendor_id": 1452,
        "product_id": 591,
        "match_count": len(karabiner_outputs),
    },
    "probe": {
        "pid": probe_pid,
        "log": probe_log,
        "kept_running": keep_running,
    },
    "devices_json": devices_json,
    "passed": bool(matches),
}

with open(status_file, "w", encoding="utf-8") as f:
    json.dump(status, f, ensure_ascii=False, indent=2)
    f.write("\n")

if not matches:
    sys.exit(1)
' "$vendor_id" "$product_id" "$status_file" "$probe_pid" "$probe_log" "$devices_json" "$keep_running"

echo
echo "PASS: Karabiner sees the Barrier input virtual keyboard as a distinct device."
echo "Status file: $status_file"

if [ "$keep_running" -eq 1 ]; then
    echo
    echo "Probe is still running as pid $probe_pid."
    echo "Stop it later with: sudo kill $probe_pid"
    trap - EXIT INT TERM
fi
