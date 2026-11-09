# Read the pinned source and the default states of the embedded plugins.
function Read-EmbeddedPlugins {
    $commit = $null
    $plugins = [ordered]@{}
    foreach ($line in Get-Content -LiteralPath (Join-Path $PSScriptRoot 'embedded-plugins.txt')) {
        if ($line -cmatch '^\s*(#.*)?$') { continue }
        if ($line -cmatch '^commit=([0-9a-f]{40})$' -and $null -eq $commit) {
            $commit = $Matches[1]
        } elseif ($line -cmatch '^([a-z0-9][a-z0-9_.-]{0,46})=(enabled|disabled|required)$') {
            $id = $Matches[1]
            if ($id -eq 'commit' -or $id.Contains('..') -or $plugins.Contains($id)) {
                throw 'embedded-plugins.txt repeats a plugin or holds a bad identifier.'
            }
            $plugins[$id] = $Matches[2]
        } else {
            throw 'embedded-plugins.txt holds an invalid line.'
        }
    }
    if (-not $commit -or $plugins.Count -eq 0) { throw 'embedded-plugins.txt needs a full commit and at least one plugin.' }
    [pscustomobject]@{ Commit = $commit; Plugins = $plugins }
}
