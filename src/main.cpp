#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

#include "mailbox/mailbox.hpp"
#include "render/rendertarget.hpp"
#include "render/rt_interaction.hpp"
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

    RenderTexture2D tex_render =
        LoadRenderTexture(wcfg.render_width, wcfg.screen_height);

    RenderTexture2D tex_interaction =
        LoadRenderTexture(wcfg.render_width, wcfg.screen_height);

    sim.begin();

    while (!WindowShouldClose()) {
        // ---- Acquire draw once for the whole frame ----
        auto view = sim.begin_read_draw();

        // Compute interpolation parameters once, mirroring the old logic
        const bool canInterp = rcfg.interpolate && view.t0 > 0 && view.t1 > 0 &&
                               view.t1 > view.t0 && view.prev && view.curr &&
                               view.prev->size() == view.curr->size() &&
                               !view.curr->empty();

        float interp_alpha = 1.0f;
        if (canInterp) {
            const long long now_ns =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count();
            const long long target_ns =
                now_ns - (long long)(rcfg.interp_delay_ms * 1'000'000.0f);

            if (target_ns <= view.t0)
                interp_alpha = 0.0f;
            else if (target_ns >= view.t1)
                interp_alpha = 1.0f;
            else
                interp_alpha =
                    float(target_ns - view.t0) / float(view.t1 - view.t0);
        }

        // ---- Render to color RT ----
        BeginTextureMode(tex_render);
        render_tex(sim, rcfg, view, canInterp, interp_alpha);
        EndTextureMode();

        // ---- Render interaction overlay RT (selection box) ----
        BeginTextureMode(tex_interaction);
        draw_selection_overlay();
        EndTextureMode();

        // ---- Present ----
        BeginDrawing();
        ClearBackground(BLACK);

        {
            if (rcfg.final_additive_blit)
                BeginBlendMode(BLEND_ADDITIVE);
            DrawTextureRec(tex_render.texture,
                           (Rectangle){0, 0, (float)tex_render.texture.width,
                                       (float)-tex_render.texture.height},
                           (Vector2){0, 0}, WHITE);
            if (rcfg.final_additive_blit)
                EndBlendMode();
        }

        DrawTextureRec(tex_interaction.texture,
                       (Rectangle){0, 0, (float)tex_render.texture.width,
                                   (float)-tex_render.texture.height},
                       (Vector2){0, 0}, WHITE);

        rlImGuiBegin();
        render_ui(wcfg, sim, rcfg);
        update_selection_from_mouse();
        // Pass the view + interpolation to the inspector so it can count
        // particles
        DrawRegionInspector(tex_render, sim.get_world(), view, canInterp,
                            interp_alpha);
        rlImGuiEnd();

        // controls...
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

        // ---- Release draw view once per frame ----
        sim.end_read_draw(view);
    }

    sim.end();

    rlImGuiShutdown();
    UnloadRenderTexture(tex_render);
    UnloadRenderTexture(tex_interaction);
    CloseWindow();
}

int main() {
    run();
    return 0;
}