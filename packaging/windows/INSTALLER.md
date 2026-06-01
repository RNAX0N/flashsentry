# Windows installers (.msi + .exe)

FlashSpartan ships two Windows installer formats built with the **WiX Toolset v3**:

| Artifact | Format | Purpose |
|----------|--------|---------|
| `FlashSpartan-x.y.z-x64.msi` | MSI | IT / GPO / `msiexec`; feature tree includes **USBPcap** checkbox |
| `FlashSpartan-x.y.z-x64-setup.exe` | Burn bundle | End-user bootstrapper; **Options** page checkbox for USBPcap |

Both require the official **USBPcapSetup.exe** at build time (not committed to git).

## Prerequisites

1. **WiX Toolset v3.14+** — https://wixtoolset.org/ or `choco install wixtoolset`
2. **USBPcap** — download into `packaging/windows/third-party/`:

   ```powershell
   pwsh packaging/windows/scripts/download-usbpcap.ps1
   ```

3. Built **Release** app (CMake + Qt + OpenSSL), same as portable ZIP.

## Build

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_BUILD_TYPE=Release `
  -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64" `
  -DFLASHSPARTAN_BUILD_WINDOWS_INSTALLERS=ON `
  -DFLASHSPARTAN_USBPCAP_INSTALLER="$PWD/packaging/windows/third-party/USBPcapSetup-1.5.4.0.exe"

cmake --build build --config Release
cmake --build build --config Release --target flashspartan-windows-installers
```

Outputs (under `build/wix-out/`):

- `FlashSpartan-1.4.2-x64.msi`
- `FlashSpartan-1.4.2-x64-setup.exe`

## USBPcap behavior

| Installer | User control |
|-----------|----------------|
| **MSI** | Feature tree: **USBPcap (BadUSB packet capture)** — on by default, can uncheck |
| **setup.exe** | Burn **Options**: **Install USBPcap (recommended)** — on by default |

When selected, the official `USBPcapSetup.exe /S` runs elevated (driver + `USBPcapCMD.exe`).

FlashSpartan finds `C:\Program Files\USBPcap\USBPcapCMD.exe` without PATH (`UsbPcapLocator`).

## Legal

- Bundle the **unmodified** USBPcap installer from https://desowin.org/usbpcap/
- Include USBPcap license notices in your release docs (driver GPLv2, CMD BSD-2-Clause)

## CI

The Windows workflow installs WiX, downloads USBPcap, and builds both artifacts when possible.

## Portable ZIP

Unchanged: `cmake --install` + `windeployqt` ZIP without USBPcap. Use installers when you want optional USBPcap.

## Troubleshooting

| Issue | Fix |
|-------|-----|
| `candle` not found | Install WiX Toolset; reopen shell |
| `heat` fails on `share/` | Run `cmake --install` first (`flashspartan-stage-windows`) |
| Burn not built | Install full WiX (`lit.exe` missing) — MSI still builds |
| USBPcap capture fails | Re-run installer with USBPcap enabled; reboot if driver requests it |
