#ifndef __SIM_CONFIG_UI_HPP
#define __SIM_CONFIG_UI_HPP

#include <imgui.h>
#include <raylib.h>

#include "../types.hpp"
#include "renderer.hpp"

class SimConfigUI : public IRenderer {
  public:
    SimConfigUI() = default;
    ~SimConfigUI() override = default;

    void render(RenderContext &ctx) override {
        if (!ctx.rcfg.show_ui || !ctx.rcfg.show_sim_config)
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

        ImGui::Begin("Simulation Configuration", &ctx.rcfg.show_sim_config);
        ImGui::SetWindowSize(ImVec2{450, 500}, ImGuiCond_FirstUseEver);

        ImGui::SeparatorText("Simulation Parameters");
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

        if (scfg_updated) {
            sim.update_config(scfg);
        }
    }
};

#endif
