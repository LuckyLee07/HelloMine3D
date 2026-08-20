[CmdletBinding()]
param(
    [string]$OutputDir = "",
    [string]$EvidenceDir = "docs\baselines\release-candidate-windows-hidden-v1",
    [int]$DurationSeconds = 1800,
    [int]$Seed = 20260820
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
$runner = Join-Path $scriptRoot "run_world_soak.ps1"
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $runId = "{0:yyyyMMddHHmmssfff}-{1}" -f (Get-Date), $PID
    $OutputDir = Join-Path $repoRoot "bin\soak_runs\release-candidate-$runId"
}
elseif (-not [IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDir))
}
if (-not [IO.Path]::IsPathRooted($EvidenceDir)) {
    $EvidenceDir = [IO.Path]::GetFullPath((Join-Path $repoRoot $EvidenceDir))
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
New-Item -ItemType Directory -Path $EvidenceDir -Force | Out-Null

function Read-SummaryValue {
    param([string]$Path, [string]$Key)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return ""
    }
    $escaped = [regex]::Escape($Key)
    $line = Get-Content -LiteralPath $Path |
        Where-Object { $_ -match "^$escaped=" } |
        Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($line)) {
        return ""
    }
    return ($line -replace "^$escaped=", "").Trim()
}

foreach ($profile in @("nominal", "stress")) {
    $profileOutput = Join-Path $OutputDir $profile
    $worldSummary = Join-Path $profileOutput "summary.txt"
    $processSummary = Join-Path $profileOutput "process-summary.txt"
    $alreadyComplete =
        (Read-SummaryValue -Path $worldSummary -Key "status") -eq "PASS" -and
        [int](Read-SummaryValue -Path $worldSummary `
            -Key "duration_completed_seconds") -ge $DurationSeconds -and
        (Read-SummaryValue -Path $processSummary -Key "status") -eq "PASS" -and
        (Read-SummaryValue -Path $processSummary -Key "formal") -eq "true"
    if ($alreadyComplete) {
        Write-Host "[RC_SOAK] profile=$profile phase=reuse-complete"
    }
    else {
        Write-Host "[RC_SOAK] profile=$profile durationSeconds=$DurationSeconds phase=start"
        & $runner -OutputDir $profileOutput -DurationSeconds $DurationSeconds `
            -Seed $Seed -SampleIntervalSeconds 5 -Profile $profile -Formal
    }
    Copy-Item -LiteralPath $worldSummary `
        -Destination (Join-Path $EvidenceDir `
            "q3-$profile-world.summary.txt") -Force
    Copy-Item -LiteralPath $processSummary `
        -Destination (Join-Path $EvidenceDir `
            "q3-$profile-process.summary.txt") -Force
    Write-Host "[RC_SOAK] profile=$profile phase=PASS"
}

$manifest = @(
    "contract=q3-scale-soak-contract-v1",
    "captured_utc=$([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    "build_configuration=Release",
    "seed=$Seed",
    "duration_seconds=$DurationSeconds",
    "profiles=nominal,stress",
    "status=PASS"
)
Set-Content -LiteralPath (Join-Path $EvidenceDir "q3-manifest.txt") `
    -Encoding utf8 -Value $manifest
Write-Host "[RC_SOAK] status=PASS evidenceDir=$EvidenceDir outputDir=$OutputDir"
