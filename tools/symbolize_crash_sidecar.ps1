[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SidecarPath,
    [string]$ImagePath = "",
    [string]$PdbPath = "",
    [string]$SymbolizerExe = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($ImagePath)) {
    $ImagePath = Join-Path $RepoRoot "bin\HelloMine3D.exe"
}
if ([string]::IsNullOrWhiteSpace($PdbPath)) {
    $PdbPath = Join-Path $RepoRoot "bin\HelloMine3D.pdb"
}
if ([string]::IsNullOrWhiteSpace($SymbolizerExe)) {
    $SymbolizerExe = Join-Path $RepoRoot `
        "bin\HelloMine3DCrashDiagnosticsSmoke.exe"
}

foreach ($path in @($SidecarPath, $ImagePath, $PdbPath, $SymbolizerExe)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Crash symbolization input is missing: $path"
    }
}

$symbolizerExitCode = 1
$quote = {
    param([string]$Value)
    return '"' + $Value.Replace('"', '\"') + '"'
}
$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $SymbolizerExe
$startInfo.WorkingDirectory = Split-Path -Parent $SymbolizerExe
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$startInfo.Arguments = @(
    "--symbolize",
    "--sidecar", (& $quote ([System.IO.Path]::GetFullPath($SidecarPath))),
    "--image", (& $quote ([System.IO.Path]::GetFullPath($ImagePath))),
    "--pdb", (& $quote ([System.IO.Path]::GetFullPath($PdbPath)))
) -join " "
$process = New-Object System.Diagnostics.Process
$process.StartInfo = $startInfo
if (-not $process.Start()) {
    throw "Unable to start crash symbolizer."
}
$stdoutTask = $process.StandardOutput.ReadToEndAsync()
$stderrTask = $process.StandardError.ReadToEndAsync()
if (-not $process.WaitForExit(30000)) {
    $process.Kill()
    $process.WaitForExit()
    throw "Crash symbolizer timed out."
}
$process.WaitForExit()
$process.Refresh()
$stdout = $stdoutTask.Result
$stderr = $stderrTask.Result
if (-not [string]::IsNullOrWhiteSpace($stdout)) {
    Write-Output $stdout.TrimEnd()
}
if (-not [string]::IsNullOrWhiteSpace($stderr)) {
    [Console]::Error.WriteLine($stderr.TrimEnd())
}
$symbolizerExitCode = [int]$process.ExitCode
exit $symbolizerExitCode
