#ifndef __RENDER_CONFIG_UI_HPP
#define __RENDER_CONFIG_UI_HPP

#include <imgui.h>
#include <raylib.h>

#include "../types.hpp"
#include "renderer.hpp"

class RenderConfigUI : public IRenderer {
  public:
    RenderConfigUI() = default;
    ~RenderConfigUI() override = default;

    void render(RenderContext &ctx) override {
        if (!ctx.rcfg.show_ui || !ctx.rcfg.show_render_config)
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

        ImGui::Begin("Render Configuration", &ctx.rcfg.show_render_config);
        ImGui::SetWindowSize(ImVec2{500, 600}, ImGuiCond_FirstUseEver);

        ImGui::SeparatorText("Interpolation");
        ImGui::Checkbox("Interpolate", &rcfg.interpolate);
        if (rcfg.interpolate) {
            ImGui::SliderFloat("Interp delay (ms)", &rcfg.interp_delay_ms, 0.0f,
                               50.0f, "%.1f");
        }

        ImGui::SeparatorText("Particle Rendering");
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

        // Update simulation config for grid data
        scfg.draw_report.grid_data = rcfg.show_grid_lines ||
                                     rcfg.show_density_heat ||
                                     rcfg.show_velocity_field;

        ImGui::End();

        if (scfg_updated) {
            sim.update_config(scfg);
        }
    }
};

#endif
