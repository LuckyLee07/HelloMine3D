local project_name = "HelloMine3D"
local source_dir = "../src/HelloMine3D"
local external_source_dir = "../src/external"

newoption {
    trigger = "deps-prefix",
    value = "PATH",
    description = "Additional dependency prefix containing include/ and lib/"
}

newoption {
    trigger = "with-tracy",
    description = "Enable Tracy profiler instrumentation"
}

local function is_truthy_env(name)
    local value = os.getenv(name)
    return value == "1" or value == "true" or value == "TRUE" or
           value == "on" or value == "ON"
end

local tracy_enabled = _OPTIONS["with-tracy"] ~= nil or
                      is_truthy_env("HELLOMINE3D_ENABLE_TRACY") or
                      is_truthy_env("TRACY_ENABLE")

local function first_non_empty(...)
    for i = 1, select("#", ...) do
        local value = select(i, ...)
        if value ~= nil and value ~= "" then
            return value
        end
    end
    return nil
end

local function add_dependency_prefix(prefix)
    if prefix == nil then
        return
    end

    local include_dir = path.join(prefix, "include")
    local lib_dir = path.join(prefix, "lib")

    if os.isdir(include_dir) then
        externalincludedirs { include_dir }
    end

    filter "configurations:Debug"
        if os.isdir(path.join(prefix, "debug", "lib")) then
            libdirs { path.join(prefix, "debug", "lib") }
        end
        if os.isdir(lib_dir) then
            libdirs { lib_dir }
        end

    filter "configurations:Release"
        if os.isdir(lib_dir) then
            libdirs { lib_dir }
        end

    filter {}
end

local function has_local_glm()
    return os.isfile("../src/external/glm/glm/glm.hpp")
end

local function project_source_patterns()
    local patterns = {
        source_dir .. "/*.h",
        source_dir .. "/*.cpp"
    }

    for _, dir in ipairs(os.matchdirs(source_dir .. "/*")) do
        if path.getname(dir) ~= "Tests" and path.getname(dir) ~= "Ogre" then
            table.insert(patterns, dir .. "/**.h")
            table.insert(patterns, dir .. "/**.cpp")
        end
    end

    return patterns
end

workspace(project_name)
    location "../build"
    startproject(project_name)
    configurations { "Debug", "Release" }
    platforms { "x64" }
    language "C++"
    cppdialect "C++17"
    warnings "Extra"
    multiprocessorcompile "On"
    staticruntime "On"

    filter "platforms:x64"
        architecture "x86_64"

    filter "configurations:Debug"
        symbols "On"
        editandcontinue "Off"

    filter "configurations:Release"
        optimize "Full"
        defines { "NDEBUG" }

    filter "system:windows"
        systemversion "10.0.22621.0"
        characterset "MBCS"

    filter {}

local premake_script_dir = path.getdirectory(_MAIN_SCRIPT or _SCRIPT or "premake/premake.lua")
dofile(path.join(premake_script_dir, "ogre.lua"))

group "External"

-- Tracy profiler client v0.13.1. The static library remains in the generated
-- graph for stable project topology, while instrumentation and networking are
-- compiled only when --with-tracy (or the matching environment flag) is set.
project "tracy"
    kind "StaticLib"
    location "../build/External/tracy"
    targetdir "../build/External/%{prj.name}/lib/%{cfg.platform}/%{cfg.buildcfg}"
    objdir "../build/External/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"
    warnings "Off"

    files {
        external_source_dir .. "/tracy/public/TracyClient.cpp",
        external_source_dir .. "/tracy/public/**.h",
        external_source_dir .. "/tracy/public/**.hpp",
        external_source_dir .. "/tracy/public/**.hmm"
    }

    externalincludedirs {
        external_source_dir .. "/tracy/public"
    }

    if tracy_enabled then
        defines {
            "TRACY_ENABLE",
            "TRACY_ON_DEMAND",
            "TRACY_ALLOW_SHADOW_WARNING"
        }
    end

    filter "system:not windows"
        buildoptions { "-pthread" }

    filter {}

