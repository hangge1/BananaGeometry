param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [ValidatePattern('^[a-z0-9]+(-[a-z0-9]+)*$')][string]$Step = 'staged',
    [string]$DependencyCache,
    [string]$ClangFormat,
    [string]$StyLua,
    [switch]$SkipBuild,
    [string]$RunId
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force
$context = New-ValidationContext -Task "index-$Step" -RunId $RunId
$status = 'failed'
try {
    $root = $context.RepositoryRoot
    $tree = (Invoke-ValidationCommand $context "$Step-tree" git @('write-tree')).Output.Trim()
    $temporaryRoot = (Resolve-Path -LiteralPath $context.TemporaryDirectory).ProviderPath
    $source = [System.IO.Path]::GetFullPath((Join-Path $temporaryRoot 'index-source'))
    if (Test-Path -LiteralPath $source) {
        # Preserve prior snapshots, including failed fixtures, without reusing stale files.
        $resolved = (Resolve-Path -LiteralPath $source).ProviderPath
        $backup = [System.IO.Path]::GetFullPath((Join-Path $temporaryRoot ("index-before-$Step-" + [guid]::NewGuid().ToString('N').Substring(0, 8))))
        if ($resolved -ne $source -or -not $resolved.StartsWith($temporaryRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
            -not $backup.StartsWith($temporaryRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
            ((Get-Item -LiteralPath $source).Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
            throw 'Index snapshot move escaped its validation directory'
        }
        Move-Item -LiteralPath $source -Destination $backup
    }
    $archive = Join-Path $temporaryRoot ("index-$Step-" + [guid]::NewGuid().ToString('N').Substring(0, 8) + '.zip')
    Invoke-ValidationCommand $context "$Step-export" git @('archive', '--format=zip', "--output=$archive", $tree) | Out-Null
    Expand-Archive -LiteralPath $archive -DestinationPath $source
    $common = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File')
    $format = $common + @((Join-Path $source 'tools/check-format.ps1'))
    if ($ClangFormat) { $format += @('-ClangFormat', $ClangFormat) }
    if (-not $StyLua) {
        $localStyLua = Join-Path $root 'build/tools/stylua-2.6.0/stylua.exe'
        if (Test-Path -LiteralPath $localStyLua) { $StyLua = $localStyLua }
    }
    if ($StyLua) { $format += @('-StyLua', $StyLua) }
    Invoke-ValidationCommand $context "$Step-format" powershell $format -WorkingDirectory $source | Out-Null
    Invoke-ValidationCommand $context "$Step-documents" powershell ($common + @((Join-Path $source 'tools/validation/check-docs.ps1'), '-RunId', $context.RunId)) -WorkingDirectory $source | Out-Null
    Invoke-ValidationCommand $context "$Step-guards" powershell ($common + @((Join-Path $source 'tools/validation/check-guards.ps1'), '-RunId', $context.RunId)) -WorkingDirectory $source | Out-Null
    $build = Join-Path $root "build/index-review/$($context.RunId)"
    if (-not $SkipBuild) {
        if ($DependencyCache) {
            if (-not [System.IO.Path]::IsPathRooted($DependencyCache)) { $DependencyCache = Join-Path $root $DependencyCache }
            $options = @('-S', $source, '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'x64')
            foreach ($line in Get-Content -LiteralPath (Join-Path $DependencyCache 'CMakeCache.txt')) {
                if ($line -match '^(FETCHCONTENT_SOURCE_DIR_[A-Z0-9_]+):PATH=(.+)$') {
                    $name, $path = $Matches[1], $Matches[2]
                    if (-not [System.IO.Path]::IsPathRooted($path) -or -not (Test-Path -LiteralPath $path -PathType Container)) {
                        throw "Invalid dependency cache path: $name"
                    }
                    $options += "-D${name}:PATH=$path"
                }
            }
            Invoke-ValidationCommand $context "$Step-cache" cmake $options -WorkingDirectory $source | Out-Null
        }
        Invoke-ValidationCommand $context "$Step-build" powershell ($common + @((Join-Path $source 'build.ps1'), '-Configuration', $Configuration, '-BuildDirectory', $build, '-RunId', $context.RunId)) -WorkingDirectory $source | Out-Null
        $junit = Join-Path $source "artifacts/validation/$($context.RunId)/junit-$($Configuration.ToLowerInvariant()).xml"
        Copy-Item -LiteralPath $junit -Destination (Join-Path $context.ArtifactDirectory "junit-index-$Step.xml")
    }
    $after = (Invoke-ValidationCommand $context "$Step-confirm-tree" git @('write-tree')).Output.Trim()
    if ($after -ne $tree) { throw 'The index changed during validation; stage again and repeat' }
    Write-ValidationJson ([ordered]@{ tree = $tree; source = $source; archive = $archive; build = $build;
        configuration = $Configuration; built = (-not $SkipBuild); dependency_cache = $DependencyCache }) (Join-Path $context.ArtifactDirectory "index-$Step.json")
    $status = 'passed'
} finally { Complete-ValidationContext $context $status }
