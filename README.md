# FlashSentry

<p align="center">
  <img src="resources/icons/flashsentry.svg" alt="FlashSentry Logo" width="128" height="128">
</p>

<p align="center">
  <strong>USB security and ISO verification for Arch Linux</strong>
</p>

<p align="center">
  <a href="https://github.com/RNAX0N/flashsentry/actions/workflows/ci.yml"><img src="https://github.com/RNAX0N/flashsentry/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://github.com/RNAX0N/flashsentry/releases"><img src="https://img.shields.io/github/v/release/RNAX0N/flashsentry?label=release" alt="Latest release"></a>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#who-is-this-for">Who is this for?</a> •
  <a href="#installation">Installation</a> •
  <a href="#quick-start">Quick start</a> •
  <a href="#documentation">Documentation</a> •
  <a href="#building">Building</a>
</p>

---

FlashSentry helps you trust USB sticks and downloaded Linux images **without becoming a cryptography expert**. It monitors removable drives, verifies only the files you care about (or whole images when you need that), and can **automatically check ISO files** against publisher checksums and OpenPGP signatures — no Kleopatra, no manual `sha256sum`, no terminal GPG workflows.

Built with Qt6 for Arch Linux: tray integration, polkit for safe mounting, and a modular C++20 codebase.

> **Note:** FlashSentry started as an experimental LLM-assisted project and is actively evolving. The recommended workflows today are **ISO verification** and **watch-folder (Merkle) checks** on USB volumes; full raw-partition hashing remains available for advanced users.

## Features

### Recommended for most users

| Feature | What it does |
|--------|----------------|
| **Automatic ISO verification** | Detects `.iso` files on mounted USB sticks (e.g. after Rufus), downloads official SHA-256 lists and signatures, verifies OpenPGP and signing-key fingerprints |
| **Watch-folder verification** | You choose folders/files on a drive; FlashSentry builds a Merkle baseline and alerts when watched content changes — fast, no full-disk read |
| **Dedicated ISO mode** | Switch the app to **Automatic ISO verification** for a focused workflow |

### USB monitoring (all modes)

- **Real-time device monitoring** — libudev, no polling
- **Whitelist & trust levels** — remember devices, prompt on unknown or modified content
- **Secure mounting** — UDisks2 + polkit (`noexec`, `nosuid`, `nodev` by default)
- **System tray** — background operation with notifications (optional libnotify)
- **Themes** — Cyber Dark, Neon Purple, Matrix Green, Blade Runner, Ghost White

### Advanced (optional)

| Feature | What it does |
|--------|----------------|
| **Full partition hash** | Raw SHA-256/SHA-512/BLAKE2b over the entire block device — slow, byte-level tamper detection |
| **Hybrid profile** | Watch folders first, then optional full partition hash |

## Who is this for?

- **Anyone who copies Linux ISOs to USB** (Rufus, Ventoy, `cp`, etc.) and wants a clear pass/fail report
- **Users who care about specific folders** on a stick (documents, `EFI`, a project tree) without hashing every sector
- **Power users** who still want full-disk fingerprints or custom hash algorithms

If you only need “is this entire stick bit-for-bit the same as last time?”, enable full-partition hashing in **Settings → Security**.

## Screenshots

![FlashSentry main window](docs/images/main-window.png)

After `sudo cmake --install build --prefix /usr`, open the installed guides under `/usr/share/doc/flashsentry/` for walkthroughs. More UI reference images: [`docs/images/`](docs/images/) (ISO verify report, watch lists).

| View | Status |
|------|--------|
| USB monitor with device cards | Coming soon |
| ISO verification report | Coming soon |
| Watch lists dialog | Coming soon |

## Installation

### From source (Arch)

```bash
git clone https://github.com/RNAX0N/flashsentry.git
cd flashsentry/packaging
makepkg -si
```

### Runtime dependencies

| Package | Purpose |
|---------|---------|
| `qt6-base` | GUI |
| `openssl` | Hashing |
| `udisks2`, `polkit` | Mount / privileges |
| `gpg` | ISO OpenPGP verification (**strongly recommended**) |
| Network | Fetch publisher checksums for ISO verify |
| `libnotify` | Desktop notifications (optional) |

### Post-install

```bash
# Raw partition hashing (optional)
sudo usermod -aG storage $USER

# Autostart (optional)
systemctl --user enable --now flashsentry.service

# Avoid double-mount fights with your DE (recommended)
gsettings set org.gnome.desktop.media-handling automount false   # GNOME
```

Log out and back in after adding the `storage` group.

## Quick start

### Verify ISOs on a USB stick (easiest path)

1. Install FlashSentry and `gpg`.
2. Open **Settings → Verification** → enable **Automatically verify ISOs when a USB drive is mounted** (on by default).
3. Plug in a stick that contains a supported `.iso` (e.g. `archlinux-*-x86_64.iso`, `ubuntu-*-desktop-amd64.iso`).
4. Read the report in the log panel, or switch **Mode** to **Automatic ISO verification** for the full ISO UI.

