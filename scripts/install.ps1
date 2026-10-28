param(
    [Parameter(Mandatory=$true)][string]$KoboDrive,
    [string]$Package
)
$ErrorActionPreference = 'Stop'
$version = '1.1.0'
$root = (Resolve-Path $KoboDrive).Path
$versionFile = Join-Path $root '.kobo/version'
if (-not (Test-Path $versionFile)) { throw 'Kobo storage is not mounted at that path.' }
$firmware = ((Get-Content $versionFile -Raw).Split(','))[2].Trim()
if ($firmware -notin @('5.18.270971', '6.0.279225')) { throw "No tested package for firmware $firmware" }
$asset = "Nickelite-$version-firmware-$firmware.zip"
$temp = Join-Path ([IO.Path]::GetTempPath()) ([IO.Path]::GetRandomFileName())
New-Item -ItemType Directory $temp | Out-Null
try {
    $zip = Join-Path $temp $asset
    if ($Package) {
        Copy-Item $Package $zip
    } else {
        if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { throw 'Install GitHub CLI (gh) to download the public release.' }
        gh release download "v$version" -R open-source-arena/Nickelite -p $asset -p SHA256SUMS -D $temp
        if ($LASTEXITCODE -ne 0) { throw 'Package download failed.' }
        $line = Get-Content (Join-Path $temp 'SHA256SUMS') | Where-Object { $_ -match [regex]::Escape($asset) }
        if (-not $line) { throw 'Release checksum is missing.' }
        $expected = ($line -split '\s+')[0].ToLowerInvariant()
        if ((Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'Release checksum mismatch.' }
    }
    $unpacked = Join-Path $temp 'package'
    Expand-Archive $zip $unpacked
    $archive = Join-Path $unpacked 'Kobo.tgz'
    $manifest = Join-Path $unpacked 'SHA256SUMS'
    if (-not ((Test-Path $archive) -and (Test-Path $manifest))) { throw 'Package is incomplete.' }
    $expected = ((Get-Content $manifest | Where-Object { $_ -match 'Kobo.tgz' }) -split '\s+')[0].ToLowerInvariant()
    if (-not $expected -or (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'Package checksum mismatch.' }
    $configDir = Join-Path $root '.adds/nm'
    if (Test-Path (Join-Path $configDir 'uninstall')) { throw 'Remove .adds/nm/uninstall before installing.' }
    New-Item -ItemType Directory -Force $configDir | Out-Null
    $cloud = Join-Path $unpacked 'cloud-shortcuts.conf'
    if ((Test-Path $cloud) -and -not (Test-Path (Join-Path $configDir 'cloud-shortcuts.conf'))) {
        $existing = Get-ChildItem $configDir -File | Where-Object { $_.Name -ne 'doc' -and -not $_.Name.StartsWith('.') } |
            ForEach-Object { Select-String -Path $_.FullName -Pattern 'nickel_open\s*:\s*library:(gdrive|dropbox)' -Quiet }
        if (-not $existing) { Copy-Item $cloud (Join-Path $configDir 'cloud-shortcuts.conf') }
    }
    Copy-Item $archive (Join-Path $root '.kobo/Kobo.tgz') -Force
    Write-Host "Staged Nickelite $version for Kobo firmware $firmware. Safely eject, then unplug the Kobo."
} finally {
    Remove-Item $temp -Recurse -Force -ErrorAction SilentlyContinue
}
