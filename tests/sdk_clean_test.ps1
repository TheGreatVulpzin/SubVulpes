# Test the SDK outside the source tree, with only Windows tools on PATH.
[CmdletBinding()]
param([string]$BuildDir, [string]$SdkZip)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\', '/')
if (-not $BuildDir) { $BuildDir = Join-Path $repo 'build\server' }
$build = [IO.Path]::GetFullPath($BuildDir)
$serverSource = Join-Path $build 'subvulpes_server.exe'
if (-not (Test-Path -LiteralPath $serverSource -PathType Leaf)) { throw 'Build the server first.' }
if (-not $SdkZip) {
    $packageDir = Join-Path $build ('tests\sdk-clean-packages\' + [Guid]::NewGuid().ToString('N'))
    $line = & (Join-Path $repo 'tools\sdk\make_sdk.ps1') -Version v0.0.0-clean-test -OutputDir $packageDir `
        -Runner (Join-Path $build 'subvulpes_plugin_test.exe') -ProbeDir (Join-Path $build 'plugins\sv.probe')
    if ($line -cnotmatch "^SDK`t(.+)`t[0-9a-f]{64}$") { throw 'The SDK script gave no package path.' }
    $SdkZip = $Matches[1]
}
$zipSource = (Resolve-Path -LiteralPath $SdkZip).Path

# Use the user temp folder, even when the parent process puts TEMP inside build/.
$tempBase = [IO.Path]::GetFullPath((Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Temp'))
if ($tempBase.StartsWith($repo + '\', [StringComparison]::OrdinalIgnoreCase) -or $tempBase -ieq $repo) {
    throw 'The clean test needs a temp folder outside the repository.'
}
$root = Join-Path $tempBase ('sv-sdk-clean-' + [Guid]::NewGuid().ToString('N'))
if (Test-Path -LiteralPath $root) { throw 'The clean test folder must be new.' }
[IO.Directory]::CreateDirectory($root) | Out-Null
$saved = @{}
$locationPushed = $false
try {
    Copy-Item -LiteralPath $zipSource -Destination (Join-Path $root 'sdk.zip')
    $serverDir = Join-Path $root 'server'
    [IO.Directory]::CreateDirectory($serverDir) | Out-Null
    $server = Join-Path $serverDir 'subvulpes_server.exe'
    Copy-Item -LiteralPath $serverSource -Destination $server
    [IO.Compression.ZipFile]::ExtractToDirectory((Join-Path $root 'sdk.zip'), (Join-Path $root 'unpacked'))
    $sdkFolders = @(Get-ChildItem -LiteralPath (Join-Path $root 'unpacked') -Directory)
    if ($sdkFolders.Count -ne 1) { throw 'The SDK ZIP must hold one folder.' }
    $sdk = $sdkFolders[0].FullName

    # Clear compiler state so build.bat must find MSVC with vswhere.
    $names = @('PATH', 'TEMP', 'TMP', 'VCVARS', 'INCLUDE', 'LIB', 'LIBPATH', 'CL', '_CL_', 'LINK', '_LINK_',
        'VCINSTALLDIR', 'VSINSTALLDIR', 'WindowsSdkDir', 'WindowsSDKVersion')
    $names += @(Get-ChildItem Env: | Where-Object { $_.Name -match '^(VSCMD_|__VSCMD)' } | ForEach-Object Name)
    foreach ($name in ($names | Select-Object -Unique)) {
        $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $null, 'Process')
    }
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\WindowsPowerShell\v1.0"
    $env:TEMP = Join-Path $root 'tmp'
    $env:TMP = $env:TEMP
    [IO.Directory]::CreateDirectory($env:TEMP) | Out-Null
    Push-Location $sdk
    $locationPushed = $true
    if ((Get-Location).Path.StartsWith($repo + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'The working folder is still inside the repository.'
    }
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) { throw 'MSVC is still on the clean PATH.' }

    & $env:ComSpec /d /c 'template\build.bat'
    if ($LASTEXITCODE -ne 0) { throw 'The clean template build failed.' }
    $line = & (Join-Path $sdk 'tools\pack.ps1') -Folder (Join-Path $sdk 'template\out') `
        -OutputDir (Join-Path $root 'dist') -ServerExe $server
    if ($line -cnotmatch "^PACKAGE`t(.+)`t[0-9a-f]{64}$") { throw 'The pack script gave no package path.' }
    $package = $Matches[1]
    & (Join-Path $sdk 'tools\install_plugin.ps1') -ServerDir $serverDir -Package $package
    if (-not (Test-Path -LiteralPath (Join-Path $serverDir 'plugins\my.hello\hello.dll'))) {
        throw 'The clean install did not copy hello.dll.'
    }

    # Run the documented approval commands against the installed DLL.
    Push-Location $serverDir
    try {
        $report = @(& $server --plugin-hash my.hello)
        if ($LASTEXITCODE -ne 0) { throw 'The server could not hash the installed DLL.' }
        $hashLine = @($report | Where-Object { $_ -cmatch "SHA256`tmy\.hello`t([0-9a-f]{64})$" })
        if ($hashLine.Count -ne 1) { throw 'The server gave no unique SHA256 line.' }
        $hash = ($hashLine[0] -split "`t")[-1]
        & $server --plugin-approve my.hello $hash
        if ($LASTEXITCODE -ne 0) { throw 'The server refused the DLL approval.' }
        & $server --plugin-enable my.hello
        if ($LASTEXITCODE -ne 0) { throw 'The server could not enable the plugin.' }
        & $server --plugins-check
        if ($LASTEXITCODE -ne 0) { throw 'The installed plugin did not start.' }
    } finally { Pop-Location }

    $run = @(& (Join-Path $sdk 'tools\run_plugin_test.ps1') -Plugin (Join-Path $serverDir 'plugins\my.hello') `
        -Scenario (Join-Path $sdk 'template\test.svtest') -ServerExe $server)
    if ($LASTEXITCODE -ne 0 -or $run[-1] -cnotlike 'PASS test.svtest*') {
        throw ("The clean scenario failed:`n" + ($run -join "`n"))
    }
    $run | Write-Output
    Write-Output 'SDK_CLEAN_PASS'
} finally {
    if ($locationPushed) { Pop-Location }
    foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
    # Delete only this test folder under the checked temp parent.
    $resolvedRoot = [IO.Path]::GetFullPath($root)
    if ([IO.Path]::GetDirectoryName($resolvedRoot) -ine $tempBase -or
        [IO.Path]::GetFileName($resolvedRoot) -cnotmatch '^sv-sdk-clean-[0-9a-f]{32}$') {
        throw 'The clean test cleanup path is outside its temp folder.'
    }
    Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
}
