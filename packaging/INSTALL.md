# Installing FlashSentry on Arch Linux

## Why `makepkg` used to fetch `archive/v1.4.2`

The old `PKGBUILD` always downloaded:

`https://github.com/RNAX0N/flashsentry/archive/v1.4.2.tar.gz`

That only works when a matching **git tag** `v1.4.2` exists on GitHub. The repo had CMake/CHANGELOG at **1.4.2** on `main`, but the newest tag was **v1.1.4**, so the download failed (404) even though the URL looked plausible.

## Recommended: install from your checkout

From the repository root:

```bash
cd packaging
./build-package.sh -si
```

This **rsyncs the parent directory** into the build (no `git+file://` download).

If an older PKGBUILD failed with *“is not a clone of file://…packaging/..”*, run `./build-package.sh -C` once to clear `pkg/` and `src/`, then `./build-package.sh -si` again. The package version is `1.4.2.r<commits>.g<hash>` from `VERSION` plus git metadata.

## Release tarball install (maintainers)

After pushing tag `v1.4.2` (or whatever `VERSION` contains):

```bash
cd packaging
FLASHSENTRY_RELEASE=1 makepkg -si
```

## Version alignment

| File | Role |
|------|------|
| `VERSION` | Single source for release number (e.g. `1.4.2`) |
| `CMakeLists.txt` | Reads `VERSION` for `PROJECT_VERSION` / `flashsentry --version` |
| `packaging/PKGBUILD` | `pkgver` base from `VERSION`; git builds append `.rN.gHASH` |
| `CHANGELOG.md` | Human-readable release notes |

## After install

```bash
sudo usermod -aG storage "$USER"
# log out and back in
systemctl --user enable --now flashsentry.service
flashsentry --version
```
