# Builds the SDK package, unpacks it in a clean folder, and uses only what it holds: it compiles every public header,
# builds the template with build.bat and with CMake, packs the result, checks it and installs it on a test server.
[CmdletBinding()]
param([string]$BuildDir)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BuildDir) { $BuildDir = Join-Path $repo 'build\server' }
$build = [IO.Path]::GetFullPath($BuildDir)
$server = Join-Path $build 'subvulpes_server.exe'
if (-not (Test-Path -LiteralPath $server -PathType Leaf)) { throw 'Build the server first.' }
$work = Join-Path $build ('tests\sdk-cases\' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work -Force | Out-Null
$checks = 0
function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    $script:checks++
}
function Run-Batch([string]$Name, [string[]]$Lines) {
    $path = Join-Path $work "$Name.bat"
    [IO.File]::WriteAllLines($path, [string[]](@('@echo off', 'setlocal', 'set VSLANG=1033',
        'where cl >nul 2>nul && goto have_cl',
        'if not defined VCVARS set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat',
        'call "%VCVARS%" >nul 2>nul || exit /b 91', ':have_cl') + $Lines), [Text.Encoding]::ASCII)
    $output = & cmd /c $path 2>&1
    return [pscustomobject]@{ Code = $LASTEXITCODE; Output = ($output -join "`n") }
}

# Make the package.
$line = (& (Join-Path $repo 'tools\sdk\make_sdk.ps1') -Version 'v0.0.0-sdk-test' -OutputDir $work) | Select-Object -Last 1
Assert ($line -cmatch "^SDK`t(.+)`t[0-9a-f]{64}$") 'make_sdk.ps1 gave no SDK line.'
$zipPath = $Matches[1]
Assert ((Split-Path $zipPath -Leaf) -ceq 'SubVulpes-sdk-v0.0.0-sdk-test.zip') 'The package name is wrong.'
$again = (& (Join-Path $repo 'tools\sdk\make_sdk.ps1') -Version 'v0.0.0-sdk-test' -OutputDir (Join-Path $work 'again')) | Select-Object -Last 1
Assert (($line -split "`t")[2] -ceq ($again -split "`t")[2]) 'The SDK package changed between two runs.'

# The content: public headers, template, tools, docs, an example, and no internal header.
$archive = [IO.Compression.ZipFile]::OpenRead($zipPath)
try { $names = @($archive.Entries | ForEach-Object FullName) } finally { $archive.Dispose() }
$prefix = 'SubVulpes-sdk-v0.0.0-sdk-test/'
Assert ($names.Count -gt 0 -and @($names | Where-Object { -not $_.StartsWith($prefix) }).Count -eq 0) 'An entry is outside the package folder.'
$relative = @($names | ForEach-Object { $_.Substring($prefix.Length) })
Assert (@($relative | Where-Object { $_ -clike 'include/sv_*.h' }).Count -eq 15) 'The SDK must carry 15 public headers.'
Assert (@($relative | Where-Object { $_ -clike 'include/*' -and $_ -cnotlike 'include/sv_*.h' }).Count -eq 0) 'The include folder holds a file that is not a public header.'
foreach ($need in @('README.txt', 'VERSION.txt', 'template/hello.c', 'template/plugin.ini', 'template/build.bat', 'template/CMakeLists.txt',
        'tools/pack.ps1', 'tools/validate_plugin.ps1', 'tools/install_plugin.ps1', 'tools/sv_plugin.cmake', 'docs/plugin-abi.md',
        'docs/plugin-manifest.md', 'examples/status/status.c')) {
    Assert ($need -cin $relative) "The package lacks $need."
}
foreach ($forbid in @('plugin_host.h', 'plugin_host.c', 'plugin_internal.h', 'config.txt')) {
    Assert (@($relative | Where-Object { $_ -clike "*$forbid" }).Count -eq 0) "The package holds $forbid."
}

# Unpack in a clean folder. From here, only the package is used.
$unpacked = Join-Path $work 'unpacked'
[IO.Compression.ZipFile]::ExtractToDirectory($zipPath, $unpacked)
$sdk = Join-Path $unpacked 'SubVulpes-sdk-v0.0.0-sdk-test'

