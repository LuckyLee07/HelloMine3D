-- Ogre 1.10 and its minimum dependency closure used during the E1-E5
-- renderer migration. The game does not link these libraries until E2; E1
-- keeps the imported engine graph isolated from first-party game targets.

local engine_dir = "../src/Engine"
local third_party_dir = engine_dir .. "/ThirdParty"
local ois_dir = "../src/external/ois"

local function configure_static_project(name, project_location)
    project(name)
        kind "StaticLib"
        location(project_location)
        targetdir(project_location .. "/lib/%{cfg.platform}/%{cfg.buildcfg}")
        objdir(project_location .. "/obj/%{cfg.platform}/%{cfg.buildcfg}")
end

local function suppress_msvc_warnings(codes)
    local options = {}
    for _, code in ipairs(codes) do
        table.insert(options, "/wd" .. code)
    end

    filter "system:windows"
        buildoptions(options)
    filter {}
end

group "Engine/ThirdParty"

configure_static_project("zlib", "../build/Engine/ThirdParty/zlib")
    includedirs { third_party_dir .. "/zlib/include" }
    files {
        third_party_dir .. "/zlib/include/**.h",
        third_party_dir .. "/zlib/src/**.c"
    }
    suppress_msvc_warnings { "4131", "4996", "4244", "4127" }
    filter "system:windows"
        defines { "WIN32" }
    filter "system:not windows"
        defines { "HAVE_UNISTD_H" }
    filter {}

configure_static_project("zzip", "../build/Engine/ThirdParty/zzip")
    dependson { "zlib" }
    includedirs {
        third_party_dir .. "/zlib/include",
        third_party_dir .. "/zzip/include"
    }
    files {
        third_party_dir .. "/zzip/include/**.h",
        third_party_dir .. "/zzip/src/**.c"
    }
    suppress_msvc_warnings { "4127", "4996", "4706", "4244", "4267", "4028", "4305" }
    filter "system:windows"
        defines { "WIN32", "_CRT_SECURE_NO_WARNINGS" }
    filter {}

configure_static_project("ogre_freetype", "../build/Engine/ThirdParty/freetype")
    dependson { "zlib" }
    includedirs { third_party_dir .. "/freetype/include" }
    files {
        third_party_dir .. "/freetype/include/**.h",
        third_party_dir .. "/freetype/src/**.c"
    }
    removefiles {
        third_party_dir .. "/freetype/src/tools/**",
        third_party_dir .. "/freetype/src/gzip/**",
        third_party_dir .. "/freetype/src/ftgzip.c",
        third_party_dir .. "/freetype/src/gxvalid/gxvfgen.c",
        third_party_dir .. "/freetype/src/autofit/aflatin2.c"
    }
    defines { "FT2_BUILD_LIBRARY" }
    suppress_msvc_warnings { "4100", "4244", "4245", "4701", "4267", "4324", "4306", "4703" }
    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }
        buildoptions { "/FI\"ft2build.h\"" }
        removefiles { third_party_dir .. "/freetype/src/base/ftmac.c" }
    filter "system:not windows"
        includedirs { third_party_dir .. "/zlib/include" }
    filter {}

configure_static_project("libjpeg", "../build/Engine/ThirdParty/libjpeg")
    includedirs { third_party_dir .. "/libjpeg/include" }
    files {
        third_party_dir .. "/libjpeg/include/**.h",
        third_party_dir .. "/libjpeg/src/**.c"
    }
    suppress_msvc_warnings { "4100", "4244", "4127", "4267" }
    filter "system:windows"
        defines { "WIN32", "_CRT_SECURE_NO_WARNINGS" }
    filter {}

configure_static_project("libopenjpeg", "../build/Engine/ThirdParty/libopenjpeg")
    includedirs { third_party_dir .. "/libopenjpeg/include" }
    files {
        third_party_dir .. "/libopenjpeg/include/**.h",
        third_party_dir .. "/libopenjpeg/src/**.c"
    }
    defines { "OPJ_STATIC" }
    suppress_msvc_warnings { "4100", "4244", "4127", "4267", "4701", "4706" }
    filter "system:windows"
        defines { "WIN32", "_CRT_SECURE_NO_WARNINGS" }
    filter {}

