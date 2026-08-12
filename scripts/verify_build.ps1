[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$premake = Join-Path $repoRoot "tools\premake\premake5.exe"
$solution = Join-Path $repoRoot "build\HelloMine3D.sln"
$binDirectory = Join-Path $repoRoot "bin"
$startupErrorVerifier = Join-Path $repoRoot "tools\validate_startup_errors.ps1"

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,
        [Parameter(Mandatory = $true)]
        [scriptblock]$Command
    )

    Write-Host "[BUILD_VERIFY] $Label"
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE."
    }
}

function Find-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} `
        "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -products * `
            -requires Microsoft.Component.MSBuild `
            -find "MSBuild\**\Bin\MSBuild.exe" |
            Select-Object -First 1
        if ($found) {
            return $found
        }
    }

    foreach ($edition in @("Community", "Professional", "Enterprise")) {
        $candidate = Join-Path $env:ProgramFiles `
            "Microsoft Visual Studio\2022\$edition\MSBuild\Current\Bin\MSBuild.exe"
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $command = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }
    throw "MSBuild was not found. Install Visual Studio 2022 with Desktop development with C++."
}

if (-not (Test-Path -LiteralPath $premake)) {
    throw "Bundled Premake executable not found: $premake"
}

$msbuild = Find-MSBuild
Push-Location $repoRoot
try {
    Invoke-Checked "Generate VS2022 projects" {
        & $premake --os=windows --file=premake/premake.lua vs2022
    }

    $oisProject = Join-Path $repoRoot "build\External\ois\ois.vcxproj"
    if (-not (Test-Path -LiteralPath $oisProject -PathType Leaf)) {
        throw "Generated OIS project is missing: $oisProject"
    }
    $oisProjectText = Get-Content -LiteralPath $oisProject -Raw
    if ($oisProjectText -match 'src[\\/]win32[\\/]extras') {
        throw "Generated OIS project still contains the Win32 demo sources."
    }
    Write-Host "[BUILD_VERIFY] OIS source inventory valid"

    $tests = @(
        "HelloMine3DCoordinateTests.exe",
        "HelloMine3DMeshDirtyTests.exe",
        "HelloMine3DSaveLoadSmoke.exe",
        "HelloMine3DEntityLifecycleSmoke.exe",
        "HelloMine3DWorldRuntimeSmoke.exe"
    )

    foreach ($configuration in @("Debug", "Release")) {
        Invoke-Checked "Build $configuration x64" {
            & $msbuild $solution `
                "/p:Configuration=$configuration" `
                "/p:Platform=x64" /m /nologo
        }

        foreach ($test in $tests) {
            $testPath = Join-Path $binDirectory $test
            if (-not (Test-Path -LiteralPath $testPath)) {
                throw "Expected test executable was not built: $testPath"
            }
            Invoke-Checked "$configuration $test" { & $testPath }
        }

        Invoke-Checked "$configuration startup error diagnostics" {
            & $startupErrorVerifier `
                -ExePath (Join-Path $binDirectory "HelloMine3D.exe") `
                -OutputDir (Join-Path $binDirectory `
                    "startup_error_validation_$configuration")
        }
    }

    $expectedExecutables = @("HelloMine3D.exe") + $tests
    $unexpectedExecutables = @(
        Get-ChildItem -LiteralPath $binDirectory -Filter "HelloMine3D*.exe" |
            Where-Object { $_.Name -notin $expectedExecutables }
    )
    if ($unexpectedExecutables.Count -gt 0) {
        throw "Unexpected stale executables in bin/: $($unexpectedExecutables.Name -join ', ')"
    }
    Write-Host "[BUILD_VERIFY] executable inventory valid"
}
finally {
    Pop-Location
}

Write-Host "[BUILD_VERIFY] status=PASS"