Supported publisher matching (by filename): **Arch Linux**, **Ubuntu** (including **Ubuntu for Raspberry Pi** `.img.xz`), **Kubuntu**, **Xubuntu**, **Lubuntu**, **Ubuntu MATE**, **Ubuntu Studio**, **Debian**, **Fedora**, **Linux Mint**, **openSUSE**, **Manjaro**, **Kali**, **Rocky/Alma/CentOS Stream**, **elementary**, **Pop!_OS**, **EndeavourOS**, **Garuda**, **CachyOS**, **Nobara**, **Raspberry Pi OS**, **Alpine**, **Void Linux**, **Armbian**, **NixOS**, and **Microsoft Windows** (embedded catalog + `.sha256` sidecars). Ventoy sticks can hold `.iso`, `.img.xz`, or `.zip` images. See [docs/VERIFICATION.md](docs/VERIFICATION.md).

### Verify specific folders on a USB stick

1. Connect and mount the device.
2. On the device card, click **Watch lists**.
3. Add groups and paths (e.g. `Documents`, `EFI/boot`).
4. Build a baseline when the content is known-good.
5. On later connects, FlashSentry verifies the Merkle root against your baseline.

Default profile for new devices: **Watch folders only**.

### Command line

```bash
flashsentry              # Normal start
flashsentry --minimized  # Start in tray
flashsentry --debug      # Verbose logging
flashsentry --help
```

## Configuration

Settings: `~/.config/FlashSentry/FlashSentry.conf`  
Device database: `~/.config/flashsentry/devices.json`

| Setting | Default | Description |
|---------|---------|-------------|
| **Mode** | USB drive monitor | Or **Automatic ISO verification** |
| **Default USB profile** | Watch folders | Or full partition / hybrid |
| **Auto ISO verify on USB mount** | On | Run ISO checks when a volume mounts |
| **Auto hash entire partition on connect** | Off | Full raw-device hash (advanced) |
| **Block modified devices** | Off | Refuse mount after failed verify |

See **[docs/USER_GUIDE.md](docs/USER_GUIDE.md)** for workflows and troubleshooting.

## Documentation

| Document | Audience |
|----------|----------|
| **[docs/README.md](docs/README.md)** | Documentation index |
| **[docs/USER_GUIDE.md](docs/USER_GUIDE.md)** | End users — ISO verify, watch lists, settings, FAQ |
| **[docs/VERIFICATION.md](docs/VERIFICATION.md)** | How verification modes work (Merkle, ISO chain, full hash) |
| **[CLAUDE.md](CLAUDE.md)** | Developers — architecture, components, build |
| **[CHANGELOG.md](CHANGELOG.md)** | Release history |
| **[CONTRIBUTING.md](CONTRIBUTING.md)** | How to contribute |

## Building

```bash
# Arch build deps
sudo pacman -S qt6-base qt6-tools cmake base-devel openssl pkgconf

mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)

# Run from build tree
./flashsentry
```

Optional tests:

```bash
cmake -DFLASHSENTRY_BUILD_TESTS=ON ..
cmake --build . && ctest --test-dir build --output-on-failure
```

Ten tests: `test_types`, `test_database_manager`, `test_merkle`, `test_iso_catalog`, `test_iso_checksum`, `test_autostart`, `test_verify_report`, `test_iso_verify_integration`, `test_iso_verify_publisher_mock`, `test_iso_http_mock` (fixtures under `tests/fixtures/`).

Install (binary, polkit policy, udev rules, and docs under `/usr/share/doc/flashsentry/`):

```bash
sudo cmake --install . --prefix /usr
```

## Troubleshooting

| Problem | What to try |
|---------|-------------|
| ISO verify says “unknown publisher” | Filename must match a supported distro pattern, or place `.sha256` / `.asc` next to the ISO |
| ISO verify needs network | Publisher checksums are downloaded automatically |
| `gpg` errors | Install `gnupg`; keys are cached under `~/.cache/FlashSentry/iso-verify/` |
| dd-written live USB, no `.iso` file | Use watch folders or full partition hash; see [docs/VERIFICATION.md](docs/VERIFICATION.md) |
| Full hash permission denied | Add user to `storage` group, re-login |
| Device not detected | `udevadm monitor --subsystem-match=block` |

## Security

- Polkit for privileged reads/mounts — not run as root
- Database `600` permissions, atomic JSON writes
- ISO path: checksum file signature → compare hash → optional fingerprint allow-list
- User prompts for unknown devices and manifest/hash mismatches (configurable)

## License

MIT — see [LICENSE](LICENSE).

---

<p align="center">
  Made for the Arch Linux community
</p>
