# ISO catalog drop-ins

Install additional manifest fragments here as `*.json` files. FlashSpartan merges them after the embedded catalog and remote cache.

User overrides: `~/.config/flashspartan/iso-catalog.d/`

## Example fragment

```json
{
  "manifest_version": 2,
  "entries": [
    {
      "publisher_id": "custom",
      "publisher_name": "Internal build",
      "file_pattern": "^corp-linux-.*\\.iso$",
      "release_label": "Corp Linux",
      "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
      "reference_url": "https://intranet.example/releases/"
    }
  ]
}
```

See `resources/iso-catalog/embedded-manifest.json` for the full schema.
