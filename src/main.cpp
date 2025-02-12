#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

#include "mailbox/mailbox.hpp"
#include "render/manager.hpp"
#include "render/types/config.hpp"
#include "save_manager.hpp"
#include "simulation/simulation.hpp"
#include "simulation/world.hpp"
#include "undo.hpp"
#include "utility/default_seed.hpp"
#include "utility/exceptions.hpp"
#include "utility/logger.hpp"
#include "window_config.hpp"

void run() {
    LOG_INFO("Starting particles application");
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    // Initialize singletons first to load window state
    SaveManager json_manager;
    auto window_state = json_manager.load_window_state();

    InitWindow(window_state.width, window_state.height, "Particles");

    // Set window position if we have saved position
    if (window_state.x != 0 || window_state.y != 0) {
        SetWindowPosition(window_state.x, window_state.y);
    }

    int monitor = GetCurrentMonitor();
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    int panelW = 500;
    int texW = screenW;

    WindowConfig wcfg = {screenW, screenH, panelW, texW};
    Config rcfg;
    rcfg.interpolate = true;
    rcfg.core_size = 1.5f;
    rcfg.glow_enabled = true;
    rcfg.outer_scale_mul = 24.f;
    rcfg.outer_rgb_gain = .78f;
    rcfg.inner_scale_mul = 1.f;
    rcfg.inner_rgb_gain = .52f;

    mailbox::SimulationConfigSnapshot scfg = {};
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

    // Initialize remaining singletons
    UndoManager undo_manager;
    std::string last_file = json_manager.get_last_opened_file();

    // Create render manager
    RenderManager rman({wcfg.screen_width, wcfg.screen_height, wcfg.panel_width,
                        wcfg.render_width});

    sim.begin();

    // Try to load last project, otherwise use default seed
    bool loaded_project = false;
    if (!last_file.empty()) {
        SaveManager::ProjectData data;
        try {
            json_manager.load_project(last_file, data);
            // Apply loaded project settings
            sim.update_config(data.sim_config);
            rcfg = data.render_config;

            // Send loaded seed to simulation
            if (data.seed) {
                sim.push_command(mailbox::command::SeedWorld{data.seed});
                loaded_project = true;
            }
            // Set current file path so Save overwrites instead of opening
            // dialog
            rman.get_menu_bar().set_current_filepath(last_file);
        } catch (const particles::IOError &e) {
            LOG_ERROR("Failed to load project: " + std::string(e.what()));
        }
    }

    // If no project was loaded, use default seed
    if (!loaded_project) {
        // send initial seed from main
        auto seed = particles::utility::create_default_seed();
        sim.push_command(mailbox::command::SeedWorld{seed});
    }

    while (!WindowShouldClose()) {
        // Handle window resize
        if (IsWindowResized()) {
            int newWidth = GetScreenWidth();
            int newHeight = GetScreenHeight();
            LOG_INFO("Window resized to " + std::to_string(newWidth) + "x" +
                     std::to_string(newHeight));

            // Update window config
            wcfg.screen_width = newWidth;
            wcfg.screen_height = newHeight;
            wcfg.render_width = newWidth;

            // Resize render manager components
            rman.resize(wcfg);
        }

        // Get world snapshot for save/undo operations
        auto world_snapshot = sim.get_world_snapshot();

        if (rman.draw_frame(sim, rcfg, json_manager, undo_manager))
            break;

        // Global Undo/Redo shortcuts: Ctrl/Cmd+Z, Ctrl/Cmd+Y, Shift+Ctrl/Cmd+Z
        bool ctrl_cmd = IsKeyDown(KEY_LEFT_CONTROL) ||
                        IsKeyDown(KEY_RIGHT_CONTROL) ||
                        IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (ctrl_cmd && IsKeyPressed(KEY_Z)) {
            if (shift) {
                undo_manager.redo();
            } else {
                undo_manager.undo();
            }
        }
        if (ctrl_cmd && IsKeyPressed(KEY_Y)) {
            undo_manager.redo();
        }

        // Check if ImGui is capturing input (moved up to be available for all
        // shortcuts)
        bool imgui_mouse_captured = false;
        bool imgui_keyboard_captured = false;
        if (rcfg.show_ui) {
            // Check ImGui IO state to see if input is captured
            ImGuiIO &io = ImGui::GetIO();
            imgui_mouse_captured = io.WantCaptureMouse;
            imgui_keyboard_captured = io.WantCaptureKeyboard;
        }

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
        // Window toggle shortcuts (only when ImGui is not capturing keyboard)
        if (!imgui_keyboard_captured) {
            if (IsKeyPressed(KEY_ONE)) {
                rcfg.show_metrics_ui = !rcfg.show_metrics_ui;
            }
            if (IsKeyPressed(KEY_TWO)) {
                rcfg.show_editor = !rcfg.show_editor;
            }
            if (IsKeyPressed(KEY_THREE)) {
                rcfg.show_render_config = !rcfg.show_render_config;
            }
            if (IsKeyPressed(KEY_FOUR)) {
                rcfg.show_sim_config = !rcfg.show_sim_config;
            }
        }

        // Camera controls (keyboard and mouse)
        const float pan_speed = 10.0f;
        const float zoom_step = 0.1f;
        const float min_zoom_log = -3.0f; // 0.125x zoom
        const float max_zoom_log = 3.0f;  // 8x zoom

        // ImGui input capture already checked above

        // Keyboard panning (only when ImGui is not capturing keyboard)
        if (!imgui_keyboard_captured) {
            if (IsKeyDown(KEY_LEFT)) {
                rcfg.camera.x -= pan_speed;
            }
            if (IsKeyDown(KEY_RIGHT)) {
                rcfg.camera.x += pan_speed;
            }
            if (IsKeyDown(KEY_UP)) {
                rcfg.camera.y -= pan_speed;
            }
            if (IsKeyDown(KEY_DOWN)) {
                rcfg.camera.y += pan_speed;
            }
        }

        // Mouse panning (left click and drag) - only when not over ImGui
        if (!ctrl_cmd && !imgui_mouse_captured &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 delta = GetMouseDelta();
            const float zoom = rcfg.camera.zoom();
            rcfg.camera.x -= delta.x / zoom;
            rcfg.camera.y -= delta.y / zoom;
        }

        // Keyboard zoom (only when ImGui is not capturing keyboard)
        if (!imgui_keyboard_captured) {
            if (IsKeyPressed(KEY_MINUS)) {
                rcfg.camera.zoom_log =
                    std::clamp(rcfg.camera.zoom_log - zoom_step, min_zoom_log,
                               max_zoom_log);
            }
            if (IsKeyPressed(KEY_EQUAL)) {
                rcfg.camera.zoom_log =
                    std::clamp(rcfg.camera.zoom_log + zoom_step, min_zoom_log,
                               max_zoom_log);
            }
        }

        // Mouse wheel zoom (center-based) - only when not over ImGui
        if (!imgui_mouse_captured) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                const float zoom_scale = 0.1f * wheel;
                rcfg.camera.zoom_log =
                    std::clamp(rcfg.camera.zoom_log + zoom_scale, min_zoom_log,
                               max_zoom_log);
            }
        }
    }

    sim.end();

    // Save window state before closing
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