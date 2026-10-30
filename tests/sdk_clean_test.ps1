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
    Copy-Item -LiteralPath (Join-Path $sdk 'tools\install_plugin.ps1') -Destination (Join-Path $serverDir 'install_plugin.ps1')

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
    # Read the same report and use the same approved-install path as the launcher dialog.
    $inspection = @(& $server --plugin-inspect $package)
    if ($LASTEXITCODE -ne 0) { throw 'The server could not inspect the clean package.' }
    $manifestRows = @($inspection | Where-Object { $_ -clike "MANIFEST`t*" })
    $identityRows = @($inspection | Where-Object { $_ -clike "INSPECT`t*" })
    $bundleRows = @($inspection | Where-Object { $_ -clike "PACKAGE`t*" })
    if ($manifestRows.Count -ne 1 -or $identityRows.Count -ne 1 -or $bundleRows.Count -ne 1 -or
        @($inspection | Where-Object { $_ -clike "DANGER`tnative`thighest`t*" }).Count -ne 1) {
        throw 'The clean package review is incomplete.'
    }
    $declaration = $manifestRows[0].Split([char]9)
    $identity = $identityRows[0].Split([char]9)
    $bundle = $bundleRows[0].Split([char]9)
    if ($declaration.Count -ne 10 -or $identity.Count -ne 5 -or $bundle.Count -ne 2 -or
        $declaration[1] -cne 'my.hello' -or $declaration[4] -cne 'native' -or $identity[2] -cne 'my.hello' -or
        $identity[3] -cnotmatch '^[0-9a-f]{64}$' -or $identity[4] -cnotmatch '^[0-9a-f]{64}$' -or
        $bundle[1] -cnotmatch '^[0-9a-f]{64}$') { throw 'The clean review has invalid identity or hashes.' }
    $capabilities = if ($declaration[5]) { $declaration[5] } else { '-' }
    & (Join-Path $sdk 'tools\install_plugin.ps1') -ServerDir $serverDir -Package $package -Approve `
        -ExpectedPackageHash $bundle[1] -ExpectedPayloadHash $identity[3] -ExpectedManifestHash $identity[4] `
        -ExpectedRuntime $declaration[4] -ExpectedCapabilities $capabilities -NativeConfirmed
    if (-not (Test-Path -LiteralPath (Join-Path $serverDir 'plugins\my.hello\hello.dll'))) {
        throw 'The clean install did not copy hello.dll.'
    }
    $approvalPath = Join-Path $serverDir 'user\plugins\approvals.ini'
    $approvalLines = @(Get-Content -LiteralPath $approvalPath | Where-Object { $_ -clike 'my.hello=*' })
    if ($approvalLines.Count -ne 1) { throw 'The clean install saved no unique scoped approval.' }
    $approval = $approvalLines[0].Substring('my.hello='.Length).Split('|')
    if ($approval.Count -ne 8 -or $approval[0] -cne '1' -or $approval[1] -cne $declaration[2] -or
        $approval[2] -cne $declaration[4] -or $approval[3] -cne $capabilities -or
        $approval[4] -cne $identity[3] -or $approval[5] -cne $identity[4] -or $approval[6] -cne $bundle[1] -or
        $approval[7] -cnotmatch '^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$') {
        throw 'The scoped approval differs from the clean package review.'
    }
    if (Test-Path -LiteralPath (Join-Path $serverDir 'user\plugins\native-approvals.ini')) {
        throw 'The clean test used legacy DLL approval instead of the reviewed install path.'
    }
    Push-Location $serverDir
    try {
        & $server --plugin-enable my.hello
        if ($LASTEXITCODE -ne 0) { throw 'The server could not enable the plugin.' }
        & $server --plugins-check
        if ($LASTEXITCODE -ne 0) { throw 'The scoped approval did not start the installed plugin.' }
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
