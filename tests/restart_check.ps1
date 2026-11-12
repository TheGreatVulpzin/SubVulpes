# Runs the real server with a plugin that asks for a restart, then with a plugin that asks for a stop, and checks the exit codes (75 and 0).
# The server needs the game data, so this check is not part of ctest. It uses a temporary folder and a port of its own.
# Usage: tests\restart_check.ps1 -DataFolder DIR [-ServerExe build\server\subvulpes_server.exe] [-Port 27999]
#   DIR is a server folder that holds data\ (for example the folder of a server package).
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$DataFolder,
      [string]$ServerExe = 'build\server\subvulpes_server.exe',
      [int]$Port = 27999)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$exe = [IO.Path]::GetFullPath((Join-Path $repo $ServerExe))
$data = [IO.Path]::GetFullPath($DataFolder)
if (-not (Test-Path -LiteralPath $exe)) { throw "No server at $exe." }
if (-not (Test-Path -LiteralPath (Join-Path $data 'data'))) { throw "No data folder in $data." }

$work = Join-Path $repo ('build\restart-check-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $work | Out-Null
try {
    # The fixture plugin.
    $bat = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat'
    $plugin = Join-Path $work 'plugins\test.restart'
    New-Item -ItemType Directory -Path $plugin -Force | Out-Null
    $build = "`"$bat`" >nul 2>nul && cl /nologo /LD /W3 /I `"$repo\src\plugins`" `"$repo\tests\restart_fixture.c`" /Fe:`"$plugin\restart.dll`" /Fo:`"$work\\`" >nul"
    cmd /c $build
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path (Join-Path $plugin 'restart.dll'))) { throw 'The fixture plugin did not build.' }
    [IO.File]::WriteAllText((Join-Path $plugin 'plugin.ini'),
        "[plugin]`nid=test.restart`nversion=0.1.0`nlibrary=restart.dll`napi=1`ncapabilities=server_control`nlive=1`n")
    $hash = (Get-FileHash -LiteralPath (Join-Path $plugin 'restart.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText((Join-Path $work 'plugins\plugins.ini'), "[plugins]`ntest.restart=enabled`n")
    [IO.File]::WriteAllText((Join-Path $work 'plugins\native-approvals.ini'), "[native]`ntest.restart=$hash`n")
    # The server folder: the data, a config of our own (no password), and the executable.
    & robocopy (Join-Path $data 'data') (Join-Path $work 'data') /E /NFL /NDL /NJH /NJS /NP | Out-Null
    [IO.File]::WriteAllText((Join-Path $work 'config.txt'), "servername=restart check`nport=$Port`ngametype=4`nmaxnumofplayers=8`nroundtime=7`nsetcpuaffinity=0`n")
    Copy-Item -LiteralPath $exe -Destination (Join-Path $work 'subvulpes_server.exe')

    $failed = 0
    foreach ($case in @(@{ request = 'restart'; code = 75 }, @{ request = 'stop'; code = 0 })) {
        $env:SV_TEST_EXIT_REQUEST = $case.request
        Remove-Item -LiteralPath (Join-Path $work 'user/restart-once.flag') -ErrorAction SilentlyContinue
        $process = Start-Process -FilePath (Join-Path $work 'subvulpes_server.exe') -WorkingDirectory $work -PassThru -WindowStyle Hidden
        if (-not $process.WaitForExit(60000)) { $process.Kill(); Write-Output "FAIL $($case.request): the server did not end in 60 seconds."; $failed++; continue }
        if ($process.ExitCode -eq $case.code) { Write-Output "PASS $($case.request): exit code $($process.ExitCode)." }
        else { Write-Output "FAIL $($case.request): exit code $($process.ExitCode), want $($case.code)."; $failed++ }
    }
    if ($failed) { exit 1 }
} finally {
    Remove-Item Env:SV_TEST_EXIT_REQUEST -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
