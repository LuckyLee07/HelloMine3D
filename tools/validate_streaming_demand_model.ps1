[CmdletBinding()]
param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot)
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
    $demandHeaderPath = Join-Path $sourceRoot `
        "World\Chunk\ChunkDemand.h"
    $demandSourcePath = Join-Path $sourceRoot `
        "World\Chunk\ChunkDemand.cpp"
    $runtimeHeaderPath = Join-Path $sourceRoot `
        "World\Chunk\ChunkRuntime.h"
    $runtimeSourcePath = Join-Path $sourceRoot `
        "World\Chunk\ChunkRuntime.cpp"
    $worldHeaderPath = Join-Path $sourceRoot "World\World.h"
    $worldSourcePath = Join-Path $sourceRoot "World\World.cpp"
    $managerPath = Join-Path $sourceRoot "Sandbox\WorldManager.cpp"
    $uiPath = Join-Path $sourceRoot "Ogre\OgreUserInterface.cpp"
    $testPath = Join-Path $sourceRoot `
        "Tests\WorldRuntimeSmokeMain.cpp"
    $contractPath = Join-Path $Root `
        "docs\contracts\streaming-demand-model-contract-v1.md"
    $b4ContractPath = Join-Path $Root `
        "docs\contracts\world-job-cancellation-contract-v1.md"

    $paths = @(
        $demandHeaderPath, $demandSourcePath, $runtimeHeaderPath,
        $runtimeSourcePath, $worldHeaderPath, $worldSourcePath,
        $managerPath, $uiPath, $testPath, $contractPath,
        $b4ContractPath)
    foreach ($path in $paths) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required B2 artifact is missing: $path"
        }
    }

    $demandHeader = Get-Content -LiteralPath $demandHeaderPath -Raw
    $demandSource = Get-Content -LiteralPath $demandSourcePath -Raw
    $runtimeHeader = Get-Content -LiteralPath $runtimeHeaderPath -Raw
    $runtimeSource = Get-Content -LiteralPath $runtimeSourcePath -Raw
    $worldHeader = Get-Content -LiteralPath $worldHeaderPath -Raw
    $worldSource = Get-Content -LiteralPath $worldSourcePath -Raw
    $manager = Get-Content -LiteralPath $managerPath -Raw
    $ui = Get-Content -LiteralPath $uiPath -Raw
    $tests = Get-Content -LiteralPath $testPath -Raw
    $contract = Get-Content -LiteralPath $contractPath -Raw
    $b4Contract = Get-Content -LiteralPath $b4ContractPath -Raw

    foreach ($token in @(
        "enum class ChunkDemandReason", "Player", "Camera",
        "TeleportDestination", "Preload",
        "ChunkDemandReasonCount = 4", "struct ChunkDemand",
        "VectorXZ coord", "ChunkDemandReason reason", "int priority",
        "std::uint64_t epoch", "std::uint64_t expiresAfterEpoch",
        "int radius", "std::array<std::optional<ChunkDemand>",
        "ChunkDemandReasonCount> m_slots")) {
        Require-Text $demandHeader $token "frozen B2 demand vocabulary"
    }

    foreach ($token in @(
        "PlayerPriority = 300", "CameraPriority = 200",
        "TeleportPriority = 400", "PreloadPriority = 100",
        "PlayerLifetimeEpochs = 2", "CameraLifetimeEpochs = 2",
        "TeleportLifetimeEpochs = 120", "PreloadLifetimeEpochs = 30")) {
        Require-Text $demandHeader $token "frozen B2 policy value"
    }

    foreach ($token in @(
        "void ChunkDemandModel::advanceEpoch()",
        "void ChunkDemandModel::refresh(",
        "m_epoch <= slot->expiresAfterEpoch",
        "!slot.has_value() || !(slot->coord == coord)",
        "if (semanticChange)", "++m_revision",
        "ChunkDemandModel::reasonBit(")) {
        Require-Text $demandSource $token "bounded refresh/expiry semantics"
    }

    foreach ($token in @(
        "ChunkDemandModel m_demandModel", "m_demandMutex",
        "planDemandWork(", "reasonMask |=", "target.priority =",
        "target.inFrustum", "target.motionRank",
        "left.newestEpoch", "left.distanceSquared",
        "m_lastPlannedTargetCount", "m_demandModel.advanceEpoch()",
        "ChunkDemandReason::Player", "ChunkDemandReason::Camera")) {
        Require-Text ($runtimeHeader + $runtimeSource) $token `
            "ChunkRuntime demand ownership/planning"
    }

    Require-Text $worldSource `
        "m_player != nullptr ? m_player->position : camera.position" `
        "Player demand publication"
    Require-Text $worldSource `
        "ChunkDemandReason::TeleportDestination" `
        "private teleport demand bridge"
    Require-Text $worldHeader "friend class WorldManager" `
        "private WorldManager bridge"
    Require-Text $manager "world->preloadAroundForTeleport(position)" `
        "successful teleport demand source"

    foreach ($token in @(
        "Demand epoch/revision/active/planned",
        "Demand P/C/T/Pre expired", "streamingDemand.playerDemands",
        "streamingDemand.teleportDemands",
        "streamingDemand.lastPlannedTargets")) {
        Require-Text ($ui + $worldHeader + $worldSource) $token `
            "developer demand diagnostics"
    }

    foreach ($testId in @(
        "B2/reason-vocabulary-and-policy",
        "B2/refresh-is-stable-and-bounded",
        "B2/overlap-merges-reason-bits",
        "B2/reason-replacement-removes-old-center",
        "B2/teleport-outprioritises-player",
        "B2/player-motion-prioritises-forward",
        "B2/camera-turn-reorders-pending-plan",
        "B2/transient-demand-expires-exactly",
        "B2/world-publishes-player-camera-preload",
        "B2/preload-replaces-single-slot",
        "B2/successful-teleport-elevates-destination",
        "B2/rejected-teleport-does-not-mutate-demand")) {
        Require-Text $tests $testId "B2 behavioral check"
    }

    foreach ($speculative in @(
        "ActiveMachineNetwork", "DebugCamera", "ChunkDemandReason::Actor")) {
        Reject-Text ($demandHeader + $demandSource + $runtimeHeader +
                     $runtimeSource) $speculative `
            "unimplemented demand reason"
    }
    foreach ($forbidden in @(
        "CancellationToken", "Backpressure",
        "SpatialInterest", "FarLOD", "FarTerrain")) {
        Reject-Text ($demandHeader + $demandSource + $runtimeHeader +
                     $runtimeSource + $worldHeader + $worldSource) `
            $forbidden "unapproved post-B2 capability"
    }

    Require-Text $contract "Status: Frozen after B2 verification" `
        "frozen B2 contract"
    Require-Text $contract "save v12" "save compatibility boundary"
    Require-Text $runtimeSource "WorldJobGenerationToken planGeneration" `
        "B4 generation attached to a copied B2 plan"
    Require-Text $b4Contract `
        "Camera/frustum priority-only reordering does not invalidate" `
        "B2 priority-only compatibility boundary"

    Write-Host (
        "[STREAMING_DEMAND_MODEL] status=PASS reasons=4 slots=4 " +
        "priorities=400/300/200/100 expiry=epoch merge=deduplicated " +
        "post_b2=B3-B4")
    exit 0
}
catch {
    Write-Error (
        "[STREAMING_DEMAND_MODEL] status=FAIL " +
        $_.Exception.Message)
    exit 1
}
