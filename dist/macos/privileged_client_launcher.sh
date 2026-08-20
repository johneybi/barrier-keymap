#!/bin/sh

set -eu

bundle_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
binary="$bundle_dir/input-leapc-vhid"
helper="$bundle_dir/input-leapc-root-helper.sh"
state_dir="${TMPDIR:-/tmp}/InputLeapKeymap-${USER:-user}"
pid_file="$state_dir/client.pid"
lock_dir="$state_dir/client.lock"

if [ ! -x "$binary" ] || [ ! -x "$helper" ]; then
    echo "Input Leap VHID client files are missing" >&2
    exit 1
fi

mkdir -p "$state_dir"
chmod 700 "$state_dir"

# A second GUI instance must not create another root client with the same
# screen name. The atomic directory creation gives us a simple per-user lock.
if ! mkdir "$lock_dir" 2>/dev/null; then
    exit 0
fi

cleanup() {
    rm -f "$pid_file"
    rmdir "$lock_dir" 2>/dev/null || true
}

shell_quote() {
    printf "'%s'" "$(printf '%s' "$1" | sed "s/'/'\\\\''/g")"
}

command="$(shell_quote "$helper") $(shell_quote "$pid_file") $(shell_quote "$binary")"
for argument in "$@"; do
    command="$command $(shell_quote "$argument")"
done

stop_client() {
    if [ -f "$pid_file" ]; then
        client_pid="$(cat "$pid_file" 2>/dev/null || true)"
        if [ -n "$client_pid" ]; then
            /usr/bin/osascript - "$client_pid" <<'APPLESCRIPT' >/dev/null 2>&1 || true
on run argv
    do shell script "/bin/kill -TERM " & quoted form of (item 1 of argv) with administrator privileges
end run
APPLESCRIPT
        fi
    fi
    cleanup
}

trap 'stop_client; exit 143' INT TERM
trap cleanup EXIT

/usr/bin/osascript - "$command" <<'APPLESCRIPT'
on run argv
    do shell script (item 1 of argv) with administrator privileges
end run
APPLESCRIPT

cleanup
