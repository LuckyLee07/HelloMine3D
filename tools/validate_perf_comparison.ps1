param()

$ErrorActionPreference = "Stop"
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Comparator = Join-Path $ScriptRoot "compare_perf_baselines.ps1"
$ContractPath = Join-Path $ScriptRoot "performance-contract-v1.json"
$FixtureRoot = Join-Path $ScriptRoot "fixtures\performance"
$InvariantCulture = [System.Globalization.CultureInfo]::InvariantCulture
$TempRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
    ("HelloMine3D-perf-contract-" + [Guid]::NewGuid().ToString("N"))
$script:CaseCount = 0

function Invoke-ComparisonCase {
    param(
        [string]$Name,
        [string]$Baseline,
        [string]$Candidate,
        [int]$ExpectedExit,
        [string]$ExpectedStatus
    )
    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File $Comparator -Baseline $Baseline -Candidate $Candidate 2>&1
    $exitCode = $LASTEXITCODE
    $text = ($output | Out-String)
    if ($exitCode -ne $ExpectedExit) {
        throw "Case '$Name' returned $exitCode instead of $ExpectedExit.`n$text"
    }
    if ($text -notmatch [regex]::Escape("status=$ExpectedStatus")) {
        throw "Case '$Name' did not report status=$ExpectedStatus.`n$text"
    }
    ++$script:CaseCount
    Write-Host "[PERF_COMPARE_VERIFY] case=$Name exit=$exitCode status=$ExpectedStatus"
}

function Read-SummaryValue {
    param([string]$Path, [string]$Key)
    $prefix = "$Key="
    $line = Get-Content -LiteralPath $Path -Encoding utf8 |
        Where-Object { $_.StartsWith($prefix) } |
        Select-Object -First 1
    if ($null -eq $line) {
        throw "Fixture '$Path' is missing '$Key'."
    }
    return $line.Substring($prefix.Length)
}

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
    if (-not $found) {
        throw "Fixture '$Path' is missing '$Key'."
    }
    Set-Content -LiteralPath $Path -Encoding utf8 -Value $lines
}

function Remove-SummaryKey {
    param([string]$Path, [string]$Key)
    $prefix = "$Key="
    $found = $false
    $lines = Get-Content -LiteralPath $Path -Encoding utf8 | Where-Object {
        if ($_.StartsWith($prefix)) {
            $found = $true
            return $false
        }
        return $true
    }
    if (-not $found) {
        throw "Fixture '$Path' is missing '$Key'."
    }
    Set-Content -LiteralPath $Path -Encoding utf8 -Value $lines
}

function Copy-Fixture {
    param([string]$Source, [string]$Name)
    $destination = Join-Path $TempRoot "$Name.summary.txt"
    Copy-Item -LiteralPath $Source -Destination $destination
    return $destination
}

