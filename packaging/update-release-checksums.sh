#!/usr/bin/env bash
# Download the GitHub release tarball for VERSION and print sha256sums lines for PKGBUILD.
set -euo pipefail

root="$(cd -- "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version_file="${root}/VERSION"
pkgver="$(tr -d '[:space:]' < "${version_file}")"
archive="flashspartan-${pkgver}.tar.gz"
url="https://github.com/RNAX0N/flashsentry/archive/refs/tags/v${pkgver}.tar.gz"
tmpdir="$(mktemp -d)"
trap 'rm -rf "${tmpdir}"' EXIT

echo "Downloading v${pkgver} from GitHub..." >&2
curl -fsSL "${url}" -o "${tmpdir}/${archive}"
sum="$(sha256sum "${tmpdir}/${archive}" | awk '{print $1}')"

cat <<EOF
# Paste into packaging/PKGBUILD when FLASHSPARTAN_RELEASE=1:
source=(
    "${archive}::${url}"
)
sha256sums=('${sum}')
EOF
