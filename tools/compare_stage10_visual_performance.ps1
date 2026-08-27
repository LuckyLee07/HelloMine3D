[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string]$Baseline,
    [Parameter(Mandatory = $true)] [string]$Candidate,
    [ValidateRange(0.0, 1000.0)] [double]$ThresholdPercent = 10.0,
    [string]$ReportPath = "",
    [switch]$AllowResourceManifestChange
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$invariant = [System.Globalization.CultureInfo]::InvariantCulture
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$coreComparator = Join-Path $scriptRoot "compare_perf_baselines.ps1"

function Resolve-SummaryPath {
    param([string]$Path)
    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction Stop
    $item = Get-Item -LiteralPath $resolved.Path
    if ($item.PSIsContainer) {
        return (Resolve-Path -LiteralPath `
            (Join-Path $item.FullName "summary.txt") `
            -ErrorAction Stop).Path
    }
    return $item.FullName
}

function Read-Summary {
    param([string]$Path, [string]$Label)
    $values = @{}
    $lineNumber = 0
    foreach ($rawLine in Get-Content -LiteralPath $Path -Encoding utf8) {
        ++$lineNumber
        $line = $rawLine.Trim()
        if ([string]::IsNullOrWhiteSpace($line) -or
            $line.StartsWith("#")) { continue }
        $separator = $line.IndexOf('=')
        if ($separator -le 0) {
            throw "$Label summary has malformed line $lineNumber."
        }
        $key = $line.Substring(0, $separator).Trim()
        $value = $line.Substring($separator + 1).Trim()
        if ([string]::IsNullOrWhiteSpace($value) -or
            $values.ContainsKey($key)) {
            throw "$Label summary has empty or duplicate '$key'."
        }
        $values[$key] = $value
    }
    return $values
}

function Require-Text {
    param([hashtable]$Summary, [string]$Key, [string]$Label)
    if (-not $Summary.ContainsKey($Key) -or
        [string]::IsNullOrWhiteSpace([string]$Summary[$Key])) {
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
        $invariant,
        [ref]$value) -or
        [double]::IsNaN($value) -or
        [double]::IsInfinity($value) -or $value -lt 0.0) {
        throw "$Label summary has invalid numeric '$Key=$text'."
    }
    return $value
}

function Write-SummaryWithValue {
    param(
        [string]$Source,
        [string]$Destination,
        [string]$Key,
        [string]$Value
    )
    $prefix = "$Key="
    $found = $false
    $lines = Get-Content -LiteralPath $Source -Encoding utf8 |
        ForEach-Object {
            if ($_.StartsWith($prefix)) {
                $found = $true
                "$prefix$Value"
            }
            else { $_ }
        }
    if (-not $found) {
        throw "Summary bridge source is missing '$Key'."
    }
    Set-Content -LiteralPath $Destination -Encoding utf8 -Value $lines
}

