# Third-party Windows installers (not committed)

Download the official USBPcap installer here for local packaging:

- https://desowin.org/usbpcap/
- Example: `USBPcapSetup-1.5.4.0.exe`

Configure CMake:

```powershell
cmake -B build -DFLASHSENTRY_BUILD_WINDOWS_INSTALLERS=ON `
  -DFLASHSENTRY_USBPCAP_INSTALLER="$PWD/packaging/windows/third-party/USBPcapSetup-1.5.4.0.exe"
```
