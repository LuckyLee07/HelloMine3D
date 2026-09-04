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
    $headerPath = Join-Path $Root `
        "src\HelloMine3D\World\Block\BlockCapability.h"
    $sourcePath = Join-Path $Root `
        "src\HelloMine3D\World\Block\BlockCapability.cpp"
    $definitionPath = Join-Path $Root `
        "src\HelloMine3D\World\Block\BlockDefinition.h"
    $databasePath = Join-Path $Root `
        "src\HelloMine3D\World\Block\BlockDatabase.cpp"
    $uiPath = Join-Path $Root `
        "src\HelloMine3D\Ogre\OgreUserInterface.cpp"
    $testPath = Join-Path $Root `
        "src\HelloMine3D\Tests\WorldRuntimeSmokeMain.cpp"
    $contractPath = Join-Path $Root `
        "docs\contracts\block-capability-model-contract-v1.md"

    foreach ($path in @(
            $headerPath, $sourcePath, $definitionPath, $databasePath,
            $uiPath, $testPath, $contractPath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required C1 file is missing: $path"
        }
    }

    $header = Get-Content -LiteralPath $headerPath -Raw
    $source = Get-Content -LiteralPath $sourcePath -Raw
    $definition = Get-Content -LiteralPath $definitionPath -Raw
    $database = Get-Content -LiteralPath $databasePath -Raw
    $ui = Get-Content -LiteralPath $uiPath -Raw
    $test = Get-Content -LiteralPath $testPath -Raw
    $contract = Get-Content -LiteralPath $contractPath -Raw

    foreach ($token in @(
            "class InventoryProvider", "class MachineProcessor",
            "class BlockCapabilityAccess", "BlockCapabilityDefinition",
            "InventoryProviderSlotView", "InventorySlotRole::General",
            "InventorySlotRole::Input", "InventorySlotRole::Fuel",
            "InventorySlotRole::Output", "AutomaticSlot = -1")) {
        Require-Text ($header + $source) $token "C1 access protocol"
    }
    foreach ($token in @(
            "ChestContainer::BlockEntityType",
            "InventoryProviderKind::Chest",
            "FurnaceContainer::BlockEntityType",
            "InventoryProviderKind::Furnace",
            "MachineProcessorKind::Furnace")) {
        Require-Text $database $token "C1 concrete provider declaration"
    }
    Require-Text $definition "BlockCapabilityDefinition capabilities" `
        "BlockDefinition-owned capability identity"

    Require-Text $ui "BlockCapabilityAccess::query(" `
        "real capability UI consumer"
    Require-Text $ui "provider.transferFromPlayer(" `
        "capability insert consumer"
    Require-Text $ui "provider.transferToPlayer(" `
        "capability extract consumer"
    foreach ($forbidden in @(
            '#include "../World/Block/ChestContainer.h"',
            '#include "../World/Block/FurnaceContainer.h"',
            "ChestContainer::view(", "FurnaceContainer::view(",
            "ChestContainer::transferFromPlayer(",
            "FurnaceContainer::transferFromPlayer(")) {
        Reject-Text $ui $forbidden "concrete container UI dispatch"
    }

    foreach ($forbidden in @(
            "BlockCapabilityRegistry", "CapabilityRegistry",
            "MechanicalPort", "ItemTransportPort", "StorageProvider",
            "MechanicalNode", "MechanicalNetwork")) {
        Reject-Text ($header + $source + $definition + $database + $ui) `
            $forbidden "unapproved C2/C3/Extended abstraction"
    }

    foreach ($token in @(
            'std::string(focus) == "C1-CAP"',
            "caseBlockCapabilityModel();",
            "C1-CAP/chest-declares-inventory-only",
            "C1-CAP/furnace-declares-inventory-and-processor",
            "C1-CAP/ordinary-block-rejects-attached-record",
            "C1-CAP/mismatched-record-exposes-no-capability",
            "C1-CAP/malformed-provider-fails-access-closed",
            "C1-CAP/stale-provider-handle-fails-closed",
            "C1-CAP/save-reopen-derives-capability-from-v12-state")) {
        Require-Text $test $token "C1 focused/negative runtime evidence"
    }

    $frozen = $contract.IndexOf(
        "Status: Frozen for C1 implementation",
        [StringComparison]::Ordinal) -ge 0
    $gateReady = $contract.IndexOf(
        "Status: Frozen for C1 full-gate verification",
        [StringComparison]::Ordinal) -ge 0
    $verified = $contract.IndexOf(
        "Status: Frozen after C1 verification",
        [StringComparison]::Ordinal) -ge 0
    if (-not ($frozen -or $gateReady -or $verified)) {
        throw "C1 contract is not frozen."
    }
    foreach ($token in @(
            "24 recipes", "eight tools", "34 objectives",
            "terrain v4", "settings v8", "save v12",
            "AI-01..AI-08", "NOT_CLAIMED")) {
        Require-Text $contract $token "C1 compatibility/evidence boundary"
    }

    Write-Host (
        "[BLOCK_CAPABILITY_MODEL] status=PASS c1_providers=2 " +
        "c1_capabilities=2 focused_cases=17 registry=absent " +
        "mechanical_port=absent")
    exit 0
}
catch {
    Write-Error (
        "[BLOCK_CAPABILITY_MODEL] status=FAIL " +
        $_.Exception.Message)
    exit 1
}
