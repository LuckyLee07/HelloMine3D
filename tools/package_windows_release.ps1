[CmdletBinding()]
param(
    [string]$OutputRoot = "",
    [string[]]$IncludePack = @(),
    [switch]$SkipRealWindow
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $RepoRoot "bin\package_runs\release"
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
$AllowedOutputParent = [System.IO.Path]::GetFullPath(
    (Join-Path $RepoRoot "bin\package_runs"))
if (-not $OutputRoot.StartsWith(
        $AllowedOutputParent + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Package output must stay under $AllowedOutputParent"
}

$DistributionName = "HelloMine3D-Windows-x64"
$DistributionRoot = Join-Path $OutputRoot $DistributionName
$ArchivePath = Join-Path $OutputRoot "$DistributionName.zip"
$ManifestPath = Join-Path $DistributionRoot `
    "distribution-manifest.txt"
$BaseManifest = Join-Path $RepoRoot "media\resource-manifest.txt"
$ClientPath = Join-Path $RepoRoot "bin\HelloMine3D.exe"

foreach ($required in @($BaseManifest, $ClientPath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Release packaging input is missing: $required"
    }
}

if (Test-Path -LiteralPath $OutputRoot) {
    Remove-Item -LiteralPath $OutputRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $DistributionRoot -Force | Out-Null

function Copy-RelativeFile {
    param([string]$RelativePath)
    $normalized = $RelativePath.Replace('/', '\')
    $source = Join-Path $RepoRoot $normalized
    $target = Join-Path $DistributionRoot $normalized
    if (-not (Test-Path -LiteralPath $source -PathType Leaf) -or
        (Get-Item -LiteralPath $source).Length -le 0) {
        throw "Package input is missing or empty: $RelativePath"
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $target) `
        -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $target -Force
}

$baseEntries = @(
    Get-Content -LiteralPath $BaseManifest |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -and -not $_.StartsWith('#') }
)
foreach ($entry in $baseEntries) {
    $separator = $entry.IndexOf('|')
    if ($separator -le 0) {
        throw "Invalid base resource manifest entry: $entry"
    }
    Copy-RelativeFile $entry.Substring($separator + 1)
}
Copy-RelativeFile "media/resource-manifest.txt"
Copy-RelativeFile "README.md"

$clientTarget = Join-Path $DistributionRoot "bin\HelloMine3D.exe"
New-Item -ItemType Directory -Path (Split-Path -Parent $clientTarget) `
    -Force | Out-Null
Copy-Item -LiteralPath $ClientPath -Destination $clientTarget -Force

$notices = [ordered]@{
    "Ogre-LICENSE.txt" = "src\Engine\ogre3d\LICENSE.txt"
    "OIS-LICENSE.md" = "src\external\ois\LICENSE.md"
    "DearImGui-LICENSE.txt" = "src\external\imgui\LICENSE.txt"
    "Tracy-LICENSE.txt" = "src\external\tracy\LICENSE"
    "GLM-COPYING.txt" = "src\external\glm\copying.txt"
    "FreeImage-LICENSE.txt" = "src\Engine\ThirdParty\freeimage\LICENSE.txt"
    "FreeType-LICENSE.txt" = "src\Engine\ThirdParty\freetype\LICENSE.TXT"
    "LibJPEG-LICENSE.txt" = "src\Engine\ThirdParty\libjpeg\LICENSE.txt"
    "LibOpenJPEG-LICENSE.txt" = "src\Engine\ThirdParty\libopenjpeg\LICENSE.txt"
    "LibPNG-LICENSE.txt" = "src\Engine\ThirdParty\libpng\LICENSE.txt"
    "LibRaw-LGPL.txt" = "src\Engine\ThirdParty\libraw\LICENSE.LGPL"
    "LibRaw-CDDL.txt" = "src\Engine\ThirdParty\libraw\LICENSE.CDDL"
    "LibTIFF-LICENSE.txt" = "src\Engine\ThirdParty\libtiff4\LICENSE.txt"
    "ZLib-LICENSE.txt" = "src\Engine\ThirdParty\zlib\LICENSE.txt"
    "ZZipLib-LICENSE.txt" = "src\Engine\ThirdParty\zzip\LICENSE.txt"
}
$noticeRoot = Join-Path $DistributionRoot "notices"
New-Item -ItemType Directory -Path $noticeRoot -Force | Out-Null
foreach ($notice in $notices.GetEnumerator()) {
    $source = Join-Path $RepoRoot $notice.Value
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required third-party notice is missing: $source"
    }
    Copy-Item -LiteralPath $source `
        -Destination (Join-Path $noticeRoot $notice.Key) -Force
}

