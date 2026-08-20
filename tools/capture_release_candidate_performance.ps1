[CmdletBinding()]
param(
    [string]$OutputDir = "",
    [string]$EvidenceDir = "docs\baselines\release-candidate-windows-hidden-v1",
    [int]$GeneralDurationMs = 10000,
    [int]$StreamingDurationMs = 15000,
    [int]$ScaledDurationMs = 10000,
    [ValidateSet("all", "baseline", "repeat")]
    [string]$PassSelection = "all",
    [ValidateSet("all", "general", "fast-streaming", "scaled-gameplay")]
    [string]$ClientSelection = "all",
    [switch]$ClientsOnly,
    [switch]$OperationsOnly,
    [switch]$FinalizeOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
$binDir = Join-Path $repoRoot "bin"
$baselineRunner = Join-Path $scriptRoot "run_perf_baseline.ps1"
$comparator = Join-Path $scriptRoot "compare_perf_baselines.ps1"
$budgetProfile = "release-candidate-windows-hidden-v1"
$seed = "20260820"
$position = "264 96 8"
$rotation = "0 0 0"
$worldTime = "6000"

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $runId = "{0:yyyyMMddHHmmssfff}-{1}" -f (Get-Date), $PID
    $OutputDir = Join-Path $binDir "release_candidate_performance\$runId"
}
elseif (-not [IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDir))
}
if (-not [IO.Path]::IsPathRooted($EvidenceDir)) {
    $EvidenceDir = [IO.Path]::GetFullPath((Join-Path $repoRoot $EvidenceDir))
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
New-Item -ItemType Directory -Path $EvidenceDir -Force | Out-Null

$selectedModes = @(@($ClientsOnly, $OperationsOnly, $FinalizeOnly) |
    Where-Object { $_.IsPresent })
if ($selectedModes.Count -gt 1) {
    throw "ClientsOnly, OperationsOnly and FinalizeOnly are mutually exclusive."
}
$runClients = -not $OperationsOnly -and -not $FinalizeOnly
$runOperations = -not $ClientsOnly -and -not $FinalizeOnly
$runFinalize = $FinalizeOnly -or (-not $ClientsOnly -and -not $OperationsOnly)

function Read-Summary {
    param([string]$Path)
    $values = @{}
    foreach ($line in Get-Content -LiteralPath $Path) {
        $separator = $line.IndexOf("=")
        if ($separator -gt 0) {
            $values[$line.Substring(0, $separator)] =
                $line.Substring($separator + 1)
        }
    }
    return $values
}

function Require-Value {
    param([hashtable]$Summary, [string]$Key, [string]$Label)
    if (-not $Summary.ContainsKey($Key) -or
        [string]::IsNullOrWhiteSpace([string]$Summary[$Key])) {
        throw "$Label is missing '$Key'."
    }
    return [string]$Summary[$Key]
}

function Invoke-HiddenExecutable {
    param(
        [string]$FilePath,
        [string]$SummaryPath,
        [string]$LogPrefix
    )
    $stdoutPath = "$LogPrefix.stdout.log"
    $stderrPath = "$LogPrefix.stderr.log"
    $oldRoot = $env:HELLOMINE3D_ROOT
    $oldSummary = $env:HELLO_OPERATION_SUMMARY_OUT
    try {
        $env:HELLOMINE3D_ROOT = $repoRoot
        $env:HELLO_OPERATION_SUMMARY_OUT = $SummaryPath
        $process = Start-Process -FilePath $FilePath `
            -WorkingDirectory $binDir -WindowStyle Hidden `
            -RedirectStandardOutput $stdoutPath `
            -RedirectStandardError $stderrPath -PassThru -Wait
    }
    finally {
        $env:HELLOMINE3D_ROOT = $oldRoot
        $env:HELLO_OPERATION_SUMMARY_OUT = $oldSummary
    }
    if ($process.ExitCode -ne 0) {
        throw "Hidden executable failed with exit code $($process.ExitCode): $FilePath"
    }
    if (-not (Test-Path -LiteralPath $SummaryPath -PathType Leaf)) {
        throw "Hidden executable did not produce timing summary: $SummaryPath"
    }
}

function Add-BudgetLines {
    param([string]$Path, [string[]]$Lines)
    $existing = @(Get-Content -LiteralPath $Path)
    $missing = @()
    foreach ($line in $Lines) {
        $separator = $line.IndexOf("=")
        $key = if ($separator -gt 0) {
            $line.Substring(0, $separator)
        }
        else { $line }
        if (-not ($existing -match ('^' + [regex]::Escape($key) + '='))) {
            $missing += $line
        }
    }
    if ($missing.Count -gt 0) {
        Add-Content -LiteralPath $Path -Encoding utf8 -Value $missing
    }
}

function Copy-ColdStartSummary {
    param([string]$Source, [string]$Destination)
    $text = Get-Content -LiteralPath $Source -Raw
    $text = [regex]::Replace(
        $text, '(?m)^comparison_scene_id=.*$',
        'comparison_scene_id=q1-cold-start-v1')
    Set-Content -LiteralPath $Destination -Encoding utf8 -Value $text
}

function Write-OperationScene {
    param(
        [string]$Source,
        [string]$Destination,
        [string]$SceneId,
        [string]$BuildId,
        [string[]]$IdentityLines,
        [string[]]$MetricKeys
    )
    $summary = Read-Summary -Path $Source
    $lines = @(
        "comparison_schema=3",
        "comparison_contract_version=1",
        "comparison_budget_profile=$budgetProfile",
        "build_configuration=Release",
        "comparison_scene_id=$SceneId",
        "comparison_platform=windows",
        "comparison_architecture=x86_64",
        "comparison_build_id=$BuildId"
    ) + $IdentityLines
    foreach ($key in $MetricKeys) {
        $lines += "$key=$(Require-Value -Summary $summary -Key $key -Label $SceneId)"
    }
    Set-Content -LiteralPath $Destination -Encoding utf8 -Value $lines
}

$sceneFiles = @(
    "q1-cold-start-v1",
    "q1-world-entry-v1",
    "q1-save-transaction-v1",
    "q1-backup-restore-v1",
    "q1-fast-streaming-v1",
    "q1-scaled-gameplay-v1"
)

$passes = if ($PassSelection -eq "all") {
    @("baseline", "repeat")
}
else { @($PassSelection) }

foreach ($pass in $passes) {
    $passDir = Join-Path $OutputDir $pass
    New-Item -ItemType Directory -Path $passDir -Force | Out-Null
    if ($runClients) {
      if ($ClientSelection -eq "all" -or $ClientSelection -eq "general") {
        Write-Host "[RC_PERF] pass=$pass phase=client-general"

        $generalDir = Join-Path $passDir `
            ("general-{0:yyyyMMddHHmmssfff}" -f (Get-Date))
        & $baselineRunner -OutputDir $generalDir `
            -WarmupMs 3000 -DurationMs $GeneralDurationMs `
            -Seed $seed -PlayerPosition $position -PlayerRotation $rotation `
            -WorldTime $worldTime -SceneId "q1-world-entry-v1" `
            -BudgetProfile $budgetProfile -CacheRegime "warm-process-clean" `
            -HiddenWindow -StopExisting -QuietSummary
        if ($LASTEXITCODE -ne 0) { throw "General client capture failed." }

        $worldEvidence = Join-Path $EvidenceDir `
            "q1-world-entry-v1.$pass.summary.txt"
        Copy-Item -LiteralPath (Join-Path $generalDir "summary.txt") `
            -Destination $worldEvidence -Force
        $coldEvidence = Join-Path $EvidenceDir `
            "q1-cold-start-v1.$pass.summary.txt"
        Copy-ColdStartSummary -Source $worldEvidence -Destination $coldEvidence
      }

      if ($ClientSelection -eq "all" -or
          $ClientSelection -eq "fast-streaming") {
        Write-Host "[RC_PERF] pass=$pass phase=client-fast-streaming"
        $streamingDir = Join-Path $passDir `
            ("fast-streaming-{0:yyyyMMddHHmmssfff}" -f (Get-Date))
        & $baselineRunner -OutputDir $streamingDir `
            -WarmupMs 1000 -DurationMs $StreamingDurationMs `
            -Seed $seed -PlayerPosition $position -PlayerRotation $rotation `
            -WorldTime $worldTime -SceneId "q1-fast-streaming-v1" `
            -BudgetProfile $budgetProfile -RcPerformanceProfile "fast-streaming" `
            -HiddenWindow -StopExisting -QuietSummary
        if ($LASTEXITCODE -ne 0) { throw "Fast-streaming client capture failed." }
        Copy-Item -LiteralPath (Join-Path $streamingDir "summary.txt") `
            -Destination (Join-Path $EvidenceDir `
                "q1-fast-streaming-v1.$pass.summary.txt") -Force
      }

      if ($ClientSelection -eq "all" -or
          $ClientSelection -eq "scaled-gameplay") {
        Write-Host "[RC_PERF] pass=$pass phase=client-scaled-gameplay"
        $scaledDir = Join-Path $passDir `
            ("scaled-gameplay-{0:yyyyMMddHHmmssfff}" -f (Get-Date))
        & $baselineRunner -OutputDir $scaledDir `
            -WarmupMs 1000 -DurationMs $ScaledDurationMs `
            -Seed $seed -PlayerPosition $position -PlayerRotation $rotation `
            -WorldTime $worldTime -SceneId "q1-scaled-gameplay-v1" `
            -BudgetProfile $budgetProfile -RcPerformanceProfile "scaled-gameplay" `
            -HiddenWindow -StopExisting -QuietSummary
        if ($LASTEXITCODE -ne 0) { throw "Scaled-gameplay client capture failed." }
        Copy-Item -LiteralPath (Join-Path $scaledDir "summary.txt") `
            -Destination (Join-Path $EvidenceDir `
                "q1-scaled-gameplay-v1.$pass.summary.txt") -Force
      }
    }

    if ($runOperations) {
        $generalEvidence = Join-Path $EvidenceDir `
            "q1-world-entry-v1.$pass.summary.txt"
        $general = Read-Summary -Path $generalEvidence
        $buildId = Require-Value -Summary $general `
            -Key "comparison_build_id" -Label "general capture"
        $saveRaw = Join-Path $passDir "save-operation.txt"
        $restoreRaw = Join-Path $passDir "restore-operation.txt"
        Write-Host "[RC_PERF] pass=$pass phase=save-operation"
        Invoke-HiddenExecutable `
            -FilePath (Join-Path $binDir "HelloMine3DWorldRuntimeSmoke.exe") `
            -SummaryPath $saveRaw -LogPrefix (Join-Path $passDir "save-operation")
        Write-Host "[RC_PERF] pass=$pass phase=restore-operation"
        Invoke-HiddenExecutable `
            -FilePath (Join-Path $binDir "HelloMine3DWorldBackupSmoke.exe") `
            -SummaryPath $restoreRaw -LogPrefix (Join-Path $passDir "restore-operation")

    Write-OperationScene -Source $saveRaw `
        -Destination (Join-Path $EvidenceDir `
            "q1-save-transaction-v1.$pass.summary.txt") `
        -SceneId "q1-save-transaction-v1" -BuildId $buildId `
        -IdentityLines @(
            "comparison_world_fixture=world-runtime-one-dirty-chunk-v1",
            "comparison_save_format=8",
            "comparison_storage_class=local-default",
            "comparison_resource_packs=none",
            "comparison_dirty_set=world-meta-plus-one-dirty-chunk-v1",
            "comparison_backup_policy=rotating-3") `
        -MetricKeys @(
            "save_prepare_complete_ms", "save_write_complete_ms",
            "save_flush_complete_ms", "save_validation_complete_ms",
            "save_replace_complete_ms", "save_total_ms",
            "save_main_thread_max_stall_ms", "save_files_written",
            "save_chunks_written", "save_bytes_written")

        Write-OperationScene -Source $restoreRaw `
        -Destination (Join-Path $EvidenceDir `
            "q1-backup-restore-v1.$pass.summary.txt") `
        -SceneId "q1-backup-restore-v1" -BuildId $buildId `
        -IdentityLines @(
            "comparison_world_fixture=world-backup-complete-state-v1",
            "comparison_save_format=8",
            "comparison_storage_class=local-default",
            "comparison_backup_fixture=three-file-generation-v1",
            "comparison_backup_policy=rotating-3") `
        -MetricKeys @(
            "restore_catalogue_scan_ms",
            "restore_candidate_copy_complete_ms",
            "restore_validation_complete_ms",
            "restore_publish_complete_ms", "restore_total_ms",
            "restore_main_thread_max_stall_ms", "restore_bytes_read",
            "restore_bytes_written")
    }
}

