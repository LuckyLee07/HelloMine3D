[CmdletBinding()]
param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent $PSScriptRoot
}

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
        RuntimeHeader = Join-Path $Root `
            "src\HelloMine3D\World\Simulation\MachineRuntime.h"
        RuntimeSource = Join-Path $Root `
            "src\HelloMine3D\World\Simulation\MachineRuntime.cpp"
        Process = Join-Path $Root `
            "src\HelloMine3D\Item\MachineProcessDefinition.cpp"
        CrusherHeader = Join-Path $Root `
            "src\HelloMine3D\World\Block\CrusherContainer.h"
        CrusherSource = Join-Path $Root `
            "src\HelloMine3D\World\Block\CrusherContainer.cpp"
        Furnace = Join-Path $Root `
            "src\HelloMine3D\World\Block\FurnaceContainer.cpp"
        CapabilityHeader = Join-Path $Root `
            "src\HelloMine3D\World\Block\BlockCapability.h"
        CapabilitySource = Join-Path $Root `
            "src\HelloMine3D\World\Block\BlockCapability.cpp"
        Database = Join-Path $Root `
            "src\HelloMine3D\World\Block\BlockDatabase.cpp"
        Simulation = Join-Path $Root `
            "src\HelloMine3D\World\Simulation\WorldSimulation.cpp"
        Ui = Join-Path $Root `
            "src\HelloMine3D\Ogre\OgreUserInterface.cpp"
        EconomyHeader = Join-Path $Root `
            "src\HelloMine3D\Item\ResourceEconomyVerifier.h"
        EconomySource = Join-Path $Root `
            "src\HelloMine3D\Item\ResourceEconomyVerifier.cpp"
        BlockIds = Join-Path $Root `
            "src\HelloMine3D\World\Block\BlockId.h"
        Material = Join-Path $Root `
            "src\HelloMine3D\Item\Material.cpp"
        Recipe = Join-Path $Root "media\recipes\Base.recipe"
        Block = Join-Path $Root "media\blocks\Crusher.block"
        WorldTest = Join-Path $Root `
            "src\HelloMine3D\Tests\WorldRuntimeSmokeMain.cpp"
        RecipeTest = Join-Path $Root `
            "src\HelloMine3D\Tests\RecipeSmokeMain.cpp"
        WorldSave = Join-Path $Root `
            "src\HelloMine3D\World\Storage\WorldSave.h"
        Contract = Join-Path $Root `
            "docs\contracts\machine-runtime-v0-contract-v1.md"
    }
    foreach ($path in $paths.Values) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required C2 file is missing: $path"
        }
    }

    $text = @{}
    foreach ($entry in $paths.GetEnumerator()) {
        $text[$entry.Key] = Get-Content -LiteralPath $entry.Value -Raw
    }

    foreach ($token in @(
            "MachineStatus::Idle", "MachineStatus::MissingInput",
            "MachineStatus::BlockedOutput", "MachineStatus::NoPower",
            "MachineStatus::Running", "MachineRuntime::inspect(",
            "MachineRuntime::tick(")) {
        Require-Text ($text.RuntimeHeader + $text.RuntimeSource) $token `
            "five-state Machine Runtime"
    }
    foreach ($token in @(
            '"hellomine:crush_cobblestone"',
            "Material::ID::Cobblestone", "Material::ID::Sand", "40")) {
        Require-Text $text.Process $token "single C2 process"
    }
    Require-Text $text.Furnace "MachineRuntime::tick(" `
        "Furnace Runtime adapter"
    Require-Text $text.CrusherSource "MachineRuntime::tick(" `
        "Crusher Runtime adapter"
    Require-Text $text.Simulation "CrusherContainer::tickOne(m_world," `
        "D1-budgeted BlockEntitySimulation Crusher tick"

    foreach ($token in @(
            "BlockId::Crusher", "CrusherContainer::BlockEntityType",
            "InventoryProviderKind::Crusher",
            "MachineProcessorKind::Crusher")) {
        Require-Text $text.Database $token "real Crusher capability declaration"
    }
    foreach ($token in @(
            "CrankPulseTicks = 20", "MaxCrankTicks = 40",
            'BlockEntityType = "hellomine:crusher"',
            'output << "v1|"', "supplyManualPower(")) {
        Require-Text ($text.CrusherHeader + $text.CrusherSource) $token `
            "bounded Crusher ownership"
    }
    Require-Text $text.Ui "manualPowerSupported" "normal Crusher UI"
    Require-Text $text.Ui "supplyManualPower(" "normal crank UI command"
    foreach ($forbidden in @(
            '#include "../World/Block/CrusherContainer.h"',
            '#include "../World/Block/FurnaceContainer.h"')) {
        Reject-Text $text.Ui $forbidden "concrete machine UI dispatch"
    }

    Require-Text $text.BlockIds "Crusher = 26" "append-only Block id"
    Require-Text $text.Material '"hellomine:crusher"' `
        "append-only Material id"
    if ($text.Block -notmatch '(?ms)^Id\s+26\s*$') {
        throw "Crusher block resource does not freeze Id 26."
    }
    Require-Text $text.Recipe "recipe hellomine:crusher shaped" `
        "normal Crusher recipe"
    foreach ($row in @(
            "row hellomine:cobblestone hellomine:iron_ingot hellomine:cobblestone",
            "row hellomine:oak_planks _ hellomine:oak_planks",
            "row hellomine:cobblestone hellomine:cobblestone hellomine:cobblestone")) {
        Require-Text $text.Recipe $row "frozen Crusher recipe row"
    }

    Require-Text $text.EconomyHeader "int version = 2;" `
        "economy schema v2"
    Require-Text $text.EconomyHeader `
        "std::vector<MachineProcessDefinition> machineProcesses;" `
        "machine transformation inputs"
    Require-Text $text.EconomySource `
        "contract.machineProcesses = {handCrusherProcessDefinition()};" `
        "single machine economy edge"
    Require-Text $text.WorldSave "WorldSaveFormatVersion = 12" `
        "save v12 boundary"

    foreach ($token in @(
            'std::string(focus) == "C2-MACHINE"',
            "caseMachineRuntimeC2();",
            "C2-MACHINE/runtime-completion-is-atomic-and-conservative",
            "C2-MACHINE/normal-use-opens-and-supplies-one-crank-pulse",
            "C2-MACHINE/one-pulse-pauses-at-no-power-with-partial-progress",
            "C2-MACHINE/save-reopen-restores-v1-crusher-payload",
            "C2-MACHINE/stale-processor-handle-fails-closed")) {
        Require-Text $text.WorldTest $token "C2 focused runtime evidence"
    }
    foreach ($token in @(
            "C2-MACHINE/crusher-is-a-normal-iron-stage-workbench-recipe",
            "C2-MACHINE/economy-v2-freezes-one-concrete-machine-process",
            "C2-MACHINE/invalid-machine-process-fails-economy-closed",
            "C2-MACHINE/machine-transformation-cycle-is-rejected")) {
        Require-Text $text.RecipeTest $token "C2 recipe/economy evidence"
    }

    $production = $text.RuntimeHeader + $text.RuntimeSource +
        $text.Process + $text.CrusherHeader + $text.CrusherSource +
        $text.CapabilityHeader + $text.CapabilitySource + $text.Database +
        $text.Simulation + $text.Ui
    foreach ($forbidden in @(
            "MachineRegistry", "CapabilityRegistry", "ItemTransportPort",
            "StorageProvider", "SharedNetworkCore")) {
        Reject-Text $production $forbidden "unapproved C3+/Extended abstraction"
    }

    $frozen = $text.Contract.Contains(
        "Status: Frozen for C2 implementation") -or
        $text.Contract.Contains(
            "Status: Frozen for C2 full-gate verification") -or
        $text.Contract.Contains("Status: Frozen after C2 verification")
    if (-not $frozen) {
        throw "C2 contract is not frozen."
    }
    foreach ($token in @(
            "25", "8 tools", "34", "terrain v4", "settings v8",
            "save remains v12", "AI-01..AI-08", "NOT_CLAIMED")) {
        Require-Text $text.Contract $token "C2 compatibility/evidence boundary"
    }

    Write-Host (
        "[MACHINE_RUNTIME] status=PASS processors=2 statuses=5 " +
        "crusher_processes=1 crank_cap=40 save=12 registry=absent " +
        "mechanical_port=downstream_c3")
    exit 0
}
catch {
    Write-Error (
        "[MACHINE_RUNTIME] status=FAIL " + $_.Exception.Message)
    exit 1
}
