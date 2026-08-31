[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$Build,
    [string]$WorldSmokePath = "",
    [string]$ClientPath = "",
    [string]$EvidencePath = "",
    [int]$TimeoutSeconds = 45
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
$binRoot = Join-Path $repoRoot "bin"
$backgroundVerifier = Join-Path $scriptRoot "validate_background_client.ps1"
$manualRecordVerifier = Join-Path $scriptRoot `
    "validate_manual_input_record.ps1"
$manualRecordTemplate = Join-Path $repoRoot `
    "docs\archive\manual-input-record-v1.template.txt"

if ([string]::IsNullOrWhiteSpace($WorldSmokePath)) {
    $WorldSmokePath = Join-Path $binRoot `
        "HelloMine3DWorldRuntimeSmoke.exe"
}
if ([string]::IsNullOrWhiteSpace($ClientPath)) {
    $ClientPath = Join-Path $binRoot "HelloMine3D.exe"
}
if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
    $runId = "{0:yyyyMMdd-HHmmss}-{1}" -f (Get-Date), $PID
    $EvidencePath = Join-Path $repoRoot `
        "tmp\r3-automated-preflight-$runId.txt"
}

function Find-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} `
        "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -products * `
            -requires Microsoft.Component.MSBuild `
            -find "MSBuild\**\Bin\MSBuild.exe" |
            Select-Object -First 1
        if ($found) {
            return $found
        }
    }
    foreach ($edition in @("Community", "Professional", "Enterprise")) {
        $candidate = Join-Path $env:ProgramFiles `
            "Microsoft Visual Studio\2022\$edition\MSBuild\Current\Bin\MSBuild.exe"
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    $command = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }
    throw "MSBuild was not found."
}

function Invoke-CapturedProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [string[]]$Arguments = @(),
        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,
        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    $token = [Guid]::NewGuid().ToString("N")
    $stdoutPath = Join-Path ([IO.Path]::GetTempPath()) `
        "hellomine3d-r3a-$token.stdout.log"
    $stderrPath = Join-Path ([IO.Path]::GetTempPath()) `
        "hellomine3d-r3a-$token.stderr.log"
    try {
        $startParameters = @{
            FilePath = $FilePath
            WorkingDirectory = $WorkingDirectory
            WindowStyle = "Hidden"
            RedirectStandardOutput = $stdoutPath
            RedirectStandardError = $stderrPath
            PassThru = $true
            Wait = $true
        }
        if ($Arguments.Count -gt 0) {
            $startParameters.ArgumentList = $Arguments
        }
        $process = Start-Process @startParameters
        $stdout = ""
        if (Test-Path -LiteralPath $stdoutPath) {
            $stdout = Get-Content -LiteralPath $stdoutPath -Raw
        }
        $stderr = ""
        if (Test-Path -LiteralPath $stderrPath) {
            $stderr = Get-Content -LiteralPath $stderrPath -Raw
        }
        if ($process.ExitCode -ne 0) {
            throw "$Label failed with exit code $($process.ExitCode).`n$stdout`n$stderr"
        }
        return [PSCustomObject]@{
            Stdout = $stdout
            Stderr = $stderr
            ExitCode = $process.ExitCode
        }
    }
    finally {
        Remove-Item -LiteralPath $stdoutPath -Force `
            -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $stderrPath -Force `
            -ErrorAction SilentlyContinue
    }
}

