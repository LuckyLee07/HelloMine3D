[CmdletBinding()]
param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot),
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Require-Text {
    param(
        [Parameter(Mandatory = $true)] [string]$Text,
        [Parameter(Mandatory = $true)] [string]$Needle,
        [Parameter(Mandatory = $true)] [string]$Label
    )
    if ($Text.IndexOf($Needle, [StringComparison]::Ordinal) -lt 0) {
        throw "Missing $Label ('$Needle')."
    }
}

function Get-TutorialManifestRows {
    param([Parameter(Mandatory = $true)] [string]$Tutorial)

    $begin = "<!-- ARCHITECTURE-LAB-TUTORIAL-MANIFEST-BEGIN -->"
    $end = "<!-- ARCHITECTURE-LAB-TUTORIAL-MANIFEST-END -->"
    $beginIndex = $Tutorial.IndexOf($begin, [StringComparison]::Ordinal)
    $endIndex = $Tutorial.IndexOf($end, [StringComparison]::Ordinal)
    if ($beginIndex -lt 0 -or $endIndex -lt 0 -or $endIndex -le $beginIndex) {
        throw "Tutorial manifest markers are missing or reordered."
    }
    if ($Tutorial.IndexOf($begin, $beginIndex + $begin.Length,
                         [StringComparison]::Ordinal) -ge 0 -or
        $Tutorial.IndexOf($end, $endIndex + $end.Length,
                         [StringComparison]::Ordinal) -ge 0) {
        throw "Tutorial manifest markers must appear exactly once."
    }

    $bodyStart = $beginIndex + $begin.Length
    $body = $Tutorial.Substring($bodyStart, $endIndex - $bodyStart)
    $rows = @()
    foreach ($rawLine in ($body -split "`r?`n")) {
        $line = $rawLine.Trim()
        if ($line.Length -eq 0) {
            continue
        }
        $match = [regex]::Match(
            $line,
            '^(?<batch>AL-A[0-9]+|B[0-9]+|C[0-9]+|D[0-9]+)\|(?<part>[0-9]{2})\|(?<section>[0-9]+\.[0-9]+)\|(?<evidence>[^|]+)$')
        if (-not $match.Success) {
            throw "Malformed tutorial manifest row: $line"
        }
        $rows += [pscustomobject]@{
            Batch = $match.Groups["batch"].Value
            Part = $match.Groups["part"].Value
            Section = $match.Groups["section"].Value
            Evidence = $match.Groups["evidence"].Value.Trim()
        }
    }
    if ($rows.Count -eq 0) {
        throw "Tutorial manifest contains no implemented batch."
    }
    return $rows
}

