[CmdletBinding()]
param(
    [string]$OutputDir = "",
    [string]$EvidenceDir = "",
    [int]$DurationSeconds = 1800,
    [int]$ProbeDurationSeconds = 10,
    [int]$Seed = 20260902,
    [switch]$Formal
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
$runner = Join-Path $scriptRoot "run_world_soak.ps1"

if ($DurationSeconds -lt 5 -or $DurationSeconds -gt 86400) {
    throw "DurationSeconds must be between 5 and 86400."
}
if ($ProbeDurationSeconds -lt 5 -or $ProbeDurationSeconds -gt 300) {
    throw "ProbeDurationSeconds must be between 5 and 300."
}
if ($Formal -and $DurationSeconds -ne 1800) {
    throw "Formal B10 Core acceptance is frozen at exactly 1800 seconds."
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $runId = "{0:yyyyMMddHHmmssfff}-{1}" -f (Get-Date), $PID
    $OutputDir = Join-Path $repoRoot "bin\soak_runs\b10-$runId"
}
elseif (-not [IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDir))
}
if (Test-Path -LiteralPath $OutputDir) {
    throw "B10 output directory already exists; use a fresh path: $OutputDir"
}
if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
    $EvidenceDir = if ($Formal) {
        Join-Path $repoRoot `
            "docs\baselines\architecture-lab-b10-windows-hidden-v1"
    }
    else {
        Join-Path $OutputDir "evidence"
    }
}
elseif (-not [IO.Path]::IsPathRooted($EvidenceDir)) {
    $EvidenceDir = [IO.Path]::GetFullPath((Join-Path $repoRoot $EvidenceDir))
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
New-Item -ItemType Directory -Path $EvidenceDir -Force | Out-Null

function Read-Summary {
    param([Parameter(Mandatory = $true)] [string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing B10 summary: $Path"
    }
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
    param(
        [Parameter(Mandatory = $true)] [hashtable]$Summary,
        [Parameter(Mandatory = $true)] [string]$Key,
        [Parameter(Mandatory = $true)] [string]$Label
    )
    if (-not $Summary.ContainsKey($Key) -or
        [string]::IsNullOrWhiteSpace([string]$Summary[$Key])) {
        throw "$Label is missing '$Key'."
    }
    return [string]$Summary[$Key]
}

function Invoke-B10Run {
    param(
        [Parameter(Mandatory = $true)] [string]$Name,
        [Parameter(Mandatory = $true)] [int]$Seconds,
        [switch]$IsFormal
    )
    $runDir = Join-Path $OutputDir $Name
    Write-Host (
        "[B10_ACCEPTANCE] phase=$Name durationSeconds=$Seconds " +
        "formal=$($IsFormal.IsPresent.ToString().ToLowerInvariant())")
    $arguments = @{
        OutputDir = $runDir
        DurationSeconds = $Seconds
        Seed = $Seed
        SampleIntervalSeconds = if ($IsFormal) { 5 } else { 1 }
        Profile = "track-b-core"
    }
    if ($IsFormal) {
        & $runner @arguments -Formal
    }
    else {
        & $runner @arguments
    }
    return $runDir
}

$coreDir = Invoke-B10Run -Name "core" -Seconds $DurationSeconds `
    -IsFormal:$Formal
$probeADir = Invoke-B10Run -Name "determinism-a" `
    -Seconds $ProbeDurationSeconds
$probeBDir = Invoke-B10Run -Name "determinism-b" `
    -Seconds $ProbeDurationSeconds

$coreWorldPath = Join-Path $coreDir "summary.txt"
$coreProcessPath = Join-Path $coreDir "process-summary.txt"
$probeAPath = Join-Path $probeADir "summary.txt"
$probeBPath = Join-Path $probeBDir "summary.txt"
$core = Read-Summary -Path $coreWorldPath
$coreProcess = Read-Summary -Path $coreProcessPath
$probeA = Read-Summary -Path $probeAPath
$probeB = Read-Summary -Path $probeBPath

foreach ($entry in @(
        @{ Summary = $core; Label = "core world" },
        @{ Summary = $coreProcess; Label = "core process" },
        @{ Summary = $probeA; Label = "probe A" },
        @{ Summary = $probeB; Label = "probe B" })) {
    if ((Require-Value -Summary $entry.Summary -Key "status" `
            -Label $entry.Label) -ne "PASS") {
        throw "$($entry.Label) did not pass."
    }
}
if ((Require-Value $core "schedule_version" "core world") -ne "3" -or
    (Require-Value $core "track_b_core" "core world") -ne "true") {
    throw "Core run did not use frozen Track B schedule v3."
}
if ($Formal) {
    if ((Require-Value $core "build_configuration" "core world") -ne
        "Release") {
        throw "Formal B10 must use the Release soak executable."
    }
    if ([int](Require-Value $core "fixed_ticks" "core world") -ne 36000) {
        throw "Formal B10 did not complete exactly 36000 fixed ticks."
    }
    $expectedPhaseTicks = @{
        lw1_straight_run = 12000
        lw2_teleport_storm = 6000
        lw3_turnaround = 6000
        lw4_render_distance_churn = 6000
        lw5_edit_and_leave = 6000
    }
    foreach ($phase in $expectedPhaseTicks.Keys) {
        if ([int](Require-Value $core "${phase}_ticks" "core world") -ne
            $expectedPhaseTicks[$phase]) {
            throw "Formal B10 phase duration mismatch: $phase"
        }
    }
}

$deterministicKeys = @(
    "schedule_version", "profile", "seed", "duration_requested_seconds",
    "duration_completed_seconds", "fixed_ticks", "movement_actions",
    "block_edit_actions", "actor_lifecycle_actions", "save_reload_actions",
    "track_b_core", "track_b_schedule_digest",
    "track_b_persistence_digest", "track_b_render_distance_changes",
    "track_b_persistence_checks",
    "lw1_straight_run_ticks", "lw1_straight_run_movements",
    "lw2_teleport_storm_ticks", "lw2_teleport_storm_movements",
    "lw3_turnaround_ticks", "lw3_turnaround_movements",
    "lw4_render_distance_churn_ticks",
    "lw4_render_distance_churn_movements",
    "lw5_edit_and_leave_ticks", "lw5_edit_and_leave_movements")
$differences = @()
foreach ($key in $deterministicKeys) {
    $first = Require-Value $probeA $key "probe A"
    $second = Require-Value $probeB $key "probe B"
    if ($first -cne $second) {
        $differences += "$key=$first/$second"
    }
}
if ($differences.Count -ne 0) {
    throw "B10 deterministic probes differ: $($differences -join '; ')"
}

$determinismLines = @(
    "status=PASS",
    "schema_version=1",
    "seed=$Seed",
    "probe_duration_seconds=$ProbeDurationSeconds",
    "compared_keys=$($deterministicKeys.Count)",
    "schedule_digest=$($probeA['track_b_schedule_digest'])",
    "persistence_digest=$($probeA['track_b_persistence_digest'])",
    "async_metrics_compared=false",
    "reason=wall_time_memory_and_queue_depth_are_os_schedule_dependent")
$determinismPath = Join-Path $EvidenceDir `
    "b10-determinism.summary.txt"
Set-Content -LiteralPath $determinismPath -Encoding utf8 `
    -Value $determinismLines
Copy-Item -LiteralPath $coreWorldPath -Destination (Join-Path $EvidenceDir `
    "b10-core-world.summary.txt") -Force
Copy-Item -LiteralPath $coreProcessPath -Destination (Join-Path $EvidenceDir `
    "b10-core-process.summary.txt") -Force

$manifestLines = @(
    "contract=large-world-stress-acceptance-contract-v1",
    "captured_utc=$([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    "formal=$($Formal.IsPresent.ToString().ToLowerInvariant())",
    "build_configuration=$($core['build_configuration'])",
    "seed=$Seed",
    "duration_seconds=$DurationSeconds",
    "probe_duration_seconds=$ProbeDurationSeconds",
    "schedule_version=3",
    "phases=LW1,LW2,LW3,LW4,LW5",
    "real_window=DEFERRED",
    "ai_acceptance=NOT_RUN",
    "human_subjective=NOT_CLAIMED",
    "status=PASS")
Set-Content -LiteralPath (Join-Path $EvidenceDir "b10-manifest.txt") `
    -Encoding utf8 -Value $manifestLines

Write-Host (
    "[B10_ACCEPTANCE] status=PASS formal=" +
    "$($Formal.IsPresent.ToString().ToLowerInvariant()) " +
    "scheduleDigest=$($probeA['track_b_schedule_digest']) " +
    "persistenceDigest=$($probeA['track_b_persistence_digest']) " +
    "evidenceDir=$EvidenceDir")
