# Prefer OpenSSL already baked into windows-latest; fall back to Chocolatey only if needed.

$ErrorActionPreference = 'Stop'

$roots = @(
    'C:\Program Files\OpenSSL',
    'C:\Program Files\OpenSSL-Win64'
)

foreach ($root in $roots) {
    $header = Join-Path $root 'include\openssl\ssl.h'
    if (Test-Path $header) {
        Write-Host "Using preinstalled OpenSSL at $root"
        if ($env:GITHUB_ENV) {
            Add-Content -Path $env:GITHUB_ENV -Value "OPENSSL_ROOT_DIR=$root"
        }
        exit 0
    }
}

Write-Host 'Preinstalled OpenSSL dev headers not found; installing via Chocolatey...'
choco install openssl -y --no-progress

foreach ($root in $roots) {
    if (Test-Path (Join-Path $root 'include\openssl\ssl.h')) {
        if ($env:GITHUB_ENV) {
            Add-Content -Path $env:GITHUB_ENV -Value "OPENSSL_ROOT_DIR=$root"
        }
        exit 0
    }
}

throw 'OpenSSL install did not produce expected layout under Program Files'
