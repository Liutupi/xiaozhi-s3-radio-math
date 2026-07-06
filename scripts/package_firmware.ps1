param(
    [Parameter(Mandatory=$true)]
    [string]$AppBin,

    [string]$Version = "1.0.0",
    [string]$Board = "bread-compact-wifi-lcd",
    [string]$Chip = "esp32s3",
    [string]$FlashSize = "16MB",
    [string]$AppOffset = "0x100000"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$appBinPath = Resolve-Path $AppBin
$date = Get-Date -Format "yyyyMMdd-HHmmss"
$commit = ""

try {
    $commit = (git -C $repoRoot rev-parse --short HEAD).Trim()
} catch {
    $commit = "unknown"
}

$releaseName = "xiaozhi-s3-radio-math-v$Version-$Board-$date"
$stageDir = Join-Path $repoRoot "releases\$releaseName"
$firmwareDir = Join-Path $stageDir "firmware"
$zipPath = Join-Path $repoRoot "releases\$releaseName.zip"

New-Item -ItemType Directory -Force -Path $firmwareDir | Out-Null
Copy-Item -Force $appBinPath (Join-Path $firmwareDir "xiaozhi.bin")

$hash = (Get-FileHash (Join-Path $firmwareDir "xiaozhi.bin") -Algorithm SHA256).Hash.ToLowerInvariant()

$manifest = [ordered]@{
    project = "xiaozhi-s3-radio-math"
    version = $Version
    board = $Board
    chip = $Chip
    flash_size = $FlashSize
    app_offset = $AppOffset
    partition_table = "partitions.csv"
    build_date = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssK")
    commit = $commit
    sha256 = $hash
    preserved_features = @("xiaozhi", "internet_radio")
}

$manifest | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 (Join-Path $stageDir "manifest.json")
"$hash  firmware/xiaozhi.bin" | Set-Content -Encoding ASCII (Join-Path $stageDir "SHA256SUMS.txt")

@"
param(
    [string]`$Port = "COM4"
)

`$ErrorActionPreference = "Stop"
python -m esptool --chip $Chip -p `$Port -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size $FlashSize --flash_freq 80m $AppOffset firmware\xiaozhi.bin
"@ | Set-Content -Encoding UTF8 (Join-Path $stageDir "flash_app_only.ps1")

@"
# xiaozhi-s3-radio-math v$Version

## Contents

- Xiaozhi assistant
- Internet radio

## Flash

Use app-only flashing to preserve NVS/Wi-Fi settings.

```powershell
.\flash_app_only.ps1 -Port COM4
```

If the device is not on COM4, replace it with the actual serial port.

## Firmware Info

- Board: $Board
- Chip: $Chip
- Flash: $FlashSize
- App offset: $AppOffset
- SHA256: $hash
"@ | Set-Content -Encoding UTF8 (Join-Path $stageDir "README_FLASH.md")

if (Test-Path $zipPath) {
    Remove-Item $zipPath
}

Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zipPath

Write-Host "Release package created:"
Write-Host $zipPath
Write-Host "SHA256:"
Write-Host $hash
