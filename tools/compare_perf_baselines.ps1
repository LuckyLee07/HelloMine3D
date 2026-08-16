param(
    [Parameter(Mandatory = $true)] [string]$Baseline,
    [Parameter(Mandatory = $true)] [string]$Candidate,
    [string]$Contract = "",
    [Nullable[double]]$FrameP95Percent = $null,
    [Nullable[double]]$FrameP99Percent = $null,
    [Nullable[double]]$ResidencyPercent = $null,
    [Nullable[double]]$LongFramePercent = $null
)

$ErrorActionPreference = "Stop"
$InvariantCulture = [System.Globalization.CultureInfo]::InvariantCulture
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($Contract)) {
    $Contract = Join-Path $ScriptRoot "performance-contract-v1.json"
}

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
    param([string]$Path)
    $values = @{}
    foreach ($rawLine in Get-Content -LiteralPath $Path -Encoding utf8) {
        $line = $rawLine.Trim()
        if ([string]::IsNullOrWhiteSpace($line) -or
            $line.StartsWith("#")) { continue }
        $separator = $line.IndexOf('=')
        if ($separator -le 0) {
            throw "Malformed summary line in '${Path}': $rawLine"
        }
        $key = $line.Substring(0, $separator).Trim()
        $value = $line.Substring($separator + 1).Trim()
        if ([string]::IsNullOrWhiteSpace($value)) {
            throw "Empty summary value '$key' in '$Path'."
        }
        if ($values.ContainsKey($key)) {
            throw "Duplicate summary key '$key' in '$Path'."
        }
        $values[$key] = $value
    }
    return $values
}

function Require-Text {
    param([hashtable]$Summary, [string]$Key, [string]$Label)
    if (-not $Summary.ContainsKey($Key) -or
        [string]::IsNullOrWhiteSpace($Summary[$Key])) {
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
        [ref]$value) -or
        [double]::IsNaN($value) -or
        [double]::IsInfinity($value) -or
        $value -lt 0.0) {
        throw "$Label summary has invalid numeric '$Key=$text'."
    }
    return $value
}

function Allowed-Delta {
    param([double]$Value, [double]$Percent, [double]$Minimum)
    return [Math]::Max(
        $Minimum,
        [Math]::Ceiling([Math]::Abs($Value) * $Percent / 100.0))
}

function Assert-Percentage {
    param([double]$Value, [string]$Label)
    if ([double]::IsNaN($Value) -or [double]::IsInfinity($Value) -or
        $Value -lt 0.0) {
        throw "$Label must be finite and non-negative."
    }
}

function Get-ContractArray {
    param([object]$Owner, [string]$Key, [string]$Label)
    $property = $Owner.PSObject.Properties[$Key]
    if ($null -eq $property) { return @() }
    $values = @($property.Value)
    $seen = @{}
    foreach ($value in $values) {
        if ($value -isnot [string] -or
            [string]::IsNullOrWhiteSpace($value)) {
            throw "Performance contract has invalid $Label.$Key."
        }
        if ($seen.ContainsKey($value)) {
            throw "Performance contract has duplicate $Label.$Key."
        }
        $seen[$value] = $true
    }
    return $values
}

function Get-ContractNumber {
    param([object]$Owner, [string]$Key, [string]$Label)
    $property = $Owner.PSObject.Properties[$Key]
    if ($null -eq $property) {
        throw "Performance contract is missing $Label.$Key."
    }
    $value = 0.0
    if (-not [double]::TryParse(
        [string]$property.Value,
        [System.Globalization.NumberStyles]::Float,
        $InvariantCulture,
        [ref]$value) -or
        [double]::IsNaN($value) -or
        [double]::IsInfinity($value) -or
        $value -lt 0.0) {
        throw "Performance contract has invalid $Label.$Key."
    }
    return $value
}

