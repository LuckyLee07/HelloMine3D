param(
    [string]$OutputDir = "",
    [int]$WarmupMs = 3000,
    [int]$DurationMs = 10000,
    [int]$StartupTimeoutMs = 15000,
    [int]$SettleMs = 500,
    [int]$WindowX = 40,
    [int]$WindowY = 40,
    [int]$WindowWidth = 1600,
    [int]$WindowHeight = 900,
    [string]$SaveDir = "",
    [string]$Seed = "",
    [string]$PlayerPosition = "",
    [string]$PlayerRotation = "",
    [string]$WorldTime = "",
    [string]$ResourcePacks = "",
    [string]$SceneId = "",
    [string]$BudgetProfile = "unapproved-local-capture",
    [string]$CacheRegime = "warm-process-clean",
    [ValidateSet("", "fast-streaming", "scaled-gameplay")]
    [string]$RcPerformanceProfile = "",
    [string]$StorageClass = "local-default",
    [string]$RuntimeRoot = "",
    [double]$MinimumSimulationTickHz = 19.0,
    [double]$MaximumSimulationTickHz = 21.0,
    [switch]$VerticalSliceFixture,
    [switch]$HiddenWindow = $true,
    [switch]$StopExisting,
    [switch]$QuietSummary,
    [switch]$KeepAlive
)

$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptRoot "..")).Path
$BinDir = Join-Path $RepoRoot "bin"
$ExeName = "HelloMine3D.exe"
$ExePath = Join-Path $BinDir $ExeName
$RuntimeRootPath = if ([string]::IsNullOrWhiteSpace($RuntimeRoot)) {
    $RepoRoot
}
else { (Resolve-Path -LiteralPath $RuntimeRoot).Path }
$RuntimeBinDir = Join-Path $RuntimeRootPath "bin"
$RunId = "{0:yyyyMMddHHmmssfff}-{1}" -f (Get-Date), $PID

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $BinDir "perf_baseline_$RunId"
}
if ([string]::IsNullOrWhiteSpace($SaveDir)) {
    $SaveDir = Join-Path $OutputDir "save"
}
if (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = [System.IO.Path]::GetFullPath(
        (Join-Path $RepoRoot $OutputDir))
}
if (-not [System.IO.Path]::IsPathRooted($SaveDir)) {
    $SaveDir = [System.IO.Path]::GetFullPath(
        (Join-Path $RepoRoot $SaveDir))
}

if (-not ("HelloMine3DPerfBaseline.NativeMethods" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace HelloMine3DPerfBaseline
{
    public struct Rect
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    public static class NativeMethods
    {
        public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        [DllImport("user32.dll")]
        public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr extraData);

        [DllImport("user32.dll")]
        public static extern bool GetWindowRect(IntPtr hWnd, out Rect rect);

        [DllImport("user32.dll")]
        public static extern int GetClassName(IntPtr hWnd, StringBuilder className, int maxCount);

        [DllImport("user32.dll")]
        public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out int processId);

        [DllImport("user32.dll")]
        public static extern bool IsWindowVisible(IntPtr hWnd);

        [DllImport("user32.dll")]
        public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);

        [DllImport("user32.dll")]
        public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int x, int y, int cx, int cy, uint flags);

        public static IntPtr FindWindowForProcess(int targetProcessId)
        {
            IntPtr best = IntPtr.Zero;
            int bestArea = 0;
            EnumWindows(delegate(IntPtr hWnd, IntPtr lParam)
            {
                int processId;
                GetWindowThreadProcessId(hWnd, out processId);
                if (processId == targetProcessId && IsWindowVisible(hWnd))
                {
                    StringBuilder className = new StringBuilder(256);
                    GetClassName(hWnd, className, className.Capacity);
                    string cls = className.ToString();
                    if (cls == "ConsoleWindowClass" || cls.StartsWith("BAIDU_CLASS_IME"))
                        return true;

                    Rect rect;
                    if (!GetWindowRect(hWnd, out rect))
                        return true;
                    int width = rect.Right - rect.Left;
                    int height = rect.Bottom - rect.Top;
                    int area = width > 0 && height > 0 ? width * height : 0;
                    if (area > bestArea)
                    {
                        best = hWnd;
                        bestArea = area;
                    }
                }
                return true;
            }, IntPtr.Zero);
            return best;
        }
    }
}
"@
}

