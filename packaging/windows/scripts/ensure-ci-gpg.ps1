# Fast GnuPG setup for GitHub Actions windows-latest.
# Prefer Git for Windows gpg (works with native MSVC/Qt QProcess). MSYS2 gpg often fails
# when spawned from non-MSYS binaries unless the full MSYS DLL path is present.

$ErrorActionPreference = 'Stop'

function Test-GpgVerify {
    param([string]$GpgExe)
    $dir = Split-Path -Parent $GpgExe
    $temp = Join-Path $env:RUNNER_TEMP "flashsentry-gpg-smoke"
    New-Item -ItemType Directory -Force -Path $temp | Out-Null
    $gpgHomeDir = Join-Path $temp "home"
    New-Item -ItemType Directory -Force -Path $gpgHomeDir | Out-Null
    $prevPath = $env:PATH
    $env:PATH = "$dir;$prevPath"
    try {
        & $GpgExe --batch --no-tty --homedir $gpgHomeDir --version | Out-Null
        if ($LASTEXITCODE -ne 0) {
            return $false
        }
        return $true
    } finally {
        $env:PATH = $prevPath
    }
}

function Add-GpgToPath {
    param([string]$GpgExe)
    if (-not (Test-GpgVerify -GpgExe $GpgExe)) {
        Write-Host "Skipping unusable gpg: $GpgExe"
        return $false
    }
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
    return $true
}

$candidates = @(
    'C:\Program Files\Git\usr\bin\gpg.exe',
    'C:\Program Files\Git\mingw64\bin\gpg.exe'
)

foreach ($path in $candidates) {
    if ((Test-Path $path) -and (Add-GpgToPath -GpgExe $path)) {
        exit 0
    }
}

if (Test-Path 'C:\msys64\usr\bin\bash.exe') {
    if (-not (Test-Path 'C:\msys64\usr\bin\gpg.exe')) {
        Write-Host 'Installing gnupg via MSYS2 pacman...'
        & 'C:\msys64\usr\bin\bash.exe' -lc 'pacman -Sy --noconfirm gnupg'
    }
    if ((Test-Path 'C:\msys64\usr\bin\gpg.exe') -and (Add-GpgToPath -GpgExe 'C:\msys64\usr\bin\gpg.exe')) {
        exit 0
    }
}

$gpgOnPath = Get-Command gpg -ErrorAction SilentlyContinue
if ($gpgOnPath -and (Add-GpgToPath -GpgExe $gpgOnPath.Source)) {
    exit 0
}

throw 'No working gpg found on runner (Git/MSYS2 gpg smoke test failed)'
