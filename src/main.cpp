#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

#include "input/key_manager.hpp"
#include "input/keys.hpp"
#include "mailbox/mailbox.hpp"
#include "render/manager.hpp"
#include "render/types/config.hpp"
#include "render/types/context.hpp"
#include "render/types/window.hpp"
#include "save_manager.hpp"
#include "simulation/simulation.hpp"
#include "simulation/world.hpp"
#include "undo/undo_manager.hpp"
#include "utility/default_seed.hpp"
#include "utility/exceptions.hpp"
#include "utility/logger.hpp"

void run() {
    LOG_INFO("Starting particles application");
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    SaveManager json_manager;
    auto window_state = json_manager.load_window_state();

    UndoManager undo_manager;
    std::string last_file = json_manager.get_last_opened_file();

    InitWindow(window_state.width, window_state.height, "Particles");

    int monitor = GetCurrentMonitor();
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    if (window_state.width != 0 || window_state.height != 0) {
        screenW = window_state.width;
        screenH = window_state.height;
    }

    WindowConfig wcfg = {screenW, screenH};
    Config rcfg;
    rcfg.interpolate = true;
    rcfg.core_size = 1.5f;
    rcfg.glow_enabled = true;
    rcfg.outer_scale_mul = 24.f;
    rcfg.outer_rgb_gain = .78f;
    rcfg.inner_scale_mul = 1.f;
    rcfg.inner_rgb_gain = .52f;

    mailbox::SimulationConfigSnapshot scfg = {};
    scfg.bounds_width = (float)wcfg.screen_width;
    scfg.bounds_height = (float)wcfg.screen_height;
    scfg.target_tps = 0;
    scfg.time_scale = 1.0f;
    scfg.viscosity = 0.271f;
    scfg.wall_repel = 86.0f;
    scfg.wall_strength = 0.129f;
    scfg.sim_threads = -1;
    Simulation sim(scfg);

    if (window_state.x != 0 || window_state.y != 0) {
        SetWindowPosition(window_state.x, window_state.y);
    } else {
        SetWindowPosition(0, 0);
    }

    SetWindowSize(wcfg.screen_width, wcfg.screen_height);
    SetTargetFPS(60);
    rlImGuiSetup(true);

    ImGui::GetIO().IniFilename = nullptr;

    RenderManager rman(wcfg);

    sim.begin();

    KeyManager key_manager;
    bool keys_setup = false;
    bool should_exit = false;

    // Try to load last project, otherwise use default seed
    bool loaded_project = false;
    if (!last_file.empty()) {
        SaveManager::ProjectData data;
        try {
            json_manager.load_project(last_file, data);
            sim.update_config(data.sim_config);
            rcfg = data.render_config;

            if (data.seed.has_value()) {
                sim.push_command(
                    mailbox::command::SeedWorld{data.seed.value()});
                loaded_project = true;
            }
            rman.get_menu_bar().set_current_filepath(last_file);
        } catch (const particles::IOError &e) {
            LOG_ERROR("Failed to load project: " + std::string(e.what()));
        }
    }

    if (!loaded_project) {
        auto seed = particles::utility::create_default_seed();
        sim.push_command(mailbox::command::SeedWorld{seed});
    }

    while (!WindowShouldClose()) {
        if (IsWindowResized()) {
            int newWidth = GetScreenWidth();
            int newHeight = GetScreenHeight();
            LOG_INFO("Window resized to " + std::to_string(newWidth) + "x" +
                     std::to_string(newHeight));

            wcfg.screen_width = newWidth;
            wcfg.screen_height = newHeight;

            rman.resize(wcfg);
        }

        if (rman.draw_frame(sim, rcfg, json_manager, undo_manager))
            break;

        // Setup keys on first frame (after we have access to RenderManager)
        if (!keys_setup) {
            setup_keys(key_manager, sim, rcfg, json_manager, undo_manager,
                       rman.get_menu_bar(), should_exit);
            keys_setup = true;
        }

        if (should_exit) {
            break;
        }

        // Check ImGui capture state
        bool imgui_mouse_captured = false;
        bool imgui_keyboard_captured = false;
        if (rcfg.show_ui) {
            ImGuiIO &io = ImGui::GetIO();
            imgui_mouse_captured = io.WantCaptureMouse;
            imgui_keyboard_captured = io.WantCaptureKeyboard;
        }

        // Process keyboard input
        key_manager.process(imgui_keyboard_captured);

        // Mouse handling
        bool ctrl_cmd = IsKeyDown(KEY_LEFT_CONTROL) ||
                        IsKeyDown(KEY_RIGHT_CONTROL) ||
                        IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
        if (!ctrl_cmd && !imgui_mouse_captured &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 delta = GetMouseDelta();
            const float zoom = rcfg.camera.zoom();
            rcfg.camera.x -= delta.x / zoom;
            rcfg.camera.y -= delta.y / zoom;
        }

        // Mouse wheel zoom
        if (!imgui_mouse_captured) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                const float zoom_scale = 0.1f * wheel;
                const float min_zoom_log = -3.0f; // 0.125x zoom
                const float max_zoom_log = 3.0f;  // 8x zoom
                rcfg.camera.zoom_log =
                    std::clamp(rcfg.camera.zoom_log + zoom_scale, min_zoom_log,
                               max_zoom_log);
            }
        }
    }

    sim.end();

    SaveManager::WindowState current_state;
    current_state.width = GetScreenWidth();
    current_state.height = GetScreenHeight();
    current_state.x = GetWindowPosition().x;
    current_state.y = GetWindowPosition().y;
    json_manager.save_window_state(current_state);

    rlImGuiShutdown();
    CloseWindow();
}

int main() {
    try {
        run();
        LOG_INFO("Application shutting down normally");
        return 0;
    } catch (const particles::ParticlesException &e) {
        LOG_ERROR("Particles error: " + std::string(e.what()));
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception &e) {
        LOG_ERROR("Standard error: " + std::string(e.what()));
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        LOG_ERROR("Unknown error occurred");
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}