function Set-ProcessEnvironment {
    param(
        [hashtable]$Values,
        [scriptblock]$Body
    )

    $oldValues = @{}
    foreach ($key in $Values.Keys) {
        $oldValues[$key] = [Environment]::GetEnvironmentVariable($key, "Process")
        Set-Item -Path "Env:$key" -Value ([string]$Values[$key])
    }

    try {
        & $Body
    }
    finally {
        foreach ($key in $Values.Keys) {
            if ($null -eq $oldValues[$key]) {
                Remove-Item -Path "Env:$key" -ErrorAction SilentlyContinue
            }
            else {
                Set-Item -Path "Env:$key" -Value $oldValues[$key]
            }
        }
    }
}

function Read-WorldMetaValue {
    param(
        [string]$Path,
        [string]$Key
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return ""
    }

    $escapedKey = [regex]::Escape($Key)
    $line = Get-Content -LiteralPath $Path | Where-Object { $_ -match "^$escapedKey\s+" } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($line)) {
        return ""
    }

    return (($line -replace "^$escapedKey\s+", "") -replace "\s+", " ").Trim()
}

function Read-SummaryValue {
    param(
        [string]$Path,
        [string]$Key
    )

    $escapedKey = [regex]::Escape($Key)
    $line = Get-Content -LiteralPath $Path | Where-Object { $_ -match "^$escapedKey=" } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($line)) {
        throw "Performance summary is missing '$Key': $Path"
    }

    return ($line -replace "^$escapedKey=", "").Trim()
}

function Read-ConfigValue {
    param(
        [string]$Path,
        [string]$Key
    )

    $escapedKey = [regex]::Escape($Key)
    $line = Get-Content -LiteralPath $Path | Where-Object {
        $_ -match "^\s*$escapedKey\s*="
    } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($line)) {
        throw "Runtime config is missing '$Key': $Path"
    }
    return (($line -split '=', 2)[1]).Trim()
}

function Read-GameConfigValue {
    param(
        [string]$Path,
        [string]$Key
    )

    $escapedKey = [regex]::Escape($Key)
    $line = Get-Content -LiteralPath $Path | Where-Object {
        $_ -match "^\s*$escapedKey\s+"
    } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($line)) {
        throw "Game config is missing '$Key': $Path"
    }
    return (($line -replace "^\s*$escapedKey\s+", "") -replace "\s+", " ").Trim()
}

