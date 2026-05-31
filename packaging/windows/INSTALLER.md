# Windows installer: optional USBPcap

FlashSentry BadUSB packet capture uses **USBPcap** (kernel driver + `USBPcapCMD.exe`). You cannot
ship only the `.exe`; the official **USBPcapSetup** installer must run elevated.

## Recommended approach (checkbox, not silent-by-default)

| Approach | Pros | Cons |
|----------|------|------|
| **Optional NSIS component (recommended)** | User consent; works with driver signing; standard pattern | Requires admin once |
| **WiX Burn bundle** | Good for `.msi` shops | More complex; same admin requirement |
| **Auto-install without UI** | Hands-off | Driver install without explicit consent is risky; may fail SmartScreen/policy |
| **Download on first run** | Small installer | Needs network; harder to audit; still needs admin |

**Do not** rely on modifying `PATH` alone. USBPcap’s installer often leaves `USBPcapCMD.exe` at:

`C:\Program Files\USBPcap\USBPcapCMD.exe`

FlashSentry resolves that path automatically (`UsbPcapLocator`). PATH is optional.

## Legal / redistribution

- Bundle the **official** `USBPcapSetup-x.y.z.exe` from https://desowin.org/usbpcap/ (do not repack the driver yourself).
- USBPcap driver: **GPLv2**; USBPcapCMD: **BSD-2-Clause** — include USBPcap license text in your installer and third-party notices.
- Run their installer as a **separate elevated step** (chain install), not by merging binaries into `FlashSentry.exe`.

## NSIS: optional component (checked by default)

1. Download `USBPcapSetup-1.5.4.0.exe` (or current version) into `packaging/windows/third-party/` (gitignored; CI downloads it).

2. Configure CMake with the path:

   ```powershell
   cmake -B build -DFLASHSENTRY_USBPCAP_INSTALLER="C:/path/USBPcapSetup-1.5.4.0.exe"
   ```

3. Build the NSIS package:

   ```powershell
   cmake --build build --config Release --target package
   ```

4. During FlashSentry setup, NSIS shows **Yes/No**: install USBPcap? (recommended). On **Yes**, it runs the bundled `USBPcapSetup.exe /S` (USBPcap’s own silent NSIS install; still requires admin, which the FlashSentry installer already requests).

For a true **checkbox on the components page**, use a custom `.nsi` (see `packaging/windows/custom-nsis-components.md` outline below) or WiX Burn.

## MSI (WiX)

Use **WiX Burn** (`<Bundle>`) with two packages:

1. FlashScap MSI (your app)
2. USBPcap EXE as `ExePackage` with `InstallCondition` from a Burn checkbox property

Example condition property: `InstallUsbPcap = 1` when the checkbox is checked.

Chain command equivalent: `USBPcapSetup.exe /S` with `InstallScope="perMachine"` and elevated bundle.

## CI: bundling USBPcap

In GitHub Actions, download the installer to the build tree (verify SHA256), then pass
`-DFLASHSENTRY_USBPCAP_INSTALLER=...` when building the NSIS target. Do not commit the binary to git.

## Environment overrides (support / power users)

| Variable | Purpose |
|----------|---------|
| `FLASHSENTRY_USBPCAP_CMD` | Full path to `USBPcapCMD.exe` |
| Custom capture command in settings | Full template with `{bus}`, `{out}`, etc. |

## Driver signing note

USBPcap ships a signed driver for normal Windows 10/11. Test-signing mode is only needed for
custom/unsigned driver builds — not for the official release installer.