configure_static_project("libpng", "../build/Engine/ThirdParty/libpng")
    dependson { "zlib" }
    includedirs {
        third_party_dir .. "/libpng/include",
        third_party_dir .. "/zlib/include"
    }
    files {
        third_party_dir .. "/libpng/include/**.h",
        third_party_dir .. "/libpng/src/**.c"
    }
    suppress_msvc_warnings { "4127" }
    filter "system:windows"
        defines { "WIN32", "_CRT_SECURE_NO_WARNINGS" }
    filter {}

configure_static_project("libraw", "../build/Engine/ThirdParty/libraw")
    includedirs { third_party_dir .. "/libraw/include" }
    files {
        third_party_dir .. "/libraw/include/**.h",
        third_party_dir .. "/libraw/src/**.c",
        third_party_dir .. "/libraw/src/**.cpp"
    }
    removefiles { third_party_dir .. "/libraw/src/**dcb_demosaicing.c" }
    defines { "LIBRAW_NODLL" }
    suppress_msvc_warnings {
        "4244", "4189", "4101", "4706", "4100", "4018", "4305",
        "4309", "4127", "4389", "4804", "4146", "4245", "4996",
        "4702", "4267", "4701", "4456"
    }
    filter "system:windows"
        defines { "WIN32", "_CRT_SECURE_NO_WARNINGS" }
    filter "system:not windows"
        buildoptions { "-Wno-c++11-narrowing" }
    filter {}

configure_static_project("libtiff4", "../build/Engine/ThirdParty/libtiff4")
    dependson { "libjpeg", "zlib" }
    includedirs {
        third_party_dir .. "/libtiff4/include",
        third_party_dir .. "/libjpeg/include",
        third_party_dir .. "/zlib/include"
    }
    files {
        third_party_dir .. "/libtiff4/include/**.h",
        third_party_dir .. "/libtiff4/src/**.c"
    }
    suppress_msvc_warnings {
        "4127", "4244", "4706", "4702", "4701", "4018", "4306",
        "4305", "4267", "4324", "4703", "4100", "4456"
    }
    filter "system:windows"
        defines { "WIN32", "_CRT_SECURE_NO_WARNINGS" }
    filter "system:not windows"
        removefiles { third_party_dir .. "/libtiff4/src/tif_win32.c" }
    filter {}

configure_static_project("openexr", "../build/Engine/ThirdParty/openexr")
    dependson { "zlib" }
    includedirs {
        third_party_dir .. "/openexr/include",
        third_party_dir .. "/openexr/include/half",
        third_party_dir .. "/openexr/include/iex",
        third_party_dir .. "/openexr/include/ilmimf",
        third_party_dir .. "/openexr/include/ilmthread",
        third_party_dir .. "/openexr/include/imath",
        third_party_dir .. "/zlib/include"
    }
    files {
        third_party_dir .. "/openexr/include/**.h",
        third_party_dir .. "/openexr/src/**.cpp"
    }
    suppress_msvc_warnings {
        "4244", "4305", "4100", "4127", "4245", "4512", "4706",
        "4267", "4702", "4101", "4800", "4018", "4701", "4389",
        "4334", "4722"
    }
    filter "system:windows"
        defines { "WIN32", "_CRT_SECURE_NO_WARNINGS" }
        linkoptions { "/ignore:4221" }
    filter {}

configure_static_project("freeimage", "../build/Engine/ThirdParty/freeimage")
    dependson {
        "libjpeg",
        "libopenjpeg",
        "libpng",
        "libraw",
        "libtiff4",
        "openexr",
        "zlib"
    }
    includedirs {
        third_party_dir .. "/freeimage/include",
        third_party_dir .. "/libjpeg/include",
        third_party_dir .. "/libopenjpeg/include",
        third_party_dir .. "/libpng/include",
        third_party_dir .. "/libraw/include",
        third_party_dir .. "/libtiff4/include",
        third_party_dir .. "/openexr/include",
        third_party_dir .. "/openexr/include/half",
        third_party_dir .. "/openexr/include/iex",
        third_party_dir .. "/openexr/include/ilmimf",
        third_party_dir .. "/openexr/include/imath",
        third_party_dir .. "/openexr/include/ilmthread",
        third_party_dir .. "/zlib/include"
    }
    files {
        third_party_dir .. "/freeimage/include/**.h",
        third_party_dir .. "/freeimage/src/**.cpp"
    }
    defines {
        "FREEIMAGE_LIB",
        "OPJ_STATIC",
        "LIBRAW_NODLL"
    }
    suppress_msvc_warnings {
        "4100", "4127", "4189", "4244", "4611", "4389", "4324",
        "4702", "4701", "4789", "4456"
    }
    filter "system:windows"
        defines {
            "WIN32",
            "_CRT_SECURE_NO_WARNINGS",
            "_HAS_AUTO_PTR_ETC=1"
        }
    filter {}

