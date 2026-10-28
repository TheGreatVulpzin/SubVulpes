param(
    [Parameter(Mandatory = $true)][string]$ServerDir,
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\..\build\server')
)
# Packs the built test room, installs it into a stopped server folder, approves the exact DLL and enables it.
# This is a developer tool for a test server. It approves native code, so do not use it on a server with real players.
$ErrorActionPreference = 'Stop'
$ServerDir = (Resolve-Path -LiteralPath $ServerDir).Path
$module = Join-Path $BuildDir 'plugins\sv.testroom'
foreach ($name in 'plugin.ini', 'testroom.dll') {
    if (-not (Test-Path -LiteralPath (Join-Path $module $name))) { throw "Build the sv-testroom target first: $name is missing." }
}
if (Get-Process subvulpes_server -ErrorAction SilentlyContinue) { throw 'Stop the server first.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = Join-Path ([IO.Path]::GetTempPath()) ('sv.testroom-' + [Guid]::NewGuid().ToString('N') + '.zip')
$archive = [IO.Compression.ZipFile]::Open($zip, 'Create')
try {
    foreach ($name in 'plugin.ini', 'testroom.dll') {
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, (Join-Path $module $name), $name)
    }
} finally { $archive.Dispose() }
try {
    & (Join-Path $PSScriptRoot 'install_plugin.ps1') -ServerDir $ServerDir -Package $zip
    Push-Location $ServerDir
    try {
        $line = (& .\subvulpes_server.exe --plugin-hash sv.testroom | Select-Object -Last 1) -split "`t"
        if ($line.Count -lt 3 -or $line[2].Length -ne 64) { throw 'Could not read the hash of the module.' }
        & .\subvulpes_server.exe --plugin-approve sv.testroom $line[2]
        & .\subvulpes_server.exe --plugin-enable sv.testroom
        & .\subvulpes_server.exe --plugins-check
    } finally { Pop-Location }
} finally { Remove-Item -LiteralPath $zip -Force -ErrorAction SilentlyContinue }
