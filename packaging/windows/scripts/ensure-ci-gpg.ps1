# Fast GnuPG setup for GitHub Actions windows-latest.
# Chocolatey "gnupg" / "gpg4win" can take 15+ minutes; prefer tools already on the image.

$ErrorActionPreference = 'Stop'

function Add-GpgToPath {
    param([string]$GpgExe)
    $dir = Split-Path -Parent $GpgExe
    Write-Host "Using gpg: $GpgExe"
    & $GpgExe --version
    if ($env:GITHUB_ENV) {
        Add-Content -Path $env:GITHUB_ENV -Value "FLASHSENTRY_GPG_PROGRAM=$GpgExe"
    }
    if ($env:GITHUB_PATH) {
        Add-Content -Path $env:GITHUB_PATH -Value $dir
    } else {
        $env:PATH = "$dir;$env:PATH"
        $env:FLASHSENTRY_GPG_PROGRAM = $GpgExe
    }
}

if (Test-Path 'C:\msys64\usr\bin\bash.exe') {
    if (-not (Test-Path 'C:\msys64\usr\bin\gpg.exe')) {
        Write-Host 'Installing gnupg via MSYS2 pacman (usually faster than Chocolatey)...'
        & 'C:\msys64\usr\bin\bash.exe' -lc 'pacman -Sy --noconfirm gnupg'
    }
    if (Test-Path 'C:\msys64\usr\bin\gpg.exe') {
        Add-GpgToPath -GpgExe 'C:\msys64\usr\bin\gpg.exe'
        exit 0
    }
}

$candidates = @(
    'C:\Program Files\Git\usr\bin\gpg.exe',
    'C:\Program Files\Git\mingw64\bin\gpg.exe'
)

foreach ($path in $candidates) {
    if (Test-Path $path) {
        Add-GpgToPath -GpgExe $path
        exit 0
    }
}

$gpgOnPath = Get-Command gpg -ErrorAction SilentlyContinue
if ($gpgOnPath) {
    Add-GpgToPath -GpgExe $gpgOnPath.Source
    exit 0
}

throw 'gpg not found on runner and MSYS2 install did not produce gpg.exe'
