[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SidecarPath,
    [string]$ImagePath = "",
    [string]$PdbPath = "",
    [string]$SymbolizerExe = "",
    [string]$SymbolizerPdb = "",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BinRoot = Join-Path $RepoRoot "bin"
if ([string]::IsNullOrWhiteSpace($ImagePath)) {
    $ImagePath = Join-Path $BinRoot "HelloMine3D.exe"
}
if ([string]::IsNullOrWhiteSpace($PdbPath)) {
    $PdbPath = Join-Path $BinRoot "HelloMine3D.pdb"
}
if ([string]::IsNullOrWhiteSpace($SymbolizerExe)) {
    $SymbolizerExe = Join-Path $BinRoot "HelloMine3DCrashDiagnosticsSmoke.exe"
}
if ([string]::IsNullOrWhiteSpace($SymbolizerPdb)) {
    $SymbolizerPdb = Join-Path $BinRoot "HelloMine3DCrashDiagnosticsSmoke.pdb"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $BinRoot "symbol_archives"
}
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)
if (-not $OutputDir.StartsWith(
        [System.IO.Path]::GetFullPath($BinRoot) +
            [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Symbol archive output must stay below $BinRoot"
}

foreach ($path in @($SidecarPath, $ImagePath, $PdbPath,
                     $SymbolizerExe, $SymbolizerPdb)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
        (Get-Item -LiteralPath $path).Length -le 0) {
        throw "Symbol archive input is missing or empty: $path"
    }
}

$SidecarPath = [System.IO.Path]::GetFullPath($SidecarPath)
$sidecarText = Get-Content -LiteralPath $SidecarPath -Raw
$identityMatch = [regex]::Match(
    $sidecarText, '(?m)^build_identity (pdb-[0-9a-f]{32}-[1-9][0-9]*)$')
if (-not $identityMatch.Success) {
    throw "Crash sidecar has no canonical build identity."
}
$BuildIdentity = $identityMatch.Groups[1].Value

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
    "--sidecar", (& $quote $SidecarPath),
    "--image", (& $quote ([System.IO.Path]::GetFullPath($ImagePath))),
    "--pdb", (& $quote ([System.IO.Path]::GetFullPath($PdbPath)))
) -join " "
$process = New-Object System.Diagnostics.Process
$process.StartInfo = $startInfo
if (-not $process.Start()) {
    throw "Unable to start the offline symbol verifier."
}
$stdoutTask = $process.StandardOutput.ReadToEndAsync()
$stderrTask = $process.StandardError.ReadToEndAsync()
if (-not $process.WaitForExit(30000)) {
    $process.Kill()
    $process.WaitForExit()
    throw "Offline symbol verification timed out."
}
$stdout = $stdoutTask.Result
$stderr = $stderrTask.Result
if ($process.ExitCode -ne 0 -or
    -not $stdout.Contains("status=PASS") -or
    -not $stdout.Contains("mode=minidump-stack-hybrid")) {
    throw "Symbol inputs do not resolve the supplied dump: $stderr"
}

$ArchiveName = "HelloMine3D-Windows-x64-symbols-$BuildIdentity"
$StagingRoot = Join-Path $OutputDir $ArchiveName
$ArchivePath = Join-Path $OutputDir "$ArchiveName.zip"
if (Test-Path -LiteralPath $StagingRoot) {
    Remove-Item -LiteralPath $StagingRoot -Recurse -Force
}
if (Test-Path -LiteralPath $ArchivePath) {
    Remove-Item -LiteralPath $ArchivePath -Force
}
New-Item -ItemType Directory -Path (Join-Path $StagingRoot "bin") `
    -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $StagingRoot "tools") `
    -Force | Out-Null
Copy-Item -LiteralPath $ImagePath `
    -Destination (Join-Path $StagingRoot "bin\HelloMine3D.exe") -Force
Copy-Item -LiteralPath $PdbPath `
    -Destination (Join-Path $StagingRoot "bin\HelloMine3D.pdb") -Force
Copy-Item -LiteralPath $SymbolizerExe `
    -Destination (Join-Path $StagingRoot "bin\HelloMine3DCrashDiagnosticsSmoke.exe") -Force
Copy-Item -LiteralPath $SymbolizerPdb `
    -Destination (Join-Path $StagingRoot "bin\HelloMine3DCrashDiagnosticsSmoke.pdb") -Force
Copy-Item -LiteralPath (Join-Path $RepoRoot "tools\symbolize_crash_sidecar.ps1") `
    -Destination (Join-Path $StagingRoot "tools\symbolize_crash_sidecar.ps1") -Force

$readme = @(
    "HelloMine3D offline symbols",
    "build_identity=$BuildIdentity",
    "upload=disabled",
    "",
    "Place a matching .dmp and .crash.txt outside this archive, then run:",
    "powershell -File tools\symbolize_crash_sidecar.ps1 -SidecarPath <report.crash.txt>"
)
[System.IO.File]::WriteAllText(
    (Join-Path $StagingRoot "README-symbols.txt"),
    (($readme -join "`n") + "`n"),
    (New-Object System.Text.UTF8Encoding($false)))

function Get-Sha256File {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

$manifestPath = Join-Path $StagingRoot "symbol-manifest.txt"
$manifest = @(
    Get-ChildItem -LiteralPath $StagingRoot -Recurse -File |
        Where-Object { $_.FullName -ne $manifestPath } |
        ForEach-Object {
            $relative = $_.FullName.Substring($StagingRoot.Length + 1).Replace('\', '/')
            "$(Get-Sha256File $_.FullName)|$($_.Length)|$relative"
        } |
        Sort-Object -CaseSensitive
)
[System.IO.File]::WriteAllText(
    $manifestPath, (($manifest -join "`n") + "`n"),
    (New-Object System.Text.UTF8Encoding($false)))

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stream = [System.IO.File]::Open(
    $ArchivePath, [System.IO.FileMode]::CreateNew)
try {
    $archive = New-Object System.IO.Compression.ZipArchive(
        $stream, [System.IO.Compression.ZipArchiveMode]::Create, $false)
    try {
        $fixedTime = [DateTimeOffset]::Parse("2000-01-01T00:00:00Z")
        foreach ($file in Get-ChildItem -LiteralPath $StagingRoot `
            -Recurse -File | Sort-Object FullName) {
            $relative = $file.FullName.Substring(
                $StagingRoot.Length + 1).Replace('\', '/')
            $entry = $archive.CreateEntry(
                "$ArchiveName/$relative",
                [System.IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = $fixedTime
            $entryStream = $entry.Open()
            try {
                $source = [System.IO.File]::OpenRead($file.FullName)
                try { $source.CopyTo($entryStream) }
                finally { $source.Dispose() }
            }
            finally { $entryStream.Dispose() }
        }
    }
    finally { $archive.Dispose() }
}
finally { $stream.Dispose() }

$ArchiveHash = Get-Sha256File $ArchivePath
Write-Host "[SYMBOL_ARCHIVE] status=PASS build_identity=$BuildIdentity entries=$($manifest.Count + 1) archive_sha256=$ArchiveHash"
Write-Host "[SYMBOL_ARCHIVE] archive=$ArchivePath"
