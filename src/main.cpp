#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

#include "mailbox/mailbox.hpp"
#include "render/control_ui.hpp"
#include "render/manager.hpp"
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
    scfg.wall_repel = 86.0f;
    scfg.wall_strength = 0.129f;
    scfg.sim_threads = -1;
    Simulation sim(scfg);

    SetWindowSize(wcfg.screen_width, wcfg.screen_height);
    SetWindowPosition(0, 0);
    SetWindowMonitor(1);
    SetTargetFPS(60);
    rlImGuiSetup(true);

    RenderManager rman({wcfg.screen_width, wcfg.screen_height, wcfg.panel_width,
                        wcfg.render_width});

    sim.begin();
    // send initial seed from main
    {
        auto seed = std::make_shared<mailbox::command::SeedSpec>();
        const int groups = 5;
        seed->sizes = std::vector<int>(groups, 1500);
        seed->colors = {(Color){0, 228, 114, 255}, (Color){238, 70, 82, 255},
                        (Color){227, 172, 72, 255}, (Color){0, 121, 241, 255},
                        (Color){200, 122, 255, 255}};
        seed->r2 = {80.f * 80.f, 80.f * 80.f, 96.6f * 96.6f, 80.f * 80.f,
                    80.f * 80.f};
        seed->rules = {
            // row 0
            +0.926f,
            -0.834f,
            +0.281f,
            -0.06427308f,
            +0.51738745f,
            // row 1
            -0.46170965f,
            +0.49142435f,
            +0.2760726f,
            +0.6413487f,
            -0.7276546f,
            // row 2
            -0.78747644f,
            +0.23373386f,
            -0.024112331f,
            -0.74875921f,
            +0.22836663f,
            // row 3
            +0.56558144f,
            +0.94846946f,
            -0.36052886f,
            +0.44114092f,
            -0.31766385f,
            // row 4
            std::sin(1.0f),
            std::cos(2.0f),
            +1.0f,
            -1.0f,
            +3.14f,
        };
        sim.push_command(mailbox::command::SeedWorld{seed});
    }

    while (!WindowShouldClose()) {
        if (rman.draw_frame(sim, rcfg))
            break;

        if (IsKeyPressed(KEY_R)) {
            sim.push_command(mailbox::command::ResetWorld{});
        }
        if (IsKeyPressed(KEY_U)) {
            rcfg.show_ui = !rcfg.show_ui;
        }
        if (IsKeyPressed(KEY_S) ||
            (IsKeyPressedRepeat(KEY_S) &&
             sim.get_run_state() == Simulation::RunState::Paused)) {
            sim.push_command(mailbox::command::OneStep{});
        }
        if (IsKeyPressed(KEY_SPACE)) {
            if (sim.get_run_state() == Simulation::RunState::Running) {
                sim.push_command(mailbox::command::Pause{});
            } else if (sim.get_run_state() == Simulation::RunState::Paused) {
                sim.push_command(mailbox::command::Resume{});
            }
        }
    }

    sim.end();
    rlImGuiShutdown();
    CloseWindow();
}

int main() {
    run();
    return 0;
}