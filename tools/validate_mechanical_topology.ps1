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
            "src\HelloMine3D\Mechanical\MechanicalTopology.h"
        Source = Join-Path $Root `
            "src\HelloMine3D\Mechanical\MechanicalTopology.cpp"
        CapabilityHeader = Join-Path $Root `
            "src\HelloMine3D\World\Block\BlockCapability.h"
        CapabilitySource = Join-Path $Root `
            "src\HelloMine3D\World\Block\BlockCapability.cpp"
        Database = Join-Path $Root `
            "src\HelloMine3D\World\Block\BlockDatabase.cpp"
        WorldHeader = Join-Path $Root "src\HelloMine3D\World\World.h"
        WorldSource = Join-Path $Root "src\HelloMine3D\World\World.cpp"
        ChunkManager = Join-Path $Root `
            "src\HelloMine3D\World\Chunk\ChunkManager.cpp"
        Ui = Join-Path $Root `
            "src\HelloMine3D\Ogre\OgreUserInterface.cpp"
        English = Join-Path $Root "media\text\en-US.text"
        Chinese = Join-Path $Root "media\text\zh-CN.text"
        Test = Join-Path $Root `
            "src\HelloMine3D\Tests\WorldRuntimeSmokeMain.cpp"
        SaveHeader = Join-Path $Root `
            "src\HelloMine3D\World\Storage\WorldSave.h"
        SaveSource = Join-Path $Root `
            "src\HelloMine3D\World\Storage\WorldSave.cpp"
        Contract = Join-Path $Root `
            "docs\contracts\mechanical-topology-model-v0-contract-v1.md"
    }
    foreach ($path in $paths.Values) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required C3 file is missing: $path"
        }
    }

    $text = @{}
    foreach ($entry in $paths.GetEnumerator()) {
        $text[$entry.Key] = Get-Content -LiteralPath $entry.Value -Raw
    }
    $topology = $text.Header + $text.Source

    foreach ($token in @(
            "struct MechanicalNodeId", "struct MechanicalNetworkId",
            "enum class MechanicalFace", "struct MechanicalConnection",
            "struct MechanicalComponent", "class MechanicalTopology",
            "class MechanicalPort", "MechanicalPortKind::CrusherAllFaces")) {
        Require-Text ($topology + $text.CapabilityHeader +
            $text.CapabilitySource + $text.Database) $token `
            "concrete Mechanical topology vocabulary"
    }
    foreach ($token in @(
            "NeighborOffsets", "CanonicalEdgeOffsets",
            "std::deque<MechanicalNodeId>",
            "std::sort(component.nodes.begin(), component.nodes.end())",
            "component.id.anchor = component.nodes.front()",
            "if (existed == present)", "m_dirty = false")) {
        Require-Text $topology $token `
            "deterministic six-face connectivity rebuild"
    }

    foreach ($token in @(
            "refreshMechanicalNodeUnlocked(",
            "synchronizeMechanicalChunkUnlocked(",
            "removeMechanicalChunkUnlocked(",
            "CrusherContainer::deserialize(record->payload, state)",
            "m_mechanicalTopology.replaceChunkNodes(",
            "m_mechanicalTopology.nodeSnapshot(")) {
        Require-Text ($text.WorldHeader + $text.WorldSource) $token `
            "authoritative World topology adapter"
    }
    foreach ($token in @(
            "m_world->synchronizeMechanicalChunkUnlocked(key);",
            "m_world->synchronizeMechanicalChunkUnlocked(chunkPosition);",
            "m_world->removeMechanicalChunkUnlocked({x, z});")) {
        Require-Text $text.ChunkManager $token `
            "Chunk load/unload topology boundary"
    }

    foreach ($token in @(
            "std::optional<MechanicalPort> mechanicalPort",
            "MechanicalPort::view(World &world)",
            "world.getMechanicalNodeSnapshot(m_position)",
            "MechanicalPortKind::CrusherAllFaces")) {
        Require-Text ($text.CapabilityHeader + $text.CapabilitySource +
            $text.Database) $token "Crusher MechanicalPort capability"
    }
    foreach ($token in @(
            '"crusher.network_id"', '"crusher.network_nodes"',
            '"crusher.network_connections"',
            "mechanicalNetworkIdString(",
            "worldStats.mechanicalTopology")) {
        Require-Text $text.Ui $token "normal/debug topology UI"
    }
    foreach ($key in @(
            "crusher.network_id", "crusher.network_nodes",
            "crusher.network_connections")) {
        Require-Text $text.English $key "English topology text"
        Require-Text $text.Chinese $key "Chinese topology text"
    }

    foreach ($token in @(
            'std::string(focus) == "C3-TOPOLOGY"',
            "caseMechanicalTopologyC3();",
            "C3-TOPOLOGY/removing-bridge-splits-components",
            "C3-TOPOLOGY/normal-placement-exposes-crusher-port-and-merge",
            "C3-TOPOLOGY/malformed-payload-removes-node-until-repaired",
            "C3-TOPOLOGY/chunk-unload-reload-removes-and-restores-edge",
            "C3-TOPOLOGY/save-reopen-rebuilds-identical-component")) {
        Require-Text $text.Test $token "C3 focused/negative evidence"
    }
    $focusCases = ([regex]::Matches(
        $text.Test, 'check\("C3-TOPOLOGY/')).Count
    if ($focusCases -ne 17) {
        throw "C3 focus must freeze exactly 17 C3 checks; found $focusCases."
    }

    Require-Text $text.SaveHeader "WorldSaveFormatVersion = 12" `
        "save v12 boundary"
    foreach ($token in @("MechanicalNode", "MechanicalNetwork",
            "MechanicalTopology")) {
        Reject-Text ($text.SaveHeader + $text.SaveSource) $token `
            "persisted derived topology"
    }

    foreach ($forbidden in @(
            "INetwork", "GenericNode", "DynamicNetworkCore",
            "MechanicalRegistry", "RPM", "Torque", "StressCapacity",
            "WaterWheel", "MechanicalShaft", "MechanicalGear",
            "MechanicalDrill")) {
        Reject-Text $topology $forbidden "unapproved C4+/generic network"
    }

    foreach ($token in @(
            "Status: Frozen for C3 implementation", "Crusher-only",
            "save v12", "C4-C11", "AI-01..AI-08", "NOT_CLAIMED")) {
        Require-Text $text.Contract $token "C3 frozen boundary"
    }

    Write-Host (
        "[MECHANICAL_TOPOLOGY] status=PASS node=crusher faces=6 " +
        "focus_cases=17 rebuild=deterministic save=12 " +
        "generic_network=absent c4_power=absent")
    exit 0
}
catch {
    Write-Error (
        "[MECHANICAL_TOPOLOGY] status=FAIL " + $_.Exception.Message)
    exit 1
}
