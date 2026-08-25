[CmdletBinding()]
param(
    [string]$Root = "",
    [switch]$Check
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Join-Path $ScriptRoot ".."
}
$Root = (Resolve-Path $Root).Path
$OutputRoot = Join-Path $Root "media\audio\samples"
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null

$ExpectedHashes = [ordered]@{
    "ambient-wind.wav" = "4556C128DEA877888C086CC96C5380FC78CA4F5B22AC8DAFC98AB86DE65BA12B"
    "block-break.wav" = "E45325C54FE291DF408AC5C28C3418E83F24FAB8771C04D29286CF0A1E6ACE5B"
    "block-place.wav" = "5FDC6C27327FA60E4F91ED2880E3F9AE17BDF309B4B54A60EE242CDECF055700"
    "combat-guard.wav" = "ACC3F99056429DAEB4D9BFF8DF9A6A1364BC262761F1D14C7272538B22F2309F"
    "combat-hit.wav" = "326EC462B46478C3B7C9EBD00CF4244C24618C915F8AE037A9E39C2835B3EB1D"
    "combat-windup.wav" = "9FB159E62DF2ABD991FDB3847797FA2789F97F0924395DB46EA3670491556004"
    "craft-success.wav" = "04BF873C721EC66478F0B57F73F2ACF6C6597122EF1C055B704E644146204FD7"
    "item-pickup.wav" = "AACE2548AC2BEC6CA49CB4A28900EB79A0CBBEC6E960E8666EC29978444A3E43"
    "ui-click.wav" = "9EC13A5DC26B7BFDB96256107A6383B9AA1BBDC791224B8A45B88BF37BA21454"
}
$ExpectedLicenseHash =
    "AF64D609430DE21C78BF0B72B8E3BA62F1C0A0FD03CBC45ED140B1302208FB7D"