group "Engine"

configure_static_project("ogre3d", "../build/Engine/ogre3d")
    dependson { "freeimage", "ogre_freetype", "zlib", "zzip" }
    pchheader "OgreStableHeaders.h"
    pchsource(engine_dir .. "/ogre3d/src/OgrePrecompiledHeaders.cpp")
    includedirs {
        engine_dir .. "/ogre3d/include",
        engine_dir .. "/ogre3d/src/nedmalloc",
        third_party_dir .. "/zlib/include",
        third_party_dir .. "/zzip/include",
        third_party_dir .. "/freeimage/include",
        third_party_dir .. "/freetype/include"
    }
    files {
        engine_dir .. "/ogre3d/include/**.h",
        engine_dir .. "/ogre3d/src/**.mm",
        engine_dir .. "/ogre3d/src/**.cpp",
        engine_dir .. "/ogre3d/resources/**.rc",
        engine_dir .. "/ogre3d/resources/**.ico",
        engine_dir .. "/ogre3d/resources/**.bmp"
    }
    removefiles { engine_dir .. "/ogre3d/src/Threading/OgreDefaultWorkQueueTBB.cpp" }
    defines {
        "OGRE_NONCLIENT_BUILD",
        "FREEIMAGE_LIB"
    }
    suppress_msvc_warnings {
        "4100", "4127", "4193", "4244", "4305", "4512", "4706",
        "4702", "4245", "4503", "4146", "4565", "4267", "4996",
        "4005", "4345"
    }
    filter "system:windows"
        defines { "WIN32", "_CRT_SECURE_NO_WARNINGS" }
        buildoptions { "/bigobj", "/Zm198" }
        removefiles {
            engine_dir .. "/ogre3d/src/OSX/**",
            engine_dir .. "/ogre3d/src/Android/**",
            engine_dir .. "/ogre3d/src/Emscripten/**",
            engine_dir .. "/ogre3d/src/GLX/**",
            engine_dir .. "/ogre3d/src/iOS/**",
            engine_dir .. "/ogre3d/src/WIN32/OgreMinGWSupport.cpp",
            engine_dir .. "/ogre3d/src/Threading/Ogre*PThreads.cpp",
            engine_dir .. "/ogre3d/src/OgreFileSystemLayerNoOp.cpp",
            engine_dir .. "/ogre3d/src/OgrePOSIXTimer.cpp",
            engine_dir .. "/ogre3d/src/OgreSearchOps.cpp",
            engine_dir .. "/ogre3d/src/OgreConfigDialogNoOp.cpp",
            engine_dir .. "/ogre3d/src/OgreErrorDialogNoOp.cpp"
        }
    filter "system:macosx"
        pchheader ""
        pchsource ""
        includedirs { engine_dir .. "/ogre3d/include/OSX" }
        removefiles {
            engine_dir .. "/ogre3d/src/WIN32/**",
            engine_dir .. "/ogre3d/src/Android/**",
            engine_dir .. "/ogre3d/src/Emscripten/**",
            engine_dir .. "/ogre3d/src/GLX/**",
            engine_dir .. "/ogre3d/src/iOS/**",
            engine_dir .. "/ogre3d/src/Threading/*Win.cpp",
            engine_dir .. "/ogre3d/src/OgreConfigDialogNoOp.cpp",
            engine_dir .. "/ogre3d/src/OgreErrorDialogNoOp.cpp"
        }
    filter "system:linux"
        pchheader ""
        pchsource ""
        removefiles {
            engine_dir .. "/ogre3d/src/WIN32/**",
            engine_dir .. "/ogre3d/src/OSX/**"
        }
    filter {}

