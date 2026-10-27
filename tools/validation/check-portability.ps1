param([string]$RunId)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force
$context = New-ValidationContext -Task 'portability' -RunId $RunId
$status = 'failed'
try {
    $root = $context.RepositoryRoot
    $checkout = Join-Path $context.TemporaryDirectory 'source checkout with spaces'
    New-Item -ItemType Directory -Path $checkout -Force | Out-Null
    $files = Invoke-ValidationCommand $context 'export-file-list' git @('-C', $root, '-c', 'core.quotepath=false', 'ls-files', '--cached', '--others', '--exclude-standard')
    foreach ($relative in ($files.Output -split '\r?\n' | Where-Object { $_ } | Sort-Object -Unique)) {
        $source = Join-Path $root $relative
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { continue }
        $destination = [System.IO.Path]::GetFullPath((Join-Path $checkout $relative))
        if (-not $destination.StartsWith($checkout + [System.IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Source export escaped its sandbox' }
        New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $destination
    }
    $common = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File')
    Invoke-ValidationCommand $context 'isolated-documents' powershell ($common + @((Join-Path $checkout 'tools/validation/check-docs.ps1'))) -WorkingDirectory $checkout | Out-Null
    Invoke-ValidationCommand $context 'isolated-guards' powershell ($common + @((Join-Path $checkout 'tools/validation/check-guards.ps1'))) -WorkingDirectory $checkout | Out-Null
    Write-ValidationJson ([ordered]@{ source_copy = $checkout; checks = @('documents', 'positive-and-negative-guards'); includes_build_directory = $false }) (Join-Path $context.ArtifactDirectory 'portability.json')
    $status = 'passed'
} finally { Complete-ValidationContext $context $status }
