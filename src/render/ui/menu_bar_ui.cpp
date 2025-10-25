#include "menu_bar_ui.hpp"
#include <filesystem>
#include <raylib.h>

#include "../../mailbox/command/cmds.hpp"
#include "../../utility/exceptions.hpp"
#include "../../utility/logger.hpp"

void MenuBarUI::render(Context &ctx) {
    if (!ctx.rcfg.show_ui)
        return;
    render_ui(ctx);
}

void MenuBarUI::set_current_filepath(const std::string &filepath) {
    m_current_filepath = filepath;
}

void MenuBarUI::trigger_new_project(Context &ctx) { handle_new_project(ctx); }

void MenuBarUI::trigger_open_project(Context &ctx) { handle_open_project(ctx); }

void MenuBarUI::trigger_save_project(Context &ctx) { handle_save_project(ctx); }

void MenuBarUI::trigger_save_as_project(Context &ctx) {
    handle_save_as_project(ctx);
}

void MenuBarUI::capture_saved_state(const Context &ctx) {
    m_saved_undo_version = ctx.undo.get_state_version();
    m_saved_file_version = ctx.save.get_file_operation_version();
    m_saved_past_size = ctx.undo.get_past_size();
}

void MenuBarUI::capture_saved_state(const UndoManager &undo_manager,
                                    const SaveManager &save_manager) {
    m_saved_undo_version = undo_manager.get_state_version();
    m_saved_file_version = save_manager.get_file_operation_version();
    m_saved_past_size = undo_manager.get_past_size();
}

bool MenuBarUI::has_unsaved_changes(const Context &ctx) const {
    // Check file operation version first
    if (ctx.save.get_file_operation_version() != m_saved_file_version) {
        return true;
    }

    // Check undo state - we're at the saved state if:
    // 1. We're at the exact same version, OR
    // 2. We've undone back to having the same number of actions in our past
    bool undo_at_saved_state =
        (ctx.undo.get_state_version() == m_saved_undo_version) ||
        (ctx.undo.get_past_size() == m_saved_past_size);

    return !undo_at_saved_state;
}

void MenuBarUI::render_ui(Context &ctx) {
    auto &sim = ctx.sim;
    mailbox::SimulationConfigSnapshot scfg = sim.get_config();
    bool scfg_updated = false;
    auto mark = [&scfg_updated](bool s) {
        if (s)
            scfg_updated = true;
    };

    if (ImGui::BeginMainMenuBar()) {
        render_project_indicator(ctx);
        render_file_menu(ctx);
        render_edit_menu(ctx);
        render_windows_menu(ctx);
        render_controls_menu(ctx);
        ImGui::EndMainMenuBar();
    }

    if (scfg_updated) {
        sim.update_config(scfg);
    }

    render_file_dialog(ctx);
}

void MenuBarUI::render_project_indicator(Context &ctx) {
    const char *label_prefix = "Project: ";
    std::string name;
    if (m_current_filepath.empty()) {
        name = "<unsaved>";
    } else {
        size_t pos = m_current_filepath.find_last_of('/');
        if (pos == std::string::npos || pos + 1 >= m_current_filepath.size())
            name = m_current_filepath;
        else
            name = m_current_filepath.substr(pos + 1);
    }

    // Check for unsaved changes and prepend asterisk
    if (has_unsaved_changes(ctx)) {
        name = "*" + name;
    }

    std::string btn = std::string(label_prefix) + name;
    if (ImGui::SmallButton(btn.c_str())) {
        handle_open_project(ctx);
    }
    if (!m_current_filepath.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", m_current_filepath.c_str());
    }
    ImGui::SameLine();
}

