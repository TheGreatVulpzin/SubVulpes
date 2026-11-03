# Builds the library that plugin.ini names, into this folder. The Workshop Action runs this script in each plugin folder.
# It compiles every .c file next to it. Set SUBVULPES_SDK to the SDK folder, the one that holds include\.
# Without SUBVULPES_SDK, it looks for include\ in the parent folders. Set VCVARS to a vcvars32.bat path to select MSVC.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$folder = $PSScriptRoot
$library = $null
foreach ($line in Get-Content -LiteralPath (Join-Path $folder 'plugin.ini')) {
    if ($line -cmatch '^library=(.+)$') { $library = $Matches[1].Trim() }
}
if (-not $library -or $library -cnotmatch '^[A-Za-z0-9._-]+\.dll$') { throw 'plugin.ini has no valid library= line.' }

$sdk = $env:SUBVULPES_SDK
if (-not $sdk) {
    for ($up = $folder; $up; $up = Split-Path $up -Parent) {
        if (Test-Path (Join-Path $up 'include\sv_plugin.h')) { $sdk = $up; break }
    }
}
if (-not $sdk -or -not (Test-Path (Join-Path $sdk 'include\sv_plugin.h'))) { throw 'Set SUBVULPES_SDK to the SDK folder that holds include\.' }

if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    $vcvars = $env:VCVARS
    if (-not $vcvars) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (-not (Test-Path $vswhere)) { throw 'Install MSVC Build Tools with the C++ x86 tools.' }
        $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $install) { throw 'vswhere found no MSVC installation with the C++ x86 tools.' }
        $vcvars = Join-Path $install 'VC\Auxiliary\Build\vcvars32.bat'
    }
    if (-not (Test-Path $vcvars)) { throw 'VCVARS must name an existing vcvars32.bat file.' }
    cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2]) }
    }
}
if ($env:VSCMD_ARG_TGT_ARCH -eq 'x64') { throw 'Use an x86 MSVC prompt or set VCVARS to vcvars32.bat.' }

$sources = @(Get-ChildItem -LiteralPath $folder -Filter '*.c' -File | ForEach-Object Name)
if ($sources.Count -eq 0) { throw 'The folder has no .c file.' }
$obj = Join-Path $env:TEMP ('svplugin-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $obj | Out-Null
try {
    Push-Location $folder
    & cl /nologo /LD /W4 /WX /utf-8 /D_CRT_SECURE_NO_WARNINGS /I (Join-Path $sdk 'include') @sources "/Fo$obj\" "/Fe:$library" /link /MACHINE:X86 "/IMPLIB:$obj\plugin.lib" /EXPORT:sv_plugin_entry
    if ($LASTEXITCODE -ne 0) { throw 'The compiler failed.' }
} finally {
    Pop-Location
    Remove-Item -LiteralPath $obj -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Output "OK: $library"
