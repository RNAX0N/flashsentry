# Downloads official USBPcap installer for CI/local packaging.
param(
    [string]$Version = "1.5.4.0",
    [string]$OutDir = "$PSScriptRoot/../third-party"
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$fileName = "USBPcapSetup-$Version.exe"
$outPath = Join-Path $OutDir $fileName
$urls = @(
    "https://desowin.org/usbpcap/$fileName",
    "https://github.com/desowin/USBpcap/releases/download/$Version/$fileName"
)

foreach ($url in $urls) {
    try {
        Write-Host "Downloading $url ..."
        Invoke-WebRequest -Uri $url -OutFile $outPath -UseBasicParsing
        if ((Get-Item $outPath).Length -gt 100000) {
            Write-Host "Saved $outPath"
            Write-Output $outPath
            exit 0
        }
    } catch {
        Write-Warning "Failed: $url"
    }
}

throw "Could not download USBPcap $Version"