function Validate-ContractMetrics {
    param([hashtable]$Summary, [string]$Label, [object]$Scene)
    $values = @{}
    foreach ($key in @(Get-ContractArray `
        -Owner $Scene -Key "required_metrics" -Label "scene")) {
        $values[$key] = Require-Number `
            -Summary $Summary -Key $key -Label $Label
    }
    foreach ($key in @(Get-ContractArray `
        -Owner $Scene -Key "positive_metrics" -Label "scene")) {
        if (-not $values.ContainsKey($key)) {
            throw "Performance contract positive metric '$key' is not required."
        }
        if ($values[$key] -le 0.0) {
            throw "$Label summary requires positive '$key'."
        }
    }
    foreach ($key in @(Get-ContractArray `
        -Owner $Scene -Key "integer_metrics" -Label "scene")) {
        if (-not $values.ContainsKey($key)) {
            throw "Performance contract integer metric '$key' is not required."
        }
        if ([Math]::Truncate($values[$key]) -ne $values[$key]) {
            throw "$Label summary requires integer '$key'."
        }
    }

    $groupsProperty = $Scene.PSObject.Properties["monotonic_groups"]
    if ($null -ne $groupsProperty) {
        $groupIndex = 0
        foreach ($rawGroup in $groupsProperty.Value) {
            $group = @($rawGroup)
            if ($group.Count -lt 2) {
                throw "Performance contract has invalid monotonic group $groupIndex."
            }
            foreach ($key in $group) {
                if (-not $values.ContainsKey([string]$key)) {
                    throw "Performance contract monotonic metric '$key' is not required."
                }
            }
            for ($index = 1; $index -lt $group.Count; ++$index) {
                $earlier = [string]$group[$index - 1]
                $later = [string]$group[$index]
                if ($values[$later] -lt $values[$earlier]) {
                    throw "$Label summary has non-monotonic phase metrics '$earlier=$($values[$earlier])' then '$later=$($values[$later])'."
                }
            }
            ++$groupIndex
        }
    }

    $rangesProperty = $Scene.PSObject.Properties["metric_ranges"]
    if ($null -ne $rangesProperty) {
        foreach ($rangeProperty in $rangesProperty.Value.PSObject.Properties) {
            $key = $rangeProperty.Name
            if (-not $values.ContainsKey($key)) {
                throw "Performance contract range metric '$key' is not required."
            }
            $minimum = Get-ContractNumber `
                -Owner $rangeProperty.Value -Key "minimum" -Label "range.$key"
            $maximum = Get-ContractNumber `
                -Owner $rangeProperty.Value -Key "maximum" -Label "range.$key"
            if ($maximum -lt $minimum) {
                throw "Performance contract range '$key' has maximum below minimum."
            }
            if ($values[$key] -lt $minimum -or
                $values[$key] -gt $maximum) {
                throw "$Label summary metric '$key=$($values[$key])' is outside [$minimum, $maximum]."
            }
        }
    }
    return $values
}

function New-ComparisonResult {
    param([string]$Status, [string[]]$Reasons = @())
    return [PSCustomObject]@{ Status = $Status; Reasons = @($Reasons) }
}

function Compare-ContractScene {
    param(
        [hashtable]$BaselineSummary,
        [hashtable]$CandidateSummary,
        [object]$ContractObject
    )
    if ([string]$ContractObject.comparison_schema -cne "3") {
        throw "Performance contract has unsupported comparison_schema=$($ContractObject.comparison_schema)."
    }
    $contractVersion = [string]$ContractObject.contract_version
    if ([string]::IsNullOrWhiteSpace($contractVersion)) {
        throw "Performance contract has invalid contract_version."
    }

    $baselineSceneId = Require-Text `
        -Summary $BaselineSummary -Key "comparison_scene_id" -Label "Baseline"
    $candidateSceneId = Require-Text `
        -Summary $CandidateSummary -Key "comparison_scene_id" -Label "Candidate"
    if ($baselineSceneId -cne $candidateSceneId) {
        return New-ComparisonResult -Status "INCOMPARABLE" -Reasons @(
            "comparison_scene_id baseline='$baselineSceneId' candidate='$candidateSceneId'")
    }
    $sceneProperty = $ContractObject.scenes.PSObject.Properties[$baselineSceneId]
    if ($null -eq $sceneProperty) {
        throw "Performance contract does not define scene '$baselineSceneId'."
    }
    $scene = $sceneProperty.Value
    $commonRequired = @(Get-ContractArray `
        -Owner $ContractObject -Key "common_required_keys" -Label "contract")
    $commonIdentity = @(Get-ContractArray `
        -Owner $ContractObject -Key "common_identity_keys" -Label "contract")
    $sceneIdentity = @(Get-ContractArray `
        -Owner $scene -Key "identity_keys" -Label "scene")
    foreach ($key in @($commonRequired + $sceneIdentity)) {
        [void](Require-Text `
            -Summary $BaselineSummary -Key $key -Label "Baseline")
        [void](Require-Text `
            -Summary $CandidateSummary -Key $key -Label "Candidate")
    }
    foreach ($entry in @(
        @{ Label = "Baseline"; Summary = $BaselineSummary },
        @{ Label = "Candidate"; Summary = $CandidateSummary })) {
        $actualVersion = Require-Text `
            -Summary $entry.Summary `
            -Key "comparison_contract_version" `
            -Label $entry.Label
        if ($actualVersion -cne $contractVersion) {
            throw "$($entry.Label) summary has comparison_contract_version=$actualVersion, expected $contractVersion."
        }
    }

    $baselineMetrics = Validate-ContractMetrics `
        -Summary $BaselineSummary -Label "Baseline" -Scene $scene
    $candidateMetrics = Validate-ContractMetrics `
        -Summary $CandidateSummary -Label "Candidate" -Scene $scene
    $budgetLimits = @{}
    foreach ($key in @(Get-ContractArray `
        -Owner $scene -Key "budget_metrics" -Label "scene")) {
        if (-not $baselineMetrics.ContainsKey($key)) {
            throw "Performance contract budget metric '$key' is not required."
        }
        $limitKey = "budget_${key}_max"
        $limit = Require-Number `
            -Summary $BaselineSummary -Key $limitKey -Label "Baseline"
        if ($baselineMetrics[$key] -gt $limit) {
            throw "Baseline summary metric '$key=$($baselineMetrics[$key])' exceeds its approved '$limitKey=$limit'."
        }
        $budgetLimits[$key] = $limit
    }

    $incomparable = @()
    foreach ($key in @($commonIdentity + $sceneIdentity)) {
        $baselineValue = [string]$BaselineSummary[$key]
        $candidateValue = [string]$CandidateSummary[$key]
        if ($baselineValue -cne $candidateValue) {
            $incomparable += "$key baseline='$baselineValue' candidate='$candidateValue'"
        }
    }
    foreach ($key in @(Get-ContractArray `
        -Owner $scene -Key "exact_metrics" -Label "scene")) {
        if (-not $baselineMetrics.ContainsKey($key)) {
            throw "Performance contract exact metric '$key' is not required."
        }
        if ($baselineMetrics[$key] -ne $candidateMetrics[$key]) {
            $incomparable += "$key baseline=$($baselineMetrics[$key]) candidate=$($candidateMetrics[$key])"
        }
    }

    $selectedResidencyPercent = if ($null -eq $ResidencyPercent) {
        Get-ContractNumber `
            -Owner $ContractObject.residency_policy `
            -Key "percent" -Label "residency_policy"
    }
    else { [double]$ResidencyPercent }
    foreach ($key in @(Get-ContractArray `
        -Owner $scene -Key "residency_metrics" -Label "scene")) {
        $minimum = Get-ContractNumber `
            -Owner $ContractObject.residency_policy.metrics `
            -Key $key -Label "residency_policy.metrics"
        $allowed = Allowed-Delta `
            -Value $baselineMetrics[$key] `
            -Percent $selectedResidencyPercent `
            -Minimum $minimum
        $delta = [Math]::Abs(
            $candidateMetrics[$key] - $baselineMetrics[$key])
        if ($delta -gt $allowed) {
            $incomparable += "$key delta=$delta allowed=$allowed baseline=$($baselineMetrics[$key]) candidate=$($candidateMetrics[$key])"
        }
    }
    if ($incomparable.Count -gt 0) {
        return New-ComparisonResult `
            -Status "INCOMPARABLE" -Reasons $incomparable
    }

    $regressions = @()
    foreach ($key in $budgetLimits.Keys) {
        if ($candidateMetrics[$key] -gt $budgetLimits[$key]) {
            $regressions += "$key candidate=$($candidateMetrics[$key]) approved_limit=$($budgetLimits[$key])"
        }
    }
    if ($scene.PSObject.Properties["apply_frame_policy"] -and
        [bool]$scene.apply_frame_policy) {
        $selectedP95 = if ($null -eq $FrameP95Percent) {
            Get-ContractNumber `
                -Owner $ContractObject.frame_policy `
                -Key "p95_percent" -Label "frame_policy"
        }
        else { [double]$FrameP95Percent }
        $selectedP99 = if ($null -eq $FrameP99Percent) {
            Get-ContractNumber `
                -Owner $ContractObject.frame_policy `
                -Key "p99_percent" -Label "frame_policy"
        }
        else { [double]$FrameP99Percent }
        $selectedLong = if ($null -eq $LongFramePercent) {
            Get-ContractNumber `
                -Owner $ContractObject.frame_policy `
                -Key "long_frame_percent" -Label "frame_policy"
        }
        else { [double]$LongFramePercent }
        $longMinimum = Get-ContractNumber `
            -Owner $ContractObject.frame_policy `
            -Key "long_frame_minimum" -Label "frame_policy"
        foreach ($metric in @(
            @{ Key = "frame_p95_ms"; Percent = $selectedP95 },
            @{ Key = "frame_p99_ms"; Percent = $selectedP99 })) {
            $key = $metric.Key
            $limit = $baselineMetrics[$key] *
                (1.0 + $metric.Percent / 100.0)
            if ($candidateMetrics[$key] -gt $limit) {
                $regressions += "$key candidate=$($candidateMetrics[$key]) limit=$([Math]::Round($limit, 3))"
            }
        }
        $allowedLongIncrease = Allowed-Delta `
            -Value $baselineMetrics["frames"] `
            -Percent $selectedLong `
            -Minimum $longMinimum
        $longIncrease = $candidateMetrics["frames_over_50ms"] -
            $baselineMetrics["frames_over_50ms"]
        if ($longIncrease -gt $allowedLongIncrease) {
            $regressions += "frames_over_50ms increase=$longIncrease allowed=$allowedLongIncrease"
        }
    }
    if ($regressions.Count -gt 0) {
        return New-ComparisonResult -Status "REGRESSION" -Reasons $regressions
    }
    return New-ComparisonResult -Status "PASS"
}

try {
    foreach ($entry in @(
        @{ Value = $FrameP95Percent; Label = "FrameP95Percent" },
        @{ Value = $FrameP99Percent; Label = "FrameP99Percent" },
        @{ Value = $ResidencyPercent; Label = "ResidencyPercent" },
        @{ Value = $LongFramePercent; Label = "LongFramePercent" })) {
        if ($null -ne $entry.Value) {
            Assert-Percentage -Value $entry.Value -Label $entry.Label
        }
    }
    $baselinePath = Resolve-SummaryPath -Path $Baseline
    $candidatePath = Resolve-SummaryPath -Path $Candidate
    $baselineSummary = Read-Summary -Path $baselinePath
    $candidateSummary = Read-Summary -Path $candidatePath
    $schema = Require-Text `
        -Summary $baselineSummary -Key "comparison_schema" -Label "Baseline"
    $candidateSchema = Require-Text `
        -Summary $candidateSummary -Key "comparison_schema" -Label "Candidate"

    if ($schema -cne $candidateSchema) {
        $result = New-ComparisonResult -Status "INCOMPARABLE" -Reasons @(
            "comparison_schema baseline='$schema' candidate='$candidateSchema'")
    }
    elseif ($schema -ceq "3") {
        $contractObject = Get-Content `
            -LiteralPath $Contract -Raw -Encoding utf8 | ConvertFrom-Json
        $result = Compare-ContractScene `
            -BaselineSummary $baselineSummary `
            -CandidateSummary $candidateSummary `
            -ContractObject $contractObject
    }
    elseif ($schema -ceq "1" -or $schema -ceq "2") {
        $selectedP95 = if ($null -eq $FrameP95Percent) { 15.0 }
            else { [double]$FrameP95Percent }
        $selectedP99 = if ($null -eq $FrameP99Percent) { 20.0 }
            else { [double]$FrameP99Percent }
        $selectedResidency = if ($null -eq $ResidencyPercent) { 5.0 }
            else { [double]$ResidencyPercent }
        $selectedLong = if ($null -eq $LongFramePercent) { 0.5 }
            else { [double]$LongFramePercent }
        $identityKeys = @(
            "build_configuration", "comparison_scene_id",
            "comparison_vsync_regime", "comparison_window", "terrain_seed")
        if ($schema -ceq "2") {
            $identityKeys += @(
                "comparison_platform", "comparison_architecture",
                "comparison_build_id", "comparison_gpu", "comparison_driver",
                "comparison_fullscreen", "comparison_fov",
                "comparison_resource_manifest_sha256",
                "comparison_resource_packs", "comparison_world_fixture",
                "comparison_save_format", "comparison_storage_class",
                "comparison_render_distance")
        }
        $incomparable = @()
        foreach ($key in $identityKeys) {
            $baselineValue = Require-Text `
                -Summary $baselineSummary -Key $key -Label "Baseline"
            $candidateValue = Require-Text `
                -Summary $candidateSummary -Key $key -Label "Candidate"
            if ($baselineValue -cne $candidateValue) {
                $incomparable += "$key baseline='$baselineValue' candidate='$candidateValue'"
            }
        }
        foreach ($entry in @(
            @{ Key = "last_loaded_chunks"; Minimum = 2.0 },
            @{ Key = "last_sections"; Minimum = 8.0 })) {
            $baselineValue = Require-Number `
                -Summary $baselineSummary -Key $entry.Key -Label "Baseline"
            $candidateValue = Require-Number `
                -Summary $candidateSummary -Key $entry.Key -Label "Candidate"
            $allowed = Allowed-Delta `
                -Value $baselineValue -Percent $selectedResidency `
                -Minimum $entry.Minimum
            $delta = [Math]::Abs($candidateValue - $baselineValue)
            if ($delta -gt $allowed) {
                $incomparable += "$($entry.Key) delta=$delta allowed=$allowed baseline=$baselineValue candidate=$candidateValue"
            }
        }
        if ($incomparable.Count -gt 0) {
            $result = New-ComparisonResult `
                -Status "INCOMPARABLE" -Reasons $incomparable
        }
        else {
            $regressions = @()
            foreach ($metric in @(
                @{ Key = "frame_p95_ms"; Percent = $selectedP95 },
                @{ Key = "frame_p99_ms"; Percent = $selectedP99 })) {
                $baselineValue = Require-Number `
                    -Summary $baselineSummary -Key $metric.Key -Label "Baseline"
                $candidateValue = Require-Number `
                    -Summary $candidateSummary -Key $metric.Key -Label "Candidate"
                $limit = $baselineValue * (1.0 + $metric.Percent / 100.0)
                if ($candidateValue -gt $limit) {
                    $regressions += "$($metric.Key) candidate=$candidateValue limit=$([Math]::Round($limit, 3))"
                }
            }
            $baselineFrames = Require-Number `
                -Summary $baselineSummary -Key "frames" -Label "Baseline"
            $candidateFrames = Require-Number `
                -Summary $candidateSummary -Key "frames" -Label "Candidate"
            if ($baselineFrames -le 0.0 -or $candidateFrames -le 0.0) {
                throw "Both summaries must contain a positive frame count."
            }
            $baselineLong = Require-Number `
                -Summary $baselineSummary `
                -Key "frames_over_50ms" -Label "Baseline"
            $candidateLong = Require-Number `
                -Summary $candidateSummary `
                -Key "frames_over_50ms" -Label "Candidate"
            $allowedLong = Allowed-Delta `
                -Value $baselineFrames -Percent $selectedLong -Minimum 2.0
            $increase = $candidateLong - $baselineLong
            if ($increase -gt $allowedLong) {
                $regressions += "frames_over_50ms increase=$increase allowed=$allowedLong"
            }
            if ($regressions.Count -gt 0) {
                $result = New-ComparisonResult `
                    -Status "REGRESSION" -Reasons $regressions
            }
            else { $result = New-ComparisonResult -Status "PASS" }
        }
    }
    else { throw "Unsupported comparison_schema=$schema." }

    foreach ($reason in $result.Reasons) {
        Write-Host "[PERF_COMPARE] $($result.Status.ToLower())=$reason"
    }
    Write-Host "[PERF_COMPARE] status=$($result.Status) baseline=$baselinePath candidate=$candidatePath"
    $exitCodes = @{ PASS = 0; REGRESSION = 2; INCOMPARABLE = 3 }
    exit $exitCodes[$result.Status]
}
catch {
    Write-Host "[PERF_COMPARE] invalid=$($_.Exception.Message)"
    Write-Host "[PERF_COMPARE] status=INVALID"
    exit 4
}
