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
    int screenW = GetMonitorWidth(monitor);
    int screenH = GetMonitorHeight(monitor) - 60;
    int panelW = 500;
    int texW = screenW;

    WindowConfig wcfg = {screenW, screenH, panelW, texW};
    RenderConfig rcfg;
    rcfg.interpolate = true;
    rcfg.core_size = 1.5f;
    rcfg.glow_enabled = true;
    rcfg.outer_scale_mul = 24.f;
    rcfg.outer_rgb_gain = .78f;
    rcfg.inner_scale_mul = 1.f;
    rcfg.inner_rgb_gain = .52f;

    mailbox::SimulationConfig::Snapshot scfg = {};
    scfg.bounds_width = (float)wcfg.render_width;
    scfg.bounds_height = (float)wcfg.screen_height;
    scfg.target_tps = 0;
    scfg.time_scale = 1.0f;
    scfg.viscosity = 0.271f;
    scfg.wallRepel = 86.0f;
    scfg.wallStrength = 0.129f;
    scfg.sim_threads = -1;
    Simulation sim(scfg);

    SetWindowSize(wcfg.screen_width, wcfg.screen_height);
    SetWindowPosition(0, 0);
    SetWindowMonitor(1);
    SetTargetFPS(60);
    rlImGuiSetup(true);

    RenderTexture2D tex =
        LoadRenderTexture(wcfg.render_width, wcfg.screen_height);

    sim.begin();

    while (!WindowShouldClose()) {
        BeginTextureMode(tex);
        render_tex(sim, rcfg);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);

        {
            if (rcfg.final_additive_blit) {
                BeginBlendMode(BLEND_ADDITIVE);
            }
            DrawTextureRec(tex.texture,
                           (Rectangle){0, 0, (float)tex.texture.width,
                                       (float)-tex.texture.height},
                           (Vector2){0, 0}, WHITE);
            if (rcfg.final_additive_blit) {
                EndBlendMode();
            }
        }

        rlImGuiBegin();
        render_ui(wcfg, sim, rcfg);
        rlImGuiEnd();

        if (IsKeyPressed(KEY_R)) {
            sim.push_command({mailbox::command::Command::Kind::ResetWorld});
        }

        if (IsKeyPressed(KEY_U)) {
            rcfg.show_ui = !rcfg.show_ui;
        }

        if (IsKeyPressed(KEY_S) ||
            (IsKeyPressedRepeat(KEY_S) &&
             sim.get_run_state() == Simulation::RunState::Paused)) {
            sim.push_command({mailbox::command::Command::Kind::OneStep});
        }

        if (IsKeyPressed(KEY_SPACE)) {
            if (sim.get_run_state() == Simulation::RunState::Running) {
                sim.push_command({mailbox::command::Command::Kind::Pause});
            } else if (sim.get_run_state() == Simulation::RunState::Paused) {
                sim.push_command({mailbox::command::Command::Kind::Resume});
            }
        }

        EndDrawing();
    }

    sim.end();

    rlImGuiShutdown();
    UnloadRenderTexture(tex);
    CloseWindow();
}

int main() {
    run();
    return 0;
}