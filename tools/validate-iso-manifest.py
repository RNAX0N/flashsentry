#!/usr/bin/env python3
"""Validate embedded ISO catalog manifest and its SHA-256 sidecar."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "resources" / "iso-catalog" / "embedded-manifest.json"
HASH_FILE = MANIFEST.with_suffix(MANIFEST.suffix + ".sha256")
ASC_FILE = MANIFEST.with_suffix(MANIFEST.suffix + ".asc")
PUB_FILE = ROOT / "resources" / "iso-catalog" / "catalog-signing.pub"


def _is_msys_gpg(gpg_exe: str) -> bool:
    lower = gpg_exe.replace("\\", "/").lower()
    return "/git/" in lower or "/msys" in lower or lower.endswith("/usr/bin/gpg")


def _to_gpg_path(gpg_exe: str, path: Path) -> str:
    """Return a path string gpg can open (MSYS gpg needs Unix-style paths)."""
    resolved = path.resolve()
    if sys.platform == "win32" and _is_msys_gpg(gpg_exe):
        try:
            result = subprocess.run(
                ["cygpath", "-u", str(resolved)],
                capture_output=True,
                text=True,
                check=True,
                timeout=10,
            )
            return result.stdout.strip()
        except (FileNotFoundError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
            text = str(resolved).replace("\\", "/")
            if len(text) >= 2 and text[1] == ":":
                return "/" + text[0].lower() + text[2:]
    return str(resolved)


def _resolve_gpg() -> str | None:
    env = os.environ.get("FLASHSENTRY_GPG_PROGRAM", "").strip()
    if env and Path(env).is_file():
        return env
    found = shutil.which("gpg")
    if found:
        return found
    if sys.platform == "win32":
        for candidate in (
            Path(r"C:\Program Files\Git\usr\bin\gpg.exe"),
            Path(r"C:\Program Files\Git\mingw64\bin\gpg.exe"),
        ):
            if candidate.is_file():
                return str(candidate)
    return None


def _gpg_batch_args() -> list[str]:
    args = ["--batch", "--no-tty"]
    if sys.platform == "win32":
        args.extend(["--pinentry-mode", "loopback"])
    return args


def _verify_openpgp() -> tuple[bool, str]:
    gpg_exe = _resolve_gpg()
    if gpg_exe is None:
        return True, "GPG signature not checked (gpg not found)"

    if not ASC_FILE.is_file() or not PUB_FILE.is_file():
        return True, "GPG signature not checked"

    env = os.environ.copy()
    env.pop("GNUPGHOME", None)

    with tempfile.TemporaryDirectory(prefix="flashsentry-gpg-", dir=ROOT) as gpg_home:
        home = Path(gpg_home)
        homedir_flag = _to_gpg_path(gpg_exe, home)
        pub = _to_gpg_path(gpg_exe, PUB_FILE)
        asc = _to_gpg_path(gpg_exe, ASC_FILE)
        manifest = _to_gpg_path(gpg_exe, MANIFEST)

        base = [gpg_exe, *_gpg_batch_args(), "--homedir", homedir_flag]
        imp = subprocess.run(
            [*base, "--import", pub],
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        if imp.returncode != 0:
            print(imp.stderr or imp.stdout, file=sys.stderr)
            return False, "GPG import failed"

        verify = subprocess.run(
            [*base, "--verify", asc, manifest],
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        if verify.returncode != 0:
            print(verify.stderr or verify.stdout, file=sys.stderr)
            return False, "GPG verify failed"

    return True, "OpenPGP signature valid"


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

    gpg_ok, gpg_note = _verify_openpgp()
    if not gpg_ok:
        return 1

    print(
        f"OK: embedded-manifest.json v{version}, "
        f"{len(entries)} entries, SHA-256 verified, {gpg_note}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
