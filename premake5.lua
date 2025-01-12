--[[ TODO:
    link these:
        - chipmunk2d
        - curl
        - mbedtls
        - zlib
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

local function raylibDeps()
    filter "system:macosx"
        linkoptions { "-framework CoreVideo", "-framework IOKit", "-framework Cocoa", "-framework GLUT", "-framework OpenGL" }
end

local function projectBase()
    kind "ConsoleApp"
    language "C++"
    targetdir "build/bin/%{cfg.buildcfg}"
    objdir "build/obj/%{cfg.buildcfg}"

    buildoptions { 
        "-std=c++20",
        "-Wno-deprecated-declarations",
        "-Wno-c++11-narrowing",
    }
    files { 
        "src/**.h", "src/**.c",
        "src/**.hpp", "src/**.cpp",
    }
    includedirs { "src" }
    links { "m", "dl", "pthread" }
    dependson { }
end

local function unitTest(name)
    project(name)
        projectBase()

        files {
            "extlib/catch2/extras/catch_amalgamated.cpp",
            "tests/"..name..".cpp" 
        }
        excludes { "src/main.cpp" }
        includedirs { "extlib/catch2/extras" }
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
    projectBase()

    linkFmt()
    linkTinydir()
    linkRaylib()
    linkImGUI()
    raylibDeps()

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

unitTest "test_script"
