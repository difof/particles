#pragma once

#include <imgui.h>
#include <raylib.h>

#include "../../undo.hpp"
#include "../../window_config.hpp"
#include "../renderer.hpp"

class SimConfigUI : public IRenderer {
  public:
    SimConfigUI() = default;
    ~SimConfigUI() override = default;

    void render(Context &ctx) override {
        if (!ctx.rcfg.show_ui || !ctx.rcfg.show_sim_config)
            return;
        render_ui(ctx);
    }

  private:
    void render_ui(Context &ctx) {
        auto &sim = ctx.sim;
        auto &rcfg = ctx.rcfg;
        mailbox::SimulationConfig::Snapshot scfg = sim.get_config();
        bool scfg_updated = false;
        auto mark = [&scfg_updated](bool s) {
            if (s)
                scfg_updated = true;
        };

        auto push_scfg = [&](const char *key, const char *label, auto before,
                             auto after, auto setter) {
            ImGuiID id = ImGui::GetItemID();
            if (ImGui::IsItemActivated())
                ctx.undo.beginInteraction(id);
            using T = decltype(before);
            ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<T>(
                key, label,
                []() {
                    return T{};
                },
                setter, before, after)));
            if (ImGui::IsItemDeactivatedAfterEdit())
                ctx.undo.endInteraction(id);
        };

        ImGui::Begin("Simulation Configuration", &ctx.rcfg.show_sim_config);
        ImGui::SetWindowSize(ImVec2{450, 500}, ImGuiCond_FirstUseEver);

        ImGui::SeparatorText("Simulation Parameters");
        ImGui::SeparatorText("Bounds");
        {
            struct BoundsUIState {
                bool inited = false;
                int bw = 0;
                int bh = 0;
                int applied_w = 0; // last committed to sim config
                int applied_h = 0;
            };
            static BoundsUIState bs;
            if (!bs.inited || ImGui::IsWindowAppearing()) {
                bs.bw = (int)std::lrint(scfg.bounds_width);
                bs.bh = (int)std::lrint(scfg.bounds_height);
                bs.applied_w = bs.bw;
                bs.applied_h = bs.bh;
                bs.inited = true;
            }
            ImGui::SliderInt("Bounds Width", &bs.bw, 64, 5000);
            bool widthActive = ImGui::IsItemActive();
            ImGui::SliderInt("Bounds Height", &bs.bh, 64, 5000);
            bool heightActive = ImGui::IsItemActive();
            // Only reflect external changes (e.g., undo) when the committed
            // config changed since last apply; do NOT clobber local edits.
            if (!widthActive && !heightActive) {
                int cfgW = (int)std::lrint(scfg.bounds_width);
                int cfgH = (int)std::lrint(scfg.bounds_height);
                if (cfgW != bs.applied_w || cfgH != bs.applied_h) {
                    bs.bw = cfgW;
                    bs.bh = cfgH;
                    bs.applied_w = cfgW;
                    bs.applied_h = cfgH;
                }
            }
            if (ImGui::Button("Apply Bounds")) {
                auto before_w = (int)std::lrint(scfg.bounds_width);
                auto before_h = (int)std::lrint(scfg.bounds_height);
                ImGuiID id = ImGui::GetItemID();
                ctx.undo.beginInteraction(id);
                ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<int>(
                    "sim.bounds_width", "Bounds Width",
                    []() {
                        return 0;
                    },
                    [&](const int &v) {
                        auto cfg = sim.get_config();
                        cfg.bounds_width = (float)v;
                        sim.update_config(cfg);
                    },
                    before_w, bs.bw)));
                ctx.undo.push(std::unique_ptr<IAction>(new ValueAction<int>(
                    "sim.bounds_height", "Bounds Height",
                    []() {
                        return 0;
                    },
                    [&](const int &v) {
                        auto cfg = sim.get_config();
                        cfg.bounds_height = (float)v;
                        sim.update_config(cfg);
                    },
                    before_h, bs.bh)));
                ctx.undo.endInteraction(id);
                scfg.bounds_width = (float)bs.bw;
                scfg.bounds_height = (float)bs.bh;
                bs.applied_w = bs.bw;
                bs.applied_h = bs.bh;
                mark(true);
            }
        }

        ImGui::Separator();
        {
            int before = scfg.target_tps;
            if (ImGui::SliderInt("Target TPS", &scfg.target_tps, 0, 240, "%d",
                                 ImGuiSliderFlags_AlwaysClamp)) {
                push_scfg("sim.target_tps", "Target TPS", before,
                          scfg.target_tps, [&](const int &v) {
                              auto cfg = sim.get_config();
                              cfg.target_tps = v;
                              sim.update_config(cfg);
                          });
                mark(true);
            }
        }
        {
            float before = scfg.time_scale;
            if (ImGui::SliderFloat("Time Scale", &scfg.time_scale, 0.01f, 2.0f,
                                   "%.3f", ImGuiSliderFlags_Logarithmic)) {
                push_scfg("sim.time_scale", "Time Scale", before,
                          scfg.time_scale, [&](const float &v) {
                              auto cfg = sim.get_config();
                              cfg.time_scale = v;
                              sim.update_config(cfg);
                          });
                mark(true);
            }
        }
        {
            float before = scfg.viscosity;
            if (ImGui::SliderFloat("Viscosity", &scfg.viscosity, 0.0f, 1.0f,
                                   "%.3f")) {
                push_scfg("sim.viscosity", "Viscosity", before, scfg.viscosity,
                          [&](const float &v) {
                              auto cfg = sim.get_config();
                              cfg.viscosity = v;
                              sim.update_config(cfg);
                          });
                mark(true);
            }
        }
        {
            float before = scfg.wall_repel;
            if (ImGui::SliderFloat("Wall Repel (px)", &scfg.wall_repel, 0.0f,
                                   200.0f, "%.1f")) {
                push_scfg("sim.wall_repel", "Wall Repel (px)", before,
                          scfg.wall_repel, [&](const float &v) {
                              auto cfg = sim.get_config();
                              cfg.wall_repel = v;
                              sim.update_config(cfg);
                          });
                mark(true);
            }
        }
        {
            float before = scfg.wall_strength;
            if (ImGui::SliderFloat("Wall Strength", &scfg.wall_strength, 0.0f,
                                   1.0f, "%.3f")) {
                push_scfg("sim.wall_strength", "Wall Strength", before,
                          scfg.wall_strength, [&](const float &v) {
                              auto cfg = sim.get_config();
                              cfg.wall_strength = v;
                              sim.update_config(cfg);
                          });
                mark(true);
            }
        }

        ImGui::SeparatorText("Parallelism");
        unsigned hc = std::thread::hardware_concurrency();
        int max_threads = std::max(1, (int)hc - 2);
        ImGui::Text("HW threads: %u", hc ? hc : 1);
        bool auto_mode = (scfg.sim_threads <= 0);
        {
            bool before = auto_mode;
            if (ImGui::Checkbox("Auto (HW-2)", &auto_mode)) {
                int new_threads = auto_mode ? -1 : std::min(1, max_threads);
                push_scfg("sim.sim_threads_auto", "Sim threads auto", before,
                          auto_mode, [&](const bool &v) {
                              auto cfg = sim.get_config();
                              cfg.sim_threads =
                                  v ? -1 : std::min(1, max_threads);
                              sim.update_config(cfg);
                          });
                scfg.sim_threads = new_threads;
                mark(true);
            }
        }
        if (!auto_mode) {
            int before = scfg.sim_threads;
            if (ImGui::SliderInt("Sim threads", &scfg.sim_threads, 1,
                                 max_threads, "%d",
                                 ImGuiSliderFlags_AlwaysClamp)) {
                push_scfg("sim.sim_threads", "Sim threads", before,
                          scfg.sim_threads, [&](const int &v) {
                              auto cfg = sim.get_config();
                              cfg.sim_threads = v;
                              sim.update_config(cfg);
                          });
                mark(true);
            }
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
