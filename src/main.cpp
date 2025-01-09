#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

#include "mailbox/mailbox.hpp"
#include "render/rendertarget.hpp"
#include "render/ui.hpp"
#include "simulation/simulation.hpp"
#include "simulation/world.hpp"
#include "types.hpp"

void run() {
    InitWindow(1080, 800, "Particles");

    int monitor = GetCurrentMonitor();
    int screenW = GetMonitorWidth(monitor) * .9;
    int screenH = GetMonitorHeight(monitor) - 60;
    int panelW = 400; // screenW * .23;
    int texW = screenW - panelW;

    WindowConfig wcfg = {screenW, screenH, panelW, texW};
    RenderConfig rcfg;
    World world;
    mailbox::DrawBuffer dbuf;
    mailbox::SimulationStats statsb;
    mailbox::command::Queue cmdq;
    mailbox::SimulationConfig scfgb;
    mailbox::SimulationConfig::Snapshot scfg = {};
    scfg.bounds_width = (float)wcfg.render_width;
    scfg.bounds_height = (float)wcfg.screen_height;
    scfg.target_tps = 0;
    scfg.time_scale = 1.0f;
    scfg.viscosity = 0.1f;
    scfg.wallRepel = 40.0f;
    scfg.wallStrength = 0.1f;
    scfg.sim_threads = 1;
    scfgb.publish(scfg);

    // InitWindow(wcfg.screen_width, wcfg.screen_height, "Particles");
    SetWindowSize(wcfg.screen_width, wcfg.screen_height);
    SetWindowPosition(0, 0);
    SetWindowMonitor(1);
    SetTargetFPS(60);
    rlImGuiSetup(true);

    RenderTexture2D tex =
        LoadRenderTexture(wcfg.render_width, wcfg.screen_height);

    std::thread sim_thread(simulation_thread_func, std::ref(world),
                           std::ref(scfgb), std::ref(dbuf), std::ref(cmdq),
                           std::ref(statsb));

    while (!WindowShouldClose()) {
        BeginTextureMode(tex);
        render_tex(world, dbuf, rcfg);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);

        {
            if (rcfg.final_additive_blit)
                BeginBlendMode(BLEND_ADDITIVE);
            DrawTextureRec(tex.texture,
                           (Rectangle){0, 0, (float)tex.texture.width,
                                       (float)-tex.texture.height},
                           (Vector2){wcfg.panel_width, 0}, WHITE);
            if (rcfg.final_additive_blit)
                EndBlendMode();
        }

        rlImGuiBegin();
        render_ui(wcfg, world, scfgb, statsb, cmdq, rcfg);
        rlImGuiEnd();

        EndDrawing();
    }

    cmdq.push({mailbox::command::Command::Kind::Quit});
    sim_thread.join();

    rlImGuiShutdown();
    UnloadRenderTexture(tex);
    CloseWindow();
}

int main() {
    run();
    return 0;
}