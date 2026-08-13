[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RecordPath,
    [string]$ExpectedCommit = "",
    [switch]$RequirePass,
    [switch]$AllowNotRun
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$resolved = (Resolve-Path -LiteralPath $RecordPath).Path
$values = @{}
foreach ($rawLine in Get-Content -LiteralPath $resolved -Encoding utf8) {
    $line = $rawLine.Trim()
    if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith("#")) {
        continue
    }
    $separator = $line.IndexOf("=")
    if ($separator -le 0) {
        throw "Malformed record line: $rawLine"
    }
    $key = $line.Substring(0, $separator).Trim()
    $value = $line.Substring($separator + 1).Trim()
    if ($values.ContainsKey($key)) {
        throw "Duplicate record key: $key"
    }
    if ([string]::IsNullOrWhiteSpace($value)) {
        throw "Empty record value: $key"
    }
    $values[$key] = $value
}

$cases = @(
    "focus_recovery",
    "wasd_movement",
    "mouse_look",
    "flight_sneak",
    "hotbar_numbers",
    "hotbar_wheel",
    "break_block",
    "attack_mob",
    "place_block",
    "container_use_transfer",
    "container_close",
    "window_close"
)
$required = @(
    "protocol_version", "date", "commit", "configuration", "gpu_driver",
    "window_mode", "window_size", "operator", "overall_result",
    "deviations"
) + @($cases | ForEach-Object { "case.$_" })

foreach ($key in $required) {
    if (-not $values.ContainsKey($key)) {
        throw "Missing record key: $key"
    }
}
$unexpected = @($values.Keys | Where-Object { $_ -notin $required })
if ($unexpected.Count -gt 0) {
    throw "Unexpected record keys: $($unexpected -join ', ')"
}
if ($values["protocol_version"] -ne "1") {
    throw "Unsupported protocol_version=$($values['protocol_version'])"
}

$isTemplate = $AllowNotRun -and $values["overall_result"] -eq "NOT_RUN"
if (-not $isTemplate) {
    $dateValue = [datetime]::MinValue
    if (-not [datetime]::TryParseExact(
            $values["date"], "yyyy-MM-dd",
            [System.Globalization.CultureInfo]::InvariantCulture,
            [System.Globalization.DateTimeStyles]::None,
            [ref]$dateValue)) {
        throw "date must use YYYY-MM-DD."
    }
    if ($values["commit"] -notmatch '^[0-9a-fA-F]{7,40}$') {
        throw "commit must be a 7-40 character hexadecimal Git id."
    }
}
if ($values["configuration"] -notin @("Debug", "Release")) {
    throw "configuration must be Debug or Release."
}
if ($values["window_mode"] -notin @("windowed", "fullscreen")) {
    throw "window_mode must be windowed or fullscreen."
}
if ($values["window_size"] -notmatch '^[1-9][0-9]*x[1-9][0-9]*$') {
    throw "window_size must use WIDTHxHEIGHT."
}
if (-not [string]::IsNullOrWhiteSpace($ExpectedCommit) -and
    $values["commit"] -ne $ExpectedCommit) {
    throw "Record commit '$($values['commit'])' does not match '$ExpectedCommit'."
}

$allowedCaseResults = @("PASS", "FAIL", "BLOCKED")
if ($AllowNotRun) {
    $allowedCaseResults += "NOT_RUN"
}
foreach ($case in $cases) {
    $key = "case.$case"
    if ($values[$key] -notin $allowedCaseResults) {
        throw "$key has invalid result '$($values[$key])'."
    }
}

$allowedOverallResults = @("PASS", "FAIL", "BLOCKED")
if ($AllowNotRun) {
    $allowedOverallResults += "NOT_RUN"
}
if ($values["overall_result"] -notin $allowedOverallResults) {
    throw "overall_result has invalid value '$($values['overall_result'])'."
}
if ($values["overall_result"] -eq "PASS") {
    $notPassing = @($cases | Where-Object {
        $values["case.$_"] -ne "PASS"
    })
    if ($notPassing.Count -gt 0) {
        throw "overall_result=PASS but cases are not PASS: $($notPassing -join ', ')"
    }
    if ($values["deviations"] -ne "none") {
        throw "overall_result=PASS requires deviations=none."
    }
}
if ($RequirePass -and $values["overall_result"] -ne "PASS") {
    throw "A passing R3 record is required; found '$($values['overall_result'])'."
}

Write-Host "[MANUAL_INPUT_RECORD] protocol=1 cases=$($cases.Count) overall=$($values['overall_result'])"
Write-Host "[MANUAL_INPUT_RECORD] status=PASS path=$resolved"

