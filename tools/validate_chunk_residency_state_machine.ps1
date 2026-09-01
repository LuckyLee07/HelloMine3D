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
    $lifecycleHeaderPath = Join-Path $sourceRoot `
        "World\Chunk\ChunkLifecycle.h"
    $lifecycleSourcePath = Join-Path $sourceRoot `
        "World\Chunk\ChunkLifecycle.cpp"
    $chunkHeaderPath = Join-Path $sourceRoot "World\Chunk\Chunk.h"
    $chunkSourcePath = Join-Path $sourceRoot "World\Chunk\Chunk.cpp"
    $sectionHeaderPath = Join-Path $sourceRoot `
        "World\Chunk\ChunkSection.h"
    $sectionSourcePath = Join-Path $sourceRoot `
        "World\Chunk\ChunkSection.cpp"
    $managerSourcePath = Join-Path $sourceRoot `
        "World\Chunk\ChunkManager.cpp"
    $runtimeSourcePath = Join-Path $sourceRoot `
        "World\Chunk\ChunkRuntime.cpp"
    $runtimeHeaderPath = Join-Path $sourceRoot `
        "World\Chunk\ChunkRuntime.h"
    $ogrePath = Join-Path $sourceRoot "Ogre\OgreBootstrap.cpp"
    $uiPath = Join-Path $sourceRoot "Ogre\OgreUserInterface.cpp"
    $testPath = Join-Path $sourceRoot `
        "Tests\WorldRuntimeSmokeMain.cpp"
    $contractPath = Join-Path $Root `
        "docs\contracts\chunk-residency-state-machine-contract-v1.md"
    $b4ContractPath = Join-Path $Root `
        "docs\contracts\world-job-cancellation-contract-v1.md"
    $b5ContractPath = Join-Path $Root `
        "docs\contracts\streaming-backpressure-contract-v1.md"

    $paths = @(
        $lifecycleHeaderPath, $lifecycleSourcePath, $chunkHeaderPath,
        $chunkSourcePath, $sectionHeaderPath, $sectionSourcePath,
        $managerSourcePath, $runtimeHeaderPath, $runtimeSourcePath,
        $ogrePath, $uiPath,
        $testPath, $contractPath, $b4ContractPath, $b5ContractPath)
    foreach ($path in $paths) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required B1 artifact is missing: $path"
        }
    }

    $lifecycleHeader = Get-Content -LiteralPath $lifecycleHeaderPath -Raw
    $lifecycleSource = Get-Content -LiteralPath $lifecycleSourcePath -Raw
    $chunkHeader = Get-Content -LiteralPath $chunkHeaderPath -Raw
    $chunkSource = Get-Content -LiteralPath $chunkSourcePath -Raw
    $sectionHeader = Get-Content -LiteralPath $sectionHeaderPath -Raw
    $sectionSource = Get-Content -LiteralPath $sectionSourcePath -Raw
    $managerSource = Get-Content -LiteralPath $managerSourcePath -Raw
    $runtimeHeader = Get-Content -LiteralPath $runtimeHeaderPath -Raw
    $runtimeSource = Get-Content -LiteralPath $runtimeSourcePath -Raw
    $ogre = Get-Content -LiteralPath $ogrePath -Raw
    $ui = Get-Content -LiteralPath $uiPath -Raw
    $tests = Get-Content -LiteralPath $testPath -Raw
    $contract = Get-Content -LiteralPath $contractPath -Raw
    $b4Contract = Get-Content -LiteralPath $b4ContractPath -Raw
    $b5Contract = Get-Content -LiteralPath $b5ContractPath -Raw

    foreach ($token in @(
        "enum class ChunkDataResidencyState", "Absent", "Requested",
        "Loading", "Generating", "Resident", "EvictRequested", "Saving",
        "ChunkDataResidencyStateCount = 7",
        "enum class ChunkMeshState", "Clean", "Dirty", "Queued",
        "Building", "CpuReady", "ChunkMeshStateCount = 5",
        "enum class ChunkRenderState", "NotResident", "UploadPending",
        "GpuResident", "Stale", "ChunkRenderStateCount = 4",
        "canTransition(ChunkDataResidencyState",
        "canTransition(ChunkMeshState",
        "canTransition(ChunkRenderState",
        "chunkDataResidencyStateName", "chunkMeshStateName",
        "chunkRenderStateName")) {
        Require-Text ($lifecycleHeader + $lifecycleSource) $token `
            "frozen B1 lifecycle vocabulary"
    }

    Require-Text $chunkHeader `
        "ChunkDataResidencyState m_dataResidencyState" `
        "Chunk-owned data state"
    Require-Text $sectionHeader "ChunkMeshState m_meshState" `
        "ChunkSection-owned CPU mesh state"
    Require-Text $ogre `
        "std::unordered_map<std::string, ChunkRenderState>" `
        "Ogre-owned render state"
    Require-Text $chunkSource `
        'assert(legal && "illegal Chunk data-residency transition")' `
        "data transition assertion"
    Require-Text $sectionSource `
        'assert(legal && "illegal ChunkSection mesh transition")' `
        "mesh transition assertion"
    Require-Text $ogre `
        'assert(legal && "illegal Ogre render-state transition")' `
        "render transition assertion"

    foreach ($token in @(
        "ChunkDataResidencyState::Requested",
        "ChunkDataResidencyState::Loading",
        "ChunkDataResidencyState::EvictRequested",
        "ChunkDataResidencyState::Saving",
        "chunk.transitionDataResidency(returnState)",
        "chunk->transitionDataResidency(ChunkDataResidencyState::Resident)")) {
        Require-Text $managerSource $token "data lifecycle orchestration"
    }
    foreach ($token in @(
        "markMeshQueued()", "beginMeshBuild()",
        "ChunkMeshState::Building", "ChunkMeshState::CpuReady",
        "markMeshClean()")) {
        Require-Text ($managerSource + $runtimeSource + $sectionSource) `
            $token "mesh lifecycle orchestration"
    }
    foreach ($token in @(
        "m_sectionRenderStates", "transitionRenderState(",
        "ChunkRenderState::UploadPending",
        "ChunkRenderState::GpuResident", "ChunkRenderState::Stale",
        "ChunkRenderState::NotResident")) {
        Require-Text $ogre $token "render lifecycle orchestration"
    }

    foreach ($token in @(
        "Data A/Rq/L/G/R/E/S", "Mesh C/D/Q/B/CPU",
        "Render N/U/G/S", "dataResidentChunks", "meshQueuedSections",
        "meshBuildingSections", "renderUploadPendingSections",
        "gpuResidentSections", "renderStaleSections")) {
        Require-Text ($ui + $ogre + $chunkHeader) $token `
            "developer state diagnostics"
    }

    foreach ($testId in @(
        "B1/data-state-vocabulary-and-graph",
        "B1/mesh-state-vocabulary-and-graph",
        "B1/render-state-vocabulary-and-graph",
        "B1/generated-chunk-is-resident",
        "B1/data-states-are-separately-counted",
        "B1/mesh-dirty-to-clean-flow",
        "B1/dirty-eviction-reaches-absent-after-save",
        "B1/reload-restores-resident-data",
        "B1/failed-save-cancels-eviction",
        "E5/stale-upload-not-acknowledged")) {
        Require-Text $tests $testId "B1 behavioral check"
    }

    Reject-Text ($chunkHeader + $sectionHeader + $managerSource +
                 $runtimeSource) "ChunkLoadState" "legacy data enum"
    Reject-Text ($sectionHeader + $sectionSource + $managerSource +
                 $runtimeSource) "ChunkSectionMeshState" `
        "legacy combined CPU/GPU mesh enum"
    foreach ($forbidden in @(
        "CancellationToken", "SpatialInterest")) {
        Reject-Text ($lifecycleHeader + $lifecycleSource + $chunkHeader +
                     $chunkSource + $sectionHeader + $sectionSource +
                     $managerSource + $runtimeSource) $forbidden `
            "unapproved post-B1 capability"
    }

    Require-Text $contract "Status: Frozen after B1 verification" `
        "frozen B1 contract"
    Require-Text $contract "Save format v12" "save compatibility boundary"
    Require-Text $lifecycleSource `
        "to == ChunkDataResidencyState::Absent;" `
        "B4 Loading-to-Absent cancellation edge"
    Require-Text $tests "dataEdges == 13" `
        "B1 graph extended by exactly one B4 edge"
    Require-Text $b4Contract `
        "only B4 extension to the B1 Data Residency graph." `
        "B4 residency scope boundary"
    foreach ($token in @(
        "struct ChunkBackpressureDebugStats",
        "MaxUnloadsPerUpdate = 8",
        "chunksToUnload.size() > MaxUnloadsPerUpdate")) {
        Require-Text ($runtimeHeader + $runtimeSource + $b5Contract) $token `
            "approved B5 bounded-consumer extension"
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
        "[CHUNK_RESIDENCY_STATE_MACHINE] status=PASS " +
        "data_states=7 mesh_states=5 render_states=4 " +
        "owners=3 debug_families=3 post_b1=B2-B5")
    exit 0
}
catch {
    Write-Error (
        "[CHUNK_RESIDENCY_STATE_MACHINE] status=FAIL " +
        $_.Exception.Message)
    exit 1
}
