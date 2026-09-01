[CmdletBinding()]
param(
    [ValidateSet("2017", "2022")]
    [string]$VisualStudioVersion = "2017",
    [switch]$SkipRealWindow
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$premake = Join-Path $repoRoot "tools\premake\premake5.exe"
$solution = Join-Path $repoRoot "build\HelloMine3D.sln"
$binDirectory = Join-Path $repoRoot "bin"
$startupErrorVerifier = Join-Path $repoRoot "tools\validate_startup_errors.ps1"
$resourceManifestVerifier = Join-Path $repoRoot `
    "tools\validate_resource_manifest.ps1"
$terrainAtlasVerifier = Join-Path $repoRoot `
    "tools\validate_terrain_atlas.ps1"
$audioSampleVerifier = Join-Path $repoRoot `
    "tools\generate_n12b_audio_samples.ps1"
$musicTrackVerifier = Join-Path $repoRoot `
    "tools\generate_n12c_music_track.ps1"
$performanceComparisonVerifier = Join-Path $repoRoot `
    "tools\validate_perf_comparison.ps1"
$stage10PerformanceVerifier = Join-Path $repoRoot `
    "tools\validate_stage10_visual_performance.ps1"
$manualInputRecordVerifier = Join-Path $repoRoot `
    "tools\validate_manual_input_record.ps1"
$physicalInputV2RecordVerifier = Join-Path $repoRoot `
    "tools\validate_physical_input_v2_record.ps1"
$developerVisualRecordVerifier = Join-Path $repoRoot `
    "tools\validate_developer_visual_record.ps1"
$resourcePackVerifier = Join-Path $repoRoot `
    "tools\validate_resource_packs.ps1"
$windowsPackager = Join-Path $repoRoot `
    "tools\package_windows_release.ps1"
$crashDiagnosticsVerifier = Join-Path $repoRoot `
    "tools\validate_crash_diagnostics.ps1"
$worldResponsibilityMapVerifier = Join-Path $repoRoot `
    "tools\validate_world_responsibility_map.ps1"
$chunkRuntimeBoundaryVerifier = Join-Path $repoRoot `
    "tools\validate_chunk_runtime_boundary.ps1"
$worldSimulationBoundaryVerifier = Join-Path $repoRoot `
    "tools\validate_world_simulation_boundary.ps1"
$eventCommandQueryBoundaryVerifier = Join-Path $repoRoot `
    "tools\validate_event_command_query_boundary.ps1"
$simulationMetricsBoundaryVerifier = Join-Path $repoRoot `
    "tools\validate_simulation_metrics_boundary.ps1"
$architectureLabDocumentationVerifier = Join-Path $repoRoot `
    "tools\validate_architecture_lab_documentation.ps1"
$chunkResidencyStateMachineVerifier = Join-Path $repoRoot `
    "tools\validate_chunk_residency_state_machine.ps1"
$streamingDemandModelVerifier = Join-Path $repoRoot `
    "tools\validate_streaming_demand_model.ps1"
$worldJobSchedulerVerifier = Join-Path $repoRoot `
    "tools\validate_world_job_scheduler.ps1"
$worldJobCancellationVerifier = Join-Path $repoRoot `
    "tools\validate_world_job_cancellation.ps1"
$streamingBackpressureVerifier = Join-Path $repoRoot `
    "tools\validate_streaming_backpressure.ps1"

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

function Invoke-HiddenExecutable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory
    )

    $token = [Guid]::NewGuid().ToString("N")
    $stdoutPath = Join-Path ([IO.Path]::GetTempPath()) `
        "hellomine3d-$token.stdout.log"
    $stderrPath = Join-Path ([IO.Path]::GetTempPath()) `
        "hellomine3d-$token.stderr.log"
    try {
        $process = Start-Process -FilePath $FilePath `
            -WorkingDirectory $WorkingDirectory `
            -WindowStyle Hidden `
            -RedirectStandardOutput $stdoutPath `
            -RedirectStandardError $stderrPath `
            -PassThru -Wait
        if (Test-Path -LiteralPath $stdoutPath) {
            Get-Content -LiteralPath $stdoutPath | Write-Host
        }
        if (Test-Path -LiteralPath $stderrPath) {
            Get-Content -LiteralPath $stderrPath | Write-Host
        }
        $global:LASTEXITCODE = $process.ExitCode
    }
    finally {
        if (Test-Path -LiteralPath $stdoutPath) {
            Remove-Item -LiteralPath $stdoutPath -Force
        }
        if (Test-Path -LiteralPath $stderrPath) {
            Remove-Item -LiteralPath $stderrPath -Force
        }
    }
}

