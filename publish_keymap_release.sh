#!/usr/bin/env sh

set -eu

echo "publish_keymap_release.sh is retained as a compatibility wrapper." >&2
echo "Use publish_keystitch_release.sh with a keystitch-v* tag." >&2
exec "$(dirname "$0")/publish_keystitch_release.sh" "$@"
