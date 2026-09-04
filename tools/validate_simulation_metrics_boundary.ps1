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
    $sourceRoot = Join-Path $Root "src\HelloMine3D"
    $simulationHeaderPath = Join-Path $sourceRoot `
        "World\Simulation\WorldSimulation.h"
    $simulationSourcePath = Join-Path $sourceRoot `
        "World\Simulation\WorldSimulation.cpp"
    $actorHeaderPath = Join-Path $sourceRoot "Actor\ActorManager.h"
    $actorSourcePath = Join-Path $sourceRoot "Actor\ActorManager.cpp"
    $worldHeaderPath = Join-Path $sourceRoot "World\World.h"
    $uiPath = Join-Path $sourceRoot "Ogre\OgreUserInterface.cpp"
    $testPath = Join-Path $sourceRoot `
        "Tests\WorldRuntimeSmokeMain.cpp"

    foreach ($path in @(
        $simulationHeaderPath, $simulationSourcePath, $actorHeaderPath,
        $actorSourcePath, $worldHeaderPath, $uiPath, $testPath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required AL-A5 source is missing: $path"
        }
    }

    $header = Get-Content -LiteralPath $simulationHeaderPath -Raw
    $source = Get-Content -LiteralPath $simulationSourcePath -Raw
    $actorHeader = Get-Content -LiteralPath $actorHeaderPath -Raw
    $actorSource = Get-Content -LiteralPath $actorSourcePath -Raw
    $worldHeader = Get-Content -LiteralPath $worldHeaderPath -Raw
    $ui = Get-Content -LiteralPath $uiPath -Raw
    $tests = Get-Content -LiteralPath $testPath -Raw

    foreach ($token in @(
        "SimulationMetricPhaseCount = 5",
        "struct SimulationPhaseMetrics",
        "double elapsedMilliseconds",
        "std::size_t processed",
        "std::size_t deferred",
        "std::size_t budget",
        "std::size_t eligible",
        "std::size_t serviceWindowTicks",
        "bool schedulerManaged",
        "SimulationPhaseBudgetScope budgetScope",
        "Unbudgeted", "PerTick", "PerPopulationCycle",
        "WithinBudget", "AtBudget", "WorkDeferred",
        "std::array<SimulationPhaseMetrics, SimulationMetricPhaseCount> metrics",
        "findSimulationPhaseMetrics")) {
        Require-Text $header $token "metrics vocabulary"
    }

    Require-OrderedText $source @(
        "snapshot.metrics[0].phase = WorldSimulationPhase::ActorSimulation;",
        "snapshot.metrics[1].phase = WorldSimulationPhase::Combat;",
        "snapshot.metrics[2].phase = WorldSimulationPhase::BlockRandomTick;",
        "snapshot.metrics[3].phase = WorldSimulationPhase::Population;",
        "snapshot.metrics[4].phase =") `
        "four retained plus one D1 concrete metric identity"

    $metricIdentityCount = [regex]::Matches(
        $source,
        'snapshot\.metrics\[[0-9]+\]\.phase\s*=\s*WorldSimulationPhase::' `
    ).Count
    if ($metricIdentityCount -ne 5) {
        throw "Metric identity slots must remain exactly five after D1; found $metricIdentityCount."
    }

    foreach ($token in @(
        "metrics->processed = managedActorsProcessed + 1",
        "metrics->deferred = actorPlan.deferred",
        "m_combatProjectileStepsUsed",
        "m_combatProjectileStepBudgetDenied",
        "m_randomTickSectionsProcessed",
        "randomTickPlan.deferred",
        "m_naturalMobSpawnAttempts -",
        "naturalSpawnAttemptsPerCycle",
        "metrics->deferred = blockEntityPlan.deferred")) {
        Require-Text $source $token "real phase work source"
    }
    Require-Text $actorHeader "std::size_t tick(World &world, float dt);" `
        "counted Actor tick"
    Require-Text $actorSource "return processed;" `
        "Actor processed result"

    foreach ($token in @(
        "RandomTickSectionBudgetPerTick = 4",
        "CombatProjectileStepBudgetPerTick = 32")) {
        Require-Text $worldHeader $token "preserved hard limit"
    }

    foreach ($token in @(
        "processed / deferred", "budget:",
        "simulationPhaseBudgetScopeName(",
        "simulationPhaseBudgetStatusName(")) {
        Require-Text $ui $token "developer Simulation panel metric"
    }

    foreach ($forbidden in @(
        "ISandboxSystem", "SimulationSystemRegistry",
        "MachineSimulation", "NetworkSimulation",
        "TransportSimulation", "AISimulation")) {
        Reject-Text ($header + $source) $forbidden `
            "speculative scheduler/system metric"
    }

    $storageRoot = Join-Path $sourceRoot "World\Storage"
    $storageText = (Get-ChildItem -LiteralPath $storageRoot -Recurse `
        -File -Include *.h,*.cpp | ForEach-Object {
            Get-Content -LiteralPath $_.FullName -Raw
        }) -join "`n"
    Reject-Text $storageText "SimulationPhaseMetrics" `
        "persisted diagnostic metric"

    foreach ($testId in @(
        "AL-A5/metric-identities-exclude-empty-system-slots",
        "AL-A5/budget-status-vocabulary-is-frozen",
        "AL-A5/metric-elapsed-matches-a3-phase-timing",
        "D1/actor-work-retains-a5-count-and-adds-scheduler-budget",
        "AL-A5/combat-uses-existing-per-tick-budget",
        "AL-A5/random-tick-uses-existing-per-tick-budget",
        "AL-A5/population-is-per-cycle-and-resets-next-tick")) {
        Require-Text $tests $testId "AL-A5 behavioral check"
    }

    Write-Host (
        "[SIMULATION_METRICS_BOUNDARY] status=PASS metric_phases=5 " +
        "budget_scopes=3 budget_statuses=4 persistence=excluded " +
        "downstream_d1=composed")
    exit 0
}
catch {
    Write-Error (
        "[SIMULATION_METRICS_BOUNDARY] status=FAIL " +
        $_.Exception.Message)
    exit 1
}
