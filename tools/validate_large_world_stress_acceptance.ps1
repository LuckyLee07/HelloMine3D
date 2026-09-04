[CmdletBinding()]
param(
    [string]$Root = "",
    [switch]$Implementation,
    [switch]$Evidence
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Require-Text {
    param([string]$Text, [string]$Needle, [string]$Label)
    if ($Text.IndexOf($Needle, [StringComparison]::Ordinal) -lt 0) {
        throw "Missing $Label ('$Needle')."
    }
}

function Read-Summary {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing B10 evidence: $Path"
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
    param([hashtable]$Summary, [string]$Key, [string]$Expected)
    if (-not $Summary.ContainsKey($Key) -or
        [string]$Summary[$Key] -cne $Expected) {
        throw "B10 evidence '$Key' must be '$Expected'."
    }
}

try {
    $soakPath = Join-Path $Root `
        "src\HelloMine3D\Tests\SoakMain.cpp"
    $worldRunnerPath = Join-Path $Root "tools\run_world_soak.ps1"
    $acceptanceRunnerPath = Join-Path $Root `
        "tools\run_large_world_stress_acceptance.ps1"
    $performanceRunnerPath = Join-Path $Root `
        "tools\run_perf_baseline.ps1"
    $bootstrapPath = Join-Path $Root `
        "src\HelloMine3D\Ogre\OgreBootstrap.cpp"
    $chunkManagerPath = Join-Path $Root `
        "src\HelloMine3D\World\Chunk\ChunkManager.cpp"
    $chunkRuntimePath = Join-Path $Root `
        "src\HelloMine3D\World\Chunk\ChunkRuntime.cpp"
    $chunkSectionPath = Join-Path $Root `
        "src\HelloMine3D\World\Chunk\ChunkSection.cpp"
    $meshInputPath = Join-Path $Root `
        "src\HelloMine3D\World\Chunk\SectionMeshInput.cpp"
    $worldSmokePath = Join-Path $Root `
        "src\HelloMine3D\Tests\WorldRuntimeSmokeMain.cpp"
    $contractPath = Join-Path $Root `
        "docs\contracts\large-world-stress-acceptance-contract-v1.md"
    foreach ($path in @(
            $soakPath, $worldRunnerPath, $acceptanceRunnerPath,
            $performanceRunnerPath, $bootstrapPath, $chunkManagerPath,
            $chunkRuntimePath, $chunkSectionPath, $meshInputPath,
            $worldSmokePath, $contractPath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required B10 artifact is missing: $path"
        }
    }

    $soak = Get-Content -LiteralPath $soakPath -Raw
    $worldRunner = Get-Content -LiteralPath $worldRunnerPath -Raw
    $acceptanceRunner = Get-Content -LiteralPath $acceptanceRunnerPath -Raw
    $performanceRunner = Get-Content -LiteralPath $performanceRunnerPath -Raw
    $bootstrap = Get-Content -LiteralPath $bootstrapPath -Raw
    $chunkManager = Get-Content -LiteralPath $chunkManagerPath -Raw
    $chunkRuntime = Get-Content -LiteralPath $chunkRuntimePath -Raw
    $chunkSection = Get-Content -LiteralPath $chunkSectionPath -Raw
    $meshInput = Get-Content -LiteralPath $meshInputPath -Raw
    $worldSmoke = Get-Content -LiteralPath $worldSmokePath -Raw
    $contract = Get-Content -LiteralPath $contractPath -Raw

    foreach ($token in @(
            "TrackBCore", "track-b-core", "? 3 : 2",
            "TrackBPhase::StraightRun", "TrackBPhase::TeleportStorm",
            "TrackBPhase::Turnaround",
            "TrackBPhase::RenderDistanceChurn",
            "TrackBPhase::EditAndLeave",
            "track_b_schedule_digest", "track_b_persistence_digest",
            "WorldJobScheduler::MaxPendingJobs",
            "ChunkRuntime::MaxAuthoritativeCommitsPerPass",
            "ChunkRuntime::MaxSectionUploadsPerFrame",
            "ChunkRuntime::MaxUnloadsPerUpdate",
            "dataAbsentChunks != 0",
            "trackBReloadInterval",
            "track_b_max_absent_chunks",
            "simulationRequestedCells >",
            "nearRepresentationCells >")) {
        Require-Text $soak $token "B10 real-world schedule/invariant"
    }
    foreach ($phase in @(
            "lw1_straight_run", "lw2_teleport_storm",
            "lw3_turnaround", "lw4_render_distance_churn",
            "lw5_edit_and_leave")) {
        Require-Text ($soak + $worldRunner + $acceptanceRunner) $phase `
            "B10 phase identity"
    }
    foreach ($token in @(
            '[ValidateSet("legacy", "nominal", "stress", "track-b-core")]',
            'track_b_core_bound_exceeded',
            'Formal B10 Core acceptance is frozen at exactly 1800 seconds.',
            'lw1_straight_run = 12000',
            'lw2_teleport_storm = 6000',
            'track_b_schedule_digest',
            'async_metrics_compared=false',
            'real_window=DEFERRED',
            'ai_acceptance=NOT_RUN',
            'human_subjective=NOT_CLAIMED')) {
        Require-Text ($worldRunner + $acceptanceRunner) $token `
            "B10 hidden runner and truthful evidence"
    }
    Require-Text $performanceRunner `
        'comparison_movement_path=rc-ring-12-chunks-v2' `
        "B10 production-teleport Q1 identity"
    Require-Text $bootstrap `
        'getWorldManager().teleportPlayer(' `
        "B10 production-teleport Q1 boundary"
    Require-Text $chunkManager `
        'm_chunks.erase(reserved);' `
        "B10 cancelled-reservation tombstone removal"
    Require-Text $chunkRuntime `
        'ChunkDataResidencyState::Resident' `
        "B10 unload eligibility before budget selection"
    Require-Text $chunkRuntime `
        'unloaded < chunksToUnload.size()' `
        "B10 truthful successful-unload backlog"
    Require-Text $worldSmoke `
        'B10/ineligible-reservations-cannot-starve-unload-budget' `
        "B10 mixed-state unload regression"
    Require-Text $worldSmoke `
        'std::string(focus) == "B10"' `
        "B10 focused runtime entry"
    Require-Text $chunkSection `
        'findAdjacent(int dx, int dz) const' `
        "B10 read-only mesh-neighbour boundary"
    Require-Text $meshInput `
        'adjacent != nullptr && adjacent->getLayer(y).isAllSolid()' `
        "B10 absent-neighbour fallback"
    Require-Text $worldSmoke `
        'B10/mesh-neighbour-read-does-not-create-absent-chunks' `
        "B10 mesh-neighbour non-creation regression"

    if ($Implementation) {
        $implementationFrozen = $contract.IndexOf(
            "Status: Frozen for B10 implementation",
            [StringComparison]::Ordinal) -ge 0
        $gateReady = $contract.IndexOf(
            "Status: Frozen for B10 full-gate verification",
            [StringComparison]::Ordinal) -ge 0
        $verified = $contract.IndexOf(
            "Status: Frozen after B10 verification",
            [StringComparison]::Ordinal) -ge 0
        if (-not ($implementationFrozen -or $gateReady -or $verified)) {
            throw "B10 implementation contract is not frozen."
        }
    }
    else {
        $gateReady = $contract.IndexOf(
            "Status: Frozen for B10 full-gate verification",
            [StringComparison]::Ordinal) -ge 0
        $verified = $contract.IndexOf(
            "Status: Frozen after B10 verification",
            [StringComparison]::Ordinal) -ge 0
        if (-not ($gateReady -or $verified)) {
            throw "B10 contract is not ready for the full gate."
        }
    }
    Require-Text $contract "save v12" "B10 compatibility boundary"
    Require-Text $contract "B7-B9" "B10 Extended exclusion"
    Require-Text $contract "rc-ring-12-chunks-v2" `
        "B10 production-teleport Q1 fixture"
    Require-Text $contract "ten LW5 persistence checks" `
        "B10 bounded formal persistence coverage"

    if ($Evidence) {
        $evidenceRoot = Join-Path $Root `
            "docs\baselines\architecture-lab-b10-windows-hidden-v1"
        $world = Read-Summary (Join-Path $evidenceRoot `
            "b10-core-world.summary.txt")
        $process = Read-Summary (Join-Path $evidenceRoot `
            "b10-core-process.summary.txt")
        $determinism = Read-Summary (Join-Path $evidenceRoot `
            "b10-determinism.summary.txt")
        $manifest = Read-Summary (Join-Path $evidenceRoot `
            "b10-manifest.txt")
        $firstFailure = Read-Summary (Join-Path $evidenceRoot `
            "b10-first-failure.summary.txt")
        foreach ($summary in @($world, $process, $determinism, $manifest)) {
            Require-Value $summary "status" "PASS"
        }
        Require-Value $world "schedule_version" "3"
        Require-Value $world "profile" "track-b-core"
        Require-Value $world "build_configuration" "Release"
        Require-Value $world "duration_completed_seconds" "1800"
        Require-Value $world "fixed_ticks" "36000"
        Require-Value $world "track_b_max_absent_chunks" "0"
        Require-Value $world "final_data_absent_chunks" "0"
        Require-Value $process "formal" "true"
        Require-Value $process "child_timed_out" "false"
        Require-Value $determinism "async_metrics_compared" "false"
        Require-Value $manifest "formal" "true"
        Require-Value $manifest "real_window" "DEFERRED"
        Require-Value $manifest "ai_acceptance" "NOT_RUN"
        Require-Value $manifest "human_subjective" "NOT_CLAIMED"
        Require-Value $firstFailure "status" "FAIL"
        Require-Value $firstFailure "threshold_relaxed" "false"
        Require-Value $firstFailure "performance_exception" "false"
    }

    Write-Host (
        "[LARGE_WORLD_STRESS] status=PASS schedule=3 phases=5 " +
        "duration=1800 pending=128 consumers=8/8/8 " +
        "evidence=$($Evidence.IsPresent.ToString().ToLowerInvariant())")
    exit 0
}
catch {
    Write-Error (
        "[LARGE_WORLD_STRESS] status=FAIL " + $_.Exception.Message)
    exit 1
}
