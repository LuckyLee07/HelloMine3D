param()

$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptRoot "..")).Path
$Premake = Join-Path $RepoRoot "tools\premake\premake5.exe"
$PremakeDir = Join-Path $RepoRoot "premake"
$BuildDir = Join-Path $RepoRoot "build"
$GraphContractPath = Join-Path $ScriptRoot `
    "xcode-project-graph-contract-v1.json"

if (-not (Test-Path -LiteralPath $Premake -PathType Leaf)) {
    throw "Bundled Premake executable is missing: $Premake"
}

Push-Location $PremakeDir
try {
    & $Premake --os=macosx --file=premake.lua xcode4
    if ($LASTEXITCODE -ne 0) {
        throw "Premake Xcode generation failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

$Checks = 0

function Require-Text {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Label
    )

    $script:Checks++
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "[XCODE_VALIDATE] Missing $Label file: $Path"
    }
    $Text = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    if ($Text -notmatch $Pattern) {
        throw "[XCODE_VALIDATE] Missing ${Label}: $Pattern"
    }
}

function Reject-Text {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Label
    )

    $script:Checks++
    $Text = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    if ($Text -match $Pattern) {
        throw "[XCODE_VALIDATE] Unexpected ${Label}: $Pattern"
    }
}

function Assert-ExactInventory {
    param(
        [string[]]$Expected,
        [string[]]$Actual,
        [string]$Label
    )

    $script:Checks++
    $ExpectedSorted = @($Expected | Sort-Object)
    $ActualSorted = @($Actual | Sort-Object)
    $ExpectedKey = [string]::Join("`n", $ExpectedSorted)
    $ActualKey = [string]::Join("`n", $ActualSorted)
    if ($ExpectedSorted.Count -ne $ActualSorted.Count -or
        $ExpectedKey -cne $ActualKey) {
        $Missing = @($ExpectedSorted | Where-Object {
            $_ -cnotin $ActualSorted
        })
        $Unexpected = @($ActualSorted | Where-Object {
            $_ -cnotin $ExpectedSorted
        })
        throw "[XCODE_VALIDATE] $Label mismatch; missing=$($Missing -join ',') unexpected=$($Unexpected -join ',')"
    }
}