function Test-TutorialDocument {
    param(
        [Parameter(Mandatory = $true)] [string]$Tutorial,
        [Parameter(Mandatory = $true)] [string]$Ledger,
        [Parameter(Mandatory = $true)] [string]$Roadmap,
        [Parameter(Mandatory = $true)] [string]$RepositoryRoot
    )

    $rows = @(Get-TutorialManifestRows -Tutorial $Tutorial)
    $batchIds = @{}
    foreach ($row in $rows) {
        if ($batchIds.ContainsKey($row.Batch)) {
            throw "Duplicate tutorial manifest batch: $($row.Batch)"
        }
        $batchIds[$row.Batch] = $true
    }
    foreach ($required in @(
        "AL-A0", "AL-A1", "AL-A2", "AL-A3", "AL-A4", "AL-A5", "AL-A6")) {
        if (-not $batchIds.ContainsKey($required)) {
            throw "Missing implemented tutorial batch: $required"
        }
    }

    $rootPath = [IO.Path]::GetFullPath($RepositoryRoot)
    $rootPrefix = $rootPath.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    foreach ($row in $rows) {
        if ([IO.Path]::IsPathRooted($row.Evidence)) {
            throw "Manifest evidence must be repository-relative: $($row.Evidence)"
        }
        $evidencePath = [IO.Path]::GetFullPath(
            (Join-Path $rootPath ($row.Evidence -replace '/', '\')))
        if (-not $evidencePath.StartsWith(
            $rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Manifest evidence escapes the repository: $($row.Evidence)"
        }
        if (-not (Test-Path -LiteralPath $evidencePath -PathType Leaf)) {
            throw "Manifest evidence is missing: $($row.Evidence)"
        }

        $ledgerPattern = ('(?m)^\|\s*`' + [regex]::Escape($row.Batch) +
            '`[^|]*\|\s*`(?:Engineering Done|Done)`\s*\|')
        if (-not [regex]::IsMatch($Ledger, $ledgerPattern)) {
            throw "Manifest batch is not completed in the current ledger: $($row.Batch)"
        }
    }

    $partMatches = [regex]::Matches(
        $Tutorial, '(?m)^## Part (?<part>[0-9]{2})\b')
    $parts = @{}
    foreach ($match in $partMatches) {
        $part = $match.Groups["part"].Value
        if ($parts.ContainsKey($part)) {
            throw "Duplicate tutorial Part heading: $part"
        }
        $parts[$part] = $true
    }
    foreach ($row in $rows) {
        if (-not $parts.ContainsKey($row.Part)) {
            throw "Manifest Part has no heading: $($row.Part)"
        }
    }
    foreach ($part in $parts.Keys) {
        $hasManifestRow = @($rows | Where-Object { $_.Part -eq $part }).Count -gt 0
        if (-not $hasManifestRow) {
            throw "Tutorial Part $part has no implemented manifest row."
        }
    }

    $checkedSections = @{}
    $logicalHeadings = @(
        "Problem", "Naive Solution", "Failure", "Design Evolution",
        "Implementation", "Validation", "Trade-offs")
    foreach ($row in $rows) {
        if ($checkedSections.ContainsKey($row.Section)) {
            continue
        }
        $checkedSections[$row.Section] = $true
        $sectionPattern = ('(?ms)^###\s+' +
            [regex]::Escape($row.Section) +
            '\b.*?(?=^###\s+[0-9]+\.[0-9]+\b|^##\s+Part\s+|\z)')
        $sectionMatch = [regex]::Match($Tutorial, $sectionPattern)
        if (-not $sectionMatch.Success) {
            throw "Manifest section has no tutorial heading: $($row.Section)"
        }
        $sectionText = $sectionMatch.Value
        foreach ($heading in $logicalHeadings) {
            $headingPattern = ('(?ms)^####\s+' +
                [regex]::Escape($heading) +
                '\s*\r?\n(?<body>.*?)(?=^####\s+|\z)')
            $headingMatch = [regex]::Match($sectionText, $headingPattern)
            if (-not $headingMatch.Success) {
                throw "Section $($row.Section) is missing logical heading: $heading"
            }
            $body = $headingMatch.Groups["body"].Value.Trim()
            if ($body.Length -lt 40) {
                throw "Section $($row.Section) has an empty or placeholder heading: $heading"
            }
        }
    }

    foreach ($roadmapRule in @(
        "docs/current/architecture-lab-tutorial.md",
        "Extended",
        "Core")) {
        Require-Text $Roadmap $roadmapRule "Architecture Lab roadmap rule"
    }

    return [pscustomobject]@{
        Batches = $rows.Count
        Parts = $parts.Count
        Sections = $checkedSections.Count
    }
}

function Invoke-ExpectedFailure {
    param(
        [Parameter(Mandatory = $true)] [string]$Name,
        [Parameter(Mandatory = $true)] [scriptblock]$Action
    )
    try {
        & $Action
    }
    catch {
        return
    }
    throw "Negative fixture unexpectedly passed: $Name"
}

try {
    $tutorialFiles = @(Get-ChildItem -LiteralPath (Join-Path $Root "docs\current") `
        -File -Filter "architecture-lab-tutorial*.md")
    if ($tutorialFiles.Count -ne 1 -or
        $tutorialFiles[0].Name -ne "architecture-lab-tutorial.md") {
        throw "Exactly one canonical living Architecture Lab tutorial is allowed."
    }

    $tutorial = Get-Content -LiteralPath $tutorialFiles[0].FullName -Raw
    $ledger = Get-Content -LiteralPath `
        (Join-Path $Root "docs\current\todolist.md") -Raw
    $roadmap = Get-Content -LiteralPath `
        (Join-Path $Root "docs\current\architecture-lab-roadmap-v1.md") -Raw
    $result = Test-TutorialDocument -Tutorial $tutorial -Ledger $ledger `
        -Roadmap $roadmap -RepositoryRoot $Root

    $fixtureCount = 0
    if ($SelfTest) {
        $fixtureCount = 4
        $malformed = $tutorial.Replace(
            "AL-A0|00|0.1|", "AL-A0|bad-part|0.1|")
        Invoke-ExpectedFailure "malformed-manifest" {
            Test-TutorialDocument -Tutorial $malformed -Ledger $ledger `
                -Roadmap $roadmap -RepositoryRoot $Root | Out-Null
        }

        $missingEvidence = $tutorial.Replace(
            "docs/reports/architecture-lab-baseline-v1.md",
            "docs/reports/architecture-lab-missing-evidence.md")
        Invoke-ExpectedFailure "missing-evidence" {
            Test-TutorialDocument -Tutorial $missingEvidence -Ledger $ledger `
                -Roadmap $roadmap -RepositoryRoot $Root | Out-Null
        }

        $emptySection = [regex]::Replace(
            $tutorial,
            '(?ms)(^###\s+0\.1\b.*?^####\s+Problem\s*\r?\n).*?(?=^####\s+Naive Solution)',
            '$1')
        Invoke-ExpectedFailure "empty-section" {
            Test-TutorialDocument -Tutorial $emptySection -Ledger $ledger `
                -Roadmap $roadmap -RepositoryRoot $Root | Out-Null
        }

        $placeholderPart = $tutorial +
            "`r`n## Part 99 - Placeholder`r`n"
        Invoke-ExpectedFailure "placeholder-part" {
            Test-TutorialDocument -Tutorial $placeholderPart -Ledger $ledger `
                -Roadmap $roadmap -RepositoryRoot $Root | Out-Null
        }
    }

    Write-Host (
        "[ARCHITECTURE_LAB_DOCUMENTATION] status=PASS batches=" +
        $result.Batches + " parts=" + $result.Parts + " sections=" +
        $result.Sections + " negative_fixtures=" + $fixtureCount)
    exit 0
}
catch {
    Write-Error (
        "[ARCHITECTURE_LAB_DOCUMENTATION] status=FAIL " +
        $_.Exception.Message)
    exit 1
}
