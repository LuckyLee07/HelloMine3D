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
    $interestHeaderPath = Join-Path $sourceRoot `
        "World\Streaming\SpatialInterest.h"
    $interestSourcePath = Join-Path $sourceRoot `
        "World\Streaming\SpatialInterest.cpp"
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
        "docs\contracts\spatial-activation-contract-v1.md"

    $paths = @(
        $interestHeaderPath, $interestSourcePath, $runtimeHeaderPath,
        $runtimeSourcePath, $worldHeaderPath, $worldSourcePath,
        $uiPath, $testPath, $contractPath)
    foreach ($path in $paths) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required B6 artifact is missing: $path"
        }
    }

    $interestHeader = Get-Content -LiteralPath $interestHeaderPath -Raw
    $interestSource = Get-Content -LiteralPath $interestSourcePath -Raw
    $runtimeHeader = Get-Content -LiteralPath $runtimeHeaderPath -Raw
    $runtimeSource = Get-Content -LiteralPath $runtimeSourcePath -Raw
    $worldHeader = Get-Content -LiteralPath $worldHeaderPath -Raw
    $worldSource = Get-Content -LiteralPath $worldSourcePath -Raw
    $ui = Get-Content -LiteralPath $uiPath -Raw
    $tests = Get-Content -LiteralPath $testPath -Raw
    $contract = Get-Content -LiteralPath $contractPath -Raw

    foreach ($token in @(
        "enum class SpatialInterestClass", "Outside", "ResidentData",
        "NearRepresentation", "SimulationRequested",
        "SpatialInterestClassCount = 4",
        "bool requiresResidentData", "bool requiresNearRepresentation",
        "bool requestsSimulation", "std::uint32_t reasonMask",
        "SimulationRequestRadiusChunks = 2",
        "SpatialInterestSnapshot", "SpatialInterestDebugStats")) {
        Require-Text $interestHeader $token `
            "frozen B6 interest vocabulary"
    }

    foreach ($token in @(
        "ChunkDemandReason::Player ||",
        "ChunkDemandReason::Camera",
        "demand.reason == ChunkDemandReason::Player &&",
        "distance <= SimulationRequestRadiusChunks",
        "interest.reasonMask |=",
        "std::sort(result.cells.begin(), result.cells.end()",
        "std::lower_bound(")) {
        Require-Text $interestSource $token `
            "deterministic B6 source policy"
    }

    foreach ($token in @(
        "SpatialInterestSnapshot m_spatialInterestSnapshot",
        "refreshSpatialInterestLocked()",
        "shouldPublishMeshFollowUp(",
        "if (!interest.requiresResidentData)",
        "if (!interest.requiresNearRepresentation)",
        "spatialInterest = m_spatialInterestSnapshot",
        "SpatialInterestModel::interestAt(",
        "MaxAuthoritativeCommitsPerPass = 8",
        "MaxSectionUploadsPerFrame = 8",
        "MaxUnloadsPerUpdate = 8")) {
        Require-Text ($runtimeHeader + $runtimeSource) $token `
            "real B6 consumer and unchanged B5 bound"
    }

    $workerStarts = [regex]::Matches(
        $runtimeSource,
        [regex]::Escape("m_chunkLoadThreads.emplace_back(")).Count
    if ($workerStarts -ne 1) {
        throw "B6 must retain exactly one loader worker declaration."
    }

    foreach ($token in @(
        "SpatialInterestDebugStats spatialInterest",
        "collectSpatialInterestDebugStats()")) {
        Require-Text ($worldHeader + $worldSource) $token `
            "World-owned copied B6 diagnostics"
    }
    Require-Text $ui "Spatial interest R/N/S:" `
        "developer B6 diagnostics"

    foreach ($testId in @(
        "B6/vocabulary-and-hierarchy-are-frozen",
        "B6/absent-coordinate-is-outside",
        "B6/player-produces-nested-simulation-and-near-rings",
        "B6/camera-produces-near-without-simulation",
        "B6/teleport-and-preload-are-resident-only",
        "B6/overlaps-merge-flags-and-reasons",
        "B6/order-and-source-revision-are-stable",
        "B6/radius-change-removes-obsolete-interest",
        "B6/resident-only-stops-before-mesh-follow-up",
        "B6/mesh-snapshot-excludes-resident-only-data",
        "B6/resident-interest-protects-then-bounded-unload-resumes",
        "B6/simulation-interest-is-publication-only")) {
        Require-Text $tests $testId "B6 behavioral/integration check"
    }

    $scopedSources = $interestHeader + $interestSource +
        $runtimeHeader + $runtimeSource + $worldHeader + $worldSource
    foreach ($forbidden in @(
        "requiresFarRepresentation", "FarLOD", "FarTerrain",
        "ActiveMachineNetwork", "SimulationFidelity",
        "ReducedSimulation", "DormantSimulation")) {
        Reject-Text $scopedSources $forbidden `
            "unapproved post-B6 capability"
    }

    if ($Implementation) {
        Require-Text $contract "Status: Frozen for B6 implementation" `
            "B6 contract status"
    }
    else {
        $gateReady = $contract.IndexOf(
            "Status: Frozen for B6 full-gate verification",
            [StringComparison]::Ordinal) -ge 0
        $verified = $contract.IndexOf(
            "Status: Frozen after B6 verification",
            [StringComparison]::Ordinal) -ge 0
        if (-not ($gateReady -or $verified)) {
            throw "B6 contract is not ready for the full gate."
        }
    }
    Require-Text $contract "save v12, terrain v4, settings v8" `
        "compatibility boundary"

    Write-Host (
        "[SPATIAL_ACTIVATION] status=PASS classes=4 simulation_radius=2 " +
        "consumers=plan/mesh/upload/unload workers=$workerStarts " +
        "post_b6=absent")
    exit 0
}
catch {
    Write-Error (
        "[SPATIAL_ACTIVATION] status=FAIL " +
        $_.Exception.Message)
    exit 1
}
