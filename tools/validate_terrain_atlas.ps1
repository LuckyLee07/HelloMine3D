[CmdletBinding()]
param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Join-Path $scriptRoot ".."
}
$Root = (Resolve-Path -LiteralPath $Root).Path
$layoutPath = Join-Path $Root "media\materials\Base.terrain-atlas"
$atlasPath = Join-Path $Root "media\textures\DefaultPack.png"
$builderPath = Join-Path $Root "tools\build_fs3_texture_atlas.ps1"
$materialSourcePath = Join-Path $Root "src\HelloMine3D\Item\Material.cpp"
$uiSourcePath = Join-Path $Root "src\HelloMine3D\Ogre\OgreUserInterface.cpp"
$appearanceSourcePath = Join-Path $Root `
    "src\HelloMine3D\World\Block\TerrainAppearance.cpp"
$meshBuilderSourcePath = Join-Path $Root `
    "src\HelloMine3D\World\Chunk\ChunkMeshBuilder.cpp"
$meshInputSourcePath = Join-Path $Root `
    "src\HelloMine3D\World\Chunk\SectionMeshInput.cpp"

$checks = 0
$failures = 0
function Test-Contract {
    param([string]$Name, [bool]$Condition, [string]$Detail = "")
    ++$script:checks
    if ($Condition) {
        Write-Host "[TERRAIN_ATLAS] PASS $Name"
        return
    }
    ++$script:failures
    [Console]::Error.WriteLine(
        "[TERRAIN_ATLAS] FAIL $Name" +
        $(if ($Detail) { ": $Detail" } else { "" }))
}

foreach ($required in @($layoutPath, $atlasPath, $builderPath,
                         $materialSourcePath, $uiSourcePath,
                         $appearanceSourcePath, $meshBuilderSourcePath,
                         $meshInputSourcePath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "V10B2 atlas validation input is missing: $required"
    }
}

$lines = @(Get-Content -LiteralPath $layoutPath -Encoding UTF8)
Test-Contract "layout-header" `
    ($lines.Count -gt 0 -and
     $lines[0] -eq "# HelloMine3D terrain atlas layout v1")