# Every public header compiles alone, and all together.
$include = Join-Path $sdk 'include'
$headerLines = @(Get-ChildItem -LiteralPath $include -Filter 'sv_*.h' | Sort-Object Name | ForEach-Object { "#include `"$($_.Name)`"" })
[IO.File]::WriteAllLines((Join-Path $work 'headers.c'), [string[]]($headerLines + 'int main(void) { return 0; }'), [Text.Encoding]::ASCII)
$result = Run-Batch 'headers' @("cl /nologo /W4 /WX /utf-8 /I `"$include`" `"$work\headers.c`" /Fo:`"$work\headers.obj`" /Fe:`"$work\headers.exe`"")
Assert ($result.Code -eq 0) "The public headers do not compile together:`n$($result.Output)"
foreach ($header in (Get-ChildItem -LiteralPath $include -Filter 'sv_*.h')) {
    [IO.File]::WriteAllText((Join-Path $work 'one.c'), "#include `"$($header.Name)`"`nint main(void) { return 0; }`n")
    $result = Run-Batch 'one' @("cl /nologo /W4 /WX /utf-8 /I `"$include`" `"$work\one.c`" /Fo:`"$work\one.obj`" /Fe:`"$work\one.exe`"")
    Assert ($result.Code -eq 0) "$($header.Name) does not compile alone:`n$($result.Output)"
}

# Build the template with build.bat.
$template = Join-Path $sdk 'template'
$result = Run-Batch 'template' @("call `"$template\build.bat`"")
Assert ($result.Code -eq 0) "build.bat failed:`n$($result.Output)"
$out = Join-Path $template 'out'
Assert ((Test-Path (Join-Path $out 'hello.dll')) -and (Test-Path (Join-Path $out 'plugin.ini'))) 'build.bat did not make hello.dll and plugin.ini.'
Assert (@(Get-ChildItem -LiteralPath $out -Force).Count -eq 2) 'The output folder must hold only the DLL and the manifest.'

# Pack, check and install with the tools of the package.
$dist = Join-Path $work 'dist'
$line = (& (Join-Path $sdk 'tools\pack.ps1') -Folder $out -OutputDir $dist -ServerExe $server) | Select-Object -Last 1
Assert ($line -cmatch "^PACKAGE`t(.+)`t[0-9a-f]{64}$") 'pack.ps1 gave no PACKAGE line.'
$package = $Matches[1]
Assert ((Split-Path $package -Leaf) -ceq 'my.hello-0.1.0-native.svplugin') 'The package name is wrong.'
$report = @(& (Join-Path $sdk 'tools\validate_plugin.ps1') -Path $package -ServerExe $server)
Assert ($LASTEXITCODE -eq 0 -and $report[-1] -clike "MANIFEST`tmy.hello`t0.1.0`thello.dll`tnative`tread`tHello`t*") "validate_plugin.ps1 refused the package:`n$($report -join "`n")"
$target = Join-Path $work 'server'
New-Item -ItemType Directory -Path $target | Out-Null
Copy-Item -LiteralPath $server -Destination (Join-Path $target 'subvulpes_server.exe')
& (Join-Path $sdk 'tools\install_plugin.ps1') -ServerDir $target -Package $package | Out-Null
Assert (Test-Path (Join-Path $target 'plugins\my.hello\hello.dll')) 'The installer did not install the package.'

# Build the template with CMake. The package target makes the same file.
$cmakeOut = Join-Path $work 'cmake-out'
$result = Run-Batch 'cmake' @(
    "cmake -S `"$template`" -B `"$cmakeOut`" -G `"NMake Makefiles`" -DCMAKE_BUILD_TYPE=Release `"-DSV_SERVER_EXE=$server`" || exit /b 1",
    "cmake --build `"$cmakeOut`" || exit /b 1",
    "cmake --build `"$cmakeOut`" --target my-hello-package || exit /b 1")
Assert ($result.Code -eq 0) "The CMake build of the template failed:`n$($result.Output)"
Assert (Test-Path (Join-Path $cmakeOut 'packages\my.hello-0.1.0-native.svplugin')) 'The CMake package target made no package.'

Write-Output "SDK_PACKAGE_PASS $checks"
