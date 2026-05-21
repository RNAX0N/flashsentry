# Contributing to FlashSentry

## Build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DFLASHSENTRY_BUILD_TESTS=ON ..
cmake --build . -j$(nproc)
ctest --output-on-failure
```

## Dependencies (Arch Linux)

```bash
sudo pacman -S qt6-base qt6-tools cmake base-devel openssl pkgconf systemd-libs
```

## Pull requests

- Keep changes focused; match existing C++/Qt style.
- Run `ctest` before submitting.
- Update README when behavior or CLI flags change.
