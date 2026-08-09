param(
    [string]$OutputDir = "",
    [string[]]$CaptureMs = @("2000"),
    [int]$Seconds = 12,
    [int]$StartupTimeoutMs = 15000,
    [int]$SettleMs = 500,
    [int]$WindowX = 40,
    [int]$WindowY = 40,
    [int]$WindowWidth = 1600,
    [int]$WindowHeight = 900,
    [string]$Prefix = "new",
    [ValidateSet("Sfml", "Ogre")]
    [string]$Backend = "Sfml",
    [ValidateSet("RuntimeReadback", "WindowScreenshot")]
    [string]$CaptureMode = "RuntimeReadback",
    [string]$SaveDir = "",
    [string]$Seed = "",
    [string]$PlayerPosition = "",
    [string]$PlayerRotation = "",
    [switch]$ShowDebugInfo,
    [switch]$StopExisting,
    [switch]$KeepAlive
)

$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptRoot "..")).Path
$BinDir = Join-Path $RepoRoot "bin"
$ExeName = if ($Backend -eq "Ogre") { "HelloMine3DOgreBootstrap.exe" } else { "HelloMine3D.exe" }
$ExePath = Join-Path $BinDir $ExeName
$RunId = "{0:yyyyMMddHHmmssfff}-{1}" -f (Get-Date), $PID

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $BinDir "render_capture_$RunId"
}
if ([string]::IsNullOrWhiteSpace($SaveDir)) {
    $SaveDir = Join-Path $OutputDir "save"
}

