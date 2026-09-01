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

function Require-OrderedText {
    param(
        [Parameter(Mandatory = $true)] [string]$Text,
        [Parameter(Mandatory = $true)] [string[]]$Needles,
        [Parameter(Mandatory = $true)] [string]$Label
    )
    $cursor = 0
    foreach ($needle in $Needles) {
        $index = $Text.IndexOf($needle, $cursor,
                              [StringComparison]::Ordinal)
        if ($index -lt 0) {
            throw "Missing or reordered $Label ('$needle')."
        }
        $cursor = $index + $needle.Length
    }
}

try {
    $worldHeaderPath = Join-Path $Root "src\HelloMine3D\World\World.h"
    $worldSourcePath = Join-Path $Root "src\HelloMine3D\World\World.cpp"
    $simulationHeaderPath = Join-Path $Root `
        "src\HelloMine3D\World\Simulation\WorldSimulation.h"
    $simulationSourcePath = Join-Path $Root `
        "src\HelloMine3D\World\Simulation\WorldSimulation.cpp"
    $ogreSourcePath = Join-Path $Root `
        "src\HelloMine3D\Ogre\OgreBootstrap.cpp"
    $testSourcePath = Join-Path $Root `
        "src\HelloMine3D\Tests\WorldRuntimeSmokeMain.cpp"

    foreach ($path in @($worldHeaderPath, $worldSourcePath,
                         $simulationHeaderPath, $simulationSourcePath,
                         $ogreSourcePath, $testSourcePath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required AL-A3 source is missing: $path"
        }
    }

    $worldHeader = Get-Content -LiteralPath $worldHeaderPath -Raw
    $worldSource = Get-Content -LiteralPath $worldSourcePath -Raw
    $simulationHeader = Get-Content -LiteralPath $simulationHeaderPath -Raw
    $simulationSource = Get-Content -LiteralPath $simulationSourcePath -Raw
    $ogreSource = Get-Content -LiteralPath $ogreSourcePath -Raw
    $testSource = Get-Content -LiteralPath $testSourcePath -Raw

    Require-Text $worldHeader "WorldSimulation m_worldSimulation;" `
        "World-owned simulation runtime"
    Require-Text $worldHeader "WorldSimulationSnapshot simulation;" `
        "existing debug-query timing snapshot"

    $tickStart = $worldSource.IndexOf(
        "void World::tick(int worldTime)", [StringComparison]::Ordinal)
    $tickEnd = $worldSource.IndexOf(
        "bool World::attackActor(", $tickStart,
        [StringComparison]::Ordinal)
    if ($tickStart -lt 0 -or $tickEnd -le $tickStart) {
        throw "World tick implementation cannot be isolated."
    }
    $worldTick = $worldSource.Substring($tickStart,
                                        $tickEnd - $tickStart)
    Require-Text $worldTick "WorldTickContext context;" `
        "World tick context"
    Require-Text $worldTick "m_worldSimulation.fixedTick(context);" `
        "single simulation delegation"
    foreach ($legacyCall in @(
        "m_actorManager.tick(", "tickCombatProjectiles(",
        "reconcileWaystoneEncounter(", "runRandomTicks(",
        "runNaturalMobPopulation(", "FurnaceContainer::tickLoaded(")) {
        Reject-Text $worldTick $legacyCall `
            "legacy World tick orchestration"
    }

    Require-OrderedText $simulationHeader @(
        "TickPreparation", "ActorSimulation", "Combat", "Encounter",
        "BlockRandomTick", "Population", "BlockEntitySimulation",
        "GameplayRuntime", "Count") "simulation phase identity"

    Require-OrderedText $simulationSource @(
        "m_world.applyPendingDifficulty();",
        "m_world.m_playerActor.tick(",
        "m_world.m_actorManager.tick(",
        "m_world.tickCombatProjectiles();",
        "m_world.reconcileWaystoneEncounter();",
        "m_world.runRandomTicks(context.tick);",
        "m_world.runNaturalMobPopulation(context.tick);",
        "FurnaceContainer::tickLoaded(",
        "m_world.m_alphaJourney->update(",
        "m_world.respawnPlayer();") "fixed-tick phase call"

    Require-Text $simulationSource "RawPhaseTimer" "raw phase timer"
    Require-Text $simulationSource "std::chrono::steady_clock" `
        "monotonic timing source"
    Require-Text $simulationSource "m_snapshot = next;" `
        "completed last-tick publication"

    $updateStart = $ogreSource.IndexOf(
        "void updateSandbox(float deltaSeconds)",
        [StringComparison]::Ordinal)
    $pauseGate = $ogreSource.IndexOf(
        "if (!m_applicationFlow.acceptsWorldSimulation())", $updateStart,
        [StringComparison]::Ordinal)
    $sandboxUpdate = $ogreSource.IndexOf(
        "m_sandbox->update(", $updateStart,
        [StringComparison]::Ordinal)
    if ($updateStart -lt 0 -or $pauseGate -le $updateStart -or
        $sandboxUpdate -le $pauseGate) {
        throw "Caller-owned pause gate no longer precedes simulation update."
    }

    foreach ($forbidden in @(
        "SimulationScheduler", "ISandboxSystem")) {
        Reject-Text ($simulationHeader + $simulationSource) $forbidden `
            "A5 abstraction in AL-A3"
    }

    foreach ($testId in @(
        "AL-A3/initial-snapshot-has-no-completed-tick",
        "AL-A3/tick-context-is-propagated",
        "AL-A3/phase-identity-and-order-are-frozen",
        "AL-A3/raw-last-tick-timings-are-observable",
        "AL-A3/caller-owned-pause-gate-freezes-simulation",
        "AL-A3/resume-advances-exactly-one-tick")) {
        Require-Text $testSource $testId "AL-A3 behavioral check"
    }

    Write-Host (
        "[WORLD_SIMULATION_BOUNDARY] status=PASS phases=8 " +
        "tick_entry=delegated raw_timing=last-tick " +
        "pause_gate=caller-owned")
    exit 0
}
catch {
    Write-Error (
        "[WORLD_SIMULATION_BOUNDARY] status=FAIL " +
        $_.Exception.Message)
    exit 1
}
