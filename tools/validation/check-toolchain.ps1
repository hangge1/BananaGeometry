param([string]$VsWhere, [string]$RunId)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force
$context = New-ValidationContext -Task 'toolchain' -RunId $RunId
$status = 'failed'
try {
    $cmake = Invoke-ValidationCommand $context 'cmake-version' cmake @('--version')
    $version = [regex]::Match($cmake.Output, 'cmake version (\d+\.\d+\.\d+)')
    if (-not $version.Success -or [version]$version.Groups[1].Value -lt [version]'3.24.0') {
        throw 'Drawer requires CMake 3.24 or newer.'
    }
    if (-not $VsWhere) {
        $VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    }
    if (-not (Test-Path -LiteralPath $VsWhere -PathType Leaf)) {
        throw 'Visual Studio discovery tool is missing. Install VS 2022 Desktop development with C++ (or Build Tools). CI must use windows-2022.'
    }
    $query = Invoke-ValidationCommand $context 'visual-studio-instances' $VsWhere @('-products', '*', '-version', '[17.0,18.0)', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-format', 'json', '-utf8')
    $decoded = $query.Output | ConvertFrom-Json
    $instances = @($decoded | Where-Object { $_.isComplete -and $_.installationPath })
    Write-ValidationJson ([ordered]@{
        generator = 'Visual Studio 17 2022'; cmake_version = $version.Groups[1].Value;
        visual_studio = @($instances | ForEach-Object { [ordered]@{
            name = $_.displayName; version = $_.installationVersion; path = $_.installationPath
        } })
    }) (Join-Path $context.ArtifactDirectory 'toolchain.json')
    if (-not $instances.Count) {
        throw 'VS 2022 C++ tools were not found. The required generator is Visual Studio 17 2022. Use windows-2022 in GitHub Actions; windows-latest may contain only VS 2026.'
    }
    Write-Output "Toolchain ready: CMake $($version.Groups[1].Value); $($instances[0].displayName) $($instances[0].installationVersion)."
    $status = 'passed'
} finally { Complete-ValidationContext $context $status }
