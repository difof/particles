--[[ TODO:
    link these:
        - curl
        - mbedtls
        - zlib
        - ffmpeg
]]

local function addImGUI()
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

local function addRaylib()
    includedirs { "extlib/raylib/src" }
    libdirs { "extlib/raylib/src" }
    links { "raylib" }
    dependson { "extlib_raylib" }

    filter "system:macosx"
        linkoptions { "-framework CoreVideo", "-framework IOKit", "-framework Cocoa", "-framework GLUT", "-framework OpenGL" }
    filter {}
end

local function addTinydir()
    includedirs { "extlib/tinydir" }
end

local function addFmt()
    includedirs { "extlib/fmt/include" }
    libdirs { "extlib/fmt/build" }
    links { "fmt" }
    dependson { "extlib_fmt" }
end

local function addJSON()
    includedirs { "extlib/nlohmann-json/single_include" }
end

local function applyOSAndArchDefines()
    filter "system:windows"
        defines { "PLATFORM_WINDOWS" }
        removefiles { "src/**.c" }
        links { "ws2_32", "winmm" }
        removebuildoptions { "-Wno-deprecated-declarations", "-Wno-c++11-narrowing" }
        buildoptions { "/W3" }

    filter "system:macosx"
        links { "m", "dl", "pthread" }
        defines { "PLATFORM_MACOS" }

    filter "system:linux"
        links { "m", "dl", "pthread" }
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
    
    filter {}
end

local function applyOutDir(build) 
    targetdir ("build/bin/" .. build)
    objdir ("build/obj/" .. build)
end

local function applyBaseConfig() 
    kind "ConsoleApp"
    language "C++"

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

    applyOSAndArchDefines()
end

local function unitTest(name, extraIncludes, extraFiles)
    project(name)
        applyBaseConfig()
        
        files {
            "extlib/catch2/extras/catch_amalgamated.cpp",
            "tests/"..name..".cpp" 
        }

        includedirs { 
            "src",
            "extlib/catch2/extras"
        }
        
        if extraFiles then
            for _, file in ipairs(extraFiles) do
                files { file }
            end
        end
        
        if extraIncludes then
            for _, include in ipairs(extraIncludes) do
                includedirs { include }
            end
        end
        
        applyOSAndArchDefines()

        defines { "DEBUG" }
        symbols "On"
        applyOutDir("debug")
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
        maxosx_deployment_target .. "make -C ../extlib/fmt/build fmt -j4",
    }
    cleancommands { maxosx_deployment_target .. "make -C ../extlib/fmt/build clean" }

project "extlib_raylib"
    kind "Makefile"
    buildcommands { maxosx_deployment_target .. "make -C ../extlib/raylib/src -j4" }
    cleancommands { maxosx_deployment_target .. "make -C ../extlib/raylib/src clean" }

project "particles"
    projectBase()

    addFmt()
    addTinydir()
    addRaylib()
    addImGUI()
    addJSON()

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
        optimize "Off"
        buildoptions { "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"}
        linkoptions { "-fsanitize=address,undefined" }
        applyOutDir("debug")
        
    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
        buildoptions { "-O3", "-ffast-math", "-fno-math-errno", "-fno-trapping-math" }
        applyOutDir("release")

unitTest("test_uniformgrid")
unitTest("test_world", { "extlib/raylib/src" }, { "src/simulation/world.cpp" })
unitTest("test_multicore", { "extlib/raylib/src" }, { "src/simulation/multicore.cpp" })
unitTest("test_mailboxes", { "extlib/raylib/src" }, { "src/mailbox/render/drawbuffer.cpp" })
unitTest("test_save_manager", { "extlib/raylib/src", "extlib/nlohmann-json/single_include" }, { "src/save_manager.cpp", "src/simulation/world.cpp" })
unitTest("test_simulation", { "extlib/raylib/src" }, { "src/simulation/simulation.cpp", "src/simulation/world.cpp", "src/simulation/multicore.cpp", "src/mailbox/render/drawbuffer.cpp" })
unitTest("test_undo_manager", { "extlib/imgui", "extlib/rlimgui", "extlib/raylib/src" }, { "src/undo/add_group_action.cpp", "src/undo/remove_group_action.cpp", "src/undo/resize_group_action.cpp", "src/undo/clear_all_groups_action.cpp" })
unitTest("test_file_dialog", { "extlib/imgui", "extlib/rlimgui", "extlib/raylib/src", "extlib/tinydir" }, { "src/render/file_dialog.cpp", "extlib/imgui/imgui.cpp", "extlib/imgui/imgui_draw.cpp", "extlib/imgui/imgui_widgets.cpp", "extlib/imgui/imgui_tables.cpp", "extlib/imgui/misc/cpp/imgui_stdlib.cpp" })
