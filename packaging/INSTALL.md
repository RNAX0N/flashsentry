# Installing FlashSpartan on Arch Linux

## The `CMakeLists.txt` / `flashspartan-1.4.2.rNN.gHASH` error

If CMake says the source directory is something like:

`packaging/src/flashspartan-1.4.2.r74.g9aebc47`

and that folder has **no** `CMakeLists.txt`, you are on an **old PKGBUILD** (before PR #31) or have **stale `packaging/src`** from that era.

What happened:

1. `pkgver()` computed `1.4.2.r<commits>.g<hash>` (your commit hash appears in the path).
2. `prepare()` copied sources into `src/flashspartan-1.4.2`.
3. makepkg **renamed** that directory to `src/flashspartan-1.4.2.rNN.gHASH` **before** or **without** moving the rsynced files, so CMake ran in an **empty** tree.

**Fix (pacman package):**

```bash
git fetch origin main
git checkout main
git pull origin main   # must include merge 5e708db (PR #31) or later
grep -n '^pkgver()' packaging/PKGBUILD   # must print nothing

cd packaging
rm -rf pkg src
./build-package.sh -si
```

Success looks like: CMake uses `.../src/flashspartan-1.4.2` (no `.rNN.gHASH` in the path).

**Fix (no makepkg):**

```bash
cd packaging
./install-local.sh
```

## Recommended: install from your checkout

```bash
cd packaging
./build-package.sh -si
```

This rsyncs the repository into `src/flashspartan-<pkgver>` and builds there. Package version matches **`VERSION`** in the repo root (no git hash in the pacman `pkgver`).

## Release tarball install (maintainers)

After pushing tag `v1.4.2` (or whatever `VERSION` contains):

```bash
cd packaging
FLASHSPARTAN_RELEASE=1 makepkg -si
```

## Version alignment

| File | Role |
|------|------|
| `VERSION` | Release number (e.g. `1.5.4`) |
| `CMakeLists.txt` | Reads `VERSION` for `flashspartan --version` |
| `packaging/PKGBUILD` | `pkgver` should match `VERSION` for local builds (static; no `pkgver()` suffix) |
| `CHANGELOG.md` | Release notes |

## After install

```bash
sudo usermod -aG storage "$USER"
# log out and back in
systemctl --user enable --now flashspartan.service
flashspartan --version
```

**Note:** Do not use `rsync --exclude src` in the PKGBUILD — that excludes the project `src/` directory, not only makepkg's `packaging/src` folder.
