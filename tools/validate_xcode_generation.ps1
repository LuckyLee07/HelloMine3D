param()

$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptRoot "..")).Path
$Premake = Join-Path $RepoRoot "tools\premake\premake5.exe"
$PremakeDir = Join-Path $RepoRoot "premake"
$BuildDir = Join-Path $RepoRoot "build"

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

$Workspace = Join-Path $BuildDir "HelloMine3D.xcworkspace\contents.xcworkspacedata"
$ClientProject = Join-Path $BuildDir "HelloMine3D\HelloMine3D.xcodeproj\project.pbxproj"
$OgreProject = Join-Path $BuildDir "Engine\ogre3d\ogre3d.xcodeproj\project.pbxproj"
$GlSupportProject = Join-Path $BuildDir "Engine\ogre3d_glsupport\ogre3d_glsupport.xcodeproj\project.pbxproj"
$OisProject = Join-Path $BuildDir "External\ois\ois.xcodeproj\project.pbxproj"
$NativeVerifier = Join-Path $RepoRoot "scripts\verify_xcode.sh"

foreach ($Target in @(
    "HelloMine3D",
    "HelloMine3DCoordinateTests",
    "HelloMine3DMeshDirtyTests",
    "HelloMine3DSaveLoadSmoke",
    "HelloMine3DEntityLifecycleSmoke",
    "HelloMine3DWorldRuntimeSmoke",
    "zlib",
    "zzip",
    "ogre_freetype",
    "libjpeg",
    "libopenjpeg",
    "libpng",
    "libraw",
    "libtiff4",
    "openexr",
    "freeimage",
    "ogre3d",
    "ogre3d_glsupport",
    "ogre3d_gl3plus",
    "ois",
    "imgui",
    "imgui_opengl3"
)) {
    Require-Text -Path $Workspace -Pattern ([regex]::Escape("$Target.xcodeproj")) -Label "workspace target $Target"
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

foreach ($Project in @(Get-ChildItem -LiteralPath $BuildDir -Recurse -Filter "project.pbxproj" | Select-Object -ExpandProperty FullName)) {
    Reject-Text -Path $Project -Pattern 'GCC_PREPROCESSOR_DEFINITIONS = \([^\)]*\bWIN32\b' -Label "WIN32 macro in macOS build settings"
    Reject-Text -Path $Project -Pattern '(Win32|WIN32|Linux|GLX|Android|Emscripten|iOS)[^\r\n]* in Sources' -Label "foreign-platform source in macOS build phase"
    Reject-Text -Path $Project -Pattern '(USER|SYSTEM)_HEADER_SEARCH_PATHS = \([^\)]*/(win32|linux|GLX)(/|,|\s)' -Label "foreign-platform header search path"
}

foreach ($Pattern in @(
    'uname -s',
    'xcodebuild',
    '-configuration "\$configuration"',
    'HelloMine3DCoordinateTests',
    'HelloMine3DWorldRuntimeSmoke',
    'HELLOMINE3D_VALIDATE_ONLY=1',
    'HELLOMINE3D_EXIT_AFTER_FRAMES=3',
    'CODE_SIGNING_ALLOWED=NO',
    'Debug Release',
    '\[VALIDATION\] checks=239 failures=0',
    '\[OGRE_VALIDATION\] renderer=OpenGL 3\+',
    '\[OGRE_TERRAIN\]',
    '\[XCODE_VERIFY\] status=PASS'
)) {
    Require-Text -Path $NativeVerifier -Pattern $Pattern -Label "native Xcode verifier contract"
}

Write-Host "[XCODE_VALIDATE] status=PASS checks=$Checks native_build=NOT_RUN"
Write-Host "[XCODE_VALIDATE] Native xcodebuild and launch still require macOS."
