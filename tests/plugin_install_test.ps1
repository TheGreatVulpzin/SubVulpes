[CmdletBinding()]
param([string]$BuildDir)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (-not $BuildDir) { $BuildDir = Join-Path $PSScriptRoot '../build/server' }
$buildRoot = [IO.Path]::GetFullPath($BuildDir)
$caseRoot = Join-Path $buildRoot ('tests/install-cases/' + [Guid]::NewGuid().ToString('N'))
$installer = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../tools/plugins/install_plugin.ps1'))
$currentDll = Join-Path $buildRoot 'plugins/sv.status/status.dll'
$updateDll = Join-Path $buildRoot 'tests/status-update.dll'
foreach ($file in @($currentDll, $updateDll)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw 'Build both status DLL versions before this test.' }
}
[IO.Directory]::CreateDirectory($caseRoot) | Out-Null
$currentBytes = [IO.File]::ReadAllBytes($currentDll)
$updateBytes = [IO.File]::ReadAllBytes($updateDll)
if ((Get-FileHash -LiteralPath $currentDll).Hash -eq (Get-FileHash -LiteralPath $updateDll).Hash) {
    throw 'The update fixture must differ from the current DLL.'
}
$script:checks = 0
function Assert-Test([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    $script:checks++
}
function Manifest([string]$Id='sv.status', [string]$Version='0.1.0', [string]$Api='1',
                  [string]$Dependencies='', [string]$Library='status.dll') {
    return [Text.Encoding]::ASCII.GetBytes("[plugin]`nid=$Id`nversion=$Version`nlibrary=$Library`napi=$Api`nlive=1`ndependencies=$Dependencies`n")
}
function Entry([string]$Name, [byte[]]$Bytes) { return [pscustomobject]@{ Name=$Name; Bytes=$Bytes } }
function Package([string]$Name, [object[]]$Entries) {
    $path = Join-Path $caseRoot ($Name + '.zip')
    $stream = [IO.File]::Open($path, [IO.FileMode]::CreateNew)
    $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($item in $Entries) {
            $entry = $zip.CreateEntry($item.Name)
            $output = $entry.Open()
            try { $output.Write($item.Bytes, 0, $item.Bytes.Length) } finally { $output.Dispose() }
        }
    } finally { $zip.Dispose(); $stream.Dispose() }
    return $path
}
function Install([string]$Folder, [string]$Zip, [bool]$Success=$true) {
    $failed = $false
    try { & $installer -ServerDir $Folder -Package $Zip | Out-Null } catch { $failed = $true }
    Assert-Test ($failed -ne $Success) 'The installer returned an unexpected result.'
}
function Hash([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }

$server = Join-Path $caseRoot 'server'
foreach ($dir in @('plugins', 'user/plugins', 'user/custom')) {
    [IO.Directory]::CreateDirectory((Join-Path $server $dir)) | Out-Null
}
$core = Join-Path $server 'subvulpes_server.exe'
$config = Join-Path $server 'config.txt'
$sentinel = Join-Path $server 'user/custom/sentinel.bin'
$profile = Join-Path $server 'plugins/plugins.ini'
$state = Join-Path $server 'user/plugins/state.ini'
$approval = Join-Path $server 'user/plugins/native-approvals.ini'
[IO.File]::WriteAllText($core, 'unchanged core fixture')
[IO.File]::WriteAllText($config, 'unchanged private configuration')
[IO.File]::WriteAllText($sentinel, 'unchanged module data')
[IO.File]::WriteAllText($state, "[plugins]`nthird.party=disabled`n")
[IO.File]::WriteAllText($profile, "[plugins]`nthird.party=enabled`nrequired.service=required`n")
[IO.File]::WriteAllText($approval, "[native]`nthird.party=previous-approval`n")
$preserved = @{}
foreach ($path in @($core, $config, $sentinel, $state, $approval)) { $preserved[$path] = Hash $path }
$initial = Package 'initial' @((Entry 'plugin.ini' (Manifest)), (Entry 'status.dll' $currentBytes))
$inspectionFolder = Join-Path $caseRoot 'inspection-only'
[IO.Directory]::CreateDirectory($inspectionFolder) | Out-Null
$inspection = & $installer -ServerDir $inspectionFolder -Package $initial -Inspect
Assert-Test ($inspection -match '^MODULE\tsv.status\t0.1.0\t[0-9a-f]{64}\t[0-9a-f]{64}$') 'Inspection did not identify the package.'
Assert-Test (-not (Test-Path -LiteralPath (Join-Path $inspectionFolder 'user'))) 'Inspection changed private data.'
Assert-Test (-not (Test-Path -LiteralPath (Join-Path $inspectionFolder 'plugins'))) 'Inspection changed public modules.'
$rejectedChange = $false
try { & $installer -ServerDir $inspectionFolder -Package $initial -ExpectedPackageHash ('0' * 64) | Out-Null }
catch { $rejectedChange = $true }
Assert-Test $rejectedChange 'A package different from the inspected content was accepted.'
Assert-Test (-not (Test-Path -LiteralPath (Join-Path $inspectionFolder 'user'))) 'Changed content reached the installation transaction.'
$update = Package 'update' @((Entry 'plugin.ini' (Manifest -Version '0.1.1')), (Entry 'status.dll' $updateBytes))
Install $server $initial
$installedDll = Join-Path $server 'plugins/sv.status/status.dll'
$installedManifest = Join-Path $server 'plugins/sv.status/plugin.ini'
Assert-Test ((Hash $installedDll) -eq (Hash $currentDll)) 'The initial DLL differs from the package.'
$profileText = [IO.File]::ReadAllText($profile)
Assert-Test ($profileText -match 'sv.status=disabled') 'New modules must start disabled.'
Assert-Test ($profileText.Contains('third.party=enabled') -and $profileText.Contains('required.service=required')) 'Existing activation classes changed.'
Assert-Test (-not (Test-Path -LiteralPath (Join-Path $server 'plugins/native-approvals.ini'))) 'Installation created a public approval.'
Assert-Test (-not ([IO.File]::ReadAllText($approval).Contains('sv.status'))) 'Installation approved native code.'
[IO.File]::WriteAllText($profile, $profileText.Replace('sv.status=disabled', 'sv.status=enabled'))
$beforeUpdateProfile = Hash $profile
Install $server $update
Assert-Test ((Hash $installedDll) -eq (Hash $updateDll)) 'The update DLL differs from the package.'
Assert-Test ([IO.File]::ReadAllText($installedManifest).Contains('version=0.1.1')) 'The update manifest version is incorrect.'
Assert-Test ((Hash $profile) -eq $beforeUpdateProfile) 'An update changed the activation profile.'
$backups = @(Get-ChildItem -LiteralPath (Join-Path $server 'backup/plugins') -Filter '*.zip')
Assert-Test ($backups.Count -eq 1) 'The update must save one rollback package.'
Install $server $backups[0].FullName
Assert-Test ((Hash $installedDll) -eq (Hash $currentDll)) 'Rollback did not restore the original DLL.'
Assert-Test ([IO.File]::ReadAllText($installedManifest).Contains('version=0.1.0')) 'Rollback did not restore the original manifest.'
Assert-Test ((Hash $profile) -eq $beforeUpdateProfile) 'Rollback changed the activation profile.'
foreach ($path in $preserved.Keys) { Assert-Test ((Hash $path) -eq $preserved[$path]) 'Installation changed a private or core file.' }

$bad = @(
    (Package 'traversal' @((Entry '../plugin.ini' (Manifest)), (Entry 'status.dll' $currentBytes))),
    (Package 'absolute' @((Entry 'C:\plugin.ini' (Manifest)), (Entry 'status.dll' $currentBytes))),
    (Package 'subdir' @((Entry 'folder/plugin.ini' (Manifest)), (Entry 'status.dll' $currentBytes))),
    (Package 'duplicate' @((Entry 'plugin.ini' (Manifest)), (Entry 'plugin.ini' (Manifest)))),
    (Package 'case-duplicate' @((Entry 'plugin.ini' (Manifest)), (Entry 'PLUGIN.INI' (Manifest)))),
    (Package 'unexpected' @((Entry 'plugin.ini' (Manifest)), (Entry 'status.dll' $currentBytes), (Entry 'config.txt' ([byte[]]@(1))))),
    (Package 'api' @((Entry 'plugin.ini' (Manifest -Api '2')), (Entry 'status.dll' $currentBytes))),
    (Package 'id' @((Entry 'plugin.ini' (Manifest -Id '../escape')), (Entry 'status.dll' $currentBytes))),
    (Package 'library' @((Entry 'plugin.ini' (Manifest -Library '../status.dll')), (Entry 'status.dll' $currentBytes))),
    (Package 'nonpe' @((Entry 'plugin.ini' (Manifest)), (Entry 'status.dll' ([byte[]]@(1,2,3))))),
    (Package 'dependency' @((Entry 'plugin.ini' (Manifest -Dependencies 'missing')), (Entry 'status.dll' $currentBytes))),
    (Package 'manifest-size' @((Entry 'plugin.ini' (New-Object byte[] 65537)), (Entry 'status.dll' $currentBytes))),
    (Package 'dll-size' @((Entry 'plugin.ini' (Manifest)), (Entry 'status.dll' (New-Object byte[] 67108865))))
)
$stableProfile = Hash $profile
$stableDll = Hash $installedDll
foreach ($zip in $bad) { Install $server $zip $false }
Assert-Test ((Hash $profile) -eq $stableProfile -and (Hash $installedDll) -eq $stableDll) 'A rejected package changed public files.'
$lockPath = Join-Path $server 'user/plugins/host.lock'
$lock = [IO.File]::Open($lockPath, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::Read)
try { Install $server $update $false } finally { $lock.Dispose() }
Assert-Test ((Hash $installedDll) -eq $stableDll) 'The installer ignored the running-host lock.'

$partial = Package 'profile-write-failure' @((Entry 'plugin.ini' (Manifest -Id 'test.partial')), (Entry 'status.dll' $currentBytes))
$profileLock = [IO.File]::Open($profile, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
try { Install $server $partial $false } finally { $profileLock.Dispose() }
Assert-Test ((Hash $profile) -eq $stableProfile) 'A failed transaction changed the profile.'
Assert-Test (-not (Test-Path -LiteralPath (Join-Path $server 'plugins/test.partial/status.dll'))) 'A failed transaction left a DLL.'
Assert-Test (-not (Test-Path -LiteralPath (Join-Path $server 'plugins/test.partial/plugin.ini'))) 'A failed transaction left a manifest.'
$linkedServer = Join-Path $caseRoot 'linked-server'
New-Item -ItemType Junction -Path $linkedServer -Target $server | Out-Null
Install $linkedServer $update $false
Assert-Test ((Hash $installedDll) -eq $stableDll) 'The installer accepted a reparse ancestor.'

$depDir = Join-Path $server 'plugins/test.dependency'
[IO.Directory]::CreateDirectory($depDir) | Out-Null
[IO.File]::WriteAllBytes((Join-Path $depDir 'plugin.ini'), (Manifest -Id 'test.dependency'))
[IO.File]::WriteAllBytes((Join-Path $depDir 'status.dll'), $currentBytes)
[IO.File]::AppendAllText($profile, "`ntest.dependency=disabled`n")
$withDependency = Package 'valid-dependency' @((Entry 'plugin.ini' (Manifest -Dependencies 'test.dependency')), (Entry 'status.dll' $currentBytes))
Install $server $withDependency
[IO.File]::WriteAllBytes((Join-Path $depDir 'plugin.ini'), (Manifest -Id 'test.dependency' -Api '2'))
Install $server $withDependency $false
$indented = Join-Path $caseRoot 'indented-profile'
[IO.Directory]::CreateDirectory((Join-Path $indented 'plugins')) | Out-Null
$indentedProfile = Join-Path $indented 'plugins/plugins.ini'
[IO.File]::WriteAllText($indentedProfile, "  [plugins]`r`nthird.party=enabled`r`n")
Install $indented $initial
$indentedText = [IO.File]::ReadAllText($indentedProfile)
Assert-Test ([regex]::Matches($indentedText, '(?im)^[ \t]*\[plugins\]').Count -eq 1) 'Installation duplicated the indented section.'
Assert-Test ($indentedText -match '(?m)^sv.status=disabled\r?$') 'The indented profile does not list the new module.'
Assert-Test ($indentedText.Contains('third.party=enabled')) 'Installation changed an existing profile entry.'

$preflight = Join-Path $caseRoot 'preflight'
[IO.Directory]::CreateDirectory($preflight) | Out-Null
$deviceNames = @('con', 'prn', 'aux', 'nul') + @(1..9 | ForEach-Object { "com$_"; "lpt$_" })
$invalidId = 0
foreach ($name in ($deviceNames + @('con.suffix', 'nul.data', 'module.'))) {
    $invalidZip = Package ("invalid-id-" + $invalidId++) @((Entry 'plugin.ini' (Manifest -Id $name)), (Entry 'status.dll' $currentBytes))
    Install $preflight $invalidZip $false
}
foreach ($name in $deviceNames) {
    $library = "$name.dll"
    $invalidZip = Package ("invalid-library-" + $invalidId++) @((Entry 'plugin.ini' (Manifest -Library $library)), (Entry $library $currentBytes))
    Install $preflight $invalidZip $false
}
$invalidZip = Package 'device-dependency' @((Entry 'plugin.ini' (Manifest -Dependencies 'aux.data')), (Entry 'status.dll' $currentBytes))
Install $preflight $invalidZip $false
$invalidZip = Package 'long-id' @((Entry 'plugin.ini' (Manifest -Id ('a' * 48))), (Entry 'status.dll' $currentBytes))
Install $preflight $invalidZip $false
$invalidZip = Package 'long-version' @((Entry 'plugin.ini' (Manifest -Version ('1' * 24))), (Entry 'status.dll' $currentBytes))
Install $preflight $invalidZip $false
$longLibrary = ('a' * 44) + '.dll'
$invalidZip = Package 'long-library' @((Entry 'plugin.ini' (Manifest -Library $longLibrary)), (Entry $longLibrary $currentBytes))
Install $preflight $invalidZip $false
Assert-Test (@(Get-ChildItem -LiteralPath $preflight -Force).Count -eq 0) 'An invalid manifest changed the server folder.'
$fullId = 'a' * 47
$fullLibrary = ('b' * 43) + '.dll'
$fullVersion = '1' * 23
$fullZip = Package 'full-width' @((Entry 'plugin.ini' (Manifest -Id $fullId -Library $fullLibrary -Version $fullVersion)), (Entry $fullLibrary $currentBytes))
Install $preflight $fullZip
Assert-Test (Test-Path -LiteralPath (Join-Path $preflight "plugins/$fullId/$fullLibrary") -PathType Leaf) 'The installer rejected full-width ABI fields.'

$cleanupFault = [pscustomobject]@{ Injected=$false; Root=([IO.Path]::GetFullPath((Join-Path $server 'user/plugins')) + '\install-') }
${function:Remove-Item} = {
    [CmdletBinding()]
    param([string]$LiteralPath, [switch]$Recurse, [switch]$Force)
    if ($Recurse -and $LiteralPath.StartsWith($cleanupFault.Root, [StringComparison]::OrdinalIgnoreCase)) {
        $cleanupFault.Injected = $true
        throw 'The test injected a stage cleanup failure.'
    }
    Microsoft.PowerShell.Management\Remove-Item @PSBoundParameters
}.GetNewClosure()
try { Install $server $update $false }
finally { Microsoft.PowerShell.Management\Remove-Item -LiteralPath Function:\Remove-Item }
Assert-Test $cleanupFault.Injected 'The cleanup failure did not occur.'
$releasedLock = [IO.File]::Open($lockPath, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try { Assert-Test ($null -ne $releasedLock) 'A cleanup failure retained the host lock.' }
finally { $releasedLock.Dispose() }
Install $server $initial
Write-Output "$script:checks plugin installation checks passed."
