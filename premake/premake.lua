local project_name = "HelloMine3D"
local source_dir = "../src/HelloMine3D"
local external_source_dir = "../src/external"
local sfml_source_dir = "../src/external/sfml"
local freetype_source_dir = "../src/external/freetype"
local sfml_build_dir = "../build/external/sfml"
local sfml_install_dir = "../build/external/sfml/install"

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

local function quote(value)
    return '"' .. value .. '"'
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
        table.insert(patterns, dir .. "/**.h")
        table.insert(patterns, dir .. "/**.cpp")
    end

    return patterns
end

local function cmake_command()
    local vs_cmake = "C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    if os.isfile(vs_cmake) then
        return quote(vs_cmake)
    end

    return "cmake"
end

local function sfml_configure_command()
    local command = cmake_command() ..
        " -S " .. quote(path.getabsolute(sfml_source_dir)) ..
        " -B " .. quote(path.getabsolute(sfml_build_dir)) ..
        " -DBUILD_SHARED_LIBS=ON" ..
        " -DSFML_BUILD_AUDIO=OFF" ..
        " -DSFML_BUILD_NETWORK=OFF" ..
        " -DSFML_BUILD_EXAMPLES=OFF" ..
        " -DSFML_BUILD_TEST_SUITE=OFF" ..
        " -DSFML_BUILD_DOC=OFF" ..
        " -DSFML_USE_SYSTEM_DEPS=OFF" ..
        " -DSTB_INCLUDE_DIR=" .. quote(path.getabsolute(path.join(sfml_source_dir, "extlibs/headers/stb_image"))) ..
        " -DFETCHCONTENT_SOURCE_DIR_FREETYPE=" .. quote(path.getabsolute(freetype_source_dir)) ..
        " -DCMAKE_INSTALL_PREFIX=" .. quote(path.getabsolute(sfml_install_dir))

    if os.host() == "windows" then
        command = command .. ' -G "Visual Studio 17 2022" -A x64'
    end

    return command
end

local function sfml_build_command()
    local command = cmake_command() ..
        " --build " .. quote(path.getabsolute(sfml_build_dir)) ..
        " --config %{cfg.buildcfg}" ..
        " --target install"

    if os.host() == "windows" then
        command = command .. " -- /m"
    end

    return command
end

workspace(project_name)
    location "../build"
    startproject(project_name)
    configurations { "Debug", "Release" }
    platforms { "x64" }
    language "C++"
    cppdialect "C++17"
    warnings "Extra"

    filter "platforms:x64"
        architecture "x86_64"

    filter "configurations:Debug"
        symbols "On"

    filter "configurations:Release"
        optimize "Full"
        defines { "NDEBUG" }

    filter "system:windows"
        systemversion "latest"

    filter {}

group "External"

project "sfml"
    kind "Makefile"
    location "../build/External/sfml"

    files {
        sfml_source_dir .. "/include/**.hpp",
        sfml_source_dir .. "/include/**.inl",
        sfml_source_dir .. "/src/**.hpp",
        sfml_source_dir .. "/src/**.cpp",
        sfml_source_dir .. "/src/**.h",
        sfml_source_dir .. "/cmake/**",
        freetype_source_dir .. "/include/**.h",
        freetype_source_dir .. "/src/**.c",
        freetype_source_dir .. "/src/**.h"
    }

    buildcommands {
        sfml_configure_command(),
        sfml_build_command()
    }

    rebuildcommands {
        sfml_configure_command(),
        sfml_build_command()
    }

    cleancommands {
        cmake_command() .. " --build " .. quote(path.getabsolute(sfml_build_dir)) ..
            " --config %{cfg.buildcfg} --target clean"
    }

project "glad"
    kind "StaticLib"
    location "../build/External/glad"
    targetdir "../build/External/%{prj.name}/lib/%{cfg.platform}/%{cfg.buildcfg}"
    objdir "../build/External/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"

    files {
        external_source_dir .. "/glad/glad.c",
        external_source_dir .. "/glad/**.h"
    }

    externalincludedirs {
        external_source_dir .. "/glad"
    }

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

project "imgui_sfml"
    kind "StaticLib"
    location "../build/External/imgui_sfml"
    targetdir "../build/External/%{prj.name}/lib/%{cfg.platform}/%{cfg.buildcfg}"
    objdir "../build/External/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"
    dependson {
        "glad",
        "imgui"
    }

    files {
        external_source_dir .. "/imgui_sfml/imgui-SFML.cpp",
        external_source_dir .. "/imgui_sfml/imgui_impl_opengl3.cpp",
        external_source_dir .. "/imgui_sfml/**.h"
    }

    externalincludedirs {
        external_source_dir .. "/imgui",
        external_source_dir .. "/imgui_sfml",
        external_source_dir .. "/sfml/include"
    }

group ""

project(project_name)
    kind "ConsoleApp"
    location "../build/%{prj.name}"
    targetdir "../bin"
    debugdir "../bin"
    objdir "../build/%{prj.name}/obj/%{cfg.platform}/%{cfg.buildcfg}"
    dependson {
        "sfml",
        "glad",
        "imgui",
        "imgui_sfml"
    }

    files(project_source_patterns())

    includedirs {
        source_dir
    }

    externalincludedirs {
        "../src/external",
        "../src/external/glad",
        "../src/external/imgui",
        "../src/external/imgui_sfml",
        "../src/external/sfml/include"
    }

    libdirs {
        path.join(sfml_install_dir, "lib")
    }

    if has_local_glm() then
        externalincludedirs { "../src/external/glm" }
    end

    defines {
        "GLM_ENABLE_EXPERIMENTAL"
    }

    links {
        "glad",
        "imgui",
        "imgui_sfml"
    }

    add_dependency_prefix(first_non_empty(_OPTIONS["deps-prefix"], os.getenv("HELLOMINE3D_DEPS_PREFIX")))

    filter "system:macosx"
        add_dependency_prefix(os.getenv("HOMEBREW_PREFIX"))
        add_dependency_prefix("/opt/homebrew")
        add_dependency_prefix("/usr/local")

    filter {}

    filter "configurations:Debug"
        links {
            "sfml-graphics-d",
            "sfml-window-d",
            "sfml-system-d"
        }

    filter "configurations:Release"
        links {
            "sfml-graphics",
            "sfml-window",
            "sfml-system"
        }

    filter {}

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }
        links {
            "opengl32",
            "winmm",
            "gdi32"
        }
        postbuildcommands {
            '{COPYDIR} "' .. path.getabsolute(path.join(sfml_install_dir, "bin")) .. '" "%{cfg.targetdir}"'
        }

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
