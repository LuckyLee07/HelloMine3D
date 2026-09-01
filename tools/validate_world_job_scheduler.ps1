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
    $worldHeaderPath = Join-Path $sourceRoot "World\World.h"
    $worldSourcePath = Join-Path $sourceRoot "World\World.cpp"
    $uiPath = Join-Path $sourceRoot "Ogre\OgreUserInterface.cpp"
    $testPath = Join-Path $sourceRoot `
        "Tests\WorldRuntimeSmokeMain.cpp"
    $contractPath = Join-Path $Root `
        "docs\contracts\world-job-scheduler-contract-v1.md"
    $b4ContractPath = Join-Path $Root `
        "docs\contracts\world-job-cancellation-contract-v1.md"
    $b5ContractPath = Join-Path $Root `
        "docs\contracts\streaming-backpressure-contract-v1.md"

    $paths = @(
        $schedulerHeaderPath, $schedulerSourcePath, $runtimeHeaderPath,
        $runtimeSourcePath, $managerHeaderPath, $managerSourcePath,
        $worldHeaderPath, $worldSourcePath, $uiPath, $testPath,
        $contractPath, $b4ContractPath, $b5ContractPath)
    foreach ($path in $paths) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required B3 artifact is missing: $path"
        }
    }

    $schedulerHeader = Get-Content -LiteralPath $schedulerHeaderPath -Raw
    $schedulerSource = Get-Content -LiteralPath $schedulerSourcePath -Raw
    $runtimeHeader = Get-Content -LiteralPath $runtimeHeaderPath -Raw
    $runtimeSource = Get-Content -LiteralPath $runtimeSourcePath -Raw
    $managerHeader = Get-Content -LiteralPath $managerHeaderPath -Raw
    $managerSource = Get-Content -LiteralPath $managerSourcePath -Raw
    $worldHeader = Get-Content -LiteralPath $worldHeaderPath -Raw
    $worldSource = Get-Content -LiteralPath $worldSourcePath -Raw
    $ui = Get-Content -LiteralPath $uiPath -Raw
    $tests = Get-Content -LiteralPath $testPath -Raw
    $contract = Get-Content -LiteralPath $contractPath -Raw
    $b4Contract = Get-Content -LiteralPath $b4ContractPath -Raw
    $b5Contract = Get-Content -LiteralPath $b5ContractPath -Raw

    foreach ($token in @(
        "enum class WorldJobType", "ChunkLoadOrGenerate",
        "ChunkMeshBuild", "enum class WorldJobState", "Pending",
        "InFlight", "Completed", "enum class WorldJobOutcome",
        "DidWork", "NoWork", "CommitRejected", "Cancelled",
        "std::uint64_t id", "std::uint64_t demandEpoch",
        "std::size_t planOrder", "enqueuedAt",
        "WorldJobSchedulerDebugStats")) {
        Require-Text $schedulerHeader $token "frozen B3 job vocabulary"
    }

    foreach ($token in @(
        "bool WorldJobScheduler::submit(",
        "bool WorldJobScheduler::replacePending(",
        "bool WorldJobScheduler::takeNext(",
        "bool WorldJobScheduler::complete(",
        "bool WorldJobScheduler::popCompleted(",
        "std::optional<WorldJob> m_inFlight",
        "std::deque<WorldJobCompletion> m_completed",
        "left.priority > right.priority",
        "left.planOrder < right.planOrder",
        "left.demandEpoch > right.demandEpoch",
        "return left.id < right.id")) {
        Require-Text ($schedulerHeader + $schedulerSource) $token `
            "deterministic scheduler lifecycle"
    }

    foreach ($token in @(
        "WorldJobScheduler m_jobScheduler",
        "m_jobScheduler.replacePending(",
        "m_jobScheduler.takeNext(scheduledJob)",
        "m_jobScheduler.complete(",
        "m_jobScheduler.popCompleted(completion)",
        "WorldJobType::ChunkLoadOrGenerate",
        "WorldJobType::ChunkMeshBuild",
        "beginChunkNeighborhoodLoadJob(",
        "prepareChunkLoadJob(",
        "finishChunkLoadJob(",
        "finishMeshJob(")) {
        Require-Text ($runtimeHeader + $runtimeSource + $managerHeader +
                      $managerSource) $token "real Chunk pipeline integration"
    }

    $workerStarts = [regex]::Matches(
        $runtimeSource,
        [regex]::Escape("m_chunkLoadThreads.emplace_back(")).Count
    if ($workerStarts -ne 1) {
        throw "B3 must retain exactly one loader worker declaration."
    }
    foreach ($budget in @(
        "std::chrono::milliseconds(6)", "MaxTargetsPerPass = 64",
        "ChunkLoadsPerTarget = 1", "MaxUnloadsPerUpdate = 8")) {
        Require-Text ($runtimeHeader + $runtimeSource + $managerSource) $budget `
            "frozen B3 runtime budget"
    }

    foreach ($token in @(
        "WorldJobSchedulerDebugStats worldJobs",
        "stats.worldJobs = m_chunkRuntime.collectJobSchedulerDebugStats()",
        "Jobs pending/in-flight/results",
        "Jobs submitted/started/completed",
        "Jobs load/mesh/work/none/reject/cancel",
        "Job ms queue/worker/commit")) {
        Require-Text ($worldHeader + $worldSource + $ui) $token `
            "developer scheduler diagnostics"
    }

    foreach ($testId in @(
        "B3/job-vocabulary-is-frozen",
        "B3/duplicate-pending-key-is-rejected",
        "B3/priority-plan-epoch-type-order-is-deterministic",
        "B3/pending-to-inflight-and-invalid-completion",
        "B3/replace-pending-preserves-inflight",
        "B3/completion-records-state-outcome-and-timing",
        "B3/debug-metrics-match-lifecycle",
        "B3/runtime-executes-both-real-job-types",
        "B3/runtime-scheduler-counters-are-consistent",
        "E5/stale-upload-not-acknowledged")) {
        Require-Text $tests $testId "B3 behavioral/integration check"
    }

    $scopedSources = $schedulerHeader + $schedulerSource +
        $runtimeHeader + $runtimeSource + $managerHeader +
        $managerSource + $worldHeader + $worldSource
    foreach ($forbidden in @(
        "CancellationToken",
        "FarLOD", "FarTerrain", "ActiveMachineNetwork")) {
        Reject-Text $scopedSources $forbidden `
            "unapproved post-B3 capability"
    }

    $expectedStatus = if ($Implementation) {
        "Status: Frozen for B3 implementation"
    }
    else {
        "Status: Frozen after B3 verification"
    }
    Require-Text $contract $expectedStatus "B3 contract status"
    Require-Text $contract "save/resource/settings/Gameplay/public-World-API" `
        "compatibility boundary"
    foreach ($token in @(
        "struct WorldJobGenerationToken", "WorldJobOutcome::Cancelled",
        "currentGenerationToken()", "invalidateGeneration()")) {
        Require-Text ($schedulerHeader + $schedulerSource) $token `
            "approved B4 extension"
    }
    $b4Implementation = $b4Contract.IndexOf(
        "Status: Frozen for B4 implementation",
        [StringComparison]::Ordinal) -ge 0
    $b4Verified = $b4Contract.IndexOf(
        "Status: Frozen after B4 verification",
        [StringComparison]::Ordinal) -ge 0
    $b4GateReady = $b4Contract.IndexOf(
        "Status: Frozen for B4 full-gate verification",
        [StringComparison]::Ordinal) -ge 0
    if (-not ($b4Implementation -or $b4GateReady -or $b4Verified)) {
        throw "B4 contract has no recognized frozen status."
    }
    foreach ($token in @(
        "enum class WorldJobAdmissionResult",
        "enum class WorldJobPressureLevel",
        "WorldJobScheduler::admit(",
        "MaxPendingJobs = 128",
        "PendingHighWatermark = 96",
        "PendingLowWatermark = 48")) {
        Require-Text ($schedulerHeader + $schedulerSource) $token `
            "approved B5 scheduler extension"
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
        "[WORLD_JOB_SCHEDULER] status=PASS types=2 states=3 " +
        "outcomes=4 workers=1 order=priority/plan/epoch/type/id " +
        "post_b3=B4-B6")
    exit 0
}
catch {
    Write-Error (
        "[WORLD_JOB_SCHEDULER] status=FAIL " +
        $_.Exception.Message)
    exit 1
}
