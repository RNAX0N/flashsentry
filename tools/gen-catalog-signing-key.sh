#!/usr/bin/env bash
# Generate a catalog signing key (maintainers only). Public key is committed to resources/.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GNUPGHOME="${GNUPGHOME:-$ROOT/tests/fixtures/catalog-signing/gnupg}"
export GNUPGHOME
mkdir -p "$GNUPGHOME"
chmod 700 "$GNUPGHOME"
BATCH="$(mktemp)"
cat > "$BATCH" <<'EOF'
%no-protection
Key-Type: RSA
Key-Length: 2048
Name-Real: FlashSpartan Catalog Signing
Name-Email: catalog@flashspartan.test
Expire-Date: 0
EOF
gpg --batch --pinentry-mode loopback --generate-key "$BATCH"
gpg --batch --armor --export catalog@flashspartan.test > "$ROOT/resources/iso-catalog/catalog-signing.pub"
rm -f "$BATCH"
echo "Wrote resources/iso-catalog/catalog-signing.pub"
echo "Run tools/sign-embedded-manifest.sh after editing embedded-manifest.json"
