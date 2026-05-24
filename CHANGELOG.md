# Changelog

All notable changes to FlashSentry are documented in this file.

## [Unreleased]

## [1.1.3] - 2026-05-24

### Added

- **Manjaro** ISO publisher (per-file `.sha256` / `.sig` on download.manjaro.org)
- Unit test `test_autostart` for XDG login autostart enable/disable

### Fixed

- Parse single-line `.iso.sha256` checksum files (Manjaro and local sidecars)
- OpenPGP verify per-ISO `.sig` when `perFileArtifacts` is set on catalog entry

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

- CMake installs `flashsentry.service` to `share/systemd/user`

## [1.1.0] - 2026-05-24

### Added

- **Automatic ISO verification** — detect `.iso` on mounted USB, download publisher checksums, verify OpenPGP and key fingerprints (Arch, Ubuntu, Debian, Fedora)
- **ISO Verify application mode** — dedicated UI with reports
- **Watch-folder verification** — Merkle-backed manifest for selected paths on a volume
- **Verification profiles** — watch manifest (default), full partition, hybrid
- **Watch list editor** per device with baseline build
- Publisher catalog (`IsoCatalog`) and isolated GPG cache under `~/.cache/FlashSentry/iso-verify/`
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
- `flashsentry-read-helper` for privileged raw reads (polkit)

## [1.0.0] - Initial release

- USB device monitoring via libudev
- Full-partition SHA-256/SHA-512/BLAKE2b hashing
- Device whitelist and tamper alerts
- Qt6 UI with themes and system tray
- UDisks2 mount integration and polkit
