# Checks a plugin folder or package with the rules of the server. It prints every problem.
# Usage: validate_plugin.ps1 -Path <folder, .svplugin or .zip> -ServerExe <subvulpes_server.exe>
param([Parameter(Mandatory=$true)][string]$Path,
      [Parameter(Mandatory=$true)][string]$ServerExe)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem

$exe = (Resolve-Path -LiteralPath $ServerExe).Path
$item = Get-Item -LiteralPath $Path
$folder = $null
$temporary = $null
try {
    if ($item.PSIsContainer) {
        $folder = $item.FullName
    } else {
        $temporary = Join-Path ([IO.Path]::GetTempPath()) ('svplugin-' + [Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $temporary | Out-Null
        $zip = [IO.Compression.ZipFile]::OpenRead($item.FullName)
        try {
            # Only plain names at the root: no folder, no drive, no dot segment.
            foreach ($entry in $zip.Entries) {
                if ($entry.FullName -cnotmatch '^[A-Za-z0-9._-]{1,64}$' -or $entry.FullName.StartsWith('.')) {
                    throw "The package holds an entry with an unsafe name: $($entry.FullName)"
                }
                if ($entry.Length -gt 67108864) { throw 'A package entry exceeds 64 MiB.' }
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, (Join-Path $temporary $entry.FullName))
            }
        } finally { $zip.Dispose() }
        $folder = $temporary
    }
    & $exe --plugin-validate-folder $folder
    exit $LASTEXITCODE
} finally {
    if ($temporary -and (Test-Path -LiteralPath $temporary)) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
