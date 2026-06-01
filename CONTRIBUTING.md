# Contributing to FlashSpartan

Thank you for your interest in contributing.

## Before you start

- Read [README.md](README.md) for build instructions
- Read [CLAUDE.md](CLAUDE.md) for architecture and component overview
- User-facing behavior is documented in [docs/USER_GUIDE.md](docs/USER_GUIDE.md)

## Development setup

```bash
sudo pacman -S qt6-base qt6-tools cmake base-devel openssl pkgconf gnupg

git clone https://github.com/RNAX0N/flashsentry.git
cd flashspartan
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DFLASHSPARTAN_BUILD_TESTS=ON ..
cmake --build . -j$(nproc)
ctest --output-on-failure
```

## Pull requests

1. Fork and create a feature branch from `main`
2. Keep changes focused; match existing code style
3. Update [CHANGELOG.md](CHANGELOG.md) and docs if behavior changes
4. Ensure `cmake --build . --target flashspartan` succeeds and all `ctest` targets pass (6 tests: types, database, merkle, iso catalog, iso checksum, autostart)
5. Open a PR with a clear description of user-visible impact

Optional: add UI screenshots under `docs/images/` (see README Screenshots section).

## Adding ISO publisher support

See [docs/VERIFICATION.md](docs/VERIFICATION.md#extending-publisher-support) and edit `src/IsoCatalog.cpp`.

## Questions

Open a [GitHub issue](https://github.com/RNAX0N/flashsentry/issues) for bugs or feature discussion.
