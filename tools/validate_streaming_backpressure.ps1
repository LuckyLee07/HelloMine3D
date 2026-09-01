[CmdletBinding()]
param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot),
    [switch]$Implementation
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Require-Text {
    param(
        [Parameter(Mandatory = $true)] [string]$Text,
        [Parameter(Mandatory = $true)] [string]$Needle,
        [Parameter(Mandatory = $true)] [string]$Label
    )
    if ($Text.IndexOf($Needle, [StringComparison]::Ordinal) -lt 0) {
        throw "Missing $Label ('$Needle')."
    }
}

function Reject-Text {
    param(
        [Parameter(Mandatory = $true)] [string]$Text,
        [Parameter(Mandatory = $true)] [string]$Needle,
        [Parameter(Mandatory = $true)] [string]$Label
    )
    if ($Text.IndexOf($Needle, [StringComparison]::Ordinal) -ge 0) {
        throw "Unexpected $Label ('$Needle')."
    }
}

try {
    $sourceRoot = Join-Path $Root "src\HelloMine3D"
    $schedulerHeaderPath = Join-Path $sourceRoot `
        "World\Streaming\WorldJobScheduler.h"
    $schedulerSourcePath = Join-Path $sourceRoot `
        "World\Streaming\WorldJobScheduler.cpp"
    $runtimeHeaderPath = Join-Path $sourceRoot `
        "World\Chunk\ChunkRuntime.h"
    $runtimeSourcePath = Join-Path $sourceRoot `
        "World\Chunk\ChunkRuntime.cpp"
    $worldHeaderPath = Join-Path $sourceRoot "World\World.h"
    $worldSourcePath = Join-Path $sourceRoot "World\World.cpp"
    $uiPath = Join-Path $sourceRoot "Ogre\OgreUserInterface.cpp"
    $testPath = Join-Path $sourceRoot `
        "Tests\WorldRuntimeSmokeMain.cpp"
    $contractPath = Join-Path $Root `
        "docs\contracts\streaming-backpressure-contract-v1.md"

    $paths = @(
        $schedulerHeaderPath, $schedulerSourcePath, $runtimeHeaderPath,
        $runtimeSourcePath, $worldHeaderPath, $worldSourcePath,
        $uiPath, $testPath, $contractPath)
    foreach ($path in $paths) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required B5 artifact is missing: $path"
        }
    }

    $schedulerHeader = Get-Content -LiteralPath $schedulerHeaderPath -Raw
    $schedulerSource = Get-Content -LiteralPath $schedulerSourcePath -Raw
    $runtimeHeader = Get-Content -LiteralPath $runtimeHeaderPath -Raw
    $runtimeSource = Get-Content -LiteralPath $runtimeSourcePath -Raw
    $worldHeader = Get-Content -LiteralPath $worldHeaderPath -Raw
    $worldSource = Get-Content -LiteralPath $worldSourcePath -Raw
    $ui = Get-Content -LiteralPath $uiPath -Raw
    $tests = Get-Content -LiteralPath $testPath -Raw
    $contract = Get-Content -LiteralPath $contractPath -Raw

    foreach ($token in @(
        "enum class WorldJobAdmissionResult", "Accepted",
        "AcceptedAfterShedding", "Duplicate", "StaleGeneration",
        "RejectedAtCapacity", "enum class WorldJobPressureLevel",
        "Normal", "Elevated", "Saturated",
        "MaxPendingJobs = 128",
        "MaxPendingGenerationJobs = 128",
        "MaxPendingMeshJobs = 128",
        "PendingHighWatermark = 96",
        "PendingLowWatermark = 48")) {
        Require-Text $schedulerHeader $token `
            "frozen B5 admission/pressure vocabulary"
    }

    foreach ($token in @(
        "WorldJobScheduler::admit(",
        "WorldJobScheduler::admitLocked(",
        "WorldJobAdmissionResult::RejectedAtCapacity",
        "WorldJobAdmissionResult::AcceptedAfterShedding",
        "jobPrecedes(job, *worst)", "m_pending.erase(worst)",
        "refreshPressureLocked()",
        "m_pending.size() >= PendingHighWatermark",
        "m_pending.size() <= PendingLowWatermark",
        "m_pending.size() >= MaxPendingJobs")) {
        Require-Text $schedulerSource $token `
            "bounded deterministic admission implementation"
    }

    foreach ($token in @(
        "pendingGenerationJobs", "pendingMeshJobs",
        "peakPendingJobs", "maxPendingJobs", "pressureLevel",
        "acceptedAdmissions", "acceptedAfterSheddingJobs",
        "duplicateAdmissionRejections",
        "capacityAdmissionRejections", "shedPendingJobs",
        "pressureTransitions", "saturationEpisodes")) {
        Require-Text $schedulerHeader $token "copied B5 scheduler metric"
    }

    foreach ($token in @(
        "struct ChunkBackpressureDebugStats",
        "MaxAuthoritativeCommitsPerPass = 8",
        "MaxSectionUploadsPerFrame = 8",
        "MaxUnloadsPerUpdate = 8",
        "planSectionMeshUploads(", "cpuReadyTotal",
        "cpuReadyDeferred", "m_deferredPlanJobCount")) {
        Require-Text $runtimeHeader $token `
            "bounded B5 consumer/diagnostic boundary"
    }
    foreach ($token in @(
        "std::vector<WorldJobRequest> activePlan",
        "std::size_t nextPlanIndex = 0",
        "activePlan.size() - nextPlanIndex",
        "WorldJobScheduler::PendingHighWatermark",
        "WorldJobScheduler::PendingLowWatermark",
        "MaxAuthoritativeCommitsPerPass",
        "MaxSectionUploadsPerFrame",
        "chunksToUnload.size() > MaxUnloadsPerUpdate",
        "m_unloadBacklog = chunksToUnload.size() > MaxUnloadsPerUpdate")) {
        Require-Text $runtimeSource $token `
            "B5 window/refill and consumer implementation"
    }

    $workerStarts = [regex]::Matches(
        $runtimeSource,
        [regex]::Escape("m_chunkLoadThreads.emplace_back(")).Count
    if ($workerStarts -ne 1) {
        throw "B5 must retain exactly one loader worker declaration."
    }

    foreach ($token in @(
        "ChunkBackpressureDebugStats streamingBackpressure",
        "collectBackpressureDebugStats()")) {
        Require-Text ($worldHeader + $worldSource) $token `
            "World-owned copied B5 diagnostics"
    }
    foreach ($token in @(
        "Jobs pending load/mesh/deferred-plan",
        "Backpressure %s pending/high/low/cap",
        "Job admission accepted/shed/duplicate/cap",
        "Job shedding total/load/mesh transitions/saturation",
        "Streaming commits/uploads/unloads")) {
        Require-Text $ui $token "developer B5 diagnostics"
    }

    foreach ($testId in @(
        "B5/backpressure-vocabulary-and-limits-are-frozen",
        "B5/below-watermark-admission-is-normal",
        "B5/duplicate-and-stale-admission-stay-distinct",
        "B5/hard-cap-rejects-lower-priority-work",
        "B5/higher-priority-work-sheds-deterministic-worst",
        "B5/high-low-watermark-hysteresis-is-exact",
        "B5/invalidation-clears-pressure-and-preserves-inflight",
        "B5/windowed-plan-refills-without-loss",
        "B5/live-plan-is-larger-than-bounded-job-window",
        "B5/loader-pass-commit-budget-is-bounded",
        "B5/upload-selection-is-deterministic-and-bounded",
        "B5/unload-budget-and-backlog-are-truthful")) {
        Require-Text $tests $testId "B5 behavioral/integration check"
    }

    $scopedSources = $schedulerHeader + $schedulerSource +
        $runtimeHeader + $runtimeSource + $worldHeader + $worldSource
    foreach ($forbidden in @(
        "SpatialInterest", "requiresResidentData", "FarLOD",
        "FarTerrain", "ActiveMachineNetwork", "SimulationInterest")) {
        Reject-Text $scopedSources $forbidden `
            "unapproved post-B5 capability"
    }

    if ($Implementation) {
        Require-Text $contract "Status: Frozen for B5 implementation" `
            "B5 contract status"
    }
    else {
        $gateReady = $contract.IndexOf(
            "Status: Frozen for B5 full-gate verification",
            [StringComparison]::Ordinal) -ge 0
        $verified = $contract.IndexOf(
            "Status: Frozen after B5 verification",
            [StringComparison]::Ordinal) -ge 0
        if (-not ($gateReady -or $verified)) {
            throw "B5 contract is not ready for the full gate."
        }
    }
    Require-Text $contract "save v12, terrain v4, settings v8" `
        "compatibility boundary"

    Write-Host (
        "[STREAMING_BACKPRESSURE] status=PASS caps=128/128/128 " +
        "watermarks=96/48 commits=8 uploads=8 unloads=8 " +
        "workers=$workerStarts post_b5=absent")
    exit 0
}
catch {
    Write-Error (
        "[STREAMING_BACKPRESSURE] status=FAIL " +
        $_.Exception.Message)
    exit 1
}
