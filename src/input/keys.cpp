#include "keys.hpp"

#include "mailbox/command/cmds.hpp"
#include "render/types/window.hpp"
#include "utility/logger.hpp"

void setup_keys(KeyManager &key_manager, Simulation &sim, Config &rcfg,
                SaveManager &save_manager, UndoManager &undo_manager,
                MenuBarUI &menu_bar, bool &should_exit) {
    // Undo/Redo operations
    key_manager.on_key_pressed(
        KEY_Z,
        [&undo_manager]() {
            undo_manager.undo();
        },
        true); // Ctrl+Z

    key_manager.on_key_pressed(
        KEY_Z,
        [&undo_manager]() {
            undo_manager.redo();
        },
        true, true); // Ctrl+Shift+Z

    key_manager.on_key_pressed(
        KEY_Y,
        [&undo_manager]() {
            undo_manager.redo();
        },
        true); // Ctrl+Y

    // File operations
    key_manager.on_key_pressed(
        KEY_N,
        [&menu_bar, &sim, &rcfg, &save_manager, &undo_manager]() {
            // Create a minimal context for the file operation
            auto view = sim.begin_read_draw();
            auto world_snapshot = sim.get_world_snapshot();
            bool can_interpolate = false;
            float alpha = 1.0f;
            WindowConfig wcfg{800, 600}; // Dummy window config
            Context temp_ctx{sim,
                             rcfg,
                             view,
                             wcfg,
                             can_interpolate,
                             alpha,
                             world_snapshot,
                             save_manager,
                             undo_manager};
            menu_bar.trigger_new_project(temp_ctx);
            sim.end_read_draw(view);
        },
        true); // Ctrl+N

    key_manager.on_key_pressed(
        KEY_O,
        [&menu_bar, &sim, &rcfg, &save_manager, &undo_manager]() {
            // Create a minimal context for the file operation
            auto view = sim.begin_read_draw();
            auto world_snapshot = sim.get_world_snapshot();
            bool can_interpolate = false;
            float alpha = 1.0f;
            WindowConfig wcfg{800, 600}; // Dummy window config
            Context temp_ctx{sim,
                             rcfg,
                             view,
                             wcfg,
                             can_interpolate,
                             alpha,
                             world_snapshot,
                             save_manager,
                             undo_manager};
            menu_bar.trigger_open_project(temp_ctx);
            sim.end_read_draw(view);
        },
        true); // Ctrl+O

    key_manager.on_key_pressed(
        KEY_S,
        [&menu_bar, &sim, &rcfg, &save_manager, &undo_manager]() {
            // Create a minimal context for the file operation
            auto view = sim.begin_read_draw();
            auto world_snapshot = sim.get_world_snapshot();
            bool can_interpolate = false;
            float alpha = 1.0f;
            WindowConfig wcfg{800, 600}; // Dummy window config
            Context temp_ctx{sim,
                             rcfg,
                             view,
                             wcfg,
                             can_interpolate,
                             alpha,
                             world_snapshot,
                             save_manager,
                             undo_manager};
            menu_bar.trigger_save_project(temp_ctx);
            sim.end_read_draw(view);
        },
        true); // Ctrl+S

    key_manager.on_key_pressed(
        KEY_S,
        [&menu_bar, &sim, &rcfg, &save_manager, &undo_manager]() {
            // Create a minimal context for the file operation
            auto view = sim.begin_read_draw();
            auto world_snapshot = sim.get_world_snapshot();
            bool can_interpolate = false;
            float alpha = 1.0f;
            WindowConfig wcfg{800, 600}; // Dummy window config
            Context temp_ctx{sim,
                             rcfg,
                             view,
                             wcfg,
                             can_interpolate,
                             alpha,
                             world_snapshot,
                             save_manager,
                             undo_manager};
            menu_bar.trigger_save_as_project(temp_ctx);
            sim.end_read_draw(view);
        },
        true, true); // Ctrl+Shift+S

    key_manager.on_key_pressed(KEY_ESCAPE, [&should_exit]() {
        should_exit = true;
    }); // Esc

    // Simulation controls
    key_manager.on_key_pressed(KEY_R, [&sim]() {
        sim.push_command(mailbox::command::ResetWorld{});
    }); // R

    key_manager.on_key_pressed(KEY_SPACE, [&sim]() {
        if (sim.get_run_state() == Simulation::RunState::Running) {
            sim.push_command(mailbox::command::Pause{});
        } else if (sim.get_run_state() == Simulation::RunState::Paused) {
            sim.push_command(mailbox::command::Resume{});
        }
    }); // Space

    key_manager.on_key_pressed(KEY_S, [&sim]() {
        sim.push_command(mailbox::command::OneStep{});
    }); // S

    key_manager.on_key_repeat(KEY_S, [&sim]() {
        if (sim.get_run_state() == Simulation::RunState::Paused) {
            sim.push_command(mailbox::command::OneStep{});
        }
    }); // S (repeat when paused)

    // UI toggles
    key_manager.on_key_pressed(KEY_U, [&rcfg]() {
        rcfg.show_ui = !rcfg.show_ui;
    }); // U

    key_manager.on_key_pressed(KEY_ONE, [&rcfg]() {
        rcfg.show_metrics_ui = !rcfg.show_metrics_ui;
    }); // 1

    key_manager.on_key_pressed(KEY_TWO, [&rcfg]() {
        rcfg.show_editor = !rcfg.show_editor;
    }); // 2

    key_manager.on_key_pressed(KEY_THREE, [&rcfg]() {
        rcfg.show_render_config = !rcfg.show_render_config;
    }); // 3

    key_manager.on_key_pressed(KEY_FOUR, [&rcfg]() {
        rcfg.show_sim_config = !rcfg.show_sim_config;
    }); // 4

    key_manager.on_key_pressed(KEY_FIVE, [&rcfg]() {
        rcfg.show_history_ui = !rcfg.show_history_ui;
    }); // 5

#ifdef DEBUG
    key_manager.on_key_pressed(KEY_F4, [&rcfg]() {
        rcfg.show_style_editor = !rcfg.show_style_editor;
    });
#endif

    // Camera controls
    static const float pan_speed = 10.0f;
    key_manager.on_key_down(KEY_LEFT, [&rcfg]() {
        rcfg.camera.x -= pan_speed;
    }); // Left arrow

    key_manager.on_key_down(KEY_RIGHT, [&rcfg]() {
        rcfg.camera.x += pan_speed;
    }); // Right arrow

    key_manager.on_key_down(KEY_UP, [&rcfg]() {
        rcfg.camera.y -= pan_speed;
    }); // Up arrow

    key_manager.on_key_down(KEY_DOWN, [&rcfg]() {
        rcfg.camera.y += pan_speed;
    }); // Down arrow

    // Zoom controls
    static const float zoom_step = 0.1f;
    static const float min_zoom_log = -3.0f; // 0.125x zoom
    static const float max_zoom_log = 3.0f;  // 8x zoom

    key_manager.on_key_pressed(KEY_MINUS, [&rcfg]() {
        rcfg.camera.zoom_log = std::clamp(rcfg.camera.zoom_log - zoom_step,
                                          min_zoom_log, max_zoom_log);
    }); // -

    key_manager.on_key_pressed(KEY_EQUAL, [&rcfg]() {
        rcfg.camera.zoom_log = std::clamp(rcfg.camera.zoom_log + zoom_step,
                                          min_zoom_log, max_zoom_log);
    }); // =

    LOG_INFO("Keyboard shortcuts registered successfully");
}
