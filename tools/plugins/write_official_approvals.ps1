# Writes the approval file of the official plugins: one line for each plugin of plugins/official.ini, with the SHA-256
# of its built DLL. The server package carries the file, so the official plugins start without a manual approval.
param([Parameter(Mandatory=$true)][string]$Official,
      [Parameter(Mandatory=$true)][string]$PluginsDir,
      [Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ids = [ordered]@{}
foreach ($line in Get-Content -LiteralPath $Official) {
    if ($line -cmatch '^([a-z0-9][a-z0-9_.-]{0,46})=(enabled|disabled|required)$') {
        if ($ids.Contains($Matches[1]) -or $Matches[1].Contains('..')) { throw 'official.ini repeats a plugin or holds a bad identifier.' }
        $ids[$Matches[1]] = $Matches[2]
    }
}
if ($ids.Count -eq 0) { throw 'official.ini lists no plugin.' }

function Assert-X86Dll([byte[]]$bytes) {
    if ($bytes.Length -lt 90 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) { throw 'The native build output is not a PE file.' }
    $offset = [BitConverter]::ToInt32($bytes, 60)
    if ($offset -lt 0 -or $offset -gt ($bytes.Length - 26) -or
        [BitConverter]::ToUInt32($bytes, $offset) -ne 0x4550 -or
        [BitConverter]::ToUInt16($bytes, $offset + 4) -ne 0x14c -or
        [BitConverter]::ToUInt16($bytes, $offset + 24) -ne 0x10b -or
        -not ([BitConverter]::ToUInt16($bytes, $offset + 22) -band 0x2000)) {
        throw 'The native build output must be an x86 DLL.'
    }
}

$keys = [string[]]@($ids.Keys)
[Array]::Sort($keys, [StringComparer]::Ordinal)
$lines = @('[native]')
foreach ($id in $keys) {
    $manifest = Join-Path $PluginsDir "$id\plugin.ini"
    $match = Select-String -LiteralPath $manifest -Pattern '^library=(.+)$' | Select-Object -First 1
    if (-not $match) { throw "The manifest of $id names no library." }
    $binary = Join-Path $PluginsDir "$id\$($match.Matches[0].Groups[1].Value.Trim())"
    Assert-X86Dll ([IO.File]::ReadAllBytes($binary))
    $lines += "$id=$((Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant())"
}
[IO.File]::WriteAllLines([IO.Path]::GetFullPath($Output), [string[]]$lines, [Text.Encoding]::ASCII)