function Get-BuildIdentity {
    param([string]$Root)

    $headCommit = (& git -C $Root rev-parse HEAD 2>$null | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($headCommit)) {
        throw "Unable to resolve Git build identity."
    }
    $diffText = (& git -C $Root -c core.safecrlf=false diff -- src premake `
        2>$null | Out-String)
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to resolve Git source state."
    }
    if ([string]::IsNullOrWhiteSpace($diffText)) {
        return $headCommit
    }

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($diffText)
        $hash = $sha256.ComputeHash($bytes)
        $suffix = ([BitConverter]::ToString($hash) -replace '-', '').ToLowerInvariant()
        return "$headCommit+dirty-$($suffix.Substring(0, 12))"
    }
    finally {
        $sha256.Dispose()
    }
}

function Wait-MainWindowHandle {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$TimeoutMs
    )

    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    do {
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "Process exited before creating a window: pid=$($Process.Id) exitCode=$($Process.ExitCode)"
        }

        $handle = [HelloMine3DPerfBaseline.NativeMethods]::FindWindowForProcess($Process.Id)
        if ($handle -ne [IntPtr]::Zero) {
            return $handle
        }

        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)

    throw "Timed out waiting for main window: pid=$($Process.Id)"
}

function Show-WindowNoActivate {
    param(
        [IntPtr]$WindowHandle,
        [int]$X,
        [int]$Y,
        [int]$Width,
        [int]$Height
    )

    if ($WindowHandle -eq [IntPtr]::Zero) {
        return
    }

    $SW_SHOWNOACTIVATE = 4
    $SWP_NOZORDER = 0x0004
    $SWP_NOACTIVATE = 0x0010
    $SWP_SHOWWINDOW = 0x0040
    $SWP_NOOWNERZORDER = 0x0200
    $flags = [uint32]($SWP_NOZORDER -bor $SWP_NOACTIVATE -bor $SWP_SHOWWINDOW -bor $SWP_NOOWNERZORDER)

    [void][HelloMine3DPerfBaseline.NativeMethods]::ShowWindowAsync($WindowHandle, $SW_SHOWNOACTIVATE)
    [void][HelloMine3DPerfBaseline.NativeMethods]::SetWindowPos($WindowHandle, [IntPtr]::Zero, $X, $Y, $Width, $Height, $flags)
}

if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "$ExeName not found: $ExePath"
}

if ($WarmupMs -lt 0) {
    throw "WarmupMs must be >= 0."
}
if ($DurationMs -le 0) {
    throw "DurationMs must be > 0."
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path $SaveDir | Out-Null

$DefaultMetaPath = Join-Path $RuntimeBinDir "saves\default\world.meta"
if ([string]::IsNullOrWhiteSpace($Seed)) {
    $Seed = Read-WorldMetaValue -Path $DefaultMetaPath -Key "seed"
}
if ([string]::IsNullOrWhiteSpace($PlayerPosition)) {
    $PlayerPosition = Read-WorldMetaValue -Path $DefaultMetaPath -Key "player_position"
}
if ([string]::IsNullOrWhiteSpace($PlayerRotation)) {
    $PlayerRotation = Read-WorldMetaValue -Path $DefaultMetaPath -Key "player_rotation"
}
if ([string]::IsNullOrWhiteSpace($SceneId)) {
    if ([string]::IsNullOrWhiteSpace($Seed) -or
        [string]::IsNullOrWhiteSpace($PlayerPosition) -or
        [string]::IsNullOrWhiteSpace($PlayerRotation)) {
        throw "SceneId requires an explicit value when seed, player position or player rotation cannot be resolved."
    }
    $SceneId = "seed=$Seed;position=$PlayerPosition;rotation=$PlayerRotation;worldTime=$WorldTime"
}

$OgreConfigPath = Join-Path $RuntimeBinDir "Mine.cfg"
$VsyncValue = Read-ConfigValue -Path $OgreConfigPath -Key "VSync"
$VsyncRegime = if ($HiddenWindow) {
    "hidden-offscreen"
} elseif ($VsyncValue -match '^(?i:yes|true|on|1)$') {
    "on"
} else {
    "off"
}

$ProcessStdoutPath = Join-Path $OutputDir "process.stdout.log"
$ProcessStderrPath = Join-Path $OutputDir "process.stderr.log"
$SummaryPath = Join-Path $OutputDir "summary.txt"
$FramesPath = Join-Path $OutputDir "frames.csv"

Remove-Item -LiteralPath $ProcessStdoutPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $ProcessStderrPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $SummaryPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $FramesPath -Force -ErrorAction SilentlyContinue

Write-Host "[PERF_BASELINE] runId=$RunId"
Write-Host "[PERF_BASELINE] exe=$ExePath"
Write-Host "[PERF_BASELINE] runtimeRoot=$RuntimeRootPath"
Write-Host "[PERF_BASELINE] outputDir=$OutputDir"
Write-Host "[PERF_BASELINE] warmupMs=$WarmupMs durationMs=$DurationMs noActivate=true hidden=$($HiddenWindow.IsPresent.ToString().ToLowerInvariant())"
Write-Host "[PERF_BASELINE] saveDir=$SaveDir"
Write-Host "[PERF_BASELINE] window=$WindowX,$WindowY ${WindowWidth}x$WindowHeight noActivate=true"
if (-not [string]::IsNullOrWhiteSpace($Seed)) { Write-Host "[PERF_BASELINE] seed=$Seed" }
if (-not [string]::IsNullOrWhiteSpace($PlayerPosition)) { Write-Host "[PERF_BASELINE] playerPosition=$PlayerPosition" }
if (-not [string]::IsNullOrWhiteSpace($PlayerRotation)) { Write-Host "[PERF_BASELINE] playerRotation=$PlayerRotation" }
if (-not [string]::IsNullOrWhiteSpace($WorldTime)) { Write-Host "[PERF_BASELINE] worldTime=$WorldTime" }
Write-Host "[PERF_BASELINE] sceneId=$SceneId vsync=$VsyncRegime"
Write-Host "[PERF_BASELINE] budgetProfile=$BudgetProfile cacheRegime=$CacheRegime rcProfile=$RcPerformanceProfile"
if ($VerticalSliceFixture) { Write-Host "[PERF_BASELINE] verticalSliceFixture=true" }
if (-not [string]::IsNullOrWhiteSpace($ResourcePacks)) { Write-Host "[PERF_BASELINE] resourcePacks=$ResourcePacks" }

if ($StopExisting) {
    $existingProcesses = @(Get-Process -Name "HelloMine3D" -ErrorAction SilentlyContinue)
    if ($existingProcesses.Count -gt 0) {
        $existingProcesses | Stop-Process -Force
        foreach ($existing in $existingProcesses) {
            try { Wait-Process -Id $existing.Id -Timeout 5 -ErrorAction SilentlyContinue } catch {}
        }
    }
    Start-Sleep -Milliseconds 500
}

$envValues = @{
    HELLOMINE3D_ROOT = $RuntimeRootPath
    HELLOMINE3D_SAVE_DIR = $SaveDir
    HELLOMINE3D_RESOURCE_PACKS = $ResourcePacks
    HELLO_PERF_CAPTURE = "1"
    HELLO_PERF_CAPTURE_DIR = $OutputDir
    HELLO_PERF_CAPTURE_WARMUP_MS = $WarmupMs
    HELLO_PERF_CAPTURE_DURATION_MS = $DurationMs
    HELLO_PERF_CAPTURE_EXIT = "1"
}
if (-not [string]::IsNullOrWhiteSpace($Seed)) {
    $envValues["HELLOMINE3D_SEED"] = $Seed
}
if (-not [string]::IsNullOrWhiteSpace($PlayerPosition)) {
    $envValues["HELLOMINE3D_PLAYER_POSITION"] = $PlayerPosition
}
if (-not [string]::IsNullOrWhiteSpace($PlayerRotation)) {
    $envValues["HELLOMINE3D_PLAYER_ROTATION"] = $PlayerRotation
}
if (-not [string]::IsNullOrWhiteSpace($WorldTime)) {
    $envValues["HELLOMINE3D_WORLD_TIME"] = $WorldTime
}
if ($VerticalSliceFixture) {
    $envValues["HELLOMINE3D_VERTICAL_SLICE_FIXTURE"] = "1"
}
if ($HiddenWindow) {
    $envValues["HELLOMINE3D_WINDOW_HIDDEN"] = "1"
}
if (-not [string]::IsNullOrWhiteSpace($RcPerformanceProfile)) {
    $envValues["HELLOMINE3D_RC_PERF_PROFILE"] = $RcPerformanceProfile
}

$process = $null
Set-ProcessEnvironment -Values $envValues -Body {
    $windowStyle = if ($HiddenWindow) { "Hidden" } else { "Minimized" }
    $script:CapturedProcess = Start-Process -FilePath $ExePath -WorkingDirectory $RuntimeBinDir -WindowStyle $windowStyle -RedirectStandardOutput $ProcessStdoutPath -RedirectStandardError $ProcessStderrPath -PassThru
}
$process = $script:CapturedProcess
$script:CapturedProcess = $null
Write-Host "[PERF_BASELINE] started pid=$($process.Id)"

$peakPrivateBytes = [int64]0
$peakWorkingSetBytes = [int64]0
$peakHandleCount = 0

try {
    if (-not $HiddenWindow) {
        $handle = Wait-MainWindowHandle -Process $process -TimeoutMs $StartupTimeoutMs
        Show-WindowNoActivate -WindowHandle $handle -X $WindowX -Y $WindowY -Width $WindowWidth -Height $WindowHeight
    }
    $deadline = (Get-Date).AddMilliseconds($StartupTimeoutMs + $WarmupMs + $DurationMs + 10000)
    do {
        if (-not $process.HasExited) {
            $process.Refresh()
            $peakPrivateBytes = [Math]::Max(
                $peakPrivateBytes, [int64]$process.PrivateMemorySize64)
            $peakWorkingSetBytes = [Math]::Max(
                $peakWorkingSetBytes, [int64]$process.WorkingSet64)
            $peakHandleCount = [Math]::Max(
                $peakHandleCount, [int]$process.HandleCount)
        }
        if (Test-Path -LiteralPath $SummaryPath) {
            break
        }

        $process.Refresh()
        if ($process.HasExited) {
            if (Test-Path -LiteralPath $SummaryPath) {
                break
            }
            throw "Process exited before performance summary completed: exitCode=$($process.ExitCode)"
        }

        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)

    if (-not (Test-Path -LiteralPath $SummaryPath)) {
        throw "Timed out waiting for performance summary: $SummaryPath"
    }
    if (-not (Test-Path -LiteralPath $FramesPath) -or (Get-Item -LiteralPath $FramesPath).Length -le 0) {
        throw "Missing or empty performance frames CSV: $FramesPath"
    }

    $simulationTickHzText = Read-SummaryValue -Path $SummaryPath -Key "simulation_tick_hz"
    $simulationTickHz = 0.0
    if (-not [double]::TryParse(
        $simulationTickHzText,
        [System.Globalization.NumberStyles]::Float,
        [System.Globalization.CultureInfo]::InvariantCulture,
        [ref]$simulationTickHz)) {
        throw "Invalid simulation_tick_hz in performance summary: '$simulationTickHzText'"
    }
    if ($simulationTickHz -lt $MinimumSimulationTickHz -or $simulationTickHz -gt $MaximumSimulationTickHz) {
        throw "Simulation tick rate $simulationTickHz Hz is outside the expected range [$MinimumSimulationTickHz, $MaximumSimulationTickHz]."
    }

    $gameConfigPath = Join-Path $RuntimeBinDir "config.txt"
    $fullscreen = Read-GameConfigValue `
        -Path $gameConfigPath -Key "fullscreen"
    $fov = Read-GameConfigValue -Path $gameConfigPath -Key "fov"
    $renderDistance = Read-GameConfigValue `
        -Path $gameConfigPath -Key "renderdistance"
    $shadowQuality = Read-GameConfigValue `
        -Path $gameConfigPath -Key "directionalshadowquality"
    if ($shadowQuality -notin @("off", "medium", "high")) {
        throw "Invalid directionalshadowquality in runtime config: '$shadowQuality'"
    }
    $postProcessingQuality = Read-GameConfigValue `
        -Path $gameConfigPath -Key "postprocessingquality"
    if ($postProcessingQuality -notin @("off", "on")) {
        throw "Invalid postprocessingquality in runtime config: '$postProcessingQuality'"
    }
    $manifestPath = Join-Path $RepoRoot "media\resource-manifest.txt"
    $manifestHash = (Get-FileHash `
        -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $buildIdentity = Get-BuildIdentity -Root $RepoRoot
    $video = Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue |
        Select-Object -First 1
    $gpu = if ($null -ne $video -and
        -not [string]::IsNullOrWhiteSpace($video.Name)) {
        ([string]$video.Name).Trim()
    }
    else { "unknown" }
    $driver = if ($null -ne $video -and
        -not [string]::IsNullOrWhiteSpace($video.DriverVersion)) {
        ([string]$video.DriverVersion).Trim()
    }
    else { "unknown" }
    $saveFormat = Read-WorldMetaValue `
        -Path (Join-Path $SaveDir "world.meta") -Key "version"
    if ([string]::IsNullOrWhiteSpace($saveFormat)) { $saveFormat = "unknown" }
    $resourcePackIdentity = if ([string]::IsNullOrWhiteSpace($ResourcePacks)) {
        "none"
    }
    else { ($ResourcePacks -replace '\s+', '') }
    $worldFixture = "seed-$Seed-position-$($PlayerPosition -replace '\s+', '_')-time-$WorldTime"

    $scenarioIdentity =
        "profile=$RcPerformanceProfile;seed=$Seed;renderDistance=$renderDistance"
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $scenarioHash = ([BitConverter]::ToString(
            $sha256.ComputeHash(
                [System.Text.Encoding]::UTF8.GetBytes($scenarioIdentity))) `
            -replace '-', '').ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
    }
    $fixedTickCount = [Math]::Floor($DurationMs / 50)

    Add-Content -LiteralPath $SummaryPath -Encoding utf8 -Value @(
        "comparison_schema=3",
        "comparison_contract_version=1",
        "comparison_budget_profile=$BudgetProfile",
        "comparison_scene_id=$SceneId",
        "comparison_platform=windows",
        "comparison_architecture=x86_64",
        "comparison_build_id=$buildIdentity",
        "comparison_gpu=$gpu",
        "comparison_driver=$driver",
        "comparison_vsync_regime=$VsyncRegime",
        "comparison_window=${WindowWidth}x$WindowHeight",
        "comparison_window_visibility=$(if ($HiddenWindow) { 'hidden' } else { 'visible-no-activate' })",
        "comparison_fullscreen=$fullscreen",
        "comparison_fov=$fov",
        "comparison_resource_manifest_sha256=$manifestHash",
        "comparison_resource_packs=$resourcePackIdentity",
        "comparison_world_fixture=$worldFixture",
        "comparison_save_format=$saveFormat",
        "comparison_storage_class=$StorageClass",
        "comparison_render_distance=$renderDistance",
        "comparison_cache_regime=$CacheRegime",
        "comparison_movement_path=rc-ring-12-chunks-v1",
        "comparison_movement_speed=teleport-after-visible-plus-2s",
        "comparison_population_fixture=rc-8-mobs-16-items-64-crops-8-chests-v1",
        "comparison_save_state_sha256=$scenarioHash",
        "comparison_fixed_tick_count=$fixedTickCount",
        "stage10_shadow_quality=$shadowQuality",
        "stage10_post_processing=$postProcessingQuality",
        "peak_private_bytes=$peakPrivateBytes",
        "peak_working_set_bytes=$peakWorkingSetBytes",
        "peak_handle_count=$peakHandleCount"
    )
}
finally {
    if ($KeepAlive) {
        Write-Host "[PERF_BASELINE] keepAlive pid=$($process.Id)"
    }
    elseif ($process -ne $null -and -not $process.HasExited) {
        # HELLO_PERF_CAPTURE_EXIT asks the client to leave immediately after
        # publishing its summary. Give Ogre/GL a bounded chance to release
        # the native context before falling back to a forced stop; otherwise
        # the next baseline in the same matrix can inherit a stale GPU
        # context and fail before the first frame.
        try {
            Wait-Process -Id $process.Id -Timeout 5 -ErrorAction Stop
        }
        catch {
        }
        $process.Refresh()
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
            try {
                Wait-Process -Id $process.Id -Timeout 5 `
                    -ErrorAction SilentlyContinue
            }
            catch {
            }
            Write-Host "[PERF_BASELINE] forcedStop pid=$($process.Id)"
        }
        else {
            Write-Host "[PERF_BASELINE] exited pid=$($process.Id)"
        }
    }
}

Write-Host "[PERF_BASELINE] summary=$SummaryPath"
Write-Host "[PERF_BASELINE] frames=$FramesPath"
if (-not $QuietSummary) {
    Get-Content -LiteralPath $SummaryPath |
        ForEach-Object { Write-Host "[PERF_BASELINE] $_" }
}
Write-Host "[PERF_BASELINE] status=PASS outputDir=$OutputDir"
