param([string]$RunId)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force
$context = New-ValidationContext -Task 'documents' -RunId $RunId
$status = 'failed'
try {
    $root = $context.RepositoryRoot
    $files = @((Get-Item -LiteralPath (Join-Path $root 'README.md')), (Get-Item -LiteralPath (Join-Path $root 'AGENTS.md')))
    foreach ($directory in 'docs', 'cases', 'benchmarks', 'third_party', 'tools') {
        $files += @(Get-ChildItem -LiteralPath (Join-Path $root $directory) -Filter '*.md' -File -Recurse)
    }
    $issues = [System.Collections.Generic.List[string]]::new()
    $utf8 = [System.Text.UTF8Encoding]::new($false, $true)
    foreach ($file in $files) {
        $source = $utf8.GetString([System.IO.File]::ReadAllBytes($file.FullName))
        foreach ($link in [regex]::Matches($source, '\[[^\]]*\]\(([^)]+)\)')) {
            $target = $link.Groups[1].Value.Trim('<', '>').Split('#')[0]
            if (-not $target -or $target -match '^https?://') { continue }
            $target = [Uri]::UnescapeDataString($target)
            if (-not (Test-Path -LiteralPath (Join-Path $file.DirectoryName $target))) {
                $issues.Add("$($file.FullName): $target")
            }
        }
    }
    Write-ValidationJson ([ordered]@{ checked_files = $files.Count; issues = @($issues.ToArray()) }) (Join-Path $context.ArtifactDirectory 'documents.json')
    if ($issues.Count) { throw ($issues -join [Environment]::NewLine) }
    $status = 'passed'
} finally { Complete-ValidationContext $context $status }
