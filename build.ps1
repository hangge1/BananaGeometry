param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [switch]$Package,
    [switch]$Benchmarks,
    [string]$BuildDirectory = 'build/windows',
    [string]$RunId
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'tools/validation/common.psm1') -Force
$context = New-ValidationContext -Task "build-$($Configuration.ToLowerInvariant())" -RunId $RunId
$status = 'failed'
$previousRun = $env:DRAWER_VALIDATION_RUN_ID
Push-Location $PSScriptRoot
try {
    if ($Benchmarks -and $Configuration -ne 'Release') { throw 'Use -Configuration Release for performance measurements' }
    $env:DRAWER_VALIDATION_RUN_ID = $context.RunId
    $benchmarkOption = if ($Benchmarks) { 'ON' } else { 'OFF' }
    Invoke-ValidationCommand $context 'toolchain' powershell @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', 'tools/validation/check-toolchain.ps1', '-RunId', $context.RunId) | Out-Null
    Invoke-ValidationCommand $context 'modules' powershell @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', 'tools/check-modules.ps1') | Out-Null
    Invoke-ValidationCommand $context 'configure' cmake @('-S', '.', '-B', $BuildDirectory, '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DBUILD_TESTING=ON', "-DDRAWER_BUILD_BENCHMARKS=$benchmarkOption") | Out-Null
    Invoke-ValidationCommand $context 'build' cmake @('--build', $BuildDirectory, '--config', $Configuration, '--parallel') | Out-Null
    $report = Join-Path $context.ArtifactDirectory "junit-$($Configuration.ToLowerInvariant()).xml"
    Invoke-ValidationCommand $context 'ctest' ctest @('--test-dir', $BuildDirectory, '-C', $Configuration, '-j', '4', '--output-on-failure', '--output-junit', $report) | Out-Null
    if ($Benchmarks) {
        Invoke-ValidationCommand $context 'measure' powershell @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', 'tools/run-benchmarks.ps1', '-Executable', "$BuildDirectory/bin/Release/drawer_benchmarks.exe", '-RunId', $context.RunId) | Out-Null
    }
    if ($Package) {
        Invoke-ValidationCommand $context 'install' cmake @('--install', $BuildDirectory, '--config', $Configuration, '--prefix', 'out/Drawer') | Out-Null
    }
    $status = 'passed'
} finally {
    $env:DRAWER_VALIDATION_RUN_ID = $previousRun
    Pop-Location
    Complete-ValidationContext $context $status
}
