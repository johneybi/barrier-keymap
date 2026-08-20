#!/bin/sh

set -eu

if [ "$#" -lt 2 ]; then
    echo "usage: $0 PID_FILE INPUT_LEAPC [ARGS...]" >&2
    exit 2
fi

pid_file="$1"
binary="$2"
shift 2

child_pid=""
cleanup() {
    rm -f "$pid_file"
}

stop_child() {
    if [ -n "$child_pid" ]; then
        kill -TERM "$child_pid" 2>/dev/null || true
        wait "$child_pid" 2>/dev/null || true
    fi
    cleanup
    exit 143
}

trap cleanup EXIT
trap stop_child INT TERM

"$binary" "$@" &
child_pid=$!
printf '%s\n' "$child_pid" > "$pid_file"
wait "$child_pid"
