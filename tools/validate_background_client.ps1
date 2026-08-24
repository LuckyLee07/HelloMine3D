param(
    [string]$ExePath = "",
    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
$binRoot = Join-Path $repoRoot "bin"
if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $ExePath = Join-Path $binRoot "HelloMine3D.exe"
}
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    throw "Client executable not found: $ExePath"
}
if (-not ("HelloMine3DBackgroundValidation.NativeMethods" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

namespace HelloMine3DBackgroundValidation
{
    public static class NativeMethods
    {
        private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        [DllImport("user32.dll")]
        private static extern bool EnumWindows(EnumWindowsProc callback,
                                               IntPtr lParam);

        [DllImport("user32.dll")]
        private static extern bool IsWindowVisible(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern uint GetWindowThreadProcessId(
            IntPtr hWnd, out uint processId);

        [DllImport("user32.dll")]
        public static extern IntPtr GetForegroundWindow();

        public static uint GetProcessId(IntPtr hWnd)
        {
            uint processId;
            GetWindowThreadProcessId(hWnd, out processId);
            return processId;
        }

        public static bool HasVisibleWindow(uint targetProcessId)
        {
            bool found = false;
            EnumWindows(delegate(IntPtr hWnd, IntPtr lParam)
            {
                uint processId;
                GetWindowThreadProcessId(hWnd, out processId);
                if (processId == targetProcessId && IsWindowVisible(hWnd))
                {
                    found = true;
                    return false;
                }
                return true;
            }, IntPtr.Zero);
            return found;
        }
    }
}
"@
}

$runId = "{0:yyyyMMddHHmmssfff}-{1}" -f (Get-Date), $PID
$saveRoot = Join-Path $repoRoot "tmp\background_client_$runId"
New-Item -ItemType Directory -Path $saveRoot -Force | Out-Null

$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $ExePath
$startInfo.WorkingDirectory = Split-Path -Parent $ExePath
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$startInfo.EnvironmentVariables["HELLOMINE3D_ROOT"] = $repoRoot
$startInfo.EnvironmentVariables["HELLOMINE3D_WINDOW_HIDDEN"] = "1"
$startInfo.EnvironmentVariables["HELLOMINE3D_VALIDATE_ONLY"] = "1"
$startInfo.EnvironmentVariables["HELLOMINE3D_SEED"] = "20260824"
$startInfo.EnvironmentVariables["HELLOMINE3D_PLAYER_POSITION"] = "264 96 8"
$startInfo.EnvironmentVariables["HELLOMINE3D_SAVE_DIR"] = $saveRoot

$process = New-Object System.Diagnostics.Process
$process.StartInfo = $startInfo
if (-not $process.Start()) {
    throw "Failed to start hidden client."
}
$stdoutTask = $process.StandardOutput.ReadToEndAsync()
$stderrTask = $process.StandardError.ReadToEndAsync()
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$ownedForeground = $false
$visibleWindow = $false
try {
    while (-not $process.HasExited) {
        $foreground = [HelloMine3DBackgroundValidation.NativeMethods]::GetForegroundWindow()
        if ([HelloMine3DBackgroundValidation.NativeMethods]::GetProcessId(
                $foreground) -eq [uint32]$process.Id) {
            $ownedForeground = $true
        }
        if ([HelloMine3DBackgroundValidation.NativeMethods]::HasVisibleWindow(
                [uint32]$process.Id)) {
            $visibleWindow = $true
        }
        if ($stopwatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
            $process.Kill()
            throw "Hidden client timed out after $TimeoutSeconds seconds."
        }
        Start-Sleep -Milliseconds 10
        $process.Refresh()
    }
    $process.WaitForExit()
}
finally {
    if (-not $process.HasExited) {
        $process.Kill()
        $process.WaitForExit()
    }
}

$stdout = $stdoutTask.Result
$stderr = $stderrTask.Result
if (-not [string]::IsNullOrWhiteSpace($stdout)) {
    $stdout.TrimEnd() | Write-Host
}
if (-not [string]::IsNullOrWhiteSpace($stderr)) {
    $stderr.TrimEnd() | Write-Host
}
if ($process.ExitCode -ne 0) {
    throw "Hidden client failed with exit code $($process.ExitCode)."
}
if ($ownedForeground) {
    throw "Hidden client became the foreground process."
}
if ($visibleWindow) {
    throw "Hidden client created a visible top-level window."
}

Write-Host "[BACKGROUND_CLIENT] status=PASS foreground_owned=false visible_window=false exit_code=0"
