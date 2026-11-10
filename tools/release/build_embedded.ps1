# Build the embedded plugins from the pinned Workshop source with the current SDK.
[CmdletBinding()]
param([string]$BuildDir = 'build/server', [string]$Version = 'v0.0.0-dev')

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$buildPath = if ([IO.Path]::IsPathRooted($BuildDir)) { [IO.Path]::GetFullPath($BuildDir) } else { [IO.Path]::GetFullPath((Join-Path $repo $BuildDir)) }
. (Join-Path $PSScriptRoot 'embedded_plugins.ps1')
. (Join-Path $PSScriptRoot 'server_data.ps1')
$config = Read-EmbeddedPlugins
$workshop = Join-Path $repo 'build\workshop'
$embedded = Join-Path $repo 'build\embedded'
$sdkOutput = Join-Path $repo 'build\embedded-sdk'

function Reset-BuildDirectory([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path)
    $buildRoot = [IO.Path]::GetFullPath((Join-Path $repo 'build')) + [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($buildRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'The output must stay inside build/.' }
    if (Test-Path -LiteralPath $full) {
        Assert-PlainPath $full
        foreach ($item in Get-ChildItem -LiteralPath $full -Force -Recurse) { Assert-PlainPath $item.FullName }
        Remove-Item -LiteralPath $full -Recurse -Force
    }
    New-Item -ItemType Directory -Path $full -Force | Out-Null
    Assert-PlainPath $full
}

# Invalidate old approvals before any step that can fail.
Reset-BuildDirectory $embedded
Reset-BuildDirectory $sdkOutput
& (Join-Path $repo 'tools\sdk\make_sdk.ps1') -Version $Version -OutputDir $sdkOutput `
    -Runner (Join-Path $buildPath 'subvulpes_plugin_test.exe') -ProbeDir (Join-Path $buildPath 'plugins\sv.probe') | Write-Host
$sdkZip = Join-Path $sdkOutput "SubVulpes-sdk-$Version.zip"
Expand-Archive -LiteralPath $sdkZip -DestinationPath $sdkOutput
$sdk = Join-Path $sdkOutput "SubVulpes-sdk-$Version"

$freshClone = -not (Test-Path -LiteralPath $workshop)
if ($freshClone) {
    & git clone --no-checkout --depth 1 https://github.com/TheGreatVulpzin/SubVulpes-Workshop.git $workshop
    if ($LASTEXITCODE -ne 0) { throw 'Cannot clone the Workshop.' }
}
Assert-PlainPath $workshop
$remote = & git -C $workshop remote get-url origin
if ($LASTEXITCODE -ne 0 -or $remote -cne 'https://github.com/TheGreatVulpzin/SubVulpes-Workshop.git') { throw 'The Workshop origin does not match.' }
# A new clone has no checkout yet, so only an older folder can hold local changes.
$changed = if ($freshClone) { $null } else { & git -C $workshop status --porcelain --untracked-files=no }
if ($LASTEXITCODE -ne 0 -or $changed) { throw 'The Workshop checkout has local changes.' }
& git -C $workshop fetch --depth 1 origin $config.Commit
if ($LASTEXITCODE -ne 0) { throw 'Cannot fetch the pinned Workshop commit.' }
& git -C $workshop checkout --detach $config.Commit
if ($LASTEXITCODE -ne 0) { throw 'Cannot check out the pinned Workshop commit.' }
$headCommit = & git -C $workshop rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $headCommit -cne $config.Commit) { throw 'The Workshop checkout does not match the pin.' }

# Use the same x86 compiler environment as the normal server build.
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    . (Join-Path $workshop 'tools\vcvars.ps1')
}
$previousSdk = $env:SUBVULPES_SDK
try {
    $env:SUBVULPES_SDK = $sdk
    foreach ($id in $config.Plugins.Keys) {
        $folder = Join-Path $workshop "official\plugins\$id"
        & (Join-Path $workshop 'tools\build_plugin.ps1') -Folder $folder | Write-Host
        if ($LASTEXITCODE -ne 0) { throw "The build of $id failed." }
        $manifest = Join-Path $folder 'plugin.ini'
        $declaredId = @(Select-String -LiteralPath $manifest -Pattern '^id=(.+)$')
        $library = @(Select-String -LiteralPath $manifest -Pattern '^library=(.+)$')
        if ($declaredId.Count -ne 1 -or $declaredId[0].Matches[0].Groups[1].Value -cne $id -or
            $library.Count -ne 1 -or $library[0].Matches[0].Groups[1].Value -cnotmatch '^[A-Za-z0-9_.-]+\.dll$') {
            throw "The manifest of $id has an invalid identifier or library."
        }
        $destination = Join-Path $embedded $id
        New-Item -ItemType Directory -Path $destination | Out-Null
        # The server package does not carry the icon file. The Workshop shows the icon, so the packaged manifest drops that line.
        $lines = @(Get-Content -LiteralPath $manifest | Where-Object { $_ -notmatch '^icon=' })
        [IO.File]::WriteAllLines((Join-Path $destination 'plugin.ini'), [string[]]$lines, [Text.Encoding]::ASCII)
        Copy-Item -LiteralPath (Join-Path $folder $library[0].Matches[0].Groups[1].Value) -Destination $destination
    }
} finally {
    $env:SUBVULPES_SDK = $previousSdk
}
& (Join-Path $repo 'tools\plugins\write_official_approvals.ps1') `
    -Official (Join-Path $PSScriptRoot 'embedded-plugins.txt') -PluginsDir $embedded -Output (Join-Path $embedded 'native-approvals.ini')
$profile = @('[plugins]') + @($config.Plugins.Keys | ForEach-Object { "$_=$($config.Plugins[$_])" })
[IO.File]::WriteAllLines((Join-Path $embedded 'plugins.ini'), [string[]]$profile, [Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $embedded 'workshop-commit.txt'), "$headCommit`n", [Text.Encoding]::ASCII)
Write-Host "Embedded plugins: $embedded (Workshop $headCommit)"
