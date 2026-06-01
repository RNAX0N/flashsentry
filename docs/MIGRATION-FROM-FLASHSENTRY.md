# Migrating from FlashSentry to FlashSpartan

FlashSpartan **1.5.0** is the first release under the new product name. GitHub releases **v1.5.0 and newer** ship Windows installers named `FlashSpartan-*`. Older release tags (through **v1.1.4**) used the **FlashSentry** name and did not include MSI/setup.exe on the Releases page.

The git repository is still hosted at [github.com/RNAX0N/flashsentry](https://github.com/RNAX0N/flashsentry); only the application branding and install artifacts were renamed.

## Automatic migration (first launch)

On first start after upgrading, FlashSpartan attempts to:

1. **QSettings** — copy keys from `flashsentry` / `FlashSentry` to `flashspartan` / `FlashSpartan`.
2. **Config directory** — copy missing files from `~/.config/FlashSentry/` into `~/.config/FlashSpartan/` (timeline, verify history, hash checkpoints, blocked drives list, legacy `devices.json`).
3. **Policy store** — import encrypted policy from legacy JSON under `~/.config/FlashSentry/flashsentry/devices.json` and `blocked-drives.json` when the new policy store does not exist yet.

Original FlashSentry files are left in place (not deleted).

## Linux package rename

| Old (Arch) | New |
|------------|-----|
| `flashsentry` | `flashspartan` |
| `flashsentry-policyd` | `flashspartan-policyd` |
| `/etc/flashsentry/` | `/etc/flashspartan/` |
| `org.flashsentry.policy` | `org.flashspartan.policy` |

Install the new package; remove the old `flashsentry` package when prompted. Data under `~/.config/FlashSentry` is migrated as above.

## Windows

- Uninstall **FlashSentry** from Settings if present (older portable or manual installs).
- Install **FlashSpartan-1.5.0-x64-setup.exe** (or MSI) from [Releases](https://github.com/RNAX0N/flashsentry/releases/latest).
- Config and policy data under `%AppData%` / `FlashSentry` are migrated on first launch when possible.

## Environment variables

| Old | New |
|-----|-----|
| `FLASHSENTRY_*` | `FLASHSPARTAN_*` |
| `FLASHSENTRY_POLICY_CONFIG` | `FLASHSPARTAN_POLICY_CONFIG` |
| `FLASHSENTRY_POLICY_IN_PROCESS` | `FLASHSPARTAN_POLICY_IN_PROCESS` |
| `FLASHSENTRY_GPG_PROGRAM` | `FLASHSPARTAN_GPG_PROGRAM` |
| `FLASHSENTRY_SOURCE_ROOT` | `FLASHSPARTAN_SOURCE_ROOT` |

Legacy variable names are not read by 1.5.0+ builds.
