# Diagnostics, logging, and crash reports

FlashSpartan keeps several logs so you can triage USB detection issues (for example many built-in USB nodes on a laptop like the ASUS ROG Flow Z13) without flooding the USB Monitor table.

## Log locations

| Log | Path (typical) | Contents |
|-----|----------------|----------|
| **Qt / app log** | `%LOCALAPPDATA%\FlashSpartan\cache\logs\flashspartan.log` (Windows) or `~/.cache/FlashSpartan/logs/flashspartan.log` (Linux) | `qDebug` / `qInfo` / `qWarning` / errors, startup, device monitor |
| **Host USB inventory** | `…/logs/host-usb-inventory.jsonl` | One JSON object per scan: every `USB\` node with `tier` (`internal` vs `peripheral`) |
| **Verification audit** | See Reports → Open audit log | ISO verify / security events (JSON lines) |
| **In-app activity** | USB Monitor sidebar (legacy module) | Recent high-level messages |
| **Config** | `%AppData%\FlashSpartan\FlashSpartan.conf` | Settings including `[diagnostics]` |

Run from a terminal with extra verbosity:

```bash
flashspartan --debug
```

## Built-in vs removable USB (Windows)

The app **tracks all** present `USB\` host nodes for BadUSB / hotplug logic. The USB Monitor UI only lists:

1. **Removable storage** — drive letters / ISO sticks (the “Removable storage” stat).
2. **External peripherals** (optional) — cameras, security keys, phones, etc.

**Built-in / host USB** (hubs, xHCI, composite `&MI_xx` interfaces, “USB device” controllers, Bluetooth USB, non-removable HID) are classified as `internal` and:

- Not counted in **Removable storage**
- Not listed in the device table by default
- Summarized under the gray note on USB Monitor (“N built-in USB node(s) tracked…”)
- Written to `host-usb-inventory.jsonl` when inventory logging is enabled

Classification uses Windows `DEVPKEY_Device_RemovalPolicy`, instance ID patterns, and device category.

### Settings (`FlashSpartan.conf`)

```ini
[diagnostics]
logHostUsbInventory=true
showExternalUsbPeripherals=true
crashReportsEnabled=false
```

- **logHostUsbInventory** — append a snapshot after the initial USB host scan (and via device row menu → Export USB inventory log).
- **showExternalUsbPeripherals** — show non-storage USB peripherals in the device table.
- **crashReportsEnabled** — opt-in; requires a Sentry build (below).

## Reviewing the “41 devices” on your Z13

1. Install/run a build with these diagnostics changes.
2. Open USB Monitor — note the built-in count in the subtitle.
3. Open the logs folder (device actions → **Export USB inventory log…** on a host row, or open the path shown in the subtitle).
4. Open `host-usb-inventory.jsonl` in a text editor or `jq`.

Each line looks like:

```json
{"time":"…","trigger":"initial_scan","count":41,"devices":[{"instance_id":"USB\\…","tier":"internal","category":"USB hub",…},…]}
```

Use this file for support tickets; it is local-only unless you attach it manually.

## Automatic crash reports (optional)

There is **no telemetry by default**. To add crash reporting:

### Recommended: Sentry (cross-platform)

1. Create a project at [sentry.io](https://sentry.io) and copy the **DSN**.
2. Add **sentry-native** to the Windows/Linux build (CMake `FetchContent` or vcpkg).
3. Configure CMake with `-DFLASHSPARTAN_SENTRY=ON` and implement `CrashReporter::tryInstall()` (stub in `src/CrashReporter.cpp`).
4. Enable in settings or environment:

   ```bash
   set FLASHSPARTAN_SENTRY_DSN=https://…@o….ingest.sentry.io/…
   ```

   And in `FlashSpartan.conf`:

   ```ini
   [diagnostics]
   crashReportsEnabled=true
   ```

5. Ship **without** the DSN in the installer; users who opt in can set the env var, or you can inject the DSN at build time for a “beta” channel only.

Sentry gives stack traces, breadcrumbs (if you call `sentry_add_breadcrumb` from `logMessage`), and release tagging (`FLASHSPARTAN_VERSION`).

### Windows-only alternatives

- **Windows Error Reporting + local dumps** — `SetUnhandledExceptionFilter` + WER registry (`LocalDumps`) for full dumps without a cloud service.
- **Breakpad / Crashpad** — Chromium minidumps; more work to host symbol servers.

### Linux

- **systemd-coredump** — `coredumpctl info flashspartan` after a crash.
- **Sentry** — same as Windows.

### Privacy

- Default: everything stays on disk under the user’s cache/config dirs.
- Crash reports should be **opt-in**, include no file paths from mounted USB unless the user consents, and document what is sent in the privacy / About page.

## Future improvements

- Settings UI toggles for diagnostics (today: `FlashSpartan.conf` or JSON profile).
- “Copy diagnostics bundle” zip (log + inventory + version + `QSysInfo`).
- Sentry breadcrumbs wired from `MainWindow::logMessage`.
- HID tiering (hide laptop keyboard/trackpad from USB Monitor while still monitoring in BadUSB).
