#ifndef __UI_HPP
#define __UI_HPP

#include <imgui.h>
#include <mutex>
#include <raylib.h>
#include <thread>

#include "mailboxes.hpp"
#include "multicore.hpp"
#include "types.hpp"
#include "world.hpp"

void render_ui(const WindowConfig &wcfg, World &world,
               SimulationConfigBuffer &scfgb, StatsBuffer &statsb,
               CommandQueue &cmdq) {

    SimulationConfigSnapshot scfg = scfgb.acquire();
    SimStatsSnapshot stats = statsb.acquire();

    bool scfg_updated = false;
    auto check_scfg_update = [&scfg_updated](bool s) {
        if (s) {
            scfg_updated = true;
        }
    };

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.);
    ImGui::Begin("main", NULL,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoTitleBar);
    ImGui::SetWindowPos(ImVec2{0.f, 0.f}, ImGuiCond_Always);
    ImGui::SetWindowSize(
        ImVec2{(float)wcfg.panel_width, (float)wcfg.screen_height},
        ImGuiCond_Always);

    {
        ImGui::SeparatorText("Stats");
        {
            ImGui::Text("FPS: %d", GetFPS());
            ImGui::SameLine();
            ImGui::Text("TPS: %d", stats.effective_tps);
            ImGui::Text("Last step: %.3f ms", stats.last_step_ns / 1e6);
            ImGui::Text("Particles: %d  Groups: %d  Threads: %d",
                        stats.particles, stats.groups, stats.sim_threads);
            ImGui::Text("Sim Bounds: %.0f x %.0f", scfg.bounds_width,
                        scfg.bounds_height);

            ImGui::SeparatorText("Controls");
            if (ImGui::Button("Reset world")) {
                cmdq.push({SimCommand::Kind::ResetWorld});
            }
            ImGui::SameLine();
            if (ImGui::Button("Quit sim")) {
                cmdq.push({SimCommand::Kind::Quit});
            }
        }

        ImGui::SeparatorText("Sim Config");
        {
            check_scfg_update(ImGui::SliderInt("Target TPS", &scfg.target_tps,
                                               0, 240, "%d",
                                               ImGuiSliderFlags_AlwaysClamp));
            check_scfg_update(
                ImGui::Checkbox("Interpolate", &scfg.interpolate));
            if (scfg.interpolate) {
                check_scfg_update(ImGui::SliderFloat("Interp delay (ms)",
                                                     &scfg.interp_delay_ms,
                                                     0.0f, 50.0f, "%.1f"));
            }
            check_scfg_update(ImGui::SliderFloat("Time Scale", &scfg.time_scale,
                                                 0.01f, 2.0f, "%.3f",
                                                 ImGuiSliderFlags_Logarithmic));
            check_scfg_update(ImGui::SliderFloat("Viscosity", &scfg.viscosity,
                                                 0.0f, 1.0f, "%.3f"));
            check_scfg_update(ImGui::SliderFloat(
                "Wall Repel (px)", &scfg.wallRepel, 0.0f, 200.0f, "%.1f"));
            check_scfg_update(ImGui::SliderFloat(
                "Wall Strength", &scfg.wallStrength, 0.0f, 1.0f, "%.3f"));
        }

        { // MULTICORE
            unsigned hc = std::thread::hardware_concurrency();
            int max_threads =
                std::max(1, (int)hc - 2); // keep 2 free: render + OS

            ImGui::SeparatorText("Parallelism");
            ImGui::Text("HW threads: %u", hc ? hc : 1);
            bool auto_mode = (scfg.sim_threads <= 0);
            if (ImGui::Checkbox("Auto (HW-2)", &auto_mode)) {
                scfg.sim_threads = auto_mode ? -1 : std::min(1, max_threads);
                check_scfg_update(true);
            }
            if (!auto_mode) {
                check_scfg_update(ImGui::SliderInt(
                    "Sim threads", &scfg.sim_threads, 1, max_threads, "%d",
                    ImGuiSliderFlags_AlwaysClamp));
            } else {
                ImGui::BeginDisabled();
                int auto_val = std::max(1, (int)compute_sim_threads());
                ImGui::SliderInt("Sim threads", &auto_val, 1, max_threads,
                                 "%d");
                ImGui::EndDisabled();
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();

    if (scfg_updated) {
        scfgb.publish(scfg);
    }
}

#endif