New-Item -ItemType Directory -Path $TempRoot | Out-Null
try {
    $legacyCases = @(
        @("r1-pass", "l4-baseline.summary.txt", "l4-baseline.summary.txt", 0, "PASS"),
        @("r1-regression", "l4-baseline.summary.txt", "l4-regressed.summary.txt", 2, "REGRESSION"),
        @("r1-incomparable", "l4-baseline.summary.txt", "incomparable.summary.txt", 3, "INCOMPARABLE"),
        @("r1-invalid", "l4-baseline.summary.txt", "missing-metric.summary.txt", 4, "INVALID"),
        @("q1-schema2-pass", "q1-baseline.summary.txt", "q1-baseline.summary.txt", 0, "PASS"),
        @("q1-schema2-regression", "q1-baseline.summary.txt", "q1-regressed.summary.txt", 2, "REGRESSION"),
        @("q1-schema2-incomparable", "q1-baseline.summary.txt", "q1-incomparable.summary.txt", 3, "INCOMPARABLE"),
        @("q1-schema2-invalid", "q1-baseline.summary.txt", "q1-missing-identity.summary.txt", 4, "INVALID")
    )
    foreach ($case in $legacyCases) {
        Invoke-ComparisonCase `
            -Name $case[0] `
            -Baseline (Join-Path $FixtureRoot $case[1]) `
            -Candidate (Join-Path $FixtureRoot $case[2]) `
            -ExpectedExit $case[3] `
            -ExpectedStatus $case[4]
    }

    $contract = Get-Content -LiteralPath $ContractPath -Raw -Encoding utf8 |
        ConvertFrom-Json
    foreach ($sceneProperty in $contract.scenes.PSObject.Properties) {
        $sceneId = $sceneProperty.Name
        $scene = $sceneProperty.Value
        $baseline = Join-Path $FixtureRoot `
            "$sceneId.baseline.summary.txt"
        if (-not (Test-Path -LiteralPath $baseline -PathType Leaf)) {
            throw "Missing contract fixture: $baseline"
        }

        Invoke-ComparisonCase `
            -Name "$sceneId/pass" `
            -Baseline $baseline -Candidate $baseline `
            -ExpectedExit 0 -ExpectedStatus "PASS"

        $budgetMetric = [string](@($scene.budget_metrics)[0])
        $budgetKey = "budget_${budgetMetric}_max"
        $budgetLimitText = Read-SummaryValue `
            -Path $baseline -Key $budgetKey
        $budgetLimit = 0.0
        if (-not [double]::TryParse(
            $budgetLimitText,
            [System.Globalization.NumberStyles]::Float,
            $InvariantCulture,
            [ref]$budgetLimit)) {
            throw "Invalid fixture budget '$budgetKey=$budgetLimitText'."
        }
        $regressed = Copy-Fixture `
            -Source $baseline -Name "$sceneId-regressed"
        Set-SummaryValue `
            -Path $regressed -Key $budgetMetric `
            -Value (($budgetLimit + 1.0).ToString($InvariantCulture))
        Invoke-ComparisonCase `
            -Name "$sceneId/regression" `
            -Baseline $baseline -Candidate $regressed `
            -ExpectedExit 2 -ExpectedStatus "REGRESSION"

        $identityKey = [string](@($scene.identity_keys)[0])
        $incomparable = Copy-Fixture `
            -Source $baseline -Name "$sceneId-incomparable"
        Set-SummaryValue `
            -Path $incomparable -Key $identityKey -Value "fixture-changed"
        Invoke-ComparisonCase `
            -Name "$sceneId/incomparable" `
            -Baseline $baseline -Candidate $incomparable `
            -ExpectedExit 3 -ExpectedStatus "INCOMPARABLE"

        $requiredMetric = [string](@($scene.required_metrics)[0])
        $invalid = Copy-Fixture `
            -Source $baseline -Name "$sceneId-invalid"
        Remove-SummaryKey -Path $invalid -Key $requiredMetric
        Invoke-ComparisonCase `
            -Name "$sceneId/invalid" `
            -Baseline $baseline -Candidate $invalid `
            -ExpectedExit 4 -ExpectedStatus "INVALID"
    }

    $difficultyBaseline = Join-Path $FixtureRoot `
        "q1-scaled-gameplay-v1.baseline.summary.txt"
    $difficultyMismatch = Copy-Fixture `
        -Source $difficultyBaseline -Name "q1-difficulty-incomparable"
    Set-SummaryValue `
        -Path $difficultyMismatch -Key "difficulty_id" -Value "2"
    Invoke-ComparisonCase `
        -Name "q1-scaled-gameplay-v1/difficulty-incomparable" `
        -Baseline $difficultyBaseline -Candidate $difficultyMismatch `
        -ExpectedExit 3 -ExpectedStatus "INCOMPARABLE"

    $postVictoryMismatch = Copy-Fixture `
        -Source $difficultyBaseline -Name "q1-post-victory-incomparable"
    Set-SummaryValue `
        -Path $postVictoryMismatch `
        -Key "post_victory_completed_events" -Value "1"
    Invoke-ComparisonCase `
        -Name "q1-scaled-gameplay-v1/post-victory-incomparable" `
        -Baseline $difficultyBaseline -Candidate $postVictoryMismatch `
        -ExpectedExit 3 -ExpectedStatus "INCOMPARABLE"
}
finally {
    if (Test-Path -LiteralPath $TempRoot) {
        Remove-Item -LiteralPath $TempRoot -Recurse -Force
    }
}

Write-Host "[PERF_COMPARE_VERIFY] status=PASS cases=$script:CaseCount"
$global:LASTEXITCODE = 0