function Find-MSBuild {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Version
    )

    $vswhere = Join-Path ${env:ProgramFiles(x86)} `
        "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $versionRange = if ($Version -eq "2017") {
            "[15.0,16.0)"
        }
        else { "[17.0,18.0)" }
        $found = & $vswhere -latest -version $versionRange `
            -products * -requires Microsoft.Component.MSBuild `
            -find "MSBuild\**\Bin\MSBuild.exe" |
            Select-Object -First 1
        if ($found) {
            return $found
        }
    }

    $programFilesRoot = if ($Version -eq "2017") {
        ${env:ProgramFiles(x86)}
    }
    else { $env:ProgramFiles }
    $msbuildRelative = if ($Version -eq "2017") {
        "MSBuild\15.0\Bin\MSBuild.exe"
    }
    else { "MSBuild\Current\Bin\MSBuild.exe" }
    foreach ($edition in @("Community", "Professional", "Enterprise")) {
        $candidate = Join-Path $programFilesRoot `
            "Microsoft Visual Studio\$Version\$edition\$msbuildRelative"
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    throw "MSBuild for Visual Studio $Version was not found. Install its Desktop development with C++ workload."
}

if (-not (Test-Path -LiteralPath $premake)) {
    throw "Bundled Premake executable not found: $premake"
}

