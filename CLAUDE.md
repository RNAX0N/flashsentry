# FlashSentry - CLAUDE.md

## Project Overview

FlashSentry is a high-performance USB flash drive security monitoring application for Arch Linux. It monitors USB device insertions, maintains a cryptographic whitelist of known devices, verifies device integrity through SHA-256/SHA-512/BLAKE2b hashing, and alerts users when devices have been modified.

**Design Philosophy**: Built the "Arch way" - simple, modular, well-documented, and optimized for speed and security.

## Features

- **Real-time Device Monitoring**: Uses libudev for efficient USB detection without polling
- **Cryptographic Verification**: SHA-256, SHA-512, or BLAKE2b hashing with memory-mapped I/O
- **Futuristic UI**: Qt6-based GUI with cyberpunk-inspired themes and smooth animations
- **System Tray Integration**: Runs in background with desktop notifications
- **Privilege Escalation**: Uses polkit for secure operations without running as root
- **Pacman Integration**: Full PKGBUILD for easy Arch Linux installation

## Technology Stack

| Component | Technology |
|-----------|------------|
| Language | C++20 |
| GUI Framework | Qt6 (Core, Gui, Widgets, DBus, Concurrent) |
| Device Monitoring | libudev |
| Cryptography | OpenSSL (EVP API) |
| Mount Operations | UDisks2 via D-Bus |
| Build System | CMake 3.20+ |
| Package Manager | pacman (PKGBUILD) |

## Project Structure

```
flashsentry/
├── CMakeLists.txt              # Build configuration
├── src/
│   ├── main.cpp                # Application entry point
│   ├── MainWindow.cpp          # Main application window
│   ├── DeviceMonitor.cpp       # libudev monitoring thread
│   ├── HashWorker.cpp          # Async hashing with QtConcurrent
│   ├── DatabaseManager.cpp     # Thread-safe JSON whitelist
│   ├── MountManager.cpp        # UDisks2 D-Bus integration
│   ├── DeviceCard.cpp          # Animated device display widget
│   ├── TrayIcon.cpp            # System tray with notifications
│   ├── SettingsDialog.cpp      # Configuration UI
│   └── StyleManager.cpp        # Theming and styling
├── include/
│   ├── Types.h                 # Shared data structures
│   ├── MainWindow.h
│   ├── DeviceMonitor.h
│   ├── HashWorker.h
│   ├── DatabaseManager.h
│   ├── MountManager.h
│   ├── DeviceCard.h
│   ├── TrayIcon.h
│   ├── SettingsDialog.h
│   └── StyleManager.h
├── resources/
│   ├── resources.qrc           # Qt resource file
│   ├── icons/
│   │   └── flashsentry.svg     # Application icon
│   ├── styles/
│   └── fonts/
├── packaging/
│   ├── PKGBUILD                # Arch Linux package build
│   ├── flashsentry.install     # pacman install hooks
│   ├── flashsentry.desktop     # Desktop entry
│   ├── flashsentry.service     # systemd user service
│   ├── org.flashsentry.policy  # polkit policy
│   └── 99-flashsentry.rules    # udev rules
└── cmake/
```

## Building

### Dependencies (Arch Linux)

```bash
sudo pacman -S qt6-base qt6-tools cmake base-devel openssl pkgconf
```

### Development Build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Release Build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Package Build (for pacman)

```bash
cd packaging
makepkg -si
```

## Installation

### From Package (Recommended)

```bash
cd packaging
makepkg -si
```

### Manual Installation

```bash
sudo cmake --install build --prefix /usr
```

### Post-Installation

```bash
# Add user to storage group for raw device access
sudo usermod -aG storage $USER

# Enable autostart
systemctl --user enable --now flashsentry.service

# Disable DE auto-mount (GNOME example)
gsettings set org.gnome.desktop.media-handling automount false
```

## Key Components

### DeviceMonitor
- Runs in dedicated QThread
- Uses poll() for efficient event waiting
- Filters for USB block device partitions
- Emits Qt signals on device add/remove/change
- Thread-safe device tracking with QMutex

