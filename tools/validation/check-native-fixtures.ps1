param([string]$Executable = 'build/windows/bin/Release/drawer.exe', [string]$RunId)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force
$context = New-ValidationContext -Task 'native-fixtures' -RunId $RunId
$status = 'failed'
try {
    if (-not [System.IO.Path]::IsPathRooted($Executable)) { $Executable = Join-Path $context.RepositoryRoot $Executable }
    $fixture = Join-Path $context.TemporaryDirectory 'rejected-fixture.lua'
    [System.IO.File]::WriteAllText($fixture, "error('expected-native-fixture-error')")
    $errorLog = Join-Path $context.ArtifactDirectory 'logs/rejected-fixture-native.log'
    $result = Invoke-ValidationCommand $context 'rejected-fixture' $Executable @('--smoke-test', '--fixture', $fixture, '--error-log', $errorLog) -TimeoutSeconds 30 -AllowFailure
    if ($result.ExitCode -eq 0 -or -not (Test-Path -LiteralPath $errorLog)) { throw 'Native fixture failure did not produce diagnostics' }
    $diagnostic = Get-Content -LiteralPath $errorLog -Raw -Encoding UTF8
    if ($diagnostic -notmatch 'expected-native-fixture-error' -or $diagnostic -notmatch 'rejected-fixture.lua') { throw 'Native fixture failed for an unexpected reason' }
    $workspaceLog = Join-Path $context.ArtifactDirectory 'logs/rejected-workspace-native.log'
    $missing = Join-Path $context.TemporaryDirectory 'missing workspace'
    $result = Invoke-ValidationCommand $context 'rejected-workspace' $Executable @('--smoke-test', '--workspace', $missing, '--error-log', $workspaceLog) -TimeoutSeconds 30 -AllowFailure
    if ($result.ExitCode -eq 0 -or -not (Test-Path -LiteralPath $workspaceLog)) { throw 'Invalid explicit workspace was not rejected' }
    $diagnostic = Get-Content -LiteralPath $workspaceLog -Raw -Encoding UTF8
    if ($diagnostic -notmatch 'Workspace requires sdk/drawer.lua and cases/' -or -not $diagnostic.Contains($missing)) { throw 'Invalid workspace failed for an unexpected reason' }
    $status = 'passed'
} finally { Complete-ValidationContext $context $status }
