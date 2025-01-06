#ifndef __UI_HPP
#define __UI_HPP

#include <raylib.h>
#include <imgui.h>
#include <mutex>
#include <thread>

#include "multicore.hpp"
#include "world.hpp"
#include "types.hpp"

void render_ui(const WindowConfig &wcfg, World &world, SimConfig &scfg)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.);
    ImGui::Begin("main", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::SetWindowPos(ImVec2{0.f, 0.f}, ImGuiCond_Always);
    ImGui::SetWindowSize(ImVec2{(float)wcfg.panel_width, (float)wcfg.screen_height}, ImGuiCond_Always);

    {
        ImGui::SeparatorText("Stats");
        ImGui::Text("FPS: %d", GetFPS());
        ImGui::SameLine();
        ImGui::Text("TPS: %d", scfg.effective_tps.load(std::memory_order_relaxed));
        ImGui::Text("Sim Bounds: %.0f x %.0f", scfg.bounds_width, scfg.bounds_height);

        const int G = world.get_groups_size();
        const int N = world.get_particles_size();
        ImGui::Text("Num particles: %d", N);
        ImGui::Text("Num groups: %d", G);
        if (G > 0)
        {
            ImGui::TextUnformatted("Particles per group:");
            for (int g = 0; g < G; ++g)
                ImGui::BulletText("G%d: %d", g, world.get_group_size(g));
        }

        ImGui::SeparatorText("Sim Config");

        // Target TPS
        int tps = scfg.target_tps.load(std::memory_order_relaxed);
        if (ImGui::SliderInt("Target TPS", &tps, 0, 240, "%d", ImGuiSliderFlags_AlwaysClamp))
            scfg.target_tps.store(tps, std::memory_order_relaxed);

        // Interpolate (based on render framerate; uses last two sim snapshots)
        bool interpolate = scfg.interpolate.load(std::memory_order_relaxed);
        if (ImGui::Checkbox("Interpolate", &interpolate))
            scfg.interpolate.store(interpolate, std::memory_order_relaxed);

        // Only show delay slider if interpolation is on
        if (interpolate)
        {
            float delay_ms = scfg.interp_delay_ms.load(std::memory_order_relaxed);
            if (ImGui::SliderFloat("Interp delay (ms)", &delay_ms, 0.0f, 50.0f, "%.1f"))
            {
                scfg.interp_delay_ms.store(delay_ms, std::memory_order_relaxed);
            }
        }

        // Time scale
        float time_scale = scfg.time_scale.load(std::memory_order_relaxed);
        if (ImGui::SliderFloat("Time Scale", &time_scale, 0.01f, 2.0f, "%.3f", ImGuiSliderFlags_Logarithmic))
            scfg.time_scale.store(time_scale, std::memory_order_relaxed);

        // Viscosity
        float viscosity = scfg.viscosity.load(std::memory_order_relaxed);
        if (ImGui::SliderFloat("Viscosity", &viscosity, 0.0f, 1.0f, "%.3f"))
            scfg.viscosity.store(viscosity, std::memory_order_relaxed);

        // Gravity
        float gravity = scfg.gravity.load(std::memory_order_relaxed);
        if (ImGui::SliderFloat("Gravity", &gravity, -2.0f, 2.0f, "%.3f"))
            scfg.gravity.store(gravity, std::memory_order_relaxed);

        // Walls
        float wallRepel = scfg.wallRepel.load(std::memory_order_relaxed);
        if (ImGui::SliderFloat("Wall Repel (px)", &wallRepel, 0.0f, 200.0f, "%.1f"))
            scfg.wallRepel.store(wallRepel, std::memory_order_relaxed);

        float wallStrength = scfg.wallStrength.load(std::memory_order_relaxed);
        if (ImGui::SliderFloat("Wall Strength", &wallStrength, 0.0f, 1.0f, "%.3f"))
            scfg.wallStrength.store(wallStrength, std::memory_order_relaxed);

        // Pulse
        float pulse = scfg.pulse.load(std::memory_order_relaxed);
        if (ImGui::SliderFloat("Pulse", &pulse, -2.0f, 2.0f, "%.3f"))
            scfg.pulse.store(pulse, std::memory_order_relaxed);

        // Pulse position (clamped to bounds)
        float px = scfg.pulse_x.load(std::memory_order_relaxed);
        float py = scfg.pulse_y.load(std::memory_order_relaxed);
        if (ImGui::DragFloat2("Pulse Pos (x,y)", &px, 1.0f, 0.0f, 0.0f, "%.0f"))
        {
            px = std::clamp(px, 0.0f, scfg.bounds_width);
            py = std::clamp(py, 0.0f, scfg.bounds_height);
            scfg.pulse_x.store(px, std::memory_order_relaxed);
            scfg.pulse_y.store(py, std::memory_order_relaxed);
        }

        // Safe reset/reseed (handled by sim thread)
        if (ImGui::Button("Reset world"))
            scfg.reset_requested.store(true, std::memory_order_release);

        { // MULTICORE
            unsigned hc = std::thread::hardware_concurrency();
            int max_threads = std::max(1, (int)hc - 2); // keep 2 free: render + OS
            int sim_thr = scfg.sim_threads.load(std::memory_order_relaxed);

            ImGui::SeparatorText("Parallelism");
            ImGui::Text("HW threads: %u", hc ? hc : 1);
            bool auto_mode = (sim_thr <= 0);
            if (ImGui::Checkbox("Auto (HW-2)", &auto_mode))
            {
                scfg.sim_threads.store(auto_mode ? -1 : std::min(2, max_threads), std::memory_order_relaxed);
            }
            if (!auto_mode)
            {
                if (ImGui::SliderInt("Sim threads", &sim_thr, 1, max_threads, "%d", ImGuiSliderFlags_AlwaysClamp))
                {
                    scfg.sim_threads.store(sim_thr, std::memory_order_relaxed);
                }
            }
            else
            {
                ImGui::BeginDisabled();
                int auto_val = std::max(1, (int)compute_sim_threads());
                ImGui::SliderInt("Sim threads", &auto_val, 1, max_threads, "%d");
                ImGui::EndDisabled();
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

#endif