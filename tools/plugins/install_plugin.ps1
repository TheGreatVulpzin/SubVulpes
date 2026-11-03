[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ServerDir,
      [Parameter(Mandatory=$true)][string]$Package,
      [switch]$Inspect,
      [string]$ExpectedPackageHash,
      [switch]$ServerInspect,
      [switch]$Approve,
      [string]$ExpectedPayloadHash,
      [string]$ExpectedManifestHash,
      [string]$ExpectedRuntime,
      [string]$ExpectedCapabilities,
      [switch]$NativeConfirmed)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
if ($ServerInspect -and -not $Inspect) { throw 'ServerInspect requires Inspect.' }
if ($Approve -and ($Inspect -or $ServerInspect)) { throw 'Inspection cannot save approval.' }
if ($Approve -and (-not $NativeConfirmed -or $ExpectedRuntime -cne 'native' -or
    $ExpectedPayloadHash -cnotmatch '^[0-9a-f]{64}$' -or $ExpectedManifestHash -cnotmatch '^[0-9a-f]{64}$' -or
    $ExpectedPackageHash -cnotmatch '^[0-9a-f]{64}$' -or -not $ExpectedCapabilities)) {
    throw 'The package review or native confirmation is incomplete.'
}

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
# The groups of digits at the start of a version, as numbers. The host compares versions the same way.
function Get-VersionGroups([string]$Version) {
    $groups = @(0, 0, 0, 0, 0, 0, 0, 0)
    $match = [regex]::Match($Version, '^[0-9]+(\.[0-9]+){0,7}')
    if (-not $match.Success) { return $null }
    $parts = $match.Value.Split('.')
    for ($i = 0; $i -lt $parts.Count; $i++) { $groups[$i] = [decimal]$parts[$i] }
    return $groups
}
function Test-VersionAtLeast([string]$Have, [string]$Want) {
    $a = Get-VersionGroups $Have
    if ($null -eq $a) { return $false }
    $b = Get-VersionGroups $Want
    for ($i = 0; $i -lt 8; $i++) {
        if ($a[$i] -ne $b[$i]) { return $a[$i] -gt $b[$i] }
    }
    return $true
}
function Read-IdList([string]$Text, [string]$Self, [bool]$AllowVersion) {
    $ids = @()
    $minimum = @{}
    if (-not $Text) { return [pscustomobject]@{ Ids=$ids; Minimum=$minimum } }
    foreach ($item in $Text.Split(',')) {
        $id, $version = $item, ''
        if ($item.Contains('@')) {
            if (-not $AllowVersion) { throw 'Invalid module conflict.' }
            $id, $version = $item.Split('@', 2)
            if ($version -cnotmatch '^[0-9]+(\.[0-9]+)*$' -or $version.Length -gt 23) { throw 'Invalid dependency version.' }
        }
        Assert-Id $id
        if ($id -ceq $Self -or $id -cin $ids) { throw 'Invalid module dependency or conflict.' }
        $ids += $id
        $minimum[$id] = $version
    }
    if ($ids.Count -gt 8) { throw 'A module can have at most eight dependencies and eight conflicts.' }
    return [pscustomobject]@{ Ids=$ids; Minimum=$minimum }
}
function Read-Manifest([byte[]]$Bytes) {
    if (-not $Bytes.Length -or $Bytes.Length -gt 65536 -or @($Bytes | Where-Object { $_ -gt 127 -or $_ -eq 0 }).Count) {
        throw 'The manifest must contain at most 64 KiB of ASCII text.'
    }
    $values = @{}
    $section = $false
    # The sections [setting.KEY] declare the settings that the administrator sets (docs/plugin-config.md). The host checks the
    # values of each field when it reads the manifest. The installer checks the shape: the names, the fields and the count.
    $settingSections = @{}
    $current = $null
    $settingKeys = @('type', 'default', 'min', 'max', 'maxlen', 'choices', 'label', 'help', 'live', 'admin')
    foreach ($raw in ([Text.Encoding]::ASCII.GetString($Bytes) -split '\r?\n')) {
        $line = $raw.Trim()
        if (-not $line -or $line.StartsWith(';') -or $line.StartsWith('#')) { continue }
        if ($line -ceq '[plugin]' -and -not $section) { $section = $true; continue }
        if ($section -and $line -cmatch '^\[setting\.([a-z][a-z0-9_]{0,31})\]$') {
            if ($settingSections.ContainsKey($Matches[1]) -or $settingSections.get_Count() -ge 32) { throw 'Invalid or duplicate setting section.' }
            $current = @{}
            $settingSections[$Matches[1]] = $current
            continue
        }
        if (-not $section -or $line -cnotmatch '^([a-z_]+)=(.*)$') { throw 'Invalid manifest syntax.' }
        $key, $value = $Matches[1], $Matches[2]
        if ($null -ne $current) {
            if ($key -notin $settingKeys -or $current.ContainsKey($key)) { throw 'Invalid or duplicate setting field.' }
            $current[$key] = $value
            continue
        }
        if ($key -notin @('id', 'version', 'library', 'api', 'live', 'dependencies', 'unsafe_events', 'conflicts', 'role',
                'runtime', 'capabilities', 'name', 'description', 'author', 'homepage',
                'icon', 'license', 'kind', 'side', 'server_min', 'tags', 'channel', 'entry') -or $values.ContainsKey($key)) {
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
    if ($values.ContainsKey('unsafe_events') -and $values.unsafe_events -cnotmatch '^[01]$') {
        throw 'Invalid unsafe_events setting.'
    }
    if ($values.ContainsKey('role') -and $values.role -cnotmatch '^(plugin|library)$') { throw 'Invalid module role.' }
    # The server checks the other store keys. The installer must know the icon, because the package carries that file.
    if ($values.ContainsKey('icon') -and ($values.icon -cnotmatch '^[a-z0-9][a-z0-9._-]{0,43}\.png$' -or $values.icon.Contains('..'))) {
        throw 'Invalid icon file name.'
    }
    # The server checks the values of the display keys and the capability list. This installer carries a DLL, so it takes native code only.
    if ($values.ContainsKey('runtime') -and $values.runtime -cne 'native') { throw 'This installer carries native plugins only.' }
    $dependencyList = Read-IdList $(if ($values.ContainsKey('dependencies')) { $values.dependencies } else { '' }) $values.id $true
    $conflictList = Read-IdList $(if ($values.ContainsKey('conflicts')) { $values.conflicts } else { '' }) $values.id $false
    foreach ($id in $conflictList.Ids) {
        if ($id -cin $dependencyList.Ids) { throw 'A module cannot depend on a module that it conflicts with.' }
    }
    return [pscustomobject]@{ Id=$values.id; Version=$values.version; Library=$values.library;
        Icon=$(if ($values.ContainsKey('icon')) { $values.icon } else { $null });
        Dependencies=$dependencyList.Ids; DependencyMinimum=$dependencyList.Minimum }
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
# The icon is a PNG of at most 128 KiB, square, from 64 to 512 pixels. The header decides and no decoder runs.
function Assert-Png([byte[]]$Bytes) {
    if ($Bytes.Length -lt 24 -or $Bytes.Length -gt 131072) { throw 'The icon must be a PNG file of at most 128 KiB.' }
    $signature = @(0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, 0x49, 0x48, 0x44, 0x52)
    for ($i = 0; $i -lt 16; $i++) { if ($Bytes[$i] -ne $signature[$i]) { throw 'The icon is not a PNG file.' } }
    $width = ([int64]$Bytes[16] * 16777216) + ([int64]$Bytes[17] * 65536) + ([int64]$Bytes[18] * 256) + [int64]$Bytes[19]
    $height = ([int64]$Bytes[20] * 16777216) + ([int64]$Bytes[21] * 65536) + ([int64]$Bytes[22] * 256) + [int64]$Bytes[23]
    if ($width -ne $height -or $width -lt 64 -or $width -gt 512) { throw 'The icon must be square, from 64 to 512 pixels.' }
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
    if ($entries.get_Count() -gt 32) { throw 'The profile exceeds 32 modules.' }
    return ,$entries
}
function Write-Backup([string]$Path, [byte[]]$Manifest, [string]$Library, [byte[]]$Dll, [string]$IconName = $null, [byte[]]$Icon = $null) {
    Assert-Target $Path
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    $archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        $pairs = @(@('plugin.ini', $Manifest), @($Library, $Dll))
        if ($IconName -and $null -ne $Icon) { $pairs += ,@($IconName, $Icon) }
        foreach ($pair in $pairs) {
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
    if ($zip.Entries.Count -lt 2 -or $zip.Entries.Count -gt 3) { throw 'A package must contain plugin.ini, its DLL and at most one icon.' }
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
    [byte[]]$iconBytes = $null
    if ($zip.Entries.Count -ne $(if ($manifest.Icon) { 3 } else { 2 })) { throw 'A package must contain only plugin.ini, its DLL and its icon.' }
    if ($manifest.Icon) {
        if (-not $names.ContainsKey($manifest.Icon) -or $names[$manifest.Icon].FullName -cne $manifest.Icon) {
            throw 'The ZIP icon does not match the manifest.'
        }
        [byte[]]$iconBytes = Read-Entry $names[$manifest.Icon] 131072
        Assert-Png $iconBytes
    }
} finally { $zip.Dispose() }

if ($Inspect -and $ServerInspect) {
    # The server owns manifest rules and review text. Reuse the checked package bytes.
    $tempParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
    $reviewStage = Join-Path $tempParent ('sv-inspect-' + [Guid]::NewGuid().ToString('N'))
    try {
        Assert-Plain $tempParent
        [IO.Directory]::CreateDirectory($reviewStage) | Out-Null
        Assert-Plain $reviewStage
        [IO.File]::WriteAllBytes((Join-Path $reviewStage 'plugin.ini'), $manifestBytes)
        [IO.File]::WriteAllBytes((Join-Path $reviewStage $manifest.Library), $dllBytes)
        if ($iconBytes) { [IO.File]::WriteAllBytes((Join-Path $reviewStage $manifest.Icon), $iconBytes) }
        $inspector = Join-Path $serverRoot 'subvulpes_server.exe'
        Assert-Plain $inspector
        & $inspector --plugin-inspect-folder $reviewStage
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    } finally {
        $resolvedStage = [IO.Path]::GetFullPath($reviewStage)
        if ([IO.Path]::GetDirectoryName($resolvedStage) -ine $tempParent -or
            [IO.Path]::GetFileName($resolvedStage) -cnotmatch '^sv-inspect-[0-9a-f]{32}$') {
            throw 'The inspection cleanup path is outside its temp folder.'
        }
        if (Test-Path -LiteralPath $resolvedStage) {
            Assert-Plain $resolvedStage
            Remove-Item -LiteralPath $resolvedStage -Recurse -Force
        }
    }
    return
}

if ($Inspect) {
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try { $digest = [BitConverter]::ToString($algorithm.ComputeHash($dllBytes)).Replace('-', '').ToLowerInvariant() }
    finally { $algorithm.Dispose() }
    Write-Output "MODULE`t$($manifest.Id)`t$($manifest.Version)`t$digest`t$packageHash"
    return
}

if ($Approve -and -not (Test-Path -LiteralPath (Join-Path $serverRoot 'subvulpes_server.exe') -PathType Leaf)) {
    throw 'Approval needs the server executable in the server folder.'
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
        if ($manifest.DependencyMinimum[$id] -and -not (Test-VersionAtLeast $dependency.Version $manifest.DependencyMinimum[$id])) {
            throw 'A module dependency has a lower version than the module needs.'
        }
        $dependencyDll = Join-Path $public "$id/$($dependency.Library)"
        Assert-Target $dependencyDll
        if (-not (Test-Path -LiteralPath $dependencyDll -PathType Leaf)) { throw 'A dependency DLL is missing.' }
    }
    if (-not $profile.ContainsKey($manifest.Id)) {
        if ($profile.get_Count() -ge 32) { throw 'The profile exceeds 32 modules.' }
        $header = [regex]::Match($profileText, '(?im)^[ \t]*\[plugins\][ \t]*(?:\r?\n|$)')
        if ($header.Success) {
            $separator = if ($header.Value.EndsWith("`n")) { '' } else { "`r`n" }
            $profileText = $profileText.Insert($header.Index + $header.Length, "$separator$($manifest.Id)=disabled`r`n")
        } else { $profileText += "`r`n[plugins]`r`n$($manifest.Id)=disabled`r`n" }
    }
    $moduleDir = Join-Path $public $manifest.Id
    $manifestPath = Join-Path $moduleDir 'plugin.ini'
    $dllPath = Join-Path $moduleDir $manifest.Library
    $iconPath = if ($manifest.Icon) { Join-Path $moduleDir $manifest.Icon } else { $null }
    Assert-FileTarget $manifestPath
    Assert-FileTarget $dllPath
    if ($iconPath) { Assert-FileTarget $iconPath }
    $hadModuleDir = Test-Path -LiteralPath $moduleDir -PathType Container
    $hadManifest = Test-Path -LiteralPath $manifestPath -PathType Leaf
    $oldManifestBytes = $null
    $oldDllBytes = $null
    $oldDllPath = $null
    $oldIconPath = $null
    $oldIconBytes = $null
    $backupPath = $null
    if ($hadManifest) {
        $oldManifestBytes = [IO.File]::ReadAllBytes($manifestPath)
        $oldManifest = Read-Manifest $oldManifestBytes
        if ($oldManifest.Id -cne $manifest.Id) { throw 'The installed manifest has a different identifier.' }
        $oldDllPath = Join-Path $moduleDir $oldManifest.Library
        Assert-FileTarget $oldDllPath
        $oldDllBytes = [IO.File]::ReadAllBytes($oldDllPath)
        Assert-Dll $oldDllBytes
        if ($oldManifest.Icon) {
            $oldIconPath = Join-Path $moduleDir $oldManifest.Icon
            Assert-FileTarget $oldIconPath
            if (Test-Path -LiteralPath $oldIconPath -PathType Leaf) { $oldIconBytes = [IO.File]::ReadAllBytes($oldIconPath) }
        }
    }
    if ((Test-Path -LiteralPath $dllPath) -and $dllPath -ine $oldDllPath) { throw 'The target DLL already exists outside the installed manifest.' }
    if ($iconPath -and (Test-Path -LiteralPath $iconPath) -and $iconPath -ine $oldIconPath) { throw 'The target icon already exists outside the installed manifest.' }
    $stage = Join-Path $private ('install-' + [Guid]::NewGuid().ToString('N'))
    New-Directory $stage
    [IO.File]::WriteAllBytes((Join-Path $stage 'plugin.ini'), $manifestBytes)
    [IO.File]::WriteAllBytes((Join-Path $stage $manifest.Library), $dllBytes)
    if ($iconBytes) { [IO.File]::WriteAllBytes((Join-Path $stage $manifest.Icon), $iconBytes) }
    # The server holds the one set of manifest rules. Ask it, when the server program is in the folder.
    $validator = Join-Path $serverRoot 'subvulpes_server.exe'
    if (Test-Path -LiteralPath $validator -PathType Leaf) {
        Assert-FileTarget $validator
        $report = if ($Approve) { @(& $validator --plugin-inspect-folder $stage) } else { @(& $validator --plugin-validate-folder $stage) }
        if ($LASTEXITCODE -ne 0) { throw ('The server refused the manifest: ' + (($report | Select-Object -First 3) -join ' | ')) }
        if ($Approve) {
            $identityRows = @($report | Where-Object { $_ -clike "INSPECT`t*" })
            $manifestRows = @($report | Where-Object { $_ -clike "MANIFEST`t*" })
            if ($identityRows.Count -ne 1 -or $manifestRows.Count -ne 1) { throw 'The server returned an incomplete review.' }
            $identityFields = $identityRows[0].Split([char]9)
            $manifestFields = $manifestRows[0].Split([char]9)
            $expectedCaps = if ($ExpectedCapabilities -ceq '-') { '' } else { $ExpectedCapabilities }
            if ($identityFields.Count -ne 5 -or $manifestFields.Count -ne 10 -or
                $identityFields[2] -cne $manifest.Id -or $identityFields[3] -cne $ExpectedPayloadHash -or
                $identityFields[4] -cne $ExpectedManifestHash -or $manifestFields[4] -cne $ExpectedRuntime -or
                $manifestFields[5] -cne $expectedCaps) { throw 'The plugin changed after review.' }
        }
    }
    if ($hadManifest) {
        $backupDir = Join-Path $serverRoot 'backup/plugins'
        New-Directory $backupDir
        $backupPath = Join-Path $backupDir ($manifest.Id + '-' + [Guid]::NewGuid().ToString('N') + '.zip')
        Write-Backup $backupPath $oldManifestBytes $oldManifest.Library $oldDllBytes $oldManifest.Icon $oldIconBytes
    }
    $changed = $false
    try {
        New-Directory $moduleDir
        $changed = $true
        Assert-FileTarget $dllPath
        Copy-Item -LiteralPath (Join-Path $stage $manifest.Library) -Destination $dllPath -Force
        if ($iconPath) {
            Assert-FileTarget $iconPath
            Copy-Item -LiteralPath (Join-Path $stage $manifest.Icon) -Destination $iconPath -Force
        }
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
        if ($oldIconPath -and $oldIconPath -ine $iconPath -and (Test-Path -LiteralPath $oldIconPath -PathType Leaf)) {
            Assert-FileTarget $oldIconPath
            Remove-Item -LiteralPath $oldIconPath -Force
        }
        # Approve last so a refused review leaves the saved approval intact.
        if ($Approve) {
            Push-Location -LiteralPath $serverRoot
            try {
                $approvalReport = @(& $validator --plugin-trust $manifest.Id $ExpectedPayloadHash $ExpectedManifestHash `
                    $ExpectedRuntime $ExpectedCapabilities $ExpectedPackageHash --native-confirmed)
                if ($LASTEXITCODE -ne 0) { throw 'The server refused approval. The installed files will return to their previous state.' }
            } finally { Pop-Location }
        }
    } catch {
        if ($changed) {
            Assert-FileTarget $dllPath
            if ($oldDllPath -and $oldDllPath -ieq $dllPath) { [IO.File]::WriteAllBytes($dllPath, $oldDllBytes) }
            elseif (Test-Path -LiteralPath $dllPath) { Remove-Item -LiteralPath $dllPath -Force }
            if ($iconPath) {
                Assert-FileTarget $iconPath
                if ($oldIconPath -and $oldIconPath -ieq $iconPath -and $null -ne $oldIconBytes) { [IO.File]::WriteAllBytes($iconPath, $oldIconBytes) }
                elseif (Test-Path -LiteralPath $iconPath) { Remove-Item -LiteralPath $iconPath -Force }
            }
            if ($oldIconPath -and $oldIconPath -ine $iconPath -and $null -ne $oldIconBytes) {
                Assert-FileTarget $oldIconPath
                [IO.File]::WriteAllBytes($oldIconPath, $oldIconBytes)
            }
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
    if ($Approve) { Write-Output "Installed and approved module: $($manifest.Id)." }
    else { Write-Output "Installed module: $($manifest.Id). Native approval remains a separate action." }
    if ($backupPath) { Write-Output "Rollback package: $backupPath" }
} finally {
    try {
        if ($stage -and (Test-Path -LiteralPath $stage)) {
            Assert-Target $stage
            Remove-Item -LiteralPath $stage -Recurse -Force
        }
    } finally { $lock.Dispose() }
}
