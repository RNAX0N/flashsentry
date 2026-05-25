# FlashSentry - CLAUDE.md

Developer reference for the FlashSentry codebase. End-user documentation: [docs/USER_GUIDE.md](docs/USER_GUIDE.md), [docs/VERIFICATION.md](docs/VERIFICATION.md), [README.md](README.md).

## Project Overview

FlashSentry is a Qt6 USB security application for Arch Linux. It:

1. **Monitors** removable block devices (libudev)
2. **Verifies** content via configurable profiles (recommended: **watch manifests** / **ISO**)
3. **Optionally** hashes entire partitions (advanced)
4. **Mounts** via UDisks2 + polkit with safe defaults

**Product defaults:** ISO auto-verify on USB mount **on**; full-partition hash on connect **off**; default profile **watch manifest**.

## Technology Stack

| Component | Technology |
|-----------|------------|
| Language | C++20 |
| GUI | Qt6 (Core, Gui, Widgets, DBus, Concurrent, Network) |
| Device monitoring | libudev |
| Cryptography | OpenSSL (EVP) |
| ISO OpenPGP | `gpg` subprocess, isolated homedir under cache |
| Mount | UDisks2 / polkit |
| Build | CMake 3.20+ |

## Project Structure

```
flashsentry/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── MainWindow.cpp              # USB UI, settings, device flow
│   ├── MainWindow_verify_ext.cpp   # Manifest + ISO module wiring
│   ├── DeviceMonitor.cpp
│   ├── HashWorker.cpp              # Raw partition hash
│   ├── RawDeviceHash.cpp           # Shared read/hash for helper
│   ├── flashsentry-read-helper.cpp # polkit elevated reads
│   ├── MerkleTree.cpp
│   ├── ManifestService.cpp         # Build/verify watch groups
│   ├── ManifestWorker.cpp          # Async manifest jobs
│   ├── WatchListDialog.cpp
│   ├── IsoCatalog.cpp              # Publisher URL + key metadata
│   ├── IsoChecksum.cpp             # Parse SHA256SUMS / single-line .sha256
│   ├── IsoVerifier.cpp             # ISO hash + remote + gpg
│   ├── IsoVerifierWorker.cpp
│   ├── IsoVerifierWidget.cpp
│   ├── DatabaseManager.cpp
│   ├── MountManager.cpp
│   ├── DeviceCard.cpp
│   ├── TrayIcon.cpp
│   ├── SettingsDialog.cpp
│   └── StyleManager.cpp
├── include/                        # Headers mirror src/
├── docs/
│   ├── USER_GUIDE.md
│   └── VERIFICATION.md
├── packaging/
│   ├── PKGBUILD
│   ├── org.flashsentry.policy.in
│   └── ...
└── tests/
    ├── test_merkle.cpp
    └── ...
```

## Verification Modes

| Mode | Entry point | Storage |
|------|-------------|---------|
| Watch manifest | `startDeviceVerification()` → `ManifestWorker` | `DeviceRecord.watchManifest` |
| Full partition | `startHashing()` → `HashWorker` | `DeviceRecord.hash` |
| Hybrid | Manifest then `PendingHashAction::RunFullHashAfterManifest` | Both |
| ISO | `IsoVerifier::verifyMountPoint()` / `IsoVerifierWidget` | Results only (not whitelisted) |

See [docs/VERIFICATION.md](docs/VERIFICATION.md) for algorithms.

## Key Components

### DeviceMonitor
- Dedicated `QThread`, `poll()` on udev fd
- USB block partitions; emits `deviceConnected` / `deviceDisconnected` / `deviceChanged`

### ManifestService / ManifestWorker
- `buildGroup()` / `verifyManifest()` on mount paths
- `ManifestWorker` runs jobs on `QtConcurrent`, signals `manifestCompleted` etc.

### IsoVerifier / IsoCatalog / IsoChecksum
- `IsoCatalog::matchIso()` → publisher URLs + trusted fingerprints
- `IsoChecksum::parseSha256Content()` → SUMS file or 64-hex line parsing
- HTTP fetch (Qt Network) + `gpg --homedir` in `~/.cache/FlashSentry/iso-verify/`
- `verifyMountPoint()` for automation on USB mount (`triggerIsoVerificationOnMount`)

