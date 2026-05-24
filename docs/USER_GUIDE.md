# FlashSentry User Guide

This guide explains how to use FlashSentry day to day. You do **not** need prior knowledge of SHA-256, PGP, or Kleopatra for the main workflows.

## What FlashSentry does

FlashSentry offers three levels of checking, from most practical to most thorough:

| Level | Best for | Speed |
|-------|----------|-------|
| **ISO verification** | Linux install images on a USB stick | Minutes (hashes one large file + downloads metadata) |
| **Watch folders** | Documents, boot files, or project trees on a stick | Seconds to minutes (only selected paths) |
| **Full partition hash** | “Every byte on this partition must match” | Slow (reads entire device) |

**Defaults are chosen for real users:** ISO auto-check on mount is **on**; full-partition hash on connect is **off**; new devices use **watch folders** when you set up verification.

---

## Application modes

Open **Settings → Verification → Mode**.

### USB drive monitor (default)

- Shows connected USB partitions as cards
- Runs the verification **profile** you chose per device (watch / full / hybrid)
- ISO auto-verify still runs on mount if enabled

### Automatic ISO verification

- Main window becomes the **ISO Verify** screen
- Table + detailed text report per image
- Use **Verify now** for a folder, or plug in a USB stick (with auto-verify enabled)

---

## Workflow 1: Verify a Linux ISO (recommended)

### What you need

- Arch Linux with FlashSentry installed
- `gpg` package (`sudo pacman -S gnupg`)
- Internet access when verifying (to download official checksums)
- A USB stick with a `.iso` file on it — typical after **Rufus**, **Ventoy**, or copying the file manually

### Steps

1. Enable **Settings → Verification → Automatically verify ISOs when a USB drive is mounted** (default: on).
2. Insert the USB drive and wait for it to mount.
3. FlashSentry will:
   - Find `.iso` files on the volume
   - Match the filename to a known publisher (Arch, Ubuntu, Debian, Fedora)
   - Download official `SHA256SUMS` (or equivalent) and signature files
   - Compute SHA-256 of your on-drive copy
   - Run `gpg --verify` in an isolated cache directory
   - Compare the signing key fingerprint to built-in trusted values
4. Read the result:
   - **Activity log** (USB mode), or
   - **ISO Verify** table and report (ISO mode / **Full report** button)

### What “PASS” means

- Your file’s hash matches the publisher’s published hash, **and**
- The checksum file’s signature is valid, **and**
- The signing key fingerprint matches FlashSentry’s trusted list for that distro

### What if verification fails?

| Message | Likely cause |
|---------|----------------|
| Hash mismatch | Corrupt download, wrong file, or incomplete copy |
| PGP failed | Missing `gpg`, bad signature file, or tampered checksums |
| Untrusted fingerprint | Key not in FlashSentry’s list (report upstream) |
| Unknown publisher | Unsupported filename; add `.sha256` / `.asc` sidecars manually |
| No ISO found | Stick was written with `dd` as a live image — see below |

### Offline / sidecar files

If you already downloaded checksums, you can place them next to the ISO on the stick:

- `myimage.iso.sha256` or `SHA256SUMS`
- `myimage.iso.asc` or `SHA256SUMS.gpg`

FlashSentry uses these when publisher download is unavailable.

### `dd` or Rufus “ISO mode” without a loose file

When the stick **is** the live system (no separate `.iso` on the filesystem), FlashSentry detects a **bootable layout** and explains that:

- Automated ISO file hashing needs a `.iso` on the volume, **or**
- Use **watch folders** for important paths, **or**
- Enable **full partition hash** for byte-level comparison

---

## Workflow 2: Watch specific folders (file-tree verification)

Use this when you care about **certain files** changing, not the entire drive.

### Setup (first time)

1. Connect the USB device and mount it.
2. Click **Watch lists** on the device card.
3. Create a **group** (e.g. “Work files”) and add paths:
   - Single file: `report.pdf`
   - Folder: `Documents` (hashes all files inside recursively)
4. Choose verification profile for this device if prompted.
5. Click **Build baseline** — FlashSentry stores a Merkle root for each group.

### Everyday use

1. Plug in the device (auto-mount).
2. FlashSentry compares current files to the baseline.
3. **Match** → status shows verified.
4. **Mismatch** → you can rebuild baseline (intentional change) or investigate tampering.

### Tips

- Keep groups small and meaningful (faster, clearer alerts).
- Rebuild baseline after **you** intentionally update files.
- Use **Hybrid** profile only if you also want a full partition hash afterward.

