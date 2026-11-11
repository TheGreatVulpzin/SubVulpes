# Fetches the Speex 1.2.1 source for the voice encoder of the server. The source goes to third_party/speex, which git ignores.
# Without it the server still builds, and voice_play answers SV_INCOMPATIBLE for a WAV file.
# Speex is free software (BSD license, Xiph.org Foundation). The license text goes with the source (third_party/speex/COPYING).
# Usage: tools\voice\fetch_speex.ps1
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$target = Join-Path $repo 'third_party\speex'
$url = 'https://github.com/xiph/speex/archive/refs/tags/Speex-1.2.1.tar.gz'
$sha256 = 'beaf2642e81a822eaade4d9ebf92e1678f301abfc74a29159c4e721ee70fdce0'

if (Test-Path -LiteralPath (Join-Path $target 'libspeex\speex.c')) {
    Write-Output "Speex is already in $target."
    return
}
$work = Join-Path ([IO.Path]::GetTempPath()) ('speex-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $archive = Join-Path $work 'speex.tar.gz'
    Invoke-WebRequest -Uri $url -OutFile $archive -UseBasicParsing
    $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hash -ne $sha256) { throw "The SHA-256 of the download is $hash. It must be $sha256." }
    & tar -xzf $archive -C $work
    if ($LASTEXITCODE -ne 0) { throw 'tar could not extract the archive.' }
    $source = Join-Path $work 'speex-Speex-1.2.1'
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $source 'libspeex') -Destination $target -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $source 'include') -Destination $target -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $source 'COPYING') -Destination $target -Force
    # The test programs and the psychoacoustic file are not part of the encoder.
    Get-ChildItem -LiteralPath (Join-Path $target 'libspeex') -Filter 'testenc*.c' | Remove-Item
    Remove-Item -LiteralPath (Join-Path $target 'libspeex\vorbis_psy.c') -ErrorAction SilentlyContinue
    Write-Output "Speex 1.2.1 is in $target."
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
