#!/usr/bin/env bash
# Build and optionally install the flashspartan package from the surrounding git tree.
set -euo pipefail
cd "$(dirname "$0")"

if [ -x ./sync-pkgver.sh ]; then
    ./sync-pkgver.sh
fi

if ! command -v makepkg >/dev/null 2>&1; then
    echo "makepkg not found. Install base-devel on Arch: sudo pacman -S base-devel" >&2
    echo "Or install without makepkg: ./install-local.sh" >&2
    exit 1
fi

if [ ! -d ../.git ]; then
    echo "Expected a git checkout at $(cd .. && pwd)" >&2
    exit 1
fi

# pkgver() renames src/flashspartan-1.4.2 -> src/flashspartan-1.4.2.rNN.gHASH after
# prepare() rsyncs into flashspartan-1.4.2, leaving CMake an empty directory.
if grep -qE '^pkgver\(\)' PKGBUILD; then
    cat >&2 <<'EOF'
ERROR: packaging/PKGBUILD still defines pkgver().

That breaks local installs: makepkg renames the source tree to
  src/flashspartan-1.4.2.r<commits>.g<hash>
while files stay under src/flashspartan-1.4.2, so CMake reports:
  ... does not appear to contain CMakeLists.txt

Fix:
  git fetch origin main && git checkout main && git pull origin main
  rm -rf pkg src
  ./build-package.sh -si

Bypass makepkg entirely:
  ./install-local.sh
EOF
    exit 1
fi

export FLASHSPARTAN_RELEASE="${FLASHSPARTAN_RELEASE:-0}"
echo "==> Building from local tree (FLASHSPARTAN_RELEASE=${FLASHSPARTAN_RELEASE})"

# Drop stale makepkg artifacts (especially old pkgver() rename dirs)
rm -rf pkg src
shopt -s nullglob
stale=(src/flashspartan-1.4.2.r*.g*)
shopt -u nullglob
if [ "${#stale[@]}" -gt 0 ]; then
    echo "==> Removed stale ${stale[*]}" >&2
fi

exec makepkg "$@"
