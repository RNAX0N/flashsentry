#!/usr/bin/env bash
# Download the GitHub release tarball for VERSION and print or apply sha256sums for PKGBUILD.
set -euo pipefail

apply=0
if [ "${1:-}" = "--apply" ]; then
  apply=1
fi

root="$(cd -- "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
pkgbuild="${root}/packaging/PKGBUILD"
version_file="${root}/VERSION"
pkgver="$(tr -d '[:space:]' < "${version_file}")"
archive="flashspartan-${pkgver}.tar.gz"
url="https://github.com/RNAX0N/flashsentry/archive/refs/tags/v${pkgver}.tar.gz"
tmpdir="$(mktemp -d)"
trap 'rm -rf "${tmpdir}"' EXIT

echo "Downloading v${pkgver} from GitHub..." >&2
curl -fsSL "${url}" -o "${tmpdir}/${archive}"
sum="$(sha256sum "${tmpdir}/${archive}" | awk '{print $1}')"

if [ "${apply}" -eq 1 ]; then
  python3 - "${pkgbuild}" "${sum}" <<'PY'
import pathlib
import re
import sys

path = pathlib.Path(sys.argv[1])
checksum = sys.argv[2]
text = path.read_text(encoding="utf-8")
text, count = re.subn(
    r"sha256sums=\(\s*'SKIP'\s*\)",
    f"sha256sums=('{checksum}')",
    text,
    count=1,
)
if count != 1:
    raise SystemExit("Could not find sha256sums=('SKIP') in PKGBUILD")
path.write_text(text, encoding="utf-8")
print(f"Updated {path} with sha256sums=('{checksum}')")
PY
  exit 0
fi

cat <<EOF
# Paste into packaging/PKGBUILD when FLASHSPARTAN_RELEASE=1, or run:
#   ./packaging/update-release-checksums.sh --apply
source=(
    "${archive}::${url}"
)
sha256sums=('${sum}')
EOF
