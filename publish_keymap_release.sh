#!/usr/bin/env sh

set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <keymap-vX.Y.Z[-suffix]>" >&2
    exit 2
fi

tag=$1

case "$tag" in
    keymap-v*) ;;
    *)
        echo "release tag must start with keymap-v: $tag" >&2
        exit 2
        ;;
esac

if [ -n "$(git status --porcelain)" ]; then
    echo "working tree is not clean" >&2
    git status --short >&2
    exit 1
fi

head_sha=$(git rev-parse HEAD)

if git rev-parse -q --verify "refs/tags/$tag" >/dev/null; then
    tag_sha=$(git rev-parse "$tag^{}")
    if [ "$tag_sha" != "$head_sha" ]; then
        echo "local tag $tag points to $tag_sha, not HEAD $head_sha" >&2
        exit 1
    fi
else
    git tag -a "$tag" -m "Barrier keymap fork $tag"
fi

if command -v gh >/dev/null 2>&1; then
    if ! gh auth status >/dev/null 2>&1; then
        echo "GitHub CLI is installed but not authenticated." >&2
        echo "Run: gh auth login" >&2
        exit 1
    fi
fi

git push origin HEAD:master
git push origin "$tag"

echo "pushed master and $tag"
echo "GitHub Actions release workflow should now build release artifacts."
