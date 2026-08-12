[CmdletBinding()]
param(
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptRoot "..")).Path
$Generator = Join-Path $ScriptRoot "generate_resource_manifest.ps1"
$Manifest = Join-Path $RepoRoot "media\resource-manifest.txt"
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $RepoRoot "bin\resource_manifest_validation"
}
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)
if (Test-Path -LiteralPath $OutputDir) {
    Remove-Item -LiteralPath $OutputDir -Recurse -Force
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

& $Generator -Root $RepoRoot -OutputPath $Manifest -Check

$currentLines = @(Get-Content -LiteralPath $Manifest)
$missingEntry = 'shader|media/ogre/HelloMine3DTerrain.vert'
$missingManifest = Join-Path $OutputDir "missing-entry.txt"
$missingLines = @($currentLines | Where-Object { $_ -ne $missingEntry })
[System.IO.File]::WriteAllText(
    $missingManifest, (($missingLines -join "`n") + "`n"),
    (New-Object System.Text.UTF8Encoding($false)))

$staleEntry = 'texture|media/textures/test.png'
if (-not (Test-Path -LiteralPath (Join-Path $RepoRoot "media\textures\test.png"))) {
    throw "Expected existing stale-entry fixture is missing."
}
$staleManifest = Join-Path $OutputDir "stale-entry.txt"
$staleLines = @($currentLines + $staleEntry)
[System.IO.File]::WriteAllText(
    $staleManifest, (($staleLines -join "`n") + "`n"),
    (New-Object System.Text.UTF8Encoding($false)))

function Invoke-ExpectedFailure {
    param(
        [string]$ManifestPath,
        [string]$ExpectedMarker,
        [string]$ExpectedEntry
    )

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = "powershell.exe"
    $startInfo.Arguments =
        "-NoProfile -ExecutionPolicy Bypass -File `"$Generator`" " +
        "-Root `"$RepoRoot`" -OutputPath `"$ManifestPath`" -Check"
    $startInfo.WorkingDirectory = $RepoRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw "Failed to start resource manifest negative check."
    }
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    $exitCode = $process.ExitCode
    $process.Dispose()
    $combined = $stdout + "`n" + $stderr

    if ($exitCode -eq 0 -or
        $combined -notlike "*$ExpectedMarker*" -or
        $combined -notlike "*$ExpectedEntry*") {
        throw "Manifest negative check did not fail as expected: exit=$exitCode output=$combined"
    }
    Write-Host "[RESOURCE_MANIFEST_VERIFY] PASS marker=$ExpectedMarker entry=$ExpectedEntry exitCode=$exitCode"
}

Invoke-ExpectedFailure $missingManifest 'MISSING_ENTRY' $missingEntry
Invoke-ExpectedFailure $staleManifest 'STALE_ENTRY' $staleEntry
Write-Host "[RESOURCE_MANIFEST_VERIFY] status=PASS cases=3"
