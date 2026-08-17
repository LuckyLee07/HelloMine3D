[CmdletBinding()]
param(
    [string]$Root = "",
    [string]$OutputPath = "",
    [switch]$Check
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Join-Path $ScriptRoot ".."
}
$Root = (Resolve-Path $Root).Path
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $Root "media\resource-manifest.txt"
}
if (-not [System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $Root $OutputPath
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

$entries = New-Object 'System.Collections.Generic.HashSet[string]'
function Add-ManifestEntry {
    param([string]$Category, [string]$RelativePath)
    $normalized = $RelativePath.Replace('\', '/')
    if ($Category -notmatch '^[a-z][a-z-]*$' -or
        $normalized -notmatch '^[A-Za-z0-9._/-]+$') {
        throw "Invalid manifest entry: $Category|$normalized"
    }
    $null = $entries.Add("$Category|$normalized")
}

$blockDatabasePath = Join-Path $Root "src\HelloMine3D\World\Block\BlockDatabase.cpp"
$programPath = Join-Path $Root "media\ogre\HelloMine3D.program"
$materialPath = Join-Path $Root "media\ogre\HelloMine3D.material"
foreach ($requiredInput in @($blockDatabasePath, $programPath, $materialPath)) {
    if (-not (Test-Path -LiteralPath $requiredInput -PathType Leaf)) {
        throw "Manifest input is missing: $requiredInput"
    }
}

Add-ManifestEntry "resource-script" "media/ogre/HelloMine3D.program"
Add-ManifestEntry "resource-script" "media/ogre/HelloMine3D.material"
Add-ManifestEntry "font" "media/fonts/rs.ttf"
Add-ManifestEntry "runtime-template" "bin/Mine.cfg"
Add-ManifestEntry "runtime-template" "bin/MineResources.cfg"
Add-ManifestEntry "runtime-template" "bin/resource-packs.txt"

$audioRoot = Join-Path $Root "media\audio"
if (-not (Test-Path -LiteralPath $audioRoot -PathType Container)) {
    throw "Audio resource directory is missing: $audioRoot"
}
$audioFiles = @(Get-ChildItem -LiteralPath $audioRoot `
    -Filter "*.audio" -File -Recurse)
if ($audioFiles.Count -eq 0) {
    throw "No base audio resources were discovered."
}
foreach ($audioFile in $audioFiles) {
    $relativeAudio = $audioFile.FullName.Substring($Root.Length).TrimStart(
        [char]'\', [char]'/')
    Add-ManifestEntry "audio" $relativeAudio
}

$objectiveRoot = Join-Path $Root "media\objectives"
if (-not (Test-Path -LiteralPath $objectiveRoot -PathType Container)) {
    throw "Objective resource directory is missing: $objectiveRoot"
}
$objectiveFiles = @(Get-ChildItem -LiteralPath $objectiveRoot `
    -Filter "*.objective" -File -Recurse)
if ($objectiveFiles.Count -eq 0) {
    throw "No base objective resources were discovered."
}
foreach ($objectiveFile in $objectiveFiles) {
    $relativeObjective = $objectiveFile.FullName.Substring($Root.Length).TrimStart(
        [char]'\', [char]'/')
    Add-ManifestEntry "objective" $relativeObjective
}

$recipeRoot = Join-Path $Root "media\recipes"
if (-not (Test-Path -LiteralPath $recipeRoot -PathType Container)) {
    throw "Recipe resource directory is missing: $recipeRoot"
}
$recipeFiles = @(Get-ChildItem -LiteralPath $recipeRoot `
    -Filter "*.recipe" -File -Recurse)
if ($recipeFiles.Count -eq 0) {
    throw "No base recipe resources were discovered."
}
foreach ($recipeFile in $recipeFiles) {
    $relativeRecipe = $recipeFile.FullName.Substring($Root.Length).TrimStart(
        [char]'\', [char]'/')
    Add-ManifestEntry "recipe" $relativeRecipe
}

$toolRoot = Join-Path $Root "media\tools"
if (-not (Test-Path -LiteralPath $toolRoot -PathType Container)) {
    throw "Tool resource directory is missing: $toolRoot"
}
$toolFiles = @(Get-ChildItem -LiteralPath $toolRoot `
    -Filter "*.tool" -File -Recurse)
if ($toolFiles.Count -eq 0) {
    throw "No base tool resources were discovered."
}
foreach ($toolFile in $toolFiles) {
    $relativeTool = $toolFile.FullName.Substring($Root.Length).TrimStart(
        [char]'\', [char]'/')
    Add-ManifestEntry "tool" $relativeTool
}

$blockSource = Get-Content -LiteralPath $blockDatabasePath -Raw
$blockMatches = [regex]::Matches(
    $blockSource,
    'addBlock\s*\(\s*BlockId::[A-Za-z0-9_]+\s*,\s*"([^"]+)"',
    [System.Text.RegularExpressions.RegexOptions]::Singleline)
if ($blockMatches.Count -eq 0) {
    throw "No registered block definitions were discovered."
}

foreach ($match in $blockMatches) {
    $blockName = $match.Groups[1].Value
    $relativeBlock = "media/blocks/$blockName.block"
    Add-ManifestEntry "block" $relativeBlock

    $blockPath = Join-Path $Root $relativeBlock
    if (-not (Test-Path -LiteralPath $blockPath -PathType Leaf)) {
        throw "Registered block definition is missing: $relativeBlock"
    }
    $lines = @(Get-Content -LiteralPath $blockPath)
    for ($index = 0; $index -lt $lines.Count; ++$index) {
        if ($lines[$index].Trim() -ne "Shape") {
            continue
        }
        for ($valueIndex = $index + 1; $valueIndex -lt $lines.Count; ++$valueIndex) {
            $shapeName = $lines[$valueIndex].Trim()
            if (-not [string]::IsNullOrWhiteSpace($shapeName)) {
                Add-ManifestEntry "shape" "media/shapes/$shapeName.shape"
                break
            }
        }
    }
}

$programSource = Get-Content -LiteralPath $programPath -Raw
foreach ($match in [regex]::Matches(
    $programSource, '(?m)^\s*source\s+(\S+)\s*$')) {
    Add-ManifestEntry "shader" "media/ogre/$($match.Groups[1].Value)"
}

$materialSource = Get-Content -LiteralPath $materialPath -Raw
foreach ($match in [regex]::Matches(
    $materialSource, '(?m)^\s*texture\s+(\S+)')) {
    Add-ManifestEntry "texture" "media/textures/$($match.Groups[1].Value)"
}
foreach ($match in [regex]::Matches(
    $materialSource,
    '(?m)^\s*cubic_texture\s+(\S+)\s+combinedUVW\s*$')) {
    $name = $match.Groups[1].Value
    $extension = [System.IO.Path]::GetExtension($name)
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($name)
    foreach ($face in @('fr', 'bk', 'lf', 'rt', 'up', 'dn')) {
        Add-ManifestEntry "texture" "media/textures/${baseName}_${face}${extension}"
    }
}

$sortedEntries = @($entries | Sort-Object)
foreach ($entry in $sortedEntries) {
    $relativePath = $entry.Substring($entry.IndexOf('|') + 1)
    $resolved = Join-Path $Root $relativePath
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf) -or
        (Get-Item -LiteralPath $resolved).Length -le 0) {
        throw "Manifest resource is missing or empty: $entry"
    }
}

$manifestLines = @(
    '# HelloMine3D resource manifest v1',
    '# Generated by tools/generate_resource_manifest.ps1; do not edit.',
    ''
) + $sortedEntries
$expected = ($manifestLines -join "`n") + "`n"

if ($Check) {
    if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) {
        throw "Resource manifest is missing: $OutputPath"
    }
    $current = (Get-Content -LiteralPath $OutputPath -Raw).Replace("`r`n", "`n")
    $current = $current.Replace("`r", "`n")
    if ($current -ne $expected) {
        $expectedEntries = @($sortedEntries)
        $currentEntries = @(
            Get-Content -LiteralPath $OutputPath |
                ForEach-Object { $_.Trim() } |
                Where-Object { $_ -and -not $_.StartsWith('#') }
        )
        foreach ($entry in $expectedEntries) {
            if ($entry -notin $currentEntries) {
                [Console]::Error.WriteLine(
                    "[RESOURCE_MANIFEST] MISSING_ENTRY $entry")
            }
        }
        foreach ($entry in $currentEntries) {
            if ($entry -notin $expectedEntries) {
                [Console]::Error.WriteLine(
                    "[RESOURCE_MANIFEST] STALE_ENTRY $entry")
            }
        }
        throw "Resource manifest is not current: $OutputPath"
    }
    Write-Host "[RESOURCE_MANIFEST] status=PASS mode=check entries=$($sortedEntries.Count) path=$OutputPath"
    return
}

$parent = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Path $parent -Force | Out-Null
[System.IO.File]::WriteAllText(
    $OutputPath, $expected, (New-Object System.Text.UTF8Encoding($false)))
Write-Host "[RESOURCE_MANIFEST] status=PASS mode=generate entries=$($sortedEntries.Count) path=$OutputPath"