---

## Workflow 3: Full partition hash (advanced)

Use when you need a fingerprint of **every byte** on the partition.

1. **Settings → Security → Full-drive hashing**
2. Enable **Hash entire partition when a device is connected**
3. Ensure your user is in the `storage` group
4. First connect stores the hash; later connects compare

**Warning:** Large drives take a long time. Prefer ISO or watch-folder modes unless you have a specific reason.

---

## Device card actions

| Button | Action |
|--------|--------|
| Mount / Unmount / Eject | Standard volume control |
| Rehash | Force verification for this device |
| Watch lists | Edit groups and baselines |
| Open folder | Open mount point in file manager |

---

## Settings reference

### General tab

| Option | Description |
|--------|-------------|
| **Start minimized to tray** | Launch into the tray instead of showing the main window |
| **Start automatically at login** | Enables `flashsentry.service` (systemd user unit) when installed from the Arch package; otherwise writes an XDG autostart entry under `~/.config/autostart/` |
| **Minimize to tray instead of closing** | Keep running in the background when the window is closed |
| **Show desktop notifications** | Tray / libnotify alerts for device events |

### Verification tab

| Option | Description |
|--------|-------------|
| **Mode** | USB monitor vs ISO-focused UI |
| **Default USB profile** | Watch folders / full partition / hybrid |
| **Scan folder** | Default directory for manual ISO scan |
| **Verify after scan** | Auto-run when scanning a folder in ISO mode |
| **Verify on USB mount** | Auto ISO check when removable media mounts |

### Security tab

| Option | Description |
|--------|-------------|
| **Hash entire partition when connected** | Full raw hash (off by default) |
| **Re-hash before eject** | Optional final check |
| **Ask before mounting new devices** | Whitelist prompt |
| **Alert when hash doesn’t match** | Tamper / manifest dialogs |
| **Block modified devices** | Do not mount after failure |

### Hashing tab

Algorithm, buffer size, memory mapping — mainly for **full partition** mode.

---

## Supported ISO publishers (automatic)

Filename patterns (examples):

| Publisher | Example filename |
|-----------|------------------|
| Arch Linux | `archlinux-2024.11.01-x86_64.iso` |
| Ubuntu | `ubuntu-24.04.2-desktop-amd64.iso` |
| Debian | `debian-12.8.0-amd64-netinst.iso` |
| Fedora | `Fedora-Workstation-Live-41-1.4.x86_64.iso` |
| Linux Mint | `linuxmint-22.2-cinnamon-64bit.iso` |
| openSUSE Leap | `openSUSE-Leap-15.6-DVD-x86_64-Media.iso` |
| openSUSE Tumbleweed | `openSUSE-Tumbleweed-DVD-x86_64-Current.iso` |
| Manjaro | `manjaro-kde-25.0.0-250527-linux612.iso` |

Publisher mirrors and keys are defined in the application; see [VERIFICATION.md](VERIFICATION.md) for technical detail.

---

## Files and privacy

| Path | Contents |
|------|----------|
| `~/.config/FlashSentry/FlashSentry.conf` | UI and behavior settings |
| `~/.config/flashsentry/devices.json` | Whitelist, baselines, optional partition hashes |
| `~/.cache/FlashSentry/iso-verify/` | Downloaded checksums, GPG homedir cache |

ISO verification contacts publisher mirrors over HTTPS. No telemetry is sent to FlashSentry developers by the app itself.

---

## FAQ

**Do I need Kleopatra?**  
No. FlashSentry calls `gpg` itself with a private homedir under your cache directory.

**Will it verify Windows ISOs?**  
Not automatically by publisher catalog today. Use sidecar `.sha256` / `.asc` files if you have them.

**Can I use it only for ISOs and ignore USB whitelisting?**  
Yes. Set **Mode** to **Automatic ISO verification** and use folder scan or mount auto-verify.

**Why is full-disk hashing off by default?**  
It is slow and confusing for most people. Watch folders and ISO checks cover common needs faster.

**Ventoy with multiple ISOs?**  
Each `.iso` on the mounted Ventoy partition can be verified when the volume mounts.

---

## Getting help

- Technical detail: [VERIFICATION.md](VERIFICATION.md)
- Build / development: [CLAUDE.md](../CLAUDE.md) and [README.md](../README.md)
- Issues: [GitHub Issues](https://github.com/RNAX0N/flashsentry/issues)
