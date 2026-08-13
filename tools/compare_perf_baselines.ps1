param(
    [Parameter(Mandatory = $true)]
    [string]$Baseline,
    [Parameter(Mandatory = $true)]
    [string]$Candidate,
    [double]$FrameP95Percent = 15.0,
    [double]$FrameP99Percent = 20.0,
    [double]$ResidencyPercent = 5.0,
    [double]$LongFramePercent = 0.5
)

$ErrorActionPreference = "Stop"
$InvariantCulture = [System.Globalization.CultureInfo]::InvariantCulture

function Resolve-SummaryPath {
    param([string]$Path)

    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction Stop
    $item = Get-Item -LiteralPath $resolved.Path
    if ($item.PSIsContainer) {
        return (Resolve-Path -LiteralPath (Join-Path $item.FullName "summary.txt") -ErrorAction Stop).Path
    }
    return $item.FullName
}

function Read-Summary {
    param([string]$Path)

    $values = @{}
    foreach ($rawLine in Get-Content -LiteralPath $Path -Encoding utf8) {
        $line = $rawLine.Trim()
        if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith("#")) {
            continue
        }
        $separator = $line.IndexOf('=')
        if ($separator -le 0) {
            throw "Malformed summary line in '${Path}': $rawLine"
        }
        $key = $line.Substring(0, $separator).Trim()
        $value = $line.Substring($separator + 1).Trim()
        if ($values.ContainsKey($key)) {
            throw "Duplicate summary key '$key' in '$Path'."
        }
        $values[$key] = $value
    }
    return $values
}

function Require-Text {
    param([hashtable]$Summary, [string]$Key, [string]$Label)

    if (-not $Summary.ContainsKey($Key) -or [string]::IsNullOrWhiteSpace($Summary[$Key])) {
        throw "$Label summary is missing '$Key'."
    }
    return [string]$Summary[$Key]
}

function Require-Number {
    param([hashtable]$Summary, [string]$Key, [string]$Label)

    $text = Require-Text -Summary $Summary -Key $Key -Label $Label
    $value = 0.0
    if (-not [double]::TryParse(
        $text,
        [System.Globalization.NumberStyles]::Float,
        $InvariantCulture,
        [ref]$value)) {
        throw "$Label summary has invalid numeric '$Key=$text'."
    }
    return $value
}

function Allowed-Delta {
    param([double]$Value, [double]$Percent, [double]$Minimum)
    return [Math]::Max($Minimum, [Math]::Ceiling([Math]::Abs($Value) * $Percent / 100.0))
}

try {
    if ($FrameP95Percent -lt 0.0 -or $FrameP99Percent -lt 0.0 -or
        $ResidencyPercent -lt 0.0 -or $LongFramePercent -lt 0.0) {
        throw "Comparison percentages must be non-negative."
    }

    $baselinePath = Resolve-SummaryPath -Path $Baseline
    $candidatePath = Resolve-SummaryPath -Path $Candidate
    $baselineSummary = Read-Summary -Path $baselinePath
    $candidateSummary = Read-Summary -Path $candidatePath

    $identityKeys = @(
        "comparison_schema",
        "build_configuration",
        "comparison_scene_id",
        "comparison_vsync_regime",
        "comparison_window",
        "terrain_seed"
    )
    $incomparable = @()
    foreach ($key in $identityKeys) {
        $baselineValue = Require-Text -Summary $baselineSummary -Key $key -Label "Baseline"
        $candidateValue = Require-Text -Summary $candidateSummary -Key $key -Label "Candidate"
        if ($baselineValue -cne $candidateValue) {
            $incomparable += "$key baseline='$baselineValue' candidate='$candidateValue'"
        }
    }

    $residencyKeys = @(
        @{ Key = "last_loaded_chunks"; Minimum = 2.0 },
        @{ Key = "last_sections"; Minimum = 8.0 }
    )
    foreach ($entry in $residencyKeys) {
        $key = $entry.Key
        $baselineValue = Require-Number -Summary $baselineSummary -Key $key -Label "Baseline"
        $candidateValue = Require-Number -Summary $candidateSummary -Key $key -Label "Candidate"
        $allowed = Allowed-Delta -Value $baselineValue -Percent $ResidencyPercent -Minimum $entry.Minimum
        $delta = [Math]::Abs($candidateValue - $baselineValue)
        if ($delta -gt $allowed) {
            $incomparable += "$key delta=$delta allowed=$allowed baseline=$baselineValue candidate=$candidateValue"
        }
    }

    if ($incomparable.Count -gt 0) {
        foreach ($reason in $incomparable) {
            Write-Host "[PERF_COMPARE] incomparable=$reason"
        }
        Write-Host "[PERF_COMPARE] status=INCOMPARABLE baseline=$baselinePath candidate=$candidatePath"
        exit 3
    }

    $regressions = @()
    foreach ($metric in @(
        @{ Key = "frame_p95_ms"; Percent = $FrameP95Percent },
        @{ Key = "frame_p99_ms"; Percent = $FrameP99Percent }
    )) {
        $key = $metric.Key
        $baselineValue = Require-Number -Summary $baselineSummary -Key $key -Label "Baseline"
        $candidateValue = Require-Number -Summary $candidateSummary -Key $key -Label "Candidate"
        $limit = $baselineValue * (1.0 + $metric.Percent / 100.0)
        Write-Host ("[PERF_COMPARE] metric={0} baseline={1:F3} candidate={2:F3} limit={3:F3}" -f
            $key, $baselineValue, $candidateValue, $limit)
        if ($candidateValue -gt $limit) {
            $regressions += "$key candidate=$candidateValue limit=$([Math]::Round($limit, 3))"
        }
    }

    $baselineFrames = Require-Number -Summary $baselineSummary -Key "frames" -Label "Baseline"
    $candidateFrames = Require-Number -Summary $candidateSummary -Key "frames" -Label "Candidate"
    if ($baselineFrames -le 0.0 -or $candidateFrames -le 0.0) {
        throw "Both summaries must contain a positive frame count."
    }
    $baselineLongFrames = Require-Number -Summary $baselineSummary -Key "frames_over_50ms" -Label "Baseline"
    $candidateLongFrames = Require-Number -Summary $candidateSummary -Key "frames_over_50ms" -Label "Candidate"
    $allowedLongFrameIncrease = Allowed-Delta -Value $baselineFrames -Percent $LongFramePercent -Minimum 2.0
    $longFrameIncrease = $candidateLongFrames - $baselineLongFrames
    Write-Host "[PERF_COMPARE] metric=frames_over_50ms baseline=$baselineLongFrames candidate=$candidateLongFrames allowedIncrease=$allowedLongFrameIncrease"
    if ($longFrameIncrease -gt $allowedLongFrameIncrease) {
        $regressions += "frames_over_50ms increase=$longFrameIncrease allowed=$allowedLongFrameIncrease"
    }

    if ($regressions.Count -gt 0) {
        foreach ($reason in $regressions) {
            Write-Host "[PERF_COMPARE] regression=$reason"
        }
        Write-Host "[PERF_COMPARE] status=REGRESSION baseline=$baselinePath candidate=$candidatePath"
        exit 2
    }

    Write-Host "[PERF_COMPARE] status=PASS baseline=$baselinePath candidate=$candidatePath"
    exit 0
}
catch {
    Write-Host "[PERF_COMPARE] invalid=$($_.Exception.Message)"
    Write-Host "[PERF_COMPARE] status=INVALID"
    exit 4
}