project "imgui"
    kind "StaticLib"
    location "../build/External/imgui"
    targetdir "../build/External/%{prj.name}/lib/%{cfg.platform}/%{cfg.buildcfg}"
    objdir "../build/External/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"

    files {
        external_source_dir .. "/imgui/imgui.cpp",
        external_source_dir .. "/imgui/imgui_draw.cpp",
        external_source_dir .. "/imgui/imgui_tables.cpp",
        external_source_dir .. "/imgui/imgui_widgets.cpp",
        external_source_dir .. "/imgui/*.h"
    }

    externalincludedirs {
        external_source_dir .. "/imgui"
    }

project "imgui_opengl3"
    kind "StaticLib"
    location "../build/External/imgui_opengl3"
    targetdir "../build/External/%{prj.name}/lib/%{cfg.platform}/%{cfg.buildcfg}"
    objdir "../build/External/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"
    dependson {
        "imgui"
    }

    files {
        external_source_dir .. "/imgui/backends/imgui_impl_opengl3.cpp",
        external_source_dir .. "/imgui/backends/imgui_impl_opengl3.h",
        external_source_dir .. "/imgui/backends/imgui_impl_opengl3_loader.h"
    }

    externalincludedirs {
        external_source_dir .. "/imgui"
    }

group ""

local function configure_game_logic_target()
    kind "ConsoleApp"
    location "../build/%{prj.name}"
    targetdir "../bin"
    debugdir "../bin"
    objdir "../build/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"
    includedirs {
        source_dir
    }

    externalincludedirs {
        "../src/external",
        "../src/Engine/ThirdParty/freeimage/include"
    }

    if has_local_glm() then
        externalincludedirs { "../src/external/glm" }
    end

    defines {
        "GLM_ENABLE_EXPERIMENTAL"
    }

    add_dependency_prefix(first_non_empty(_OPTIONS["deps-prefix"], os.getenv("HELLOMINE3D_DEPS_PREFIX")))

    filter "system:macosx"
        add_dependency_prefix(os.getenv("HOMEBREW_PREFIX"))
        add_dependency_prefix("/opt/homebrew")
        add_dependency_prefix("/usr/local")

    filter {}

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }
        links { "dbghelp" }

    filter "system:macosx"
        linkoptions {
            "-framework Cocoa",
            "-framework Carbon",
            "-framework IOKit",
            "-framework Foundation",
            "-framework AppKit",
            "-framework CoreFoundation",
            "-framework OpenGL"
        }

    filter "system:linux"
        links {
            "GL",
            "pthread",
            "dl"
        }

    filter {}
end

