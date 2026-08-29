param(
    [string]$Source =
        "docs\art-sources\hellomine3d-v10b2-materials-imagegen-source.png",
    [string]$EconomySource =
        "docs\art-sources\hellomine3d-economy-icons-imagegen-source.png",
    [string]$TorchSource =
        "docs\art-sources\hellomine3d-p11-0-torch-imagegen-source.png",
    [string]$Layout = "media\materials\Base.terrain-atlas",
    [string]$Output = "media\textures\DefaultPack.png"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Resolve-ProjectPath {
    param([string]$Path, [switch]$AllowMissing)
    if ([IO.Path]::IsPathRooted($Path)) {
        if ($AllowMissing) {
            return [IO.Path]::GetFullPath($Path)
        }
        return (Resolve-Path -LiteralPath $Path).Path
    }
    $fullPath = [IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
    if ($AllowMissing) {
        return $fullPath
    }
    return (Resolve-Path -LiteralPath $fullPath).Path
}

function Read-AtlasLayout {
    param([string]$Path)

    $lines = @(Get-Content -LiteralPath $Path -Encoding UTF8)
    if ($lines.Count -eq 0 -or
        $lines[0] -ne "# HelloMine3D terrain atlas layout v1") {
        throw "Invalid terrain atlas layout header: $Path"
    }

    $bySemantic = @{}
    $byCoordinate = @{}
    foreach ($line in $lines | Select-Object -Skip 1) {
        $trimmed = $line.Trim()
        if (-not $trimmed -or $trimmed.StartsWith("#")) {
            continue
        }
        $parts = $trimmed.Split('|')
        if ($parts.Count -ne 7) {
            throw "Invalid terrain atlas layout entry: $trimmed"
        }

        $semantic = $parts[0]
        $x = 0
        $y = 0
        if ($semantic -notmatch '^[a-z][a-z0-9_]*$' -or
            -not [int]::TryParse($parts[1], [ref]$x) -or
            -not [int]::TryParse($parts[2], [ref]$y) -or
            $x -lt 0 -or $x -ge 16 -or $y -lt 0 -or $y -ge 16) {
            throw "Invalid terrain atlas semantic or coordinate: $trimmed"
        }
        $alpha = $parts[3]
        if ($alpha -notin @('opaque', 'cutout', 'translucent', 'icon')) {
            throw "Invalid terrain atlas alpha mode: $trimmed"
        }
        $fill = $parts[4]
        if ($fill -notmatch '^[0-9A-Fa-f]{6}$') {
            throw "Invalid terrain atlas fill colour: $trimmed"
        }
        if ([string]::IsNullOrWhiteSpace($parts[5]) -or
            [string]::IsNullOrWhiteSpace($parts[6])) {
            throw "Terrain atlas entries require en-US and zh-CN names: $trimmed"
        }

        $coordinate = "$x,$y"
        if ($bySemantic.ContainsKey($semantic) -or
            $byCoordinate.ContainsKey($coordinate)) {
            throw "Duplicate terrain atlas semantic or coordinate: $trimmed"
        }
        $entry = [PSCustomObject]@{
            Semantic = $semantic
            X = $x
            Y = $y
            Alpha = $alpha
            Fill = $fill.ToUpperInvariant()
            English = $parts[5]
            Chinese = $parts[6]
        }
        $bySemantic[$semantic] = $entry
        $byCoordinate[$coordinate] = $entry
    }

    if ($bySemantic.Count -ne 113) {
        throw "Terrain atlas layout must define exactly 113 populated tiles; got $($bySemantic.Count)."
    }
    return $bySemantic
}

function Convert-HexColour {
    param([string]$Hex)
    $red = [Convert]::ToInt32($Hex.Substring(0, 2), 16)
    $green = [Convert]::ToInt32($Hex.Substring(2, 2), 16)
    $blue = [Convert]::ToInt32($Hex.Substring(4, 2), 16)
    return [Drawing.Color]::FromArgb(255, $red, $green, $blue)
}

$sourcePath = Resolve-ProjectPath -Path $Source
$economySourcePath = Resolve-ProjectPath -Path $EconomySource
$torchSourcePath = Resolve-ProjectPath -Path $TorchSource
$layoutPath = Resolve-ProjectPath -Path $Layout
$outputPath = Resolve-ProjectPath -Path $Output -AllowMissing
$outputDirectory = Split-Path -Parent $outputPath
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

Add-Type -AssemblyName System.Drawing

$layoutEntries = Read-AtlasLayout -Path $layoutPath
$sourceBitmap = [Drawing.Bitmap]::FromFile($sourcePath)
$economyBitmap = [Drawing.Bitmap]::FromFile($economySourcePath)
$torchBitmap = [Drawing.Bitmap]::FromFile($torchSourcePath)
$atlas = New-Object Drawing.Bitmap 256, 256,
    ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [Drawing.Graphics]::FromImage($atlas)
$graphics.CompositingMode =
    [Drawing.Drawing2D.CompositingMode]::SourceCopy
$graphics.CompositingQuality =
    [Drawing.Drawing2D.CompositingQuality]::HighSpeed
$graphics.InterpolationMode =
    [Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$graphics.PixelOffsetMode =
    [Drawing.Drawing2D.PixelOffsetMode]::Half
$graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::None
$graphics.Clear([Drawing.Color]::Transparent)
$builtSemantics = New-Object 'System.Collections.Generic.HashSet[string]'

function Copy-GeneratedTile {
    param(
        [string]$Semantic,
        [ValidateSet('material', 'economy', 'torch')][string]$SourceKind,
        [int]$SourceX, [int]$SourceY,
        [int]$SourceWidth, [int]$SourceHeight,
        [switch]$KeepAspect
    )

    if (-not $layoutEntries.ContainsKey($Semantic)) {
        throw "Generated tile is missing from the layout: $Semantic"
    }
    if (-not $builtSemantics.Add($Semantic)) {
        throw "Generated tile was emitted twice: $Semantic"
    }
    $entry = $layoutEntries[$Semantic]
    $sourceImage = switch ($SourceKind) {
        'material' { $sourceBitmap }
        'economy' { $economyBitmap }
        'torch' { $torchBitmap }
    }
    if ($SourceX -lt 0 -or $SourceY -lt 0 -or
        $SourceWidth -le 0 -or $SourceHeight -le 0 -or
        $SourceX + $SourceWidth -gt $sourceImage.Width -or
        $SourceY + $SourceHeight -gt $sourceImage.Height) {
        throw "Source crop is outside '$SourceKind' for $Semantic."
    }

    $tile = New-Object Drawing.Bitmap 16, 16,
        ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $tileGraphics = [Drawing.Graphics]::FromImage($tile)
    try {
        $tileGraphics.CompositingQuality =
            [Drawing.Drawing2D.CompositingQuality]::HighSpeed
        $tileGraphics.InterpolationMode =
            [Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
        $tileGraphics.PixelOffsetMode =
            [Drawing.Drawing2D.PixelOffsetMode]::Half
        $tileGraphics.SmoothingMode =
            [Drawing.Drawing2D.SmoothingMode]::None

        if ($entry.Alpha -eq 'opaque') {
            $tileGraphics.CompositingMode =
                [Drawing.Drawing2D.CompositingMode]::SourceCopy
            $tileGraphics.Clear((Convert-HexColour $entry.Fill))
            $tileGraphics.CompositingMode =
                [Drawing.Drawing2D.CompositingMode]::SourceOver
        }
        else {
            $tileGraphics.CompositingMode =
                [Drawing.Drawing2D.CompositingMode]::SourceCopy
            $tileGraphics.Clear([Drawing.Color]::Transparent)
        }

        $destination = [Drawing.Rectangle]::FromLTRB(0, 0, 16, 16)
        if ($KeepAspect) {
            $scale = [Math]::Min(14.0 / $SourceWidth,
                                 14.0 / $SourceHeight)
            $width = [Math]::Max(
                1, [int][Math]::Round($SourceWidth * $scale))
            $height = [Math]::Max(
                1, [int][Math]::Round($SourceHeight * $scale))
            $left = [int][Math]::Floor((16 - $width) / 2.0)
            $top = [int][Math]::Floor((16 - $height) / 2.0)
            $destination = [Drawing.Rectangle]::FromLTRB(
                $left, $top, $left + $width, $top + $height)
        }
        $sourceRectangle = [Drawing.Rectangle]::FromLTRB(
            $SourceX, $SourceY,
            $SourceX + $SourceWidth, $SourceY + $SourceHeight)
        $tileGraphics.DrawImage($sourceImage, $destination, $sourceRectangle,
                                [Drawing.GraphicsUnit]::Pixel)
        $tileGraphics.Flush()
    }
    finally {
        $tileGraphics.Dispose()
    }

    try {
        if ($entry.Alpha -in @('cutout', 'icon')) {
            for ($y = 0; $y -lt 16; ++$y) {
                for ($x = 0; $x -lt 16; ++$x) {
                    $pixel = $tile.GetPixel($x, $y)
                    if ($pixel.A -lt 64) {
                        $tile.SetPixel($x, $y, [Drawing.Color]::Transparent)
                    }
                    else {
                        $tile.SetPixel(
                            $x, $y,
                            [Drawing.Color]::FromArgb(
                                255, $pixel.R, $pixel.G, $pixel.B))
                    }
                }
            }
        }

        $transparent = 0
        $partial = 0
        $opaque = 0
        for ($y = 0; $y -lt 16; ++$y) {
            for ($x = 0; $x -lt 16; ++$x) {
                $alpha = $tile.GetPixel($x, $y).A
                if ($alpha -eq 0) {
                    ++$transparent
                }
                elseif ($alpha -eq 255) {
                    ++$opaque
                }
                else {
                    ++$partial
                }
            }
        }
        if ($entry.Alpha -eq 'opaque' -and $opaque -ne 256) {
            throw "Opaque tile '$Semantic' contains transparent pixels."
        }
        if ($entry.Alpha -in @('cutout', 'icon') -and
            ($transparent -eq 0 -or $opaque -eq 0 -or $partial -ne 0)) {
            throw "Cutout tile '$Semantic' has an invalid alpha boundary."
        }
        if ($entry.Alpha -eq 'translucent' -and
            ($transparent + $partial -eq 0 -or $opaque + $partial -eq 0)) {
            throw "Translucent tile '$Semantic' has no alpha variation."
        }

        $graphics.DrawImageUnscaled(
            $tile, [int]$entry.X * 16, [int]$entry.Y * 16)
    }
    finally {
        $tile.Dispose()
    }
}

function Copy-EcologyTile {
    param(
        [string]$SourceSemantic,
        [string]$DestinationSemantic,
        [double[]]$Tint,
        [ValidateRange(0, 2)][int]$Variant
    )

    if (-not $layoutEntries.ContainsKey($SourceSemantic) -or
        -not $layoutEntries.ContainsKey($DestinationSemantic)) {
        throw "Ecology tile references an unknown semantic: $SourceSemantic -> $DestinationSemantic"
    }
    if (-not $builtSemantics.Contains($SourceSemantic)) {
        throw "Ecology source tile was not built first: $SourceSemantic"
    }
    if (-not $builtSemantics.Add($DestinationSemantic)) {
        throw "Generated tile was emitted twice: $DestinationSemantic"
    }
    if ($Tint.Count -ne 3) {
        throw "Ecology tint requires exactly three RGB multipliers."
    }

    $sourceEntry = $layoutEntries[$SourceSemantic]
    $destinationEntry = $layoutEntries[$DestinationSemantic]
    # Top-facing natural surfaces use quarter turns so the strongest pixel
    # strokes do not repeat in one direction across every block. Vertical
    # flora and grass sides may only mirror horizontally; rotating those
    # would put roots/cut edges on the wrong side. The brightness delta stays
    # deliberately small so the variants read as one material, not patches.
    $variantFactor = @(1.0, 1.02, 0.98)[$Variant]
    $tile = New-Object Drawing.Bitmap 16, 16,
        ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        for ($y = 0; $y -lt 16; ++$y) {
            for ($x = 0; $x -lt 16; ++$x) {
                $sourceX = $x
                $sourceY = $y
                if ($SourceSemantic -in @(
                        'grass_top', 'oak_leaves', 'water')) {
                    if ($Variant -eq 1) {
                        $sourceX = $y
                        $sourceY = 15 - $x
                    }
                    elseif ($Variant -eq 2) {
                        $sourceX = 15 - $y
                        $sourceY = $x
                    }
                }
                elseif ($SourceSemantic -in @(
                        'grass_side', 'tall_grass') -and
                        $Variant -eq 1) {
                    $sourceX = 15 - $x
                }

                $pixel = $atlas.GetPixel(
                    [int]$sourceEntry.X * 16 + $sourceX,
                    [int]$sourceEntry.Y * 16 + $sourceY)
                if ($pixel.A -eq 0) {
                    $tile.SetPixel($x, $y, [Drawing.Color]::Transparent)
                    continue
                }

                # Grass sides keep their dirt body neutral; only pixels whose
                # green channel clearly dominates receive the ecology tint.
                $applyTint = $SourceSemantic -ne 'grass_side' -or
                    ($pixel.G -gt $pixel.R * 1.05 -and
                     $pixel.G -gt $pixel.B * 1.05)
                $redTint = if ($applyTint) { $Tint[0] } else { 1.0 }
                $greenTint = if ($applyTint) { $Tint[1] } else { 1.0 }
                $blueTint = if ($applyTint) { $Tint[2] } else { 1.0 }
                $red = [Math]::Min(255, [Math]::Max(
                    0, [int][Math]::Round(
                        $pixel.R * $redTint * $variantFactor)))
                $green = [Math]::Min(255, [Math]::Max(
                    0, [int][Math]::Round(
                        $pixel.G * $greenTint * $variantFactor)))
                $blue = [Math]::Min(255, [Math]::Max(
                    0, [int][Math]::Round(
                        $pixel.B * $blueTint * $variantFactor)))
                $tile.SetPixel(
                    $x, $y,
                    [Drawing.Color]::FromArgb(
                        $pixel.A, $red, $green, $blue))
            }
        }

        $graphics.DrawImageUnscaled(
            $tile, [int]$destinationEntry.X * 16,
            [int]$destinationEntry.Y * 16)
    }
    finally {
        $tile.Dispose()
    }
}

try {
    # V10B2 original material source. Opaque surface crops exclude the
    # presentation outline; flora and icons keep their transparent silhouette.
    Copy-GeneratedTile grass_top material 89 49 54 24
    Copy-GeneratedTile grass_side material 89 51 54 60
    Copy-GeneratedTile dirt material 165 51 56 60
    Copy-GeneratedTile stone material 242 51 55 60
    Copy-GeneratedTile oak_bark_side material 319 51 56 60
    Copy-GeneratedTile oak_bark_top material 395 51 57 60
    Copy-GeneratedTile oak_leaves material 471 48 58 66
    Copy-GeneratedTile sand material 556 51 53 60
    Copy-GeneratedTile water material 634 51 54 60
    Copy-GeneratedTile cactus material 713 51 54 60
    Copy-GeneratedTile rose material 797 48 48 66 -KeepAspect
    Copy-GeneratedTile tall_grass material 868 49 58 65 -KeepAspect
    Copy-GeneratedTile dead_shrub material 943 48 56 66 -KeepAspect
    Copy-GeneratedTile coal_ore material 1024 51 53 60
    Copy-GeneratedTile iron_ore material 1098 51 49 60
    Copy-GeneratedTile waystone_core material 1167 51 51 62

    Copy-GeneratedTile chest material 89 172 54 69
    Copy-GeneratedTile workbench material 165 170 56 71
    Copy-GeneratedTile furnace material 242 172 55 69
    Copy-GeneratedTile glass material 316 169 63 75
    Copy-GeneratedTile glass_borderless material 392 169 63 75
    Copy-GeneratedTile oak_planks material 472 172 59 69
    Copy-GeneratedTile torch torch 496 64 279 1073 -KeepAspect

    Copy-GeneratedTile wheat_seeds material 98 305 39 38 -KeepAspect
    Copy-GeneratedTile wheat material 162 284 61 68 -KeepAspect
    Copy-GeneratedTile wooden_pickaxe material 241 292 54 56 -KeepAspect
    Copy-GeneratedTile stone_pickaxe material 316 292 55 56 -KeepAspect
    Copy-GeneratedTile iron_ingot material 396 295 58 48 -KeepAspect
    Copy-GeneratedTile iron_pickaxe material 476 292 54 56 -KeepAspect
    Copy-GeneratedTile iron_sword material 551 292 54 56 -KeepAspect
    Copy-GeneratedTile bread material 717 292 57 59 -KeepAspect
    Copy-GeneratedTile wooden_sword material 797 288 57 60 -KeepAspect
    Copy-GeneratedTile stone_sword material 875 288 58 60 -KeepAspect

    # N10 economy icons remain an independently frozen generated source.
    Copy-GeneratedTile raw_meat economy 802 175 65 65 -KeepAspect
    Copy-GeneratedTile cooked_meat economy 883 181 63 61 -KeepAspect
    Copy-GeneratedTile cactus_salad economy 962 169 69 74 -KeepAspect
    Copy-GeneratedTile trail_ration economy 1042 175 65 68 -KeepAspect
    Copy-GeneratedTile plant_fiber economy 1116 176 74 71 -KeepAspect

    # V10B3 derives restrained world-only ecology colours and three small
    # orientation/brightness variants from the frozen V10B2 tiles. This stays
    # deterministic, preserves every alpha value and leaves the base HUD/held
    # coordinates untouched.
    $ecologies = @(
        [PSCustomObject]@{
            Slug='desert'; Plant=@(1.12, 0.88, 0.68);
            Water=@(0.94, 1.02, 1.04)
        },
        [PSCustomObject]@{
            Slug='grassland'; Plant=@(1.02, 1.05, 0.92);
            Water=@(0.92, 1.02, 1.04)
        },
        [PSCustomObject]@{
            Slug='light_forest'; Plant=@(0.92, 1.03, 0.90);
            Water=@(0.88, 1.00, 1.06)
        },
        [PSCustomObject]@{
            Slug='temperate_forest'; Plant=@(0.82, 0.96, 0.84);
            Water=@(0.84, 0.96, 1.08)
        },
        [PSCustomObject]@{
            Slug='ocean'; Plant=@(0.84, 0.98, 1.02);
            Water=@(0.80, 0.98, 1.14)
        }
    )
    foreach ($ecology in $ecologies) {
        foreach ($variant in 0..2) {
            foreach ($semantic in @(
                    'grass_top', 'grass_side', 'oak_leaves',
                    'water', 'tall_grass')) {
                $tint = if ($semantic -eq 'water') {
                    $ecology.Water
                }
                else {
                    $ecology.Plant
                }
                $destination = '{0}_{1}_v{2}' -f
                    $semantic, $ecology.Slug, $variant
                Copy-EcologyTile -SourceSemantic $semantic -DestinationSemantic (
                    $destination) -Tint $tint -Variant $variant
            }
        }
    }

    if ($builtSemantics.Count -ne $layoutEntries.Count) {
        $missing = @($layoutEntries.Keys | Where-Object {
            -not $builtSemantics.Contains($_)
        })
        throw "Terrain atlas layout has unbuilt semantics: $($missing -join ', ')"
    }
    $atlas.Save($outputPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $graphics.Dispose()
    $atlas.Dispose()
    $sourceBitmap.Dispose()
    $economyBitmap.Dispose()
    $torchBitmap.Dispose()
}

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputPath).Hash
Write-Host "[FS3_ATLAS] source=$sourcePath"
Write-Host "[FS3_ATLAS] economy_source=$economySourcePath"
Write-Host "[FS3_ATLAS] torch_source=$torchSourcePath"
Write-Host "[FS3_ATLAS] layout=$layoutPath tiles=$($layoutEntries.Count)"
Write-Host "[FS3_ATLAS] output=$outputPath size=256x256 tile=16 sha256=$hash"
Write-Host "[FS3_ATLAS] status=PASS"
