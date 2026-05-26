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

1. **Scan** mount point recursively for `.iso`, `.img.xz`, `.img`, and `.zip` (`IsoCatalog::isVerifiableImageFileName`).
2. **Identify publisher** from filename (`IsoCatalog.cpp`).
3. **Download** checksum and signature URLs for that publisher/release.
4. **Hash** local ISO with OpenSSL EVP SHA-256.
5. **Parse** checksum file for the ISO basename (`IsoChecksum::parseSha256Content`).
6. **Import** signing keys via `gpg --homedir ~/.cache/FlashSentry/iso-verify/gnupg --recv-keys`.
7. **Verify** detached signature on the checksum file.
8. **Extract** fingerprint from GPG output; compare to `trustedFingerprints` in catalog.
9. Emit `IsoVerifyResult` with `reportSummary` for the UI.

### Local sidecars

If remote fetch fails or publisher is unknown:

- `SHA256SUMS`, `sha256sums.txt`, `sha256sum.txt` (directory-level)
- `{iso}.sha256` or single-line hash (e.g. Manjaro layout)
- `{iso}.sha256sum` (Nobara layout)
- `*.asc`, `*.sig`, `{iso}.sig`, `SHA256SUMS.gpg` for detached signatures

Sidecar OpenPGP on the ISO file itself is also attempted when no checksum-file signature was verified.

### Per-file publisher layout (`perFileArtifacts`)

Some publishers (e.g. **Manjaro**, **elementary OS**, **EndeavourOS**) ship `{iso}.sha256` and `{iso}.sig` (or `.asc`) instead of a combined `SHA256SUMS` file. Catalog entries set `perFileArtifacts = true` so that:

1. The checksum URL points at the single hash file (often one 64-character hex line).
2. GPG verifies the detached `.sig` against the **ISO file**, not against a sums file.

### Ventoy / multi-ISO volumes

`verifyMountPoint()` calls `findIsoFiles()` recursively on the mounted path (`.iso`, `.img.xz`, `.img`, `.zip`). A Ventoy data partition with many images produces **one `IsoVerifyResult` per file**; results are independent (one failure does not block others).

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

## Performance and offline options (1.2+)

| Feature | Behavior |
|---------|----------|
| **Parallel verify** | `iso/verifyParallel` (default 2) — `QtConcurrent` over multiple images on one mount |
| **Hash cache** | Session cache keyed by path, size, mtime (always on for GUI/CLI) |
| **Decompressed `.img.xz`** | `iso/verifyDecompressed` — pipes through `xz -dc` before hashing (needs `xz` in PATH) |
| **Offline-first** | `iso/preferOfflineSidecars` — try local sidecars before HTTPS |
| **Mirror fallbacks** | Arch `geo.` → `mirror.`; Rocky `download.` → `dl.` |

## Manifest extensions (1.2+)

Load order (later overrides earlier for matching filenames):

1. Embedded `embedded-manifest.json` (SHA-256 checked against `.sha256` sidecar at build/CI time)
2. Cached remote manifest (`~/.cache/FlashSentry/iso-catalog-manifest.json`)
3. `/usr/share/flashsentry/iso-catalog.d/*.json`
4. `~/.config/flashsentry/iso-catalog.d/*.json`
5. User TOFU file `~/.config/flashsentry/user-iso-hashes.json`

`IsoCatalogManifest::trustUserHash()` and CLI `--trust-hash file:hex` append to the user TOFU store.

## Audit log and reports

- **Audit:** `~/.config/flashsentry/audit.log` — one JSON object per line after each verify
- **Reports:** `IsoVerifyReport` — plain text, CSV, HTML; CLI `--export-report PATH --report-format csv`

## Command-line interface

```bash
flashsentry --list-publishers
flashsentry --verify-iso /path/to/file.iso
flashsentry --verify-mount /run/media/$USER/Ventoy
flashsentry --verify-dir ~/Downloads/isos
flashsentry --update-catalog
flashsentry --export-report /path --report-format html
flashsentry --trust-hash Win11.iso:41196290521b7e4f814aca30c2cc4c7fab1e3076439418673b90954a1ffc54
```

Exit codes: `0` pass, `1` verify failure, `2` error. ISO options are read from `FlashSentry.conf` (`iso/verifyParallel`, etc.); use `-c /path/to/FlashSentry.conf` to override.

## Configuration keys (QSettings)

| Key | Purpose |
|-----|---------|
| `general/appModule` | `usb_monitor` / `iso_verifier` |
| `security/defaultVerificationProfile` | `watch_manifest`, `full_partition`, `hybrid` |
| `security/autoHashOnConnect` | Full partition on connect |
| `iso/autoVerify` | ISO scan auto-run |
| `iso/autoVerifyOnUsbMount` | ISO check on mount |
| `iso/scanDirectory` | Default ISO folder |
| `iso/verifyParallel` | Max parallel image hashes (default 2) |
| `iso/verifyDecompressed` | Hash decompressed `.img.xz` stream |
| `iso/preferOfflineSidecars` | Prefer local checksum files before download |
| `iso/blockMountOnFailure` | Block mount when verify fails on USB insert |

## Supported automatic publishers

