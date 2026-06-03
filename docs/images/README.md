# FlashSpartan UI screenshots

PNG files in this directory are captured from the shipping Qt UI using:

```bash
./build/flashspartan --capture-screenshots=docs/images --no-tray --force
```

See [../SCREENSHOTS.md](../SCREENSHOTS.md) for headless (`xvfb-run`) instructions.

| File | Description |
|------|-------------|
| [usb-monitor.png](usb-monitor.png) | USB Monitor — stats, demo device row, recent events |
| [iso-verifier.png](iso-verifier.png) | ISO Verifier module |
| [settings.png](settings.png) | Settings page |
| [about.png](about.png) | About page |
| [allow-block-list.png](allow-block-list.png) | Allow/Block list |
| [reports.png](reports.png) | Reports & audit tails |
| [badusb-monitor.png](badusb-monitor.png) | BadUSB Monitor |

**Legacy names** (duplicates for backward-compatible links):

- `main-window.png` → USB Monitor
- `iso-verify-report.png` → ISO Verifier
- `watch-lists.png` → Settings (placeholder; use Settings or re-capture watch UI manually)

Do not commit images that expose real drive serials, usernames, or internal hostnames.
