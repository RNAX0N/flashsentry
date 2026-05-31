# Windows 10/11 support

FlashSentry ships a native Windows build with the same Qt shell, ISO verification, policy store,
BadUSB monitoring, and removable-volume workflows as Linux—implemented with Windows APIs instead of
udev, UDisks2, and polkit.

## Supported on Windows

- ISO verification for local files and removable-volume folders (`E:\`, etc.).
- Embedded ISO catalog and user-trusted hash entries.
- Watch-manifest verification on mounted paths.
- Removable volume detection via `QStorageInfo` and `GetDriveType`.
- **Programmatic safe eject** via `FSCTL_DISMOUNT_VOLUME` / `IOCTL_STORAGE_EJECT_MEDIA`.
- **Full-disk raw hashing** via `\\.\PhysicalDriveN` with optional **UAC-elevated**
  `flashsentry-read-helper.exe` (same JSON protocol as Linux's polkit helper).
- **BadUSB HID monitoring** via SetupAPI / HID APIs (VID/PID, capabilities, connect/disconnect).
- **USBPcap capture** when `USBPcapCMD.exe` is on `PATH` (default command template in settings).
- **Policy store** via `flashsentry-policyd.exe` (QLocalServer) or in-process fallback
  (`FLASHSENTRY_POLICY_IN_PROCESS=1`).
- System tray and login autostart (`HKCU\...\Run`).

## Build notes

Use a Qt 6 Windows toolchain and OpenSSL. The following are built and installed next to
`FlashSentry.exe`:

- `flashsentry-policyd.exe`
- `flashsentry-read-helper.exe`

Example:

```powershell
cmake -B build-windows -G "Ninja" `
  -DCMAKE_BUILD_TYPE=Release `
  -DFLASHSENTRY_BUILD_TESTS=ON `
  -DOPENSSL_ROOT_DIR=C:\path\to\openssl `
  C:\path\to\flashsentry
cmake --build build-windows
ctest --test-dir build-windows --output-on-failure
```

Override helper location:

```powershell
$env:FLASHSENTRY_READ_HELPER = "C:\path\to\flashsentry-read-helper.exe"
```

Force in-process policy (no daemon):

```powershell
$env:FLASHSENTRY_POLICY_IN_PROCESS = "1"
```

## Privileged hashing (UAC)

If opening `\\.\PhysicalDriveN` returns access denied, FlashSentry launches
`flashsentry-read-helper.exe` with the **runas** verb. Approve the UAC prompt; the helper writes a
JSON result file that the app reads (same fields as Linux stdout JSON).

## USB packet capture

FlashSentry finds `USBPcapCMD.exe` under `C:\Program Files\USBPcap\` automatically (PATH not required).

- **Installer:** set `-DFLASHSENTRY_USBPCAP_INSTALLER=...\USBPcapSetup.exe` when configuring CMake to
  bundle USBPcap and prompt during NSIS setup (see `packaging/windows/INSTALLER.md`).
- **Manual:** install from https://desowin.org/usbpcap/
- **Override:** `FLASHSENTRY_USBPCAP_CMD` or a custom capture command in settings (`{bus}`, `{out}`, …)

## Installers (.msi and .exe)

Release builds can produce:

- **`FlashSentry-x.y.z-x64.msi`** — Windows Installer with a feature-tree checkbox for USBPcap
- **`FlashSentry-x.y.z-x64-setup.exe`** — WiX Burn bootstrapper with an **Install USBPcap** option (checked by default)

See [packaging/windows/INSTALLER.md](../packaging/windows/INSTALLER.md) for build steps.

## Portable package

The Windows CI job also builds a portable ZIP with `windeployqt` and OpenSSL DLLs. See the root
`README.md` Windows section for local packaging commands.

## Roadmap

1. `WM_DEVICECHANGE` hotplug (lower latency than polling).
2. Richer USB serial/model identity (SetupAPI / WMI).
3. WiX/MSI installer and Authenticode signing.
4. Deeper USBPcap device matching (hub/port → USBPcap instance).