function Validate-PbxGraph {
    param(
        [string]$Path,
        [string]$Label
    )

    $script:Checks++
    $Text = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    $GroupSection = [regex]::Match(
        $Text,
        '(?s)/\* Begin PBXGroup section \*/(?<body>.*?)/\* End PBXGroup section \*/')
    if (-not $GroupSection.Success) {
        throw "[XCODE_VALIDATE] $Label has invalid PBXGroup markers."
    }
    $Groups = [regex]::Matches(
        $GroupSection.Groups['body'].Value,
        '(?ms)^\s*(?<group>[A-F0-9]{24})(?: /\*.*?\*/)? = \{\r?\n(?<body>.*?)^\s*\};')
    if ($Groups.Count -eq 0) {
        throw "[XCODE_VALIDATE] $Label has no PBXGroup objects."
    }

    $Memberships = @{}
    foreach ($Group in $Groups) {
        $GroupId = $Group.Groups['group'].Value
        $Body = $Group.Groups['body'].Value
        if ($Body -notmatch 'isa = PBXGroup;') {
            throw "[XCODE_VALIDATE] $Label group $GroupId has invalid isa."
        }
        $ChildrenMatch = [regex]::Match(
            $Body, '(?s)children = \((?<children>.*?)\);')
        if (-not $ChildrenMatch.Success) {
            throw "[XCODE_VALIDATE] $Label group $GroupId has no children list."
        }
        $ChildIds = @(
            [regex]::Matches(
                $ChildrenMatch.Groups['children'].Value,
                '(?m)^\s*(?<id>[A-F0-9]{24})(?: /\*.*?\*/)?,$') |
                ForEach-Object { $_.Groups['id'].Value }
        )
        if (@($ChildIds | Select-Object -Unique).Count -ne
            $ChildIds.Count) {
            throw "[XCODE_VALIDATE] $Label group $GroupId contains duplicate children."
        }
        foreach ($ChildId in $ChildIds) {
            if ($Memberships.ContainsKey($ChildId)) {
                $Memberships[$ChildId] = @($Memberships[$ChildId]) + $GroupId
            }
            else {
                $Memberships[$ChildId] = @($GroupId)
            }
        }
    }

    foreach ($ChildId in @($Memberships.Keys)) {
        if (@($Memberships[$ChildId]).Count -gt 1) {
            throw "[XCODE_VALIDATE] $Label child $ChildId belongs to multiple PBXGroups: $(@($Memberships[$ChildId]) -join ',')"
        }
    }

    $ProjectRefs = @(
        [regex]::Matches($Text, 'ProjectRef = (?<id>[A-F0-9]{24})') |
            ForEach-Object { $_.Groups['id'].Value }
    )
    if (@($ProjectRefs | Select-Object -Unique).Count -ne
        $ProjectRefs.Count) {
        throw "[XCODE_VALIDATE] $Label contains duplicate ProjectRef entries."
    }
    foreach ($ProjectRef in $ProjectRefs) {
        $Declaration = '(?m)^\s*' + [regex]::Escape($ProjectRef) +
            '(?: /\*.*?\*/)? = \{isa = PBXFileReference;'
        if ($Text -notmatch $Declaration) {
            throw "[XCODE_VALIDATE] $Label ProjectRef $ProjectRef is not a PBXFileReference."
        }
        if (-not $Memberships.ContainsKey($ProjectRef) -or
            @($Memberships[$ProjectRef]).Count -ne 1) {
            throw "[XCODE_VALIDATE] $Label ProjectRef $ProjectRef must belong to exactly one PBXGroup."
        }
    }
}

