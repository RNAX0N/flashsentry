#!/usr/bin/env bash
# Build and optionally install the flashsentry package from the surrounding git tree.
set -euo pipefail
cd "$(dirname "$0")"

if ! command -v makepkg >/dev/null 2>&1; then
    echo "makepkg not found. Install base-devel on Arch: sudo pacman -S base-devel" >&2
    exit 1
fi

if [ ! -d ../.git ]; then
    echo "Expected a git checkout at $(cd .. && pwd)" >&2
    exit 1
fi

export FLASHSENTRY_RELEASE="${FLASHSENTRY_RELEASE:-0}"
echo "==> Building from local git (FLASHSENTRY_RELEASE=${FLASHSENTRY_RELEASE})"
exec makepkg "$@"
