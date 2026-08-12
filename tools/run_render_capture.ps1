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
    [ValidateSet("RuntimeReadback", "WindowScreenshot")]
    [string]$CaptureMode = "RuntimeReadback",
    [string]$SaveDir = "",
    [string]$Seed = "",
    [string]$WorldTime = "",
    [string]$PlayerPosition = "",
    [string]$PlayerRotation = "",
    [switch]$ShowDebugInfo,
    [switch]$SpawnValidationActors,
    [switch]$ShowOreFixture,
    [switch]$StopExisting,
    [switch]$KeepAlive,
    [switch]$ValidateCapturePolling
)

$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptRoot "..")).Path
$BinDir = Join-Path $RepoRoot "bin"
$ExeName = "HelloMine3D.exe"
$ExePath = Join-Path $BinDir $ExeName
$RunId = "{0:yyyyMMddHHmmssfff}-{1}" -f (Get-Date), $PID

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $BinDir "render_capture_$RunId"
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

function Test-CompletePng {
    param(
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return $false
    }

    $stream = $null
    $reader = $null
    try {
        $stream = [System.IO.File]::Open(
            $Path,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read,
            [System.IO.FileShare]::ReadWrite)
        if ($stream.Length -lt 20) {
            return $false
        }

        $reader = New-Object System.IO.BinaryReader($stream)
        $expectedSignature = [byte[]](137, 80, 78, 71, 13, 10, 26, 10)
        $signature = $reader.ReadBytes($expectedSignature.Length)
        if ($signature.Length -ne $expectedSignature.Length) {
            return $false
        }
        for ($index = 0; $index -lt $expectedSignature.Length; ++$index) {
            if ($signature[$index] -ne $expectedSignature[$index]) {
                return $false
            }
        }

        while ($stream.Position + 12 -le $stream.Length) {
            $lengthBytes = $reader.ReadBytes(4)
            [Array]::Reverse($lengthBytes)
            $chunkLength = [BitConverter]::ToInt32($lengthBytes, 0)
            if ($chunkLength -lt 0) {
                return $false
            }

            $chunkTypeBytes = $reader.ReadBytes(4)
            if ($chunkTypeBytes.Length -ne 4) {
                return $false
            }
            $remainingBytes = $stream.Length - $stream.Position
            if ([int64]$chunkLength + 4 -gt $remainingBytes) {
                return $false
            }

            $stream.Seek($chunkLength, [System.IO.SeekOrigin]::Current) |
                Out-Null
            if ($reader.ReadBytes(4).Length -ne 4) {
                return $false
            }

            $chunkType = [System.Text.Encoding]::ASCII.GetString(
                $chunkTypeBytes)
            if ($chunkType -eq "IEND") {
                return $chunkLength -eq 0 -and
                    $stream.Position -eq $stream.Length
            }
        }
    }
    catch {
        return $false
    }
    finally {
        if ($null -ne $reader) {
            $reader.Dispose()
        }
        elseif ($null -ne $stream) {
            $stream.Dispose()
        }
    }

    return $false
}

function Get-PendingCaptureFiles {
    param(
        [string[]]$Paths
    )

    $pending = @()
    foreach ($filePath in $Paths) {
        if (-not (Test-Path -LiteralPath $filePath)) {
            $pending += $filePath
        }
        elseif ((Get-Item -LiteralPath $filePath).Length -le 0) {
            $pending += "$filePath (empty)"
        }
        elseif (-not (Test-CompletePng -Path $filePath)) {
            $pending += "$filePath (incomplete PNG)"
        }
    }
    return $pending
}

