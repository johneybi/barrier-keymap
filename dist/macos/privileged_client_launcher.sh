#!/bin/sh

set -eu

bundle_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
binary="$bundle_dir/input-leapc-vhid"
helper="$bundle_dir/input-leapc-root-helper.sh"
state_dir="${TMPDIR:-/tmp}/InputLeapKeymap-${USER:-user}"
helper_pid_file="$state_dir/vhid-helper.pid"
socket_path="$state_dir/vhid.sock"
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
    rm -f "$helper_pid_file" "$socket_path"
    rmdir "$lock_dir" 2>/dev/null || true
}

shell_quote() {
    printf "'%s'" "$(printf '%s' "$1" | sed "s/'/'\\\\''/g")"
}

helper_command="$(shell_quote "$helper") $(shell_quote "$helper_pid_file") $(shell_quote "$binary")"
helper_command="$helper_command --karabiner-vhid-helper $(shell_quote "$socket_path") $(shell_quote "$(id -u)")"
client_pid=""
auth_pid=""

start_helper() {
    /usr/bin/osascript - "$helper_command" <<'APPLESCRIPT'
on run argv
    do shell script ((item 1 of argv) & " >/dev/null 2>&1 &") with administrator privileges
end run
APPLESCRIPT
}

stop_helper() {
    if [ -f "$helper_pid_file" ]; then
        helper_child_pid="$(cat "$helper_pid_file" 2>/dev/null || true)"
        case "$helper_child_pid" in
            ''|*[!0-9]*) ;;
            *)
                /usr/bin/osascript - "$helper_child_pid" <<'APPLESCRIPT' >/dev/null 2>&1 || true
on run argv
    do shell script "/bin/kill -TERM " & quoted form of (item 1 of argv) with administrator privileges
end run
APPLESCRIPT
                ;;
        esac
    fi
}

stop_client() {
    if [ -n "$client_pid" ]; then
        kill -TERM "$client_pid" 2>/dev/null || true
    fi
    if [ -n "$auth_pid" ]; then
        kill -TERM "$auth_pid" 2>/dev/null || true
    fi
    stop_helper
    cleanup
}

trap 'stop_client; exit 143' INT TERM
trap stop_client EXIT

# The client starts before the authorization dialog. This keeps mouse,
# clipboard, and the regular keyboard fallback usable while the VHID helper
# is waiting for authentication.
export INPUTLEAP_VHID_HELPER_SOCKET="$socket_path"
"$binary" "$@" &
client_pid=$!

# The VHID driver needs administrator privileges, but this prompt must not
# block the user-session client above.
start_helper >/dev/null 2>&1 &
auth_pid=$!

client_status=0
wait "$client_pid" || client_status=$?
if kill -0 "$auth_pid" 2>/dev/null; then
    kill "$auth_pid" 2>/dev/null || true
fi

exit "$client_status"
