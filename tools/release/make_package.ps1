[CmdletBinding()]
param([string]$Version, [string]$DataDir)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
. (Join-Path $PSScriptRoot 'server_data.ps1')
if (-not $DataDir) { throw 'Pass -DataDir with the server data directory.' }
$dataFiles = @(Get-ServerDataFiles $DataDir)

function Read-Git([string[]]$Arguments) {
    $value = & git -C $repo @Arguments
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read Git metadata.' }
    return ($value -join "`n").Trim()
}

if (-not $Version) {
    if ($env:GITHUB_REF_TYPE -eq 'tag') {
        $Version = $env:GITHUB_REF_NAME
    } else {
        $Version = Read-Git -Arguments @('describe', '--tags', '--exact-match', 'HEAD')
    }
}
if ($Version -notmatch '^v?\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?$') {
    throw 'Use a version such as v0.1.0 or v0.1.0-rc1.'
}
if (-not $Version.StartsWith('v')) { $Version = 'v' + $Version }
$commit = Read-Git -Arguments @('rev-parse', '--short=12', 'HEAD')
$date = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
$name = "SubVulpes-server-$Version"
$output = Join-Path $repo 'build\package'
$package = Join-Path $output $name
$zip = Join-Path $output "$name.zip"
$checksum = "$zip.sha256"
# The official plugins come from plugins/official.ini. Each one needs its built manifest and DLL.
$official = [ordered]@{}
foreach ($line in Get-Content -LiteralPath (Join-Path $repo 'plugins\official.ini')) {
    if ($line -cmatch '^([a-z0-9][a-z0-9_.-]{0,46})=(enabled|disabled|required)$') {
        if ($official.Contains($Matches[1]) -or $Matches[1].Contains('..')) { throw 'official.ini repeats a plugin or holds a bad identifier.' }
        $official[$Matches[1]] = $Matches[2]
    }
}
if ($official.Count -eq 0) { throw 'official.ini lists no plugin.' }
$sources = [ordered]@{
    'subvulpes_server.exe' = 'build\server\subvulpes_server.exe'
    'subvulpes_launcher.exe' = 'build\launcher\subvulpes_launcher.exe'
    'LEIA-ME.txt' = 'tools\release\LEIA-ME.txt'
    'readme.txt' = 'tools\release\newspaper.txt'
    'plugins/plugins.ini' = 'plugins\plugins.ini'
    'plugins/native-approvals.ini' = 'build\server\plugins\native-approvals.ini'
    'install_plugin.ps1' = 'tools\plugins\install_plugin.ps1'
}
$directories = @('data', 'data/block', 'data/building', 'data/texture', 'data/weapon', 'plugins')
$libraries = @{}
foreach ($id in $official.Keys) {
    $built = "build\server\plugins\$id"
    $manifestFile = Join-Path $repo "$built\plugin.ini"
    if (-not (Test-Path -LiteralPath $manifestFile -PathType Leaf)) { throw "Build or source file is missing: $built\plugin.ini" }
    $library = (Select-String -LiteralPath $manifestFile -Pattern '^library=(.+)$' | Select-Object -First 1).Matches[0].Groups[1].Value.Trim()
    if ($library -cnotmatch '^[A-Za-z0-9_.-]+\.dll$') { throw "The manifest of $id names a bad library." }
    $libraries[$id] = "plugins/$id/$library"
    $sources["plugins/$id/plugin.ini"] = "$built\plugin.ini"
    $sources[$libraries[$id]] = "$built\$library"
    $directories += "plugins/$id"
}
$allowed = @($sources.Keys) + @('version.txt')
$allowed += @($dataFiles | ForEach-Object { "data/$($_.Relative)" })
$officialKeys = [string[]]@($official.Keys)
[Array]::Sort($officialKeys, [StringComparer]::Ordinal)
$approvalLines = @('[native]')
foreach ($id in $officialKeys) {
    $approvalLines += "$id=$((Get-FileHash -LiteralPath (Join-Path $repo $sources[$libraries[$id]]) -Algorithm SHA256).Hash.ToLowerInvariant())"
}
$approvalText = [IO.File]::ReadAllText((Join-Path $repo $sources['plugins/native-approvals.ini'])).Replace("`r", '').Trim()
if ($approvalText -cne ($approvalLines -join "`n")) { throw 'The official native approval does not match the built official plugins.' }
$profileText = [IO.File]::ReadAllText((Join-Path $repo $sources['plugins/plugins.ini'])).Replace("`r", '').Trim()
$expectedProfile = (@('[plugins]') + @($official.Keys | ForEach-Object { "$_=$($official[$_])" })) -join "`n"
if ($profileText -cne $expectedProfile) { throw 'plugins/plugins.ini does not match plugins/official.ini.' }