function Wait-RuntimeCaptureFiles {
    param(
        [System.Diagnostics.Process]$Process,
        [string[]]$Paths,
        [int]$TimeoutMs,
        [int]$ExitGraceMs = 2000
    )

    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    $exitDeadline = $null
    do {
        # Check the files before HasExited. The runtime intentionally exits
        # immediately after its last write, so process exit is not evidence
        # that a completed capture is missing.
        $pending = @(Get-PendingCaptureFiles -Paths $Paths)
        $Process.Refresh()
        if ($pending.Count -eq 0) {
            if ($Process.HasExited) {
                $Process.WaitForExit()
                $Process.Refresh()
                if ($Process.ExitCode -ne 0) {
                    throw "Runtime capture process failed: exitCode=$($Process.ExitCode)"
                }
            }
            return
        }

        if ($Process.HasExited) {
            $Process.WaitForExit()
            $Process.Refresh()
            $exitCode = $Process.ExitCode
            if ($exitCode -ne 0) {
                throw "Process exited before runtime captures completed: exitCode=$exitCode; pending=$($pending -join '; ')"
            }
            if ($null -eq $exitDeadline) {
                $exitDeadline = (Get-Date).AddMilliseconds($ExitGraceMs)
            }
            elseif ((Get-Date) -ge $exitDeadline) {
                throw "Process exited cleanly but runtime captures remained incomplete: $($pending -join '; ')"
            }
        }

        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)

    throw "Timed out waiting for runtime captures: $($pending -join '; ')"
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

if ($ValidateCapturePolling) {
    $validationRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("HelloMine3D_capture_polling_{0}_{1}" -f $PID, [guid]::NewGuid())
    New-Item -ItemType Directory -Force -Path $validationRoot | Out-Null
    try {
        $validPngBase64 =
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="
        $validPngBytes = [Convert]::FromBase64String($validPngBase64)
        $truncatedPngBytes = $validPngBytes[0..31]

        $incompletePath = Join-Path $validationRoot "incomplete.png"
        [System.IO.File]::WriteAllBytes($incompletePath,
                                        $truncatedPngBytes)
        $completedProbe = Start-Process -FilePath $env:ComSpec `
            -ArgumentList @("/d", "/c", "exit 0") `
            -WindowStyle Hidden -PassThru
        $completedProbe.WaitForExit()
        $incompleteRejected = $false
        try {
            Wait-RuntimeCaptureFiles -Process $completedProbe `
                -Paths @($incompletePath) -TimeoutMs 1000 -ExitGraceMs 100
        }
        catch {
            $incompleteRejected =
                $_.Exception.Message -match "incomplete PNG"
        }
        finally {
            $completedProbe.Dispose()
        }
        if (-not $incompleteRejected) {
            throw "Incomplete PNG fixture was accepted as complete."
        }
        Write-Host "[RENDER_CAPTURE_POLLING] incomplete_png status=REJECTED"

        foreach ($iteration in 1..10) {
            $paths = @(
                (Join-Path $validationRoot ("capture_{0}_a.png" -f $iteration)),
                (Join-Path $validationRoot ("capture_{0}_b.png" -f $iteration))
            )

            $escapedPaths = @($paths | ForEach-Object {
                $_.Replace("'", "''")
            })
            $writerScript =
                "`$truncated=[Convert]::FromBase64String('" +
                [Convert]::ToBase64String($truncatedPngBytes) +
                "'); `$complete=[Convert]::FromBase64String('" +
                $validPngBase64 + "'); " +
                "[IO.File]::WriteAllBytes('" + $escapedPaths[0] +
                "',`$truncated); [IO.File]::WriteAllBytes('" +
                $escapedPaths[1] + "',`$truncated); " +
                "Start-Sleep -Milliseconds 100; " +
                "[IO.File]::WriteAllBytes('" + $escapedPaths[0] +
                "',`$complete); [IO.File]::WriteAllBytes('" +
                $escapedPaths[1] + "',`$complete)"
            $encodedWriter = [Convert]::ToBase64String(
                [System.Text.Encoding]::Unicode.GetBytes($writerScript))
            $probe = Start-Process -FilePath "powershell.exe" `
                -ArgumentList @("-NoProfile", "-EncodedCommand",
                                $encodedWriter) `
                -WindowStyle Hidden -PassThru
            try {
                Wait-RuntimeCaptureFiles -Process $probe -Paths $paths `
                    -TimeoutMs 2000 -ExitGraceMs 250
            }
            finally {
                $probe.Dispose()
            }
            Write-Host "[RENDER_CAPTURE_POLLING] run=$iteration status=PASS"
        }
    }
    finally {
        Remove-Item -LiteralPath $validationRoot -Recurse -Force `
            -ErrorAction SilentlyContinue
    }

    Write-Host "[RENDER_CAPTURE_POLLING] runs=10 status=PASS"
    exit 0
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
if (-not [string]::IsNullOrWhiteSpace($WorldTime)) { Write-Host "[RENDER_CAPTURE] worldTime=$WorldTime" }
if (-not [string]::IsNullOrWhiteSpace($PlayerPosition)) { Write-Host "[RENDER_CAPTURE] playerPosition=$PlayerPosition" }
if (-not [string]::IsNullOrWhiteSpace($PlayerRotation)) { Write-Host "[RENDER_CAPTURE] playerRotation=$PlayerRotation" }
if ($ShowDebugInfo) { Write-Host "[RENDER_CAPTURE] showDebugInfo=true" }
if ($SpawnValidationActors) { Write-Host "[RENDER_CAPTURE] spawnValidationActors=true" }
if ($ShowOreFixture) { Write-Host "[RENDER_CAPTURE] showOreFixture=true" }

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
if (-not [string]::IsNullOrWhiteSpace($WorldTime)) {
    $envValues["HELLOMINE3D_WORLD_TIME"] = $WorldTime
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
if ($SpawnValidationActors) {
    $envValues["HELLOMINE3D_SPAWN_VALIDATION_ACTORS"] = "1"
}
if ($ShowOreFixture) {
    $envValues["HELLOMINE3D_ORE_FIXTURE"] = "1"
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
# PowerShell 5 opens the native process handle lazily. If a fast runtime exits
# before the first handle access, both Handle and ExitCode can remain empty
# even after WaitForExit(). Prime the handle while the process is alive so the
# final exit code remains available after the last capture closes the client.
$processHandle = $process.Handle
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
        Wait-RuntimeCaptureFiles -Process $process -Paths $screenshotPaths `
            -TimeoutMs ($StartupTimeoutMs + $lastCaptureMs + 5000)

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

$missing = @(Get-PendingCaptureFiles -Paths $screenshotPaths)

if ($missing.Count -gt 0) {
    throw "Render capture missing files: $($missing -join '; ')"
}

Write-Host "[RENDER_CAPTURE] status=PASS outputDir=$OutputDir"