$entries = @()
$semanticSet = New-Object 'System.Collections.Generic.HashSet[string]'
$coordinateSet = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($line in $lines | Select-Object -Skip 1) {
    $trimmed = $line.Trim()
    if (-not $trimmed -or $trimmed.StartsWith('#')) {
        continue
    }
    $parts = $trimmed.Split('|')
    if ($parts.Count -ne 7) {
        Test-Contract "layout-entry-shape" $false $trimmed
        continue
    }
    $x = 0
    $y = 0
    $validCoordinate =
        [int]::TryParse($parts[1], [ref]$x) -and
        [int]::TryParse($parts[2], [ref]$y) -and
        $x -ge 0 -and $x -lt 16 -and $y -ge 0 -and $y -lt 16
    $entry = [PSCustomObject]@{
        Semantic = $parts[0]
        X = $x
        Y = $y
        Alpha = $parts[3]
        Fill = $parts[4]
        English = $parts[5]
        Chinese = $parts[6]
    }
    $entries += $entry
    Test-Contract "layout-entry-$($entry.Semantic)" `
        ($validCoordinate -and
         $entry.Semantic -match '^[a-z][a-z0-9_]*$' -and
         $entry.Alpha -in @('opaque', 'cutout', 'translucent', 'icon') -and
         $entry.Fill -match '^[0-9A-F]{6}$' -and
         -not [string]::IsNullOrWhiteSpace($entry.English) -and
         $entry.Chinese -match '[\u4e00-\u9fff]' -and
         $semanticSet.Add($entry.Semantic) -and
         $coordinateSet.Add("$x,$y")) $trimmed
}
Test-Contract "layout-populated-count" ($entries.Count -eq 119) `
    "expected=119 actual=$($entries.Count)"

$requiredSemantics = @(
    'grass_top', 'grass_side', 'dirt', 'stone', 'oak_bark_side',
    'oak_bark_top', 'oak_leaves', 'sand', 'water', 'cactus', 'rose',
    'tall_grass', 'dead_shrub', 'coal_ore', 'iron_ore', 'waystone_core',
    'chest', 'workbench', 'furnace', 'glass', 'glass_borderless',
    'oak_planks', 'wheat_seeds', 'wheat', 'wooden_pickaxe',
    'stone_pickaxe', 'iron_ingot', 'iron_pickaxe', 'iron_sword', 'bread',
    'wooden_sword', 'stone_sword', 'raw_meat', 'cooked_meat',
    'cactus_salad', 'trail_ration', 'plant_fiber', 'torch',
    'cobblestone', 'oak_door', 'wooden_axe', 'wooden_shovel',
    'ancient_compass', 'raider_ward'
)
Test-Contract "required-material-coverage" `
    (@($requiredSemantics | Where-Object {
        -not $semanticSet.Contains($_)
    }).Count -eq 0)

$ecologyRows = [ordered]@{
    desert = 3
    grassland = 4
    light_forest = 5
    temperate_forest = 6
    ocean = 7
}
$ecologyGroups = [ordered]@{
    grass_top = @{X=0; Alpha='opaque'}
    grass_side = @{X=3; Alpha='opaque'}
    oak_leaves = @{X=6; Alpha='cutout'}
    water = @{X=9; Alpha='translucent'}
    tall_grass = @{X=12; Alpha='cutout'}
}
$ecologyLayoutMatches = $true
foreach ($ecology in $ecologyRows.Keys) {
    foreach ($group in $ecologyGroups.Keys) {
        for ($variant = 0; $variant -lt 3; ++$variant) {
            $semantic = "${group}_${ecology}_v${variant}"
            $entry = $entries | Where-Object {
                $_.Semantic -eq $semantic
            } | Select-Object -First 1
            $ecologyLayoutMatches = $ecologyLayoutMatches -and
                $null -ne $entry -and
                $entry.X -eq $ecologyGroups[$group].X + $variant -and
                $entry.Y -eq $ecologyRows[$ecology] -and
                $entry.Alpha -eq $ecologyGroups[$group].Alpha
        }
    }
}
Test-Contract "v10b3-ecology-layout-5x5x3" `
    $ecologyLayoutMatches

Add-Type -AssemblyName System.Drawing
$bitmap = [Drawing.Bitmap]::FromFile($atlasPath)
try {
    Test-Contract "atlas-dimensions" `
        ($bitmap.Width -eq 256 -and $bitmap.Height -eq 256) `
        "$($bitmap.Width)x$($bitmap.Height)"

    $tileHashes = New-Object 'System.Collections.Generic.HashSet[string]'
    foreach ($entry in $entries) {
        $transparent = 0
        $partial = 0
        $opaque = 0
        $bytes = New-Object byte[] (16 * 16 * 4)
        $offset = 0
        for ($localY = 0; $localY -lt 16; ++$localY) {
            for ($localX = 0; $localX -lt 16; ++$localX) {
                $pixel = $bitmap.GetPixel(
                    $entry.X * 16 + $localX,
                    $entry.Y * 16 + $localY)
                $bytes[$offset++] = $pixel.R
                $bytes[$offset++] = $pixel.G
                $bytes[$offset++] = $pixel.B
                $bytes[$offset++] = $pixel.A
                if ($pixel.A -eq 0) {
                    ++$transparent
                }
                elseif ($pixel.A -eq 255) {
                    ++$opaque
                }
                else {
                    ++$partial
                }
            }
        }
        $sha = [Security.Cryptography.SHA256]::Create()
        try {
            $hash = [BitConverter]::ToString(
                $sha.ComputeHash($bytes)).Replace('-', '')
        }
        finally {
            $sha.Dispose()
        }
        $null = $tileHashes.Add($hash)

        $validAlpha = switch ($entry.Alpha) {
            'opaque' { $opaque -eq 256 }
            'cutout' {
                $transparent -gt 0 -and $opaque -gt 0 -and $partial -eq 0
            }
            'icon' {
                $transparent -gt 0 -and $opaque -gt 0 -and $partial -eq 0
            }
            'translucent' {
                ($transparent + $partial) -gt 0 -and
                ($opaque + $partial) -gt 0
            }
        }
        Test-Contract "alpha-$($entry.Semantic)" $validAlpha `
            "transparent=$transparent partial=$partial opaque=$opaque"
    }
    Test-Contract "all-populated-tiles-distinct" `
        ($tileHashes.Count -eq $entries.Count) `
        "unique=$($tileHashes.Count) total=$($entries.Count)"

    $unusedVisible = 0
    for ($tileY = 0; $tileY -lt 16; ++$tileY) {
        for ($tileX = 0; $tileX -lt 16; ++$tileX) {
            if ($coordinateSet.Contains("$tileX,$tileY")) {
                continue
            }
            for ($localY = 0; $localY -lt 16; ++$localY) {
                for ($localX = 0; $localX -lt 16; ++$localX) {
                    if ($bitmap.GetPixel(
                            $tileX * 16 + $localX,
                            $tileY * 16 + $localY).A -ne 0) {
                        ++$unusedVisible
                    }
                }
            }
        }
    }
    Test-Contract "unused-tiles-transparent" ($unusedVisible -eq 0) `
        "visible_pixels=$unusedVisible"
}
finally {
    $bitmap.Dispose()
}

function Read-BlockCoordinates {
    param([string]$BlockName)
    $path = Join-Path $Root "media\blocks\$BlockName.block"
    $blockLines = @(Get-Content -LiteralPath $path)
    $result = @{}
    for ($index = 0; $index -lt $blockLines.Count; ++$index) {
        $key = $blockLines[$index].Trim()
        if ($key -notin @('TexAll', 'TexTop', 'TexSide', 'TexBottom')) {
            continue
        }
        for ($valueIndex = $index + 1;
             $valueIndex -lt $blockLines.Count; ++$valueIndex) {
            $value = $blockLines[$valueIndex].Trim()
            if ($value) {
                $result[$key] = $value.Replace(' ', ',')
                break
            }
        }
    }
    return $result
}

$expectedBlocks = @{
    Grass = @{TexTop='0,0'; TexSide='1,0'; TexBottom='2,0'}
    Dirt = @{TexAll='2,0'}
    Stone = @{TexAll='3,0'}
    OakBark = @{TexTop='5,0'; TexSide='4,0'; TexBottom='5,0'}
    OakLeaf = @{TexAll='6,0'}
    Sand = @{TexAll='7,0'}
    Water = @{TexAll='8,0'}
    Cactus = @{TexAll='9,0'}
    Rose = @{TexAll='10,0'}
    TallGrass = @{TexAll='11,0'}
    WheatCrop = @{TexAll='11,0'}
    DeadShrub = @{TexAll='12,0'}
    CoalOre = @{TexAll='13,0'}
    IronOre = @{TexAll='14,0'}
    WaystoneCore = @{TexAll='15,0'}
    Chest = @{TexAll='0,1'}
    Workbench = @{TexAll='1,1'}
    Furnace = @{TexAll='2,1'}
    Glass = @{TexAll='3,1'}
    GlassBorderless = @{TexAll='4,1'}
    OakPlank = @{TexAll='5,1'}
    Torch = @{TexAll='6,1'}
    Cobblestone = @{TexAll='7,1'}
    OakDoorClosed = @{TexAll='8,1'}
    OakDoorOpen = @{TexAll='8,1'}
}
foreach ($blockName in $expectedBlocks.Keys) {
    $actual = Read-BlockCoordinates $blockName
    $matches = $actual.Count -eq $expectedBlocks[$blockName].Count
    foreach ($key in $expectedBlocks[$blockName].Keys) {
        $matches = $matches -and $actual[$key] -eq $expectedBlocks[$blockName][$key]
    }
    Test-Contract "block-faces-$BlockName" $matches `
        "actual=$($actual | ConvertTo-Json -Compress)"
}

$semanticByMaterial = @(
    '', 'grass_top', 'dirt', 'stone', 'oak_bark_side', 'oak_leaves',
    'sand', 'cactus', 'rose', 'tall_grass', 'dead_shrub', 'coal_ore',
    'iron_ore', 'glass', 'glass_borderless', 'chest', 'wheat_seeds',
    'wheat', 'workbench', 'wooden_pickaxe', 'stone_pickaxe', 'furnace',
    'iron_ingot', 'iron_pickaxe', 'iron_sword', 'bread', 'wooden_sword',
    'stone_sword', 'waystone_core', 'raw_meat', 'cooked_meat',
    'cactus_salad', 'trail_ration', 'plant_fiber', 'torch', 'oak_planks',
    'cobblestone', 'oak_door', 'wooden_axe', 'wooden_shovel',
    'ancient_compass', 'raider_ward'
)
$materialSource = Get-Content -LiteralPath $materialSourcePath -Raw
$iconsMatch = [regex]::Match(
    $materialSource,
    'MaterialIcons\s*=\s*\{\{(?<body>.*?)\}\};',
    [Text.RegularExpressions.RegexOptions]::Singleline)
$coordinates = @()
if ($iconsMatch.Success) {
    foreach ($match in [regex]::Matches(
            $iconsMatch.Groups['body'].Value,
            '\{\s*(-?\d+)\s*,\s*(-?\d+)\s*\}')) {
        $coordinates += "$($match.Groups[1].Value),$($match.Groups[2].Value)"
    }
}
Test-Contract "material-icon-count" `
    ($coordinates.Count -eq $semanticByMaterial.Count) `
    "expected=$($semanticByMaterial.Count) actual=$($coordinates.Count)"
if ($coordinates.Count -eq $semanticByMaterial.Count) {
    $iconMapMatches = $coordinates[0] -eq '-1,-1'
    for ($index = 1; $index -lt $semanticByMaterial.Count; ++$index) {
        $entry = $entries | Where-Object {
            $_.Semantic -eq $semanticByMaterial[$index]
        } | Select-Object -First 1
        $iconMapMatches = $iconMapMatches -and
            $null -ne $entry -and
            $coordinates[$index] -eq "$($entry.X),$($entry.Y)"
    }
    Test-Contract "world-hud-held-coordinate-identity" $iconMapMatches
}

$uiSource = Get-Content -LiteralPath $uiSourcePath -Raw
Test-Contract "hud-uses-frozen-terrain-profile" `
    ($uiSource.Contains('terrainMaterial.atlasPixels') -and
     $uiSource.Contains('terrainMaterial.tilePixels') -and
     $uiSource.Contains('terrainMaterial.containsTile') -and
     -not $uiSource.Contains('constexpr float atlasSize = 256.f'))

$appearanceSource = Get-Content -LiteralPath $appearanceSourcePath -Raw
$meshBuilderSource = Get-Content -LiteralPath $meshBuilderSourcePath -Raw
$meshInputSource = Get-Content -LiteralPath $meshInputSourcePath -Raw
Test-Contract "v10b3-world-only-ecology-selection" `
    ($appearanceSource.Contains('BlockId::Grass') -and
     $appearanceSource.Contains('BlockId::OakLeaf') -and
     $appearanceSource.Contains('BlockId::Water') -and
     $appearanceSource.Contains('BlockId::TallGrass') -and
     $appearanceSource.Contains('coordinateVariant') -and
     $appearanceSource.Contains('ecologyRow'))
Test-Contract "v10b3-coordinate-patch-bounds-fragmentation" `
    ($appearanceSource.Contains('VariantPatchSize') -and
     $appearanceSource.Contains('patchCoordinate') -and
     $appearanceSource.Contains('remainder < 0'))
Test-Contract "v10b3-greedy-key-includes-appearance" `
    ($meshBuilderSource.Contains('appearanceKey') -and
     $meshBuilderSource.Contains(
         'left.appearanceKey == right.appearanceKey'))
Test-Contract "v10b3-snapshot-freezes-biome-and-seed" `
    ($meshInputSource.Contains('getBiomeAtWorld') -and
     $meshInputSource.Contains('m_terrainSeed = terrainSeed'))

$temporaryOutput = Join-Path $Root "tmp\v10b3-atlas-rebuild.png"
try {
    New-Item -ItemType Directory -Path (Split-Path -Parent $temporaryOutput) `
        -Force | Out-Null
    & $builderPath -Output $temporaryOutput | Out-Host
    $committedHash = (Get-FileHash -Algorithm SHA256 `
        -LiteralPath $atlasPath).Hash
    $rebuiltHash = (Get-FileHash -Algorithm SHA256 `
        -LiteralPath $temporaryOutput).Hash
    Test-Contract "deterministic-rebuild" `
        ($committedHash -eq $rebuiltHash) `
        "committed=$committedHash rebuilt=$rebuiltHash"
}
finally {
    if (Test-Path -LiteralPath $temporaryOutput -PathType Leaf) {
        Remove-Item -LiteralPath $temporaryOutput -Force
    }
}

Write-Host "[TERRAIN_ATLAS] checks=$checks failures=$failures"
Write-Host "[TERRAIN_ATLAS] status=$(if ($failures -eq 0) {'PASS'} else {'FAIL'})"
if ($failures -ne 0) {
    exit 1
}
