param()

$ErrorActionPreference = "Stop"
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Comparator = Join-Path $ScriptRoot "compare_perf_baselines.ps1"
$FixtureRoot = Join-Path $ScriptRoot "fixtures\performance"
$Baseline = Join-Path $FixtureRoot "l4-baseline.summary.txt"

function Invoke-ComparisonCase {
    param(
        [string]$Name,
        [string]$Candidate,
        [int]$ExpectedExit,
        [string]$ExpectedStatus
    )

    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Comparator `
        -Baseline $Baseline -Candidate $Candidate 2>&1
    $exitCode = $LASTEXITCODE
    $text = ($output | Out-String)
    if ($exitCode -ne $ExpectedExit) {
        throw "Case '$Name' returned $exitCode instead of $ExpectedExit.`n$text"
    }
    if ($text -notmatch [regex]::Escape("status=$ExpectedStatus")) {
        throw "Case '$Name' did not report status=$ExpectedStatus.`n$text"
    }
    Write-Host "[PERF_COMPARE_VERIFY] case=$Name exit=$exitCode status=$ExpectedStatus"
}

Invoke-ComparisonCase -Name "real-l4-self" -Candidate $Baseline -ExpectedExit 0 -ExpectedStatus "PASS"
Invoke-ComparisonCase -Name "threshold-regression" `
    -Candidate (Join-Path $FixtureRoot "l4-regressed.summary.txt") `
    -ExpectedExit 2 -ExpectedStatus "REGRESSION"
Invoke-ComparisonCase -Name "incomparable-scene" `
    -Candidate (Join-Path $FixtureRoot "incomparable.summary.txt") `
    -ExpectedExit 3 -ExpectedStatus "INCOMPARABLE"
Invoke-ComparisonCase -Name "missing-metric" `
    -Candidate (Join-Path $FixtureRoot "missing-metric.summary.txt") `
    -ExpectedExit 4 -ExpectedStatus "INVALID"

Write-Host "[PERF_COMPARE_VERIFY] status=PASS cases=4"
$global:LASTEXITCODE = 0
