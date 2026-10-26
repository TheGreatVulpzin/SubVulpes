[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ServerDir,
      [Parameter(Mandatory=$true)][string]$Package,
      [switch]$Inspect,
      [string]$ExpectedPackageHash)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Assert-Plain([string]$Path) {
    $candidate = [IO.Path]::GetFullPath($Path)
    while ($candidate) {
        $item = Get-Item -LiteralPath $candidate -Force -ErrorAction SilentlyContinue
        if ($null -ne $item -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Reparse paths are not allowed.'
        }
        $parent = [IO.Path]::GetDirectoryName($candidate)
        if ($parent -eq $candidate) { break }
        $candidate = $parent
    }
}

$serverRoot = [IO.Path]::GetFullPath($ServerDir).TrimEnd('\', '/')
Assert-Plain $serverRoot
if (-not (Test-Path -LiteralPath $serverRoot -PathType Container)) { throw 'The server folder does not exist.' }
function Assert-Target([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($serverRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'The target leaves the server folder.'
    }
    Assert-Plain $full
}
function Assert-FileTarget([string]$Path) {
    Assert-Target $Path
    if (Test-Path -LiteralPath $Path -PathType Container) { throw 'A file target is a directory.' }
}
function New-Directory([string]$Path) {
    Assert-Target $Path
    [IO.Directory]::CreateDirectory($Path) | Out-Null
    Assert-Target $Path
}
function Assert-Id([string]$Id) {
    if ($Id -cnotmatch '^[a-z0-9][a-z0-9._-]{0,46}$' -or $Id.Contains('..') -or $Id.EndsWith('.') -or
        $Id -cmatch '^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)') { throw 'Invalid module identifier.' }
}
function Read-Manifest([byte[]]$Bytes) {
    if (-not $Bytes.Length -or $Bytes.Length -gt 65536 -or @($Bytes | Where-Object { $_ -gt 127 -or $_ -eq 0 }).Count) {
        throw 'The manifest must contain at most 64 KiB of ASCII text.'
    }
    $values = @{}
    $section = $false
    foreach ($raw in ([Text.Encoding]::ASCII.GetString($Bytes) -split '\r?\n')) {
        $line = $raw.Trim()
        if (-not $line -or $line.StartsWith(';') -or $line.StartsWith('#')) { continue }
        if ($line -ceq '[plugin]' -and -not $section) { $section = $true; continue }
        if (-not $section -or $line -cnotmatch '^([a-z]+)=(.*)$') { throw 'Invalid manifest syntax.' }
        $key, $value = $Matches[1], $Matches[2]
        if ($key -notin @('id', 'version', 'library', 'api', 'live', 'dependencies') -or $values.ContainsKey($key)) {
            throw 'Invalid or duplicate manifest field.'
        }
        $values[$key] = $value
    }
    foreach ($key in @('id', 'version', 'library', 'api', 'live')) {
        if (-not $values.ContainsKey($key)) { throw 'A required manifest field is missing.' }
    }
    Assert-Id $values.id
    Assert-Id $values.library
    if ($values.library -cnotmatch '\.dll$' -or $values.library.Length -lt 5 -or
        $values.version -cnotmatch '^[A-Za-z0-9.+-]{1,23}$' -or $values.api -cne '1' -or $values.live -cnotmatch '^[01]$') {
        throw 'Invalid library, version, API or live setting.'
    }
    $dependencies = @()
    if ($values.ContainsKey('dependencies') -and $values.dependencies) {
        foreach ($id in $values.dependencies.Split(',')) {
            Assert-Id $id
            if ($id -ceq $values.id -or $id -cin $dependencies) { throw 'Invalid module dependency.' }
            $dependencies += $id
        }
    }
    if ($dependencies.Count -gt 8) { throw 'A module can have at most eight dependencies.' }
    return [pscustomobject]@{ Id=$values.id; Version=$values.version; Library=$values.library; Dependencies=$dependencies }
}
function Assert-Dll([byte[]]$Bytes) {
    if ($Bytes.Length -lt 90 -or $Bytes.Length -gt 67108864 -or $Bytes[0] -ne 0x4d -or $Bytes[1] -ne 0x5a) {
        throw 'The package binary must be an x86 PE DLL of at most 64 MiB.'
    }
    $offset = [BitConverter]::ToUInt32($Bytes, 60)
    if ($offset -gt ($Bytes.Length - 26) -or [BitConverter]::ToUInt32($Bytes, $offset) -ne 0x4550 -or
        [BitConverter]::ToUInt16($Bytes, $offset + 4) -ne 0x14c -or
        [BitConverter]::ToUInt16($Bytes, $offset + 24) -ne 0x10b -or
        -not ([BitConverter]::ToUInt16($Bytes, $offset + 22) -band 0x2000)) { throw 'The binary is not an x86 PE DLL.' }
}
function Read-Entry($Entry, [long]$Limit) {
    if ($Entry.Length -gt $Limit) { throw 'A ZIP entry exceeds its size limit.' }
    $inputStream = $Entry.Open()
    $outputStream = [IO.MemoryStream]::new()
    try {
        $buffer = New-Object byte[] 8192
        while (($read = $inputStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            if ($outputStream.Length + $read -gt $Limit) { throw 'A ZIP entry exceeds its size limit.' }
            $outputStream.Write($buffer, 0, $read)
        }
        if ($outputStream.Length -ne $Entry.Length) { throw 'The ZIP entry length is invalid.' }
        return ,$outputStream.ToArray()
    } finally { $inputStream.Dispose(); $outputStream.Dispose() }
}
function Read-Profile([string]$Text) {
    $entries = @{}
    $selected = $false
    $seen = $false
    foreach ($raw in ($Text -split '\r?\n')) {
        $line = $raw.Trim()
        if (-not $line -or $line.StartsWith(';') -or $line.StartsWith('#')) { continue }
        if ($line -match '^\[([^\]]+)\]$') {
            $selected = $Matches[1] -ieq 'plugins'
            if ($selected -and $seen) { throw 'The profile has duplicate plugin sections.' }
            if ($selected) { $seen = $true }
            continue
        }
        if (-not $selected) { continue }
        if ($line -cnotmatch '^([^=]+)=(required|enabled|disabled)$') { throw 'Invalid profile entry.' }
        $id, $state = $Matches[1], $Matches[2]
        Assert-Id $id
        if ($entries.ContainsKey($id)) { throw 'The profile has duplicate modules.' }
        $entries[$id] = $state
    }
    if ($entries.Count -gt 32) { throw 'The profile exceeds 32 modules.' }
    return ,$entries
}
function Write-Backup([string]$Path, [byte[]]$Manifest, [string]$Library, [byte[]]$Dll) {
    Assert-Target $Path
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    $archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($pair in @(@('plugin.ini', $Manifest), @($Library, $Dll))) {
            $entry = $archive.CreateEntry($pair[0])
            $out = $entry.Open()
            try { $out.Write($pair[1], 0, $pair[1].Length) } finally { $out.Dispose() }
        }
    } finally { $archive.Dispose(); $stream.Dispose() }
}

$packagePath = [IO.Path]::GetFullPath($Package)
Assert-Plain $packagePath
if ((Get-Item -LiteralPath $packagePath).Length -gt 73400320) { throw 'A module ZIP cannot exceed 70 MiB.' }
$zip = [IO.Compression.ZipFile]::OpenRead($packagePath)
try {
    $packageHash = (Get-FileHash -LiteralPath $packagePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($ExpectedPackageHash -and ($ExpectedPackageHash -cnotmatch '^[0-9a-f]{64}$' -or
        $ExpectedPackageHash -cne $packageHash)) { throw 'The package changed after inspection.' }
    if ($zip.Entries.Count -ne 2) { throw 'A package must contain only plugin.ini and its DLL.' }
    $names = @{}
    foreach ($entry in $zip.Entries) {
        $name = $entry.FullName
        $kind = ($entry.ExternalAttributes -shr 16) -band 0xf000
        if (-not $name -or $name -match '[/\\:]' -or $name.Contains('..') -or $names.ContainsKey($name) -or
            ($entry.ExternalAttributes -band 0x410) -or ($kind -ne 0 -and $kind -ne 0x8000)) {
            throw 'The ZIP contains an unsafe or duplicate entry.'
        }
        $names[$name] = $entry
    }
    if (-not $names.ContainsKey('plugin.ini') -or $names['plugin.ini'].FullName -cne 'plugin.ini') {
        throw 'The ZIP root must contain plugin.ini.'
    }
    [byte[]]$manifestBytes = Read-Entry $names['plugin.ini'] 65536
    $manifest = Read-Manifest $manifestBytes
    if (-not $names.ContainsKey($manifest.Library) -or $names[$manifest.Library].FullName -cne $manifest.Library) {
        throw 'The ZIP DLL does not match the manifest.'
    }
    [byte[]]$dllBytes = Read-Entry $names[$manifest.Library] 67108864
    Assert-Dll $dllBytes
} finally { $zip.Dispose() }

if ($Inspect) {
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try { $digest = [BitConverter]::ToString($algorithm.ComputeHash($dllBytes)).Replace('-', '').ToLowerInvariant() }
    finally { $algorithm.Dispose() }
    Write-Output "MODULE`t$($manifest.Id)`t$($manifest.Version)`t$digest`t$packageHash"
    return
}

$private = Join-Path $serverRoot 'user/plugins'
New-Directory $private
$lockPath = Join-Path $private 'host.lock'
Assert-Target $lockPath
$lock = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
$stage = $null
try {
    $public = Join-Path $serverRoot 'plugins'
    $profilePath = Join-Path $public 'plugins.ini'
    Assert-FileTarget $profilePath
    $hadProfile = Test-Path -LiteralPath $profilePath -PathType Leaf
    [byte[]]$profileBytes = @()
    $profileText = "[plugins]`r`n"
    if ($hadProfile) {
        $profileBytes = [IO.File]::ReadAllBytes($profilePath)
        $profileText = [Text.Encoding]::UTF8.GetString($profileBytes).TrimStart([char]0xfeff)
    }
    $profile = Read-Profile $profileText
    foreach ($id in $manifest.Dependencies) {
        if (-not $profile.ContainsKey($id)) { throw 'A module dependency is missing from the profile.' }
        $dependencyPath = Join-Path $public "$id/plugin.ini"
        Assert-Target $dependencyPath
        $dependency = Read-Manifest ([IO.File]::ReadAllBytes($dependencyPath))
        if ($dependency.Id -cne $id) { throw 'A dependency manifest has a different identifier.' }
        $dependencyDll = Join-Path $public "$id/$($dependency.Library)"
        Assert-Target $dependencyDll
        if (-not (Test-Path -LiteralPath $dependencyDll -PathType Leaf)) { throw 'A dependency DLL is missing.' }
    }
    if (-not $profile.ContainsKey($manifest.Id)) {
        if ($profile.Count -ge 32) { throw 'The profile exceeds 32 modules.' }
        $header = [regex]::Match($profileText, '(?im)^[ \t]*\[plugins\][ \t]*(?:\r?\n|$)')
        if ($header.Success) {
            $separator = if ($header.Value.EndsWith("`n")) { '' } else { "`r`n" }
            $profileText = $profileText.Insert($header.Index + $header.Length, "$separator$($manifest.Id)=disabled`r`n")
        } else { $profileText += "`r`n[plugins]`r`n$($manifest.Id)=disabled`r`n" }
    }
    $moduleDir = Join-Path $public $manifest.Id
    $manifestPath = Join-Path $moduleDir 'plugin.ini'
    $dllPath = Join-Path $moduleDir $manifest.Library
    Assert-FileTarget $manifestPath
    Assert-FileTarget $dllPath
    $hadModuleDir = Test-Path -LiteralPath $moduleDir -PathType Container
    $hadManifest = Test-Path -LiteralPath $manifestPath -PathType Leaf
    $oldManifestBytes = $null
    $oldDllBytes = $null
    $oldDllPath = $null
    $backupPath = $null
    if ($hadManifest) {
        $oldManifestBytes = [IO.File]::ReadAllBytes($manifestPath)
        $oldManifest = Read-Manifest $oldManifestBytes
        if ($oldManifest.Id -cne $manifest.Id) { throw 'The installed manifest has a different identifier.' }
        $oldDllPath = Join-Path $moduleDir $oldManifest.Library
        Assert-FileTarget $oldDllPath
        $oldDllBytes = [IO.File]::ReadAllBytes($oldDllPath)
        Assert-Dll $oldDllBytes
    }
    if ((Test-Path -LiteralPath $dllPath) -and $dllPath -ine $oldDllPath) { throw 'The target DLL already exists outside the installed manifest.' }
    $stage = Join-Path $private ('install-' + [Guid]::NewGuid().ToString('N'))
    New-Directory $stage
    [IO.File]::WriteAllBytes((Join-Path $stage 'plugin.ini'), $manifestBytes)
    [IO.File]::WriteAllBytes((Join-Path $stage $manifest.Library), $dllBytes)
    if ($hadManifest) {
        $backupDir = Join-Path $serverRoot 'backup/plugins'
        New-Directory $backupDir
        $backupPath = Join-Path $backupDir ($manifest.Id + '-' + [Guid]::NewGuid().ToString('N') + '.zip')
        Write-Backup $backupPath $oldManifestBytes $oldManifest.Library $oldDllBytes
    }
    $changed = $false
    try {
        New-Directory $moduleDir
        $changed = $true
        Assert-FileTarget $dllPath
        Copy-Item -LiteralPath (Join-Path $stage $manifest.Library) -Destination $dllPath -Force
        Assert-FileTarget $manifestPath
        Copy-Item -LiteralPath (Join-Path $stage 'plugin.ini') -Destination $manifestPath -Force
        if (-not $profile.ContainsKey($manifest.Id)) {
            Assert-FileTarget $profilePath
            [IO.File]::WriteAllText($profilePath, $profileText, [Text.UTF8Encoding]::new($false))
        }
        if ($oldDllPath -and $oldDllPath -ine $dllPath) {
            Assert-FileTarget $oldDllPath
            Remove-Item -LiteralPath $oldDllPath -Force
        }
    } catch {
        if ($changed) {
            Assert-FileTarget $dllPath
            if ($oldDllPath -and $oldDllPath -ieq $dllPath) { [IO.File]::WriteAllBytes($dllPath, $oldDllBytes) }
            elseif (Test-Path -LiteralPath $dllPath) { Remove-Item -LiteralPath $dllPath -Force }
            Assert-FileTarget $manifestPath
            if ($hadManifest) {
                [IO.File]::WriteAllBytes($manifestPath, $oldManifestBytes)
                Assert-FileTarget $oldDllPath
                [IO.File]::WriteAllBytes($oldDllPath, $oldDllBytes)
            } elseif (Test-Path -LiteralPath $manifestPath) { Remove-Item -LiteralPath $manifestPath -Force }
            Assert-FileTarget $profilePath
            if ($hadProfile) {
                $currentProfile = [IO.File]::ReadAllBytes($profilePath)
                if ([Convert]::ToBase64String($currentProfile) -cne [Convert]::ToBase64String($profileBytes)) {
                    [IO.File]::WriteAllBytes($profilePath, $profileBytes)
                }
            } elseif (Test-Path -LiteralPath $profilePath) { Remove-Item -LiteralPath $profilePath -Force }
            if (-not $hadModuleDir -and @(Get-ChildItem -LiteralPath $moduleDir -Force).Count -eq 0) {
                Assert-Target $moduleDir
                Remove-Item -LiteralPath $moduleDir
            }
        }
        throw
    }
    Write-Output "Installed module: $($manifest.Id). Native approval remains a separate action."
    if ($backupPath) { Write-Output "Rollback package: $backupPath" }
} finally {
    try {
        if ($stage -and (Test-Path -LiteralPath $stage)) {
            Assert-Target $stage
            Remove-Item -LiteralPath $stage -Recurse -Force
        }
    } finally { $lock.Dispose() }
}
