[CmdletBinding()]
param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Get-Sha256Text {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $algorithm = [Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes($Value)
        return ([BitConverter]::ToString(
            $algorithm.ComputeHash($bytes))).Replace("-", "")
    }
    finally {
        $algorithm.Dispose()
    }
}

try {
    $worldHeader = Join-Path $Root "src\HelloMine3D\World\World.h"
    $architectureDocument = Join-Path $Root `
        "docs\current\architecture.md"
    if (-not (Test-Path -LiteralPath $worldHeader)) {
        throw "World header is missing: $worldHeader"
    }
    if (-not (Test-Path -LiteralPath $architectureDocument)) {
        throw "Architecture document is missing: $architectureDocument"
    }

    $source = [IO.File]::ReadAllText($worldHeader)
    $publicMatch = [regex]::Match(
        $source,
        '(?s)\n  public:\r?\n(.*?)\n  private:\r?\n')
    if (-not $publicMatch.Success) {
        throw "World.h public API boundary was not found."
    }

    $publicSurface = $publicMatch.Groups[1].Value -replace "`r`n", "`n"
    $publicSurface = $publicSurface -replace '[ \t]+(?=\n|$)', ''
    $publicSurface = $publicSurface.Trim()
    $actualHash = Get-Sha256Text -Value $publicSurface

    $document = [IO.File]::ReadAllText($architectureDocument)
    $hashMatch = [regex]::Match(
        $document,
        '<!-- AL-A1-WORLD-API-HASH sha256=([A-F0-9]{64}) -->')
    if (-not $hashMatch.Success) {
        throw "AL-A1 World API hash marker is missing or malformed."
    }
    $expectedHash = $hashMatch.Groups[1].Value
    if ($actualHash -cne $expectedHash) {
        throw "World.h public surface changed: expected $expectedHash, actual $actualHash. Update and review the AL-A1 responsibility map."
    }

    $mapMatch = [regex]::Match(
        $document,
        '(?s)<!-- AL-A1-WORLD-API-MAP-BEGIN -->(.*?)<!-- AL-A1-WORLD-API-MAP-END -->')
    if (-not $mapMatch.Success) {
        throw "AL-A1 World API map markers are missing."
    }

    $rowPattern = '(?m)^\| `(?<api>[^`]+)` \| `(?<concept>Query|Command|Runtime Tick)` \| `(?<responsibility>World Query|World Mutation|Simulation|Streaming|Persistence|Actor|Combat|Progression|Diagnostics)` \|'
    $rows = [regex]::Matches($mapMatch.Groups[1].Value, $rowPattern)
    if ($rows.Count -eq 0) {
        throw "AL-A1 World API map contains no machine-readable rows."
    }

    $mapped = New-Object `
        'System.Collections.Generic.HashSet[string]' `
        ([StringComparer]::Ordinal)
    $conceptCounts = @{
        "Query" = 0
        "Command" = 0
        "Runtime Tick" = 0
    }
    foreach ($row in $rows) {
        $api = $row.Groups['api'].Value
        if (-not $mapped.Add($api)) {
            throw "AL-A1 World API map duplicates '$api'."
        }
        $conceptCounts[$row.Groups['concept'].Value]++
    }

    $withoutLineComments = [regex]::Replace(
        $publicMatch.Groups[1].Value,
        '(?m)//[^\r\n]*',
        '')
    $declared = New-Object `
        'System.Collections.Generic.HashSet[string]' `
        ([StringComparer]::Ordinal)
    foreach ($lineMatch in [regex]::Matches(
        $withoutLineComments,
        '(?m)^ {4}(?! )([^\r\n]*\()')) {
        $identifiers = [regex]::Matches(
            $lineMatch.Groups[1].Value,
            '([A-Za-z_~][A-Za-z0-9_]*)\s*\(')
        if ($identifiers.Count -gt 0) {
            [void]$declared.Add(
                $identifiers[$identifiers.Count - 1].Groups[1].Value)
        }
    }

    $missing = @($declared | Where-Object { -not $mapped.Contains($_) } |
        Sort-Object)
    $stale = @($mapped | Where-Object { -not $declared.Contains($_) } |
        Sort-Object)
    if ($missing.Count -gt 0 -or $stale.Count -gt 0) {
        throw "World responsibility map mismatch. Missing=[$($missing -join ', ')]; stale=[$($stale -join ', ')]."
    }

    Write-Host (("[WORLD_API_MAP] status=PASS methods={0} queries={1} " +
        "commands={2} runtime_ticks={3} public_sha256={4}") -f `
        $declared.Count,
        $conceptCounts['Query'],
        $conceptCounts['Command'],
        $conceptCounts['Runtime Tick'],
        $actualHash)
}
catch {
    Write-Error "[WORLD_API_MAP] status=FAIL $($_.Exception.Message)"
    exit 1
}
