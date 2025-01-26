--[[ TODO:
    link these:
        - curl
        - mbedtls
        - zlib
        - ffmpeg
]]

local function linkImGUI()
    includedirs {
        "extlib/imgui",
        "extlib/rlimgui",
    }

    files {
        "extlib/imgui/imgui.cpp",
        "extlib/imgui/imgui_draw.cpp",
        "extlib/imgui/imgui_widgets.cpp",
        "extlib/imgui/imgui_tables.cpp",
        "extlib/imgui/imgui_demo.cpp",
        "extlib/imgui/misc/cpp/imgui_stdlib.cpp",
        "extlib/rlimgui/rlImGui.cpp",
    }
end

local function linkRaylib()
    includedirs { "extlib/raylib/src" }
    libdirs { "extlib/raylib/src" }
    links { "raylib" }
    dependson { "extlib_raylib" }
end

local function linkTinydir()
    includedirs { "extlib/tinydir" }
end

local function linkFmt()
    includedirs { "extlib/fmt/include" }
    libdirs { "extlib/fmt/build" }
    links { "fmt" }
    dependson { "extlib_fmt" }
end

local function linkJSON()
    includedirs { "extlib/nlohmann-json/single_include" }
end

local function raylibDeps()
    filter "system:macosx"
        linkoptions { "-framework CoreVideo", "-framework IOKit", "-framework Cocoa", "-framework GLUT", "-framework OpenGL" }
end

local function applyOSAndArchDefines()
    filter "system:windows"
        defines { "PLATFORM_WINDOWS" }
        removefiles { "src/**.c" }
        links { "ws2_32", "winmm" }
        removebuildoptions { "-Wno-deprecated-declarations", "-Wno-c++11-narrowing" }
        buildoptions { "/W3" }

    filter "system:macosx"
        defines { "PLATFORM_MACOS" }

    filter "system:linux"
        defines { "PLATFORM_LINUX" }

    filter "architecture:x64"
        defines { "ARCH_X64" }
        filter "system:windows"
            defines { "USE_X86_SSE" }
        filter "system:macosx"
            defines { "USE_X86_SSE" }
        filter "system:linux"
            defines { "USE_X86_SSE" }

    filter "architecture:ARM64"
        defines { "ARCH_ARM64" }
        filter "system:macosx"
            defines { "__ARM_NEON" }
        filter "system:linux"
            defines { "__ARM_NEON" }
end

local function applyBaseConfig() 
    kind "ConsoleApp"
    language "C++"
    targetdir "build/bin/%{cfg.buildcfg}"
    objdir "build/obj/%{cfg.buildcfg}"

    buildoptions { 
        "-std=c++20",
        "-Wno-deprecated-declarations",
        "-Wno-c++11-narrowing",
    }
end

local function projectBase()
    applyBaseConfig()

    files { 
        "src/**.h", "src/**.c",
        "src/**.hpp", "src/**.cpp",
    }
    includedirs { 
        "src",
        "extlib/imgui",
        "extlib/rlimgui", 
        "extlib/raylib/src",
        "extlib/tinydir",
        "extlib/fmt/include",
        "extlib/nlohmann-json/single_include"
    }
    links { "m", "dl", "pthread" }
    dependson { }

    applyOSAndArchDefines()
end

local function unitTest(name, extraIncludes, extraFiles)
    project(name)
        applyBaseConfig()
        
        files {
            "extlib/catch2/extras/catch_amalgamated.cpp",
            "tests/"..name..".cpp" 
        }
        
        if extraFiles then
            for _, file in ipairs(extraFiles) do
                files { file }
            end
        end

        includedirs { 
            "src",
            "extlib/catch2/extras"
        }
        
        if extraIncludes then
            for _, include in ipairs(extraIncludes) do
                includedirs { include }
            end
        end
        
        links { "m", "dl", "pthread" }
        dependson { }

        applyOSAndArchDefines()

        defines { "DEBUG" }
        symbols "On"
end

local maxosx_deployment_target = "export MACOSX_DEPLOYMENT_TARGET=10.15; "

workspace "particles"
    location "build"
    configurations { "Debug", "Release" }

project "extlib_fmt"
    kind "Makefile"
    buildcommands {
        "mkdir -p ../extlib/fmt/build",
        maxosx_deployment_target .. "cd ../extlib/fmt/build && " .. "cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE",
        maxosx_deployment_target .. "make -C ../extlib/fmt/build fmt -j8",
    }
    cleancommands { maxosx_deployment_target .. "make -C ../extlib/fmt/build clean" }

project "extlib_raylib"
    kind "Makefile"
    buildcommands { maxosx_deployment_target .. "make -C ../extlib/raylib/src -j8" }
    cleancommands { maxosx_deployment_target .. "make -C ../extlib/raylib/src clean" }

project "particles"
    linkFmt()
    linkTinydir()
    linkRaylib()
    linkImGUI()
    linkJSON()
    raylibDeps()
    
    projectBase()

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
        optimize "Off"
        buildoptions { "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"}
        linkoptions { "-fsanitize=address,undefined" }

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
        buildoptions { "-O3", "-ffast-math", "-fno-math-errno", "-fno-trapping-math" }

unitTest "test_uniformgrid"
unitTest("test_world", { "extlib/raylib/src" }, { "src/simulation/world.cpp" })
unitTest("test_multicore", { "extlib/raylib/src" }, { "src/simulation/multicore.cpp" })
unitTest("test_mailboxes", { "extlib/raylib/src" })
unitTest("test_json_manager", { "extlib/raylib/src", "extlib/nlohmann-json/single_include" }, { "src/json_manager.cpp", "src/simulation/world.cpp" })
unitTest("test_simulation", { "extlib/raylib/src" }, { "src/simulation/simulation.cpp", "src/simulation/world.cpp", "src/simulation/multicore.cpp" })
