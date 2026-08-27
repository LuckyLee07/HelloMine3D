[CmdletBinding()]
param(
    [string]$RecordPath = "",
    [switch]$AllowNotRun,
    [switch]$RequirePass
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($RecordPath)) {
    $RecordPath = Join-Path $repoRoot `
        "docs\developer-visual-record-v10b3.txt"
}
if (-not [IO.Path]::IsPathRooted($RecordPath)) {
    $RecordPath = Join-Path $repoRoot $RecordPath
}
$RecordPath = [IO.Path]::GetFullPath($RecordPath)
if (-not (Test-Path -LiteralPath $RecordPath -PathType Leaf)) {
    throw "Developer visual record is missing: $RecordPath"
}

$requiredKeys = @(
    'record_version', 'kind', 'batch', 'date', 'commit', 'configuration',
    'gpu', 'driver', 'window', 'graphics', 'scenes', 'result', 'reason'
)
$fields = @{}
foreach ($line in Get-Content -LiteralPath $RecordPath -Encoding UTF8) {
    $trimmed = $line.Trim()
    if (-not $trimmed -or $trimmed.StartsWith('#')) {
        continue
    }
    $separator = $trimmed.IndexOf('=')
    if ($separator -le 0) {
        throw "Invalid developer visual record line: $trimmed"
    }
    $key = $trimmed.Substring(0, $separator)
    $value = $trimmed.Substring($separator + 1)
    if ($key -notin $requiredKeys) {
        throw "Unknown developer visual record key '$key'."
    }
    if ($fields.ContainsKey($key)) {
        throw "Duplicate developer visual record key '$key'."
    }
    if ([string]::IsNullOrWhiteSpace($value)) {
        throw "Developer visual record key '$key' is empty."
    }
    $fields[$key] = $value
}

$missing = @($requiredKeys | Where-Object {
    -not $fields.ContainsKey($_)
})
if ($missing.Count -ne 0) {
    throw "Developer visual record is missing keys: $($missing -join ', ')"
}
if ($fields['record_version'] -ne '1') {
    throw "Developer visual record_version must be 1."
}
if ($fields['kind'] -ne 'developer_visual') {
    throw "Developer visual kind must be developer_visual."
}
if ($fields['batch'] -notmatch '^V10(A|B1|B2|B3|C|D|E)$') {
    throw "Developer visual batch is unsupported: $($fields['batch'])"
}
if ($fields['commit'] -notmatch '^[0-9a-fA-F]{40}$') {
    throw "Developer visual commit must be a 40-hex Git identity."
}
if ($fields['configuration'] -ne 'Release') {
    throw "Developer visual configuration must be Release."
}
if ($fields['result'] -notin @('NOT_RUN', 'PASS', 'FAIL', 'BLOCKED')) {
    throw "Developer visual result must be NOT_RUN, PASS, FAIL or BLOCKED."
}
if ($fields['reason'].Length -gt 256) {
    throw "Developer visual reason exceeds 256 characters."
}
$scenes = @($fields['scenes'].Split(',') | ForEach-Object {
    $_.Trim()
} | Where-Object { $_ })
if ($scenes.Count -eq 0 -or $scenes.Count -gt 16 -or
    @($scenes | Where-Object {
        $_ -notmatch '^[a-z0-9][a-z0-9_-]*$'
    }).Count -ne 0) {
    throw "Developer visual scenes must contain 1-16 canonical ids."
}

$notRunFields = @('date', 'gpu', 'driver', 'window')
if ($fields['result'] -eq 'NOT_RUN') {
    if (-not $AllowNotRun -or $RequirePass) {
        throw "Developer visual record is NOT_RUN."
    }
    foreach ($key in $notRunFields) {
        if ($fields[$key] -ne 'NOT_RUN') {
            throw "NOT_RUN developer visual record must keep '$key=NOT_RUN'."
        }
    }
}
else {
    if ($fields['date'] -notmatch '^\d{4}-\d{2}-\d{2}$') {
        throw "Completed developer visual date must use YYYY-MM-DD."
    }
    foreach ($key in @('gpu', 'driver')) {
        if ($fields[$key] -eq 'NOT_RUN' -or $fields[$key].Length -gt 160) {
            throw "Completed developer visual '$key' is invalid."
        }
    }
    if ($fields['window'] -notmatch '^\d{3,5}x\d{3,5};fullscreen=[01]$') {
        throw "Completed developer visual window must be WIDTHxHEIGHT;fullscreen=0|1."
    }
}
if ($RequirePass -and $fields['result'] -ne 'PASS') {
    throw "Developer visual record result is '$($fields['result'])', not PASS."
}

Write-Host "[DEVELOPER_VISUAL_RECORD] status=PASS batch=$($fields['batch']) result=$($fields['result']) scenes=$($scenes.Count) path=$RecordPath"
