param(
    [Parameter(Mandatory=$true)][string]$KoboDrive,
    [string]$Package
)
$ErrorActionPreference = 'Stop'
$repo = 'open-source-arena/Nickelite'
$version = '1.3.0'
$asset = 'KFMon-Nickelite-Install.zip'
$root = (Resolve-Path $KoboDrive).Path
$versionFile = Join-Path $root '.kobo/version'
if (-not (Test-Path $versionFile)) { throw 'Kobo storage is not mounted at that path.' }
$firmware = ((Get-Content $versionFile -Raw).Split(','))[2].Trim()
if ($firmware -notin @('5.18.270971', '6.0.279225')) { throw "No tested KFMon updater for firmware $firmware" }
foreach ($name in @('Kobo.tgz', 'KoboRoot.tgz', 'update.tar', 'update.tar.tmp')) {
    if (Test-Path (Join-Path $root ".kobo/$name")) { throw "Another updater is staged at .kobo/$name. Let it finish before choosing KFMon." }
}
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { throw 'Install GitHub CLI (gh) to download and verify the public release asset.' }
$temp = Join-Path ([IO.Path]::GetTempPath()) ([IO.Path]::GetRandomFileName())
New-Item -ItemType Directory $temp | Out-Null
$stage = Join-Path $root '.kobo/update.tar.tmp'
try {
    if ($Package) {
        Copy-Item $Package (Join-Path $temp $asset)
        gh release download "v$version" --repo $repo --pattern SHA256SUMS --dir $temp
    } else {
        gh release download "v$version" --repo $repo --pattern $asset --pattern SHA256SUMS --dir $temp
    }
    if ($LASTEXITCODE -ne 0) { throw 'KFMon package or release checksum download failed.' }
    $zip = Join-Path $temp $asset
    $manifest = Join-Path $temp 'SHA256SUMS'
    if (-not ((Test-Path $zip) -and (Test-Path $manifest))) { throw 'KFMon package or checksum is missing.' }
    $line = Get-Content $manifest | Where-Object { $_ -match "\s$([regex]::Escape($asset))$" }
    if (-not $line) { throw 'KFMon release checksum is missing.' }
    $expected = ($line -split '\s+')[0].ToLowerInvariant()
    if ((Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'KFMon release checksum mismatch.' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead($zip)
    try {
        $entries = @($archive.Entries | ForEach-Object FullName)
        if (($entries.Count -ne 1) -or ($entries[0] -ne '.kobo/update.tar')) { throw 'KFMon package must contain only .kobo/update.tar.' }
        $entry = $archive.GetEntry('.kobo/update.tar')
        if ($entry.Length -le 0) { throw 'KFMon updater is empty.' }
        $input = $entry.Open()
        try {
            $output = [IO.File]::Open($stage, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
            try { $input.CopyTo($output); $output.Flush($true) }
            finally { $output.Dispose() }
        } finally { $input.Dispose() }
    } finally { $archive.Dispose() }
    Move-Item $stage (Join-Path $root '.kobo/update.tar')
    Write-Host "Staged KFMon $version updater for firmware $firmware. Safely eject and unplug the Kobo."
    Write-Host 'This is a separate updater cycle: do not stage it alongside Nickelite .kobo/Kobo.tgz.'
} finally {
    if (Test-Path $stage) { Remove-Item $stage -Force }
    Remove-Item $temp -Recurse -Force -ErrorAction SilentlyContinue
}
