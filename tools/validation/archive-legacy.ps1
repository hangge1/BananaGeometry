param([switch]$Apply, [string]$RunId)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force
$context = New-ValidationContext -Task 'legacy-archive' -RunId $RunId
$status = 'failed'
try {
    $root = $context.RepositoryRoot
    $build = [System.IO.Path]::GetFullPath((Join-Path $root 'build'))
    $candidates = @()
    if (Test-Path -LiteralPath $build) {
        $candidates = @(Get-ChildItem -LiteralPath $build -File | Where-Object { $_.Extension -in '.png', '.bmp', '.log', '.py', '.ps1', '.txt' })
    }
    foreach ($name in 'windows', 'versioning') {
        $folder = Join-Path $build $name
        if (Test-Path -LiteralPath $folder) {
            $candidates += @(Get-ChildItem -LiteralPath $folder -File | Where-Object { $_.Name -like 'test-results-*.xml' -or $_.Name -like 'benchmark-*.json' -or $_.Name -like 'verify-*.log' -or $_.Name -eq 'checkpoint.py' })
        }
    }
    foreach ($relative in 'flexible-preview', 'selection-preview', 'module-rule-fixture', 'module-backup', 'test-output', 'windows/tests/test-output', 'windows/test-output') {
        $path = Join-Path $build $relative
        if (Test-Path -LiteralPath $path) { $candidates += Get-Item -LiteralPath $path }
    }
    $plan = @()
    foreach ($item in $candidates) {
        $source = [System.IO.Path]::GetFullPath($item.FullName)
        if (-not $source.StartsWith($build + [System.IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw "Archive source escaped build/: $source" }
        $evidence = $item.Extension -in '.png', '.log', '.xml', '.json'
        $base = if ($evidence) { $context.ArtifactDirectory } else { $context.TemporaryDirectory }
        $relative = $source.Substring($build.Length + 1)
        $destination = [System.IO.Path]::GetFullPath((Join-Path $base "legacy/$relative"))
        if (-not $destination.StartsWith($base + [System.IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw "Archive destination escaped its run: $destination" }
        $plan += [ordered]@{ source = $source; destination = $destination; kind = if ($evidence) { 'evidence' } else { 'temporary' }; status = 'planned' }
    }
    Write-ValidationJson ([ordered]@{ applied = $false; entries = $plan }) (Join-Path $context.ArtifactDirectory 'legacy-map.json')
    if ($Apply) {
        foreach ($entry in $plan) {
            if (Test-Path -LiteralPath $entry.destination) { throw "Archive destination already exists: $($entry.destination)" }
            New-Item -ItemType Directory -Path (Split-Path $entry.destination -Parent) -Force | Out-Null
            Move-Item -LiteralPath $entry.source -Destination $entry.destination
            $entry.status = 'moved'
            Write-ValidationJson ([ordered]@{ applied = $false; entries = $plan }) (Join-Path $context.ArtifactDirectory 'legacy-map.json')
        }
        Write-ValidationJson ([ordered]@{ applied = $true; entries = $plan }) (Join-Path $context.ArtifactDirectory 'legacy-map.json')
    }
    Write-Output "Legacy entries: $($plan.Count); apply: $Apply"
    $status = 'passed'
} finally { Complete-ValidationContext $context $status }