# Copy build outputs, package defaults and checked data files.
# The package has no config.txt: the launcher creates it with a new admin password.
foreach ($source in $sources.Values) {
    $path = Join-Path $repo $source
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Build or source file is missing: $source"
    }
    Assert-PlainPath $path
}
New-Item -ItemType Directory -Path $package -Force | Out-Null
Assert-PlainPath $package

function Assert-Package {
    $items = @(Get-ChildItem -LiteralPath $package -Force -Recurse)
    foreach ($item in $items) {
        $relative = $item.FullName.Substring($package.Length + 1).Replace('\', '/')
        Assert-PlainPath $item.FullName
        if ($item.PSIsContainer) {
            if ($relative -cnotin $directories) { throw "Forbidden package entry: $relative" }
            continue
        }
        if ($relative -cnotin $allowed) {
            throw "Forbidden package entry: $($item.Name)"
        }
        if ($item.Extension -eq '.txt') {
            $text = [IO.File]::ReadAllText($item.FullName)
            if ($text -match '(?im)^[ \t]*(?:serverpassword|password|passwd|token|secret|api_key)[ \t]*[=:][ \t]*\S+' -or
                $text -match '-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----|gh[pousr]_[A-Za-z0-9]{20,}') {
                throw "Private data detected in package text: $($item.Name)"
            }
        }
    }
}

# Refuse contamination from an earlier run instead of silently hiding it.
Assert-Package
foreach ($entry in $sources.GetEnumerator()) {
    $target = Join-Path $package $entry.Key
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($target)) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repo $entry.Value) -Destination $target -Force
}
foreach ($file in $dataFiles) {
    $target = Join-Path $package "data/$($file.Relative)"
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($target)) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $target -Force
}
[IO.File]::WriteAllText((Join-Path $package 'version.txt'),
    "$Version`ncommit $commit`nbuilt $date`n", [Text.UTF8Encoding]::new($false))
Assert-Package
if (@(Get-ChildItem -LiteralPath $package -Force -Recurse -File).Count -ne $allowed.Count) {
    throw 'The package file count is incorrect.'
}

# Reject game binaries even if someone renames them as a package executable.
$gameHashes = @(Get-Content -LiteralPath (Join-Path $repo 'tools\ci\game_binary_hashes.txt') |
    ForEach-Object { if ($_ -match '^([0-9a-fA-F]{64})\s') { $Matches[1] } })
foreach ($file in Get-ChildItem -LiteralPath $package -File -Recurse) {
    if ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -in $gameHashes) {
        throw "A game file cannot enter the package: $($file.Name)"
    }
    if ($file.Extension -in @('.exe', '.dll')) {
        $bytes = [IO.File]::ReadAllBytes($file.FullName)
        if ($bytes.Length -lt 64 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) {
            throw "Invalid executable: $($file.Name)"
        }
        $offset = [BitConverter]::ToInt32($bytes, 0x3c)
        if ($offset -lt 0 -or $offset -gt ($bytes.Length - 6) -or
            [BitConverter]::ToUInt32($bytes, $offset) -ne 0x4550 -or
            [BitConverter]::ToUInt16($bytes, $offset + 4) -ne 0x14c) {
            throw "Expected an x86 PE executable: $($file.Name)"
        }
        if ($file.Extension -eq '.dll' -and
            ([BitConverter]::ToUInt16($bytes, $offset + 22) -band 0x2000) -eq 0) {
            throw "Expected a DLL: $($file.Name)"
        }
    }
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
foreach ($path in @($zip, $checksum)) {
    if (Test-Path -LiteralPath $path) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Expected an output file: $path"
        }
        Assert-PlainPath $path
        Remove-Item -LiteralPath $path
    }
}
$archive = [IO.Compression.ZipFile]::Open($zip, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in $allowed) {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,
            (Join-Path $package $file), "$name/$file", [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally {
    $archive.Dispose()
}
$archive = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    $expected = @($allowed | ForEach-Object { "$name/$_" } | Sort-Object)
    $actual = @($archive.Entries | ForEach-Object { $_.FullName } | Sort-Object)
    if (Compare-Object $expected $actual) { throw 'The zip contains unexpected entries.' }
} finally {
    $archive.Dispose()
}
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($checksum, "$hash  $name.zip`n", [Text.Encoding]::ASCII)
Write-Host "Package: $package"
Write-Host "SHA256: $hash"
[pscustomobject]@{ Version = $Version; Package = $package; Zip = $zip; Checksum = $checksum }
