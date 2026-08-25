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
$OutputRoot = Join-Path $Root "media\music\tracks"
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$TrackPath = Join-Path $OutputRoot "quiet-horizons.wav"
$LicensePath = Join-Path $OutputRoot "LICENSE-HelloMine3D-Music.txt"

$ExpectedTrackHash =
    "2B17B204D9580DB67CAE410946C4AD0D494A3C9132F2F41178D152E79F45987F"
$ExpectedLicenseHash =
    "2090735E2B82765C0636B9CE22DF79C34B2DFF3007D1F0E0844C0A6883DC90F1"

if ($Check) {
    $actualTracks = @(
        Get-ChildItem -LiteralPath $OutputRoot -Filter "*.wav" -File)
    if ($actualTracks.Count -ne 1) {
        throw "Expected exactly one N12C music track, found $($actualTracks.Count)."
    }
    $trackHash = (Get-FileHash -LiteralPath $TrackPath `
        -Algorithm SHA256).Hash
    if ($trackHash -ne $ExpectedTrackHash) {
        throw "N12C music track hash mismatch: expected=$ExpectedTrackHash actual=$trackHash"
    }
    $licenseHash = (Get-FileHash -LiteralPath $LicensePath `
        -Algorithm SHA256).Hash
    if ($licenseHash -ne $ExpectedLicenseHash) {
        throw "N12C music license hash mismatch: expected=$ExpectedLicenseHash actual=$licenseHash"
    }
    Write-Host "[N12C_MUSIC] status=PASS mode=check tracks=1 license=1"
    return
}

if (-not ("HelloMine3DMusic.TrackGenerator" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.IO;

namespace HelloMine3DMusic
{
    public static class TrackGenerator
    {
        private const int SampleRate = 44100;
        private const int DurationSeconds = 20;
        private const double TwoPi = Math.PI * 2.0;

        private static double SmoothStep(double value)
        {
            value = Math.Max(0.0, Math.Min(1.0, value));
            return value * value * (3.0 - 2.0 * value);
        }

        public static void Write(string path)
        {
            int frames = SampleRate * DurationSeconds;
            int dataBytes = frames * 2;
            uint noise = 0x4d595df4u;
            double smoothNoise = 0.0;
            using (FileStream stream = new FileStream(
                path, FileMode.Create, FileAccess.Write, FileShare.None))
            using (BinaryWriter writer = new BinaryWriter(stream))
            {
                writer.Write(System.Text.Encoding.ASCII.GetBytes("RIFF"));
                writer.Write(36 + dataBytes);
                writer.Write(System.Text.Encoding.ASCII.GetBytes("WAVE"));
                writer.Write(System.Text.Encoding.ASCII.GetBytes("fmt "));
                writer.Write(16);
                writer.Write((short)1);
                writer.Write((short)1);
                writer.Write(SampleRate);
                writer.Write(SampleRate * 2);
                writer.Write((short)2);
                writer.Write((short)16);
                writer.Write(System.Text.Encoding.ASCII.GetBytes("data"));
                writer.Write(dataBytes);

                for (int frame = 0; frame < frames; ++frame)
                {
                    double t = (double)frame / SampleRate;
                    noise = unchecked(noise * 1664525u + 1013904223u);
                    double white = noise / 2147483647.5 - 1.0;
                    smoothNoise += (white - smoothNoise) * 0.0025;

                    double entrance = SmoothStep(t / 2.4);
                    double exit = SmoothStep((DurationSeconds - t) / 2.0);
                    double envelope = Math.Min(entrance, exit);
                    double breath = 0.72 + 0.16 * Math.Sin(TwoPi * 0.073 * t) +
                                    0.12 * Math.Sin(TwoPi * 0.117 * t + 0.8);
                    double drone = 0.42 * Math.Sin(TwoPi * 110.0 * t) +
                                   0.24 * Math.Sin(TwoPi * 164.81 * t + 0.3) +
                                   0.18 * Math.Sin(TwoPi * 220.0 * t + 1.1);

                    int pulseIndex = (int)(t / 2.5);
                    double pulseTime = t - pulseIndex * 2.5;
                    double[] notes = { 329.63, 392.00, 440.00, 493.88,
                                       440.00, 392.00, 329.63, 293.66 };
                    double note = notes[pulseIndex % notes.Length];
                    double bellEnvelope = pulseTime < 1.8
                        ? Math.Exp(-2.2 * pulseTime) *
                          SmoothStep(pulseTime / 0.025)
                        : 0.0;
                    double bell = Math.Sin(TwoPi * note * pulseTime) +
                                  0.35 * Math.Sin(TwoPi * note * 2.01 * pulseTime);
                    double air = smoothNoise *
                                 (0.35 + 0.15 * Math.Sin(TwoPi * 0.19 * t));
                    double value = envelope *
                        (0.24 * breath * drone +
                         0.16 * bellEnvelope * bell + 0.08 * air);
                    value = Math.Max(-1.0, Math.Min(1.0, value));
                    writer.Write((short)Math.Round(value * 32767.0));
                }
            }
        }
    }
}
"@
}

[HelloMine3DMusic.TrackGenerator]::Write($TrackPath)
Write-Host "[N12C_MUSIC] generated=quiet-horizons.wav duration_ms=20000 rate=44100 channels=1 bits=16"
Write-Host "[N12C_MUSIC] status=PASS tracks=1"
