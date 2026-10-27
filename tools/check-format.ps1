param(
    [string]$ClangFormat,
    [string]$StyLua,
    [switch]$Fix
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = Split-Path $PSScriptRoot -Parent

function Resolve-Formatter([string]$ExplicitPath, [string]$Name, [string]$Fallback) {
    if ($ExplicitPath) { return (Get-Item -LiteralPath $ExplicitPath).FullName }
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    if ($Fallback -and (Test-Path -LiteralPath $Fallback)) { return $Fallback }
    throw "Install $Name or pass its path. See docs/coding-style.md."
}

Push-Location $repositoryRoot
try {
    $vsFormatter = Join-Path ${env:ProgramFiles} 'Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin/clang-format.exe'
    $ClangFormat = Resolve-Formatter $ClangFormat 'clang-format' $vsFormatter
    $StyLua = Resolve-Formatter $StyLua 'stylua' (Join-Path $repositoryRoot 'build/tools/stylua-2.6.0/stylua.exe')
    $clangVersion = & $ClangFormat --version
    if ($LASTEXITCODE -ne 0 -or "$clangVersion" -notmatch '\b19\.1\.5\b') {
        throw 'Use clang-format 19.1.5 for reproducible formatting.'
    }
    $luaVersion = & $StyLua --version
    if ($LASTEXITCODE -ne 0 -or "$luaVersion" -notmatch '\b2\.6\.0\b') {
        throw 'Use StyLua 2.6.0 for reproducible formatting.'
    }
    $cppFiles = @(Get-ChildItem -LiteralPath src, tests, benchmarks -Recurse -File |
        Where-Object { $_.Extension -in '.cpp', '.h' } |
        Sort-Object FullName | ForEach-Object { $_.FullName })
    if ($Fix) {
        & $ClangFormat -i @cppFiles
        if ($LASTEXITCODE -ne 0) { throw 'C++ formatting failed' }
        & $StyLua --verify sdk cases templates tests/lua
        if ($LASTEXITCODE -ne 0) { throw 'Lua formatting or AST verification failed' }
    }
    & $ClangFormat --dry-run --Werror @cppFiles
    if ($LASTEXITCODE -ne 0) { throw 'C++ format check failed' }
    & $StyLua --check sdk cases templates tests/lua
    if ($LASTEXITCODE -ne 0) { throw 'Lua format check failed' }
    Write-Output "Format checks passed ($($cppFiles.Count) C++ files and project Lua directories)."
} finally {
    Pop-Location
}
