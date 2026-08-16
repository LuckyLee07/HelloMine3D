[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$premake = Join-Path $repoRoot "tools\premake\premake5.exe"
$solution = Join-Path $repoRoot "build\HelloMine3D.sln"
$binDirectory = Join-Path $repoRoot "bin"
$startupErrorVerifier = Join-Path $repoRoot "tools\validate_startup_errors.ps1"
$resourceManifestVerifier = Join-Path $repoRoot `
    "tools\validate_resource_manifest.ps1"
$performanceComparisonVerifier = Join-Path $repoRoot `
    "tools\validate_perf_comparison.ps1"
$manualInputRecordVerifier = Join-Path $repoRoot `
    "tools\validate_manual_input_record.ps1"
$resourcePackVerifier = Join-Path $repoRoot `
    "tools\validate_resource_packs.ps1"
$windowsPackager = Join-Path $repoRoot `
    "tools\package_windows_release.ps1"
$crashDiagnosticsVerifier = Join-Path $repoRoot `
    "tools\validate_crash_diagnostics.ps1"

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,
        [Parameter(Mandatory = $true)]
        [scriptblock]$Command
    )

    Write-Host "[BUILD_VERIFY] $Label"
    $global:LASTEXITCODE = 0
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
    Invoke-Checked "Resource manifest" {
        & $resourceManifestVerifier
    }

    Invoke-Checked "Performance comparison fixtures" {
        & $performanceComparisonVerifier
    }

    Invoke-Checked "Manual input protocol schema" {
        & $manualInputRecordVerifier `
            -RecordPath (Join-Path $repoRoot `
                "docs\manual-input-record-v1.template.txt") `
            -AllowNotRun
    }

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

    $clientProject = Join-Path $repoRoot `
        "build\HelloMine3D\HelloMine3D.vcxproj"
    $crashProject = Join-Path $repoRoot `
        "build\HelloMine3DCrashDiagnosticsSmoke\HelloMine3DCrashDiagnosticsSmoke.vcxproj"
    foreach ($projectPath in @($clientProject, $crashProject)) {
        if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
            throw "Generated crash diagnostics project is missing: $projectPath"
        }
        $projectText = Get-Content -LiteralPath $projectPath -Raw
        if ($projectText -notmatch 'WindowsCrashDiagnostics\.cpp' -or
            $projectText -notmatch '(?i)dbghelp\.lib') {
            throw "Generated project does not record the Windows DbgHelp backend: $projectPath"
        }
    }
    $portableCrashSources = @(
        "src\HelloMine3D\Diagnostics\CrashDiagnostics.h",
        "src\HelloMine3D\Diagnostics\CrashDiagnostics.cpp",
        "src\HelloMine3D\Diagnostics\CrashDiagnosticsPlatform.h",
        "src\HelloMine3D\Diagnostics\CrashDiagnosticsPlatformStub.cpp"
    )
    foreach ($relativePath in $portableCrashSources) {
        $sourceText = Get-Content -LiteralPath `
            (Join-Path $repoRoot $relativePath) -Raw
        if ($sourceText -match `
            '(?i)EXCEPTION_POINTERS|MINIDUMP_|windows\.h|dbghelp\.h') {
            throw "Portable crash boundary leaks a Windows exception type: $relativePath"
        }
    }
    Write-Host "[BUILD_VERIFY] crash diagnostics build boundary valid"

    $tests = @(
        "HelloMine3DCoordinateTests.exe",
        "HelloMine3DMeshDirtyTests.exe",
        "HelloMine3DSaveLoadSmoke.exe",
        "HelloMine3DEntityLifecycleSmoke.exe",
        "HelloMine3DWorldRuntimeSmoke.exe",
        "HelloMine3DSoak.exe",
        "HelloMine3DResourcePackSmoke.exe",
        "HelloMine3DRecipeSmoke.exe",
        "HelloMine3DWorldCatalogueSmoke.exe",
        "HelloMine3DStorageTransactionSmoke.exe",
        "HelloMine3DWorldBackupSmoke.exe",
        "HelloMine3DOperationTimingSmoke.exe",
        "HelloMine3DCrashDiagnosticsSmoke.exe"
    )

    foreach ($configuration in @("Debug", "Release")) {
        Invoke-Checked "Build $configuration x64" {
            & $msbuild $solution `
                "/t:Rebuild" `
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

        Invoke-Checked "$configuration validation-only client" {
            $previousValidateOnly = $env:HELLOMINE3D_VALIDATE_ONLY
            $previousRoot = $env:HELLOMINE3D_ROOT
            $previousSeed = $env:HELLOMINE3D_SEED
            $previousPosition = $env:HELLOMINE3D_PLAYER_POSITION
            $previousRotation = $env:HELLOMINE3D_PLAYER_ROTATION
            $previousSaveDir = $env:HELLOMINE3D_SAVE_DIR
            $previousTransparentFixture =
                $env:HELLOMINE3D_TRANSPARENT_FIXTURE
            $previousResourcePacks = $env:HELLOMINE3D_RESOURCE_PACKS
            $previousEffectiveManifest =
                $env:HELLOMINE3D_EFFECTIVE_MANIFEST_OUT
            $validationSaveDir = Join-Path $binDirectory `
                "build_verify_validation_$configuration"
            try {
                if (Test-Path -LiteralPath $validationSaveDir) {
                    Remove-Item -LiteralPath $validationSaveDir `
                        -Recurse -Force
                }
                $env:HELLOMINE3D_VALIDATE_ONLY = "1"
                $env:HELLOMINE3D_ROOT = $repoRoot
                $env:HELLOMINE3D_SEED = "20260809"
                $env:HELLOMINE3D_PLAYER_POSITION = "264 96 8"
                $env:HELLOMINE3D_PLAYER_ROTATION = "0 0 0"
                $env:HELLOMINE3D_SAVE_DIR = $validationSaveDir
                $env:HELLOMINE3D_TRANSPARENT_FIXTURE = "1"
                $env:HELLOMINE3D_RESOURCE_PACKS = ""
                $env:HELLOMINE3D_EFFECTIVE_MANIFEST_OUT = ""
                Push-Location $binDirectory
                try {
                    & (Join-Path $binDirectory "HelloMine3D.exe")
                }
                finally {
                    Pop-Location
                }
            }
            finally {
                $env:HELLOMINE3D_VALIDATE_ONLY = $previousValidateOnly
                $env:HELLOMINE3D_ROOT = $previousRoot
                $env:HELLOMINE3D_SEED = $previousSeed
                $env:HELLOMINE3D_PLAYER_POSITION = $previousPosition
                $env:HELLOMINE3D_PLAYER_ROTATION = $previousRotation
                $env:HELLOMINE3D_SAVE_DIR = $previousSaveDir
                $env:HELLOMINE3D_TRANSPARENT_FIXTURE =
                    $previousTransparentFixture
                $env:HELLOMINE3D_RESOURCE_PACKS = $previousResourcePacks
                $env:HELLOMINE3D_EFFECTIVE_MANIFEST_OUT =
                    $previousEffectiveManifest
            }
        }

        Invoke-Checked "$configuration resource-pack validation" {
            & $resourcePackVerifier `
                -ExePath (Join-Path $binDirectory "HelloMine3D.exe") `
                -SmokePath (Join-Path $binDirectory `
                    "HelloMine3DResourcePackSmoke.exe") `
                -OutputDir (Join-Path $binDirectory `
                    "resource_pack_validation_$configuration")
        }

        Invoke-Checked "$configuration startup error diagnostics" {
            & $startupErrorVerifier `
                -ExePath (Join-Path $binDirectory "HelloMine3D.exe") `
                -OutputDir (Join-Path $binDirectory `
                    "startup_error_validation_$configuration")
        }
    }

    Invoke-Checked "Release local crash diagnostics" {
        & $crashDiagnosticsVerifier `
            -ExePath (Join-Path $binDirectory "HelloMine3D.exe") `
            -OutputDir (Join-Path $binDirectory `
                "crash_diagnostics_validation")
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

    Invoke-Checked "Release clean-root package" {
        & $windowsPackager -IncludePack "example-stone" -SkipRealWindow
    }
}
finally {
    Pop-Location
}

Write-Host "[BUILD_VERIFY] status=PASS"
