# Packs a plugin folder into ID-VERSION-RUNTIME.svplugin. The folder holds plugin.ini and the library, and nothing else.
# The server checks the manifest first, so a package that this script makes passes the rules of the loader.
# The ZIP has a fixed order and a fixed time stamp, so the same folder gives the same bytes.
# Usage: pack.ps1 -Folder <plugin folder> -OutputDir <folder> [-ServerExe <subvulpes_server.exe>]
param([Parameter(Mandatory=$true)][string]$Folder,
      [Parameter(Mandatory=$true)][string]$OutputDir,
      [string]$ServerExe)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$folderPath = (Resolve-Path -LiteralPath $Folder).Path
if (-not $ServerExe) { $ServerExe = Join-Path $PSScriptRoot '..\..\build\server\subvulpes_server.exe' }
if (-not (Test-Path -LiteralPath $ServerExe -PathType Leaf)) { throw 'Pass -ServerExe with the server program, which holds the manifest rules.' }
$serverPath = (Resolve-Path -LiteralPath $ServerExe).Path

$manifestPath = Join-Path $folderPath 'plugin.ini'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'The folder has no plugin.ini.' }

# Ask the server about every problem before anything is written.
$report = @(& $serverPath --plugin-validate-folder $folderPath)
if ($LASTEXITCODE -ne 0) { throw ("The manifest is not valid:`n" + ($report -join "`n")) }

$values = @{}
foreach ($line in Get-Content -LiteralPath $manifestPath) {
    if ($line -cmatch '^(id|version|library|runtime)=(.+)$') { $values[$Matches[1]] = $Matches[2].Trim() }
}
$runtime = if ($values.ContainsKey('runtime')) { $values.runtime } else { 'native' }
foreach ($key in @('id', 'version', 'library')) {
    if (-not $values.ContainsKey($key)) { throw "The manifest has no $key." }
}
$package = "$($values.id)-$($values.version)-$runtime.svplugin"
if ($package -cnotmatch '^[A-Za-z0-9._+-]{1,120}$') { throw 'The package name has a character that is not safe.' }

# The folder holds exactly the manifest and the library.
$files = @(Get-ChildItem -LiteralPath $folderPath -Force | ForEach-Object Name)
$expected = @('plugin.ini', $values.library)
foreach ($name in $files) {
    if ($name -cnotin $expected) { throw "The folder holds a file that the package does not carry: $name" }
}
foreach ($name in $expected) {
    if ($name -cnotin $files) { throw "The folder does not hold $name." }
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$target = Join-Path (Resolve-Path -LiteralPath $OutputDir).Path $package
$stamp = [DateTimeOffset]::new(2000, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
$stream = [IO.File]::Open($target, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
try {
    $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in $expected) {
            $entry = $zip.CreateEntry($name, [IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = $stamp
            $output = $entry.Open()
            try {
                $bytes = [IO.File]::ReadAllBytes((Join-Path $folderPath $name))
                $output.Write($bytes, 0, $bytes.Length)
            } finally { $output.Dispose() }
        }
    } finally { $zip.Dispose() }
} finally { $stream.Dispose() }
$hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Output "PACKAGE`t$target`t$hash"
