#ifndef __CONTROL_UI_HPP
#define __CONTROL_UI_HPP

#include <imgui.h>
#include <raylib.h>

#include "../types.hpp"
#include "renderconfig.hpp"
#include "renderer.hpp"

class ControlUI : public IRenderer {
  public:
    ControlUI(const WindowConfig &wcfg) : m_wcfg(wcfg) {}
    ~ControlUI() override = default;

    void render(RenderContext &ctx) override {
        if (!ctx.rcfg.show_ui)
            return;
        render_ui(ctx);
    }

  private:
    void render_ui(RenderContext &ctx) {
        auto &wcfg = m_wcfg;
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

        ImGui::SeparatorText("Sim Config");
        mark(ImGui::SliderInt("Target TPS", &scfg.target_tps, 0, 240, "%d",
                              ImGuiSliderFlags_AlwaysClamp));
        mark(ImGui::SliderFloat("Time Scale", &scfg.time_scale, 0.01f, 2.0f,
                                "%.3f", ImGuiSliderFlags_Logarithmic));
        mark(ImGui::SliderFloat("Viscosity", &scfg.viscosity, 0.0f, 1.0f,
                                "%.3f"));
        mark(ImGui::SliderFloat("Wall Repel (px)", &scfg.wall_repel, 0.0f,
                                200.0f, "%.1f"));
        mark(ImGui::SliderFloat("Wall Strength", &scfg.wall_strength, 0.0f,
                                1.0f, "%.3f"));

        ImGui::SeparatorText("Render");
        ImGui::Checkbox("Interpolate", &rcfg.interpolate);
        if (rcfg.interpolate) {
            ImGui::SliderFloat("Interp delay (ms)", &rcfg.interp_delay_ms, 0.0f,
                               50.0f, "%.1f");
        }
        ImGui::SliderFloat("Core size (px)", &rcfg.core_size, 0.5f, 4.0f,
                           "%.2f");
        ImGui::Checkbox("Glow enabled", &rcfg.glow_enabled);
        if (rcfg.glow_enabled) {
            ImGui::SliderFloat("Outer scale (x core)", &rcfg.outer_scale_mul,
                               4.0f, 24.0f, "%.1f");
            ImGui::SliderFloat("Outer RGB gain", &rcfg.outer_rgb_gain, 0.0f,
                               1.0f, "%.2f");
            ImGui::SliderFloat("Inner scale (x core)", &rcfg.inner_scale_mul,
                               1.0f, 8.0f, "%.1f");
            ImGui::SliderFloat("Inner RGB gain", &rcfg.inner_rgb_gain, 0.0f,
                               1.0f, "%.2f");
            ImGui::Checkbox("Final additive blit", &rcfg.final_additive_blit);
        }

        ImGui::SeparatorText("Overlays");
        mark(ImGui::Checkbox("Density heatmap", &rcfg.show_density_heat));
        if (rcfg.show_density_heat) {
            ImGui::SliderFloat("Heat alpha", &rcfg.heat_alpha, 0.0f, 1.0f,
                               "%.2f");
        }
        mark(ImGui::Checkbox("Show grid lines", &rcfg.show_grid_lines));
        mark(ImGui::Checkbox("Velocity field", &rcfg.show_velocity_field));
        if (rcfg.show_velocity_field) {
            ImGui::SliderFloat("Vel scale", &rcfg.vel_scale, 0.1f, 5.0f,
                               "%.2f");
            ImGui::SliderFloat("Vel thickness", &rcfg.vel_thickness, 0.5f, 4.0f,
                               "%.1f");
        }
        scfg.draw_report.grid_data = rcfg.show_grid_lines ||
                                     rcfg.show_density_heat ||
                                     rcfg.show_velocity_field;

        ImGui::SeparatorText("Parallelism");
        unsigned hc = std::thread::hardware_concurrency();
        int max_threads = std::max(1, (int)hc - 2);
        ImGui::Text("HW threads: %u", hc ? hc : 1);
        bool auto_mode = (scfg.sim_threads <= 0);
        if (ImGui::Checkbox("Auto (HW-2)", &auto_mode)) {
            scfg.sim_threads = auto_mode ? -1 : std::min(1, max_threads);
            mark(true);
        }
        if (!auto_mode) {
            mark(ImGui::SliderInt("Sim threads", &scfg.sim_threads, 1,
                                  max_threads, "%d",
                                  ImGuiSliderFlags_AlwaysClamp));
        } else {
            ImGui::BeginDisabled();
            int auto_val = std::max(1, (int)compute_sim_threads());
            ImGui::SliderInt("Sim threads", &auto_val, 1, max_threads, "%d");
            ImGui::EndDisabled();
        }

        ImGui::End();
        ImGui::PopStyleVar();
        if (scfg_updated) {
            sim.update_config(scfg);
        }
    }

  private:
    WindowConfig m_wcfg;
};

#endif
