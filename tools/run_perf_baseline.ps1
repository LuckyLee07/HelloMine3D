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
    [double]$MinimumSimulationTickHz = 19.0,
    [double]$MaximumSimulationTickHz = 21.0,
    [switch]$StopExisting,
    [switch]$KeepAlive
)

$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptRoot "..")).Path
$BinDir = Join-Path $RepoRoot "bin"
$ExePath = Join-Path $BinDir "HelloMine3D.exe"
$RunId = "{0:yyyyMMddHHmmssfff}-{1}" -f (Get-Date), $PID

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $BinDir "perf_baseline_$RunId"
}
if ([string]::IsNullOrWhiteSpace($SaveDir)) {
    $SaveDir = Join-Path $OutputDir "save"
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
    throw "HelloMine3D.exe not found: $ExePath"
}

if ($WarmupMs -lt 0) {
    throw "WarmupMs must be >= 0."
}
if ($DurationMs -le 0) {
    throw "DurationMs must be > 0."
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path $SaveDir | Out-Null

$DefaultMetaPath = Join-Path $BinDir "saves\default\world.meta"
if ([string]::IsNullOrWhiteSpace($Seed)) {
    $Seed = Read-WorldMetaValue -Path $DefaultMetaPath -Key "seed"
}
if ([string]::IsNullOrWhiteSpace($PlayerPosition)) {
    $PlayerPosition = Read-WorldMetaValue -Path $DefaultMetaPath -Key "player_position"
}
if ([string]::IsNullOrWhiteSpace($PlayerRotation)) {
    $PlayerRotation = Read-WorldMetaValue -Path $DefaultMetaPath -Key "player_rotation"
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
Write-Host "[PERF_BASELINE] outputDir=$OutputDir"
Write-Host "[PERF_BASELINE] warmupMs=$WarmupMs durationMs=$DurationMs noActivate=true"
Write-Host "[PERF_BASELINE] saveDir=$SaveDir"
Write-Host "[PERF_BASELINE] window=$WindowX,$WindowY ${WindowWidth}x$WindowHeight noActivate=true"
if (-not [string]::IsNullOrWhiteSpace($Seed)) { Write-Host "[PERF_BASELINE] seed=$Seed" }
if (-not [string]::IsNullOrWhiteSpace($PlayerPosition)) { Write-Host "[PERF_BASELINE] playerPosition=$PlayerPosition" }
if (-not [string]::IsNullOrWhiteSpace($PlayerRotation)) { Write-Host "[PERF_BASELINE] playerRotation=$PlayerRotation" }

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
    HELLOMINE3D_ROOT = $RepoRoot
    HELLOMINE3D_SAVE_DIR = $SaveDir
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

$process = $null
Set-ProcessEnvironment -Values $envValues -Body {
    $script:CapturedProcess = Start-Process -FilePath $ExePath -WorkingDirectory $BinDir -WindowStyle Minimized -RedirectStandardOutput $ProcessStdoutPath -RedirectStandardError $ProcessStderrPath -PassThru
}
$process = $script:CapturedProcess
$script:CapturedProcess = $null
Write-Host "[PERF_BASELINE] started pid=$($process.Id)"

try {
    $handle = Wait-MainWindowHandle -Process $process -TimeoutMs $StartupTimeoutMs
    Show-WindowNoActivate -WindowHandle $handle -X $WindowX -Y $WindowY -Width $WindowWidth -Height $WindowHeight
    Start-Sleep -Milliseconds $SettleMs

    $deadline = (Get-Date).AddMilliseconds($StartupTimeoutMs + $WarmupMs + $DurationMs + 10000)
    do {
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
}
finally {
    if ($KeepAlive) {
        Write-Host "[PERF_BASELINE] keepAlive pid=$($process.Id)"
    }
    elseif ($process -ne $null -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        try { Wait-Process -Id $process.Id -Timeout 5 -ErrorAction SilentlyContinue } catch {}
        Write-Host "[PERF_BASELINE] stopped pid=$($process.Id)"
    }
}

Write-Host "[PERF_BASELINE] summary=$SummaryPath"
Write-Host "[PERF_BASELINE] frames=$FramesPath"
Get-Content -LiteralPath $SummaryPath | ForEach-Object { Write-Host "[PERF_BASELINE] $_" }
Write-Host "[PERF_BASELINE] status=PASS outputDir=$OutputDir"
