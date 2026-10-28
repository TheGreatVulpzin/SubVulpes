# Runs a scenario (.svtest) against a plugin. The plugin is a folder or a .svplugin or .zip file.
# The script checks the manifest with the server rules, builds a clean test root with the plugin and the probe plugin,
# approves both DLLs in that root only, and runs the test runner there. It removes the root at the end.
# Usage: run_plugin_test.ps1 -Plugin PATH -Scenario FILE -ServerExe subvulpes_server.exe [-Runner EXE] [-Probe DIR] [-Keep]
# In the SDK package the runner and the probe sit next to this script, and the defaults find them.
param([Parameter(Mandatory=$true)][string]$Plugin,
      [Parameter(Mandatory=$true)][string]$Scenario,
      [Parameter(Mandatory=$true)][string]$ServerExe,
      [string]$Runner = (Join-Path $PSScriptRoot 'subvulpes_plugin_test.exe'),
      [string]$Probe = (Join-Path $PSScriptRoot 'probe'),
      [switch]$Keep)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem

$validate = Join-Path $PSScriptRoot 'validate_plugin.ps1'
if (-not (Test-Path -LiteralPath $validate -PathType Leaf)) { $validate = Join-Path $PSScriptRoot '..\plugins\validate_plugin.ps1' }
$runnerPath = (Resolve-Path -LiteralPath $Runner).Path
$scenarioPath = (Resolve-Path -LiteralPath $Scenario).Path
$probePath = (Resolve-Path -LiteralPath $Probe).Path
$pluginItem = Get-Item -LiteralPath $Plugin

# Check the plugin first. The validate script prints every problem.
$report = @(& $validate -Path $pluginItem.FullName -ServerExe $ServerExe)
if ($LASTEXITCODE -ne 0) { $report | ForEach-Object { Write-Output $_ }; throw 'The plugin is not valid.' }
$fields = ($report | Select-Object -Last 1) -split "`t"
$id = $fields[1]
$library = $fields[3]
if ($fields[4] -cne 'native') { throw 'This runner runs native plugins only until the script adapters exist.' }

$root = Join-Path ([IO.Path]::GetTempPath()) ('svtest-' + [Guid]::NewGuid().ToString('N'))
try {
    foreach ($dir in @('plugins', 'user\plugins')) { New-Item -ItemType Directory -Path (Join-Path $root $dir) -Force | Out-Null }
    $target = Join-Path $root "plugins\$id"
    New-Item -ItemType Directory -Path $target | Out-Null
    if ($pluginItem.PSIsContainer) {
        Copy-Item -LiteralPath (Join-Path $pluginItem.FullName 'plugin.ini') -Destination $target
        Copy-Item -LiteralPath (Join-Path $pluginItem.FullName $library) -Destination $target
    } else {
        [IO.Compression.ZipFile]::ExtractToDirectory($pluginItem.FullName, $target)
    }
    $probeTarget = Join-Path $root 'plugins\sv.probe'
    New-Item -ItemType Directory -Path $probeTarget | Out-Null
    Copy-Item -LiteralPath (Join-Path $probePath 'plugin.ini') -Destination $probeTarget
    Copy-Item -LiteralPath (Join-Path $probePath 'probe.dll') -Destination $probeTarget
    [IO.File]::WriteAllLines((Join-Path $root 'plugins\plugins.ini'), [string[]]@('[plugins]', "$id=enabled", 'sv.probe=enabled'), [Text.Encoding]::ASCII)
    # The test root approves the two DLLs. This approval counts in this temporary folder only.
    $lines = @('[native]')
    foreach ($pair in @(@($id, (Join-Path $target $library)), @('sv.probe', (Join-Path $probeTarget 'probe.dll')))) {
        $lines += "$($pair[0])=$((Get-FileHash -LiteralPath $pair[1] -Algorithm SHA256).Hash.ToLowerInvariant())"
    }
    [IO.File]::WriteAllLines((Join-Path $root 'plugins\native-approvals.ini'), [string[]]$lines, [Text.Encoding]::ASCII)
    Push-Location $root
    try {
        & $runnerPath $scenarioPath
        $code = $LASTEXITCODE
    } finally { Pop-Location }
    exit $code
} finally {
    if (-not $Keep -and (Test-Path -LiteralPath $root)) { Remove-Item -LiteralPath $root -Recurse -Force }
}
