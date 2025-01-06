#include <raylib.h>
#include <imgui.h>
#include <rlImGui.h>

#include "math.hpp"
#include "multicore.hpp"
#include "uniformgrid.hpp"
#include "world.hpp"
#include "types.hpp"
#include "ui.hpp"
#include "simulation.hpp"
#include "renderer.hpp"

void run()
{
    InitWindow(1080, 800, "Particles");
    int monitor = GetCurrentMonitor();
    int screenW = GetMonitorWidth(monitor);
    int screenH = GetMonitorHeight(monitor) - 50;
    int panelW = 400; // screenW * .23;
    int texW = screenW - panelW;
    WindowConfig wcfg = {screenW, screenH, panelW, texW};

    SimConfig scfg = {};
    {
        scfg.bounds_width = (float)wcfg.render_width;
        scfg.bounds_height = (float)wcfg.screen_height;

        scfg.target_tps.store(0, std::memory_order_relaxed);
        scfg.interpolate.store(false, std::memory_order_relaxed);
        scfg.interp_delay_ms.store(16.0f, std::memory_order_relaxed);

        scfg.time_scale.store(1.0f, std::memory_order_relaxed);
        scfg.viscosity.store(0.1f, std::memory_order_relaxed);
        scfg.gravity.store(0.0f, std::memory_order_relaxed);
        scfg.wallRepel.store(40.0f, std::memory_order_relaxed);
        scfg.wallStrength.store(0.1f, std::memory_order_relaxed);
        scfg.pulse.store(0.0f, std::memory_order_relaxed);
        scfg.pulse_x.store(scfg.bounds_width * 0.5f, std::memory_order_relaxed);
        scfg.pulse_y.store(scfg.bounds_height * 0.5f, std::memory_order_relaxed);

        scfg.sim_threads.store(-1, std::memory_order_relaxed); // Auto
        scfg.reset_requested.store(true);
    }

    DrawBuffers dbuf;
    World world;

    // InitWindow(wcfg.screen_width, wcfg.screen_height, "Particles");
    SetWindowSize(wcfg.screen_width, wcfg.screen_height);
    SetWindowPosition(0, 0);
    SetWindowMonitor(GetCurrentMonitor());
    SetTargetFPS(60);
    rlImGuiSetup(true);

    RenderTexture2D tex = LoadRenderTexture(wcfg.render_width, wcfg.screen_height);

    std::thread sim_thread(simulation_thread_func, std::ref(world), std::ref(scfg), std::ref(dbuf));

    while (!WindowShouldClose())
    {
        BeginTextureMode(tex);
        render_tex(world, dbuf, scfg);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);

        // render texture must be y-flipped due to default OpenGL coordinates (left-bottom)
        DrawTextureRec(tex.texture, (Rectangle){0, 0, (float)tex.texture.width, (float)-tex.texture.height}, (Vector2){wcfg.panel_width, 0}, WHITE);

        rlImGuiBegin();
        render_ui(wcfg, world, scfg);
        rlImGuiEnd();

        EndDrawing();
    }

    scfg.sim_running.store(false, std::memory_order_relaxed);
    sim_thread.join();

    rlImGuiShutdown();
    UnloadRenderTexture(tex);
    CloseWindow();
}

int main()
{
    run();
    return 0;
}