$enabledPackNames = New-Object 'System.Collections.Generic.List[string]'
foreach ($packName in $IncludePack) {
    if ($packName -notmatch '^[A-Za-z0-9_.-]+$') {
        throw "Invalid package resource-pack name: $packName"
    }
    $sourcePack = Join-Path $RepoRoot "packs\$packName"
    if (-not (Test-Path -LiteralPath $sourcePack -PathType Container)) {
        throw "Optional resource pack is missing: $sourcePack"
    }
    $targetPack = Join-Path $DistributionRoot "packs\$packName"
    New-Item -ItemType Directory -Path (Split-Path -Parent $targetPack) `
        -Force | Out-Null
    Copy-Item -LiteralPath $sourcePack -Destination $targetPack `
        -Recurse -Force
    $enabledPackNames.Add($packName)
}

$packConfigPath = Join-Path $DistributionRoot "bin\resource-packs.txt"
$packConfigLines = @(
    '# HelloMine3D resource packs, highest priority first.'
) + @($enabledPackNames)
[System.IO.File]::WriteAllText(
    $packConfigPath, (($packConfigLines -join "`n") + "`n"),
    (New-Object System.Text.UTF8Encoding($false)))

function Get-RelativeDistributionPath {
    param([string]$Path)
    return $Path.Substring($DistributionRoot.Length + 1).Replace('\', '/')
}

function Get-Sha256File {
    param([string]$Path)
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try {
            return [System.BitConverter]::ToString(
                $sha256.ComputeHash($stream)).Replace('-', '')
        }
        finally {
            $sha256.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

function Write-DistributionManifest {
    $records = @(
        Get-ChildItem -LiteralPath $DistributionRoot -Recurse -File |
            Where-Object { $_.FullName -ne $ManifestPath } |
            ForEach-Object {
                $relative = Get-RelativeDistributionPath $_.FullName
                $hash = (Get-Sha256File `
                    -Path $_.FullName).ToLowerInvariant()
                "$hash|$($_.Length)|$relative"
            } |
            Sort-Object -CaseSensitive
    )
    [System.IO.File]::WriteAllText(
        $ManifestPath, (($records -join "`n") + "`n"),
        (New-Object System.Text.UTF8Encoding($false)))
    return $records
}

function Test-DistributionInventory {
    param([string]$Root)
    $manifest = Join-Path $Root "distribution-manifest.txt"
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
        return $false
    }
    $declared = @{}
    foreach ($line in Get-Content -LiteralPath $manifest) {
        $parts = $line.Split('|')
        if ($parts.Count -ne 3 -or $declared.ContainsKey($parts[2])) {
            return $false
        }
        $declared[$parts[2]] = $parts
    }
    $actual = @(
        Get-ChildItem -LiteralPath $Root -Recurse -File |
            Where-Object { $_.FullName -ne $manifest }
    )
    if ($actual.Count -ne $declared.Count) {
        return $false
    }
    foreach ($file in $actual) {
        $relative = $file.FullName.Substring($Root.Length + 1).Replace('\', '/')
        if (-not $declared.ContainsKey($relative)) {
            return $false
        }
        $expected = $declared[$relative]
        $hash = (Get-Sha256File `
            -Path $file.FullName).ToLowerInvariant()
        if ($expected[0] -ne $hash -or
            [long]$expected[1] -ne $file.Length) {
            return $false
        }
    }
    return $true
}

$inventory = Write-DistributionManifest
$forbidden = @($inventory | Where-Object {
    $_ -match '(?i)(^|/)(saves?|logs?|captures?|crashes?|build|validation_runs)(/|$)' -or
    $_ -match '(?i)\.(pdb|obj|lib|ilk|csv)$'
})
if ($forbidden.Count -gt 0 -or
    -not (Test-DistributionInventory $DistributionRoot)) {
    throw "Distribution inventory contains forbidden or inconsistent files: $forbidden"
}

function Invoke-PackagedClient {
    param(
        [string]$Root,
        [bool]$ValidateOnly,
        [string]$Name,
        [bool]$ExpectSuccess,
        [string]$ControlledCrash = "",
        [string]$CrashDirectory = "",
        [int]$ExpectedCrashArtifacts = 0
    )
    $exe = Join-Path $Root "bin\HelloMine3D.exe"
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $exe
    $startInfo.WorkingDirectory = Join-Path $Root "bin"
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $caseCrashDirectory = if ([string]::IsNullOrWhiteSpace($CrashDirectory)) {
        Join-Path $OutputRoot "runtime-crashes\$Name"
    }
    else { $CrashDirectory }
    $environmentOverrides = [ordered]@{
        HELLOMINE3D_ROOT = $Root
        HELLOMINE3D_WINDOW_HIDDEN = "1"
        HELLOMINE3D_SEED = "20260809"
        HELLOMINE3D_PLAYER_POSITION = "264 96 8"
        HELLOMINE3D_PLAYER_ROTATION = "0 0 0"
        HELLOMINE3D_SAVE_DIR = Join-Path $OutputRoot "runtime-state\$Name"
        HELLOMINE3D_CRASH_DIR = $caseCrashDirectory
        HELLOMINE3D_CONTROLLED_CRASH = $ControlledCrash
        HELLOMINE3D_STARTUP_ERROR_NO_DIALOG = "1"
        HELLOMINE3D_RESOURCE_PACKS = $null
        HELLOMINE3D_VALIDATE_ONLY = $null
        HELLOMINE3D_TRANSPARENT_FIXTURE = $null
        HELLOMINE3D_EXIT_AFTER_FRAMES = $null
    }
    if ($ValidateOnly) {
        $environmentOverrides["HELLOMINE3D_VALIDATE_ONLY"] = "1"
        $environmentOverrides["HELLOMINE3D_TRANSPARENT_FIXTURE"] = "1"
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
            throw "Failed to start packaged client case $Name."
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
    if (-not $process.WaitForExit(60000)) {
        $process.Kill()
        $process.WaitForExit()
        throw "Packaged client case $Name timed out."
    }
    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    $exitCode = $process.ExitCode
    $process.Dispose()
    [System.IO.File]::WriteAllText(
        (Join-Path $OutputRoot "$Name.stdout.log"), $stdout,
        (New-Object System.Text.UTF8Encoding($false)))
    [System.IO.File]::WriteAllText(
        (Join-Path $OutputRoot "$Name.stderr.log"), $stderr,
        (New-Object System.Text.UTF8Encoding($false)))
    if ($ExpectSuccess -and $exitCode -ne 0) {
        throw "Packaged client $Name failed: exit=$exitCode stderr=$stderr"
    }
    if (-not $ExpectSuccess -and $exitCode -eq 0) {
        throw "Negative packaged client $Name unexpectedly succeeded."
    }
    $unexpectedDumps = @()
    if (Test-Path -LiteralPath $caseCrashDirectory -PathType Container) {
        $unexpectedDumps = @(
            Get-ChildItem -LiteralPath $caseCrashDirectory `
                -Filter "*.dmp" -File
        )
    }
    $sidecars = @()
    if (Test-Path -LiteralPath $caseCrashDirectory -PathType Container) {
        $sidecars = @(
            Get-ChildItem -LiteralPath $caseCrashDirectory `
                -Filter "*.crash.txt" -File
        )
    }
    if ($unexpectedDumps.Count -ne $ExpectedCrashArtifacts -or
        $sidecars.Count -ne $ExpectedCrashArtifacts) {
        throw "Packaged client $Name expected $ExpectedCrashArtifacts crash artifact pairs but found $($unexpectedDumps.Count) dumps and $($sidecars.Count) sidecars."
    }
    return [pscustomobject]@{
        ExitCode = $exitCode
        Stdout = $stdout
        Stderr = $stderr
    }
}

$ValidationRoot = Join-Path $OutputRoot "clean-root"
Copy-Item -LiteralPath $DistributionRoot -Destination $ValidationRoot `
    -Recurse -Force
if (-not (Test-DistributionInventory $ValidationRoot)) {
    throw "Copied clean-root distribution failed inventory verification."
}
$validationResult = Invoke-PackagedClient $ValidationRoot $true `
    "validation-only" $true
if (-not $validationResult.Stdout.Contains("[RESOURCE_PACK] enabled=")) {
    throw "Packaged validation did not report its effective resource view."
}

if (-not $SkipRealWindow) {
    $null = Invoke-PackagedClient $ValidationRoot $false `
        "real-window-three-frames" $true
}

$h3CrashDirectory = Join-Path $OutputRoot "runtime-crashes\h3-local"
$controlledPackageCrash = Invoke-PackagedClient `
    -Root $ValidationRoot `
    -ValidateOnly $false `
    -Name "controlled-crash" `
    -ExpectSuccess $false `
    -ControlledCrash "after-first-frame" `
    -CrashDirectory $h3CrashDirectory `
    -ExpectedCrashArtifacts 1
if (-not $controlledPackageCrash.Stdout.Contains(
        "controlled_crash=after-first-frame active_world_saved=1")) {
    throw "Packaged controlled crash did not publish the active world."
}
$postCrashPrompt = Invoke-PackagedClient `
    -Root $ValidationRoot `
    -ValidateOnly $false `
    -Name "next-start-crash-prompt" `
    -ExpectSuccess $true `
    -CrashDirectory $h3CrashDirectory `
    -ExpectedCrashArtifacts 1
if (-not $postCrashPrompt.Stdout.Contains(
        "[CRASH_REPORT] pending=1 ignored=0 invalid=0 upload=0")) {
    throw "Packaged next startup did not expose the local crash prompt."
}

$missingRoot = Join-Path $OutputRoot "negative-missing"
Copy-Item -LiteralPath $DistributionRoot -Destination $missingRoot `
    -Recurse -Force
$missingShader = Join-Path $missingRoot `
    "media\ogre\HelloMine3DTerrain.vert"
Remove-Item -LiteralPath $missingShader -Force
$missingResult = Invoke-PackagedClient $missingRoot $true `
    "negative-missing" $false
if (-not $missingResult.Stderr.Contains(
        "Missing or empty effective shader resource") -or
    -not $missingResult.Stderr.Contains(
        "media/ogre/HelloMine3DTerrain.vert")) {
    throw "Missing-resource package diagnostic was not specific: $($missingResult.Stderr)"
}

$staleRoot = Join-Path $OutputRoot "negative-stale"
Copy-Item -LiteralPath $DistributionRoot -Destination $staleRoot `
    -Recurse -Force
[System.IO.File]::WriteAllText(
    (Join-Path $staleRoot "media\textures\stale.txt"), "stale`n",
    (New-Object System.Text.UTF8Encoding($false)))
if (Test-DistributionInventory $staleRoot) {
    throw "Stale-resource inventory fixture unexpectedly passed."
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path -LiteralPath $ArchivePath) {
    Remove-Item -LiteralPath $ArchivePath -Force
}
$stream = [System.IO.File]::Open(
    $ArchivePath, [System.IO.FileMode]::CreateNew)
try {
    $archive = New-Object System.IO.Compression.ZipArchive(
        $stream, [System.IO.Compression.ZipArchiveMode]::Create, $false)
    try {
        $fixedTime = [DateTimeOffset]::Parse("2000-01-01T00:00:00Z")
        foreach ($file in Get-ChildItem -LiteralPath $DistributionRoot `
            -Recurse -File | Sort-Object FullName) {
            $relative = Get-RelativeDistributionPath $file.FullName
            $entry = $archive.CreateEntry(
                "$DistributionName/$relative",
                [System.IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = $fixedTime
            $entryStream = $entry.Open()
            try {
                $sourceStream = [System.IO.File]::OpenRead($file.FullName)
                try { $sourceStream.CopyTo($entryStream) }
                finally { $sourceStream.Dispose() }
            }
            finally { $entryStream.Dispose() }
        }
    }
    finally { $archive.Dispose() }
}
finally { $stream.Dispose() }

$archiveHash = Get-Sha256File -Path $ArchivePath
$summaryLines = @(
    "status=PASS",
    "distribution=$DistributionRoot",
    "archive=$ArchivePath",
    "archive_sha256=$archiveHash",
    "inventory_entries=$($inventory.Count)",
    "resource_manifest_entries=$($baseEntries.Count)",
    "included_packs=$($enabledPackNames -join ',')",
    "validation_only=PASS",
    "real_window=$(if ($SkipRealWindow) { 'SKIPPED' } else { 'PASS' })",
    "missing_resource_negative=PASS",
    "stale_resource_negative=PASS",
    "ordinary_crash_dumps=0",
    "controlled_package_crash=PASS",
    "next_start_local_prompt=PASS"
)
$summaryPath = Join-Path $OutputRoot "package-summary.txt"
[System.IO.File]::WriteAllText(
    $summaryPath, (($summaryLines -join "`n") + "`n"),
    (New-Object System.Text.UTF8Encoding($false)))

Write-Host "[PACKAGE] status=PASS inventory=$($inventory.Count) archive_sha256=$archiveHash"
Write-Host "[PACKAGE] distribution=$DistributionRoot"
Write-Host "[PACKAGE] archive=$ArchivePath"