$premakeAction = "vs$VisualStudioVersion"
$expectedToolset = if ($VisualStudioVersion -eq "2017") {
    "v141"
}
else { "v143" }
$msbuild = Find-MSBuild -Version $VisualStudioVersion
Push-Location $repoRoot
try {
    Write-Host "[BUILD_VERIFY] visual_studio=$VisualStudioVersion platform_toolset=$expectedToolset"
    Write-Host "[BUILD_VERIFY] msbuild=$msbuild"
    Invoke-Checked "AL-A1 World responsibility map" {
        & $worldResponsibilityMapVerifier -Root $repoRoot
    }

    Invoke-Checked "AL-A2 Chunk runtime boundary" {
        & $chunkRuntimeBoundaryVerifier -Root $repoRoot
    }

    Invoke-Checked "AL-A3 World simulation boundary" {
        & $worldSimulationBoundaryVerifier -Root $repoRoot
    }

    Invoke-Checked "AL-A4 Event command query boundary" {
        & $eventCommandQueryBoundaryVerifier -Root $repoRoot
    }

    Invoke-Checked "AL-A5 Simulation metrics boundary" {
        & $simulationMetricsBoundaryVerifier -Root $repoRoot
    }

    Invoke-Checked "AL-A6 Architecture Lab documentation pipeline" {
        & $architectureLabDocumentationVerifier -Root $repoRoot -SelfTest
    }

    Invoke-Checked "B1 Chunk residency state machine" {
        & $chunkResidencyStateMachineVerifier -Root $repoRoot
    }

    Invoke-Checked "B2 Streaming demand model" {
        & $streamingDemandModelVerifier -Root $repoRoot
    }

    Invoke-Checked "B3 World job scheduler" {
        & $worldJobSchedulerVerifier -Root $repoRoot
    }

    Invoke-Checked "B4 World job cancellation" {
        & $worldJobCancellationVerifier -Root $repoRoot
    }

    Invoke-Checked "B5 Streaming backpressure" {
        & $streamingBackpressureVerifier -Root $repoRoot
    }

    Invoke-Checked "Resource manifest" {
        & $resourceManifestVerifier
    }

    Invoke-Checked "Stage 10 terrain atlas contract" {
        & $terrainAtlasVerifier -Root $repoRoot
    }

    Invoke-Checked "N12B sampled audio assets" {
        & $audioSampleVerifier -Root $repoRoot -Check
    }

    Invoke-Checked "N12C streamed music asset" {
        & $musicTrackVerifier -Root $repoRoot -Check
    }

    Invoke-Checked "Performance comparison fixtures" {
        & $performanceComparisonVerifier
    }

    Invoke-Checked "Stage 10 visual performance supplement" {
        & $stage10PerformanceVerifier
    }

    Invoke-Checked "Manual input protocol schema" {
        & $manualInputRecordVerifier `
            -RecordPath (Join-Path $repoRoot `
                "docs\archive\manual-input-record-v1.template.txt") `
            -AllowNotRun
    }

    Invoke-Checked "Physical Input v2 protocol schema" {
        & $physicalInputV2RecordVerifier `
            -RecordPath (Join-Path $repoRoot `
                "docs\archive\physical-input-record-v2.template.txt") `
            -AllowNotRun
    }

    Invoke-Checked "Developer visual record schema" {
        & $developerVisualRecordVerifier `
            -RecordPath (Join-Path $repoRoot `
                "docs\reports\developer-visual-record-v10b2.txt") `
            -RequirePass
    }

    Invoke-Checked "V10B3 developer visual record" {
        & $developerVisualRecordVerifier `
            -RecordPath (Join-Path $repoRoot `
                "docs\reports\developer-visual-record-v10b3.txt") `
            -RequirePass
    }

    Invoke-Checked "V10C developer visual record" {
        & $developerVisualRecordVerifier `
            -RecordPath (Join-Path $repoRoot `
                "docs\reports\developer-visual-record-v10c.txt") `
            -RequirePass
    }

    Invoke-Checked "V10D developer visual record" {
        & $developerVisualRecordVerifier `
            -RecordPath (Join-Path $repoRoot `
                "docs\reports\developer-visual-record-v10d.txt") `
            -RequirePass
    }

    Invoke-Checked "V10E developer visual record" {
        & $developerVisualRecordVerifier `
            -RecordPath (Join-Path $repoRoot `
                "docs\reports\developer-visual-record-v10e.txt") `
            -RequirePass
    }

    Invoke-Checked "Generate VS$VisualStudioVersion projects" {
        & $premake --os=windows --file=premake/premake.lua $premakeAction
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
        if ($projectText -notmatch (
                '<PlatformToolset>' + [regex]::Escape($expectedToolset) +
                '</PlatformToolset>')) {
            throw "Generated project does not use expected toolset $expectedToolset`: $projectPath"
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
            Invoke-Checked "$configuration $test" {
                Invoke-HiddenExecutable -FilePath $testPath `
                    -WorkingDirectory $binDirectory
            }
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
            $previousHiddenWindow = $env:HELLOMINE3D_WINDOW_HIDDEN
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
                $env:HELLOMINE3D_WINDOW_HIDDEN = "1"
                Push-Location $binDirectory
                try {
                    Invoke-HiddenExecutable `
                        -FilePath (Join-Path $binDirectory "HelloMine3D.exe") `
                        -WorkingDirectory $binDirectory
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
                $env:HELLOMINE3D_WINDOW_HIDDEN = $previousHiddenWindow
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
        $crashArguments = @{
            ExePath = Join-Path $binDirectory "HelloMine3D.exe"
            OutputDir = Join-Path $binDirectory `
                "crash_diagnostics_validation"
        }
        if ($SkipRealWindow) {
            $crashArguments.SkipRealWindow = $true
        }
        & $crashDiagnosticsVerifier @crashArguments
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
        $packageArguments = @{
            IncludePack = "example-stone"
        }
        if ($SkipRealWindow) {
            $packageArguments.SkipRealWindow = $true
        }
        & $windowsPackager @packageArguments
    }
}
finally {
    Pop-Location
}

$realWindowStatus = if ($SkipRealWindow) { "DEFERRED" } else { "PASS" }
Write-Host "[BUILD_VERIFY] status=PASS real_window=$realWindowStatus"
