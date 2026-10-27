Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-DrawerRepositoryRoot {
    return (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent)
}
function Write-ValidationJson($Value, [string]$Path) {
    [System.IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 16), [System.Text.UTF8Encoding]::new($false))
}
function ConvertTo-ProcessArgument([string]$Value) {
    $escaped = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $escaped = [regex]::Replace($escaped, '(\\+)$', '$1$1')
    return '"' + $escaped + '"'
}
function Invoke-ValidationCommand {
    param($Context, [string]$Step, [string]$Executable, [string[]]$Arguments = @(),
        [string]$WorkingDirectory = (Get-DrawerRepositoryRoot),
        [int]$TimeoutSeconds = 1800, [switch]$AllowFailure, [hashtable]$Environment = @{})
    if ($Step -notmatch '^[a-z0-9-]+$') { throw "Invalid step name: $Step" }
    $command = Get-Command $Executable -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $Executable -PathType Leaf) {
        $file = (Get-Item -LiteralPath $Executable).FullName
    } elseif ($command) { $file = $command.Source } else { throw "Missing executable: $Executable" }
    $info = [System.Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $file
    $info.Arguments = ($Arguments | ForEach-Object { ConvertTo-ProcessArgument $_ }) -join ' '
    $info.WorkingDirectory = $WorkingDirectory
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = [System.Text.UTF8Encoding]::new($false)
    $info.StandardErrorEncoding = [System.Text.UTF8Encoding]::new($false)
    foreach ($key in $Environment.Keys) { $info.EnvironmentVariables[$key] = $Environment[$key] }
    if ($Step -notlike 'source-*') { Write-Host "[$Step] $Executable" }
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $info
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        if (-not $process.Start()) { throw "Could not start $file" }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
        if ($timedOut) { $process.Kill(); $process.WaitForExit() }
        $output = $stdout.GetAwaiter().GetResult()
        $errorOutput = $stderr.GetAwaiter().GetResult()
        $code = if ($timedOut) { -1 } else { $process.ExitCode }
    } finally {
        $timer.Stop()
        $process.Dispose()
    }
    $log = Join-Path $Context.ArtifactDirectory "logs/$Step.log"
    [System.IO.File]::WriteAllText($log, ($output + $errorOutput), [System.Text.UTF8Encoding]::new($false))
    $record = [ordered]@{ step = $Step; executable = $file; arguments = $Arguments;
        exit_code = $code; allow_failure = $AllowFailure.IsPresent; duration_seconds = $timer.Elapsed.TotalSeconds; log = $log }
    $Context.Commands.Add($record)
    if ($code -ne 0 -and -not $AllowFailure) {
        $tail = ($output + $errorOutput)
        if ($tail.Length -gt 3000) { $tail = $tail.Substring($tail.Length - 3000) }
        throw "$Step failed (exit $code). Log: $log`n$tail"
    }
    return [pscustomobject]@{ ExitCode = $code; Output = $output; ErrorOutput = $errorOutput; Log = $log }
}
function New-ValidationContext {
    param([string]$Task, [string]$RunId)
    if ($Task -notmatch '^[a-z0-9-]+$') { throw "Invalid task name: $Task" }
    if (-not $RunId) { $RunId = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8) }
    if ($RunId -notmatch '^\d{8}T\d{9}Z-[a-f0-9]{8}$') { throw "Invalid run ID: $RunId" }
    $root = Get-DrawerRepositoryRoot
    $artifact = Join-Path $root "artifacts/validation/$RunId"
    $temporary = Join-Path $root ".tmp/validation/$RunId"
    foreach ($path in $artifact, $temporary, (Join-Path $artifact 'logs')) {
        New-Item -ItemType Directory -Path $path -Force | Out-Null
    }
    $context = [pscustomobject]@{ RunId = $RunId; Task = $Task; RepositoryRoot = $root;
        ArtifactDirectory = $artifact; TemporaryDirectory = $temporary;
        Commands = [System.Collections.Generic.List[object]]::new() }
    $manifest = Join-Path $artifact 'run.json'
    if (-not (Test-Path -LiteralPath $manifest)) {
        $revision = 'unknown'
        $dirty = $null
        if (Get-Command git -ErrorAction SilentlyContinue) {
            $top = Invoke-ValidationCommand $context 'source-root' git @('-C', $root, 'rev-parse', '--show-toplevel') -AllowFailure
            $sameRoot = $top.ExitCode -eq 0 -and [System.IO.Path]::GetFullPath($top.Output.Trim()) -eq [System.IO.Path]::GetFullPath($root)
            if ($sameRoot) {
                $git = Invoke-ValidationCommand $context 'source-revision' git @('-C', $root, 'rev-parse', 'HEAD') -AllowFailure
                if ($git.ExitCode -eq 0) {
                    $revision = $git.Output.Trim()
                    $git = Invoke-ValidationCommand $context 'source-status' git @('-C', $root, 'status', '--porcelain')
                    $dirty = $git.Output.Trim().Length -gt 0
                    $files = Invoke-ValidationCommand $context 'source-files' git @('-C', $root, '-c', 'core.quotepath=false', 'ls-files', '--cached', '--others', '--exclude-standard')
                    $hashes = @($files.Output -split '\r?\n' | Where-Object { $_ } | Sort-Object -Unique | ForEach-Object {
                        $relative = $_
                        $path = Join-Path $root $relative
                        if (Test-Path -LiteralPath $path -PathType Leaf) {
                            [ordered]@{ path = $relative; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
                        }
                    })
                    Write-ValidationJson $hashes (Join-Path $artifact 'source-files.json')
                }
            }
        }
        Write-ValidationJson ([ordered]@{ schema_version = 1; run_id = $RunId;
            created_utc = [DateTime]::UtcNow.ToString('o'); source_revision = $revision;
            dirty_worktree = $dirty; powershell_version = $PSVersionTable.PSVersion.ToString();
            os_version = [Environment]::OSVersion.VersionString }) $manifest
    }
    return $context
}
function Complete-ValidationContext($Context, [string]$Status) {
    Write-ValidationJson ([ordered]@{ schema_version = 1; task = $Context.Task; status = $Status;
        completed_utc = [DateTime]::UtcNow.ToString('o'); commands = @($Context.Commands.ToArray()) }) `
        (Join-Path $Context.ArtifactDirectory "task-$($Context.Task).json")
    Write-Output "Validation $($Context.Task): $Status; artifacts: $($Context.ArtifactDirectory)"
}
Export-ModuleMember -Function Get-DrawerRepositoryRoot, Write-ValidationJson, Invoke-ValidationCommand, New-ValidationContext, Complete-ValidationContext
