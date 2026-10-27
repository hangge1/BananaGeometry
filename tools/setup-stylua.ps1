# Development tool only; its binary and archive stay in ignored build/.
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path $PSScriptRoot -Parent
$toolDirectory = Join-Path $repositoryRoot 'build/tools'
$archive = Join-Path $toolDirectory 'stylua-2.6.0.zip'
$destination = Join-Path $toolDirectory 'stylua-2.6.0'
$expectedHash = 'FD13B4014E3B779C244BBC03F2C282E357F15BC526DE6A6FA5ECBC447538313C'
New-Item -ItemType Directory -Path $toolDirectory -Force | Out-Null
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/JohnnyMorganz/StyLua/releases/download/v2.6.0/stylua-windows-x86_64.zip' -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) {
    throw "StyLua archive hash mismatch. Remove $archive and retry."
}
Expand-Archive -LiteralPath $archive -DestinationPath $destination -Force
& (Join-Path $destination 'stylua.exe') --version
if ($LASTEXITCODE -ne 0) { throw 'StyLua installation check failed' }
