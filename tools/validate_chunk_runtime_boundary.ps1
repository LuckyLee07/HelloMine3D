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
    $worldHeaderPath = Join-Path $Root "src\HelloMine3D\World\World.h"
    $worldSourcePath = Join-Path $Root "src\HelloMine3D\World\World.cpp"
    $runtimeHeaderPath = Join-Path $Root `
        "src\HelloMine3D\World\Chunk\ChunkRuntime.h"
    $runtimeSourcePath = Join-Path $Root `
        "src\HelloMine3D\World\Chunk\ChunkRuntime.cpp"

    foreach ($path in @($worldHeaderPath, $worldSourcePath,
                         $runtimeHeaderPath, $runtimeSourcePath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required AL-A2 source is missing: $path"
        }
    }

    $worldHeader = Get-Content -LiteralPath $worldHeaderPath -Raw
    $worldSource = Get-Content -LiteralPath $worldSourcePath -Raw
    $runtimeHeader = Get-Content -LiteralPath $runtimeHeaderPath -Raw
    $runtimeSource = Get-Content -LiteralPath $runtimeSourcePath -Raw

    Require-Text $worldHeader "ChunkRuntime m_chunkRuntime;" `
        "World-owned ChunkRuntime"
    Require-Text $worldSource `
        "return ChunkRuntime::planMeshWork(center, radius, sectionY, frustum);" `
        "World mesh-planner delegation"
    Require-Text $worldSource "m_chunkRuntime.processChunkUpdates(" `
        "bounded World update delegation"

    foreach ($legacy in @(
        "m_chunkUpdateQueue", "m_queuedChunkUpdates", "m_chunkLoadThreads",
        "m_meshPrioritySnapshot", "m_loadCenterX", "m_renderDistance",
        "void loadChunks()", "void updateChunks()",
        "void preloadChunksAround(")) {
        Reject-Text $worldHeader $legacy "legacy World Chunk runtime ownership"
    }
    foreach ($legacyDefinition in @(
        "void World::loadChunks(", "void World::updateChunks(",
        "void World::setChunkLoadCenter(",
        "void World::publishMeshPrioritySnapshot(",
        "void World::queueSectionUpdate(",
        "void World::preloadChunksAround(")) {
        Reject-Text $worldSource $legacyDefinition `
            "legacy World Chunk runtime implementation"
    }

    foreach ($owned in @(
        "m_chunkUpdateQueue", "m_queuedChunkUpdates", "m_chunkLoadThreads",
        "m_meshPrioritySnapshot", "m_demandModel", "m_jobScheduler",
        "m_renderDistance")) {
        Require-Text $runtimeHeader $owned "ChunkRuntime-owned coordination"
    }
    foreach ($budget in @(
        "std::chrono::milliseconds(6)", "MaxTargetsPerPass = 64",
        "ChunkLoadsPerTarget = 1", "ActiveSleepMs = 1",
        "IdleSleepMs = 10", "MaxUnloadsPerUpdate = 8")) {
        Require-Text ($runtimeHeader + $runtimeSource) $budget `
            "frozen AL-A2 budget"
    }

    $loaderStart = $runtimeSource.IndexOf(
        "void ChunkRuntime::runLoader()", [StringComparison]::Ordinal)
    if ($loaderStart -lt 0) {
        throw "ChunkRuntime loader implementation is missing."
    }
    $loader = $runtimeSource.Substring($loaderStart)
    $begin = $loader.IndexOf("beginMeshJob(", [StringComparison]::Ordinal)
    $build = $loader.IndexOf(
        "ChunkMeshBuilder(meshJob.input, builtMeshes)",
        [StringComparison]::Ordinal)
    $finish = $loader.IndexOf("finishMeshJob(", [StringComparison]::Ordinal)
    if ($begin -lt 0 -or $build -le $begin -or $finish -le $build) {
        throw "Snapshot -> off-lock build -> revision commit order drifted."
    }

    Reject-Text ($worldHeader + $worldSource) "m_dataResidencyState" `
        "Chunk data-residency ownership leaked into World"
    Reject-Text ($worldHeader + $worldSource) "m_meshState" `
        "Chunk mesh-state ownership leaked into World"

    Write-Host (
        "[CHUNK_RUNTIME_BOUNDARY] status=PASS " +
        "world_queue_fields=0 runtime_queue_fields=2 loader_workers=1 " +
        "streaming_input=demand-model jobs=typed-scheduler " +
        "mesh_protocol=begin-build-finish")
    exit 0
}
catch {
    Write-Error "[CHUNK_RUNTIME_BOUNDARY] status=FAIL $($_.Exception.Message)"
    exit 1
}
