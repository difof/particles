#ifndef __CONTROL_UI_HPP
#define __CONTROL_UI_HPP

#include "../types.hpp"
#include "renderconfig.hpp"
#include "renderer.hpp"
#include <imgui.h>
#include <raylib.h>

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

        ImGui::SeparatorText("Stats");
        ImGui::Text("FPS: %d", GetFPS());
        ImGui::SameLine();
        ImGui::Text("TPS: %d", stats.effective_tps);
        ImGui::Text("Last step: %.3f ms", stats.last_step_ns / 1e6);
        ImGui::Text("Num steps: %lld", stats.num_steps);
        ImGui::Text("Particles: %d  Groups: %d  Threads: %d", stats.particles,
                    stats.groups, stats.sim_threads);
        ImGui::Text("Sim Bounds: %.0f x %.0f", scfg.bounds_width,
                    scfg.bounds_height);

        ImGui::SeparatorText("Debug DPI");
        ImGui::Text("Screen %d x %d", GetScreenWidth(), GetScreenHeight());
        ImGui::Text("Render %d x %d", GetRenderWidth(), GetRenderHeight());
        ImGui::Text("Mouse  %.1f, %.1f", GetMousePosition().x,
                    GetMousePosition().y);

        ImGui::SeparatorText("Controls");
        if (ImGui::Button("Reset world")) {
            sim.push_command(mailbox::command::ResetWorld{});
        }
        ImGui::SameLine();
        if (ImGui::Button("Quit sim")) {
            sim.push_command(mailbox::command::Quit{});
        }
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

        ImGui::SeparatorText("Groups & Rules");
        struct EditorState {
            int G = 0;
            std::vector<float> r2;
            std::vector<float> rules;
            std::vector<int> sizes;
            std::vector<Color> colors;
            bool dirty = false;
        };
        static EditorState editor;
        auto refresh_from_world = [&]() {
            const int G = world.get_groups_size();
            editor.G = G;
            editor.r2.resize(G);
            editor.rules.resize(G * G);
            editor.sizes.resize(G);
            editor.colors.resize(G);
            for (int g = 0; g < G; ++g) {
                editor.r2[g] = world.r2_of(g);
                editor.colors[g] = world.get_group_color(g);
                editor.sizes[g] =
                    world.get_group_end(g) - world.get_group_start(g);
                const auto rowv = world.rules_of(g);
                if (!rowv.row) {
                    for (int j = 0; j < G; ++j)
                        editor.rules[g * G + j] = 0.f;
                } else {
                    for (int j = 0; j < G; ++j)
                        editor.rules[g * G + j] = rowv.get(j);
                }
            }
            editor.dirty = false;
        };
        static int last_seen_groups = -1;
        if (last_seen_groups != stats.groups) {
            refresh_from_world();
            last_seen_groups = stats.groups;
        }
        ImGui::BeginChild("GroupsRulesChild", ImVec2(0, 260), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);
        for (int g = 0; g < editor.G; ++g) {
            ImGui::PushID(g);
            ImGui::SeparatorText(
                (std::string("Group ") + std::to_string(g)).c_str());
            float col[4] = {
                editor.colors[g].r / 255.f, editor.colors[g].g / 255.f,
                editor.colors[g].b / 255.f, editor.colors[g].a / 255.f};
            if (ImGui::ColorEdit4("Color", col, ImGuiColorEditFlags_NoInputs)) {
                editor.colors[g] = Color{
                    (unsigned char)std::clamp(int(col[0] * 255.f), 0, 255),
                    (unsigned char)std::clamp(int(col[1] * 255.f), 0, 255),
                    (unsigned char)std::clamp(int(col[2] * 255.f), 0, 255),
                    (unsigned char)std::clamp(int(col[3] * 255.f), 0, 255)};
                editor.dirty = true;
            }
            int sz = editor.sizes[g];
            ImGui::InputInt("Size (info)", &sz, 0, 0,
                            ImGuiInputTextFlags_ReadOnly);
            float r = std::sqrt(std::max(0.f, editor.r2[g]));
            if (ImGui::SliderFloat("Radius (r)", &r, 0.f, 300.f, "%.1f")) {
                editor.r2[g] = r * r;
                editor.dirty = true;
            }
            if (ImGui::TreeNode("Rules Row")) {
                for (int j = 0; j < editor.G; ++j) {
                    ImGui::PushID(j);
                    ImVec4 csrc = to_imvec4(editor.colors[g]);
                    ImVec4 cdst = to_imvec4(editor.colors[j]);
                    ImGui::ColorButton("src", csrc,
                                       ImGuiColorEditFlags_NoTooltip |
                                           ImGuiColorEditFlags_NoPicker |
                                           ImGuiColorEditFlags_NoDragDrop,
                                       ImVec2(14, 14));
                    ImGui::SameLine(0.0f, 6.0f);
                    ImGui::Text("g%d  \xE2\x86\x92  g%d", g, j);
                    ImGui::SameLine(0.0f, 6.0f);
                    ImGui::ColorButton("dst", cdst,
                                       ImGuiColorEditFlags_NoTooltip |
                                           ImGuiColorEditFlags_NoPicker |
                                           ImGuiColorEditFlags_NoDragDrop,
                                       ImVec2(14, 14));
                    float &v = editor.rules[g * editor.G + j];
                    if (ImGui::SliderFloat("Strength", &v, -3.14f, 3.14f,
                                           "%.3f")) {
                        editor.dirty = true;
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        bool can_hot_apply = (editor.G == stats.groups);
        if (!can_hot_apply)
            ImGui::BeginDisabled();
        if (ImGui::Button("Apply (hot, no reseed)")) {
            auto patch = std::make_shared<mailbox::command::RulePatch>();
            patch->groups = editor.G;
            patch->r2 = editor.r2;
            patch->rules = editor.rules;
            patch->colors = editor.colors;
            patch->hot = true;
            sim.push_command(mailbox::command::ApplyRules{patch});
            editor.dirty = false;
        }
        if (!can_hot_apply) {
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "Group count/order changed. Hot apply disabled.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply & Reseed")) {
            auto patch = std::make_shared<mailbox::command::RulePatch>();
            patch->groups = editor.G;
            patch->r2 = editor.r2;
            patch->rules = editor.rules;
            patch->colors = editor.colors;
            patch->hot = false;
            sim.push_command(mailbox::command::ApplyRules{patch});
            editor.dirty = false;
        }
        if (ImGui::Button("Make symmetric (w_ij = w_ji)")) {
            for (int i = 0; i < editor.G; ++i)
                for (int j = i + 1; j < editor.G; ++j) {
                    float m = 0.5f * (editor.rules[i * editor.G + j] +
                                      editor.rules[j * editor.G + i]);
                    editor.rules[i * editor.G + j] =
                        editor.rules[j * editor.G + i] = m;
                }
            editor.dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Zero self (w_ii = 0)")) {
            for (int i = 0; i < editor.G; ++i)
                editor.rules[i * editor.G + i] = 0.f;
            editor.dirty = true;
        }
        if (ImGui::Button("Randomize rules")) {
            static std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<float> dist(-3.14f, 3.14f);
            for (int i = 0; i < editor.G; ++i)
                for (int j = 0; j < editor.G; ++j)
                    editor.rules[i * editor.G + j] = dist(rng);
            editor.dirty = true;
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
