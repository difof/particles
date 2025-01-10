#ifndef __UI_HPP
#define __UI_HPP

#include <imgui.h>
#include <mutex>
#include <random>
#include <raylib.h>
#include <thread>

#include "../mailbox/mailbox.hpp"
#include "../simulation/multicore.hpp"
#include "../simulation/world.hpp"
#include "../types.hpp"

void render_ui(const WindowConfig &wcfg, World &world,
               mailbox::SimulationConfig &scfgb,
               mailbox::SimulationStats &statsb, mailbox::command::Queue &cmdq,
               RenderConfig &rcfg) {

    mailbox::SimulationConfig::Snapshot scfg = scfgb.acquire();
    mailbox::SimulationStats::Snapshot stats = statsb.acquire();

    bool scfg_updated = false;
    auto mark = [&scfg_updated](bool s) {
        if (s) {
            scfg_updated = true;
        }
    };

    auto to_imvec4 = [](Color c) -> ImVec4 {
        return ImVec4(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
    };

    auto window_size =
        ImVec2{(float)wcfg.panel_width, (float)wcfg.screen_height};

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.);
    ImGui::Begin("main", NULL,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::SetWindowPos(ImVec2{0.f, 0.f}, ImGuiCond_Always);
    ImGui::SetWindowSize(window_size, ImGuiCond_Always);

    {
        ImGui::SeparatorText("Stats");
        { /// MARK: Stats
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
                cmdq.push({mailbox::command::Command::Kind::ResetWorld});
            }
            ImGui::SameLine();
            if (ImGui::Button("Quit sim")) {
                cmdq.push({mailbox::command::Command::Kind::Quit});
            }
        }

        ImGui::SeparatorText("Sim Config");
        { /// MARK: Simuation config
            mark(ImGui::SliderInt("Target TPS", &scfg.target_tps, 0, 240, "%d",
                                  ImGuiSliderFlags_AlwaysClamp));
            mark(ImGui::SliderFloat("Time Scale", &scfg.time_scale, 0.01f, 2.0f,
                                    "%.3f", ImGuiSliderFlags_Logarithmic));
            mark(ImGui::SliderFloat("Viscosity", &scfg.viscosity, 0.0f, 1.0f,
                                    "%.3f"));
            mark(ImGui::SliderFloat("Wall Repel (px)", &scfg.wallRepel, 0.0f,
                                    200.0f, "%.1f"));
            mark(ImGui::SliderFloat("Wall Strength", &scfg.wallStrength, 0.0f,
                                    1.0f, "%.3f"));
        }

        ImGui::SeparatorText("Render");
        { /// MARK: Render config
            ImGui::Checkbox("Interpolate", &rcfg.interpolate);
            if (rcfg.interpolate) {
                ImGui::SliderFloat("Interp delay (ms)", &rcfg.interp_delay_ms,
                                   0.0f, 50.0f, "%.1f");
            }

            ImGui::SliderFloat("Core size (px)", &rcfg.core_size, 0.5f, 4.0f,
                               "%.2f");
            ImGui::Checkbox("Glow enabled", &rcfg.glow_enabled);
            if (rcfg.glow_enabled) {
                ImGui::SliderFloat("Outer scale (x core)",
                                   &rcfg.outer_scale_mul, 4.0f, 24.0f, "%.1f");
                ImGui::SliderFloat("Outer RGB gain", &rcfg.outer_rgb_gain, 0.0f,
                                   1.0f, "%.2f");
                ImGui::SliderFloat("Inner scale (x core)",
                                   &rcfg.inner_scale_mul, 1.0f, 8.0f, "%.1f");
                ImGui::SliderFloat("Inner RGB gain", &rcfg.inner_rgb_gain, 0.0f,
                                   1.0f, "%.2f");
                ImGui::Checkbox("Final additive blit",
                                &rcfg.final_additive_blit);
            }

            ImGui::SeparatorText("Overlays");
            ImGui::Checkbox("Density heatmap", &rcfg.show_density_heat);
            if (rcfg.show_density_heat) {
                ImGui::SliderFloat("Heat alpha", &rcfg.heat_alpha, 0.0f, 1.0f,
                                   "%.2f");
            }

            ImGui::Checkbox("Velocity field", &rcfg.show_velocity_field);
            if (rcfg.show_velocity_field) {
                ImGui::SliderFloat("Vel scale", &rcfg.vel_scale, 0.1f, 5.0f,
                                   "%.2f");
                ImGui::SliderFloat("Vel thickness", &rcfg.vel_thickness, 0.5f,
                                   4.0f, "%.1f");
            }

            ImGui::Checkbox("Show grid lines", &rcfg.show_grid_lines);
        }

        ImGui::SeparatorText("Parallelism");
        { /// MARK: Parallelism
            unsigned hc = std::thread::hardware_concurrency();
            int max_threads =
                std::max(1, (int)hc - 2); // keep 2 free: render + OS

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
                ImGui::SliderInt("Sim threads", &auto_val, 1, max_threads,
                                 "%d");
                ImGui::EndDisabled();
            }
        }

        ImGui::SeparatorText("Groups & Rules");
        { /// MARK: Groups and rules
            struct EditorState {
                int G = 0;
                std::vector<float> r2;    // size G
                std::vector<float> rules; // size G*G row-major
                std::vector<int> sizes;   // per-group particle counts (for add)
                std::vector<Color> colors; // per-group color
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
                    editor.colors[g] = *world.get_group_color(g);
                    editor.sizes[g] =
                        world.get_group_end(g) - world.get_group_start(g);

                    const float *row = world.rules_row(g);
                    if (!row) {
                        // transient: rules not ready yet
                        for (int j = 0; j < G; ++j)
                            editor.rules[g * G + j] = 0.f;
                    } else {
                        for (int j = 0; j < G; ++j)
                            editor.rules[g * G + j] = row[j];
                    }
                }
                editor.dirty = false;
            };

            // Initialize or resync when sim-side group count changed
            static int last_seen_groups = -1;
            if (last_seen_groups != stats.groups) {
                refresh_from_world();
                last_seen_groups = stats.groups;
            }

            // Scrollable child so the whole editor can be long
            ImGui::BeginChild("GroupsRulesChild", ImVec2(0, 260), true,
                              ImGuiWindowFlags_AlwaysVerticalScrollbar);

            // per-group controls
            for (int g = 0; g < editor.G; ++g) {
                ImGui::PushID(g);
                ImGui::SeparatorText(
                    (std::string("Group ") + std::to_string(g)).c_str());

                // color
                float col[4] = {
                    editor.colors[g].r / 255.f, editor.colors[g].g / 255.f,
                    editor.colors[g].b / 255.f, editor.colors[g].a / 255.f};
                if (ImGui::ColorEdit4("Color", col,
                                      ImGuiColorEditFlags_NoInputs)) {
                    editor.colors[g] = Color{
                        (unsigned char)std::clamp(int(col[0] * 255.f), 0, 255),
                        (unsigned char)std::clamp(int(col[1] * 255.f), 0, 255),
                        (unsigned char)std::clamp(int(col[2] * 255.f), 0, 255),
                        (unsigned char)std::clamp(int(col[3] * 255.f), 0, 255)};
                    editor.dirty = true;
                }

                // size (display-only unless you implement hot resize)
                int sz = editor.sizes[g];
                ImGui::InputInt("Size (info)", &sz, 0, 0,
                                ImGuiInputTextFlags_ReadOnly);

                // interaction radius^2
                float r = std::sqrt(std::max(0.f, editor.r2[g]));
                if (ImGui::SliderFloat("Radius (r)", &r, 0.f, 300.f, "%.1f")) {
                    editor.r2[g] = r * r;
                    editor.dirty = true;
                }

                // row of rule sliders (g affects j)
                if (ImGui::TreeNode("Rules Row")) {
                    for (int j = 0; j < editor.G; ++j) {
                        ImGui::PushID(j);

                        // header line: [src color] "g -> j" [dst color]
                        ImVec4 csrc = to_imvec4(editor.colors[g]);
                        ImVec4 cdst = to_imvec4(editor.colors[j]);

                        // Put labels above the slider (not inline with it)
                        ImGui::ColorButton("src", csrc,
                                           ImGuiColorEditFlags_NoTooltip |
                                               ImGuiColorEditFlags_NoPicker |
                                               ImGuiColorEditFlags_NoDragDrop,
                                           ImVec2(14, 14));
                        ImGui::SameLine(0.0f, 6.0f);
                        ImGui::Text("g%d  \xE2\x86\x92  g%d", g, j); // → arrow
                        ImGui::SameLine(0.0f, 6.0f);
                        ImGui::ColorButton("dst", cdst,
                                           ImGuiColorEditFlags_NoTooltip |
                                               ImGuiColorEditFlags_NoPicker |
                                               ImGuiColorEditFlags_NoDragDrop,
                                           ImVec2(14, 14));

                        // the actual rule slider
                        float &v = editor.rules[g * editor.G + j];
                        if (ImGui::SliderFloat("Strength", &v, -3.14f, 3.14f,
                                               "%.3f")) {
                            editor.dirty = true;
                        }

                        // a thin separator between rules
                        ImGui::Separator();

                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }

                // TODO: remove button (forces reseed on sim side)
                // if (ImGui::Button("Remove Group")) {
                //     auto cmd = mailbox::command::Command{};
                //     cmd.kind = mailbox::command::Command::Kind::RemoveGroup;
                //     cmd.rem_group =
                //         std::make_shared<mailbox::command::RemoveGroupCmd>(
                //             mailbox::command::RemoveGroupCmd{g});
                //     cmdq.push(cmd);
                // }

                ImGui::PopID();
            }

            // ImGui::Separator();

            // TODO: Add group (appends; positions for new group are randomized
            // in sim) static int new_size = 500; static float new_r = 80.f;
            // static float new_col[4] = {0.8f, 0.8f, 0.2f, 1.f};
            // ImGui::InputInt("New group size", &new_size);
            // ImGui::SliderFloat("New group radius r", &new_r, 1.f, 300.f,
            //                    "%.1f");
            // ImGui::ColorEdit4("New group color", new_col,
            //                   ImGuiColorEditFlags_NoInputs);
            // if (ImGui::Button("Add Group")) {
            //     auto cmd = mailbox::command::Command{};
            //     cmd.kind = mailbox::command::Command::Kind::AddGroup;
            //     auto add = std::make_shared<mailbox::command::AddGroupCmd>();
            //     add->size = std::max(0, new_size);
            //     add->r2 = new_r * new_r;
            //     add->color = Color{
            //         (unsigned char)std::clamp(int(new_col[0] * 255.f), 0,
            //         255), (unsigned char)std::clamp(int(new_col[1] * 255.f),
            //         0, 255), (unsigned char)std::clamp(int(new_col[2] *
            //         255.f), 0, 255), (unsigned char)std::clamp(int(new_col[3]
            //         * 255.f), 0, 255)};
            //     cmd.add_group = add;
            //     cmdq.push(cmd);
            // }

            ImGui::EndChild(); // end scrollable

            // MARK: Apply buttons
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

                auto cmd = mailbox::command::Command{};
                cmd.kind = mailbox::command::Command::Kind::ApplyRules;
                cmd.rules = patch;
                cmdq.push(cmd);
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

                auto cmd = mailbox::command::Command{};
                cmd.kind = mailbox::command::Command::Kind::ApplyRules;
                cmd.rules = patch;
                cmdq.push(cmd);
                editor.dirty = false;
            }

            // Quality-of-life helpers
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
                for (int i = 0; i < editor.G; ++i) {
                    for (int j = 0; j < editor.G; ++j) {
                        editor.rules[i * editor.G + j] = dist(rng);
                    }
                }
                editor.dirty = true;
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