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
        auto &wcfg = ctx.wcfg;
        auto &sim = ctx.sim;
        auto &rcfg = ctx.rcfg;
        mailbox::SimulationConfig::Snapshot scfg = sim.get_config();
        mailbox::SimulationStats::Snapshot stats = sim.get_stats();
        const World &world = sim.get_world();
        bool scfg_updated = false;
        auto mark = [&scfg_updated](bool s) {
            if (s)
                scfg_updated = true;
        };

        auto to_imvec4 = [](Color c) -> ImVec4 {
            return ImVec4(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
        };
        auto window_size =
            ImVec2{(float)wcfg.panel_width, (float)wcfg.screen_height * .7f};
        auto window_x =
            (float)wcfg.screen_width / 2 - (float)wcfg.panel_width / 2;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.);
        ImGui::Begin("control", NULL);
        ImGui::SetWindowPos(ImVec2{window_x, 0.f}, ImGuiCond_Appearing);
        ImGui::SetWindowSize(window_size, ImGuiCond_Appearing);

        ImGui::SeparatorText("Windows");
        ctx.rcfg.show_metrics_ui =
            ImGui::Button("Show metrics window") || ctx.rcfg.show_metrics_ui;
        ctx.rcfg.show_editor = ImGui::Button("Open Particle & Rule Editor") ||
                               ctx.rcfg.show_editor;
        ctx.rcfg.show_render_config =
            ImGui::Button("Open Render Config") || ctx.rcfg.show_render_config;
        ctx.rcfg.show_sim_config =
            ImGui::Button("Open Simulation Config") || ctx.rcfg.show_sim_config;

        ImGui::SeparatorText("Controls");
        if (ImGui::Button("Reset world")) {
            sim.push_command(mailbox::command::ResetWorld{});
        }
        ImGui::SameLine();
        if (ImGui::Button("Pause")) {
            sim.push_command(mailbox::command::Pause{});
        }
        ImGui::SameLine();
        if (ImGui::Button("Resume")) {
            sim.push_command(mailbox::command::Resume{});
        }
        ImGui::SameLine();
        if (ImGui::Button("One Step")) {
            sim.push_command(mailbox::command::OneStep{});
        }

        ImGui::End();
        ImGui::PopStyleVar();
        if (scfg_updated) {
            sim.update_config(scfg);
        }
    }
};

#endif