project(project_name)
    configure_game_logic_target()
    files(project_source_patterns())
    files {
        source_dir .. "/Ogre/**.h",
        source_dir .. "/Ogre/**.cpp"
    }
    includedirs {
        source_dir,
        "../src/Engine/ogre3d/include",
        "../src/Engine/ogre3d_glsupport/include",
        "../src/Engine/ogre3d_glsupport/include/GLSL",
        "../src/Engine/ogre3d_gl3plus/include",
        "../src/Engine/ogre3d_gl3plus/include/GLSL",
        "../src/external/imgui",
        "../src/external/ois/includes"
    }

    links {
        "tracy",
        "imgui",
        "imgui_opengl3",
        "ogre3d_gl3plus",
        "ogre3d_glsupport",
        "ogre3d",
        "freeimage",
        "libjpeg",
        "libopenjpeg",
        "libpng",
        "libraw",
        "libtiff4",
        "openexr",
        "ogre_freetype",
        "zzip",
        "zlib",
        "ois"
    }

    defines {
        "OGRE_STATIC_LIB",
        "OGRE_BUILD_RENDERSYSTEM_GL3PLUS",
        "FREEIMAGE_LIB"
    }

    externalincludedirs {
        external_source_dir .. "/tracy/public"
    }

    if tracy_enabled then
        defines {
            "HELLOMINE3D_ENABLE_TRACY",
            "TRACY_ENABLE",
            "TRACY_ON_DEMAND",
            "TRACY_ALLOW_SHADOW_WARNING"
        }
    end

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }
        includedirs {
            "../src/Engine/ogre3d_glsupport/include/win32",
            "../src/external/ois/includes/win32"
        }
        links {
            "advapi32",
            "opengl32",
            "winmm",
            "gdi32",
            "user32",
            "shell32",
            "comctl32",
            "dinput8",
            "dxguid",
            "ws2_32"
        }

    filter "system:macosx"
        includedirs {
            "../src/Engine/ogre3d/include/OSX",
            "../src/Engine/ogre3d_glsupport/include/OSX",
            "../src/external/ois/includes/mac"
        }
        externalincludedirs {
            "../src/external/imgui"
        }
        linkoptions {
            "-framework Cocoa",
            "-framework Carbon",
            "-framework IOKit",
            "-framework Foundation",
            "-framework AppKit",
            "-framework CoreFoundation",
            "-framework OpenGL"
        }

    filter "system:linux"
        includedirs {
            "../src/Engine/ogre3d_glsupport/include/GLX",
            "../src/external/ois/includes/linux"
        }
        links {
            "GL",
            "X11",
            "pthread",
            "dl"
        }

    filter {}

-- Headless runtime validation over the real World/WorldManager/actor code.
-- Links the whole game runtime except the client entry point.
project "HelloMine3DWorldRuntimeSmoke"
    configure_game_logic_target()
    files(project_source_patterns())
    files { source_dir .. "/Tests/WorldRuntimeSmokeMain.cpp" }
    links {
        "freeimage",
        "libjpeg",
        "libopenjpeg",
        "libpng",
        "libraw",
        "libtiff4",
        "openexr",
        "zlib"
    }
    defines { "FREEIMAGE_LIB" }

-- Deterministic long-running R2 soak over the same headless world runtime.
project "HelloMine3DSoak"
    configure_game_logic_target()
    files(project_source_patterns())
    files { source_dir .. "/Tests/SoakMain.cpp" }
    links {
        "freeimage",
        "libjpeg",
        "libopenjpeg",
        "libpng",
        "libraw",
        "libtiff4",
        "openexr",
        "zlib"
    }
    defines { "FREEIMAGE_LIB" }

-- X1-X3 parser, precedence, frozen-view and effective-manifest fixtures.
project "HelloMine3DResourcePackSmoke"
    kind "ConsoleApp"
    location "../build/%{prj.name}"
    targetdir "../bin"
    debugdir "../bin"
    objdir "../build/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"
    files {
        source_dir .. "/Tests/ResourcePackSmokeMain.cpp",
        source_dir .. "/Util/ResourcePackResolver.h",
        source_dir .. "/Util/ResourcePackResolver.cpp"
    }
    includedirs { source_dir }

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }

    filter {}

-- G1 strict recipe parser, immutable registry and base-resource ownership.
project "HelloMine3DRecipeSmoke"
    kind "ConsoleApp"
    location "../build/%{prj.name}"
    targetdir "../bin"
    debugdir "../bin"
    objdir "../build/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"
    files {
        source_dir .. "/Tests/RecipeSmokeMain.cpp",
        source_dir .. "/Item/Material.h",
        source_dir .. "/Item/Material.cpp",
        source_dir .. "/Item/ItemStack.h",
        source_dir .. "/Item/ItemStack.cpp",
        source_dir .. "/Item/Inventory.h",
        source_dir .. "/Item/Inventory.cpp",
        source_dir .. "/Item/CraftingSession.h",
        source_dir .. "/Item/CraftingSession.cpp",
        source_dir .. "/Item/ToolRegistry.h",
        source_dir .. "/Item/ToolRegistry.cpp",
        source_dir .. "/Item/RecipeRegistry.h",
        source_dir .. "/Item/RecipeRegistry.cpp",
        source_dir .. "/Util/NonCopyable.h",
        source_dir .. "/Util/ResourcePackResolver.h",
        source_dir .. "/Util/ResourcePackResolver.cpp",
        source_dir .. "/Util/ResourcePaths.h",
        source_dir .. "/World/Block/BlockId.h"
    }
    includedirs { source_dir }

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }

    filter {}

