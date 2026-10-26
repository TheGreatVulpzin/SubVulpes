param([Parameter(Mandatory=$true)][string]$Binary,
      [Parameter(Mandatory=$true)][string]$Id,
      [Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
if ($Id -cnotmatch '^[a-z0-9][a-z0-9_.-]{0,46}$' -or $Id.Contains('..')) {
    throw 'Invalid plugin identifier.'
}
$bytes = [IO.File]::ReadAllBytes([IO.Path]::GetFullPath($Binary))
if ($bytes.Length -lt 90 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) {
    throw 'The native build output is not a PE file.'
}
$offset = [BitConverter]::ToInt32($bytes, 60)
if ($offset -lt 0 -or $offset -gt ($bytes.Length - 26) -or
    [BitConverter]::ToUInt32($bytes, $offset) -ne 0x4550 -or
    [BitConverter]::ToUInt16($bytes, $offset + 4) -ne 0x14c -or
    [BitConverter]::ToUInt16($bytes, $offset + 24) -ne 0x10b -or
    -not ([BitConverter]::ToUInt16($bytes, $offset + 22) -band 0x2000)) {
    throw 'The native build output must be an x86 DLL.'
}
$entries = @{}
$entries[$Id] = (Get-FileHash -LiteralPath $Binary -Algorithm SHA256).Hash.ToLowerInvariant()
$lines = @('[native]') + @($entries.Keys | Sort-Object | ForEach-Object { "$_=$($entries[$_])" })
[IO.File]::WriteAllLines([IO.Path]::GetFullPath($Output), [string[]]$lines, [Text.Encoding]::ASCII)
