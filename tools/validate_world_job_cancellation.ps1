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
    $managerHeaderPath = Join-Path $sourceRoot `
        "World\Chunk\ChunkManager.h"
    $managerSourcePath = Join-Path $sourceRoot `
        "World\Chunk\ChunkManager.cpp"
    $lifecycleSourcePath = Join-Path $sourceRoot `
        "World\Chunk\ChunkLifecycle.cpp"
    $chunkSourcePath = Join-Path $sourceRoot "World\Chunk\Chunk.cpp"
    $storageSourcePath = Join-Path $sourceRoot `
        "World\Storage\ChunkStorage.cpp"
    $uiPath = Join-Path $sourceRoot "Ogre\OgreUserInterface.cpp"
    $testPath = Join-Path $sourceRoot `
        "Tests\WorldRuntimeSmokeMain.cpp"
    $contractPath = Join-Path $Root `
        "docs\contracts\world-job-cancellation-contract-v1.md"
    $b5ContractPath = Join-Path $Root `
        "docs\contracts\streaming-backpressure-contract-v1.md"

    $paths = @(
        $schedulerHeaderPath, $schedulerSourcePath, $runtimeHeaderPath,
        $runtimeSourcePath, $managerHeaderPath, $managerSourcePath,
        $lifecycleSourcePath, $chunkSourcePath, $storageSourcePath,
        $uiPath, $testPath, $contractPath, $b5ContractPath)
    foreach ($path in $paths) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required B4 artifact is missing: $path"
        }
    }

    $schedulerHeader = Get-Content -LiteralPath $schedulerHeaderPath -Raw
    $schedulerSource = Get-Content -LiteralPath $schedulerSourcePath -Raw
    $runtimeHeader = Get-Content -LiteralPath $runtimeHeaderPath -Raw
    $runtimeSource = Get-Content -LiteralPath $runtimeSourcePath -Raw
    $managerHeader = Get-Content -LiteralPath $managerHeaderPath -Raw
    $managerSource = Get-Content -LiteralPath $managerSourcePath -Raw
    $lifecycleSource = Get-Content -LiteralPath $lifecycleSourcePath -Raw
    $chunkSource = Get-Content -LiteralPath $chunkSourcePath -Raw
    $storageSource = Get-Content -LiteralPath $storageSourcePath -Raw
    $ui = Get-Content -LiteralPath $uiPath -Raw
    $tests = Get-Content -LiteralPath $testPath -Raw
    $contract = Get-Content -LiteralPath $contractPath -Raw
    $b5Contract = Get-Content -LiteralPath $b5ContractPath -Raw

    foreach ($token in @(
        "struct WorldJobGenerationToken",
        "std::uint64_t value = 0",
        "WorldJobGenerationToken generation",
        "WorldJobOutcome::Cancelled",
        "std::atomic<std::uint64_t> m_generation{1}",
        "currentGeneration", "generationInvalidations",
        "cancelledPendingJobs", "staleSubmitRejections",
        "stalePlanRejections", "cancelledJobs")) {
        Require-Text ($schedulerHeader + $schedulerSource) $token `
            "B4 token/cancellation vocabulary"
    }

    foreach ($token in @(
        "WorldJobScheduler::currentGenerationToken()",
        "WorldJobScheduler::invalidateGeneration()",
        "WorldJobScheduler::isCurrent(",
        "if (!isCurrent(request.generation))",
        "if (!isCurrent(generation))",
        "m_totals.cancelledPendingJobs += m_pending.size()",
        "m_pending.clear()",
        "++m_totals.cancelledJobs")) {
        Require-Text $schedulerSource $token `
            "B4 scheduler cancellation lifecycle"
    }

    foreach ($token in @(
        "std::mutex m_worldJobCommitMutex",
        "void ChunkRuntime::invalidateWorldJobs()",
        "m_jobScheduler.invalidateGeneration()",
        "WorldJobGenerationToken planGeneration",
        "m_jobScheduler.replacePending(",
        "m_jobScheduler.isCurrent(",
        "scheduledJob.generation",
        "WorldJobOutcome::Cancelled")) {
        Require-Text ($runtimeHeader + $runtimeSource) $token `
            "B4 runtime generation/commit boundary"
    }

    $invalidationCalls = [regex]::Matches(
        $runtimeSource,
        [regex]::Escape("invalidateWorldJobs();")).Count
    if ($invalidationCalls -lt 5) {
        throw "B4 must invalidate at all frozen semantic boundaries."
    }

    foreach ($token in @(
        "struct ChunkLoadJob", "std::unique_ptr<Chunk> candidate",
        "beginChunkNeighborhoodLoadJob(", "prepareChunkLoadJob(",
        "finishChunkLoadJob(", "cancelChunkLoadJob(",
        "cancelMeshJob(", "m_terrainGeneratorMutex",
        "enableWorldIndexUpdates()")) {
        Require-Text ($managerHeader + $managerSource) $token `
            "detached Chunk/mesh cancellation protocol"
    }
    foreach ($token in @(
        "updateWorldIndex", "m_worldIndexUpdatesEnabled",
        "assert(updateWorldIndex || !m_worldIndexUpdatesEnabled)",
        "section.setWorldIndexUpdatesEnabled(true)")) {
        Require-Text ($chunkSource + $storageSource) $token `
            "detached storage/random-tick isolation boundary"
    }
    Require-Text $lifecycleSource `
        "to == ChunkDataResidencyState::Absent;" `
        "single B4 residency rollback edge"

    foreach ($token in @(
        "Jobs load/mesh/work/none/reject/cancel",
        "Job generation/current invalidations",
        "Job cancellation pending/submit/plan")) {
        Require-Text $ui $token "developer cancellation diagnostics"
    }

    foreach ($testId in @(
        "B4/generation-token-and-cancel-vocabulary-is-frozen",
        "B4/invalidation-clears-pending-and-preserves-inflight",
        "B4/stale-submit-and-plan-are-rejected",
        "B4/current-generation-keeps-b3-ordering",
        "B4/cancelled-completion-and-metrics-are-exact",
        "B4/cancelled-detached-load-publishes-nothing",
        "B4/current-detached-load-commits-once",
        "B4/cancelled-mesh-returns-dirty-without-adoption",
        "B4/rapid-replan-stress-is-bounded-and-deadlock-free",
        "B4/live-world-advances-generation-and-runs-current-work")) {
        Require-Text $tests $testId "B4 behavioral/integration check"
    }

    $scopedSources = $schedulerHeader + $schedulerSource +
        $runtimeHeader + $runtimeSource + $managerHeader + $managerSource
    foreach ($forbidden in @(
        "FarLOD", "FarTerrain",
        "ActiveMachineNetwork")) {
        Reject-Text $scopedSources $forbidden `
            "unapproved post-B4 capability"
    }

    if ($Implementation) {
        Require-Text $contract "Status: Frozen for B4 implementation" `
            "B4 contract status"
    }
    else {
        $gateReady = $contract.IndexOf(
            "Status: Frozen for B4 full-gate verification",
            [StringComparison]::Ordinal) -ge 0
        $verified = $contract.IndexOf(
            "Status: Frozen after B4 verification",
            [StringComparison]::Ordinal) -ge 0
        if (-not ($gateReady -or $verified)) {
            throw "B4 contract is not ready for the full gate."
        }
    }
    Require-Text $contract "save v12, terrain v4, settings v8" `
        "compatibility boundary"
    foreach ($token in @(
        "WorldJobAdmissionResult",
        "WorldJobPressureLevel",
        "MaxPendingJobs = 128",
        "PendingHighWatermark = 96",
        "PendingLowWatermark = 48")) {
        Require-Text ($schedulerHeader + $schedulerSource) $token `
            "approved B5 admission/pressure extension"
    }
    $b5Implementation = $b5Contract.IndexOf(
        "Status: Frozen for B5 implementation",
        [StringComparison]::Ordinal) -ge 0
    $b5GateReady = $b5Contract.IndexOf(
        "Status: Frozen for B5 full-gate verification",
        [StringComparison]::Ordinal) -ge 0
    $b5Verified = $b5Contract.IndexOf(
        "Status: Frozen after B5 verification",
        [StringComparison]::Ordinal) -ge 0
    if (-not ($b5Implementation -or $b5GateReady -or $b5Verified)) {
        throw "B5 contract has no recognized frozen status."
    }

    Write-Host (
        "[WORLD_JOB_CANCELLATION] status=PASS token=uint64 " +
        "outcome=Cancelled load=detached commit=linearized " +
        "invalidation_boundaries=$invalidationCalls post_b4=B5-B6")
    exit 0
}
catch {
    Write-Error (
        "[WORLD_JOB_CANCELLATION] status=FAIL " +
        $_.Exception.Message)
    exit 1
}
