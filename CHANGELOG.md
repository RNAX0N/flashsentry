# Changelog

All notable changes to FlashSpartan are documented in this file.

## [Unreleased]

### Added

- **`--capture-screenshots`** — export real UI PNGs for documentation (`docs/images/`).
- **README** — rewritten for v1.5.4 accuracy; screenshots from the live app.

## [1.5.2] - 2026-06-02

### Added

- **Windows USBPcap download** — BadUSB Monitor → **Download USBPcap** downloads and launches the official installer, polls until `USBPcapCMD.exe` is detected, then enables packet capture without restarting the app.

### Fixed

- **Windows hashing** — no longer ejects the USB drive before verification; dismounts the volume in place when possible, then reads via `\\.\PhysicalDriveN` / UAC helper.
- **Windows device UI** — Open drive / Eject labels on cards and context menus; Mount removed where it does not apply.
- **Windows drive identity** — allow/block and multi-partition logic use physical-drive keys (`\\.\PhysicalDriveN`).
- **Mount status** — volume plug/unplug updates device cards via `mountStatusChanged`.
- **Windows `setup.exe`** — Burn bundle installs only FlashSpartan (no bundled USBPcap step), so the bootstrapper cannot fail on USBPcap driver install.
- **Windows USB Monitor** — lists storage volumes, HID/security keys, and other USB devices (hubs, power/chargers, generic USB).
- **Windows USB detection** — include USB mass-storage volumes reported as fixed disks; hotplug via `WM_DEVICECHANGE`.
- **Windows copy** — settings/about/autostart strings no longer reference Arch-only setup where inappropriate.

### Changed

- **USBPcap** — not bundled in installers; optional in-app download on BadUSB Monitor; app detects presence and shows a clear notice when capture is unavailable.

## [1.5.1] - 2026-05-21

### Fixed

- **Windows USB detection** — include USB mass-storage volumes that Windows reports as fixed disks (`IOCTL_STORAGE_QUERY_PROPERTY` bus type), not only `DRIVE_REMOVABLE`.
- **Windows hotplug** — rescan on `WM_DEVICECHANGE` when volumes are added or removed.
- **Windows installer** — USBPcap is optional and off by default in MSI and setup.exe; USBPcap setup failures no longer block FlashSpartan install (`Return="ignore"`).
- **Windows first-run copy** — welcome wizard and About page no longer show Arch-only `storage` group / `gsettings` guidance.

## [1.5.0] - 2026-06-01

### Fixed

- GitHub links and ISO catalog `remote_url` point at the existing **RNAX0N/flashsentry** repository (product name is FlashSpartan).
- Config paths consolidated via `AppPaths::configDir()` for policy, verify history, timeline, and hash checkpoints.

### Added

- [docs/MIGRATION-FROM-FLASHSENTRY.md](docs/MIGRATION-FROM-FLASHSENTRY.md) — upgrade guide from FlashSentry-branded releases.

### Added

