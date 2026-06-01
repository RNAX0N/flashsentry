#!/usr/bin/env bash
# Build and install FlashSpartan from the git tree without makepkg.
# Use when makepkg/pkgver issues block packaging, or for quick dev installs.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build"

for cmd in cmake ninja gcc g++; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    : # optional; cmake picks a generator
  fi
done

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake not found. On Arch: sudo pacman -S cmake base-devel qt6-base openssl pkgconf" >&2
  exit 1
fi

echo "==> Configuring in ${BUILD}"
cmake -S "${ROOT}" -B "${BUILD}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DFLASHSPARTAN_BUILD_TESTS=ON \
  -Wno-dev

echo "==> Building"
cmake --build "${BUILD}" --parallel

if [ -f "${BUILD}/CTestTestfile.cmake" ]; then
  echo "==> Running tests"
  ctest --test-dir "${BUILD}" --output-on-failure
fi

echo "==> Installing (sudo)"
sudo cmake --install "${BUILD}"

if [ -f "${ROOT}/packaging/config.json.default" ]; then
  sudo install -Dm644 "${ROOT}/packaging/config.json.default" /etc/flashspartan/config.json
fi

if [ -f "${ROOT}/packaging/flashspartan.service" ]; then
  sudo install -Dm644 "${ROOT}/packaging/flashspartan.service" \
    /usr/lib/systemd/user/flashspartan.service
fi

for size in 16 32 48 64 128 256; do
  icon="${ROOT}/resources/icons/flashspartan-${size}.png"
  if [ -f "${icon}" ]; then
    sudo install -Dm644 "${icon}" \
      "/usr/share/icons/hicolor/${size}x${size}/apps/flashspartan.png"
  fi
done

echo "==> Done. Enable with: systemctl --user enable --now flashspartan.service"
