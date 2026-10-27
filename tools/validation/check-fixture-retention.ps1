param([string]$Executable = 'build/windows/bin/Release/drawer_tests.exe', [string]$RunId)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force
$context = New-ValidationContext -Task 'fixture-retention' -RunId $RunId
$status = 'failed'
try {
    if (-not [System.IO.Path]::IsPathRooted($Executable)) { $Executable = Join-Path $context.RepositoryRoot $Executable }
    foreach ($fail in $true, $false) {
        $environment = @{ DRAWER_VALIDATION_RUN_ID = $context.RunId; DRAWER_ARTIFACT_PROBE_FAIL = if ($fail) { '1' } else { '' } }
        $step = if ($fail) { 'failure-probe' } else { 'success-probe' }
        $result = Invoke-ValidationCommand $context $step $Executable @('Temporary fixture lifecycle probe', '--reporter', 'compact') -Environment $environment -AllowFailure
        $match = [regex]::Match($result.Output, '(?m)^DRAWER_FIXTURE_PATH=(.+)\r?$')
        if (-not $match.Success) { throw 'Fixture probe did not identify its directory' }
        $path = $match.Groups[1].Value.Trim()
        if ($fail) {
            if ($result.ExitCode -eq 0 -or -not (Test-Path -LiteralPath (Join-Path $path 'evidence.txt'))) { throw 'Failed test evidence was not retained' }
        } elseif ($result.ExitCode -ne 0 -or (Test-Path -LiteralPath $path)) { throw 'Successful test fixtures were not cleaned' }
    }
    $status = 'passed'
} finally { Complete-ValidationContext $context $status }
