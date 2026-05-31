local project_name = "MineCraft3D"
local source_dir = "../src/HelloMine3D"

newoption {
    trigger = "vcpkg-root",
    value = "PATH",
    description = "Path to a vcpkg checkout"
}

newoption {
    trigger = "vcpkg-triplet",
    value = "TRIPLET",
    description = "vcpkg target triplet, for example arm64-osx, x64-osx, x64-windows"
}

newoption {
    trigger = "deps-prefix",
    value = "PATH",
    description = "Additional dependency prefix containing include/ and lib/"
}

local function first_non_empty(...)
    for _, value in ipairs({ ... }) do
        if value ~= nil and value ~= "" then
            return value
        end
    end
    return nil
end

local function default_vcpkg_triplet()
    local host = os.host()
    if host == "macosx" then
        return "arm64-osx"
    elseif host == "windows" then
        return "x64-windows"
    end
    return "x64-linux"
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

    if os.isdir(lib_dir) then
        libdirs { lib_dir }
    end
end

local function add_vcpkg_paths()
    local vcpkg_root = first_non_empty(_OPTIONS["vcpkg-root"], os.getenv("VCPKG_ROOT"))
    if vcpkg_root == nil then
        return
    end

    local triplet = first_non_empty(_OPTIONS["vcpkg-triplet"], os.getenv("VCPKG_TARGET_TRIPLET"),
        default_vcpkg_triplet())
    local installed_dir = path.join(vcpkg_root, "installed", triplet)

    externalincludedirs { path.join(installed_dir, "include") }

    filter "configurations:Debug"
        libdirs {
            path.join(installed_dir, "debug", "lib"),
            path.join(installed_dir, "lib")
        }

    filter "configurations:Release"
        libdirs { path.join(installed_dir, "lib") }

    filter {}
end

local function has_local_sfml()
    return os.isdir("../src/external/sfml/include") and os.isdir("../src/external/sfml/lib")
end

local function has_local_imgui()
    return os.isfile("../src/external/imgui/imgui.cpp")
end

local function has_local_glm()
    return os.isfile("../src/external/glm/glm/glm.hpp")
end

local function has_local_sfml_bin()
    return os.isdir("../src/external/sfml/bin")
end

local sfml_links = {
    "sfml-system",
    "sfml-audio",
    "sfml-network",
    "sfml-graphics",
    "sfml-window"
}

local sfml_debug_links = {
    "sfml-system-d",
    "sfml-audio-d",
    "sfml-network-d",
    "sfml-graphics-d",
    "sfml-window-d"
}

local function add_sfml_links()
    if has_local_sfml() then
        filter { "system:windows", "configurations:Debug" }
            links(sfml_debug_links)

        filter { "system:windows", "configurations:Release" }
            links(sfml_links)

        filter "system:not windows"
            links(sfml_links)

        filter {}
    else
        links(sfml_links)
    end
end

workspace(project_name)
    location "../build"
    startproject(project_name)
    configurations { "Debug", "Release" }
    language "C++"
    cppdialect "C++23"
    warnings "Extra"

    filter "configurations:Debug"
        symbols "On"

    filter "configurations:Release"
        optimize "Full"
        defines { "NDEBUG" }

    filter "system:windows"
        systemversion "latest"

    filter {}

project(project_name)
    kind "ConsoleApp"
    targetdir "../bin"
    debugdir "../bin"
    objdir "../build/obj/%{prj.name}/%{cfg.buildcfg}"

    files {
        source_dir .. "/**.h",
        source_dir .. "/**.cpp",
        "../src/external/glad/glad.c",
        "../src/external/glad/**.h",
        "../src/external/imgui/imgui.cpp",
        "../src/external/imgui/imgui_draw.cpp",
        "../src/external/imgui/imgui_tables.cpp",
        "../src/external/imgui/imgui_widgets.cpp",
        "../src/external/imgui_sfml/imgui-SFML.cpp",
        "../src/external/imgui_sfml/imgui_impl_opengl3.cpp",
        "../src/external/imgui_sfml/**.h",
        "../media/**",
        "../bin/config.txt",
        "../bin/info.txt"
    }

    includedirs {
        source_dir
    }

    externalincludedirs {
        "../src/external",
        "../src/external/glad",
        "../src/external/imgui",
        "../src/external/imgui_sfml"
    }

    if has_local_sfml() then
        externalincludedirs { "../src/external/sfml/include" }
        libdirs { "../src/external/sfml/lib" }
    end

    if has_local_glm() then
        externalincludedirs { "../src/external/glm" }
    end

    if not has_local_imgui() then
        removefiles {
            "../src/external/imgui/imgui.cpp",
            "../src/external/imgui/imgui_draw.cpp",
            "../src/external/imgui/imgui_tables.cpp",
            "../src/external/imgui/imgui_widgets.cpp"
        }
    end

    defines {
        "GLM_ENABLE_EXPERIMENTAL"
    }

    add_vcpkg_paths()
    add_dependency_prefix(first_non_empty(_OPTIONS["deps-prefix"], os.getenv("MINECRAFT3D_DEPS_PREFIX")))

    filter "system:macosx"
        add_dependency_prefix(os.getenv("HOMEBREW_PREFIX"))
        add_dependency_prefix("/opt/homebrew")
        add_dependency_prefix("/usr/local")

    filter {}

    add_sfml_links()

    if not has_local_imgui() then
        links { "imgui" }
    end

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }
        links { "opengl32" }

        if has_local_sfml_bin() then
            postbuildcommands {
                '{COPYDIR} "' .. path.getabsolute("../src/external/sfml/bin") .. '" "%{cfg.targetdir}"'
            }
        end

    filter "system:macosx"
        linkoptions {
            "-Wl,-rpath,@executable_path/../src/external/sfml/lib",
            "-Wl,-rpath," .. path.getabsolute("../src/external/sfml/lib"),
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
