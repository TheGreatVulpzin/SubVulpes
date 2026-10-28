# Makes SubVulpes-sdk-VERSION.zip: the public headers, a template plugin, the tools, the pages of the API and an example.
# Usage: make_sdk.ps1 -Version v0.1.0 -OutputDir DIR -Runner subvulpes_plugin_test.exe -ProbeDir DIR
param([Parameter(Mandatory=$true)][string]$Version,
      [Parameter(Mandatory=$true)][string]$OutputDir,
      [Parameter(Mandatory=$true)][string]$Runner,
      [Parameter(Mandatory=$true)][string]$ProbeDir)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

if ($Version -cnotmatch '^v?[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$') { throw 'Use a version such as v0.1.0 or v0.1.0-rc1.' }
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$root = "SubVulpes-sdk-$Version"

# The public headers are the files sv_*.h. A new public header changes this count, and the change must be on purpose.
$headers = @(Get-ChildItem -LiteralPath (Join-Path $repo 'src\plugins') -Filter 'sv_*.h' -File | Sort-Object Name)
if ($headers.Count -ne 15) { throw "The SDK expects 15 public headers and found $($headers.Count)." }

$files = [ordered]@{}
foreach ($header in $headers) { $files["include/$($header.Name)"] = $header.FullName }
foreach ($name in @('hello.c', 'plugin.ini', 'build.bat', 'CMakeLists.txt')) {
    $files["template/$name"] = Join-Path $repo "tools\sdk\template\$name"
}
foreach ($name in @('pack.ps1', 'sv_plugin.cmake', 'run_plugin_test.ps1')) { $files["tools/$name"] = Join-Path $repo "tools\sdk\$name" }
# The test runner and the probe plugin come from the build. The scenario of the template sits in the template folder.
$files['tools/subvulpes_plugin_test.exe'] = (Resolve-Path -LiteralPath $Runner).Path
$files['tools/probe/probe.dll'] = Join-Path (Resolve-Path -LiteralPath $ProbeDir).Path 'probe.dll'
$files['tools/probe/plugin.ini'] = Join-Path $repo 'tests\runner\probe\plugin.ini'
$files['template/test.svtest'] = Join-Path $repo 'tools\sdk\template\test.svtest'
foreach ($name in @('validate_plugin.ps1', 'install_plugin.ps1')) { $files["tools/$name"] = Join-Path $repo "tools\plugins\$name" }
foreach ($name in @('plugin-abi.md', 'plugin-sdk.md', 'plugin-manifest.md', 'sdk-packaging.md', 'plugin-game-api.md',
        'plugin-events.md', 'plugin-config.md', 'plugin-libraries.md', 'plugin-ai.md', 'plugin-economy.md', 'plugin-engine.md',
        'plugin-mission.md', 'plugin-settings.md', 'plugin-shop.md', 'plugin-storage.md', 'sdk-test-runner.md')) {
    $files["docs/$name"] = Join-Path $repo "docs\$name"
}
$files['examples/status/status.c'] = Join-Path $repo 'plugins\status\status.c'
$files['examples/status/plugin.ini'] = Join-Path $repo 'plugins\status\plugin.ini'
$files['README.txt'] = Join-Path $repo 'tools\sdk\SDK-README.txt'
foreach ($path in $files.Values) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "An SDK file is missing: $path" }
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$target = Join-Path (Resolve-Path -LiteralPath $OutputDir).Path "$root.zip"
$stamp = [DateTimeOffset]::new(2000, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
$stream = [IO.File]::Open($target, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
try {
    $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        $entries = @{}
        foreach ($entry in $files.GetEnumerator()) { $entries["$root/$($entry.Key)"] = $entry.Value }
        $entries["$root/VERSION.txt"] = $null
        $names = [string[]]@($entries.Keys)
        [Array]::Sort($names, [StringComparer]::Ordinal)
        foreach ($name in $names) {
            $item = $zip.CreateEntry($name, [IO.Compression.CompressionLevel]::Optimal)
            $item.LastWriteTime = $stamp
            $output = $item.Open()
            try {
                if ($null -eq $entries[$name]) { $bytes = [Text.Encoding]::ASCII.GetBytes("$Version`n") }
                else { $bytes = [IO.File]::ReadAllBytes($entries[$name]) }
                $output.Write($bytes, 0, $bytes.Length)
            } finally { $output.Dispose() }
        }
    } finally { $zip.Dispose() }
} finally { $stream.Dispose() }
Write-Output "SDK`t$target`t$((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant())"
