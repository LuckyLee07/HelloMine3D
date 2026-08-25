param(
    [string]$Source = "docs\art-sources\hellomine3d-atlas-imagegen-source.png",
    [string]$EconomySource =
        "docs\art-sources\hellomine3d-economy-icons-imagegen-source.png",
    [string]$Output = "media\textures\DefaultPack.png"
)

$ErrorActionPreference = "Stop"
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

$sourcePath = Resolve-ProjectPath -Path $Source
$economySourcePath = Resolve-ProjectPath -Path $EconomySource
$outputPath = Resolve-ProjectPath -Path $Output -AllowMissing
$outputDirectory = Split-Path -Parent $outputPath
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

Add-Type -AssemblyName System.Drawing

$sourceBitmap = [Drawing.Bitmap]::FromFile($sourcePath)
$economyBitmap = [Drawing.Bitmap]::FromFile($economySourcePath)
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

function Copy-Tile {
    param(
        [int]$AtlasX, [int]$AtlasY,
        [int]$SourceX, [int]$SourceY,
        [int]$SourceWidth, [int]$SourceHeight,
        [switch]$KeepAspect
    )

    $destination = [Drawing.Rectangle]::FromLTRB(
        ($AtlasX * 16), ($AtlasY * 16),
        ($AtlasX * 16 + 16), ($AtlasY * 16 + 16))
    if ($KeepAspect) {
        $scale = [Math]::Min(14.0 / $SourceWidth,
                             14.0 / $SourceHeight)
        $width = [Math]::Max(1, [int][Math]::Round($SourceWidth * $scale))
        $height = [Math]::Max(1, [int][Math]::Round($SourceHeight * $scale))
        $left = ($AtlasX * 16) +
            [int][Math]::Floor((16 - $width) / 2.0)
        $top = ($AtlasY * 16) +
            [int][Math]::Floor((16 - $height) / 2.0)
        $destination = [Drawing.Rectangle]::FromLTRB(
            $left, $top, $left + $width, $top + $height)
    }
    $sourceRectangle = [Drawing.Rectangle]::FromLTRB(
        $SourceX, $SourceY,
        $SourceX + $SourceWidth, $SourceY + $SourceHeight)
    $graphics.DrawImage($sourceBitmap, $destination, $sourceRectangle,
                        [Drawing.GraphicsUnit]::Pixel)
}

function Copy-EconomyTile {
    param(
        [int]$AtlasX,
        [int]$SourceX, [int]$SourceY,
        [int]$SourceWidth, [int]$SourceHeight
    )

    $scale = [Math]::Min(14.0 / $SourceWidth,
                         14.0 / $SourceHeight)
    $width = [Math]::Max(1, [int][Math]::Round($SourceWidth * $scale))
    $height = [Math]::Max(1, [int][Math]::Round($SourceHeight * $scale))
    $left = ($AtlasX * 16) +
        [int][Math]::Floor((16 - $width) / 2.0)
    $top = 32 + [int][Math]::Floor((16 - $height) / 2.0)
    $destination = [Drawing.Rectangle]::FromLTRB(
        $left, $top, $left + $width, $top + $height)
    $sourceRectangle = [Drawing.Rectangle]::FromLTRB(
        $SourceX, $SourceY,
        $SourceX + $SourceWidth, $SourceY + $SourceHeight)
    $graphics.DrawImage($economyBitmap, $destination, $sourceRectangle,
                        [Drawing.GraphicsUnit]::Pixel)
}

try {
    # 第一行保持既有方块坐标；草方块顶部从草皮侧面的顶层裁出。
    Copy-Tile 0 0 8 10 81 34
    Copy-Tile 1 0 8 10 81 89
    Copy-Tile 2 0 95 10 81 89
    Copy-Tile 3 0 181 10 80 89
    Copy-Tile 4 0 266 10 80 89
    Copy-Tile 5 0 352 10 81 89
    Copy-Tile 6 0 440 10 80 89
    Copy-Tile 7 0 525 10 80 89
    Copy-Tile 8 0 610 10 81 89
    Copy-Tile 9 0 695 10 79 89
    Copy-Tile 10 0 787 10 60 89
    Copy-Tile 11 0 866 10 69 89
    Copy-Tile 12 0 941 10 75 89
    Copy-Tile 13 0 1024 10 78 89
    Copy-Tile 14 0 1107 10 76 89
    Copy-Tile 15 0 1189 10 58 89

    # 第二行给交互方块独立外观，不再共用树皮占位纹理。
    Copy-Tile 0 1 13 112 72 90
    Copy-Tile 1 1 96 112 79 89
    Copy-Tile 2 1 181 112 80 89
    Copy-Tile 3 1 267 114 79 88
    Copy-Tile 4 1 352 114 81 87
    Copy-Tile 5 1 440 113 80 88

    # 第三行是 HUD/手持物图标；跳过生成源中重复的第二把铁剑。
    Copy-Tile 0 2 25 240 50 64 -KeepAspect
    Copy-Tile 1 2 96 218 76 86 -KeepAspect
    Copy-Tile 2 2 181 223 72 80 -KeepAspect
    Copy-Tile 3 2 267 223 71 74 -KeepAspect
    Copy-Tile 4 2 354 223 79 80 -KeepAspect
    Copy-Tile 5 2 444 224 72 80 -KeepAspect
    Copy-Tile 6 2 524 219 71 85 -KeepAspect
    Copy-Tile 7 2 698 224 76 80 -KeepAspect
    Copy-Tile 8 2 787 219 80 85 -KeepAspect
    Copy-Tile 9 2 875 219 80 85 -KeepAspect

    # N10 资源经济图标来自独立的透明生成源，避免重绘 FS3 已冻结单元。
    Copy-EconomyTile 10 802 175 65 65
    Copy-EconomyTile 11 883 181 63 61
    Copy-EconomyTile 12 962 169 69 74
    Copy-EconomyTile 13 1042 175 65 68
    Copy-EconomyTile 14 1116 176 74 71

    $atlas.Save($outputPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $graphics.Dispose()
    $atlas.Dispose()
    $sourceBitmap.Dispose()
    $economyBitmap.Dispose()
}

Write-Host "[FS3_ATLAS] source=$sourcePath"
Write-Host "[FS3_ATLAS] economy_source=$economySourcePath"
Write-Host "[FS3_ATLAS] output=$outputPath size=256x256 tile=16"
Write-Host "[FS3_ATLAS] status=PASS"
