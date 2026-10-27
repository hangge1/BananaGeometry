param(
    [string]$Executable = 'build/windows/bin/Release/drawer_benchmarks.exe',
    [string]$OutputPath = 'build/windows/benchmark-Release.json',
    [ValidateRange(1, 100)][int]$Repetitions = 3
)
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path $PSScriptRoot -Parent
Push-Location $repositoryRoot
try {
    $resolvedExecutable = (Get-Item -LiteralPath $Executable).FullName
    $resolvedOutput = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
    New-Item -ItemType Directory -Path (Split-Path $resolvedOutput -Parent) -Force | Out-Null
    & $resolvedExecutable '--benchmark_min_time=0.1s' "--benchmark_repetitions=$Repetitions" `
        "--benchmark_out=$resolvedOutput" '--benchmark_out_format=json' '--benchmark_report_aggregates_only=true'
    if ($LASTEXITCODE -ne 0) { throw 'Benchmark process failed' }
    $report = Get-Content -LiteralPath $resolvedOutput -Raw -Encoding UTF8 | ConvertFrom-Json
    if (-not $report.benchmarks -or $report.benchmarks.Count -eq 0) {
        throw 'Benchmark report contains no measurements'
    }
    # Google Benchmark can return success after SkipWithError; inspect the report.
    $errors = @($report.benchmarks | Where-Object { $_.error_occurred })
    if ($errors.Count) { throw ($errors | ConvertTo-Json -Depth 10) }
    Write-Output "Benchmark report: $resolvedOutput"
} finally {
    Pop-Location
}