### HashWorker / RawDeviceHash
- Full-device hashing; may use `flashsentry-read-helper` via polkit (`FLASHSENTRY_READ_HELPER_PATH`)

### DatabaseManager
- `canonicalUniqueId()` → `DeviceInfo::partitionUniqueId()`
- `updateWatchManifest()`, `setVerificationProfile()`
- JSON at `~/.config/flashsentry/devices.json`, atomic write, mode 600

### MainWindow
- `m_appModeStack`: USB splitter vs `IsoVerifierWidget`
- `startDeviceVerification()` routes by `VerificationProfile`
- `applyAppModule()` switches stacked UI

## Database Format (devices.json)

```json
{
  "version": "1.0",
  "devices": [
    {
      "unique_id": "SERIAL_Vendor_Model_sdb1",
      "hash": "optional_full_partition_sha256",
      "verification_profile": "watch_manifest",
      "watch_manifest": {
        "groups": [
          {
            "id": "docs",
            "name": "Documents",
            "watch_paths": ["Documents"],
            "merkle_root": "abc...",
            "files": []
          }
        ],
        "manifest_root": "def..."
      },
      "trust_level": 1,
      "device_info": { }
    }
  ]
}
```

## Configuration (QSettings)

File: `~/.config/FlashSentry/FlashSentry.conf`

| Key | Default | Notes |
|-----|---------|-------|
| `general/appModule` | `usb_monitor` | or `iso_verifier` |
| `security/defaultVerificationProfile` | `watch_manifest` | |
| `security/autoHashOnConnect` | `false` | Full partition |
| `iso/autoVerifyOnUsbMount` | `true` | ISO automation |
| `iso/autoVerify` | `true` | After folder scan |
| `hashing/algorithm` | SHA256 | Full partition only |

Types: `include/Types.h` (`AppSettings`, `VerificationProfile`, `WatchManifest`, `IsoVerifyResult`).

## Signal Flow (USB + watch manifest)

1. `DeviceMonitor::deviceConnected`
2. `MainWindow::handleNewDevice` / `handleKnownDevice`
3. `startDeviceVerification` → mount if needed → `ManifestWorker::startVerify`
4. `onManifestCompleted` → mount or `handleManifestMismatch`
5. Hybrid → `startHashing` after manifest pass

## Signal Flow (ISO)

1. `MountManager::mountCompleted` → `triggerIsoVerificationOnMount`
2. `IsoVerifierWidget::verifyMountPoint` → `IsoVerifierWorker`
3. `IsoVerifier::verifyMountPoint` per `.iso`
4. UI table + `verificationReportReady` / log

## Building

```bash
sudo pacman -S qt6-base qt6-tools cmake base-devel openssl pkgconf

mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j$(nproc)
```

Runtime: `gpg` recommended for ISO; network for publisher fetch.

```bash
cmake -DFLASHSENTRY_BUILD_TESTS=ON ..
ctest --output-on-failure
```

Package:

```bash
cd packaging && makepkg -si
```

## Command Line

```
flashsentry [--minimized] [--debug] [--no-tray] [--force] [--config PATH]
```

## Threading

| Thread / pool | Work |
|---------------|------|
| Main | Qt GUI, D-Bus |
| DeviceMonitor | udev poll |
| QtConcurrent | HashWorker, ManifestWorker, IsoVerifierWorker |
| DatabaseManager | `QReadWriteLock`; `markModified()` may queue `save()` |

## Adding a Publisher (ISO)

Edit `src/IsoCatalog.cpp`: regex, checksum/signature URLs, `signingKeyIds`, `trustedFingerprints`. Document in [docs/VERIFICATION.md](docs/VERIFICATION.md).

## Adding a Theme

1. `StyleManager::Theme` enum
2. Palette in `StyleManager.cpp` `s_themePalettes`
3. `themeName()` / `availableThemes()`

## Security Notes

- Polkit for helper and mount; not setuid GUI
- ISO: trust publisher fingerprints in catalog; TOFU for unknown keys fails closed on fingerprint mismatch
- Manifest: only watches explicit paths; unrelated stick changes are invisible by design
- Full partition: detects any byte change; requires `storage` group

## License

MIT — see LICENSE.
