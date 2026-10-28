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
    if ($diagnostic -notmatch 'Case directory does not exist:' -or -not $diagnostic.Contains($missing)) { throw 'Invalid workspace failed for an unexpected reason' }
    $directoryLog = Join-Path $context.ArtifactDirectory 'logs/rejected-case-directory-native.log'
    $result = Invoke-ValidationCommand $context 'rejected-case-directory' $Executable @('--smoke-test', '--cases-dir', $missing, '--error-log', $directoryLog) -TimeoutSeconds 30 -AllowFailure
    if ($result.ExitCode -eq 0 -or -not (Test-Path -LiteralPath $directoryLog)) { throw 'Invalid explicit case directory was not rejected' }
    $diagnostic = Get-Content -LiteralPath $directoryLog -Raw -Encoding UTF8
    if ($diagnostic -notmatch 'Case directory does not exist:' -or -not $diagnostic.Contains($missing)) { throw 'Invalid case directory failed for an unexpected reason' }
    $documentLog = Join-Path $context.ArtifactDirectory 'logs/rejected-document-native.log'
    $invalidDocument = Join-Path $context.TemporaryDirectory 'invalid-document.json'
    [System.IO.File]::WriteAllText($invalidDocument, '{"format":"drawer","format_version":999}')
    $result = Invoke-ValidationCommand $context 'rejected-document' $Executable @('--smoke-test', '--document', $invalidDocument, '--error-log', $documentLog) -TimeoutSeconds 30 -AllowFailure
    if ($result.ExitCode -eq 0 -or -not (Test-Path -LiteralPath $documentLog)) { throw 'Invalid document was not rejected with diagnostics' }
    $diagnostic = Get-Content -LiteralPath $documentLog -Raw -Encoding UTF8
    if ($diagnostic -notmatch 'Unsupported workspace format') { throw 'Invalid document failed for an unexpected reason' }
    $status = 'passed'
} finally { Complete-ValidationContext $context $status }