if ($Check) {
    $actualSamples = @(
        Get-ChildItem -LiteralPath $OutputRoot -Filter "*.wav" -File)
    if ($actualSamples.Count -ne $ExpectedHashes.Count) {
        throw "Expected $($ExpectedHashes.Count) WAV files, found $($actualSamples.Count)."
    }
    foreach ($entry in $ExpectedHashes.GetEnumerator()) {
        $path = Join-Path $OutputRoot $entry.Key
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Missing N12B audio sample: $path"
        }
        $actualHash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if ($actualHash -ne $entry.Value) {
            throw "N12B audio sample hash mismatch: $($entry.Key) expected=$($entry.Value) actual=$actualHash"
        }
    }
    $licensePath = Join-Path $OutputRoot "LICENSE-HelloMine3D-Audio.txt"
    $licenseHash = (Get-FileHash -LiteralPath $licensePath `
        -Algorithm SHA256).Hash
    if ($licenseHash -ne $ExpectedLicenseHash) {
        throw "N12B audio license hash mismatch: expected=$ExpectedLicenseHash actual=$licenseHash"
    }
    Write-Host "[N12B_AUDIO] status=PASS mode=check samples=$($ExpectedHashes.Count) license=1"
    return
}

$SampleRate = 44100
$TwoPi = 2.0 * [Math]::PI

function Get-Envelope {
    param(
        [double]$Time,
        [double]$Duration,
        [double]$Attack,
        [double]$Release
    )
    $attackGain = [Math]::Min(1.0, $Time / [Math]::Max($Attack, 0.0001))
    $releaseGain = [Math]::Min(
        1.0, ($Duration - $Time) / [Math]::Max($Release, 0.0001))
    return [Math]::Max(0.0, [Math]::Min($attackGain, $releaseGain))
}

function Write-MonoWave {
    param(
        [string]$Name,
        [double]$Duration,
        [int]$Seed,
        [scriptblock]$Render
    )

    $frameCount = [int][Math]::Round($Duration * $SampleRate)
    $samples = New-Object 'System.Int16[]' $frameCount
    [uint64]$noiseState = [uint32]$Seed
    [double]$smoothNoise = 0.0
    for ($frame = 0; $frame -lt $frameCount; ++$frame) {
        $noiseState = (($noiseState * 1664525L) + 1013904223L) -band 0xffffffffL
        $whiteNoise = ([double]$noiseState / 2147483647.5) - 1.0
        $smoothNoise += ($whiteNoise - $smoothNoise) * 0.085
        $time = [double]$frame / [double]$SampleRate
        $value = & $Render $time $Duration $whiteNoise $smoothNoise $TwoPi
        $value = [Math]::Max(-1.0, [Math]::Min(1.0, [double]$value))
        $samples[$frame] = [int16][Math]::Round($value * 32767.0)
    }

    $path = Join-Path $OutputRoot $Name
    $stream = New-Object System.IO.FileStream(
        $path, [System.IO.FileMode]::Create,
        [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try {
        $writer = New-Object System.IO.BinaryWriter($stream)
        try {
            $dataBytes = $samples.Length * 2
            $writer.Write([System.Text.Encoding]::ASCII.GetBytes("RIFF"))
            $writer.Write([int](36 + $dataBytes))
            $writer.Write([System.Text.Encoding]::ASCII.GetBytes("WAVE"))
            $writer.Write([System.Text.Encoding]::ASCII.GetBytes("fmt "))
            $writer.Write([int]16)
            $writer.Write([int16]1)
            $writer.Write([int16]1)
            $writer.Write([int]$SampleRate)
            $writer.Write([int]($SampleRate * 2))
            $writer.Write([int16]2)
            $writer.Write([int16]16)
            $writer.Write([System.Text.Encoding]::ASCII.GetBytes("data"))
            $writer.Write([int]$dataBytes)
            foreach ($sample in $samples) {
                $writer.Write([int16]$sample)
            }
        }
        finally {
            $writer.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
    Write-Host "[N12B_AUDIO] generated=$Name frames=$frameCount"
}

Write-MonoWave "ui-click.wav" 0.070 1201 {
    param($t, $d, $white, $smooth, $tau)
    $env = Get-Envelope $t $d 0.002 0.025
    $tone = [Math]::Sin($tau * (980.0 - 280.0 * $t / $d) * $t)
    return 0.48 * $env * ($tone + 0.18 * $white)
}

Write-MonoWave "block-break.wav" 0.210 1202 {
    param($t, $d, $white, $smooth, $tau)
    $env = Get-Envelope $t $d 0.004 0.080
    $grit = 0.62 * $white + 0.38 * $smooth
    $body = [Math]::Sin($tau * (118.0 - 32.0 * $t / $d) * $t)
    return 0.64 * $env * (0.78 * $grit + 0.22 * $body)
}

Write-MonoWave "block-place.wav" 0.135 1203 {
    param($t, $d, $white, $smooth, $tau)
    $env = Get-Envelope $t $d 0.002 0.060
    $body = [Math]::Sin($tau * 94.0 * $t)
    $knock = [Math]::Sin($tau * 188.0 * $t)
    return 0.58 * $env * (0.58 * $body + 0.24 * $knock + 0.18 * $smooth)
}

Write-MonoWave "item-pickup.wav" 0.190 1204 {
    param($t, $d, $white, $smooth, $tau)
    $env = Get-Envelope $t $d 0.003 0.050
    $frequency = 620.0 + 880.0 * $t / $d
    $sparkle = [Math]::Sin($tau * $frequency * $t)
    return 0.42 * $env * ($sparkle + 0.22 * [Math]::Sin($tau * 2.01 * $frequency * $t))
}

Write-MonoWave "craft-success.wav" 0.430 1205 {
    param($t, $d, $white, $smooth, $tau)
    $env = Get-Envelope $t $d 0.006 0.110
    $first = [Math]::Sin($tau * 523.25 * $t)
    $secondGate = if ($t -ge 0.115) { 1.0 } else { 0.0 }
    $second = [Math]::Sin($tau * 659.25 * ($t - 0.115)) * $secondGate
    $thirdGate = if ($t -ge 0.235) { 1.0 } else { 0.0 }
    $third = [Math]::Sin($tau * 783.99 * ($t - 0.235)) * $thirdGate
    return 0.30 * $env * ($first + 0.78 * $second + 0.64 * $third)
}

Write-MonoWave "combat-hit.wav" 0.155 1206 {
    param($t, $d, $white, $smooth, $tau)
    $env = Get-Envelope $t $d 0.0015 0.065
    $body = [Math]::Sin($tau * (82.0 - 24.0 * $t / $d) * $t)
    return 0.72 * $env * (0.58 * $white + 0.42 * $body)
}

Write-MonoWave "combat-windup.wav" 0.330 1207 {
    param($t, $d, $white, $smooth, $tau)
    $env = Get-Envelope $t $d 0.018 0.075
    $frequency = 145.0 + 310.0 * ($t / $d) * ($t / $d)
    $warning = [Math]::Sin($tau * $frequency * $t)
    return 0.46 * $env * (0.82 * $warning + 0.18 * $smooth)
}

Write-MonoWave "combat-guard.wav" 0.220 1208 {
    param($t, $d, $white, $smooth, $tau)
    $env = Get-Envelope $t $d 0.0015 0.095
    $ring = [Math]::Sin($tau * 920.0 * $t) +
            0.52 * [Math]::Sin($tau * 1380.0 * $t)
    return 0.43 * $env * ($ring + 0.20 * $white)
}

Write-MonoWave "ambient-wind.wav" 1.800 1209 {
    param($t, $d, $white, $smooth, $tau)
    $env = Get-Envelope $t $d 0.180 0.260
    $gust = 0.62 + 0.22 * [Math]::Sin($tau * 0.71 * $t) +
            0.16 * [Math]::Sin($tau * 1.37 * $t)
    return 0.38 * $env * $gust * $smooth
}

Write-Host "[N12B_AUDIO] status=PASS samples=9 rate=$SampleRate channels=1 bits=16"
