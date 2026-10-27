param(
    [string]$Executable = 'build/windows/bin/Release/drawer_benchmarks.exe',
    [string]$OutputPath,
    [ValidateRange(1, 100)][int]$Repetitions = 3,
    [string]$RunId
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'validation/common.psm1') -Force
$context = New-ValidationContext -Task 'benchmarks' -RunId $RunId
$status = 'failed'
try {
    if (-not [System.IO.Path]::IsPathRooted($Executable)) { $Executable = Join-Path $context.RepositoryRoot $Executable }
    if (-not $OutputPath) { $OutputPath = Join-Path $context.ArtifactDirectory 'benchmark-release.json' }
    if (-not [System.IO.Path]::IsPathRooted($OutputPath)) { $OutputPath = Join-Path $context.RepositoryRoot $OutputPath }
    New-Item -ItemType Directory -Path (Split-Path $OutputPath -Parent) -Force | Out-Null
    Invoke-ValidationCommand $context 'benchmark' $Executable @('--benchmark_min_time=0.1s', "--benchmark_repetitions=$Repetitions", "--benchmark_out=$OutputPath", '--benchmark_out_format=json', '--benchmark_report_aggregates_only=true') | Out-Null
    $report = Get-Content -LiteralPath $OutputPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if (-not $report.benchmarks -or $report.benchmarks.Count -eq 0) { throw 'Benchmark report contains no measurements' }
    $errors = @($report.benchmarks | Where-Object { $flag = $_.PSObject.Properties['error_occurred']; $flag -and $flag.Value })
    if ($errors.Count) { throw ($errors | ConvertTo-Json -Depth 10) }
    $status = 'passed'
} finally { Complete-ValidationContext $context $status }
