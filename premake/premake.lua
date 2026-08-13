local project_name = "HelloMine3D"
local source_dir = "../src/HelloMine3D"
local external_source_dir = "../src/external"

newoption {
    trigger = "deps-prefix",
    value = "PATH",
    description = "Additional dependency prefix containing include/ and lib/"
}

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
    dependson {
        "imgui",
        "imgui_opengl3",
        "ogre3d",
        "ogre3d_glsupport",
        "ogre3d_gl3plus",
        "freeimage",
        "ogre_freetype",
        "zlib",
        "zzip",
        "ois"
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

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }
        includedirs {
            "../src/Engine/ogre3d_glsupport/include/win32",
            "../src/external/ois/includes/win32"
        }
        links {
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
    dependson {
        "freeimage",
        "libjpeg",
        "libopenjpeg",
        "libpng",
        "libraw",
        "libtiff4",
        "openexr",
        "zlib"
    }
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
    dependson {
        "freeimage",
        "libjpeg",
        "libopenjpeg",
        "libpng",
        "libraw",
        "libtiff4",
        "openexr",
        "zlib"
    }
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
