# Capturing real screenshots for FlashSpartan

The files under `docs/images/` are reference mockups. For documentation that matches the shipping UI:

1. Build and run FlashSpartan on Arch with a test USB stick or loop device.
2. Use **Cyber Dark** (Settings → Appearance) unless documenting another theme.
3. Capture at **1100×700** or higher (window default size).
4. Suggested scenes:
   - **Main window** — one connected device card, sidebar statistics, verify history entries, activity log.
   - **ISO verify** — USB devices | ISO verify tab with summary chips and report table.
   - **Security alert** — hash mismatch or ISO failure dialog (use a test stick with a known-good hash, then change one byte).
5. Avoid real serial numbers, hostnames, or personal paths in filenames committed to git.

Replace or add files in `docs/images/` and point `README.md` at them.
