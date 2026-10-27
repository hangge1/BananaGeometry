param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$Benchmarks,
    [switch]$Screenshots,
    [switch]$Package,
    [switch]$SkipFormat,
    [switch]$SkipBuild,
    [string]$ClangFormat,
    [string]$StyLua,
    [string]$RunId
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'validation/common.psm1') -Force
$context = New-ValidationContext -Task 'validation' -RunId $RunId
$status = 'failed'
try {
    $common = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File')
    if (-not $SkipFormat) {
        $formatArguments = $common + @('tools/check-format.ps1')
        if ($ClangFormat) { $formatArguments += @('-ClangFormat', $ClangFormat) }
        if ($StyLua) { $formatArguments += @('-StyLua', $StyLua) }
        Invoke-ValidationCommand $context 'format' powershell $formatArguments | Out-Null
    }
    Invoke-ValidationCommand $context 'documents' powershell ($common + @('tools/validation/check-docs.ps1', '-RunId', $context.RunId)) | Out-Null
    Invoke-ValidationCommand $context 'guards' powershell ($common + @('tools/validation/check-guards.ps1', '-RunId', $context.RunId)) | Out-Null
    if (-not $SkipBuild) {
        $arguments = $common + @('build.ps1', '-Configuration', $Configuration, '-RunId', $context.RunId)
        if ($Benchmarks) { $arguments += '-Benchmarks' }
        if ($Package) { $arguments += '-Package' }
        Invoke-ValidationCommand $context 'build-and-test' powershell $arguments | Out-Null
    }
    if ($Screenshots) {
        Invoke-ValidationCommand $context 'native-fixtures' powershell ($common + @('tools/validation/check-native-fixtures.ps1', '-Executable', "build/windows/bin/$Configuration/drawer.exe", '-RunId', $context.RunId)) | Out-Null
        Invoke-ValidationCommand $context 'screenshots' powershell ($common + @('tools/validation/capture-screenshots.ps1', '-Executable', "build/windows/bin/$Configuration/drawer.exe", '-RunId', $context.RunId)) | Out-Null
    }
    if (-not $SkipBuild) {
        Invoke-ValidationCommand $context 'fixture-retention' powershell ($common + @('tools/validation/check-fixture-retention.ps1', '-Executable', "build/windows/bin/$Configuration/drawer_tests.exe", '-RunId', $context.RunId)) | Out-Null
    }
    $status = 'passed'
} finally { Complete-ValidationContext $context $status }