### HashWorker
- Asynchronous hashing via QtConcurrent
- Supports SHA-256, SHA-512, BLAKE2b
- Memory-mapped I/O for speed (mmap)
- Falls back to buffered read with O_DIRECT
- Real-time progress reporting
- Cancellation support

### DatabaseManager
- Thread-safe with QReadWriteLock
- JSON storage for human readability
- Atomic writes with temp file + rename
- Automatic backups (max 5)
- Secure file permissions (600)

### MountManager
- Communicates with UDisks2 via D-Bus
- Async mount/unmount/eject operations
- polkit integration for privileges
- Security defaults: noexec, nosuid, nodev

### StyleManager
- 5 built-in themes: CyberDark, NeonPurple, MatrixGreen, BladeRunner, GhostWhite
- Dynamic theme switching
- Glow and pulse animations
- Consistent styling across all widgets

## Database Format (devices.json)

```json
{
  "version": "1.0",
  "devices": [
    {
      "unique_id": "SERIAL_VENDOR_MODEL",
      "hash": "sha256_hex_string",
      "hash_algorithm": "SHA256",
      "first_seen": "2024-01-01T00:00:00Z",
      "last_seen": "2024-01-01T00:00:00Z",
      "trust_level": 1,
      "auto_mount": false,
      "device_info": {
        "serial": "ABC123",
        "vendor": "SanDisk",
        "model": "Ultra USB 3.0"
      }
    }
  ]
}
```

## Configuration

Settings stored in: `~/.config/FlashSentry/FlashSentry.conf`

Key settings:
- `security/autoHashOnConnect`: Auto-verify devices on plug-in
- `security/autoHashOnEject`: Re-hash before ejecting
- `security/blockModified`: Prevent mounting modified devices
- `hashing/algorithm`: SHA256, SHA512, or BLAKE2b
- `hashing/bufferSizeKB`: Read buffer size (64-16384 KB)
- `hashing/useMemoryMapping`: Use mmap for speed

## Kernel Compatibility

FlashSentry works with any Linux kernel that has:
- udev support (all modern kernels)
- Block device support
- USB mass storage support

Tested kernels:
- linux (standard Arch kernel)
- linux-lts
- linux-zen
- linux-hardened

## Security Considerations

1. **No Root Required**: Uses polkit for privilege escalation
2. **Mount Security**: Default mount options include noexec, nosuid, nodev
3. **Secure Storage**: Database file has 600 permissions
4. **Tamper Detection**: Cryptographic hashes detect any modification
5. **User Confirmation**: Prompts before mounting unknown/modified devices

## Troubleshooting

### Device not detected
```bash
# Check udev rules
udevadm test /sys/block/sdX

# Monitor udev events
udevadm monitor --property --udev --subsystem-match=block
```

### Permission denied on hash
```bash
# Add user to storage group
sudo usermod -aG storage $USER
# Log out and back in
```

### UDisks2 mount fails
```bash
# Check UDisks2 service
systemctl status udisks2.service

# Check polkit agent
pgrep -f polkit
```

### Non-standard kernel issues
```bash
# Verify kernel config
zcat /proc/config.gz | grep CONFIG_BLK_DEV_BSG
```

## Command Line Options

```
flashsentry [options]

Options:
  -m, --minimized    Start minimized to system tray
  -f, --force        Force start even if another instance is running
  -d, --debug        Enable debug output
  --no-tray          Disable system tray icon
  -c, --config PATH  Path to configuration file
  -h, --help         Display help
  -v, --version      Display version
```

## Development Notes

### Threading Model
- Main thread: Qt event loop, GUI
- DeviceMonitor thread: udev event polling
- HashWorker threads: QtConcurrent thread pool
- DatabaseManager: Thread-safe, called from any thread

### Signal Flow
1. DeviceMonitor emits `deviceConnected`
2. MainWindow receives, creates DeviceCard
3. MainWindow checks database, starts HashWorker
4. HashWorker emits `hashProgress`, `hashCompleted`
5. MainWindow updates DeviceCard status
6. MountManager called if verified

### Adding a New Theme
1. Add enum value to `StyleManager::Theme`
2. Add color palette to `s_themePalettes` in StyleManager.cpp
3. Add name mapping in `themeName()`
4. Update `availableThemes()` return list

## License

MIT License - See LICENSE file