project "HelloMine3DCoordinateTests"
    kind "ConsoleApp"
    location "../build/%{prj.name}"
    targetdir "../bin"
    debugdir "../bin"
    objdir "../build/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"

    files {
        source_dir .. "/Tests/CoordinateTestMain.cpp",
        source_dir .. "/World/WorldCoordinateTests.h",
        source_dir .. "/World/WorldCoordinateTests.cpp",
        source_dir .. "/World/WorldCoordinates.h",
        source_dir .. "/World/WorldCoordinates.cpp",
        source_dir .. "/World/WorldConstants.h",
        source_dir .. "/Maths/Vector2XZ.h",
        source_dir .. "/Maths/Vector2XZ.cpp"
    }

    includedirs {
        source_dir
    }

    externalincludedirs {
        "../src/external"
    }

-- K1 strict, read-only world catalogue and versioned metadata fixtures.
project "HelloMine3DWorldCatalogueSmoke"
    kind "ConsoleApp"
    location "../build/%{prj.name}"
    targetdir "../bin"
    debugdir "../bin"
    objdir "../build/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"

    files {
        source_dir .. "/Tests/WorldCatalogueSmokeMain.cpp",
        source_dir .. "/Diagnostics/OperationPerformanceTiming.h",
        source_dir .. "/Diagnostics/OperationPerformanceTiming.cpp",
        source_dir .. "/Sandbox/GameApplicationFlow.h",
        source_dir .. "/Sandbox/GameApplicationFlow.cpp",
        source_dir .. "/World/Block/BlockEntity.h",
        source_dir .. "/World/Block/BlockEntity.cpp",
        source_dir .. "/World/Storage/StorageTransaction.h",
        source_dir .. "/World/Storage/StorageTransaction.cpp",
        source_dir .. "/World/Storage/ChunkStorageData.h",
        source_dir .. "/World/Storage/ChunkStorageData.cpp",
        source_dir .. "/World/Storage/WorldBackup.h",
        source_dir .. "/World/Storage/WorldBackup.cpp",
        source_dir .. "/World/Storage/WorldCatalogue.h",
        source_dir .. "/World/Storage/WorldCatalogue.cpp",
        source_dir .. "/World/Storage/WorldManagementService.h",
        source_dir .. "/World/Storage/WorldManagementService.cpp",
        source_dir .. "/World/Storage/WorldSave.h",
        source_dir .. "/World/Storage/WorldSave.cpp"
    }

    includedirs {
        source_dir
    }

    externalincludedirs {
        "../src/external"
    }

    if has_local_glm() then
        externalincludedirs { "../src/external/glm" }
    end

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }

    filter {}

