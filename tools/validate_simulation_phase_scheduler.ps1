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
        throw "Forbidden $Label ('$Needle')."
    }
}

try {
    $paths = @{
        Header = Join-Path $Root `
            "src\HelloMine3D\World\Simulation\SimulationPhaseScheduler.h"
        Source = Join-Path $Root `
            "src\HelloMine3D\World\Simulation\SimulationPhaseScheduler.cpp"
        SimulationHeader = Join-Path $Root `
            "src\HelloMine3D\World\Simulation\WorldSimulation.h"
        SimulationSource = Join-Path $Root `
            "src\HelloMine3D\World\Simulation\WorldSimulation.cpp"
        ActorHeader = Join-Path $Root `
            "src\HelloMine3D\Actor\ActorManager.h"
        ActorSource = Join-Path $Root `
            "src\HelloMine3D\Actor\ActorManager.cpp"
        WorldHeader = Join-Path $Root "src\HelloMine3D\World\World.h"
        WorldSource = Join-Path $Root "src\HelloMine3D\World\World.cpp"
        Furnace = Join-Path $Root `
            "src\HelloMine3D\World\Block\FurnaceContainer.cpp"
        Crusher = Join-Path $Root `
            "src\HelloMine3D\World\Block\CrusherContainer.cpp"
        Ui = Join-Path $Root `
            "src\HelloMine3D\Ogre\OgreUserInterface.cpp"
        Test = Join-Path $Root `
            "src\HelloMine3D\Tests\WorldRuntimeSmokeMain.cpp"
        SaveHeader = Join-Path $Root `
            "src\HelloMine3D\World\Storage\WorldSave.h"
        Contract = Join-Path $Root `
            "docs\contracts\simulation-phase-scheduler-v0-contract-v1.md"
    }
    foreach ($path in $paths.Values) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required D1 file is missing: $path"
        }
    }

    $text = @{}
    foreach ($entry in $paths.GetEnumerator()) {
        $text[$entry.Key] = Get-Content -LiteralPath $entry.Value -Raw
    }
    $scheduler = $text.Header + $text.Source
    $simulation = $text.SimulationHeader + $text.SimulationSource

    foreach ($token in @(
            "enum class SimulationScheduledWorkload",
            "ManagedActors = 0", "RandomTickSections", "BlockEntities",
            "SimulationScheduledWorkloadCount",
            "struct SimulationWorkPlan", "std::size_t eligible",
            "std::size_t admitted", "std::size_t deferred",
            "std::size_t budget", "std::size_t firstIndex",
            "std::size_t serviceWindowTicks",
            "class SimulationPhaseScheduler")) {
        Require-Text $scheduler $token "concrete D1 vocabulary"
    }
    foreach ($token in @(
            "ManagedActorBudgetPerTick = 64",
            "BlockEntityBudgetPerTick = 32",
            "planManagedActors(", "planRandomTickSections(",
            "planBlockEntities(", "std::min(eligible, budget)",
            "1 + (eligible - 1) / budget",
            "next = (next + plan.admitted) % eligible")) {
        Require-Text $scheduler $token "deterministic item admission"
    }

    foreach ($token in @(
            "prepareBudgetedTick(", "getLiveActorCount() const",
            "tickBudgetedRange(", "completeBudgetedTick()",
            "(firstIndex + offset) % actorCount")) {
        Require-Text ($text.ActorHeader + $text.ActorSource) $token `
            "budgeted Actor adapter"
    }
    foreach ($token in @(
            "planManagedActors(managedActorsEligible)",
            "planRandomTickSections(",
            "m_world.runRandomTicks(context.tick, randomTickPlan.admitted)",
            "collectScheduledBlockEntities(m_world)",
            "std::sort(work.begin(), work.end(), scheduledBlockEntityLess)",
            "planBlockEntities(work.size())",
            "FurnaceContainer::tickOne(",
            "CrusherContainer::tickOne(",
            "SimulationMetricPhaseCount = 5",
            "scheduledWorkloads")) {
        Require-Text $simulation $token "three real phase consumers"
    }
    Require-Text $text.WorldHeader `
        "RandomTickSectionBudgetPerTick = 4" `
        "retained Random Tick budget"
    Require-Text $text.WorldSource `
        "void World::runRandomTicks(int worldTime, std::size_t sectionBudget)" `
        "scheduler-owned Random Tick admission"
    Require-Text $text.Furnace "bool FurnaceContainer::tickOne(" `
        "Furnace item adapter"
    Require-Text $text.Crusher "bool CrusherContainer::tickOne(" `
        "Crusher item adapter"

    foreach ($token in @(
            "Scheduler %s:", "admitted / deferred / eligible",
            "service window:", "schedulerManaged")) {
        Require-Text ($text.Ui + $text.SimulationHeader) $token `
            "copied D1 diagnostics"
    }

    foreach ($testId in @(
            "D1-SCHEDULER/exactly-three-real-workload-identities",
            "D1-SCHEDULER/actor-round-robin-is-bounded-and-deterministic",
            "D1-SCHEDULER/existing-random-tick-fifo-owns-order",
            "D1-SCHEDULER/block-entity-round-robin-is-bounded",
            "D1-SCHEDULER/runtime-publishes-three-copied-plans",
            "D1-SCHEDULER/actor-overload-defers-without-dropping",
            "D1-SCHEDULER/block-entity-overload-defers-in-stable-order",
            "D1-SCHEDULER/deferred-actors-are-serviced-next-window",
            "D1-SCHEDULER/deferred-block-entities-are-serviced-next-window",
            "D1-SCHEDULER/player-and-phase-barriers-remain-mandatory")) {
        Require-Text $text.Test $testId "D1 focused evidence"
    }
    $focusCases = ([regex]::Matches(
        $text.Test, 'check\("D1-SCHEDULER/')).Count
    if ($focusCases -ne 10) {
        throw "D1 focus must freeze exactly 10 D1 checks; found $focusCases."
    }
    Require-Text $text.Test `
        'std::string(focus) == "D1-SCHEDULER"' `
        "D1 focused runner"

    Require-Text $text.SaveHeader "WorldSaveFormatVersion = 12" `
        "save v12 boundary"
    $storageRoot = Join-Path $Root "src\HelloMine3D\World\Storage"
    $storageText = (Get-ChildItem -LiteralPath $storageRoot -Recurse `
        -File -Include *.h,*.cpp | ForEach-Object {
            Get-Content -LiteralPath $_.FullName -Raw
        }) -join "`n"
    foreach ($token in @(
            "SimulationPhaseScheduler", "SimulationWorkPlan",
            "serviceWindowTicks")) {
        Reject-Text $storageText $token "persisted scheduler state"
    }

    foreach ($token in @(
            "ISandboxSystem", "SimulationSystemRegistry",
            "SimulationRegistry", "priority_queue", "ActivationLevel",
            "ReducedSimulation", "OfflineCatchUp")) {
        Reject-Text ($scheduler + $simulation) $token `
            "unapproved generic or D2+ runtime"
    }
    Reject-Text $scheduler "std::chrono" "wall-clock admission"

    foreach ($token in @(
            "Status: Frozen after D1 verification", "three current",
            "64", "four-item FIFO", "32", "ceil(N / B)",
            "save v12", "D2-D8", "AI-01..AI-08",
            "NOT_CLAIMED")) {
        Require-Text $text.Contract $token "D1 frozen boundary"
    }

    Write-Host (
        "[SIMULATION_PHASE_SCHEDULER] status=PASS workloads=3 " +
        "budgets=64/4/32 fairness=round-robin/fifo " +
        "wall_clock=absent save=12 d2_plus=absent")
    exit 0
}
catch {
    Write-Error (
        "[SIMULATION_PHASE_SCHEDULER] status=FAIL " +
        $_.Exception.Message)
    exit 1
}