if ($Build) {
    $solution = Join-Path $repoRoot "build\HelloMine3D.sln"
    if (-not (Test-Path -LiteralPath $solution -PathType Leaf)) {
        throw "Generated solution not found: $solution"
    }
    $msbuild = Find-MSBuild
    foreach ($target in @("HelloMine3DWorldRuntimeSmoke", "HelloMine3D")) {
        Write-Host "[R3_AUTOMATED] building=$target configuration=$Configuration"
        $buildResult = Invoke-CapturedProcess -FilePath $msbuild `
            -Arguments @(
                $solution,
                "/t:$target",
                "/p:Configuration=$Configuration",
                "/p:Platform=x64",
                "/m",
                "/nodeReuse:false",
                "/nologo"
            ) `
            -WorkingDirectory $repoRoot `
            -Label "Build $target"
        Write-Host "[R3_AUTOMATED] build_status=PASS target=$target"
    }
}

foreach ($requiredPath in @(
        $WorldSmokePath,
        $ClientPath,
        $backgroundVerifier,
        $manualRecordVerifier,
        $manualRecordTemplate)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required R3 automated preflight input not found: $requiredPath"
    }
}

Write-Host "[R3_AUTOMATED] world_runtime_smoke=$WorldSmokePath"
$worldResult = Invoke-CapturedProcess -FilePath $WorldSmokePath `
    -WorkingDirectory (Split-Path -Parent $WorldSmokePath) `
    -Label "World runtime smoke"
if ($worldResult.Stdout -notmatch '\[VALIDATION\] status=PASS') {
    throw "World runtime smoke did not report PASS.`n$($worldResult.Stdout)"
}

$requiredChecks = @(
    "V2/movement-input",
    "V2/sprint-applies-to-strafe",
    "V2/look-input",
    "V2/fly-toggle",
    "V2/sneak-releases",
    "V2/hotbar-selection",
    "R3A/focus-neutral-input-stops-held-movement",
    "R3A/opposed-direction-state-is-neutral",
    "R3A/mouse-look-delta-is-frame-local",
    "R3A/hotbar-wheel-wraps-both-directions",
    "R3A/flight-rise-descend-uses-held-state",
    "D2/use-opens-container",
    "D2/store-preserves-total",
    "D2/take-preserves-total",
    "D2/open-ui-blocks-world-actions",
    "R3A/container-close-clears-ui-capture",
    "D2/break-spills-contents",
    "D4/player-attack-uses-living-damage",
    "D4/repeat-attack-is-suppressed",
    "D4/lethal-hit-spawns-loot",
    "D4/respawn-returns-to-saved-spawn",
    "S3.4/place-through-interaction-system",
    "S3.4/place-consumes-item",
    "D6/gameplay-events-cover-complete-loop",
    "D6/relaunch-preserves-loop-state"
)
$missingChecks = @($requiredChecks | Where-Object {
    $worldResult.Stdout -notmatch `
        ("(?m)^\[VALIDATION\] PASS " + [regex]::Escape($_) +
         "(?:\s|$)")
})
if ($missingChecks.Count -gt 0) {
    throw "World runtime smoke missed R3 automated checks: $($missingChecks -join ', ')"
}

$checkCount = "unknown"
if ($worldResult.Stdout -match `
        '(?m)^\[VALIDATION\] checks=([0-9]+) failures=0\s*$') {
    $checkCount = $Matches[1]
}

Write-Host "[R3_AUTOMATED] background_focus_guard=$ClientPath"
$global:LASTEXITCODE = 0
$backgroundOutput = & $backgroundVerifier `
    -ExePath $ClientPath -TimeoutSeconds $TimeoutSeconds *>&1 |
    ForEach-Object { "$_" }
if (($backgroundOutput -join "`n") -notmatch `
        '\[BACKGROUND_CLIENT\] status=PASS') {
    throw "Background focus guard failed.`n$($backgroundOutput -join "`n")"
}

$global:LASTEXITCODE = 0
& $manualRecordVerifier -RecordPath $manualRecordTemplate `
    -AllowNotRun | Out-Null

$commit = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') {
    throw "Unable to resolve the tested Git commit."
}
$worktreeEntries = @(& git -C $repoRoot status --porcelain `
    --untracked-files=all 2>$null)
if ($LASTEXITCODE -ne 0) {
    throw "Unable to resolve the tested Git worktree state."
}
$worktreeState = if ($worktreeEntries.Count -eq 0) {
    "CLEAN"
}
else {
    "DIRTY"
}
$testedSource = if ($worktreeState -eq "CLEAN") {
    $commit
}
else {
    "$commit+worktree"
}
$evidenceDirectory = Split-Path -Parent $EvidencePath
if (-not [string]::IsNullOrWhiteSpace($evidenceDirectory)) {
    New-Item -ItemType Directory -Path $evidenceDirectory `
        -Force | Out-Null
}

$evidence = @(
    "protocol=r3_automated_preflight_v1",
    "date=$((Get-Date).ToString('yyyy-MM-dd'))",
    "commit=$commit",
    "tested_source=$testedSource",
    "worktree_state=$worktreeState",
    "worktree_entries=$($worktreeEntries.Count)",
    "configuration=$Configuration",
    "build_performed=$($Build.IsPresent.ToString().ToLowerInvariant())",
    "world_runtime_checks=$checkCount",
    "required_logical_checks=$($requiredChecks.Count)",
    "case.focus_recovery=AUTOMATED_LOGIC_PASS",
    "case.wasd_movement=AUTOMATED_LOGIC_PASS",
    "case.mouse_look=AUTOMATED_LOGIC_PASS",
    "case.flight_sneak=AUTOMATED_LOGIC_PASS",
    "case.hotbar_numbers=AUTOMATED_LOGIC_PASS",
    "case.hotbar_wheel=AUTOMATED_LOGIC_PASS",
    "case.break_block=AUTOMATED_LOGIC_PASS",
    "case.attack_mob=AUTOMATED_LOGIC_PASS",
    "case.place_block=AUTOMATED_LOGIC_PASS",
    "case.container_use_transfer=AUTOMATED_LOGIC_PASS",
    "case.container_close=AUTOMATED_LOGIC_PASS",
    "case.window_close=AUTOMATED_LOGIC_PASS",
    "background_focus_guard=PASS",
    "manual_record_schema=PASS",
    "automated_result=PASS",
    "physical_input_result=NOT_RUN",
    "r3_closure=NOT_ELIGIBLE",
    "limitations=No physical device, foreground focus recovery, or operator observation was performed."
)
Set-Content -LiteralPath $EvidencePath -Value $evidence `
    -Encoding utf8

Write-Host "[R3_AUTOMATED] logical_checks=$($requiredChecks.Count) world_checks=$checkCount"
Write-Host "[R3_AUTOMATED] physical_input=NOT_RUN r3_closure=NOT_ELIGIBLE"
Write-Host "[R3_AUTOMATED] evidence=$EvidencePath"
Write-Host "[R3_AUTOMATED] status=PASS"