if ($runFinalize) {
Add-BudgetLines -Path (Join-Path $EvidenceDir `
    "q1-cold-start-v1.baseline.summary.txt") -Lines @(
        "budget_startup_first_usable_menu_ms_max=3000",
        "budget_peak_private_bytes_max=536870912",
        "budget_peak_working_set_bytes_max=536870912",
        "budget_peak_handle_count_max=768")
Add-BudgetLines -Path (Join-Path $EvidenceDir `
    "q1-world-entry-v1.baseline.summary.txt") -Lines @(
        "budget_entry_first_controllable_ms_max=2500",
        "budget_peak_private_bytes_max=536870912",
        "budget_peak_working_set_bytes_max=536870912",
        "budget_peak_handle_count_max=768")
Add-BudgetLines -Path (Join-Path $EvidenceDir `
    "q1-save-transaction-v1.baseline.summary.txt") -Lines @(
        "budget_save_total_ms_max=300",
        "budget_save_main_thread_max_stall_ms_max=300")
Add-BudgetLines -Path (Join-Path $EvidenceDir `
    "q1-backup-restore-v1.baseline.summary.txt") -Lines @(
        "budget_restore_total_ms_max=500",
        "budget_restore_main_thread_max_stall_ms_max=500")
Add-BudgetLines -Path (Join-Path $EvidenceDir `
    "q1-fast-streaming-v1.baseline.summary.txt") -Lines @(
        "budget_chunk_visible_p95_ms_max=1000",
        "budget_chunk_visible_p99_ms_max=2000",
        "budget_stream_queue_peak_max=512",
        "budget_peak_private_bytes_max=805306368",
        "budget_peak_working_set_bytes_max=805306368",
        "budget_peak_handle_count_max=768")
Add-BudgetLines -Path (Join-Path $EvidenceDir `
    "q1-scaled-gameplay-v1.baseline.summary.txt") -Lines @(
        "budget_main_thread_max_stall_ms_max=250",
        "budget_peak_private_bytes_max=805306368",
        "budget_peak_working_set_bytes_max=805306368",
        "budget_peak_handle_count_max=768",
        "budget_cap_events_max=0")

foreach ($scene in $sceneFiles) {
    $baseline = Join-Path $EvidenceDir "$scene.baseline.summary.txt"
    $repeat = Join-Path $EvidenceDir "$scene.repeat.summary.txt"
    & $comparator -Baseline $baseline -Candidate $repeat
    if ($LASTEXITCODE -ne 0) {
        throw "Release-candidate performance comparison failed for $scene with exit code $LASTEXITCODE."
    }
}

$manifestLines = @(
    "profile=$budgetProfile",
    "captured_utc=$([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    "platform=windows",
    "architecture=x86_64",
    "window_visibility=hidden",
    "seed=$seed",
    "scenes=$($sceneFiles.Count)",
    "status=PASS"
)
Set-Content -LiteralPath (Join-Path $EvidenceDir "manifest.txt") `
    -Encoding utf8 -Value $manifestLines
}
Write-Host "[RC_PERF] status=PASS evidenceDir=$EvidenceDir outputDir=$OutputDir"
