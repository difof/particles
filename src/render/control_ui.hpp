#ifndef __CONTROL_UI_HPP
#define __CONTROL_UI_HPP

#include <imgui.h>
#include <raylib.h>

#include "../types.hpp"
#include "renderconfig.hpp"
#include "renderer.hpp"

class ControlUI : public IRenderer {
  public:
    ControlUI() = default;
    ~ControlUI() override = default;

    void render(RenderContext &ctx) override {
        if (!ctx.rcfg.show_ui)
            return;
        render_ui(ctx);
    }

  private:
    void render_ui(RenderContext &ctx) {
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
            // Windows menu
            if (ImGui::BeginMenu("Windows")) {
                if (ImGui::MenuItem("Toggle UI", "U")) {
                    ctx.rcfg.show_ui = !ctx.rcfg.show_ui;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Show metrics window")) {
                    ctx.rcfg.show_metrics_ui = true;
                }
                if (ImGui::MenuItem("Open Particle & Rule Editor")) {
                    ctx.rcfg.show_editor = true;
                }
                if (ImGui::MenuItem("Open Render Config")) {
                    ctx.rcfg.show_render_config = true;
                }
                if (ImGui::MenuItem("Open Simulation Config")) {
                    ctx.rcfg.show_sim_config = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "ESC")) {
                    ctx.should_exit = true;
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
    }
};

#endif
