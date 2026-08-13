[CmdletBinding()]
param(
    [string]$ExePath = "",
    [string]$SmokePath = "",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $ExePath = Join-Path $RepoRoot "bin\HelloMine3D.exe"
}
if ([string]::IsNullOrWhiteSpace($SmokePath)) {
    $SmokePath = Join-Path $RepoRoot `
        "bin\HelloMine3DResourcePackSmoke.exe"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $RepoRoot "bin\resource_pack_validation"
}
$ExePath = [System.IO.Path]::GetFullPath($ExePath)
$SmokePath = [System.IO.Path]::GetFullPath($SmokePath)
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)
$ExamplePack = (Resolve-Path (Join-Path $RepoRoot `
    "packs\example-stone")).Path

foreach ($required in @($ExePath, $SmokePath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Resource-pack validation executable is missing: $required"
    }
}
if (Test-Path -LiteralPath $OutputDir) {
    Remove-Item -LiteralPath $OutputDir -Recurse -Force
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

& $SmokePath
if ($LASTEXITCODE -ne 0) {
    throw "Resource-pack parser/resolver smoke failed with $LASTEXITCODE."
}

function Invoke-PackStartup {
    param(
        [string]$Name,
        [string]$PackList,
        [string]$ExpectedMarker
    )

    $manifestPath = Join-Path $OutputDir "$Name.effective.txt"
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $ExePath
    $startInfo.WorkingDirectory = Split-Path -Parent $ExePath
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    $environmentOverrides = [ordered]@{
        HELLOMINE3D_ROOT = $RepoRoot
        HELLOMINE3D_VALIDATE_ONLY = "1"
        HELLOMINE3D_SEED = "20260809"
        HELLOMINE3D_PLAYER_POSITION = "264 96 8"
        HELLOMINE3D_PLAYER_ROTATION = "0 0 0"
        HELLOMINE3D_TRANSPARENT_FIXTURE = "1"
        HELLOMINE3D_SAVE_DIR = Join-Path $OutputDir "$Name-save"
        HELLOMINE3D_RESOURCE_PACKS = $PackList
        HELLOMINE3D_EFFECTIVE_MANIFEST_OUT = $manifestPath
    }
    $previousEnvironment = @{}
    try {
        foreach ($key in $environmentOverrides.Keys) {
            $previousEnvironment[$key] = `
                [Environment]::GetEnvironmentVariable($key, "Process")
            [Environment]::SetEnvironmentVariable(
                $key, $environmentOverrides[$key], "Process")
        }
        if (-not $process.Start()) {
            throw "Failed to start $Name resource-pack client validation."
        }
    }
    finally {
        foreach ($key in $environmentOverrides.Keys) {
            [Environment]::SetEnvironmentVariable(
                $key, $previousEnvironment[$key], "Process")
        }
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    $exitCode = $process.ExitCode
    $process.Dispose()
    [System.IO.File]::WriteAllText(
        (Join-Path $OutputDir "$Name.stdout.log"), $stdout,
        (New-Object System.Text.UTF8Encoding($false)))
    [System.IO.File]::WriteAllText(
        (Join-Path $OutputDir "$Name.stderr.log"), $stderr,
        (New-Object System.Text.UTF8Encoding($false)))

    if ($exitCode -ne 0 -or -not $stdout.Contains($ExpectedMarker)) {
        throw "$Name startup failed: exit=$exitCode stdout=$stdout stderr=$stderr"
    }
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "$Name did not produce its effective manifest."
    }
    $lines = @(Get-Content -LiteralPath $manifestPath)
    if ($lines[0] -ne '# HelloMine3D effective resource manifest v1') {
        throw "$Name effective manifest has an invalid header."
    }
    $entries = @($lines | Where-Object { $_ -and -not $_.StartsWith('#') })
    $sorted = @($entries | Sort-Object -CaseSensitive)
    if ($entries.Count -ne 42 -or
        (Compare-Object $entries $sorted -SyncWindow 0)) {
        throw "$Name effective manifest is not a sorted 42-entry view."
    }
    return [pscustomobject]@{
        Path = $manifestPath
        Entries = $entries
        Hash = (Get-FileHash -LiteralPath $manifestPath `
            -Algorithm SHA256).Hash
    }
}

$base = Invoke-PackStartup -Name "base" -PackList "" `
    -ExpectedMarker "[RESOURCE_PACK] enabled=0 overrides=0 effective=42"
$packed = Invoke-PackStartup -Name "example" -PackList $ExamplePack `
    -ExpectedMarker "[RESOURCE_PACK] enabled=1 overrides=1 effective=42"

$expectedOverride = 'block|media/blocks/Stone.block|Example Stone'
if ($expectedOverride -notin $packed.Entries -or
    'block|media/blocks/Stone.block|base' -notin $base.Entries) {
    throw "The example pack did not own exactly the Stone logical resource."
}
$baseWithoutStone = @($base.Entries | Where-Object {
    $_ -notlike 'block|media/blocks/Stone.block|*'
})
$packedWithoutStone = @($packed.Entries | Where-Object {
    $_ -notlike 'block|media/blocks/Stone.block|*'
})
if (Compare-Object $baseWithoutStone $packedWithoutStone -SyncWindow 0) {
    throw "The example pack changed resources outside its declared override."
}

Write-Host "[RESOURCE_PACK_VERIFY] PASS base_hash=$($base.Hash)"
Write-Host "[RESOURCE_PACK_VERIFY] PASS packed_hash=$($packed.Hash) override=$expectedOverride"
Write-Host "[RESOURCE_PACK_VERIFY] status=PASS resolver_checks=18 startup_cases=2 entries=42"