$bridgeTempRoot = ""
try {
    $baselinePath = Resolve-SummaryPath -Path $Baseline
    $candidatePath = Resolve-SummaryPath -Path $Candidate

    $baselineSummary = Read-Summary -Path $baselinePath -Label "Baseline"
    $candidateSummary = Read-Summary -Path $candidatePath -Label "Candidate"
    $baselineManifest = Require-Text -Summary $baselineSummary `
        -Key "comparison_resource_manifest_sha256" -Label "Baseline"
    $candidateManifest = Require-Text -Summary $candidateSummary `
        -Key "comparison_resource_manifest_sha256" -Label "Candidate"
    $coreCandidatePath = $candidatePath
    $resourceManifestBridge = "not-required"
    if ($baselineManifest -cne $candidateManifest -and
        $AllowResourceManifestChange) {
        foreach ($entry in @(
            @{ Label = "Baseline"; Value = $baselineManifest },
            @{ Label = "Candidate"; Value = $candidateManifest })) {
            if ($entry.Value -notmatch '^[0-9a-fA-F]{64}$') {
                throw "$($entry.Label) resource manifest hash is invalid."
            }
        }
        $bridgeTempRoot = Join-Path ([IO.Path]::GetTempPath()) `
            ("HelloMine3D-stage10-manifest-bridge-" +
             [Guid]::NewGuid().ToString("N"))
        New-Item -ItemType Directory -Path $bridgeTempRoot | Out-Null
        $coreCandidatePath = Join-Path $bridgeTempRoot "candidate.summary.txt"
        Write-SummaryWithValue -Source $candidatePath `
            -Destination $coreCandidatePath `
            -Key "comparison_resource_manifest_sha256" `
            -Value $baselineManifest
        $resourceManifestBridge = "allowed"
    }

    $coreOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File $coreComparator -Baseline $baselinePath `
        -Candidate $coreCandidatePath 2>&1
    $coreExit = $LASTEXITCODE
    $coreOutput | Write-Host
    if ($coreExit -ne 0) {
        exit $coreExit
    }

    foreach ($entry in @(
        @{ Label = "Baseline"; Summary = $baselineSummary },
        @{ Label = "Candidate"; Summary = $candidateSummary })) {
        if ((Require-Text -Summary $entry.Summary `
                -Key "comparison_schema" -Label $entry.Label) -cne "3") {
            throw "$($entry.Label) summary must use comparison_schema=3."
        }
    }

    $scene = Require-Text -Summary $baselineSummary `
        -Key "comparison_scene_id" -Label "Baseline"
    if ($scene -notin @(
        "q1-fast-streaming-v1", "q1-scaled-gameplay-v1")) {
        throw "Stage 10 supplement does not support scene '$scene'."
    }

    $metrics = @(
        "frame_p95_ms",
        "frame_p99_ms",
        "last_mesh_build_avg_ms",
        "last_mesh_build_max_ms",
        "last_solid_vertices",
        "last_transparent_vertices",
        "last_water_vertices",
        "last_flora_vertices",
        "last_resident_terrain_vertices",
        "last_resident_terrain_indices",
        "last_resident_terrain_buffer_bytes"
    )
    if ($scene -ceq "q1-fast-streaming-v1") {
        $metrics += @("chunk_visible_p95_ms", "chunk_visible_p99_ms")
    }

    $report = @(
        "stage10_performance_supplement=1",
        "comparison_scene_id=$scene",
        "threshold_percent=$($ThresholdPercent.ToString($invariant))",
        "resource_manifest_bridge=$resourceManifestBridge",
        "baseline_resource_manifest_sha256=$baselineManifest",
        "candidate_resource_manifest_sha256=$candidateManifest"
    )
    $review = @()
    foreach ($metric in $metrics) {
        $baselineValue = Require-Number `
            -Summary $baselineSummary -Key $metric -Label "Baseline"
        $candidateValue = Require-Number `
            -Summary $candidateSummary -Key $metric -Label "Candidate"
        $limit = $baselineValue * (1.0 + $ThresholdPercent / 100.0)
        $deltaPercent = if ($baselineValue -eq 0.0) {
            if ($candidateValue -eq 0.0) { 0.0 } else { [double]::PositiveInfinity }
        }
        else {
            ($candidateValue - $baselineValue) / $baselineValue * 100.0
        }
        $metricStatus = if ($candidateValue -le $limit) {
            "PASS"
        }
        else {
            $review += $metric
            "REVIEW_REQUIRED"
        }
        $deltaText = if ([double]::IsPositiveInfinity($deltaPercent)) {
            "inf"
        }
        else { $deltaPercent.ToString("0.###", $invariant) }
        $line = "metric=$metric baseline=$($baselineValue.ToString('0.###', $invariant)) candidate=$($candidateValue.ToString('0.###', $invariant)) delta_percent=$deltaText status=$metricStatus"
        $report += $line
        Write-Host "[STAGE10_PERF] $line"
    }

    foreach ($stride in @(
        @{ Key = "terrain_vertex_stride_bytes"; Expected = 32.0 },
        @{ Key = "terrain_index_stride_bytes"; Expected = 4.0 })) {
        $baselineValue = Require-Number -Summary $baselineSummary `
            -Key $stride.Key -Label "Baseline"
        $candidateValue = Require-Number -Summary $candidateSummary `
            -Key $stride.Key -Label "Candidate"
        if ($baselineValue -ne $stride.Expected -or
            $candidateValue -ne $stride.Expected) {
            $review += $stride.Key
        }
    }

    $status = if ($review.Count -eq 0) { "PASS" }
        else { "REVIEW_REQUIRED" }
    $report += "status=$status"
    $report += "review_metrics=$($review -join ',')"
    if (-not [string]::IsNullOrWhiteSpace($ReportPath)) {
        $parent = Split-Path -Parent $ReportPath
        if (-not [string]::IsNullOrWhiteSpace($parent)) {
            New-Item -ItemType Directory -Path $parent -Force | Out-Null
        }
        Set-Content -LiteralPath $ReportPath -Encoding utf8 -Value $report
    }
    Write-Host "[STAGE10_PERF] status=$status scene=$scene threshold_percent=$ThresholdPercent"
    exit $(if ($status -ceq "PASS") { 0 } else { 2 })
}
catch {
    Write-Host "[STAGE10_PERF] invalid=$($_.Exception.Message)"
    Write-Host "[STAGE10_PERF] status=INVALID"
    exit 4
}
finally {
    if (-not [string]::IsNullOrWhiteSpace($bridgeTempRoot) -and
        (Test-Path -LiteralPath $bridgeTempRoot)) {
        Remove-Item -LiteralPath $bridgeTempRoot -Recurse -Force
    }
}
