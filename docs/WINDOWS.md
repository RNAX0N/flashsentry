# Windows 10/11 support

FlashSentry is being ported to Windows in stages. The initial Windows build keeps the Qt
application, ISO verification, reports, audit logging, settings, tray support, and watch-manifest
verification paths portable.

## Initial Windows build

Supported in the first porting milestone:

- ISO verification for local files and removable-volume folders.
- Embedded ISO catalog and user-trusted hash entries.
- Watch-manifest verification on normal mounted paths such as `E:/`.
- System tray notifications.
- Login autostart through the current user's Windows `Run` registry key.
- Removable volume polling through Qt storage APIs.

Linux-only features that are intentionally stubbed in the initial Windows build:

- UDisks2/polkit mount control. Windows normally auto-mounts removable volumes.
- Full-partition raw hashing. A future build needs a native `CreateFileW("\\\\.\\PhysicalDriveN")`
  reader and a UAC elevation story.
- BadUSB HID monitoring. A future build needs SetupAPI/HID enumeration and device notifications.
- usbmon packet capture. Windows users should use USBPcap/Wireshark manually until native
  integration is added.

## Build notes

Use a Qt 6 Windows toolchain and OpenSSL. CMake no longer requires libudev, Qt DBus, pthread, or
the Linux read helper on Windows.

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

If using vcpkg, prefer a normal CMake toolchain invocation with `qtbase` and `openssl` installed
for the selected triplet.

## Roadmap

1. Native removable-volume hotplug and richer device identity via Windows device notifications,
   SetupAPI, and/or WMI.
2. Native full-partition hashing using Windows storage APIs and a UAC-elevated helper.
3. BadUSB HID monitoring using SetupAPI/HID APIs with baseline/anomaly rules shared with Linux.
4. Optional USBPcap capture integration.
5. Windows installer packaging.
