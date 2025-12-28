# FlashSentry

<p align="center">
  <img src="resources/icons/flashsentry.svg" alt="FlashSentry Logo" width="128" height="128">
</p>

<p align="center">
  <strong>🛡️ USB Flash Drive Security Monitor for Arch Linux</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#usage">Usage</a> •
  <a href="#configuration">Configuration</a> •
  <a href="#building">Building</a>
</p>

---

FlashSentry monitors USB flash drives, maintains a cryptographic whitelist of trusted devices, and alerts you when a device has been modified. Built with a futuristic Qt6 interface, optimized for speed, and designed the "Arch way" — simple, modular, and well-documented.

## Features

- **🔍 Real-time Device Monitoring** — Instant detection of USB storage devices via libudev
- **🔐 Cryptographic Verification** — SHA-256, SHA-512, or BLAKE2b hashing with memory-mapped I/O
- **🎨 Futuristic UI** — Cyberpunk-inspired themes with smooth animations and glow effects
- **📌 System Tray Integration** — Runs in the background with desktop notifications
- **🔑 No Root Required** — Uses polkit for secure privilege escalation
- **📦 Pacman Integration** — Full PKGBUILD for seamless Arch Linux installation

## Screenshots

| CyberDark Theme | Device Detection |
|:---:|:---:|
| ![Main Window](docs/screenshot-main.png) | ![Alert](docs/screenshot-alert.png) |

## Installation

### From AUR (Recommended)

```bash
yay -S flashsentry
# or
paru -S flashsentry
```

### From Source

```bash
# Clone the repository
git clone https://github.com/flashsentry/flashsentry.git
cd flashsentry/packaging

# Build and install
makepkg -si
```

### Post-Installation Setup

```bash
# Add your user to the storage group for raw device access
sudo usermod -aG storage $USER

# Enable autostart (optional)
systemctl --user enable --now flashsentry.service

# Disable your DE's auto-mount (recommended)
# GNOME:
gsettings set org.gnome.desktop.media-handling automount false
# KDE: System Settings > Removable Storage > Uncheck automount
```

**Important:** Log out and back in after adding yourself to the storage group.

## Usage

### Starting FlashSentry

```bash
# Normal start
flashsentry

# Start minimized to tray
flashsentry --minimized

# With debug output
flashsentry --debug
```

### Workflow

1. **Connect a USB device** — FlashSentry detects it automatically
2. **New device?** — You'll be prompted to add it to the whitelist
3. **Known device?** — Hash verification runs automatically
4. **Hash matches** — Device is mounted normally
5. **Hash mismatch** — ⚠️ Security alert! The device has been modified
6. **Eject** — Re-hash before safe removal (optional)

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+R` | Refresh device list |
| `Ctrl+,` | Open settings |
| `Ctrl+Q` | Quit application |
| `Escape` | Minimize to tray |

## Configuration

Settings are stored in `~/.config/FlashSentry/FlashSentry.conf`

### Key Settings

| Setting | Default | Description |
|---------|---------|-------------|
| Auto-hash on connect | ✅ | Verify devices automatically when plugged in |
| Auto-hash on eject | ✅ | Re-calculate hash before ejecting |
| Block modified devices | ❌ | Prevent mounting of modified devices |
| Hash algorithm | SHA256 | SHA256, SHA512, or BLAKE2b |
| Buffer size | 1024 KB | Read buffer for hashing (64-16384 KB) |
| Use memory mapping | ✅ | Use mmap for faster hashing |

### Themes

FlashSentry includes 5 built-in themes:

- **Cyber Dark** — Cyan accents on dark background (default)
- **Neon Purple** — Magenta/purple neon aesthetic
- **Matrix Green** — Classic green-on-black terminal look
- **Blade Runner** — Warm orange/amber tones
- **Ghost White** — Light theme with blue accents

## Building from Source

### Dependencies

```bash
# Arch Linux
sudo pacman -S qt6-base qt6-tools cmake base-devel openssl pkgconf

# The following are runtime dependencies (installed automatically via PKGBUILD)
# udisks2, polkit, systemd-libs
```

### Development Build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Run
./FlashSentry
```

### Release Build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Install
sudo cmake --install . --prefix /usr
```

## Kernel Compatibility

FlashSentry works with any Linux kernel that has udev and USB mass storage support:

- ✅ `linux` (standard Arch kernel)
- ✅ `linux-lts`
- ✅ `linux-zen`
- ✅ `linux-hardened`
- ✅ Custom kernels with standard block device support

## Troubleshooting

### Device not detected

```bash
# Check if udev sees the device
udevadm monitor --property --udev --subsystem-match=block

# Verify udev rules are loaded
udevadm control --reload-rules
udevadm trigger
```

### Permission denied when hashing

```bash
# Verify group membership
groups | grep storage

# If not in storage group, add yourself
sudo usermod -aG storage $USER
# Then log out and back in
```

### Mount operations fail

```bash
# Check UDisks2 service
systemctl status udisks2.service

# Ensure polkit agent is running
pgrep -f polkit
```

### Hash speed is slow

1. Enable memory mapping in settings
2. Increase buffer size (try 4096 KB or 8192 KB)
3. Use a USB 3.0 port if available

## Security

FlashSentry is designed with security in mind:

- **No root privileges** — Uses polkit for privilege escalation
- **Secure mount options** — Default: `noexec,nosuid,nodev`
- **Secure storage** — Database file has 600 permissions
- **Tamper detection** — Cryptographic hashes detect any byte-level modification
- **User confirmation** — Always prompts before mounting unknown devices

### Security Best Practices

1. Enable "Block modified devices" for sensitive environments
2. Use SHA-512 or BLAKE2b for stronger verification
3. Regularly review the device whitelist
4. Keep FlashSentry running in the background via systemd

## Contributing

Contributions are welcome! Please read our [Contributing Guidelines](CONTRIBUTING.md) before submitting a pull request.

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Qt Project for the excellent GUI framework
- The Arch Linux community for the "keep it simple" philosophy
- OpenSSL for robust cryptographic functions
- freedesktop.org for UDisks2 and polkit standards

---

<p align="center">
  Made with ❤️ for the Arch Linux community
</p>