if (-not ("HelloMine3DRenderCapture.NativeMethods" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace HelloMine3DRenderCapture
{
    public struct Rect
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    public struct Point
    {
        public int X;
        public int Y;
    }

    public static class NativeMethods
    {
        public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        [DllImport("user32.dll")]
        public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr extraData);

        [DllImport("user32.dll")]
        public static extern bool GetWindowRect(IntPtr hWnd, out Rect rect);

        [DllImport("user32.dll")]
        public static extern bool GetClientRect(IntPtr hWnd, out Rect rect);

        [DllImport("user32.dll")]
        public static extern bool ClientToScreen(IntPtr hWnd, ref Point point);

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

function Convert-CaptureTimes {
    param(
        [string[]]$Values
    )

    $result = @()
    foreach ($value in $Values) {
        if ([string]::IsNullOrWhiteSpace($value)) {
            continue
        }

        foreach ($token in ([string]$value -split "[,\s]+")) {
            if ([string]::IsNullOrWhiteSpace($token)) {
                continue
            }

            $parsed = 0
            if (-not [int]::TryParse($token, [ref]$parsed) -or $parsed -lt 0) {
                throw "Invalid capture time: $token"
            }
            $result += $parsed
        }
    }

    return @($result | Sort-Object -Unique)
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

        $handle = [HelloMine3DRenderCapture.NativeMethods]::FindWindowForProcess($Process.Id)
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

    [void][HelloMine3DRenderCapture.NativeMethods]::ShowWindowAsync($WindowHandle, $SW_SHOWNOACTIVATE)
    [void][HelloMine3DRenderCapture.NativeMethods]::SetWindowPos($WindowHandle, [IntPtr]::Zero, $X, $Y, $Width, $Height, $flags)
}

function Save-WindowScreenshot {
    param(
        [IntPtr]$WindowHandle,
        [string]$Path
    )

    Add-Type -AssemblyName System.Drawing

    $rect = New-Object HelloMine3DRenderCapture.Rect
    $topLeft = New-Object HelloMine3DRenderCapture.Point
    if ([HelloMine3DRenderCapture.NativeMethods]::GetClientRect($WindowHandle, [ref]$rect) -and
        [HelloMine3DRenderCapture.NativeMethods]::ClientToScreen($WindowHandle, [ref]$topLeft)) {
        $width = $rect.Right - $rect.Left
        $height = $rect.Bottom - $rect.Top
        $rect.Left = $topLeft.X
        $rect.Top = $topLeft.Y
        $rect.Right = $topLeft.X + $width
        $rect.Bottom = $topLeft.Y + $height
    }
    elseif (-not [HelloMine3DRenderCapture.NativeMethods]::GetWindowRect($WindowHandle, [ref]$rect)) {
        throw "GetClientRect/GetWindowRect failed."
    }

    $captureWidth = $rect.Right - $rect.Left
    $captureHeight = $rect.Bottom - $rect.Top
    if ($captureWidth -le 0 -or $captureHeight -le 0) {
        throw "Invalid window rect: $($rect.Left),$($rect.Top),$($rect.Right),$($rect.Bottom)"
    }

    $bitmap = New-Object System.Drawing.Bitmap $captureWidth, $captureHeight
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "$ExeName not found: $ExePath"
}

$sortedCaptures = @(Convert-CaptureTimes -Values $CaptureMs)
if ($sortedCaptures.Count -eq 0) {
    throw "CaptureMs is empty."
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

$screenshotPaths = @()
foreach ($captureTime in $sortedCaptures) {
    $screenshotPaths += Join-Path $OutputDir ("{0}_{1:D5}ms.png" -f $Prefix, [int]$captureTime)
}

$ProcessStdoutPath = Join-Path $OutputDir "process.stdout.log"
$ProcessStderrPath = Join-Path $OutputDir "process.stderr.log"

foreach ($filePath in $screenshotPaths) {
    Remove-Item -LiteralPath $filePath -Force -ErrorAction SilentlyContinue
}
Remove-Item -LiteralPath $ProcessStdoutPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $ProcessStderrPath -Force -ErrorAction SilentlyContinue

Write-Host "[RENDER_CAPTURE] runId=$RunId"
Write-Host "[RENDER_CAPTURE] exe=$ExePath"
Write-Host "[RENDER_CAPTURE] outputDir=$OutputDir"
Write-Host "[RENDER_CAPTURE] capturesMs=$($sortedCaptures -join ',') seconds=$Seconds prefix=$Prefix mode=$CaptureMode noActivate=true"
Write-Host "[RENDER_CAPTURE] saveDir=$SaveDir"
Write-Host "[RENDER_CAPTURE] window=$WindowX,$WindowY ${WindowWidth}x$WindowHeight noActivate=true"
if (-not [string]::IsNullOrWhiteSpace($Seed)) { Write-Host "[RENDER_CAPTURE] seed=$Seed" }
if (-not [string]::IsNullOrWhiteSpace($PlayerPosition)) { Write-Host "[RENDER_CAPTURE] playerPosition=$PlayerPosition" }
if (-not [string]::IsNullOrWhiteSpace($PlayerRotation)) { Write-Host "[RENDER_CAPTURE] playerRotation=$PlayerRotation" }
if ($ShowDebugInfo) { Write-Host "[RENDER_CAPTURE] showDebugInfo=true" }

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
if ($ShowDebugInfo) {
    $envValues["HELLOMINE3D_SHOW_DEBUG_INFO"] = "1"
}
if ($CaptureMode -eq "RuntimeReadback") {
    $envValues["HELLO_RENDER_CAPTURE"] = "1"
    $envValues["HELLO_RENDER_CAPTURE_DIR"] = $OutputDir
    $envValues["HELLO_RENDER_CAPTURE_PREFIX"] = $Prefix
    $envValues["HELLO_RENDER_CAPTURE_MS"] = ($sortedCaptures -join ",")
    $envValues["HELLO_RENDER_CAPTURE_MAX_DELTA_MS"] = "5000"
    $envValues["HELLO_RENDER_CAPTURE_EXIT"] = "1"
}

$windowStyle = "Minimized"

$process = $null
Set-ProcessEnvironment -Values $envValues -Body {
    $script:CapturedProcess = Start-Process -FilePath $ExePath -WorkingDirectory $BinDir -WindowStyle $windowStyle -RedirectStandardOutput $ProcessStdoutPath -RedirectStandardError $ProcessStderrPath -PassThru
}
$process = $script:CapturedProcess
$script:CapturedProcess = $null
Write-Host "[RENDER_CAPTURE] started pid=$($process.Id)"

try {
    if ($CaptureMode -eq "WindowScreenshot") {
        $handle = Wait-MainWindowHandle -Process $process -TimeoutMs $StartupTimeoutMs
        Show-WindowNoActivate -WindowHandle $handle -X $WindowX -Y $WindowY -Width $WindowWidth -Height $WindowHeight
        Start-Sleep -Milliseconds $SettleMs

        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        foreach ($captureTime in $sortedCaptures) {
            $remainingMs = [int]$captureTime - [int]$stopwatch.ElapsedMilliseconds
            if ($remainingMs -gt 0) {
                Start-Sleep -Milliseconds $remainingMs
            }

            $process.Refresh()
            if ($process.HasExited) {
                throw "Process exited before capture ${captureTime}ms: exitCode=$($process.ExitCode)"
            }

            $filePath = Join-Path $OutputDir ("{0}_{1:D5}ms.png" -f $Prefix, [int]$captureTime)
            Save-WindowScreenshot -WindowHandle $handle -Path $filePath
            Write-Host "[RENDER_CAPTURE] captured $filePath"
        }
    }
    else {
        $handle = Wait-MainWindowHandle -Process $process -TimeoutMs $StartupTimeoutMs
        Show-WindowNoActivate -WindowHandle $handle -X $WindowX -Y $WindowY -Width $WindowWidth -Height $WindowHeight
        Start-Sleep -Milliseconds $SettleMs

        $lastCaptureMs = [int]($sortedCaptures[$sortedCaptures.Count - 1])
        $deadline = (Get-Date).AddMilliseconds($StartupTimeoutMs + $lastCaptureMs + 5000)
        do {
            $process.Refresh()
            if ($process.HasExited) {
                throw "Process exited before runtime captures completed: exitCode=$($process.ExitCode)"
            }

            $pending = @()
            foreach ($filePath in $screenshotPaths) {
                if (-not (Test-Path -LiteralPath $filePath)) {
                    $pending += $filePath
                }
                elseif ((Get-Item -LiteralPath $filePath).Length -le 0) {
                    $pending += "$filePath (empty)"
                }
            }

            if ($pending.Count -eq 0) {
                break
            }

            Start-Sleep -Milliseconds 100
        } while ((Get-Date) -lt $deadline)

        if ($pending.Count -gt 0) {
            throw "Timed out waiting for runtime captures: $($pending -join '; ')"
        }

        foreach ($filePath in $screenshotPaths) {
            Write-Host "[RENDER_CAPTURE] captured $filePath"
        }
    }
}
finally {
    if ($KeepAlive) {
        Write-Host "[RENDER_CAPTURE] keepAlive pid=$($process.Id)"
    }
    elseif ($process -ne $null -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        try { Wait-Process -Id $process.Id -Timeout 5 -ErrorAction SilentlyContinue } catch {}
        Write-Host "[RENDER_CAPTURE] stopped pid=$($process.Id)"
    }
}

$missing = @()
foreach ($filePath in $screenshotPaths) {
    if (-not (Test-Path -LiteralPath $filePath)) {
        $missing += $filePath
    }
    elseif ((Get-Item -LiteralPath $filePath).Length -le 0) {
        $missing += "$filePath (empty)"
    }
}

if ($missing.Count -gt 0) {
    throw "Render capture missing files: $($missing -join '; ')"
}

Write-Host "[RENDER_CAPTURE] status=PASS outputDir=$OutputDir"
