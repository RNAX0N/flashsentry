#!/usr/bin/env bash
# Sign resources/iso-catalog/embedded-manifest.json for release builds.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MANIFEST="$ROOT/resources/iso-catalog/embedded-manifest.json"
HASH_FILE="${MANIFEST}.sha256"
ASC_FILE="${MANIFEST}.asc"
KEY_ID="${FLASHSENTRY_CATALOG_KEY_ID:-catalog@flashsentry.test}"

sha256sum "$MANIFEST" | awk '{print $1}' > "$HASH_FILE"
gpg --batch --pinentry-mode loopback --local-user "$KEY_ID" \
    --detach-sign --armor -o "$ASC_FILE" "$MANIFEST"
echo "Updated $HASH_FILE and $ASC_FILE"
