#!/usr/bin/env bash
# Sync packaging/PKGBUILD pkgver= with the repo VERSION file.
set -euo pipefail

root="$(cd -- "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version_file="${root}/VERSION"
pkgbuild="${root}/packaging/PKGBUILD"

if [[ ! -f "${version_file}" ]]; then
  echo "ERROR: ${version_file} not found" >&2
  exit 1
fi

pkgver="$(tr -d '[:space:]' < "${version_file}")"
if [[ ! "${pkgver}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "ERROR: VERSION must look like MAJOR.MINOR.PATCH, got: ${pkgver}" >&2
  exit 1
fi

sed -i "s/^pkgver=.*/pkgver=${pkgver}/" "${pkgbuild}"
echo "Updated ${pkgbuild} -> pkgver=${pkgver}"
echo "For release tarballs: FLASHSPARTAN_RELEASE=1 makepkg -g >> packaging/PKGBUILD  # or updpkgsums"
