[CmdletBinding()]
param(
    [string]$ExePath = "",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $ExePath = Join-Path $RepoRoot "bin\HelloMine3D.exe"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $RepoRoot "bin\startup_error_validation"
}
$ExePath = [System.IO.Path]::GetFullPath($ExePath)
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)

if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    throw "Client executable not found: $ExePath"
}
if (Test-Path -LiteralPath $OutputDir) {
    Remove-Item -LiteralPath $OutputDir -Recurse -Force
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$requirements = @(
    [pscustomobject]@{
        Category = "shader"
        RelativePath = "media\ogre\HelloMine3DTerrain.vert"
        DiagnosticPath = "media/ogre/HelloMine3DTerrain.vert"
    },
    [pscustomobject]@{
        Category = "texture"
        RelativePath = "media\textures\DefaultPack.png"
        DiagnosticPath = "media/textures/DefaultPack.png"
    },
    [pscustomobject]@{
        Category = "block"
        RelativePath = "media\blocks\Stone.block"
        DiagnosticPath = "media/blocks/Stone.block"
    },
    [pscustomobject]@{
        Category = "enemy"
        RelativePath = "media\enemies\Base.enemy"
        DiagnosticPath = "media/enemies/Base.enemy"
    },
    [pscustomobject]@{
        Category = "food"
        RelativePath = "media\foods\Base.food"
        DiagnosticPath = "media/foods/Base.food"
    },
    [pscustomobject]@{
        Category = "objective"
        RelativePath = "media\objectives\Base.objective"
        DiagnosticPath = "media/objectives/Base.objective"
    },
    [pscustomobject]@{
        Category = "smelting"
        RelativePath = "media\smelting\Base.smelting"
        DiagnosticPath = "media/smelting/Base.smelting"
    }
)

foreach ($missing in $requirements) {
    $caseRoot = Join-Path $OutputDir $missing.Category
    New-Item -ItemType Directory -Path $caseRoot -Force | Out-Null

    $manifestPath = Join-Path $caseRoot "media\resource-manifest.txt"
    New-Item -ItemType Directory -Path (Split-Path -Parent $manifestPath) `
        -Force | Out-Null
    $manifestLines = @(
        '# HelloMine3D resource manifest v1',
        '',
        'block|media/blocks/Stone.block',
        'enemy|media/enemies/Base.enemy',
        'food|media/foods/Base.food',
        'objective|media/objectives/Base.objective',
        'runtime-template|bin/resource-packs.txt',
        'shader|media/ogre/HelloMine3DTerrain.vert',
        'smelting|media/smelting/Base.smelting',
        'texture|media/textures/DefaultPack.png'
    )
    [System.IO.File]::WriteAllText(
        $manifestPath, (($manifestLines -join "`n") + "`n"),
        (New-Object System.Text.UTF8Encoding($false)))
    $packConfig = Join-Path $caseRoot "bin\resource-packs.txt"
    New-Item -ItemType Directory -Path (Split-Path -Parent $packConfig) `
        -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $RepoRoot `
        "bin\resource-packs.txt") -Destination $packConfig -Force

    foreach ($present in $requirements) {
        if ($present.Category -eq $missing.Category) {
            continue
        }
        $source = Join-Path $RepoRoot $present.RelativePath
        $target = Join-Path $caseRoot $present.RelativePath
        $targetParent = Split-Path -Parent $target
        New-Item -ItemType Directory -Path $targetParent -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $target -Force
    }

    $reportPath = Join-Path $caseRoot "startup-error-report.txt"
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
        HELLOMINE3D_ROOT = $caseRoot
        HELLOMINE3D_STARTUP_ERROR_REPORT = $reportPath
        HELLOMINE3D_STARTUP_ERROR_NO_DIALOG = "1"
        HELLOMINE3D_VALIDATE_ONLY = $null
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
            throw "Failed to start client for missing $($missing.Category) case."
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

    if ($exitCode -eq 0) {
        throw "Missing $($missing.Category) case unexpectedly returned zero."
    }
    $expectedCategory =
        "Missing or empty effective $($missing.Category) resource"
    if ($stderr -notlike "*$expectedCategory*" -or
        $stderr -notlike "*$($missing.DiagnosticPath)*") {
        throw "Missing $($missing.Category) stderr did not name the failed resource: $stderr"
    }
    if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) {
        throw "Missing $($missing.Category) case did not produce the Windows error report."
    }
    $report = Get-Content -LiteralPath $reportPath -Raw
    if ($report -notlike "*ui=MessageBoxW*" -or
        $report -notlike "*dialog_requested=true*" -or
        $report -notlike "*$expectedCategory*" -or
        $report -notlike "*$($missing.DiagnosticPath)*") {
        throw "Missing $($missing.Category) Windows report is incomplete: $report"
    }

    Set-Content -LiteralPath (Join-Path $caseRoot "process.stdout.log") `
        -Value $stdout -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $caseRoot "process.stderr.log") `
        -Value $stderr -Encoding UTF8
    Write-Host "[STARTUP_ERROR_VERIFY] PASS category=$($missing.Category) exitCode=$exitCode resource=$($missing.DiagnosticPath)"
}

Write-Host "[STARTUP_ERROR_VERIFY] status=PASS cases=$($requirements.Count)"