-- K2 transactional world/chunk publication and deterministic fault matrix.
project "HelloMine3DStorageTransactionSmoke"
    kind "ConsoleApp"
    location "../build/%{prj.name}"
    targetdir "../bin"
    debugdir "../bin"
    objdir "../build/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"

    files {
        source_dir .. "/Tests/StorageTransactionSmokeMain.cpp",
        source_dir .. "/Diagnostics/OperationPerformanceTiming.h",
        source_dir .. "/Diagnostics/OperationPerformanceTiming.cpp",
        source_dir .. "/World/Block/BlockEntity.h",
        source_dir .. "/World/Block/BlockEntity.cpp",
        source_dir .. "/World/Storage/StorageTransaction.h",
        source_dir .. "/World/Storage/StorageTransaction.cpp",
        source_dir .. "/World/Storage/ChunkStorageData.h",
        source_dir .. "/World/Storage/ChunkStorageData.cpp",
        source_dir .. "/World/Storage/WorldCatalogue.h",
        source_dir .. "/World/Storage/WorldCatalogue.cpp",
        source_dir .. "/World/Storage/WorldSave.h",
        source_dir .. "/World/Storage/WorldSave.cpp"
    }

    includedirs {
        source_dir
    }

    externalincludedirs {
        "../src/external"
    }

    if has_local_glm() then
        externalincludedirs { "../src/external/glm" }
    end

    add_dependency_prefix(first_non_empty(_OPTIONS["deps-prefix"], os.getenv("HELLOMINE3D_DEPS_PREFIX")))

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }

    filter {}

-- K3 bounded backups, corruption quarantine and verified restore.
project "HelloMine3DWorldBackupSmoke"
    kind "ConsoleApp"
    location "../build/%{prj.name}"
    targetdir "../bin"
    debugdir "../bin"
    objdir "../build/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"

    files {
        source_dir .. "/Tests/WorldBackupSmokeMain.cpp",
        source_dir .. "/Diagnostics/OperationPerformanceTiming.h",
        source_dir .. "/Diagnostics/OperationPerformanceTiming.cpp",
        source_dir .. "/World/Block/BlockEntity.h",
        source_dir .. "/World/Block/BlockEntity.cpp",
        source_dir .. "/World/Storage/StorageTransaction.h",
        source_dir .. "/World/Storage/StorageTransaction.cpp",
        source_dir .. "/World/Storage/ChunkStorageData.h",
        source_dir .. "/World/Storage/ChunkStorageData.cpp",
        source_dir .. "/World/Storage/WorldCatalogue.h",
        source_dir .. "/World/Storage/WorldCatalogue.cpp",
        source_dir .. "/World/Storage/WorldSave.h",
        source_dir .. "/World/Storage/WorldSave.cpp",
        source_dir .. "/World/Storage/WorldBackup.h",
        source_dir .. "/World/Storage/WorldBackup.cpp"
    }

    includedirs {
        source_dir
    }

    externalincludedirs {
        "../src/external"
    }

    if has_local_glm() then
        externalincludedirs { "../src/external/glm" }
    end

    add_dependency_prefix(first_non_empty(_OPTIONS["deps-prefix"], os.getenv("HELLOMINE3D_DEPS_PREFIX")))

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }

    filter {}

-- Q2 bounded operation phases, counters and disabled-capture behavior.
project "HelloMine3DOperationTimingSmoke"
    kind "ConsoleApp"
    location "../build/%{prj.name}"
    targetdir "../bin"
    debugdir "../bin"
    objdir "../build/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"

    files {
        source_dir .. "/Tests/OperationTimingSmokeMain.cpp",
        source_dir .. "/Diagnostics/OperationPerformanceTiming.h",
        source_dir .. "/Diagnostics/OperationPerformanceTiming.cpp"
    }

    includedirs {
        source_dir
    }

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }

    filter {}

-- H1 portable path/trigger policy plus the selected Windows DbgHelp backend.
project "HelloMine3DCrashDiagnosticsSmoke"
    kind "ConsoleApp"
    location "../build/%{prj.name}"
    targetdir "../bin"
    debugdir "../bin"
    objdir "../build/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"

    files {
        source_dir .. "/Tests/CrashDiagnosticsSmokeMain.cpp",
        source_dir .. "/Diagnostics/CrashDiagnostics.h",
        source_dir .. "/Diagnostics/CrashDiagnostics.cpp",
        source_dir .. "/Diagnostics/CrashDiagnosticsPlatform.h",
        source_dir .. "/Diagnostics/CrashDiagnosticsPlatformStub.cpp",
        source_dir .. "/Diagnostics/WindowsCrashDiagnostics.cpp"
    }

    includedirs {
        source_dir
    }

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }
        links { "dbghelp" }

    filter {}

