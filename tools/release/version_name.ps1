# Gives the title, the name, and the picture of a release from its version tag.
# The name belongs to the minor version, so a fix release keeps it. The table is tools/release/version-names.csv.
# A tag never holds the name. Only the title and the notes show it.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$Table = (Join-Path $PSScriptRoot 'version-names.csv'),
    [string]$Repository = 'TheGreatVulpzin/SubVulpes'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Version -cnotmatch '^v(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*))?$') {
    throw 'Use a version tag such as v0.2.0 or v1.0.0-rc.1.'
}
$major, $minor, $patch, $suffix = $Matches[1], $Matches[2], $Matches[3], $(if ($Matches[4]) { $Matches[4] } else { '' })

$rows = @{}
foreach ($line in Get-Content -LiteralPath $Table) {
    if ($line.Trim().Length -eq 0 -or $line.StartsWith('#')) { continue }
    $f = $line.Split(',', 4)
    if ($f.Count -ne 4) { throw "The version table has a bad line: $line" }
    $rows[$f[0]] = $f
}
$row = if ($rows.ContainsKey("$major.$minor")) { $rows["$major.$minor"] } elseif ($rows.ContainsKey($major)) { $rows[$major] } else { $null }
$name = if ($row) { $row[1] } else { '' }

# A new minor version shows as 0.2. A fix or a candidate shows its full number.
$number = if ($patch -eq '0' -and $suffix -eq '') { "$major.$minor" } else { $Version.Substring(1) }
$title = ('SubVulpes ' + $number + $(if ($name) { " $name" } else { '' }))

$picture = ''
$caption = ''
if ($row -and $row[2] -and $patch -eq '0' -and $suffix -eq '') {
    $picture = "https://raw.githubusercontent.com/$Repository/$Version/$($row[2])"
    $caption = $row[3]
}
[pscustomobject]@{ Title = $title; Name = $name; Picture = $picture; Caption = $caption }
