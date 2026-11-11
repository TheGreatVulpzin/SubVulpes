# Makes an internal test package for the owner and friends. It is never a release and it never goes to the Workshop.
# It holds the server executable of a build, the test room plugin from a folder, and the steps to install them.
# Usage: make_internal.ps1 -TestRoomDir DIR [-BuildDir build/server] [-Label NAME]
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$TestRoomDir,
      [string]$BuildDir = 'build/server',
      [string]$Label = (Get-Date -Format 'yyyyMMdd-HHmm'))

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$buildPath = if ([IO.Path]::IsPathRooted($BuildDir)) { [IO.Path]::GetFullPath($BuildDir) } else { [IO.Path]::GetFullPath((Join-Path $repo $BuildDir)) }
$server = Join-Path $buildPath 'subvulpes_server.exe'
$roomDir = [IO.Path]::GetFullPath($TestRoomDir)
foreach ($required in @($server, (Join-Path $roomDir 'plugin.ini'), (Join-Path $roomDir 'testroom.dll'))) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing file: $required" }
}
if ($Label -notmatch '^[0-9A-Za-z._-]+$') { throw 'Use only letters, digits, dot, underscore and hyphen in the label.' }

$commit = (& git -C $repo rev-parse --short=12 HEAD).Trim()
$dirty = @(& git -C $repo status --porcelain --untracked-files=no).Count -gt 0
$roomVersion = ((Get-Content -LiteralPath (Join-Path $roomDir 'plugin.ini')) | Where-Object { $_ -match '^version=' } | Select-Object -First 1) -replace '^version=', ''
$dllHash = (Get-FileHash -LiteralPath (Join-Path $roomDir 'testroom.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
$serverHash = (Get-FileHash -LiteralPath $server -Algorithm SHA256).Hash.ToLowerInvariant()

$name = "SubVulpes-internal-$Label"
$output = Join-Path $repo 'build\internal'
$stage = Join-Path $output $name
if (Test-Path -LiteralPath $stage) { [IO.Directory]::Delete($stage, $true) }
$pluginDir = Join-Path $stage 'plugins\sv.testroom'
New-Item -ItemType Directory -Path $pluginDir -Force | Out-Null
Copy-Item -LiteralPath $server -Destination (Join-Path $stage 'subvulpes_server.exe')
Copy-Item -LiteralPath (Join-Path $roomDir 'plugin.ini') -Destination $pluginDir
Copy-Item -LiteralPath (Join-Path $roomDir 'testroom.dll') -Destination $pluginDir

$lines = @(
    "SubVulpes internal test build $Label",
    "Commit $commit$(if ($dirty) { ' (with changes that are not committed)' })",
    "Test room $roomVersion",
    "Server SHA-256 $serverHash",
    '',
    'This build is for tests only. It is not a release. The Workshop does not have it.',
    '',
    'Install',
    '1. Stop the server. Keep a copy of your subvulpes_server.exe.',
    '2. Extract this zip over the server folder. It replaces subvulpes_server.exe and adds plugins\sv.testroom.',
    '3. Open plugins\plugins.ini. Under [plugins], add this line:',
    '   sv.testroom=enabled',
    '4. Open plugins\native-approvals.ini. Under [native], add this line:',
    "   sv.testroom=$dllHash",
    '5. Start the server and join as an administrator. Type /test start in the game.',
    '   Answer each case with /test yes or /test no. Type /test go when a case asks for it.',
    '',
    'Remove',
    '1. Stop the server. Put your old subvulpes_server.exe back.',
    '2. Delete the folder plugins\sv.testroom and the two lines that you added.',
    '',
    'The plugins in your package (permissions, patchouli, ayalog, admcmd) keep working. They use the older API.')
[IO.File]::WriteAllLines((Join-Path $stage 'INSTALL.txt'), $lines)

$zip = Join-Path $output "$name.zip"
if (Test-Path -LiteralPath $zip) { [IO.File]::Delete($zip) }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($stage, $zip, [IO.Compression.CompressionLevel]::Optimal, $false)
$zipHash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$zip.sha256", "$zipHash  $name.zip`n")
Write-Output $zip