project "HelloMine3DSaveLoadSmoke"
    kind "ConsoleApp"
    location "../build/%{prj.name}"
    targetdir "../bin"
    debugdir "../bin"
    objdir "../build/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"

    files {
        source_dir .. "/Tests/SaveLoadSmokeMain.cpp",
        source_dir .. "/Util/ResourcePaths.h",
        source_dir .. "/World/WorldConstants.h",
        source_dir .. "/World/Block/BlockId.h",
        source_dir .. "/World/Block/BlockEntity.h",
        source_dir .. "/World/Block/BlockEntity.cpp",
        source_dir .. "/World/Storage/StorageTransaction.h",
        source_dir .. "/World/Storage/StorageTransaction.cpp",
        source_dir .. "/World/Storage/ChunkStorageData.h",
        source_dir .. "/World/Storage/ChunkStorageData.cpp"
    }

    includedirs {
        source_dir
    }

    externalincludedirs {
        "../src/external"
    }

    if has_local_glm() then
        externalincludedirs { "../src/external/glm" }
    end

    add_dependency_prefix(first_non_empty(_OPTIONS["deps-prefix"], os.getenv("HELLOMINE3D_DEPS_PREFIX")))

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }

    filter "system:linux"
        links {
            "pthread",
            "dl"
        }

    filter {}

project "HelloMine3DMeshDirtyTests"
    kind "ConsoleApp"
    location "../build/%{prj.name}"
    targetdir "../bin"
    debugdir "../bin"
    objdir "../build/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"

    files {
        source_dir .. "/Tests/MeshDirtyTestMain.cpp",
        source_dir .. "/World/Chunk/ChunkUpdatePlanner.h",
        source_dir .. "/World/Chunk/ChunkUpdatePlanner.cpp",
        source_dir .. "/World/WorldCoordinates.h",
        source_dir .. "/World/WorldCoordinates.cpp",
        source_dir .. "/World/WorldConstants.h",
        source_dir .. "/Maths/Vector2XZ.h",
        source_dir .. "/Maths/Vector2XZ.cpp"
    }

    includedirs {
        source_dir
    }

    externalincludedirs {
        "../src/external"
    }

project "HelloMine3DEntityLifecycleSmoke"
    kind "ConsoleApp"
    location "../build/%{prj.name}"
    targetdir "../bin"
    debugdir "../bin"
    objdir "../build/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"

    files {
        source_dir .. "/Tests/EntityLifecycleSmokeMain.cpp",
        source_dir .. "/Actor/ActorTypes.h",
        source_dir .. "/Actor/Actor.h",
        source_dir .. "/Actor/Actor.cpp",
        source_dir .. "/Actor/ActorManager.h",
        source_dir .. "/Actor/ActorManager.cpp",
        source_dir .. "/Actor/LivingActor.h",
        source_dir .. "/Actor/LivingActor.cpp",
        source_dir .. "/Actor/MobActor.h",
        source_dir .. "/Actor/MobActor.cpp",
        source_dir .. "/Entity/Entity.h",
        source_dir .. "/Physics/AABB.h",
        source_dir .. "/Sandbox/Events/SandboxEventBus.h",
        source_dir .. "/Sandbox/Events/SandboxEventBus.cpp",
        source_dir .. "/Sandbox/Events/EntityEvents.h",
        source_dir .. "/Maths/glm.h"
    }

    includedirs {
        source_dir
    }

    externalincludedirs {
        "../src/external"
    }

    if has_local_glm() then
        externalincludedirs { "../src/external/glm" }
    end
