#pragma once

#include <imgui.h>
#include <raylib.h>
#include <string>

#include "../../save_manager.hpp"
#include "../../window_config.hpp"
#include "../file_dialog.hpp"
#include "../renderer.hpp"
#include "../types/config.hpp"

class MenuBarUI : public IRenderer {
  public:
    MenuBarUI() = default;
    ~MenuBarUI() override = default;

    // File operations

    void render(Context &ctx) override {
        if (!ctx.rcfg.show_ui)
            return;
        render_ui(ctx);
    }

    // Set current file path (used when loading files outside of menu)
    void set_current_filepath(const std::string &filepath) {
        m_current_filepath = filepath;
    }

  private:
    std::string m_current_filepath;
    FileDialog m_file_dialog;
    bool m_file_dialog_open = false;
    enum class PendingAction { None, Open, SaveAs };
    PendingAction m_pending_action = PendingAction::None;

    void render_ui(Context &ctx) {
        auto &sim = ctx.sim;
        auto &rcfg = ctx.rcfg;
        mailbox::SimulationConfig::Snapshot scfg = sim.get_config();
        bool scfg_updated = false;
        auto mark = [&scfg_updated](bool s) {
            if (s)
                scfg_updated = true;
        };

        // Create main menu bar
        if (ImGui::BeginMainMenuBar()) {
            // File menu
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

                // Recent files
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

            // Edit menu
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

            // Windows menu
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

            // Controls menu
            if (ImGui::BeginMenu("Controls")) {
                if (ImGui::MenuItem("Reset world", "R")) {
                    sim.push_command(mailbox::command::ResetWorld{});
                }
                if (ImGui::MenuItem("Pause/Resume", "SPACE")) {
                    if (sim.get_run_state() == Simulation::RunState::Running) {
                        sim.push_command(mailbox::command::Pause{});
                    } else if (sim.get_run_state() ==
                               Simulation::RunState::Paused) {
                        sim.push_command(mailbox::command::Resume{});
                    }
                }
                if (ImGui::MenuItem("One Step", "S")) {
                    sim.push_command(mailbox::command::OneStep{});
                }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        if (scfg_updated) {
            sim.update_config(scfg);
        }

        // Render file dialog if open
        if (m_file_dialog_open) {
            if (m_file_dialog.render()) {
                m_file_dialog_open = false;
                if (m_file_dialog.has_result() && !m_file_dialog.canceled()) {
                    const std::string path = m_file_dialog.selected_path();
                    if (m_pending_action == PendingAction::Open) {
                        handle_open_file(ctx, path);
                    } else if (m_pending_action == PendingAction::SaveAs) {
                        // Collect current state and save
                        SaveManager::ProjectData data;
                        data.sim_config = ctx.sim.get_config();
                        data.render_config = ctx.rcfg;
                        data.seed =
                            ctx.save.extract_current_seed(ctx.sim.get_world());
                        try {
                            ctx.save.save_project(path, data);
                            m_current_filepath = path;
                        } catch (const particles::IOError &e) {
                            // Error handling is done in the catch block
                        }
                    }
                }
                m_pending_action = PendingAction::None;
            }
        }
    }

    void handle_new_project(Context &ctx);
    void handle_open_project(Context &ctx);
    void handle_save_project(Context &ctx);
    void handle_save_as_project(Context &ctx);
    void handle_open_file(Context &ctx, const std::string &filepath);
};