void MenuBarUI::render_file_menu(Context &ctx) {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New", "Ctrl+N")) {
            handle_new_project(ctx);
        }
        if (ImGui::MenuItem("Open", "Ctrl+O")) {
            handle_open_project(ctx);
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            handle_save_project(ctx);
        }
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
            handle_save_as_project(ctx);
        }
        ImGui::Separator();

        auto recent_files = ctx.save.get_recent_files();
        if (!recent_files.empty()) {
            for (const auto &file : recent_files) {
                if (ImGui::MenuItem(file.c_str())) {
                    handle_open_file(ctx, file);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear Recent Files")) {
                ctx.save.clear_recent_files();
            }
        }

        if (ImGui::MenuItem("Exit", "ESC")) {
            ctx.should_exit = true;
        }

        ImGui::EndMenu();
    }
}

void MenuBarUI::render_edit_menu(Context &ctx) {
    if (ImGui::BeginMenu("Edit")) {
        bool canUndo = ctx.undo.canUndo();
        bool canRedo = ctx.undo.canRedo();
        if (!canUndo)
            ImGui::BeginDisabled();
        if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
            ctx.undo.undo();
        }
        if (!canUndo)
            ImGui::EndDisabled();
        if (!canRedo)
            ImGui::BeginDisabled();
        if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
            ctx.undo.redo();
        }
        if (!canRedo)
            ImGui::EndDisabled();
        ImGui::EndMenu();
    }
}

void MenuBarUI::render_windows_menu(Context &ctx) {
    if (ImGui::BeginMenu("Windows")) {
        if (ImGui::MenuItem("Toggle UI", "U")) {
            ctx.rcfg.show_ui = !ctx.rcfg.show_ui;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Show metrics window", "1")) {
            ctx.rcfg.show_metrics_ui = true;
        }
        if (ImGui::MenuItem("Open Particle & Rule Editor", "2")) {
            ctx.rcfg.show_editor = true;
        }
        if (ImGui::MenuItem("Open Render Config", "3")) {
            ctx.rcfg.show_render_config = true;
        }
        if (ImGui::MenuItem("Open Simulation Config", "4")) {
            ctx.rcfg.show_sim_config = true;
        }
        ImGui::EndMenu();
    }
}

void MenuBarUI::render_controls_menu(Context &ctx) {
    if (ImGui::BeginMenu("Controls")) {
        if (ImGui::MenuItem("Reset world", "R")) {
            ctx.sim.push_command(mailbox::command::ResetWorld{});
        }
        if (ImGui::MenuItem("Pause/Resume", "SPACE")) {
            if (ctx.sim.get_run_state() == Simulation::RunState::Running) {
                ctx.sim.push_command(mailbox::command::Pause{});
            } else if (ctx.sim.get_run_state() ==
                       Simulation::RunState::Paused) {
                ctx.sim.push_command(mailbox::command::Resume{});
            }
        }
        if (ImGui::MenuItem("One Step", "S")) {
            ctx.sim.push_command(mailbox::command::OneStep{});
        }
        ImGui::EndMenu();
    }
}

void MenuBarUI::render_file_dialog(Context &ctx) {
    if (!m_file_dialog_open) {
        return;
    }

    if (m_file_dialog.render()) {
        m_file_dialog_open = false;
        if (m_file_dialog.has_result() && !m_file_dialog.canceled()) {
            const std::string path = m_file_dialog.selected_path();
            if (m_pending_action == PendingAction::Open) {
                {
                    handle_open_file(ctx, path);
                }
            } else if (m_pending_action == PendingAction::SaveAs) {
                SaveManager::ProjectData data;
                data.sim_config = ctx.sim.get_config();
                data.render_config = ctx.rcfg;
                data.seed = ctx.save.extract_current_seed(ctx.world_snapshot);
                data.window_config.panel_width = 500;
                data.window_config.render_width = ctx.wcfg.screen_width;

                try {
                    ctx.save.save_project(path, data);
                    m_current_filepath = path;
                    // Capture version snapshots after successful save as
                    capture_saved_state(ctx);
                    LOG_INFO("Project saved successfully to: " + path);
                } catch (const particles::IOError &e) {
                    LOG_ERROR("Failed to save project: " +
                              std::string(e.what()));
                    throw particles::UIError("Failed to save project to '" +
                                             path + "': " + e.what());
                }
            }
        }
        m_pending_action = PendingAction::None;
    }
}