if (-not (Test-Path -LiteralPath $GraphContractPath -PathType Leaf)) {
    throw "[XCODE_VALIDATE] Missing graph contract: $GraphContractPath"
}
$GraphContract = Get-Content -LiteralPath $GraphContractPath -Raw `
    -Encoding UTF8 | ConvertFrom-Json
$Checks++
if ($GraphContract.contract_version -ne 1) {
    throw "[XCODE_VALIDATE] Graph contract must use contract_version=1."
}
$ExpectedProjects = @($GraphContract.projects | ForEach-Object {
    [string]$_
})
if ($GraphContract.expected_project_count -ne $ExpectedProjects.Count -or
    $ExpectedProjects.Count -eq 0 -or
    @($ExpectedProjects | Select-Object -Unique).Count -ne
        $ExpectedProjects.Count) {
    throw "[XCODE_VALIDATE] Graph contract project count/uniqueness is invalid."
}
$SortedExpectedProjects = @($ExpectedProjects | Sort-Object)
if ([string]::Join("`n", $ExpectedProjects) -cne
    [string]::Join("`n", $SortedExpectedProjects)) {
    throw "[XCODE_VALIDATE] Graph contract projects must be sorted."
}
foreach ($ProjectPath in $ExpectedProjects) {
    if ([IO.Path]::IsPathRooted($ProjectPath) -or
        $ProjectPath.Contains("\") -or
        $ProjectPath -match '(^|/)\.\.(/|$)' -or
        -not $ProjectPath.EndsWith(".xcodeproj")) {
        throw "[XCODE_VALIDATE] Invalid graph contract project path: $ProjectPath"
    }
}

$WorkspaceContract = [string]$GraphContract.workspace
if ([IO.Path]::IsPathRooted($WorkspaceContract) -or
    $WorkspaceContract.Contains("\") -or
    $WorkspaceContract -match '(^|/)\.\.(/|$)' -or
    -not $WorkspaceContract.EndsWith(".xcworkspacedata")) {
    throw "[XCODE_VALIDATE] Invalid graph contract workspace path: $WorkspaceContract"
}
$WorkspaceRelative = $WorkspaceContract.Replace(
    '/', [IO.Path]::DirectorySeparatorChar)
$Workspace = Join-Path $BuildDir $WorkspaceRelative
$ClientProject = Join-Path $BuildDir "HelloMine3D\HelloMine3D.xcodeproj\project.pbxproj"
$OgreProject = Join-Path $BuildDir "Engine\ogre3d\ogre3d.xcodeproj\project.pbxproj"
$GlSupportProject = Join-Path $BuildDir "Engine\ogre3d_glsupport\ogre3d_glsupport.xcodeproj\project.pbxproj"
$OisProject = Join-Path $BuildDir "External\ois\ois.xcodeproj\project.pbxproj"
$TracyProject = Join-Path $BuildDir "External\tracy\tracy.xcodeproj\project.pbxproj"
$NativeVerifier = Join-Path $RepoRoot "scripts\verify_xcode.sh"

$WorkspaceText = Get-Content -LiteralPath $Workspace -Raw -Encoding UTF8
$WorkspaceProjects = @(
    [regex]::Matches(
        $WorkspaceText,
        'location\s*=\s*"group:(?<path>[^"]+\.xcodeproj)"') |
        ForEach-Object { $_.Groups['path'].Value }
)
Assert-ExactInventory -Expected $ExpectedProjects `
    -Actual $WorkspaceProjects -Label "workspace project inventory"

$GeneratedProjects = @(Get-ChildItem -LiteralPath $BuildDir -Recurse `
    -Filter "project.pbxproj" | Select-Object -ExpandProperty FullName)
$GeneratedProjectPaths = @(
    foreach ($Project in $GeneratedProjects) {
        $ProjectDirectory = Split-Path -Parent $Project
        $Relative = $ProjectDirectory.Substring($BuildDir.Length).TrimStart(
            [char]'\', [char]'/')
        $Relative.Replace('\', '/')
    }
)
Assert-ExactInventory -Expected $ExpectedProjects `
    -Actual $GeneratedProjectPaths -Label "on-disk project inventory"

foreach ($ProjectPath in $ExpectedProjects) {
    Require-Text -Path $Workspace `
        -Pattern ([regex]::Escape("group:$ProjectPath")) `
        -Label "workspace project $ProjectPath"
}

foreach ($Framework in @("Cocoa", "Carbon", "IOKit", "Foundation", "AppKit", "CoreFoundation", "OpenGL")) {
    Require-Text -Path $ClientProject -Pattern ([regex]::Escape("-framework $Framework")) -Label "framework $Framework"
}

foreach ($Source in @("OgreOSXCocoaContext.mm", "OgreOSXCocoaView.mm", "OgreOSXCocoaWindow.mm", "OgreOSXGL3PlusSupport.mm")) {
    Require-Text -Path $GlSupportProject -Pattern ([regex]::Escape("$Source in Sources")) -Label "macOS GL support source $Source"
}

foreach ($Source in @("OISInputManager.mm", "CocoaInputManager.mm", "CocoaKeyboard.mm", "CocoaMouse.mm")) {
    Require-Text -Path $OisProject -Pattern ([regex]::Escape("$Source in Sources")) -Label "macOS OIS source $Source"
}

Require-Text -Path $TracyProject -Pattern ([regex]::Escape("TracyClient.cpp in Sources")) -Label "Tracy client source"
Require-Text -Path $ClientProject -Pattern ([regex]::Escape("../../src/external/tracy/public")) -Label "Tracy client header search path"
Require-Text -Path $ClientProject -Pattern ([regex]::Escape("libtracy.a in Frameworks")) -Label "Tracy client library dependency"
Reject-Text -Path $TracyProject -Pattern '\bTRACY_ENABLE\b' -Label "Tracy enable macro in default library generation"
Reject-Text -Path $ClientProject -Pattern '\bHELLOMINE3D_ENABLE_TRACY\b' -Label "Tracy enable macro in default client generation"

$RequiredSearchPaths = @(
    @($ClientProject, "../../src/Engine/ogre3d/include/OSX"),
    @($ClientProject, "../../src/Engine/ogre3d_glsupport/include/OSX"),
    @($ClientProject, "../../src/external/ois/includes/mac"),
    @($OgreProject, "../../../src/Engine/ogre3d/include/OSX"),
    @($GlSupportProject, "../../../src/Engine/ogre3d/include/OSX"),
    @($GlSupportProject, "../../../src/Engine/ogre3d_glsupport/include/OSX"),
    @($OisProject, "../../../src/external/ois/includes/mac")
)
foreach ($Entry in $RequiredSearchPaths) {
    Require-Text -Path $Entry[0] -Pattern ([regex]::Escape($Entry[1])) -Label "macOS header search path $($Entry[1])"
}

foreach ($Project in $GeneratedProjects) {
    $ProjectDirectory = Split-Path -Parent $Project
    $ProjectLabel = $ProjectDirectory.Substring($BuildDir.Length).TrimStart(
        [char]'\', [char]'/').Replace('\', '/')
    Validate-PbxGraph -Path $Project -Label $ProjectLabel
    Reject-Text -Path $Project -Pattern 'GCC_PREPROCESSOR_DEFINITIONS = \([^\)]*\bWIN32\b' -Label "WIN32 macro in macOS build settings"
    Reject-Text -Path $Project -Pattern '(Win32|WIN32|Linux|GLX|Android|Emscripten|iOS)[^\r\n]* in Sources' -Label "foreign-platform source in macOS build phase"
    Reject-Text -Path $Project -Pattern '(USER|SYSTEM)_HEADER_SEARCH_PATHS = \([^\)]*/(win32|linux|GLX)(/|,|\s)' -Label "foreign-platform header search path"
}

foreach ($Pattern in @(
    'uname -s',
    'xcodebuild',
    'validate_xcode_project_graph.py',
    '--self-test',
    '-parallelizeTargets',
    '-configuration "\$configuration"',
    'HelloMine3DCoordinateTests',
    'HelloMine3DWorldRuntimeSmoke',
    'HelloMine3DSoak',
    'HelloMine3DResourcePackSmoke',
    'HelloMine3DRecipeSmoke',
    'HelloMine3DWorldCatalogueSmoke',
    'HelloMine3DStorageTransactionSmoke',
    'HelloMine3DWorldBackupSmoke',
    'HelloMine3DOperationTimingSmoke',
    'HELLOMINE3D_VALIDATE_ONLY=1',
    'HELLOMINE3D_EXIT_AFTER_FRAMES=120',
    'HELLO_PERF_CAPTURE=1',
    'startup_first_usable_menu_ms=',
    'entry_first_controllable_ms=',
    'CODE_SIGNING_ALLOWED=NO',
    'Debug Release',
    '\[VALIDATION\] checks=346 failures=0',
    '\[WORLD_CATALOGUE_TEST\] checks=30 failures=0',
    '\[STORAGE_TRANSACTION_TEST\] checks=16 failures=0',
    '\[WORLD_BACKUP_TEST\] checks=19 failures=0',
    '\[OPERATION_TIMING_TEST\] checks=12 failures=0',
    '\[OGRE_VALIDATION\] renderer=OpenGL 3\+',
    '\[OGRE_TERRAIN\]',
    'duplicate Xcode project reference',
    'manual Xcode target ordering',
    'first-party compiler warning',
    '\[XCODE_GRAPH\] status=PASS',
    '\[XCODE_VERIFY\] status=PASS'
)) {
    Require-Text -Path $NativeVerifier -Pattern $Pattern -Label "native Xcode verifier contract"
}

Write-Host "[XCODE_VALIDATE] status=PASS checks=$Checks native_build=NOT_RUN"
Write-Host "[XCODE_VALIDATE] Native xcodebuild and launch still require macOS."