- **GitHub Releases for Windows** — each `v*` tag publishes `FlashSpartan-x.y.z-x64-setup.exe`, `.msi`, and portable `.zip` on the [Releases](https://github.com/RNAX0N/flashsentry/releases) page.
- **Windows WiX installers** — MSI + Burn setup.exe with optional USBPcap; see [packaging/windows/INSTALLER.md](packaging/windows/INSTALLER.md).

### Changed

- **Product rename: FlashSentry → FlashSpartan** — first release under the new name on GitHub (**v1.5.0**); **v1.1.4 and earlier** remain the last FlashSentry-branded release tags. Application config under `~/.config/FlashSpartan`, binaries (`flashspartan`, `FlashSpartan.exe`), packaging, and documentation. Existing FlashSentry settings and data are migrated on first launch when possible.

### Added (since 1.4.2)

- **Windows 10/11 preview build** — CMake cross-compile with MSVC; removable-volume monitoring via `QStorageInfo`; read-only mount status; registry autostart; in-process policy store; CI portable ZIP. See [docs/WINDOWS.md](docs/WINDOWS.md).
- **Platform capabilities API** (`Platform.h`) — runtime feature flags per OS.

### Added (smarter hashing)

- **Partition vs whole-disk** target when a USB stick has multiple partitions (hash options dialog + Settings defaults)
- **Scan modes**: full read, quick sample (spaced 1 MiB chunks), or watch-folders-only (Merkle manifest)
- **Cancel** on device cards during hashing; **ETA** and GiB progress on card and status bar
- **Resume checkpoints** for full scans (`~/.config/FlashSpartan/hash-checkpoints.json`, 64 MiB blocks)

### Added

- **Verify history** — persistent sidebar log (`verify-history.json`) for partition hash, watch-folder manifest, and ISO scan results; device-card click filters history and opens the ISO verify tab
- **First-run wizard** — multi-page setup: security preset, `storage` group check, desktop automount guidance
- **Security preset descriptions** in Settings and wizard
- [docs/SCREENSHOTS.md](docs/SCREENSHOTS.md) — how to capture real UI screenshots for documentation

### Changed

- **Paranoid** preset strengthened: auto hash on connect/eject, ISO verify on mount and scan, block on failures, single parallel verify job

## [1.4.2] - 2026-05-21

### Added

- **ISO verify tab**: empty state (plug in USB flash drive), verification **profile** picker, table row → report scroll
- **Device cards**: show last image verification summary (`Images: N/M passed`)

### Changed

- Settings profile id **`multi_image`** replaces legacy **`ventoy`** (auto-migrated in config)
- Profile display name: **Multi-image USB**

## [1.4.1] - 2026-05-21

### Changed

- User-facing copy reframed for **any** image-on-USB workflow (`dd`, Rufus, copy, multiboot) — not Ventoy-specific
- Settings profile label: **Multi-image USB** (later renamed to id `multi_image` in 1.4.2)
- Docs/README describe multiboot coexistence as one compatibility case among others

## [1.4.0] - 2026-05-21

### Added

- **Header mode tabs**: switch between **USB devices** and **ISO verify** without opening Settings
- ISO tab UI refresh: summary pass/fail/sidecar chips, color-coded results table, vertical splitter (results + report)
- **Multiboot layout badge** when a known stick layout is detected on the scan path
- **More** menu groups export, catalog, and trust-hash actions; primary **Verify images** action
- Status bar catalog control opens the ISO verify tab (clickable)

### Changed

- ISO verify intro text shortened; path field grouped in a card
- Progress bar shows file name and index during multi-image runs

## [1.3.3] - 2026-05-21

### Added

- **Multiboot compatibility** (`IsoScanRules`): skip `ventoy/`, `EFI/`, Easy2Boot `_ISO/`, and other reserved trees when scanning
- Ventoy data-partition detection and coexistence notes in scan reports
- Auto ISO verify when the **desktop already mounted** the stick (Ventoy-friendly; no remount fight)
- Skip auto-verify on small EFI-only Ventoy boot partitions
- Tests: `test_iso_scan_rules`, ventoy fixture with reserved `ventoy/` tree

### Changed

- **Ventoy** settings profile: prefer offline sidecars, no block-on-failure, no full-disk hash on connect
- [VERIFICATION.md](docs/VERIFICATION.md) and [USER_GUIDE.md](docs/USER_GUIDE.md): Ventoy / Easy2Boot coexistence tables

## [1.3.2] - 2026-05-21

### Added

- Status bar **ISO catalog** indicator with integrity tooltip
- Persistent warning banner on the ISO verification tab when embedded catalog checks fail
- GUI report export includes **JSON** format

### Fixed

- Embedded catalog OpenPGP verify ignores a stale or missing `GNUPGHOME` (uses `--keyring` only)
- `test_iso_catalog` clears invalid `GNUPGHOME` before integrity checks

### Changed

- `IsoCatalogManifest::integrityStatusText()` and granular SHA-256 / GPG status accessors
- [VERIFICATION.md](docs/VERIFICATION.md): documents `--json`, `--quiet`, and audit log tray action

## [1.3.1] - 2026-05-21

### Added

- CLI: `--json` and `--quiet` for verify, export, catalog update, list-publishers, and trust-hash
- `IsoVerifyReport::buildJson()` for machine-readable verification output
- Tray menu: **Open audit log** (opens `~/.config/FlashSpartan/audit.log`)
- GUI warning when embedded ISO catalog integrity (SHA-256 / OpenPGP) fails at startup
- Determinate progress bar during multi-file ISO verification

## [1.3.0] - 2026-05-21

### Added

- **OpenPGP-signed embedded catalog**: `embedded-manifest.json.asc` + `catalog-signing.pub`; SHA-256 and GPG checked at load
- **`IsoHttpClient`**: injectable HTTP layer for tests (`IsoHttpClient::setHandler`)
- **Modular ISO catalog**: `src/iso_catalog/` (`IsoCatalogBuilders`, `IsoCatalogMatch`, `IsoCatalogUtil`)
- **Tests**: `test_iso_verify_publisher_mock` (local SHA256SUMS + `.gpg` + trusted key), `test_iso_http_mock`
- **Docs screenshots**: `docs/images/main-window.png`, `iso-verify-report.png`, `watch-lists.png`
- **Tools**: `tools/sign-embedded-manifest.sh`, `tools/gen-catalog-signing-key.sh`

### Changed

- Manifest drop-ins support `signature_url_template`, `signing_key_ids`, `trusted_fingerprints`
- `importPublisherKeys` uses local keyring before contacting a keyserver
- `tools/validate-iso-manifest.py` verifies OpenPGP signature when `gpg` is available

## [1.2.2] - 2026-05-21

### Added

- Integration test `test_iso_verify_integration` with offline fixture tree (Ventoy layout, sidecars, user TOFU)
- Data-driven `publisherFilenameTable` test for 12 core distros
- `IsoVerifier::findChecksumSidecar()` / `findSignatureSidecar()` as public helpers

### Changed

- `FLASHSPARTAN_SKIP_REMOTE_CATALOG` env skips network manifest refresh (used in tests)

## [1.2.1] - 2026-05-21

### Added

- CLI: `--list-publishers`, `--trust-hash file:hex`, `--report-format` for `--export-report`
- `IsoVerifySettingsLoader` — CLI reads `iso/*` settings from FlashSpartan.conf (respects `-c`)
- `IsoCatalog::isVerifiableImageFileName()` for shared extension checks
- Packaged `iso-catalog.d` README and example fragment under `/usr/share/flashspartan/`

### Changed

- `IsoVerifyReport::countSummary()` shared by GUI tray summaries and report text
- VERIFICATION.md documents parallel verify, manifest drop-ins, audit log, and CLI

## [1.2.0] - 2026-05-21

### Added

- CLI: `--verify-iso`, `--verify-mount`, `--verify-dir`, `--update-catalog`, `--export-report`
- Embedded ISO catalog: remote refresh, `iso-catalog.d` drop-ins, user TOFU hashes, manifest SHA-256 integrity check
- Parallel image verification, session hash cache, optional `.img.xz` decompress verify (`xz`)
- Audit log (`audit.log`), CSV/HTML report export, welcome wizard, settings profiles (Default, Ventoy, Work USB, Paranoid)
- Tray notifications and device card status for ISO verify results; optional block mount on verify failure

### Changed

- ISO verify worker supports cancel and per-file progress

## [1.1.6] - 2026-05-26

### Added

- **Microsoft Windows** verification via embedded manifest (`resources/iso-catalog/embedded-manifest.json`) with optional remote catalog refresh
- SBC / ARM images: **Raspberry Pi OS** (`.img.xz`/`.zip`), **Ubuntu for Raspberry Pi**, **Armbian** (local `.img.xz.sha` sidecar)
- **Alpine Linux**, **Void Linux**, **NixOS** catalog entries
- USB scan discovers `.iso`, `.img.xz`, `.img`, and `.zip` images (Ventoy / Pi Imager layouts)

### Changed

- Windows: known `Win11_24H2_English_x64.iso` hash in manifest; other `Win11_*` / `Win10_*` names use hint + `.sha256` sidecar
- Docs: Windows manifest workflow and SBC publisher table (30 catalog IDs)

## [1.1.5] - 2026-05-21

### Added

- ISO publisher catalog: **Kali**, **Rocky Linux**, **AlmaLinux**, **Pop!_OS**, **EndeavourOS**, **CentOS Stream**, **elementary OS**, **Garuda Linux**, **CachyOS**, **Nobara Linux**
- Ubuntu flavors: **Kubuntu**, **Xubuntu**, **Lubuntu**, **Ubuntu MATE**, **Ubuntu Studio**
- Parse Rocky/Alma `CHECKSUM` parenthesis format (`SHA256 (file.iso) = …`)
- Local sidecar lookup for `CHECKSUM` and `{iso}.CHECKSUM`
- Unit tests for new catalog entries and Rocky checksum parsing

### Changed

- Parse Nobara `.sha256sum` lines with `./` relative paths; local `.sha256sum` sidecars
- [README.md](README.md), [docs/USER_GUIDE.md](docs/USER_GUIDE.md), [docs/VERIFICATION.md](docs/VERIFICATION.md): 23 automatic publishers

## [1.1.4] - 2026-05-25

### Added

- `IsoChecksum` helper and `test_iso_checksum` for SHA-256 SUMS parsing
- GitHub pull request template

### Changed

- Application version from CMake `PROJECT_VERSION` (fixes stale `1.0.0` in CLI and About dialog)
- Compiler warning cleanup; six unit tests in CI

## [1.1.3] - 2026-05-24

### Added

- **Manjaro** ISO publisher (per-file `.sha256` / `.sig` on download.manjaro.org)
- Unit test `test_autostart` for XDG login autostart enable/disable

### Fixed

- Parse single-line `.iso.sha256` checksum files (Manjaro and local sidecars)
- OpenPGP verify per-ISO `.sig` when `perFileArtifacts` is set on catalog entry

### Changed

- README: CI and release badges; Ventoy / multi-ISO documentation
- [VERIFICATION.md](docs/VERIFICATION.md): publisher table and `perFileArtifacts`

## [1.1.2] - 2026-05-24

### Added

- ISO publisher catalog: **Linux Mint**, **openSUSE Leap**, **openSUSE Tumbleweed**
- Unit tests for `IsoCatalog` filename matching (`test_iso_catalog`)

### Fixed

- `IsoVerifierWorker` stores `QFuture` from `QtConcurrent::run` (silences `-Wunused-result`)
- Local sidecar detection for `sha256sum.txt` / `sha256sum.txt.gpg` (Linux Mint layout)

## [1.1.1] - 2026-05-24

### Added

- **Login autostart** — Settings → General → “Start automatically at login” uses the systemd user unit when installed, or an XDG autostart entry otherwise
- `docs/images/` placeholder for future UI screenshots

### Changed

- CMake installs `flashspartan.service` to `share/systemd/user`

## [1.1.0] - 2026-05-24

### Added

- **Automatic ISO verification** — detect `.iso` on mounted USB, download publisher checksums, verify OpenPGP and key fingerprints (Arch, Ubuntu, Debian, Fedora)
- **ISO Verify application mode** — dedicated UI with reports
- **Watch-folder verification** — Merkle-backed manifest for selected paths on a volume
- **Verification profiles** — watch manifest (default), full partition, hybrid
- **Watch list editor** per device with baseline build
- Publisher catalog (`IsoCatalog`) and isolated GPG cache under `~/.cache/FlashSpartan/iso-verify/`
- Documentation: [docs/USER_GUIDE.md](docs/USER_GUIDE.md), [docs/VERIFICATION.md](docs/VERIFICATION.md)

### Changed

- **Defaults:** ISO auto-verify on USB mount enabled; full-partition hash on connect disabled
- Default USB verification profile is watch manifest (not full raw hash)
- README and developer docs rewritten for the new workflows
- `gnupg` listed as optional package dependency for ISO verification

### Technical

- Qt6 Network for HTTPS checksum fetch
- `ManifestWorker`, `IsoVerifierWorker`, `MainWindow_verify_ext.cpp`
- Partition-aware device IDs in database
- `flashspartan-read-helper` for privileged raw reads (polkit)

## [1.0.0] - Initial release

- USB device monitoring via libudev
- Full-partition SHA-256/SHA-512/BLAKE2b hashing
- Device whitelist and tamper alerts
- Qt6 UI with themes and system tray
- UDisks2 mount integration and polkit
