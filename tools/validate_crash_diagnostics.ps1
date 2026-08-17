[CmdletBinding()]
param(
    [string]$ExePath = "",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ($env:OS -ne "Windows_NT") {
    throw "The H1 minidump harness must run on Windows."
}

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BinRoot = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot "bin"))
if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $ExePath = Join-Path $BinRoot "HelloMine3D.exe"
}
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    throw "Crash diagnostics client is missing: $ExePath"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $BinRoot "crash_diagnostics_validation"
}
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)
if (-not $OutputDir.StartsWith(
        $BinRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Crash diagnostics output must stay below $BinRoot"
}

if (Test-Path -LiteralPath $OutputDir) {
    Remove-Item -LiteralPath $OutputDir -Recurse -Force
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

function Invoke-CrashClientCase {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$SaveDirectory,
        [Parameter(Mandatory = $true)]
        [string]$CrashDirectory,
        [bool]$ValidateOnly,
        [string]$ControlledCrash,
        [bool]$ExpectSuccess
    )

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $ExePath
    $startInfo.WorkingDirectory = Split-Path -Parent $ExePath
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $environmentOverrides = [ordered]@{
        HELLOMINE3D_ROOT = $RepoRoot
        HELLOMINE3D_SAVE_DIR = $SaveDirectory
        HELLOMINE3D_CRASH_DIR = $CrashDirectory
        HELLOMINE3D_CONTROLLED_CRASH = $ControlledCrash
        HELLOMINE3D_WINDOW_HIDDEN = "1"
        HELLOMINE3D_VALIDATE_ONLY = $null
        HELLOMINE3D_EXIT_AFTER_FRAMES = $null
        HELLOMINE3D_SEED = "20260809"
        HELLOMINE3D_PLAYER_POSITION = "3038 66 1922"
        HELLOMINE3D_PLAYER_ROTATION = "0 0 0"
        HELLOMINE3D_STARTUP_ERROR_NO_DIALOG = "1"
        HELLOMINE3D_RESOURCE_PACKS = $null
    }
    if ($ValidateOnly) {
        $environmentOverrides["HELLOMINE3D_VALIDATE_ONLY"] = "1"
    }
    else {
        $environmentOverrides["HELLOMINE3D_EXIT_AFTER_FRAMES"] = "3"
    }

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    $previousEnvironment = @{}
    try {
        foreach ($key in $environmentOverrides.Keys) {
            $previousEnvironment[$key] = `
                [Environment]::GetEnvironmentVariable($key, "Process")
            [Environment]::SetEnvironmentVariable(
                $key, $environmentOverrides[$key], "Process")
        }
        if (-not $process.Start()) {
            throw "Unable to start crash diagnostics case $Name."
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
    if (-not $process.WaitForExit(90000)) {
        $process.Kill()
        $process.WaitForExit()
        throw "Crash diagnostics case $Name timed out."
    }
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

    if ($ExpectSuccess -and $exitCode -ne 0) {
        throw "Crash diagnostics case $Name failed: exit=$exitCode stderr=$stderr"
    }
    if (-not $ExpectSuccess -and $exitCode -eq 0) {
        throw "Controlled crash case $Name unexpectedly returned success."
    }
    if (-not $stdout.Contains(
            "[CRASH_DIAGNOSTICS] backend=windows-dbghelp supported=1 installed=1 upload=0")) {
        throw "Crash diagnostics case $Name did not install the selected backend."
    }

    return [pscustomobject]@{
        ExitCode = $exitCode
        Stdout = $stdout
        Stderr = $stderr
    }
}

function Get-Dumps {
    param([string]$Directory)
    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) {
        return @()
    }
    return @(Get-ChildItem -LiteralPath $Directory -Filter "*.dmp" -File)
}

function Get-Sidecars {
    param([string]$Directory)
    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) {
        return @()
    }
    return @(Get-ChildItem -LiteralPath $Directory `
        -Filter "*.crash.txt" -File)
}

function Invoke-OfflineSymbolizer {
    param(
        [string]$Name,
        [string]$Sidecar,
        [string]$Pdb
    )
    $symbolizer = Join-Path $BinRoot `
        "HelloMine3DCrashDiagnosticsSmoke.exe"
    $image = Join-Path $BinRoot "HelloMine3D.exe"
    foreach ($path in @($symbolizer, $image, $Pdb, $Sidecar)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Offline symbolization input is missing: $path"
        }
    }

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $symbolizer
    $startInfo.WorkingDirectory = $BinRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.WindowStyle = `
        [System.Diagnostics.ProcessWindowStyle]::Hidden
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $quote = {
        param([string]$Value)
        return '"' + $Value.Replace('"', '\"') + '"'
    }
    $startInfo.Arguments = @(
        "--symbolize",
        "--sidecar", (& $quote $Sidecar),
        "--image", (& $quote $image),
        "--pdb", (& $quote $Pdb)
    ) -join " "

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw "Unable to start offline symbolizer case $Name."
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(30000)) {
        $process.Kill()
        $process.WaitForExit()
        throw "Offline symbolizer case $Name timed out."
    }
    $result = [pscustomobject]@{
        ExitCode = $process.ExitCode
        Stdout = $stdoutTask.Result
        Stderr = $stderrTask.Result
    }
    $process.Dispose()
    [System.IO.File]::WriteAllText(
        (Join-Path $OutputDir "$Name.stdout.log"), $result.Stdout,
        (New-Object System.Text.UTF8Encoding($false)))
    [System.IO.File]::WriteAllText(
        (Join-Path $OutputDir "$Name.stderr.log"), $result.Stderr,
        (New-Object System.Text.UTF8Encoding($false)))
    return $result
}

$OrdinaryCrash = Join-Path $OutputDir "ordinary-crashes"
$null = Invoke-CrashClientCase `
    -Name "ordinary-validation" `
    -SaveDirectory (Join-Path $OutputDir "ordinary-validation-save") `
    -CrashDirectory $OrdinaryCrash `
    -ValidateOnly $true `
    -ControlledCrash "" `
    -ExpectSuccess $true
if (@(Get-Dumps $OrdinaryCrash).Count -ne 0) {
    throw "Validation-only startup unexpectedly produced a dump."
}
if (@(Get-Sidecars $OrdinaryCrash).Count -ne 0) {
    throw "Validation-only startup unexpectedly produced a sidecar."
}

$null = Invoke-CrashClientCase `
    -Name "ordinary-window" `
    -SaveDirectory (Join-Path $OutputDir "ordinary-window-save") `
    -CrashDirectory $OrdinaryCrash `
    -ValidateOnly $false `
    -ControlledCrash "" `
    -ExpectSuccess $true
if (@(Get-Dumps $OrdinaryCrash).Count -ne 0) {
    throw "Ordinary real-window startup unexpectedly produced a dump."
}
if (@(Get-Sidecars $OrdinaryCrash).Count -ne 0) {
    throw "Ordinary real-window startup unexpectedly produced a sidecar."
}

$ControlledSave = Join-Path $OutputDir "controlled-save"
$ControlledCrash = Join-Path $OutputDir "controlled-crashes"
$controlledResult = Invoke-CrashClientCase `
    -Name "controlled-after-first-frame" `
    -SaveDirectory $ControlledSave `
    -CrashDirectory $ControlledCrash `
    -ValidateOnly $false `
    -ControlledCrash "after-first-frame" `
    -ExpectSuccess $false
if (-not $controlledResult.Stdout.Contains(
        "controlled_crash=after-first-frame active_world_saved=1")) {
    throw "Controlled crash did not publish the active world first."
}
$dumps = @(Get-Dumps $ControlledCrash)
if ($dumps.Count -ne 1 -or $dumps[0].Length -le 0) {
    throw "Controlled crash must produce exactly one non-empty dump; found $($dumps.Count)."
}
$sidecars = @(Get-Sidecars $ControlledCrash)
if ($sidecars.Count -ne 1 -or $sidecars[0].Length -le 0) {
    throw "Controlled crash must produce exactly one non-empty sidecar; found $($sidecars.Count)."
}
$sidecarText = Get-Content -LiteralPath $sidecars[0].FullName -Raw
foreach ($secret in @($RepoRoot, $ControlledSave, $ControlledCrash,
                       $env:USERPROFILE)) {
    if (-not [string]::IsNullOrWhiteSpace($secret) -and
        $sidecarText.Contains($secret)) {
        throw "Crash sidecar leaks a local absolute path."
    }
}
if (-not $sidecarText.Contains("schema 1") -or
    -not $sidecarText.Contains("upload_enabled 0") -or
    -not $sidecarText.Contains("build_identity pdb-")) {
    throw "Crash sidecar is missing its version, build identity or upload policy."
}

$matchingSymbols = Invoke-OfflineSymbolizer `
    -Name "matching-symbols" `
    -Sidecar $sidecars[0].FullName `
    -Pdb (Join-Path $BinRoot "HelloMine3D.pdb")
if ($matchingSymbols.ExitCode -ne 0 -or
    -not $matchingSymbols.Stdout.Contains(
        "[CRASH_SYMBOLIZER] status=PASS") -or
    -not $matchingSymbols.Stdout.Contains("triggerControlledCrash")) {
    throw "Matching symbols did not resolve a controlled project frame: $($matchingSymbols.Stderr)"
}

$wrongSymbols = Invoke-OfflineSymbolizer `
    -Name "wrong-symbols" `
    -Sidecar $sidecars[0].FullName `
    -Pdb (Join-Path $BinRoot "HelloMine3DWorldRuntimeSmoke.pdb")
if ($wrongSymbols.ExitCode -eq 0 -or
    -not $wrongSymbols.Stderr.Contains("symbol-identity-mismatch")) {
    throw "Wrong symbols were not rejected with an explicit mismatch."
}
$worldMetadata = Join-Path $ControlledSave "world.meta"
if (-not (Test-Path -LiteralPath $worldMetadata -PathType Leaf) -or
    (Get-Item -LiteralPath $worldMetadata).Length -le 0) {
    throw "Controlled crash did not leave non-empty active-world metadata."
}

$null = Invoke-CrashClientCase `
    -Name "post-crash-save-validation" `
    -SaveDirectory $ControlledSave `
    -CrashDirectory $ControlledCrash `
    -ValidateOnly $true `
    -ControlledCrash "" `
    -ExpectSuccess $true
$dumpsAfterValidation = @(Get-Dumps $ControlledCrash)
if ($dumpsAfterValidation.Count -ne 1) {
    throw "Post-crash validation unexpectedly changed the dump count."
}
if (@(Get-Sidecars $ControlledCrash).Count -ne 1) {
    throw "Post-crash validation unexpectedly changed the sidecar count."
}
$pending = @(
    Get-ChildItem -LiteralPath $ControlledSave -Recurse -Force |
        Where-Object {
            $_.Name -eq ".pending" -or
            $_.Name -eq ".restore.pending" -or
            $_.Name -eq "recovery.pending" -or
            $_.Name.EndsWith(".pending")
        }
)
if ($pending.Count -ne 0) {
    throw "Controlled crash left unpublished save candidates: $($pending.FullName -join ', ')"
}

$summary = @(
    "status=PASS",
    "configuration=Release",
    "backend=windows-dbghelp",
    "upload=disabled",
    "ordinary_validation_dump_count=0",
    "ordinary_window_dump_count=0",
    "controlled_exit_code=$($controlledResult.ExitCode)",
    "controlled_dump_count=1",
    "controlled_dump_bytes=$($dumps[0].Length)",
    "controlled_sidecar_count=1",
    "controlled_sidecar_bytes=$($sidecars[0].Length)",
    "matching_symbolization=PASS",
    "wrong_symbol_rejection=PASS",
    "post_crash_world_validation=PASS",
    "pending_save_candidates=0"
)
[System.IO.File]::WriteAllText(
    (Join-Path $OutputDir "crash-diagnostics-summary.txt"),
    (($summary -join "`n") + "`n"),
    (New-Object System.Text.UTF8Encoding($false)))

Write-Host "[CRASH_DIAGNOSTICS_VERIFY] status=PASS dump_bytes=$($dumps[0].Length) exit=$($controlledResult.ExitCode)"
