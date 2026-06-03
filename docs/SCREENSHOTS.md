# Capturing documentation screenshots

Screenshots in `docs/images/` are generated from the **real application UI**, not mockups.

## Quick capture

From a Release build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target flashspartan

# Local display
./build/flashspartan --capture-screenshots=docs/images --no-tray --force

# Headless CI / SSH
xvfb-run -a -s "-screen 0 1280x900x24" \
  ./build/flashspartan --capture-screenshots=docs/images --no-tray --force
```

This writes:

| File | Page |
|------|------|
| `usb-monitor.png` | USB Monitor (demo device + events) |
| `iso-verifier.png` | ISO Verifier |
| `allow-block-list.png` | Allow/Block List |
| `reports.png` | Reports |
| `settings.png` | Settings |
| `about.png` | About |
| `badusb-monitor.png` | BadUSB Monitor |

Legacy aliases (copies for older doc links): `main-window.png`, `iso-verify-report.png`, `watch-lists.png`.

## Tips

1. Use **Cyber Dark** (default) unless documenting another theme.
2. Window size is **1280×820** during capture.
3. Demo data uses fictional “Demo USB Drive” labels — safe to commit.
4. Do not commit screenshots with real serial numbers, hostnames, or personal paths.
5. After UI changes, re-run capture and commit updated PNGs with the PR.

## Manual capture

If you prefer a live session:

1. Build and run FlashSpartan with a test USB stick or loop device.
2. Set appearance to Cyber Dark.
3. Capture each nav page at ≥1280 px width.
4. Replace files in `docs/images/` and update `README.md` if you add new views.
