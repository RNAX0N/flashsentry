#!/usr/bin/env python3
"""Validate embedded ISO catalog manifest and its SHA-256 sidecar."""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "resources" / "iso-catalog" / "embedded-manifest.json"
HASH_FILE = MANIFEST.with_suffix(MANIFEST.suffix + ".sha256")


def main() -> int:
    errors: list[str] = []

    if not MANIFEST.is_file():
        errors.append(f"missing manifest: {MANIFEST}")
    if not HASH_FILE.is_file():
        errors.append(f"missing hash file: {HASH_FILE}")

    if errors:
        for msg in errors:
            print(msg, file=sys.stderr)
        return 1

    digest = hashlib.sha256(MANIFEST.read_bytes()).hexdigest()
    expected = HASH_FILE.read_text(encoding="utf-8").strip().split()[0]
    if digest != expected:
        print(
            f"embedded-manifest.json SHA-256 mismatch:\n"
            f"  computed:  {digest}\n"
            f"  expected:  {expected}\n"
            f"Run: sha256sum {MANIFEST} > {HASH_FILE}",
            file=sys.stderr,
        )
        return 1

    try:
        data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        print(f"invalid JSON: {exc}", file=sys.stderr)
        return 1

    if not isinstance(data, dict):
        print("manifest root must be an object", file=sys.stderr)
        return 1

    version = data.get("version") or data.get("manifest_version")
    if not version:
        print("manifest missing 'version' or 'manifest_version'", file=sys.stderr)
        return 1

    entries = data.get("entries")
    if entries is None:
        print("manifest missing 'entries'", file=sys.stderr)
        return 1
    if not isinstance(entries, list):
        print("'entries' must be an array", file=sys.stderr)
        return 1

    for i, entry in enumerate(entries):
        if not isinstance(entry, dict):
            errors.append(f"entries[{i}] is not an object")
            continue
        if not entry.get("filename") and not entry.get("file_pattern"):
            errors.append(f"entries[{i}] missing filename or file_pattern")

    if errors:
        for msg in errors:
            print(msg, file=sys.stderr)
        return 1

    print(
        f"OK: embedded-manifest.json v{version}, "
        f"{len(entries)} entries, SHA-256 verified"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