void MenuBarUI::handle_new_project(Context &ctx) {
    SaveManager::ProjectData data;
    try {
        ctx.save.new_project(data);
        data.sim_config.bounds_width = (float)ctx.wcfg.screen_width;
        data.sim_config.bounds_height = (float)ctx.wcfg.screen_height;

        ctx.sim.update_config(data.sim_config);
        ctx.rcfg = data.render_config;

        if (data.seed.has_value()) {
            ctx.sim.push_command(
                mailbox::command::SeedWorld{data.seed.value()});
        }

        m_current_filepath.clear();
        // Capture version snapshots after successful new project
        capture_saved_state(ctx);

        LOG_INFO("New project created successfully");
    } catch (const particles::IOError &e) {
        LOG_ERROR("Failed to create new project: " + std::string(e.what()));
        throw particles::UIError("Failed to create new project: " +
                                 std::string(e.what()));
    }
}

void MenuBarUI::handle_open_project(Context &ctx) {
    if (!m_file_dialog_open) {
        m_file_dialog.set_filename("");
        m_file_dialog.open(FileDialog::Mode::Open, "Open Project", "",
                           &ctx.save);
        m_file_dialog_open = true;
        m_pending_action = PendingAction::Open;
        return;
    }
}

void MenuBarUI::handle_save_project(Context &ctx) {
    if (m_current_filepath.empty()) {
        handle_save_as_project(ctx);
        return;
    }

    SaveManager::ProjectData data;
    data.sim_config = ctx.sim.get_config();
    data.render_config = ctx.rcfg;

    data.seed = ctx.save.extract_current_seed(ctx.world_snapshot);

    try {
        ctx.save.save_project(m_current_filepath, data);
        // Capture version snapshots after successful save
        capture_saved_state(ctx);
        LOG_INFO("Project saved successfully to: " + m_current_filepath);
    } catch (const particles::IOError &e) {
        LOG_ERROR("Failed to save project: " + std::string(e.what()));
        throw particles::UIError("Failed to save project to '" +
                                 m_current_filepath + "': " + e.what());
    }
}

void MenuBarUI::handle_save_as_project(Context &ctx) {
    if (!m_file_dialog_open) {
        if (!m_current_filepath.empty()) {
            auto pos = m_current_filepath.find_last_of('/');
            if (pos != std::string::npos && pos + 1 < m_current_filepath.size())
                m_file_dialog.set_filename(m_current_filepath.substr(pos + 1));
            else
                m_file_dialog.set_filename(m_current_filepath);
        } else {
            m_file_dialog.set_filename("project.json");
        }
        m_file_dialog.open(FileDialog::Mode::Save, "Save Project", "",
                           &ctx.save);
        m_file_dialog_open = true;
        m_pending_action = PendingAction::SaveAs;
        return;
    }
}

void MenuBarUI::handle_open_file(Context &ctx, const std::string &filepath) {
    SaveManager::ProjectData data;
    try {
        ctx.save.load_project(filepath, data);
        ctx.sim.update_config(data.sim_config);
        ctx.rcfg = data.render_config;

        if (data.seed.has_value()) {
            ctx.sim.push_command(
                mailbox::command::SeedWorld{data.seed.value()});
        }

        m_current_filepath = filepath;
        // Capture version snapshots after successful load
        capture_saved_state(ctx);
        LOG_INFO("Project loaded successfully from: " + filepath);
    } catch (const particles::IOError &e) {
        LOG_ERROR("Failed to load project: " + std::string(e.what()));
        throw particles::UIError("Failed to load project from '" + filepath +
                                 "': " + e.what());
    }
}
