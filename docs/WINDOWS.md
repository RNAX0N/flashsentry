# Windows 10/11 support

FlashSpartan ships a native Windows build with the same Qt shell, ISO verification, policy store,
BadUSB monitoring, and removable-volume workflows as Linux—implemented with Windows APIs instead of
udev, UDisks2, and polkit.

## Supported on Windows

- ISO verification for local files and removable-volume folders (`E:\`, etc.).
- Embedded ISO catalog and user-trusted hash entries.
- Watch-manifest verification on mounted paths.
- USB flash volume detection via `QStorageInfo`, `GetDriveType`, and USB bus type
  (`IOCTL_STORAGE_QUERY_PROPERTY`) so sticks reported as **fixed disks** are included.
- **All USB attachments** on the USB Monitor page — security keys (HID), hubs, chargers/power,
  and generic USB devices via SetupAPI enumeration, in addition to storage volumes.
- Hotplug rescan via `WM_DEVICECHANGE` (volume device interface notifications).
- **Programmatic safe eject** via `FSCTL_DISMOUNT_VOLUME` / `IOCTL_STORAGE_EJECT_MEDIA`.
- **Full-disk raw hashing** via `\\.\PhysicalDriveN` with optional **UAC-elevated**
  `flashspartan-read-helper.exe` (same JSON protocol as Linux's polkit helper).
- **BadUSB HID monitoring** via SetupAPI / HID APIs (VID/PID, capabilities, connect/disconnect).
- **USBPcap capture** when `USBPcapCMD.exe` is on `PATH` (default command template in settings).
- **Policy store** via `flashspartan-policyd.exe` (QLocalServer) or in-process fallback
  (`FLASHSPARTAN_POLICY_IN_PROCESS=1`).
- System tray and login autostart (`HKCU\...\Run`).

## Build notes

Use a Qt 6 Windows toolchain and OpenSSL. The following are built and installed next to
`FlashSpartan.exe`:

- `flashspartan-policyd.exe`
- `flashspartan-read-helper.exe`

Example:

```powershell
cmake -B build-windows -G "Ninja" `
  -DCMAKE_BUILD_TYPE=Release `
  -DFLASHSPARTAN_BUILD_TESTS=ON `
  -DOPENSSL_ROOT_DIR=C:\path\to\openssl `
  C:\path\to\flashspartan
cmake --build build-windows
ctest --test-dir build-windows --output-on-failure
```

Override helper location:

```powershell
$env:FLASHSPARTAN_READ_HELPER = "C:\path\to\flashspartan-read-helper.exe"
```

Force in-process policy (no daemon):

```powershell
$env:FLASHSPARTAN_POLICY_IN_PROCESS = "1"
```

## Privileged hashing (UAC)

If opening `\\.\PhysicalDriveN` returns access denied, FlashSpartan launches
`flashspartan-read-helper.exe` with the **runas** verb. Approve the UAC prompt; the helper writes a
JSON result file that the app reads (same fields as Linux stdout JSON).

## USB packet capture (optional)

USBPcap is **not** bundled in FlashSpartan installers. Install it separately from https://desowin.org/usbpcap/ if you want BadUSB packet capture.

FlashSpartan finds `USBPcapCMD.exe` under `C:\Program Files\USBPcap\` automatically (PATH not required). If USBPcap is missing, HID BadUSB monitoring still works; on **BadUSB Monitor** use **Download USBPcap** to fetch and run the official installer — FlashSpartan polls until capture is ready (no app restart).

- **Override:** `FLASHSPARTAN_USBPCAP_CMD` or a custom capture command in settings (`{bus}`, `{out}`, …)

## Installers (.msi and .exe)

Release builds produce:

- **`FlashSpartan-x.y.z-x64.msi`** — Windows Installer (FlashSpartan only)
- **`FlashSpartan-x.y.z-x64-setup.exe`** — WiX Burn bootstrapper that installs the MSI (no USBPcap step; cannot fail on a missing driver)

See [packaging/windows/INSTALLER.md](../packaging/windows/INSTALLER.md) for build steps.

## Portable package

The Windows CI job also builds a portable ZIP with `windeployqt` and OpenSSL DLLs. See the root
`README.md` Windows section for local packaging commands.

## Roadmap

1. Richer USB serial/model identity (SetupAPI / WMI).
2. Authenticode signing for installers and binaries.
3. Deeper USBPcap device matching (hub/port → USBPcap instance).
