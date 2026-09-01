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
    $worldHeaderPath = Join-Path $sourceRoot "World\World.h"
    $worldSourcePath = Join-Path $sourceRoot "World\World.cpp"
    $runtimePath = Join-Path $sourceRoot "Sandbox\SandboxRuntime.cpp"
    $busHeaderPath = Join-Path $sourceRoot `
        "Sandbox\Events\SandboxEventBus.h"
    $busSourcePath = Join-Path $sourceRoot `
        "Sandbox\Events\SandboxEventBus.cpp"
    $commandHeaderPath = Join-Path $sourceRoot `
        "World\Command\IWorldCommand.h"
    $interactionHeaderPath = Join-Path $sourceRoot `
        "World\Command\PlayerBlockInteractionCommand.h"
    $interactionSourcePath = Join-Path $sourceRoot `
        "World\Command\PlayerBlockInteractionCommand.cpp"
    $objectivePath = Join-Path $sourceRoot `
        "Gameplay\ObjectiveSystem.cpp"
    $feedbackPath = Join-Path $sourceRoot `
        "Feedback\ActionFeedback.cpp"
    $audioPath = Join-Path $sourceRoot "Audio\AudioRuntime.cpp"
    $testPath = Join-Path $sourceRoot `
        "Tests\WorldRuntimeSmokeMain.cpp"

    $requiredPaths = @(
        $worldHeaderPath, $worldSourcePath, $runtimePath,
        $busHeaderPath, $busSourcePath, $commandHeaderPath,
        $interactionHeaderPath, $interactionSourcePath, $objectivePath,
        $feedbackPath, $audioPath, $testPath)
    foreach ($path in $requiredPaths) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required AL-A4 source is missing: $path"
        }
    }

    foreach ($legacyPath in @(
        (Join-Path $sourceRoot "World\Event\IWorldEvent.h"),
        (Join-Path $sourceRoot "World\Event\PlayerDigEvent.h"),
        (Join-Path $sourceRoot "World\Event\PlayerDigEvent.cpp"))) {
        if (Test-Path -LiteralPath $legacyPath) {
            throw "Legacy command-as-event path still exists: $legacyPath"
        }
    }

    $worldHeader = Get-Content -LiteralPath $worldHeaderPath -Raw
    $worldSource = Get-Content -LiteralPath $worldSourcePath -Raw
    $runtime = Get-Content -LiteralPath $runtimePath -Raw
    $busHeader = Get-Content -LiteralPath $busHeaderPath -Raw
    $busSource = Get-Content -LiteralPath $busSourcePath -Raw
    $objective = Get-Content -LiteralPath $objectivePath -Raw
    $feedback = Get-Content -LiteralPath $feedbackPath -Raw
    $audio = Get-Content -LiteralPath $audioPath -Raw
    $tests = Get-Content -LiteralPath $testPath -Raw

    Require-Text $worldHeader "void addCommand(Args &&... args)" `
        "World command submission"
    Require-Text $worldHeader "std::is_base_of<IWorldCommand, T>" `
        "typed command constraint"
    Require-Text $worldHeader `
        "std::vector<std::unique_ptr<IWorldCommand>> m_commands;" `
        "World-owned command FIFO"
    Require-Text $worldSource "command->execute(*this);" `
        "command execution"
    Require-Text $runtime "PlayerBlockInteractionCommand" `
        "player interaction command"

    $productionFiles = Get-ChildItem -LiteralPath $sourceRoot -Recurse `
        -File -Include *.h,*.cpp |
        Where-Object { $_.FullName -notlike "*\Tests\*" }
    $productionText = ($productionFiles | ForEach-Object {
        Get-Content -LiteralPath $_.FullName -Raw
    }) -join "`n"
    foreach ($legacyToken in @(
        "IWorldEvent", "PlayerDigEvent", "PlayerDigAction", "addEvent<")) {
        Reject-Text $productionText $legacyToken `
            "legacy command-as-event vocabulary"
    }
    $productionSubscriberText = ($productionFiles | Where-Object {
        $_.FullName -ne $busSourcePath
    } | ForEach-Object {
        Get-Content -LiteralPath $_.FullName -Raw
    }) -join "`n"
    $productionSubscribeCount = [regex]::Matches(
        $productionSubscriberText, '\.subscribe\s*\(').Count
    $declaredEffectCount = [regex]::Matches(
        $productionSubscriberText,
        'SandboxEventSubscriptionOptions::(?:observer|domainMutation)\(' `
    ).Count
    if ($productionSubscribeCount -ne $declaredEffectCount) {
        throw (
            "Every production subscriber must declare an effect owner: " +
            "subscribe=$productionSubscribeCount declared=$declaredEffectCount")
    }

    foreach ($token in @(
        "SandboxEventCategory", "const SandboxEventType type",
        "const SandboxEventCategory category",
        "SandboxEventHandlerEffect", "SandboxEventRepublishPolicy",
        "MaxDispatchDepth = 8", "SandboxEventDispatchResult publish",
        "SandboxEventBusDebugSnapshot debugSnapshot")) {
        Require-Text $busHeader $token "event boundary declaration"
    }
    foreach ($token in @(
        "HandlerRepublishForbidden", "DepthLimit",
        "event.category == SandboxEventCategory::Diagnostic",
        "const std::vector<Subscription> dispatchSubscriptions",
        "m_activeRepublishPolicy", "m_maxObservedDispatchDepth")) {
        Require-Text $busSource $token "event dispatch guard"
    }

    Require-Text $objective "SandboxEventSubscriptionOptions::domainMutation(" `
        "ObjectiveSystem mutation policy"
    Require-Text $objective '"ObjectiveSystem"' `
        "ObjectiveSystem mutation owner"
    Require-Text $worldSource '"World.WaystoneGuardianDeath"' `
        "Waystone mutation owner"
    Require-Text $worldSource "SandboxEventRepublishPolicy::Bounded" `
        "Waystone bounded republish declaration"
    Require-Text $feedback "SandboxEventSubscriptionOptions::observer(" `
        "feedback observer policy"
    Require-Text $feedback '"ActionFeedbackTimeline"' `
        "feedback observer owner"
    Require-Text $audio 'observer("AudioRuntime")' `
        "audio observer declaration"

    foreach ($testId in @(
        "AL-A4/current-events-are-immutable-domain-facts",
        "AL-A4/observer-delivery-is-synchronous-and-declared",
        "AL-A4/observer-cannot-hide-nested-publication",
        "AL-A4/declared-domain-reaction-may-publish-bounded-fact",
        "AL-A4/recursive-publication-has-hard-depth-limit",
        "AL-A4/diagnostic-event-cannot-drive-domain-mutation",
        "AL-A4/subscription-membership-is-snapshotted-per-publication",
        "AL-A4/handler-exception-restores-dispatch-boundary")) {
        Require-Text $tests $testId "AL-A4 behavioral check"
    }

    $networkRoot = Join-Path $sourceRoot "Network"
    if (Test-Path -LiteralPath $networkRoot -PathType Container) {
        $networkText = (Get-ChildItem -LiteralPath $networkRoot -Recurse `
            -File -Include *.h,*.cpp | ForEach-Object {
                Get-Content -LiteralPath $_.FullName -Raw
            }) -join "`n"
        foreach ($forbidden in @("Ogre", "../Ogre/")) {
            Reject-Text $networkText $forbidden "Network renderer dependency"
        }
    }

    $machineRoot = Join-Path $sourceRoot "Machine"
    if (Test-Path -LiteralPath $machineRoot -PathType Container) {
        $machineText = (Get-ChildItem -LiteralPath $machineRoot -Recurse `
            -File -Include *.h,*.cpp | ForEach-Object {
                Get-Content -LiteralPath $_.FullName -Raw
            }) -join "`n"
        foreach ($forbidden in @("../UI/", "../Presentation/", "Ogre")) {
            Reject-Text $machineText $forbidden "Machine UI dependency"
        }
    }

    Write-Host (
        "[EVENT_COMMAND_QUERY_BOUNDARY] status=PASS command_fifo=typed " +
        "event_categories=2 max_dispatch_depth=8 " +
        "production_effect_owners=4")
    exit 0
}
catch {
    Write-Error (
        "[EVENT_COMMAND_QUERY_BOUNDARY] status=FAIL " +
        $_.Exception.Message)
    exit 1
}