configure_static_project("ogre3d_glsupport", "../build/Engine/ogre3d_glsupport")
    dependson { "ogre3d" }
    includedirs {
        engine_dir .. "/ogre3d/include",
        engine_dir .. "/ogre3d_glsupport/include",
        engine_dir .. "/ogre3d_glsupport/include/GLSL"
    }
    files {
        engine_dir .. "/ogre3d_glsupport/include/**.h",
        engine_dir .. "/ogre3d_glsupport/src/**.mm",
        engine_dir .. "/ogre3d_glsupport/src/**.cpp"
    }
    filter "system:windows"
        includedirs { engine_dir .. "/ogre3d_glsupport/include/win32" }
        removefiles {
            engine_dir .. "/ogre3d_glsupport/src/OSX/**",
            engine_dir .. "/ogre3d_glsupport/src/GLX/**",
            engine_dir .. "/ogre3d_glsupport/src/EGL/**"
        }
    filter "system:macosx"
        includedirs {
            engine_dir .. "/ogre3d/include/OSX",
            engine_dir .. "/ogre3d_glsupport/include/OSX"
        }
        removefiles {
            engine_dir .. "/ogre3d_glsupport/src/win32/**",
            engine_dir .. "/ogre3d_glsupport/src/GLX/**",
            engine_dir .. "/ogre3d_glsupport/src/EGL/**",
            engine_dir .. "/ogre3d_glsupport/src/OSX/OgreOSXRenderTexture.cpp"
        }
    filter "system:linux"
        includedirs { engine_dir .. "/ogre3d_glsupport/include/GLX" }
        removefiles {
            engine_dir .. "/ogre3d_glsupport/src/OSX/**",
            engine_dir .. "/ogre3d_glsupport/src/win32/**",
            engine_dir .. "/ogre3d_glsupport/src/EGL/**"
        }
    filter {}

configure_static_project("ogre3d_gl3plus", "../build/Engine/ogre3d_gl3plus")
    dependson { "ogre3d", "ogre3d_glsupport" }
    includedirs {
        engine_dir .. "/ogre3d/include",
        engine_dir .. "/ogre3d/include/Threading",
        engine_dir .. "/ogre3d_glsupport/include",
        engine_dir .. "/ogre3d_glsupport/include/GLSL",
        engine_dir .. "/ogre3d_gl3plus/include",
        engine_dir .. "/ogre3d_gl3plus/include/GLSL"
    }
    files {
        engine_dir .. "/ogre3d_gl3plus/include/**.h",
        engine_dir .. "/ogre3d_gl3plus/src/**.c",
        engine_dir .. "/ogre3d_gl3plus/src/**.cpp"
    }

group "External"

configure_static_project("ois", "../build/External/ois")
    includedirs { ois_dir .. "/includes" }
    files {
        ois_dir .. "/includes/**.h",
        ois_dir .. "/src/**.cpp",
        ois_dir .. "/src/**.mm"
    }
    suppress_msvc_warnings { "4512", "4100", "4189" }
    filter "system:windows"
        includedirs { ois_dir .. "/includes/win32" }
        removefiles {
            ois_dir .. "/src/mac/**",
            ois_dir .. "/src/linux/**",
            ois_dir .. "/src/iphone/**",
            ois_dir .. "/src/SDL/**",
            ois_dir .. "/src/extras/**",
            ois_dir .. "/src/win32/extras/**"
        }
    filter "system:macosx"
        includedirs { ois_dir .. "/includes/mac" }
        removefiles {
            ois_dir .. "/src/win32/**",
            ois_dir .. "/src/linux/**",
            ois_dir .. "/src/iphone/**",
            ois_dir .. "/src/SDL/**",
            ois_dir .. "/src/extras/**",
            ois_dir .. "/src/OISInputManager.cpp"
        }
    filter "system:linux"
        includedirs { ois_dir .. "/includes/linux" }
        removefiles {
            ois_dir .. "/src/win32/**",
            ois_dir .. "/src/mac/**",
            ois_dir .. "/src/iphone/**",
            ois_dir .. "/src/SDL/**"
        }
    filter {}

group ""