| ID | Filename hint |
|----|----------------|
| `archlinux` | `archlinux-*-x86_64.iso` |
| `ubuntu` | `ubuntu-*-desktop-amd64.iso` |
| `kubuntu` / `xubuntu` / `lubuntu` | `{flavor}-*-desktop-amd64.iso` (cdimage.ubuntu.com) |
| `ubuntu-mate` / `ubuntustudio` | `ubuntu-mate-*`, `ubuntustudio-*` |
| `debian` | `debian-*-amd64*.iso` |
| `fedora` | `Fedora-*-*.iso` |
| `linuxmint` | `linuxmint-{major}-*.iso` |
| `opensuse-leap` | `openSUSE-Leap-*-*-Media.iso` |
| `opensuse-tumbleweed` | `openSUSE-Tumbleweed-*.iso` |
| `manjaro` | `manjaro-{edition}-{version}-*.iso` (per-file `.sha256` / `.sig`) |
| `kali` | `kali-linux-{version}-*.iso` |
| `centos-stream` | `CentOS-Stream-{major}-x86_64*.iso` (`CHECKSUM`, parenthesis format) |
| `rocky` / `almalinux` | `Rocky-*`, `AlmaLinux-*` (`CHECKSUM`) |
| `elementary` | `elementaryos-{version}-amd64.iso` (per-file `.sha256`) |
| `pop-os` | `pop-os_{version}_amd64_*.iso` |
| `endeavouros` | `endeavouros-{date}-x86_64.iso` (GitHub release `.sha256` / `.asc`) |
| `garuda` | `garuda-{edition}-linux-zen-{YYMMDD}.iso` (iso.builds.garudalinux.org) |
| `cachyos` | `cachyos-{variant}-linux-{YYMMDD}.iso` (build.cachyos.org, `.sha256` + `.sig`) |
| `nobara` | `Nobara-{ver}-{edition}-{date}.iso` (`.sha256sum` on nobara-images) |
| `raspios` | `YYYY-MM-DD-raspios-{variant}.img.xz` (downloads.raspberrypi.com) |
| `ubuntu-rpi` | `ubuntu-*-preinstalled-*-arm64+raspi.img.xz` |
| `alpine` | `alpine-*-{version}-{arch}.iso` |
| `voidlinux` | `void-live-*.iso` (`sha256sum.txt` on repo-default.voidlinux.org) |
| `armbian` | `Armbian_*.img.xz` (copy `.img.xz.sha` beside image from armbian.com) |
| `nixos` | `nixos-{channel}-{variant}-x86_64-linux.iso` |
| `microsoft-windows` | `Win11_*` / `Win10_*` / `Windows*.iso` ([embedded manifest](#windows-iso-verification) + sidecars) |

Run `test_iso_catalog` after editing `IsoCatalog.cpp` or `embedded-manifest.json`.

### Integration tests (offline fixtures)

`tests/fixtures/` provides a Ventoy-style tree, per-file `.sha256` sidecars, directory `SHA256SUMS`, and mismatch cases. `test_iso_verify_integration` runs without network when `FLASHSENTRY_SKIP_REMOTE_CATALOG=1` is set (automatic in CI).

```bash
ctest --test-dir build -R test_iso_verify_integration --output-on-failure
```

## Windows ISO verification

Microsoft publishes SHA-256 values on their [Windows 11](https://www.microsoft.com/software-download/windows11) and [Windows 10](https://www.microsoft.com/software-download/windows10) download pages, but not at a stable per-file URL. FlashSentry uses:

1. **Embedded manifest** — shipped at `:/iso-catalog/embedded-manifest.json` (also installed under `share/flashsentry/iso-catalog/`). Includes known hashes (e.g. `Win11_24H2_English_x64.iso`) and `hint_only` patterns for other `Win11_*` / `Win10_*` names. Integrity is checked with **SHA-256** (`embedded-manifest.json.sha256`) and **OpenPGP** (`embedded-manifest.json.asc` + `catalog-signing.pub`). Re-sign after edits: `tools/sign-embedded-manifest.sh`.
2. **Remote refresh** — the manifest’s `remote_url` (GitHub raw) is cached weekly under `~/.cache/FlashSentry/iso-catalog-manifest.json` so hashes can be updated without rebuilding the app.
3. **Local sidecar** — place `Win11_24H2_English_x64.iso.sha256` (single hex line or `hash  filename` format) next to the ISO on the USB stick.

After downloading from Microsoft, copy the hash from the download page into a sidecar file if your exact filename is not in the manifest yet.

## SBC and compressed images

`findIsoFiles()` scans recursively for:

- `*.iso` — PC installers, Ventoy
- `*.img.xz` — Raspberry Pi OS, Ubuntu for Pi, Armbian
- `*.img` — raw images
- `*.zip` — legacy Raspberry Pi OS zip releases

Checksums are computed on the **file as stored** (e.g. the compressed `.img.xz` bytes), matching `sha256sum` on the downloaded artifact.

## Extending publisher support

Add an entry in `IsoCatalog.cpp`:

- Filename `QRegularExpression`
- Checksum and signature URL builders
- `signingKeyIds` for `gpg --recv-keys`
- `trustedFingerprints` (normalized hex, no spaces)
- Set `perFileArtifacts = true` when URLs are `{iso}.sha256` / `{iso}.sig` and GPG signs the ISO

Rebuild and test with a real ISO on a loop mount or USB stick.
