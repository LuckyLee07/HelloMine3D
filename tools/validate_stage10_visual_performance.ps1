[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
$comparator = Join-Path $scriptRoot `
    "compare_stage10_visual_performance.ps1"
$source = Join-Path $repoRoot `
    "docs\baselines\release-candidate-windows-hidden-v1\q1-fast-streaming-v1.baseline.summary.txt"
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) `
    ("HelloMine3D-stage10-perf-" + [Guid]::NewGuid().ToString("N"))
$script:caseCount = 0

function Set-SummaryValue {
    param([string]$Path, [string]$Key, [string]$Value)
    $prefix = "$Key="
    $found = $false
    $lines = Get-Content -LiteralPath $Path -Encoding utf8 | ForEach-Object {
        if ($_.StartsWith($prefix)) {
            $found = $true
            "$prefix$Value"
        }
        else { $_ }
    }
    if (-not $found) { throw "Fixture is missing '$Key'." }
    Set-Content -LiteralPath $Path -Encoding utf8 -Value $lines
}

function Remove-SummaryKey {
    param([string]$Path, [string]$Key)
    $prefix = "$Key="
    $lines = Get-Content -LiteralPath $Path -Encoding utf8 |
        Where-Object { -not $_.StartsWith($prefix) }
    Set-Content -LiteralPath $Path -Encoding utf8 -Value $lines
}

function Invoke-Case {
    param(
        [string]$Name,
        [string]$Candidate,
        [int]$ExpectedExit,
        [string]$ExpectedStatus
    )
    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File $comparator -Baseline $source -Candidate $Candidate 2>&1
    $exitCode = $LASTEXITCODE
    $text = $output | Out-String
    if ($exitCode -ne $ExpectedExit -or
        $text -notmatch [regex]::Escape("status=$ExpectedStatus")) {
        throw "Case '$Name' failed: exit=$exitCode expected=$ExpectedExit/$ExpectedStatus`n$text"
    }
    ++$script:caseCount
    Write-Host "[STAGE10_PERF_VERIFY] case=$Name exit=$exitCode status=$ExpectedStatus"
}

New-Item -ItemType Directory -Path $tempRoot | Out-Null
try {
    $pass = Join-Path $tempRoot "pass.summary.txt"
    Copy-Item -LiteralPath $source -Destination $pass
    Invoke-Case -Name "unchanged" -Candidate $pass `
        -ExpectedExit 0 -ExpectedStatus "PASS"

    $within = Join-Path $tempRoot "within.summary.txt"
    Copy-Item -LiteralPath $source -Destination $within
    Set-SummaryValue -Path $within -Key "last_mesh_build_avg_ms" `
        -Value "0.589"
    Invoke-Case -Name "within-ten-percent" -Candidate $within `
        -ExpectedExit 0 -ExpectedStatus "PASS"

    $review = Join-Path $tempRoot "review.summary.txt"
    Copy-Item -LiteralPath $source -Destination $review
    Set-SummaryValue -Path $review -Key "last_mesh_build_avg_ms" `
        -Value "0.610"
    Invoke-Case -Name "above-ten-percent" -Candidate $review `
        -ExpectedExit 2 -ExpectedStatus "REVIEW_REQUIRED"

    $stride = Join-Path $tempRoot "stride.summary.txt"
    Copy-Item -LiteralPath $source -Destination $stride
    Set-SummaryValue -Path $stride -Key "terrain_vertex_stride_bytes" `
        -Value "36"
    Invoke-Case -Name "vertex-format-change" -Candidate $stride `
        -ExpectedExit 2 -ExpectedStatus "REVIEW_REQUIRED"

    $missing = Join-Path $tempRoot "missing.summary.txt"
    Copy-Item -LiteralPath $source -Destination $missing
    Remove-SummaryKey -Path $missing -Key "last_solid_vertices"
    Invoke-Case -Name "missing-supplement" -Candidate $missing `
        -ExpectedExit 4 -ExpectedStatus "INVALID"
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}

Write-Host "[STAGE10_PERF_VERIFY] status=PASS cases=$script:caseCount"
$global:LASTEXITCODE = 0
