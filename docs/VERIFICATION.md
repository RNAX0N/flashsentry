# Verification technical overview

This document describes how FlashSentry verifies data. For step-by-step usage, see [USER_GUIDE.md](USER_GUIDE.md).

## Verification profiles (USB)

Each whitelisted device can use one profile (default for new devices: **watch manifest**).

### Watch manifest

1. User defines **groups** with one or more relative paths on the mount point.
2. **Baseline build** hashes every file under those paths (SHA-256), sorts leaves, builds a **Merkle tree**, stores the root in `devices.json`.
3. **Verify** recomputes the tree and compares roots.
4. Mismatch reports changed, missing, or added paths (per group).

**Properties:**

- Does not read unlisted areas of the stick
- Fast relative to partition size
- Good for “these folders should not change without me knowing”

### Full partition

1. Reads the raw block device (`/dev/sdXN`) via privileged helper / direct read.
2. Computes a single hash (SHA-256, SHA-512, or BLAKE2b).
3. Compares to stored hash in the device record.

**Properties:**

- Detects any byte change anywhere on the partition
- Slow on large drives
- Requires `storage` group membership for unprivileged access patterns

### Hybrid

1. Run watch manifest verification first.
2. On success, optionally queue full partition hash (`RunFullHashAfterManifest`).

## Merkle tree

- **Leaf:** `SHA256(relative_path + content_hash_hex)`
- **Internal nodes:** `SHA256(left_child || right_child)`
- **Leaves sorted** by path for deterministic roots
- **Group root** stored per watch group; **manifest root** hashes group roots

Implementation: `MerkleTree.cpp`, `ManifestService.cpp`.

## ISO verification chain

For each `.iso` file:

```
┌─────────────────┐     HTTPS      ┌──────────────────────┐
│  Publisher      │ ──────────────►│ SHA256SUMS + .sig    │
│  mirror         │                │ (cached locally)     │
└─────────────────┘                └──────────┬───────────┘
                                              │
                                              ▼
┌─────────────────┐     gpg --verify          ┌──────────────────────┐
│  .iso on USB    │ ◄── hash compare ◄───────│ Signature valid?     │
│  (SHA-256)      │                         │ Fingerprint trusted? │
└─────────────────┘                         └──────────────────────┘
```

### Steps (`IsoVerifier.cpp`)

1. **Scan** mount point for `*.iso` (recursive).
2. **Identify publisher** from filename (`IsoCatalog.cpp`).
3. **Download** checksum and signature URLs for that publisher/release.
4. **Hash** local ISO with OpenSSL EVP SHA-256.
5. **Parse** checksum file for the ISO basename.
6. **Import** signing keys via `gpg --homedir ~/.cache/FlashSentry/iso-verify/gnupg --recv-keys`.
7. **Verify** detached signature on the checksum file.
8. **Extract** fingerprint from GPG output; compare to `trustedFingerprints` in catalog.
9. Emit `IsoVerifyResult` with `reportSummary` for the UI.

### Local sidecars

If remote fetch fails or publisher is unknown:

- `*.sha256`, `SHA256SUMS`, `sha256sums.txt` next to ISO
- `*.asc`, `*.sig`, `SHA256SUMS.gpg` for detached signatures

Sidecar PGP on the ISO file itself is also attempted.

### Bootable stick without `.iso`

`scanMountPoint()` sets `looksLikeDdIsoStick` when:

- No `.iso` files found, and
- Typical live layout markers exist (e.g. `.disk/info`, `EFI`, `arch/`)

A informational result is returned instead of a false PASS.

## Device identity

Partition-aware ID (stored in database):

```
{serial}_{vendor}_{model}_{partition}
```

Example: `ABC123_SanDisk_Ultra_sdb1`

Legacy records without partition suffix may still resolve via `legacyUniqueId()` fallback where implemented.

## Database record fields (relevant)

```json
{
  "unique_id": "SERIAL_Vendor_Model_sdb1",
  "hash": "optional_full_partition_hash",
  "verification_profile": "watch_manifest",
  "watch_manifest": { "groups": [...], "manifest_root": "..." },
  "last_manifest_root": "..."
}
```

## Configuration keys (QSettings)

| Key | Purpose |
|-----|---------|
| `general/appModule` | `usb_monitor` / `iso_verifier` |
| `security/defaultVerificationProfile` | `watch_manifest`, `full_partition`, `hybrid` |
| `security/autoHashOnConnect` | Full partition on connect |
| `iso/autoVerify` | ISO scan auto-run |
| `iso/autoVerifyOnUsbMount` | ISO check on mount |
| `iso/scanDirectory` | Default ISO folder |

## Extending publisher support

Add an entry in `IsoCatalog.cpp`:

- Filename `QRegularExpression`
- Checksum and signature URL builders
- `signingKeyIds` for `gpg --recv-keys`
- `trustedFingerprints` (normalized hex, no spaces)

Rebuild and test with a real ISO on a loop mount or USB stick.
