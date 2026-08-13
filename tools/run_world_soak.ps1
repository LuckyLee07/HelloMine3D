[CmdletBinding()]
param(
    [string]$OutputDir = "",
    [int]$DurationSeconds = 60,
    [int]$Seed = 20260813,
    [int]$SampleIntervalSeconds = 5,
    [int]$ShutdownGraceSeconds = 180,
    [switch]$Formal
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
$exePath = Join-Path $repoRoot "bin\HelloMine3DSoak.exe"

if ($DurationSeconds -lt 1 -or $DurationSeconds -gt 86400) {
    throw "DurationSeconds must be between 1 and 86400."
}
if ($Formal -and $DurationSeconds -lt 1800) {
    throw "A formal R2 run must last at least 1800 seconds."
}
if ($SampleIntervalSeconds -lt 1 -or $SampleIntervalSeconds -gt 60) {
    throw "SampleIntervalSeconds must be between 1 and 60."
}
if ($ShutdownGraceSeconds -lt 10 -or $ShutdownGraceSeconds -gt 3600) {
    throw "ShutdownGraceSeconds must be between 10 and 3600."
}
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "HelloMine3DSoak.exe not found: $exePath"
}

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $runId = "{0:yyyyMMddHHmmssfff}-{1}" -f (Get-Date), $PID
    $OutputDir = Join-Path $repoRoot "bin\soak_runs\$runId"
}
elseif (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = [System.IO.Path]::GetFullPath(
        (Join-Path $repoRoot $OutputDir))
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$stdoutPath = Join-Path $OutputDir "process.stdout.log"
$stderrPath = Join-Path $OutputDir "process.stderr.log"
$processCsvPath = Join-Path $OutputDir "process-snapshots.csv"
$processSummaryPath = Join-Path $OutputDir "process-summary.txt"
$worldSummaryPath = Join-Path $OutputDir "summary.txt"

Set-Content -LiteralPath $processCsvPath -Encoding utf8 -Value `
    "elapsed_seconds,private_bytes,working_set_bytes,handles,threads"

$arguments = @(
    "--duration-seconds", $DurationSeconds,
    "--seed", $Seed,
    "--output-dir", $OutputDir
)

$previousRoot = $env:HELLOMINE3D_ROOT
$process = $null
try {
    $env:HELLOMINE3D_ROOT = $repoRoot
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $exePath
    $startInfo.Arguments = ($arguments | ForEach-Object {
        '"' + ([string]$_).Replace('"', '\"') + '"'
    }) -join " "
    $startInfo.WorkingDirectory = Join-Path $repoRoot "bin"
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw "Failed to start HelloMine3DSoak.exe."
    }
    # Begin draining both redirected streams immediately. Waiting until the
    # child exits can fill the OS pipe during a long run and block the child.
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
}
finally {
    $env:HELLOMINE3D_ROOT = $previousRoot
}

$processHandle = $process.Handle
$started = Get-Date
$samples = @()
$timedOut = $false
Write-Host "[WORLD_SOAK] started pid=$($process.Id) durationSeconds=$DurationSeconds formal=$($Formal.IsPresent)"
Write-Host "[WORLD_SOAK] outputDir=$OutputDir seed=$Seed scheduleVersion=1"

while (-not $process.HasExited) {
    try {
        $process.Refresh()
        $elapsed = [Math]::Round(((Get-Date) - $started).TotalSeconds, 3)
        $sample = [pscustomobject]@{
            elapsed = $elapsed
            private = [int64]$process.PrivateMemorySize64
            working = [int64]$process.WorkingSet64
            handles = [int]$process.HandleCount
            threads = [int]$process.Threads.Count
        }
        $samples += $sample
        Add-Content -LiteralPath $processCsvPath -Encoding utf8 -Value `
            ("{0:F3},{1},{2},{3},{4}" -f $sample.elapsed, $sample.private, `
                $sample.working, $sample.handles, $sample.threads)
        if ($elapsed -gt ($DurationSeconds + $ShutdownGraceSeconds)) {
            $timedOut = $true
            $process.Kill()
            break
        }
    }
    catch {
        if (-not $process.HasExited) {
            throw
        }
    }

    if (-not $process.HasExited) {
        Start-Sleep -Seconds $SampleIntervalSeconds
    }
}

$process.WaitForExit()
$exitCode = $process.ExitCode
$capturedStdout = $stdoutTask.Result
$capturedStderr = $stderrTask.Result
Set-Content -LiteralPath $stdoutPath -Encoding utf8 -Value $capturedStdout
Set-Content -LiteralPath $stderrPath -Encoding utf8 -Value $capturedStderr
if ($samples.Count -eq 0) {
    throw "Soak process exited before the first process sample."
}

$warmupIndex = [Math]::Min(
    $samples.Count - 1,
    [Math]::Floor($samples.Count * 0.2))
$steadySamples = @($samples[$warmupIndex..($samples.Count - 1)])
$peakPrivate = ($samples | Measure-Object -Property private -Maximum).Maximum
$peakWorking = ($samples | Measure-Object -Property working -Maximum).Maximum
$peakHandles = ($samples | Measure-Object -Property handles -Maximum).Maximum
$peakThreads = ($samples | Measure-Object -Property threads -Maximum).Maximum
$minimumSteadyPrivate =
    ($steadySamples | Measure-Object -Property private -Minimum).Minimum
$minimumSteadyHandles =
    ($steadySamples | Measure-Object -Property handles -Minimum).Minimum
$finalSample = $samples[-1]
$privateGrowth = $finalSample.private - $minimumSteadyPrivate
$handleGrowth = $finalSample.handles - $minimumSteadyHandles
$elapsedSeconds = [Math]::Round(((Get-Date) - $started).TotalSeconds, 3)

$worldValues = @{}
if (Test-Path -LiteralPath $worldSummaryPath -PathType Leaf) {
    foreach ($line in Get-Content -LiteralPath $worldSummaryPath) {
        $separator = $line.IndexOf("=")
        if ($separator -gt 0) {
            $worldValues[$line.Substring(0, $separator)] =
                $line.Substring($separator + 1)
        }
    }
}

$failures = @()
if ($exitCode -ne 0) {
    $failures += "child_exit_code=$exitCode"
}
if ($timedOut) {
    $failures += "child_timeout_after_shutdown_grace"
}
if (-not $worldValues.ContainsKey("status") -or
    $worldValues["status"] -ne "PASS") {
    $failures += "world_summary_missing_or_failed"
}
if ($Formal -and
    [int]$worldValues["duration_completed_seconds"] -lt 1800) {
    $failures += "formal_duration_below_1800_seconds"
}
if ($peakPrivate -gt 2GB) {
    $failures += "private_bytes_exceeded_2GiB"
}
if ($peakHandles -gt 4096) {
    $failures += "handles_exceeded_4096"
}
if ($privateGrowth -gt 256MB) {
    $failures += "steady_private_growth_exceeded_256MiB"
}
if ($handleGrowth -gt 128) {
    $failures += "steady_handle_growth_exceeded_128"
}

$status = if ($failures.Count -eq 0) { "PASS" } else { "FAIL" }
$summaryLines = @(
    "status=$status",
    "formal=$($Formal.IsPresent.ToString().ToLowerInvariant())",
    "seed=$Seed",
    "schedule_version=1",
    "duration_requested_seconds=$DurationSeconds",
    "wall_duration_seconds=$elapsedSeconds",
    "sample_interval_seconds=$SampleIntervalSeconds",
    "shutdown_grace_seconds=$ShutdownGraceSeconds",
    "child_timed_out=$($timedOut.ToString().ToLowerInvariant())",
    "process_samples=$($samples.Count)",
    "child_exit_code=$exitCode",
    "peak_private_bytes=$peakPrivate",
    "peak_working_set_bytes=$peakWorking",
    "peak_handles=$peakHandles",
    "peak_threads=$peakThreads",
    "final_private_bytes=$($finalSample.private)",
    "final_working_set_bytes=$($finalSample.working)",
    "final_handles=$($finalSample.handles)",
    "final_threads=$($finalSample.threads)",
    "steady_private_growth_bytes=$privateGrowth",
    "steady_handle_growth=$handleGrowth",
    "failure_reasons=$($failures -join ';')"
)
Set-Content -LiteralPath $processSummaryPath -Encoding utf8 `
    -Value $summaryLines

Write-Host "[WORLD_SOAK] peakPrivateBytes=$peakPrivate peakHandles=$peakHandles peakThreads=$peakThreads"
Write-Host "[WORLD_SOAK] steadyGrowthBytes=$privateGrowth steadyHandleGrowth=$handleGrowth"
Write-Host "[WORLD_SOAK] status=$status outputDir=$OutputDir"

if ($failures.Count -ne 0) {
    throw "World soak failed: $($failures -join ', ')"
}
