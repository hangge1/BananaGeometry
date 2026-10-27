param([string]$Executable = 'build/windows/bin/Release/drawer.exe', [string[]]$Scene, [string]$RunId)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force
$context = New-ValidationContext -Task 'screenshots' -RunId $RunId
$status = 'failed'
try {
    $root = $context.RepositoryRoot
    if (-not [System.IO.Path]::IsPathRooted($Executable)) { $Executable = Join-Path $root $Executable }
    $manifest = Get-Content -LiteralPath (Join-Path $root 'tests/scenarios/manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($manifest.schema_version -ne 1) { throw 'Unsupported scenario manifest' }
    $scenes = @($manifest.scenes | Where-Object { -not $Scene -or $_.id -in $Scene })
    if (-not $scenes.Count -or ($Scene -and @($Scene | Where-Object { $_ -notin $scenes.id }).Count)) { throw 'Unknown or empty screenshot selection' }
    $destination = Join-Path $context.ArtifactDirectory 'screenshots'
    $scratch = Join-Path $context.TemporaryDirectory 'screenshots'
    New-Item -ItemType Directory -Path $destination, $scratch -Force | Out-Null
    Add-Type -AssemblyName System.Drawing
    $results = @()
    foreach ($entry in $scenes) {
        if ($entry.id -notmatch '^[a-z0-9]+(-[a-z0-9]+)*$' -or $entry.fixture -notmatch '^[a-z0-9-]+\.lua$') { throw 'Invalid scene identifier or fixture path' }
        $fixture = Join-Path $root "tests/scenarios/$($entry.fixture)"
        $bitmapPath = Join-Path $scratch "$($entry.id).bmp"
        $errorLog = Join-Path $context.ArtifactDirectory "logs/$($entry.id)-native.log"
        $result = Invoke-ValidationCommand $context $entry.id $Executable @('--smoke-test', '--case-id', $entry.case_id, '--fixture', $fixture, '--screenshot', $bitmapPath, '--error-log', $errorLog) -TimeoutSeconds 30 -AllowFailure
        if ($result.ExitCode -ne 0) {
            $details = if (Test-Path -LiteralPath $errorLog) { Get-Content -LiteralPath $errorLog -Raw -Encoding UTF8 } else { 'No native diagnostic was produced' }
            throw "Scene $($entry.id) failed (exit $($result.ExitCode)): $details"
        }
        if (-not (Test-Path -LiteralPath $bitmapPath)) { throw "Missing capture: $($entry.id)" }
        $bitmap = [System.Drawing.Image]::FromFile($bitmapPath)
        $png = Join-Path $destination "$($entry.id).png"
        try {
            if ($bitmap.Width -lt 1 -or $bitmap.Height -lt 1) { throw 'Invalid screenshot dimensions' }
            $bitmap.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
            $results += [ordered]@{ id = $entry.id; case_id = $entry.case_id; status = 'captured';
                width = $bitmap.Width; height = $bitmap.Height; file = $png }
        } finally { $bitmap.Dispose() }
        Remove-Item -LiteralPath $bitmapPath
    }
    Write-ValidationJson $results (Join-Path $context.ArtifactDirectory 'screenshots.json')
    $status = 'passed'
} finally { Complete-ValidationContext $context $status }
